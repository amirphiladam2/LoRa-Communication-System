# LoRa Emergency Alert System

A wireless emergency alert system using LoRa communication technology for fire and earthquake detection. The system consists of three main components: a **Transmitter** node that monitors sensors, a **Receiver** node that displays alerts, and an **Intermediate Node** that acts as a relay to extend communication range.

## 🎯 Project Overview

This system provides a long-range, low-power solution for emergency alert transmission. It can detect fires using a flame sensor and earthquakes using an accelerometer, then broadcast alerts via LoRa radio to receivers located kilometers away.

### Key Features

- 🔥 **Fire Detection**: Monitors flame sensor with debouncing for reliable detection
- 🌍 **Earthquake Detection**: Uses LIS3DH accelerometer with filtering and windowing algorithm
- 📡 **Long-Range Communication**: LoRa technology enables communication over several kilometers
- 📊 **Visual Alerts**: OLED display shows alert details including location coordinates
- 🔊 **Audio Alerts**: Buzzer provides audible notifications
- 🔄 **Range Extension**: Intermediate relay node extends communication range
- 🛡️ **Message Deduplication**: Prevents message loops in relay network

## 📦 System Components

### 1. Transmitter (ESP32)

**Location**: `Transmitter/Transmitter.ino`

The transmitter node continuously monitors sensors and broadcasts alerts when events are detected.

**Hardware Requirements:**
- ESP32 development board
- LoRa module (SX1276/77/78/79 based)
- LIS3DH accelerometer (I2C)
- Flame sensor (digital)
- GPS module (TinyGPS++, currently using sample coordinates)
- LED indicator (GPIO 2)

**Features:**
- Fire detection with debouncing (requires 3 consecutive detections)
- Earthquake detection using accelerometer with low-pass filtering
- LoRa transmission at 433 MHz
- Status monitoring via Serial output
- Configurable alert intervals to prevent spam

**Pin Configuration:**
```cpp
LoRa Module:
  SS    -> GPIO 5
  RST   -> -1 (not used)
  DIO0  -> GPIO 27
  SPI: SCK=18, MISO=19, MOSI=23

Accelerometer (LIS3DH):
  SDA   -> GPIO 21
  SCL   -> GPIO 22

Flame Sensor:
  Signal -> GPIO 25

LED:
  LED   -> GPIO 2
```

**LoRa Parameters:**
- Frequency: 433 MHz
- Spreading Factor: 7
- Bandwidth: 125 kHz
- Coding Rate: 5
- Sync Word: 0x12
- TX Power: 20 dBm
- CRC: Enabled

### 2. Receiver (ESP8266)

**Location**: `Receiver/Receiver.ino`

The receiver node listens for LoRa alerts and displays them on an OLED screen while sounding a buzzer.

**Hardware Requirements:**
- ESP8266 (NodeMCU or similar)
- LoRa module (SX1276/77/78/79 based)
- SH1106 OLED display (128x64, I2C)
- Buzzer
- Optional: External LED for status

**Features:**
- Real-time alert reception
- OLED display showing alert type, coordinates, and additional data
- Buzzer alerts with different patterns for fire (5 beeps) and earthquake (3 beeps)
- Auto-clearing display after 15 seconds
- Serial monitoring for debugging

**Pin Configuration:**
```cpp
LoRa Module:
  CS    -> GPIO 15 (D8)
  RST   -> -1 (not used)
  DIO0  -> GPIO 16 (D0)

OLED (SH1106):
  SDA   -> GPIO 4 (D2)
  SCL   -> GPIO 5 (D1)
  Address: 0x3C or 0x3D

Buzzer:
  Signal -> GPIO 2 (D4)
```

### 3. Intermediate Node (ESP8266 Relay)

**Location**: `Intermediate_Nod/Intermediate_Nod.ino`

The relay node extends the communication range by receiving and forwarding messages.

**Hardware Requirements:**
- ESP8266 (NodeMCU or similar)
- LoRa module (SX1276/77/78/79 based)
- External LED (optional, for RX indication)
- Onboard LED (for TX indication)

