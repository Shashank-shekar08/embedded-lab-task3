# Embedded Lab Task 3 - MQTT + JSON Communication

Bidirectional MQTT communication between a Raspberry Pi Pico W (Wokwi simulation) 
and an Arduino Nano 33 IoT (real hardware) using JSON messages over the internet.

## Wokwi Simulation
[Pico W Simulation](https://wokwi.com/projects/463390459928401921)

## Hardware
- **Pico W** (Wokwi) — LED on GP28, Button on GP2, Potentiometer on GP26
- **Arduino Nano 33 IoT** — Serial command gateway, mirrors Pico LED state

## MQTT Broker
`broker.hivemq.com:1883`

| Topic | Publisher | Content |
|---|---|---|
| iem/task3/pico/data | Pico W | Sensor data (pot, LED state, uptime) |
| iem/task3/pico/cmd | Nano IoT | Control commands |

## Commands
| Command | Description |
|---|---|
| `ON` / `OFF` | Turn LED on or off |
| `BLINK` / `NOBLINK` | Enable or disable blinking |
| `INTERVAL <ms>` | Set blink speed (50–2000ms) |
| `POT` | Hand control back to potentiometer |
| `STATUS` | Show current Pico state |
| `STATS` | Show message statistics |

## Robustness
- Token validation, source checking, sequence numbers, watchdog/safe state, range checks

## Course
IEM – Hochschule Ravensburg-Weingarten | Embedded Lab & Project
