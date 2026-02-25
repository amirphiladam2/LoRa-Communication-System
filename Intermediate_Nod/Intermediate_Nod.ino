/*
 * ESP8266 LoRa Relay Node (Range Extender)
 * - Receives alerts from transmitter
 * - Forwards them to receivers
 * - External LED (D4/GPIO2) = Signal Reception
 * - Onboard LED (D0/GPIO16) = Transmission
 * - Prevents message loops with deduplication
 */

#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// ===== LoRa Pins (ESP8266) =====
#define LORA_CS   15   
#define LORA_RST  -1   
#define LORA_DIO0 4   

// ===== LED Indicators =====
#define RX_LED_PIN 2       // D4 (GPIO2) - External LED for reception
#define TX_LED_PIN 16      // D0 (GPIO16) - Onboard LED for transmission

const unsigned long LED_BLINK_MS = 300;
unsigned long rxLedOffTime = 0;
unsigned long txLedOffTime = 0;

// ===== Message Deduplication =====
const int CACHE_SIZE = 10;
struct MessageCache {
  char msg[64];
  unsigned long timestamp;
} messageCache[CACHE_SIZE];
int cacheIndex = 0;

const unsigned long DEDUPE_WINDOW = 5000;  // 5 seconds

// ===== ESP-NOW Configuration =====
uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast address
struct __attribute__((packed)) EspNowMessage {
  char msg[64];
};

// ===== Distance Threshold =====
const float DISTANCE_THRESHOLD_METERS = 50.0;
const float RECEIVER_LAT = 30.768885; // Default Receiver Lat
const float RECEIVER_LNG = 76.57521;  // Default Receiver Lng

// ===== Statistics =====
unsigned long messagesReceived = 0;
unsigned long messagesForwardedLoRa = 0;
unsigned long messagesForwardedEspNow = 0;
unsigned long messagesDuplicate = 0;
unsigned long lastStatsDisplay = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== LoRa Relay Node ==="));
  
  // Initialize LEDs
  pinMode(RX_LED_PIN, OUTPUT);
  pinMode(TX_LED_PIN, OUTPUT);
  digitalWrite(RX_LED_PIN, LOW);
  digitalWrite(TX_LED_PIN, LOW);
  
  Serial.println(F("LEDs initialized"));
  Serial.print(F("   RX LED: D4 (GPIO"));
  Serial.print(RX_LED_PIN);
  Serial.println(F(")"));
  Serial.print(F("   TX LED: Onboard (GPIO"));
  Serial.print(TX_LED_PIN);
  Serial.println(F(")"));

  // Startup LED test
  Serial.println(F("\nLED Test..."));
  for (int i = 0; i < 3; i++) {
    digitalWrite(RX_LED_PIN, HIGH);
    delay(150);
    digitalWrite(RX_LED_PIN, LOW);
    delay(150);
  }
  delay(300);
  for (int i = 0; i < 3; i++) {
    digitalWrite(TX_LED_PIN, HIGH);
    delay(150);
    digitalWrite(TX_LED_PIN, LOW);
    delay(150);
  }

  // Initialize WiFi for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println(F("ESP-NOW Init Failed"));
  } else {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(receiverAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    Serial.println(F("ESP-NOW Initialized"));
  }

  // Initialize LoRa
  Serial.print(F("Initializing LoRa... "));
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println(F("FAILED!"));
    while (1) {
      digitalWrite(RX_LED_PIN, !digitalRead(RX_LED_PIN));
      digitalWrite(TX_LED_PIN, !digitalRead(TX_LED_PIN));
      delay(200);
    }
  }
  
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.setTxPower(20);
  
  Serial.println(F("OK (433MHz)"));
  Serial.println(F("\nRelay Active - Listening...\n"));
  
  // Initialize message cache
  for (int i = 0; i < CACHE_SIZE; i++) {
    messageCache[i].msg[0] = '\0';
    messageCache[i].timestamp = 0;
  }
}

void blinkRxLed() {
  digitalWrite(RX_LED_PIN, HIGH);
  rxLedOffTime = millis() + LED_BLINK_MS;
}

void blinkTxLed() {
  digitalWrite(TX_LED_PIN, HIGH);
  txLedOffTime = millis() + LED_BLINK_MS;
}

// Check if message was recently seen (deduplication)
bool isDuplicate(const char* msg) {
  unsigned long now = millis();
  
  for (int i = 0; i < CACHE_SIZE; i++) {
    // Check if message matches and is within time window
    if (strcmp(messageCache[i].msg, msg) == 0) {
      if (now - messageCache[i].timestamp < DEDUPE_WINDOW) {
        return true;  // Duplicate found
      }
    }
  }
  
  return false;  // Not a duplicate
}

