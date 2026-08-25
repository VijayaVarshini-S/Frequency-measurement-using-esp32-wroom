# Industrial 4-Channel Frequency Monitoring System

An ESP32-WROVER based real-time frequency monitoring system for four independent low-frequency digital pulse signals. The system uses the ESP32 hardware **Pulse Counter (PCNT)** peripheral for efficient pulse counting with low CPU overhead.

## Features

- 4 independent frequency input channels
- ESP32 PCNT hardware pulse counting
- Target frequency range: **8.0000–10.0000 Hz**
- 1-second measurement gate in the prototype
- Four-decimal frequency calculation/API output
- Real-time ESP32 web dashboard
- JSON API
- NTP time synchronization
- CSV logging
- Browser CSV download
- PCNT overflow handling
- Hardware pulse glitch filtering
- Serial Monitor diagnostics
- Responsive dark industrial dashboard

## Hardware

| Channel | GPIO | Function |
|---|---:|---|
| CH1 | GPIO32 | Frequency input |
| CH2 | GPIO33 | Frequency input |
| CH3 | GPIO34 | Frequency input |
| CH4 | GPIO35 | Frequency input |

### Important electrical requirement

ESP32 GPIOs are **3.3 V logic inputs**.

Do not connect 5 V, 12 V, 24 V, or other higher-voltage signals directly to the GPIO pins.

Use appropriate signal conditioning, level shifting, a comparator, resistor divider, or galvanic isolation depending on the source.

## Software

Current prototype uses:

```text
Arduino framework
WiFi.h
WebServer.h
driver/pcnt.h
time.h
