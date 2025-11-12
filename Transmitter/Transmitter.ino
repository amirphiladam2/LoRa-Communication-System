#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

//  LoRa Pins (ESP32) 
#define LORA_SS    5
#define LORA_RST   -1   
#define LORA_DIO0 27
//  Fire Detection 
#define FLAME_SENSOR_PIN 25
#define FLAME_SENSOR_ACTIVE_LOW true
const int FLAME_SAMPLES_NEEDED = 3;
const unsigned long FIRE_MIN_INTERVAL = 5000UL;

// LED Indicator 
#define LED_PIN 2
const unsigned long LED_BLINK_MS = 200;

// Accelerometer (LIS3DH) 
Adafruit_LIS3DH lis3dh;

// Quake Detection 
float filteredAccel = 0;
const float ALPHA = 0.5;
const float QUAKE_THRESHOLD = 0.12;
const int WINDOW_SIZE = 10;
int quakeWindow[WINDOW_SIZE] = {0};
int windowIndex = 0;

// System State 
unsigned long lastFireAlert = 0;
unsigned long lastQuakeAlert = 0;
unsigned long ledTurnOffTime = 0;
unsigned long lastStatusPrint = 0;
unsigned long loopCount = 0;
bool loraReady = false;
bool accelReady = false;

//Setup Sample Coordinates 
const float SAMPLE_LAT = 30.768885;
const float SAMPLE_LNG =  76.57521;

//Setup
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== LoRa Transmitter (Demo Coordinates) ==="));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(FLAME_SENSOR_PIN, INPUT_PULLUP);
  Serial.println(F("Flame sensor ready"));

  // I2C for accelerometer
  Wire.begin(21, 22);
  Wire.setClock(400000);

  // LoRa init
  Serial.print(F("Init LoRa... "));
  SPI.end(); 
  SPI.begin(18, 19, 23, LORA_SS); 
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println(F("FAILED"));
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);
    }
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.setTxPower(20);
  loraReady = true;
  Serial.println(F("OK"));

  // Accelerometer init
  Serial.print(F("Init LIS3DH... "));
  if (lis3dh.begin(0x19) || lis3dh.begin(0x18)) {
    lis3dh.setRange(LIS3DH_RANGE_2_G);
    lis3dh.setDataRate(LIS3DH_DATARATE_100_HZ);
    accelReady = true;
    Serial.println(F("OK"));
  } else {
    Serial.println(F("SKIPPED"));
  }

  if (accelReady) {
    Serial.print(F("Calibrating accel"));
    for (int i = 0; i < 10; i++) {
      sensors_event_t event;
      lis3dh.getEvent(&event);
      float ax = event.acceleration.x / 9.81;
      float ay = event.acceleration.y / 9.81;
      float az = (event.acceleration.z / 9.81) - 1.0;
      float rawAccel = sqrt(ax * ax + ay * ay + az * az);
      filteredAccel = (0.5 * rawAccel) + (0.5 * filteredAccel);
      Serial.print(F("."));
      delay(50);
    }
    Serial.println(F(" done"));
  }

  digitalWrite(LED_PIN, LOW);
  Serial.println(F("System Ready"));
}

void blinkLED() {
  digitalWrite(LED_PIN, HIGH);
  ledTurnOffTime = millis() + LED_BLINK_MS;
}

void sendLoRaAlert(const char* type, float lat, float lng, float extra) {
  if (!loraReady) return;
  char payload[64];
  snprintf(payload, sizeof(payload), "%s,%.6f,%.6f,%.2f", type, lat, lng, extra);
  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();
  Serial.print(F("📡 SENT: "));
  Serial.println(payload);
  blinkLED();
}

void loop() {
  unsigned long now = millis();
  loopCount++;

  if (now >= ledTurnOffTime && digitalRead(LED_PIN) == HIGH) {
    digitalWrite(LED_PIN, LOW);
  }

  //FIRE DETECTION 
  static int fireCount = 0;
  bool flameDetected = (digitalRead(FLAME_SENSOR_PIN) == (FLAME_SENSOR_ACTIVE_LOW ? LOW : HIGH));
  if (flameDetected) fireCount++;
  else if (fireCount > 0) fireCount--;

  if (fireCount >= FLAME_SAMPLES_NEEDED && (now - lastFireAlert > FIRE_MIN_INTERVAL)) {
    
    sendLoRaAlert("FIRE", SAMPLE_LAT, SAMPLE_LNG, 1.0);
    lastFireAlert = now;
    fireCount = 0;
  }

  //  EARTHQUAKE DETECTION 
  if (accelReady) {
    sensors_event_t event;
    lis3dh.getEvent(&event);
    float ax = event.acceleration.x / 9.81;
    float ay = event.acceleration.y / 9.81;
    float az = (event.acceleration.z / 9.81) - 1.0;
    float rawAccel = sqrt(ax * ax + ay * ay + az * az);
    filteredAccel = (ALPHA * rawAccel) + ((1 - ALPHA) * filteredAccel);
    quakeWindow[windowIndex] = (filteredAccel > QUAKE_THRESHOLD) ? 1 : 0;
    windowIndex = (windowIndex + 1) % WINDOW_SIZE;
    int shakeCount = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) shakeCount += quakeWindow[i];

    if (shakeCount >= 4 && (now - lastQuakeAlert > 5000)) {
      sendLoRaAlert("QUAKE", SAMPLE_LAT, SAMPLE_LNG, filteredAccel);
      lastQuakeAlert = now;
    }
  }

  // STATUS PRINT 
  if (now - lastStatusPrint >= 1000) {
    unsigned long loopsPerSec = loopCount;
    loopCount = 0;
    Serial.print(F("GPS:sent | Flame:"));
    Serial.print(fireCount);
    Serial.print(F("/"));
    Serial.print(FLAME_SAMPLES_NEEDED);
    Serial.print(F(" | LORA:"));
    Serial.print(loraReady ? F("OK") : F("NO"));
    if (accelReady) {
      Serial.print(F(" | Accel:"));
      Serial.print(filteredAccel, 3);
    }
    Serial.print(F(" | Loop:"));
    Serial.print(loopsPerSec);
    Serial.println(F("Hz"));
    lastStatusPrint = now;
  }

  delay(10); // 100 Hz loop
}