// Add message to cache
void cacheMessage(const char* msg) {
  strncpy(messageCache[cacheIndex].msg, msg, sizeof(messageCache[0].msg) - 1);
  messageCache[cacheIndex].msg[sizeof(messageCache[0].msg) - 1] = '\0';
  messageCache[cacheIndex].timestamp = millis();
  
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
}

// Validate message format
bool isValidMessage(const char* msg) {
  // Must start with FIRE or EARTHQUAKE
  if (strncmp(msg, "FIRE", 4) == 0) return true;
  if (strncmp(msg, "EARTHQUAKE", 10) == 0) return true;
  return false;
}

float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371000; // Earth radius in meters
  float phi1 = lat1 * PI / 180;
  float phi2 = lat2 * PI / 180;
  float deltaPhi = (lat2 - lat1) * PI / 180;
  float deltaLambda = (lon2 - lon1) * PI / 180;

  float a = sin(deltaPhi / 2) * sin(deltaPhi / 2) +
            cos(phi1) * cos(phi2) *
            sin(deltaLambda / 2) * sin(deltaLambda / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));

  return R * c;
}

void forwardMessage(const char* msg) {
  // Parse coordinates from message to calculate distance
  float transLat = 0, transLng = 0;
  const char* p = strchr(msg, ',');
  if (p) {
    transLat = atof(++p);
    p = strchr(p, ',');
    if (p) transLng = atof(++p);
  }

  float dist = calculateDistance(transLat, transLng, RECEIVER_LAT, RECEIVER_LNG);
  Serial.print(F("Distance to Receiver: "));
  Serial.print(dist);
  Serial.println(F(" meters"));

  if (dist < DISTANCE_THRESHOLD_METERS) {
    // Forward via ESP-NOW (Short Range)
    EspNowMessage espMsg;
    strncpy(espMsg.msg, msg, sizeof(espMsg.msg) - 1);
    espMsg.msg[sizeof(espMsg.msg) - 1] = '\0';
    
    int result = esp_now_send(receiverAddress, (uint8_t *)&espMsg, sizeof(espMsg));
    if (result == 0) {
      Serial.println(F("FORWARDED (ESP-NOW)"));
      messagesForwardedEspNow++;
      blinkTxLed();
    } else {
      Serial.println(F("ESP-NOW Forward failed"));
    }
  } else {
    // Forward via LoRa (Long Range)
    LoRa.beginPacket();
    LoRa.print(msg);
    if (LoRa.endPacket()) {
      Serial.println(F("FORWARDED (LoRa)"));
      messagesForwardedLoRa++;
      blinkTxLed();
    } else {
      Serial.println(F("LoRa Forward failed"));
    }
  }
}

void loop() {
  unsigned long now = millis();
  
  // Manage LED timeouts
  if (now >= rxLedOffTime && digitalRead(RX_LED_PIN) == HIGH) {
    digitalWrite(RX_LED_PIN, LOW);
  }
  if (now >= txLedOffTime && digitalRead(TX_LED_PIN) == HIGH) {
    digitalWrite(TX_LED_PIN, LOW);
  }

  // ===== Check for incoming LoRa packets =====
  int packetSize = LoRa.parsePacket();
  
  if (packetSize > 0) {
    char msg[64];
    uint8_t idx = 0;
    
    // Read packet
    while (LoRa.available() && idx < sizeof(msg) - 1) {
      msg[idx++] = (char)LoRa.read();
    }
    msg[idx] = '\0';  // Null terminate
    
    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();
    
    messagesReceived++;
    blinkRxLed();
    
    Serial.print(F("📥 RECEIVED: "));
    Serial.print(msg);
    Serial.print(F(" | RSSI: "));
    Serial.print(rssi);
    Serial.print(F(" dBm | SNR: "));
    Serial.print(snr);
    Serial.println(F(" dB"));
    
    // Validate message format
    if (!isValidMessage(msg)) {
      Serial.println(F("Invalid format - not forwarding"));
      return;
    }
    
    // Check for duplicates
    if (isDuplicate(msg)) {
      messagesDuplicate++;
      Serial.println(F("Duplicate - not forwarding"));
      return;
    }
    
    // Cache this message
    cacheMessage(msg);
    
    // Forward the message
    forwardMessage(msg);
  }

  // ===== Display statistics every 10 seconds =====
  if (now - lastStatsDisplay >= 10000) {
    Serial.println(F("\n--- Statistics ---"));
    Serial.print(F("Received:      "));
    Serial.println(messagesReceived);
    Serial.print(F("Forwarded LoRa:  "));
    Serial.println(messagesForwardedLoRa);
    Serial.print(F("Forwarded E-NOW: "));
    Serial.println(messagesForwardedEspNow);
    Serial.print(F("Duplicates:    "));
    Serial.println(messagesDuplicate);
    Serial.print(F("Uptime:        "));
    Serial.print(now / 1000);
    Serial.println(F(" seconds\n"));
    
    lastStatsDisplay = now;
  }

  yield();  // ESP8266 watchdog feed
}