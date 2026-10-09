# Arduino-Based Intelligent Battery Thermal Management System

### Real-Time Monitoring | Dual-Sensor Temperature Measurement | Multi-Stage Thermal Protection

An Arduino-based intelligent battery thermal management system designed to continuously monitor lithium-ion battery temperature and automatically respond to abnormal thermal conditions through a three-stage protection mechanism.

The system combines **dual temperature sensing, automated cooling, emergency load disconnection, sensor fault detection, and real-time LCD monitoring** to demonstrate a low-cost approach to battery thermal management.

---

## System Architecture

<p align="center">
  <img src="docs/system_architecture.svg"
       alt="Battery Thermal Management System Architecture"
       width="100%">
</p>

The system uses an Arduino UNO as its central controller. Temperature measurements are acquired from two independent sensors, validated, and combined into an effective temperature reading.

The Arduino continuously evaluates the measured temperature and activates the appropriate protection mechanisms.

**Overall workflow:**

1. Monitor battery temperature using the DS18B20 and MLX90614 sensors.
2. Validate sensor measurements and detect faults.
3. Calculate the effective battery temperature.
4. Execute temperature-based protection logic.
5. Control the buzzer, cooling fan, and motor load.
6. Display the temperature and operating status on the LCD.
7. Repeat the monitoring and control cycle.

---

## Key Features

- **Dual-Sensor Temperature Monitoring:** Combines contact and infrared temperature measurements.
- **Sensor Validation:** Detects disconnected or invalid temperature readings.
- **Temperature Fusion:** Averages both sensor measurements when available.
- **Sensor Redundancy:** Continues operation using the remaining sensor if one fails.
- **Three-Stage Thermal Protection:** Progressive warning, cooling, and load disconnection.
- **Hysteresis Control:** Prevents frequent relay switching near temperature thresholds.
- **Automated Cooling:** Controls a 12V DC cooling fan through a relay module.
- **Emergency Load Disconnection:** Disconnects the motor load during excessive temperature conditions.
- **Real-Time LCD Monitoring:** Displays temperature, load status, and sensor availability.
- **I2C Bus Recovery:** Attempts to recover communication with the infrared sensor.
- **Fail-Safe Operation:** Applies predefined output states when both sensors become unavailable.

---

## Hardware Components

| Component | Function |
|---|---|
| Arduino UNO | Main system controller |
| DS18B20 | Contact temperature measurement |
| MLX90614 | Non-contact infrared temperature measurement |
| 16x2 I2C LCD | Real-time monitoring and system status |
| 5V Relay Module (2 units) | Fan and motor load switching |
| 12V DC Cooling Fan | Forced-air cooling |
| 12V DC Motor | Electrical load |
| Lithium-Ion Batteries | Temperature monitoring target |
| Buzzer | Audible overheating warning |
| 12V DC Adapter | External power supply |
| Toggle Switch | Manual load switching |
| Breadboard and Connecting Wires | Circuit connections |

---

## Temperature Control Strategy

The system uses three temperature thresholds to implement progressive thermal protection.

| Temperature | Protection Stage | System Response |
|---|---|---|
| Below 32°C | Normal Operation | Monitoring without thermal intervention |
| 32°C and above | Stage 1: Early Warning | Buzzer activated |
| 35°C and above | Stage 2: Active Cooling | Cooling fan activated |
| 40°C and above | Stage 3: Emergency Shutdown | Motor load disconnected |
| 32°C or below during recovery | Recovery | Fan disabled, buzzer silenced, load restored |

### Stage 1 — Early Warning

When the effective battery temperature reaches **32°C**, the Arduino activates the buzzer.

The buzzer operates using a non-blocking ON/OFF timing mechanism to provide an audible warning without stopping the main control loop.

### Stage 2 — Active Cooling

At **35°C**, Relay 1 activates the 12V DC cooling fan.

The fan circulates air around the battery to improve heat dissipation and reduce temperature rise.

### Stage 3 — Emergency Shutdown

When the effective temperature reaches **40°C**, Relay 2 disconnects the motor load.

Removing the load reduces the electrical demand and associated heat generation.

The cooling fan continues operating according to its independent control logic.

---

## Hysteresis Control

Hysteresis is implemented to reduce unnecessary relay switching when temperature fluctuates near the control thresholds.

**Cooling fan:**

- Fan ON when temperature reaches 35°C.
- Fan OFF when temperature decreases to 32°C or below.
- Between these thresholds, the previous fan state is retained.

**Motor load:**

- Load disconnected at 40°C or above.
- Load reconnected at 32°C or below.
- Between these thresholds, the previous load state is retained.

This approach reduces rapid switching and improves control stability.

---

## Dual-Sensor Temperature Processing

The system uses two temperature sensors:

**DS18B20**

A digital contact temperature sensor used to measure the battery temperature through direct thermal contact.

**MLX90614**

A non-contact infrared sensor used to measure the battery surface temperature through infrared radiation.

### Effective Temperature Calculation

When both sensors return valid readings:

