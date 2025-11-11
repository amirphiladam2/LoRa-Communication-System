#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

//OLED 
Adafruit_SH1106G display(128, 64, &Wire, -1);
bool displayReady = false;

//LoRa Pins 
#define LORA_CS   15
#define LORA_RST  -1
#define LORA_DIO0 16

// Buzzer 
#define BUZZER_PIN 2  // GPIO2 (D4 on NodeMCU)

//LoRa Parameters 
const float LORA_FREQ = 433E6;
const uint8_t LORA_SF = 7;
const long LORA_BW = 125E3;
const uint8_t LORA_CR = 5;
const uint8_t LORA_SYNC = 0x12;

//Timing 
const unsigned long DISPLAY_TIMEOUT = 15000UL;

//States 
unsigned long lastAlertTime = 0;
bool alertDisplayed = false;

//Buzzer Control 
struct {
  bool active;
  bool isHigh;
  uint8_t beepsRemaining;
  uint16_t beepDuration;
  unsigned long nextToggle;
} buzzer = {false, false, 0, 0, 0};

//Alert Types 
enum AlertType { NONE = 0, FIRE = 1, QUAKE = 2 };

//Function Prototypes
void showListeningScreen();
void displayAlert(AlertType type, float lat, float lng, float extra);
void parseMessage(const char* msg, AlertType* type, float* lat, float* lng, float* extra);
void startBuzzer(AlertType type);
void updateBuzzer();

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== ESP8266 LoRa Alert Receiver (SH1106) ==="));

  // OLED Init
  Wire.begin(4, 5);  
  Wire.setClock(400000);
  delay(200);

  Serial.print(F("Initializing SH1106 OLED... "));
  displayReady = display.begin(0x3C, true);
  if (!displayReady) displayReady = display.begin(0x3D, true);

  if (displayReady) {
    display.setTextColor(SH110X_WHITE);
    Serial.println(F("OK"));
    showListeningScreen();
  } else {
    Serial.println(F("OLED not detected! Check wiring."));
  }

  // Buzzer setup
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // LoRa setup
  Serial.print(F("Initializing LoRa... "));
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println(F("FAILED (Check wiring and power)"));
    while (true) delay(1000);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setSyncWord(LORA_SYNC);
  LoRa.enableCrc();
  Serial.println(F("LoRa Ready (433 MHz)"));
  Serial.println(F("Listening for alerts..."));
}

void showListeningScreen() {
  if (!displayReady) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Receiver Ready"));
  display.println(F("Listening..."));
  display.display();
}

void displayAlert(AlertType type, float lat, float lng, float extra) {
  if (!displayReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (type == FIRE) {
    display.println(F("FIRE ALERT!"));
  } else if (type == QUAKE) {
    display.println(F("EARTHQUAKE!"));
  }

  display.println();
  display.print(F("Lat: ")); display.println(lat, 5);
  display.print(F("Lng: ")); display.println(lng, 5);

  if (type == QUAKE) {
    display.print(F("Accel: "));
    display.print(extra, 2);
    display.println(F("g"));
  }

  display.display();
}

void startBuzzer(AlertType type) {
  buzzer.beepsRemaining = (type == FIRE) ? 5 : 3;
  buzzer.beepDuration = (type == FIRE) ? 150 : 300;
  buzzer.isHigh = true;
  buzzer.active = true;
  buzzer.nextToggle = millis() + buzzer.beepDuration;
  digitalWrite(BUZZER_PIN, HIGH);
}

void updateBuzzer() {
  if (!buzzer.active) return;
  unsigned long now = millis();

  if (now >= buzzer.nextToggle) {
    buzzer.isHigh = !buzzer.isHigh;
    digitalWrite(BUZZER_PIN, buzzer.isHigh ? HIGH : LOW);

    if (!buzzer.isHigh) {
      buzzer.beepsRemaining--;
      if (buzzer.beepsRemaining == 0) {
        buzzer.active = false;
        digitalWrite(BUZZER_PIN, LOW);
        return;
      }
      buzzer.nextToggle = now + 120; // short gap
    } else {
      buzzer.nextToggle = now + buzzer.beepDuration;
    }
  }
}

void parseMessage(const char* msg, AlertType* type, float* lat, float* lng, float* extra) {
  *type = NONE;
  *lat = *lng = *extra = 0.0f;

  if (strncmp(msg, "FIRE", 4) == 0) *type = FIRE;
  else if (strncmp(msg, "QUAKE", 5) == 0) *type = QUAKE;
  else return;

  const char* p = strchr(msg, ',');   
  if (!p) return;
  *lat = atof(++p);
  p = strchr(p, ',');
  if (!p) return;
  *lng = atof(++p);
  p = strchr(p, ',');
  if (!p) return;
  *extra = atof(++p);
}

void loop() {
  updateBuzzer();

  // Auto clear OLED after timeout
  if (alertDisplayed && (millis() - lastAlertTime > DISPLAY_TIMEOUT)) {
    showListeningScreen();
    alertDisplayed = false;
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    char msg[80] = {0};
    uint8_t idx = 0;
    while (LoRa.available() && idx < sizeof(msg) - 1) {
      msg[idx++] = (char)LoRa.read();
    }
    msg[idx] = '\0';

    AlertType type;
    float lat, lng, extra;
    parseMessage(msg, &type, &lat, &lng, &extra);
    if (type != NONE) {
      Serial.print(F("📡 Received: "));
      Serial.println(msg);
      displayAlert(type, lat, lng, extra);
      startBuzzer(type);
      lastAlertTime = millis();
      alertDisplayed = true;
    }
  }

  yield(); // keep watchdog happy
}
