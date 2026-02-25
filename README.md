# Disaster Resilient Hybrid ESP-LoRa Communication System

A highly resilient, multi-layer wireless emergency alert system designed for fire and earthquake detection. This system utilizes a **Hybrid Switching** mechanism that dynamically chooses between high-speed **ESP-NOW Mesh** (short-range) and long-range **LoRa** (kilometers range) based on a configured distance threshold.

## 📡 Architecture Overview

The system consists of three distinct nodes:
1.  **Transmitter (ESP32)**: Collects sensor data (Fire/Earthquake) and GPS coordinates, broadcasting alerts via LoRa.
2.  **Intermediate Relay (ESP8266)**: Acts as a range extender. It calculates the distance between the Transmitter and the Receiver. 
    - **Auto-Switching**: If the distance is < 50m, it forwards the alert via **ESP-NOW** for lower latency and lower power.
    - **LoRa Fallback**: If the distance > 50m, it forwards the alert via **LoRa** for maximum range.
3.  **Receiver (ESP8266)**: A hybrid receiver that listens for alerts from both LoRa and ESP-NOW, displaying them on an OLED with audible buzzer warnings.

##  Key Features

- 🔄 **Hybrid Auto-Switching**: Intelligent selection between ESP-NOW and LoRa based on real-time Haversine distance calculations.
- 🔥 **Advanced Fire Detection**: Multi-sample debouncing for high reliability using flame sensors.
- 🌍 **Earthquake Detection**: LIS3DH accelerometer integration with low-pass filtering and shake-window algorithm.
- 📍 **Live GPS Tracking**: Transmits real-time coordinates (Lat/Lng) and satellite status.
- 🛡️ **Message Deduplication**: Prevents message storms and loops in the relay network.
- 🔊 **Visual & Audible Alerts**: SH1106 OLED display and configurable buzzer patterns (5 beeps for Fire, 3 beeps for Quake).

## 🛠️ Hardware Requirements

| Component | Hardware | Role |
| :--- | :--- | :--- |
| **Transmitter** | ESP32 + LoRa (SX1278) + GPS + LIS3DH + Flame Sensor | Data Originator |
| **Relay** | ESP8266 (NodeMCU) + LoRa (SX1278) | Intelligent Forwarder |
| **Receiver** | ESP8266 (NodeMCU) + LoRa (SX1278) + OLED + Buzzer | Alert Terminal |

## 📐 Pin Configurations

### Transmitter (ESP32)
- **LoRa**: SS=5, SCK=18, MISO=19, MOSI=23, DIO0=27
- **GPS**: RX=16, TX=17
- **Accelerometer (I2C)**: SDA=21, SCL=22
- **Flame Sensor**: GPIO 25

### Relay & Receiver (ESP8266)
- **LoRa**: CS=15 (D8), SCK=D5, MISO=D6, MOSI=D7, DIO0=16 (D0)
- **OLED (I2C)**: SDA=4 (D2), SCL=5 (D1)
- **Buzzer**: GPIO 2 (D4)

## 🚀 Installation & Setup

1.  **Library Prerequisites**: Install via Arduino Library Manager:
    - `LoRa` (Sandeep Mistry)
    - `TinyGPS++` (Mikal Hart)
    - `Adafruit LIS3DH` & `Adafruit SH110X`
2.  **Configure MAC Addresses**: In `Intermediate_Nod.ino`, update the `receiverAddress` if not using broadcast.
3.  **Update Thresholds**: Modify `DISTANCE_THRESHOLD_METERS` in the relay node to suit your deployment environment.
4.  **Flash Boards**:
    - Upload `Transmitter.ino` to the ESP32.
    - Upload `Intermediate_Nod.ino` to the first ESP8266 (Relay).
    - Upload `Receiver.ino` to the second ESP8266 (Receiver).

## 📊 Message Format
The system uses a comma-separated payload for interoperability:
`TYPE, LATITUDE, LONGITUDE, EXTRA_VALUE, SATS`
*Example*: `FIRE,30.768885,76.575210,1.00,Sats:8`

---
*Developed for disaster-resilient infrastructure using Hybrid Wireless Mesh-LoRa topology.*