\[
T_{\mathrm{effective}} =
\frac{T_{\mathrm{DS18B20}}+T_{\mathrm{MLX90614}}}{2}
\]

If only one sensor provides a valid reading, the system uses that measurement.

The control logic can therefore continue operating when one of the two sensors becomes unavailable.

---

## Sensor Fault Detection and Fail-Safe Operation

The Arduino checks sensor readings before using them for temperature-based decisions.

### Single-Sensor Failure

If either temperature sensor becomes unavailable:

- The remaining valid sensor is used.
- Temperature monitoring continues.
- The LCD displays the corresponding sensor fault indicator.

### Dual-Sensor Failure

If both sensors become unavailable, the implemented firmware enters fail-safe mode.

The following output states are applied:

| Device | Fail-Safe State |
|---|---|
| Motor Load | OFF |
| Cooling Fan | OFF |
| Buzzer | OFF |
| LCD | Displays `BOTH SENS FAIL` |

These states reproduce the implemented prototype firmware.

**Important:** The fail-safe output configuration is specific to this experimental prototype and should not be treated as a validated safety strategy for commercial battery systems.

---

## Software and Libraries

The firmware was developed for the Arduino UNO using the Arduino programming environment.

Required libraries:

| Library | Purpose |
|---|---|
| OneWire | Communication with DS18B20 |
| DallasTemperature | DS18B20 temperature acquisition |
| Wire | I2C communication |
| Adafruit_MLX90614 | MLX90614 sensor interface |
| LiquidCrystal_I2C | LCD display control |

### Firmware Installation

1. Install the Arduino IDE.
2. Install the required libraries through the Library Manager.
3. Open `firmware/Battery_Thermal_Management/Battery_Thermal_Management.ino`.
4. Connect the Arduino UNO.
5. Select the correct board and serial port.
6. Verify the wiring and relay switching polarity.
7. Compile and upload the firmware.

The firmware uses non-blocking timing based on `millis()` for sensor acquisition and buzzer operation.

---

## Experimental Results

The system was evaluated by monitoring temperature changes under motor load and observing its response at the programmed protection thresholds.

### Observed System Behavior

| Parameter | Experimental Observation |
|---|---|
| Initial Effective Temperature | 25.11°C |
| Cooling Activation | 35.24°C |
| Load Disconnection | 40.41°C |
| Recovery Temperature | 31.74°C |
| Final Effective Temperature | 26.60°C |

The experiment demonstrated the sequential activation of thermal protection mechanisms.

As temperature increased:

- The buzzer provided an early warning.
- The cooling fan activated after the second threshold.
- The motor load was disconnected at the emergency threshold.
- The battery temperature subsequently decreased.
- The system restored normal operation after the recovery threshold was reached.

The results demonstrate the operation of the prototype's monitoring, control, and recovery mechanisms.

---

## Project Structure

```text
Battery-Thermal-Management/
│
├── README.md
│
├── firmware/
│   └── Battery_Thermal_Management/
│       └── Battery_Thermal_Management.ino
│
└── docs/
    ├── system_architecture.svg
    ├── system_architecture.png
    └── Battery_Thermal_Management_Project_Report.pdf
```

---

## Project Report

The complete project report includes:

- Introduction and project objectives
- Literature review
- Experimental setup and hardware components
- Circuit connections
- System algorithm and control logic
- Arduino firmware
- Experimental results and discussion
- Conclusions and recommendations

**[Read the Full Project Report](docs/Battery_Thermal_Management_Project_Report.pdf)**

---

## Future Improvements

Potential improvements include:

- Integration of Wi-Fi or GSM for remote monitoring.
- Expansion to multi-point temperature sensing.
- PWM-based variable-speed fan control.
- Battery voltage, current, and state-of-charge monitoring.
- Development of an advanced monitoring dashboard.
- Improved fault-tolerant control and hardware-level safety interlocks.

---

## Limitations and Safety Notice

This project is an experimental Arduino-based thermal management prototype developed for educational and research purposes.

It demonstrates temperature monitoring and automated thermal intervention, but it is **not a replacement for a dedicated Battery Management System (BMS)**.

The current prototype does not implement all protections required for safety-critical lithium-ion battery applications, such as comprehensive cell-level voltage monitoring, overcurrent protection, and independently validated thermal runaway mitigation.

Hardware connections, relay states, temperature thresholds, and fail-safe behavior must be verified before operation with actual battery systems.

---

## Author

**Nafiz Fuad Niloy**

Department of Energy Science and Engineering  
Khulna University of Engineering & Technology (KUET)  
Khulna, Bangladesh

**Project Supervisor:**  
Dr. Md. Shameem Hossain  
Professor, Department of Energy Science and Engineering  
Khulna University of Engineering & Technology (KUET)

---

## Acknowledgement

This project was carried out as an academic project at the Department of Energy Science and Engineering, Khulna University of Engineering & Technology.

The author acknowledges the guidance and support of the project supervisor and the department throughout the development and experimental evaluation of the system.
