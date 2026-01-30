# Accelerator Pedal Position Sensor (APPS) – Arduino Based

This project implements a **dual-potentiometer Accelerator Pedal Position Sensor (APPS)** using an Arduino.  
The system performs continuous safety and plausibility checks and transmits the calculated throttle position over a **CAN bus**.

The design is inspired by automotive safety principles and is intended for **educational, prototyping, and Formula-style applications**.

---

## Features
- Dual redundant potentiometer throttle inputs
- Operating range (voltage) validation for each sensor
- Plausibility checking between sensors during throttle activation
- Latching fail-safe behavior requiring power reset
- CAN-based throttle output
- Serial diagnostics for debugging and testing

---

## Safety Logic Overview
The system monitors two independent throttle sensors and enforces the following checks:

- **Operating Range Check**  
  Detects voltage readings outside expected sensor limits.

- **Plausibility (Disagreement) Check**  
  Ensures both sensors agree within a defined threshold when throttle is applied.

- **Fail-Safe Behavior**  
  Repeated faults trigger a system failure state:
  - Throttle output is disabled
  - A failure CAN message is transmitted
  - System requires a power reset to recover

---

## Hardware Requirements
- Arduino-compatible board with CAN support
- CAN transceiver
- Two potentiometers (dual APPS)
- External power supply (as required by hardware)

---

## CAN Configuration
| Parameter | Value |
|--------|------|
| Bit rate | 250 kbps |
| Throttle message ID | `0x20` |
| Failure message ID | `0xFF` |
| Payload size | 8 bytes |

Throttle data is encoded as decimal digits across the CAN payload.

---

## File Structure
├── apps_can_controller.ino
└── README.md

---

## Usage
1. Connect the potentiometers to analog inputs `A0` and `A1`
2. Configure CAN wiring and termination
3. Upload the sketch to the Arduino
4. Monitor throttle output and diagnostics via Serial Monitor

---

## Notes
- This project is intended for **bench testing and educational use**
- Breadboards are suitable for development but **not for safety-critical deployment**
- For real-world use, a soldered or PCB-based implementation is recommended

---

## License
This project is open-source and intended for learning and prototyping purposes.  
Add a license file (MIT recommended) if distributing or modifying.

---

## Author
Developed by **RavensRacing**
