# AcaiaArduinoBLE
Acaia / Felicita Scale Gateway using the ArduinoBLE library for devices such as the esp32, arduino nano esp32, and arduino nano iot 33.
This is an Arduino Library which can be found in the Arduino IDE Library Manager.

Tested on:
* Arduino Nano ESP32, Nano 33 IoT
* Acaia Pyxis v1.0.022, Acaia Lunar Pre-2021 v2.6.019, Acaia Lunar 2021 v1.0.016, Felicita Arc
* La Marzocco Linea Mini, Linea Micra, La Marzocco GS3, Rancilio Silvia Pro, Stone Espresso

...using Arduino IDE 2.3.2 and ArduinoBLE 1.3.6


## Requirements
This library is intended to be used with any arduino device which is compatible with the [ArduinoBLE](https://www.arduino.cc/reference/en/libraries/arduinoble/) library.

As of version V2.0.0, non-volatile storage for the setpoint and offset is only available for ESP32-based devices.

## Printed Circuit Board
The included "shotStopper" example code uses the ShotStopper PCB to make it simple to control your espresso machine using the scale. Files are hosted on [altium 365](https://365.altium.com/files/A15F83F1-2418-4843-B2E7-787275773560).

A kit can also be ordered by filling out this interest form: https://forms.gle/DqZSysbdBHmdgNSM7

[![Video showing developmnent of the shotStopper](https://img.youtube.com/vi/434hrQDGtxo/0.jpg)](https://youtu.be/434hrQDGtxo)

## ShotStopper Example Code Configuration

The following variables at the top of the shotStopper.ino file can be configured by the user:

`MOMENTARY`
* true for momentary switches such as GS3 AV, Rancilio Silvia Pro, Breville, etc.
* false for latching switches such as Linea Mini/Micra, etc.

`REEDSWITCH`
* true if a reed switch on the brew solenoid is being used to determine the brew state. This is typically not necessary so set to FALSE by default. This feature is only available for non-momentary-switches.

`AUTOTARE`
* true by default. The scale will automatically tare when the shot is started, and, if MOMENTARY is false, will perform another tare at 3 seconds to notify the user that the switch is latched and should be returned to the home position.
* if set to false, the shotStopper will never send a tare command. It is the user's responsibility to tare before each shot. This may be helpful if the scale is not stable when the shot begins, and thus the scale is unable to tare reliably.

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

☑ change setpoint over bluetooth

☑ maintain setpoint and offset after powercycle

☑ auto start/stop timer

☑ flowrate-based shot end-time

⬜ auto timer reset

⬜ Improve Tare Command Reliability



Scale Compatibility:

☑ Acaia Pyxis

☑ Acaia Lunar

☑ Acaia Lunar (Pre-2021)

☑ Felicita Arc


Hardware:

☑ PCB Design for Low Voltage Switches (V1.1)

☑ 3D-Printed Half Case

☑ Compatibility with La Marzocco GS3 AV

☑ Compatibility with Rancilio Silvia Pro (and Pro X)

❌ Compatibility with La Marzocco Linea Classic S (Not Compatible, requires investigation)

☑ Compatibility with Stone Espresso (requires reed switch)

☑ Compatibility with La Marzocco Mini

☑ Compatibility with La Marzocco Micra (V2.0)

☑ Powered by espresso machine (V2.0)

☑ Reed switch input (V2.0)

⬜ Investigate less expensive microcontroller

⬜ Compatibility with Breville (presumed but untested)

⬜ Support for High-Voltage Switches (Hall-Effect Sensor and SSR?)

Sales:

☑ Beta Users Determined

☑ Beta Units Built

☑ Beta Units Shipped (2/22/24)

☑ Beta Test Complete (5/7/24)

☑ Sales Open For GS3, Silvia, and Micra In the US (5/25/24)

⬜ International Sales Open 

⬜ Sales Open for Linea Mini

## Bugs/Missing
1. Tare command is less reliable than pressing the tare button.
2. Only supports grams.

# Acknowledgement
This is largely a basic port of the  [LunarGateway](https://github.com/frowin/LunarGateway/) library written for the ESP32.

In addition to some minor notes from [pyacaia](https://github.com/lucapinello/pyacaia) library written for raspberryPI.

Felicita Arc support contributions from baettigp