**Features:**
- Message forwarding with deduplication
- Prevents message loops using time-windowed cache
- Visual indicators for reception and transmission
- Statistics tracking (received, forwarded, duplicates)
- 5-second deduplication window

**Pin Configuration:**
```cpp
LoRa Module:
  CS    -> GPIO 15 (D8)
  RST   -> -1 (tied to 3.3V)
  DIO0  -> GPIO 16 (D0)

LEDs:
  RX LED -> GPIO 2 (D4, external)
  TX LED -> GPIO 16 (D0, onboard)
```

## 🔧 Installation & Setup

### Prerequisites

1. **Arduino IDE** (1.8.x or 2.x) with the following boards installed:
   - ESP32 Board Support (for Transmitter)
   - ESP8266 Board Support (for Receiver and Relay)

2. **Required Libraries** (install via Arduino Library Manager):
   - LoRa (by Sandeep Mistry)
   - TinyGPS++ (by Mikal Hart)
   - Adafruit LIS3DH
   - Adafruit Unified Sensor
   - Adafruit GFX Library
   - Adafruit SH110X
   - Adafruit BusIO

### Installation Steps

1. **Clone the Repository**
   ```bash
   git clone https://github.com/amirphiladam2/LoRa-Communication-System.git
   cd LoRa-Communication-System
   ```

2. **Install Libraries**
   - Open Arduino IDE
   - Go to `Tools > Manage Libraries`
   - Install all required libraries listed above

3. **Configure Board Settings**
   
   **For ESP32 (Transmitter):**
   - Board: "ESP32 Dev Module"
   - Upload Speed: 115200
   - CPU Frequency: 240MHz
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Partition Scheme: Default

   **For ESP8266 (Receiver/Relay):**
   - Board: "NodeMCU 1.0 (ESP-12E Module)"
   - Upload Speed: 115200
   - CPU Frequency: 80MHz
   - Flash Size: 4MB
   - Debug Level: None

4. **Upload Code**
   - Open the appropriate `.ino` file for your node
   - Select the correct board and port
   - Upload the code

5. **Wire Hardware**
   - Follow the pin configurations listed above for each node
   - Ensure proper power supply (3.3V for LoRa modules)
   - Check all connections before powering on

## 🚀 Usage

### Transmitter Setup

1. Upload `Transmitter/Transmitter.ino` to your ESP32
2. Connect all sensors (flame sensor, accelerometer)
3. Power on and monitor Serial output (115200 baud)
4. The system will calibrate the accelerometer on startup
5. Alerts will be transmitted when events are detected

**Note**: The transmitter currently uses sample coordinates. To use real GPS:
- Connect a GPS module to Serial1
- Modify the code to read from GPS instead of using `SAMPLE_LAT` and `SAMPLE_LNG`

### Receiver Setup

1. Upload `Receiver/Receiver.ino` to your ESP8266
2. Connect OLED display and buzzer
3. Power on and wait for "Receiver Ready" message on OLED
4. The receiver will automatically display alerts when received
5. Monitor Serial output for debugging (115200 baud)

### Relay Node Setup

1. Upload `Intermediate_Nod/Intermediate_Nod.ino` to your ESP8266
2. Power on and wait for "Relay Active" message
3. The relay will automatically forward messages it receives
4. Monitor Serial output for statistics (115200 baud)

## 📊 Message Format

Alerts are transmitted in the following format:
```
TYPE,LATITUDE,LONGITUDE,EXTRA_DATA
```

Examples:
- Fire: `FIRE,30.768885,76.575210,1.00`
- Earthquake: `QUAKE,30.768885,76.575210,0.15`

Where:
- `TYPE`: Alert type (FIRE or QUAKE)
- `LATITUDE`: GPS latitude (6 decimal places)
- `LONGITUDE`: GPS longitude (6 decimal places)
- `EXTRA_DATA`: Additional information (fire intensity or acceleration in g)

