# 🕒 39th AIU Time Control System
> **An IoT-driven timing solution for the 39th AIU West Zone Youth Festival.**

<div align="center">
  <img src="WhatsApp Image 2026-02-11 at 14.39.21.jpeg" width="600" alt="Hardware Device Presentation" scale="120px">
  <p><i>The physical BLE Timer IoT device in action at CVM University.</i></p>
</div>

---

### 🛠️ Tech Stack

<div align="center">

![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Arduino](https://img.shields.io/badge/-Arduino-00979D?style=for-the-badge&logo=Arduino&logoColor=white)
![Espressif](https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![Bluetooth](https://img.shields.io/badge/Bluetooth_4.2-0082FC?style=for-the-badge&logo=bluetooth&logoColor=white)

</div>

---

### 🚀 Overview
The **39th AIU Time Control System** is a professional-grade speech timer designed to manage competitive university events with precision. It bridges the gap between physical hardware and mobile convenience using **ESP32** and **Bluetooth Low Energy (BLE)**.

---

### ✨ Key Features
* **📱 Smart Connectivity** — Automatic filtering and connection to `NOT_CNT_XXXX` devices.
* **⏲️ Advanced Timing** — Managed phases for **PREP**, **MIN**, **PERF**, and **GRACE** periods.
* **💡 Live Sync** — Real-time countdown on both the app and the physical 8x32 LED matrix.
* **🔒 Secure Operations** — PIN-based authentication to prevent unauthorized control during sessions.
* **📊 Performance Logs** — Automatically logs session data including configurations and total overtime.

---

### 🎨 Visual Feedback & Logic
The device communicates status through a high-visibility RGB LED and a matrix display:

| Phase | LED Color | Logic |
| :--- | :---: | :--- |
| **Preparation** | 🟡 Yellow | Counts down the user-defined setup time. |
| **Session Start** | 🟢 Green | Double green blink to signal the speaker to begin. |
| **Min Threshold** | 🔵 Blue | Quick blue flash when the minimum time is reached. |
| **Overtime (Grace)**| ⚫ Off | Display counts **UP**; no LED distractions for the speaker. |
| **Conclusion** | 🔴 Red | 5 red flashes to signal the official end of the round. |

---

### 📐 Hardware Architecture
<div align="center">
  <img src="circuit_diagram.png" width="500" alt="Circuit Diagram">
</div>

* **Microcontroller:** ESP32 (utilizing BLE 4.2 stack).
* **Display:** MAX7219 Driven 8x32 LED Matrix (0-15 brightness levels).
* **Format:** Supports `MM:SS` and automatically switches to `H:MM` for sessions ≥ 100 minutes.

---

### ⚙️ How to Run
1.  **Hardware:** Flash the firmware to your ESP32 and wire according to the diagram above.
2.  **Permissions:** Ensure Bluetooth and Location (on Android) are enabled for scanning.
3.  **Authentication:** Set a 4-digit PIN upon first launch to secure the interface.
4.  **Testing:** Use the **"Test Device"** menu in Settings to run the R/G/B and Segment diagnostic.

---

<div align="center">
  <p><b>Developed for the Association of Indian Universities (AIU)</b><br>
  <i>BLE Timer IoT App v1.0.0</i></p>
</div>
