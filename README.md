# ATtiny85 I2C Rotary Encoder
An ultra-compact, low-cost I2C slave co-processor firmware for the **Microchip ATtiny85** (e.g., Digispark). Offloads quadrature Gray-code decoding, switch debouncing, multi-gesture recognition, and range clamping from host microcontrollers (ESP32, ESP8266, STM32, Arduino, Raspberry Pi Pico).

## Features

- **Standard I2C Slave Interface**: Default 7-bit address `0x30` running at 100 kHz Standard Mode.
- **Supported Pushbutton functionality**:
  - **Single Click**
  - **Double Click** (configurable interval window)
  - **Long Press** (configurable threshold)
  - Hardware contact debounce filter (35 ms lockout)
- **Dynamic Runtime Configuration**: Host can change Min, Max, Step size, and Button timing windows on-the-fly over I2C.
- **Atomic 16-Bit Registers**: High/Low byte atomic read/write protection prevents torn reads across 16-bit boundaries (e.g., `255` $\rightarrow$ `256`).
- Using TinyWireS from [nadavmatalon/TinyWireS](https://github.com/nadavmatalon/TinyWireS)

  ## Hardware Schematic & Connections

### Pinout Mapping

| ATtiny85 Physical Pin | Port Pin | Function |
| :---: | :---: | :---: | 
| **Pin 1** | PB5 / RESET | Reset / Weak Pull-up | 
| **Pin 2** | PB3 | Encoder CLK (Channel A) | Rotary Encoder CLK pin |
| **Pin 3** | PB4 | Encoder SW (Pushbutton) | Rotary Encoder Switch pin (SW to GND) |
| **Pin 4** | GND | Ground | Common System GND |
| **Pin 5** | PB0 | I2C SDA | 
| **Pin 6** | PB1 | Encoder DT (Channel B) | 
| **Pin 7** | PB2 | I2C SCL  |
| **Pin 8** | VCC | Power Supply | 

> **For Digistump:**  
> Desoldered the red LED for reliable encoder decoding.
>
### Wiring Diagram

```text

       =================================================
             ROTARY ENCODER MODULE WIRING DETAIL
       =================================================

          [ A ] (Signal A / CLK) ------> ATtiny85 PB3 (Pin 2)
          [ C ] (Common Ground)  ------> COMMON GROUND (GND)
          [ B ] (Signal B / DT)  ------> ATtiny85 PB1 (Pin 6)
                  
          [SW1] (Switch Pin 1)   ------> ATtiny85 PB4 (Pin 3)
          [SW2] (Switch Pin 2)   ------> COMMON GROUND (GND)
```
---

## 7-Byte Virtual Register Map (All 1-Byte!)

| Register | Name | Type | Range / Unit | Default | Description |
| :---: | :--- | :---: | :---: | :---: | :--- |
| **0x00** | `VAL` | `int8_t` | `-128` .. `+127` | `0` | Current encoder position count |
| **0x01** | `MIN` | `int8_t` | `-128` .. `+127` | `0` | Lower boundary limit |
| **0x02** | `MAX` | `int8_t` | `-128` .. `+127` | `100` | Upper boundary limit |
| **0x03** | `STEP`| `int8_t` | `1` .. `127` | `5` | Step increment/decrement per detent click |
| **0x04** | `DBL_CLICK`| `uint8_t`| `10ms units` (`0..255`) | `25` | Double-click max interval (`25` = 250 ms) |
| **0x05** | `LONG_PRESS`| `uint8_t`| `10ms units` (`0..255`) | `60` | Long-press threshold (`60` = 600 ms, up to 2.55s) |
| **0x06** | `BTN_EVT`| `uint8_t`| Enum (`0..3`) | `0` | **0** = Idle, **1** = Click, **2** = Long Press, **3** = Double Click |

> **Event Handshake**: When `BTN_EVT` (Register 6) is read as non-zero, the host master should write `0` back to Register 6 to acknowledge and clear the event.

---
## LICENSE
[The MIT License (MIT)](https://opensource.org/licenses/MIT) Copyright (c) 2026 Satish Kini

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
