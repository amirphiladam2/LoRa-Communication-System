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

// ===== LoRa Pins (ESP8266) =====
#define LORA_CS   15   // D8
#define LORA_RST  -1   // Tied to 3.3V externally
#define LORA_DIO0 4   // D0 (GPIO16) - Also onboard LED

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

// ===== Statistics =====
unsigned long messagesReceived = 0;
unsigned long messagesForwarded = 0;
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
  
  Serial.println(F("✅ LEDs initialized"));
  Serial.print(F("   RX LED: D4 (GPIO"));
  Serial.print(RX_LED_PIN);
  Serial.println(F(")"));
  Serial.print(F("   TX LED: Onboard (GPIO"));
  Serial.print(TX_LED_PIN);
  Serial.println(F(")"));

  // Startup LED test
  Serial.println(F("\n🔆 LED Test..."));
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

  // Initialize I2C for LoRa (ESP8266 standard pins)
  SPI.begin();

  // Initialize LoRa
  Serial.print(F("Initializing LoRa... "));
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println(F("❌ FAILED!"));
    Serial.println(F("Check connections:"));
    Serial.println(F("  SCK  -> D5 (GPIO14)"));
    Serial.println(F("  MISO -> D6 (GPIO12)"));
    Serial.println(F("  MOSI -> D7 (GPIO13)"));
    Serial.println(F("  CS   -> D8 (GPIO15)"));
    Serial.println(F("  DIO0 -> D0 (GPIO16)"));
    
    while (1) {
      digitalWrite(RX_LED_PIN, !digitalRead(RX_LED_PIN));
      digitalWrite(TX_LED_PIN, !digitalRead(TX_LED_PIN));
      delay(200);
    }
  }
  
  // Match transmitter/receiver settings
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.setTxPower(20);  // Max power for relay
  
  Serial.println(F("✅ OK (433MHz, SF7)"));
  Serial.println(F("\n✅ Relay Active - Listening...\n"));
  
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

void forwardMessage(const char* msg) {
  // Add small random delay to avoid collision with original transmitter
  delay(random(50, 150));
  
  // Transmit
  LoRa.beginPacket();
  LoRa.print(msg);
  bool success = LoRa.endPacket();
  
  if (success) {
    blinkTxLed();
    messagesForwarded++;
    
    Serial.print(F("📤 FORWARDED: "));
    Serial.println(msg);
  } else {
    Serial.println(F("⚠️ Forward failed"));
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
      Serial.println(F("   ⚠️ Invalid format - not forwarding"));
      return;
    }
    
    // Check for duplicates
    if (isDuplicate(msg)) {
      messagesDuplicate++;
      Serial.println(F("   🔁 Duplicate - not forwarding"));
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
    Serial.print(F("Received:   "));
    Serial.println(messagesReceived);
    Serial.print(F("Forwarded:  "));
    Serial.println(messagesForwarded);
    Serial.print(F("Duplicates: "));
    Serial.println(messagesDuplicate);
    Serial.print(F("Uptime:     "));
    Serial.print(now / 1000);
    Serial.println(F(" seconds\n"));
    
    lastStatsDisplay = now;
  }

  yield();  // ESP8266 watchdog feed
}