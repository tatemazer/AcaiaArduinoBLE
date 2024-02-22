# AcaiaArduinoBLE
Acaia / Felicita Scale Gateway using the ArduinoBLE library for devices such as the esp32, arduino nano esp32, and arduino nano iot 33.
This is an Arduino Library which can be found in the Arduino IDE Library Manager.

Tested on:
* Arduino Nano ESP32, Nano 33 IoT
* Acaia Pyxis, Acaia Lunar Pre-2021, Acaia Lunar 2021, Felicita Arc
* La Marzocco Linea Mini, La Marzocco GS3, Rancilio Silvia Pro

...using Arduino IDE 2.3.1 and ArduinoBLE 1.3.6


## Requirements
This library is intended to be used with any arduino device which is compatible with the [ArduinoBLE](https://www.arduino.cc/reference/en/libraries/arduinoble/) library.

## Printed Circuit Board
The included "shotStopper" example code uses the ShotStopper PCB to make it simple to control your espresso machine using the scale. Files are hosted on [altium 365](https://365.altium.com/files/A15F83F1-2418-4843-B2E7-787275773560).

[![Video showing developmnent of the shotStopper](https://img.youtube.com/vi/434hrQDGtxo/0.jpg)](https://youtu.be/434hrQDGtxo)

## ShotStopper Example Code Configuration

END_WEIGHT
* Goal weight of espresso

MOMENTARY
* true for momentary switches such as GS3 AV, Rancilio Silvia Pro, Breville, etc.
* false for latching switches such as Linea Mini/Micra, etc.

weightOffset:
* Changes the default value at which the machine will stop the shot ( END_WEIGHT + weightOffset )
* Will change as the machine pulls repeated shots to increase precision
* Reset upon power cycle
* A value of -1.5 is usually within +/- 1 gram for a standard style espresso.
* By the second shot, precision is often within 0.1g


## Demo

You can find a demo on Youtube:

[![Video showing an shotStopper pulling a shot on a silvia pro](https://img.youtube.com/vi/oP3Cmke6daE/0.jpg)](https://www.youtube.com/shorts/oP3Cmke6daE)

## Project Status

Firmware:

☑ Connect Acaia Pyxis to ESP32

☑ Tare Command

☑ Receive Weight Data

☑ shotStopper Espresso Machine Brew-By-Weight Firmware

☑ Compatibility with Lunar (Pre-2021)

☑ Compatibility with Lunar 2021

☑ Positive *and* negative weight support

☑ Latching-switch support (LM Mini, LM Micra, etc)

☑ Auto-reconnect

⬜ Improve Tare Command Reliability

⬜ Investigate method to change setpoint (bluetooth?)

Scale Compatibility:

☑ Acaia Pyxis

☑ Acaia Lunar

☑ Acaia Lunar (Pre-2021)

☑ Felicita Arc


Hardware:

☑ PCB Design for Low Voltage Switches (V1.1)

☑ Compatibility with La Marzocco GS3 AV

☑ Compatibility with Rancilio Silvia Pro (and Pro X)

❌ Compatibility with La Marzocco Linea Classic S (Not Compatible, requires investigation)

❌ Compatibility with Stone Espresso (Not Compatible, not enough current across optocoupler)

☑ 3D-Printed Half Case

☑ Compatibility with La Marzocco Mini/Micra (in progress)

⬜ Compatibility with Breville (presumed but untested)

⬜ Support for High-Voltage Switches (Hall-Effect Sensor and SSR?)

Sales:

☑ Beta Users Determined

☑ Beta Units Built

⬜ Beta Units Shipped

⬜ Beta Test Complete

⬜ Sales Open

## Bugs/Missing
1. Tare command is less reliable than pressing the tare button.
2. Only supports grams.

# Acknowledgement
This is largely a basic port of the  [LunarGateway](https://github.com/frowin/LunarGateway/) library written for the ESP32.

In addition to some minor notes from [pyacaia](https://github.com/lucapinello/pyacaia) library written for raspberryPI.

Felicita Arc support contributions from baettigp
