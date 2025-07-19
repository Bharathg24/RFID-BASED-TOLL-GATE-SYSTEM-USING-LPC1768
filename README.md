# RFID-BASED-TOLL-GATE-SYSTEM-USING-LPC1768

Project Overview
This project implements a RFID-Based Tollgate Automation System using the LPC1768 ARM Cortex-M3 Microcontroller. Vehicles are identified using RFID tags, and toll fees are automatically deducted from the user's prepaid balance. A servo motor simulates the gate barrier, and a buzzer alerts for insufficient balance. Communication with users and debugging is facilitated via UART.

Features
🚗 RFID Card Detection (MFRC522 module via SPI)

📊 Prepaid Balance Management for each vehicle (hardcoded UID)

🔄 Automatic Toll Deduction (₹50 per entry)

⚙️ Servo Motor Gate Control

🔊 Buzzer Alert for insufficient balance

📡 Real-Time UID Transmission via UART

🔁 Continuous card scanning every second

Components Used
LPC1768 Microcontroller

MFRC522 RFID Module

Servo Motor

Buzzer

UART Communication (9600 baud)

SPI Interface

Block Diagram
less
Copy
Edit
[RFID Tag] --> [MFRC522 Module] --> [LPC1768] --> [Servo Motor + Buzzer]
                                      |
                                    [UART]
System Workflow
Card Detection: RFID reader scans for nearby cards.

UID Comparison: Scanned UID compared to known UIDs.

Balance Checking:

If balance ≥ ₹50, toll is deducted, gate opens, then closes.

If balance < ₹50, buzzer alerts the user.

Status Updates: UID, balance, and system status are sent via UART for monitoring.

Getting Started
Connect the RFID module to the LPC1768 using SPI.

Interface the servo motor and buzzer as per GPIO pin configuration.

Flash the code to the LPC1768 microcontroller.

Monitor UART at 9600 baud for real-time updates.

Future Improvements
Dynamic UID registration.

Database integration for balance tracking.

LCD display for real-time balance display.

Remote recharge of balances.