## 🔍 Troubleshooting

### Transmitter Issues

**LoRa initialization fails:**
- Check SPI connections (SCK, MISO, MOSI, SS)
- Verify LoRa module power supply (3.3V)
- Ensure DIO0 is connected correctly
- Try different spreading factors if range is an issue

**Accelerometer not detected:**
- Check I2C connections (SDA, SCL)
- Verify I2C address (0x18 or 0x19)
- Ensure proper pull-up resistors (usually built into ESP32)

**False fire detections:**
- Adjust `FLAME_SAMPLES_NEEDED` threshold
- Check flame sensor wiring and power
- Verify `FLAME_SENSOR_ACTIVE_LOW` setting matches your sensor

### Receiver Issues

**OLED not displaying:**
- Check I2C connections
- Verify OLED address (try 0x3C and 0x3D)
- Ensure display is powered correctly

**No alerts received:**
- Verify LoRa parameters match transmitter
- Check antenna connections
- Ensure receiver is within range (or use relay node)
- Verify sync word matches (0x12)

**Buzzer not working:**
- Check buzzer connections
- Verify GPIO pin configuration
- Test buzzer directly with 3.3V

### Relay Node Issues

**Messages not forwarding:**
- Check LoRa module connections
- Verify both RX and TX LEDs are functioning
- Monitor Serial output for error messages
- Ensure relay is within range of both transmitter and receiver

**Message loops:**
- Deduplication should prevent this
- If issues persist, increase `DEDUPE_WINDOW` time
- Check cache size (`CACHE_SIZE`)

## 📈 Performance

- **Communication Range**: Up to several kilometers (depending on terrain and obstacles)
- **Power Consumption**: Low (LoRa is designed for battery operation)
- **Alert Latency**: < 100ms (excluding propagation delay)
- **False Positive Rate**: Low (due to filtering and debouncing)

## 🛠️ Customization

### Adjusting Detection Sensitivity

**Fire Detection:**
```cpp
const int FLAME_SAMPLES_NEEDED = 3;  // Increase for less sensitivity
const unsigned long FIRE_MIN_INTERVAL = 5000UL;  // Minimum time between alerts
```

**Earthquake Detection:**
```cpp
const float QUAKE_THRESHOLD = 0.12;  // Increase for less sensitivity (in g)
const float ALPHA = 0.5;  // Filter coefficient (0-1, lower = more filtering)
const int WINDOW_SIZE = 10;  // Detection window size
```

### Changing LoRa Parameters

Modify these settings in all nodes for consistency:
```cpp
LoRa.setSpreadingFactor(7);        // Range: 6-12 (higher = longer range, slower)
LoRa.setSignalBandwidth(125E3);    // Bandwidth: 7.8E3 to 500E3
LoRa.setCodingRate4(5);            // Error correction: 5-8
LoRa.setSyncWord(0x12);            // Network identifier: 0x00-0xFF
LoRa.setTxPower(20);               // Power: 2-20 dBm
```

### Using Real GPS

Replace sample coordinates with real GPS data:
```cpp
// In Transmitter.ino, replace SAMPLE_LAT/SAMPLE_LNG with:
if (gps.location.isValid()) {
    float lat = gps.location.lat();
    float lng = gps.location.lng();
    sendLoRaAlert("FIRE", lat, lng, 1.0);
}
```

## 📝 License

This project is open source. Please check individual library licenses for their respective terms.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📧 Contact

For questions or issues, please open an issue on the GitHub repository.

## 🙏 Acknowledgments

- LoRa library by Sandeep Mistry
- Adafruit for sensor and display libraries
- TinyGPS++ by Mikal Hart
- ESP32 and ESP8266 communities

---

**⚠️ Important Notes:**
- This system is for educational and prototype purposes
- Always test thoroughly before deploying in critical applications
- Ensure compliance with local radio regulations (LoRa frequency usage)
- GPS coordinates are currently hardcoded - implement real GPS for production use
- Fire and earthquake detection algorithms may need tuning for your specific environment
