#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

// GPS Configuration
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
HardwareSerial gpsSerial(2); 
TinyGPSPlus gps;

// LoRa Pins (ESP32)
#define LORA_SS    5
#define LORA_RST   -1   
#define LORA_DIO0 27

// Fire Detection 
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

// Fallback Coordinates (if GPS signal is lost)
const float FALLBACK_LAT = 30.768885;
const float FALLBACK_LNG = 76.57521;

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
  delay(500);
  Serial.println(F("\n=== LoRa Transmitter (Live GPS) ==="));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(FLAME_SENSOR_PIN, INPUT_PULLUP);

  // I2C for accelerometer
  Wire.begin(21, 22);
  Wire.setClock(400000);

  // LoRa init
  SPI.begin(18, 19, 23, LORA_SS); 
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println(F("LoRa FAILED"));
    while (1) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(100); }
  }
  LoRa.setSyncWord(0x12);
  loraReady = true;

  // Accelerometer init
  if (lis3dh.begin(0x19) || lis3dh.begin(0x18)) {
    lis3dh.setRange(LIS3DH_RANGE_2_G);
    accelReady = true;
  }

  digitalWrite(LED_PIN, LOW);
  Serial.println(F("System Ready"));
}

void blinkLED() {
  digitalWrite(LED_PIN, HIGH);
  ledTurnOffTime = millis() + LED_BLINK_MS;
}

// Function to feed GPS during delays
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (gpsSerial.available())
      gps.encode(gpsSerial.read());
  } while (millis() - start < ms);
}

void sendLoRaAlert(const char* type, float extra) {
  if (!loraReady) return;

  float currentLat = (gps.location.isValid()) ? gps.location.lat() : FALLBACK_LAT;
  float currentLng = (gps.location.isValid()) ? gps.location.lng() : FALLBACK_LNG;

  char payload[80];
  snprintf(payload, sizeof(payload), "%s,%.6f,%.6f,%.2f,Sats:%u", 
           type, currentLat, currentLng, extra, (unsigned int)gps.satellites.value());
  
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

  // Feed GPS
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (now >= ledTurnOffTime && digitalRead(LED_PIN) == HIGH) {
    digitalWrite(LED_PIN, LOW);
  }

  // FIRE DETECTION
  static int fireCount = 0;
  bool flameDetected = (digitalRead(FLAME_SENSOR_PIN) == (FLAME_SENSOR_ACTIVE_LOW ? LOW : HIGH));
  if (flameDetected) fireCount++;
  else if (fireCount > 0) fireCount--;

  if (fireCount >= FLAME_SAMPLES_NEEDED && (now - lastFireAlert > FIRE_MIN_INTERVAL)) {
    sendLoRaAlert("FIRE", 1.0);
    lastFireAlert = now;
    fireCount = 0;
  }

  // EARTHQUAKE DETECTION
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
      sendLoRaAlert("QUAKE", filteredAccel);
      lastQuakeAlert = now;
    }
  }

  // STATUS PRINT
  if (now - lastStatusPrint >= 1000) {
    Serial.print(F("GPS:"));
    if (gps.location.isValid()) {
        Serial.print(gps.satellites.value());
        Serial.print(F(" sats"));
    } else {
        Serial.print(F("WAITING"));
    }
    Serial.print(F(" | Flame:")); Serial.print(fireCount);
    if (accelReady) { Serial.print(F(" | Accel:")); Serial.print(filteredAccel, 3); }
    Serial.println();
    lastStatusPrint = now;
    loopCount = 0;
  }

  smartDelay(10); 
}
