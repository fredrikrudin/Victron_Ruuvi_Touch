# 📑 Lessons Learned - Waveshare ESP32-S3-Touch-LCD-4 v4 Development

This document compiles the technical discoveries, hidden hardware traps, and optimization strategies identified during the development, based on the official first-party code examples from Waveshare.

---

## 1. 🔌 RS485 & Modbus RTU (`13_RS485`)

### Insights & Hardware Architecture
* **Pin Mapping:** The hardware transceiver (SP3485) is permanently hardwired to **GPIO 43 (RX)** and **GPIO 44 (TX)** [13_RS485]. It must be driven using a dedicated hardware UART instance (`HardwareSerial Serial1`) [13_RS485].
* **Automatic Flow Control:** The PCB design incorporates a hardware-based automatic direction switcher (auto-direction) [13_RS485]. The software does *not* need to manually toggle a DE/RE pin high or low before/after transmission [13_RS485].
* **Blocking Transmission:** Because UART transmissions occur in the background via shift registers, calling `RS485Serial.flush()` immediately after a Modbus command is mandatory. This ensures the entire packet has physically left the PCB before the processor reverts to heavy graphical calculations.

---

## 📡 2. CAN Bus / TWAI (`12_TWAIreceive`)

### Insights & Hardware Architecture
* **Pin Mapping:** The CAN transceiver (TJA1051) is routed to **GPIO 41 (RX)** and **GPIO 42 (TX)** [12_TWAIreceive].
* **Protocol Framework:** The ESP32-S3 does not feature a "hardware CAN" in the traditional sense, but instead utilizes Espressif's proprietary **TWAI** (Two-Wire Automotive Interface) driver [12_TWAIreceive].
* **Buffer Saturation:** TWAI reception is strictly event-driven via internal ring buffers [12_TWAIreceive]. If `twai_receive()` is not continuously polled in microsecond loops (non-blocking, timeout=0), the buffer becomes saturated, leading to silent packet loss without throwing errors [12_TWAIreceive].

---

## 🛠️ 3. The Hidden Trap: I2C I/O Expander (CH422G / PCA9554)

### The Main Hardware Hurdle
* **The Problem:** During initial tests, both the RS485 and CAN buses remained completely unresponsive, despite correctly initialized UART and TWAI drivers.
* **Root Cause Found in Schematics:** Waveshare placed an **I2C-driven I/O expander** on the main sensor bus (**GPIO 11 (SDA)** and **GPIO 12 (SCL)**). This expander acts as an electronic power switch (Isolation Power Control) for the bus transceivers.
* **Solution:** Before using the buses, the software must initialize `Wire` and transmit a specific register sequence to the expander's I2C address (`0x20`) to set the pins as outputs and pull them high. Failing to do this keeps the peripherals completely powered off.

---

## 🖥️ 4. Display, Touch & Memory Allocation (`09_LVGL_Widgets`)

### Insights & Hardware Architecture
* **PSRAM Is Mandatory:** The screen (ST7701) features a resolution of 480x480 pixels and operates over a 16-bit RGB interface [09_LVGL_Widgets]. This requires massive frame-buffers (our deeply integrated double-buffers implemented to eliminate screen flickering) that simply cannot fit into the ESP32-S3's internal SRAM.
* **Arduino IDE Configuration:** The compilation will either fail or crash into an infinite boot-loop if **PSRAM: OPI PSRAM** is not enabled in the IDE tools menu.
* **LVGL Version Tolerance:** Waveshare's BSP (Board Support Package) is strictly locked and validated against **LVGL v8.3.x/v8.4.0** [09_LVGL_Widgets]. Trying to migrate to LVGL v9 in Arduino IDE 2.3.10 breaks low-level driver calls and rendering timers.

---

## ⏰ 5. Offline Timekeeping (`05_GFX_PCF85063_simpleTime`)

### Insights & Hardware Architecture
* **Hardware RTC:** The onboard real-time clock is a **PCF85063** sitting on I2C address `0x51` [05_GFX_PCF85063_simpleTime].
* **BCD Format:** The chip communicates exclusively in **BCD (Binary-Coded Decimal)** [05_GFX_PCF85063_simpleTime]. When reading, bit-shifting must be performed (`(val/16*10) + (val%16)`) to translate the data into standard decimal integers, and vice versa when writing data back from the web interface's manual clock setting [05_GFX_PCF85063_simpleTime].

---

## 🚀 6. Final Software Optimization (Core Separation)

To guarantee stable 24/7 field deployment, the following architectural rules were enforced:

1. **Core Asynchrony (FreeRTOS):** Radio operations (WiFi/BLE) were pinned to **Core 0**, while graphics (LVGL) and Modbus were moved to **Core 1**. This prevents heavy BLE scans or slow web clients from freezing the touch responsiveness or rendering frame rates [09_LVGL_Widgets].
2. **Thread-Safe Mutexes:** Shared memory structures are strictly guarded using FreeRTOS `SemaphoreHandle_t` Mutex locks. A core is never allowed to manipulate device arrays without safely acquiring the lock first.
3. **Preventing Heap Fragmentation:** No dynamic `String` allocations are permitted inside loops. All string formatting is executed via pre-allocated static `char` buffers and `snprintf()`, securing an uptime span of several months.
