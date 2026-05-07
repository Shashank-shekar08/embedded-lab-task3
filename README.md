# Embedded Lab Task 3 - MQTT + JSON

## Description
Bidirectional MQTT communication between Raspberry Pi Pico W (Wokwi) and Arduino Nano 33 IoT.

## Setup
- Pico W simulated in Wokwi: [https://wokwi.com/projects/463390459928401921]
- Arduino Nano 33 IoT: real hardware

## MQTT Broker
broker.hivemq.com:1883

## Topics
- iem/task3/pico/data - Pico publishes sensor data
- iem/task3/pico/cmd - Nano publishes commands

## Commands
ON, OFF, BLINK, NOBLINK, INTERVAL <ms>, POT, STATUS, STATS, HELP
