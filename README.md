# AcaiaArduinoBLE
Acaia Scale Gateway using the ArduinoBLE library for devices such as the esp32, arduino nano esp32, and arduino nano iot 33.
This is an Arduino Library which can be found in the Arduino IDE Library Manager.

Tested on the Arduino Nano ESP32 and Acaia Pyxis, using Arduino IDE 2.2.1 and ArduinoBLE 1.3.6

## Requirements
This library is intended to be used with any arduino device which is compatible with the [ArduinoBLE](https://www.arduino.cc/reference/en/libraries/arduinoble/) library.

## Printed Circuit Board
The included "shotStopper" example code uses the ShotStopper PCB to make it simple to control your espresso machine using the scale. Files are hosted on [altium 365](https://365.altium.com/files/A15F83F1-2418-4843-B2E7-787275773560).

## Demo

You can find a demo on Youtube:

[![Video showing an shotStopper pulling a shot on a silvia pro](https://img.youtube.com/vi/oP3Cmke6daE/0.jpg)](https://www.youtube.com/shorts/oP3Cmke6daE)

## Bugs/Missing
1. Tare command is less reliable than pressing the tare button.
2. Only supports Grams and positive weight values.
3. Reconnection is unreliable. Power Cycle is best.
4. Yet to implement shotStopper example code for "latching" brew switches (ex: Linea Mini)

# Acknowledgement
This is largely a basic port of the [LunarGateway](https://github.com/frowin/LunarGateway/) library written for the ESP32.