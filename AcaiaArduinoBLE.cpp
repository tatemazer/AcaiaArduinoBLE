/*
  AcaiaArduinoBLE.cpp - Library for connecting to
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.

  Adding Generic Scale Support, Pio Baettig
*/
#include "Arduino.h"
#include "AcaiaArduinoBLE.h"
#include <ArduinoBLE.h>

byte IDENTIFY[20]               = { 0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d };
byte HEARTBEAT[7]               = { 0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00 };
byte NOTIFICATION_REQUEST[14]   = { 0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06 };
byte START_TIMER[7]             = { 0xef, 0xdd, 0x0d, 0x00, 0x00, 0x00, 0x00 };
byte STOP_TIMER[7]              = { 0xef, 0xdd, 0x0d, 0x00, 0x02, 0x00, 0x02 };
byte RESET_TIMER[7]             = { 0xef, 0xdd, 0x0d, 0x00, 0x01, 0x00, 0x01 };
byte TARE_ACAIA[6]              = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00 };
byte TARE_GENERIC[6]            = { 0x03, 0x0a, 0x01, 0x00, 0x00, 0x08 };
byte START_TIMER_GENERIC[6]     = { 0x03, 0x0a, 0x04, 0x00, 0x00, 0x0a };
byte STOP_TIMER_GENERIC[6]      = { 0x03, 0x0a, 0x05, 0x00, 0x00, 0x0d };
byte RESET_TIMER_GENERIC[6]     = { 0x03, 0x0a, 0x06, 0x00, 0x00, 0x0c };

/* Generic commands from
   https://github.com/graphefruit/Beanconqueror/blob/master/src/classes/devices/felicita/constants.ts
*/

AcaiaArduinoBLE::AcaiaArduinoBLE(bool debug) {
    _debug = debug;
    _currentWeight = 0;
    _connected = false;
    _packetPeriod = 0;
    _connectionState = IDLE;
    _connectionStartTime = 0;
    _targetMac = "";
}

bool AcaiaArduinoBLE::init(String mac) {
    if (!BLE.begin()) {
        if (_debug) Serial.println("Failed to start BLE!");
        return false;
    }

    _targetMac = mac;
    _connectionStartTime = millis();
    _connectionState = SCANNING;
    _lastPacket = 0;
    _connected = false;

    if (mac == "") {
        BLE.scan();
    }
    else if (!BLE.scanForAddress(mac)) {
        Serial.print("Failed to find ");
        Serial.println(mac);
        _connectionState = FAILED;
        return false;
    }

    if (_debug) Serial.println("Starting connection process...");

    return true; // Returns immediately, connection continues in background
}

bool AcaiaArduinoBLE::updateConnection(){
    // If failed, automatically restart connection after delay
    if (_connectionState == FAILED) {
        if (millis() - _connectionStartTime > 2000) {
            if (_debug) Serial.println("Auto-reconnecting...");

            _connectionState = SCANNING;
            _connectionStartTime = millis();

            if (_targetMac == ""){
                BLE.scan();
            }
            else if (!BLE.scanForAddress(_targetMac)){
                if(_debug) {
                    Serial.print("Failed to start scan for ");
                    Serial.println(_targetMac);
                }

                return false;
            }
        }

        return false;
    }

    switch(_connectionState) {
        case SCANNING:
            {
                BLEDevice peripheral = BLE.available();

                if (_debug && peripheral) {
                    Serial.print("Found ");
                    Serial.print(peripheral.address());
                    Serial.print(" '");
                    Serial.print(peripheral.localName());
                    Serial.print("' ");
                    Serial.print(peripheral.advertisedServiceUuid());
                    Serial.println();
                }

                if (peripheral && isScaleName(peripheral.localName())) {
                    BLE.stopScan();
                    _foundPeripheral = peripheral;
                    _connectionState = CONNECTING;

                    if (_debug) Serial.println("Scale found, connecting...");
                }
            }
            break;

        case CONNECTING:
            if (_debug) Serial.println("Connecting ...");

            if (_foundPeripheral.connect()) {
                if (_debug) Serial.println("Connected");
                _connectionState = DISCOVERING;
            }
            else {
                if (_debug) Serial.println("Failed to connect!");
                _connectionState = FAILED;
            }
            break;

        case DISCOVERING:
            if (_debug) Serial.println("Discovering attributes ...");

            if (_foundPeripheral.discoverAttributes()) {
                if (_debug) Serial.println("Attributes discovered");
                _connectionState = CONFIGURING;
            }
            else {
                if (_debug) Serial.println("Attribute discovery failed!");
                _foundPeripheral.disconnect();
                _connectionState = FAILED;
            }
            break;

        case CONFIGURING:
            if (_debug) {
                Serial.println();
                Serial.print("Device name: ");
                Serial.println(_foundPeripheral.deviceName());
                Serial.print("Appearance: 0x");
                Serial.println(_foundPeripheral.appearance(), HEX);
                Serial.println();

                for (int i = 0; i < _foundPeripheral.serviceCount(); i++) {
                    BLEService service = _foundPeripheral.service(i);
                    exploreService(service);
                }
            }

            // Determine type of scale
            if (_foundPeripheral.characteristic(READ_CHAR_OLD_VERSION).canSubscribe()) {
                if (_debug) Serial.println("Old version Acaia Detected");
                _type   = OLD;
                _write  = _foundPeripheral.characteristic(WRITE_CHAR_OLD_VERSION);
                _read   = _foundPeripheral.characteristic(READ_CHAR_OLD_VERSION);
            }
            else if (_foundPeripheral.characteristic(READ_CHAR_NEW_VERSION).canSubscribe()) {
                if (_debug) Serial.println("New version Acaia Detected");
                _type   = NEW;
                _write  = _foundPeripheral.characteristic(WRITE_CHAR_NEW_VERSION);
                _read   = _foundPeripheral.characteristic(READ_CHAR_NEW_VERSION);
            }
            else if (_foundPeripheral.characteristic(READ_CHAR_GENERIC).canSubscribe()) {
                if (_debug) Serial.println("Generic Scale Detected");
                _type   = GENERIC;
                _write  = _foundPeripheral.characteristic(WRITE_CHAR_GENERIC);
                _read   = _foundPeripheral.characteristic(READ_CHAR_GENERIC);
            }
            else {
                if (_debug) Serial.println("Unable to determine scale type");
                _connectionState = FAILED;
                return false;
            }

            if (!_read.canSubscribe()) {
                if (_debug) Serial.println("Unable to subscribe to read");
                _connectionState = FAILED;
                return false;
            }
            else if (!_read.subscribe()) {
                if (_debug) Serial.println("Subscription failed");
                _connectionState = FAILED;
                return false;
            }
            else {
                if (_debug) Serial.println("Subscribed!");
            }

            if (_write.writeValue(IDENTIFY, 20)) {
               if (_debug) Serial.println("Identify write successful");
            }
            else {
                if (_debug) Serial.println("identify write failed");
                _connectionState = FAILED;
                return false;
            }

            if (_write.writeValue(NOTIFICATION_REQUEST,14)) {
                if (_debug) Serial.println("Notification request write successful");
            }
            else {
                if (_debug) Serial.println("Notification request write failed");
                _connectionState = FAILED;
                return false;
            }

            _connected = true;
            _packetPeriod = 0;
            _lastPacket = millis();
            _connectionState = CONNECTED;
            if (_debug) Serial.println("Scale connection completed successfully!");
            return true;

        case CONNECTED:
            // Check if the peripheral is still connected
            if (!_foundPeripheral.connected()) {
                if (_debug) Serial.println("BLE peripheral disconnected");
                _connectionState = FAILED;
                _connected = false;
                _connectionStartTime = millis();
                break;
            }

            // Check for data timeout - only if we've been connected for a while
            if (_lastPacket > 0 && millis() - _lastPacket > MAX_PACKET_PERIOD_MS) {
                if (_debug) Serial.println("Scale data timeout");
                _connectionState = FAILED;
                _connected = false;
                _connectionStartTime = millis();
                break;
            }

            // If we reach here, connection is healthy
            break;

        case FAILED:
            // Add a delay before reconnecting
            if (millis() - _connectionStartTime > 3000) {
                if (_debug) Serial.println("Attempting reconnection...");

                // Clean up any existing connection state
                if (_foundPeripheral) {
                    _foundPeripheral.disconnect();
                }

                // Stop any ongoing scan
                BLE.stopScan();

                // Reset BLE stack (this might be the missing piece!)
                BLE.end();
                delay(100);  // Brief delay
                BLE.begin();

                // Reset connection state and start fresh
                _connectionState = SCANNING;
                _connectionStartTime = millis();
                _connected = false;

                // Clear previous peripheral
                _foundPeripheral = BLEDevice();
            }
            break;

        case IDLE:

        default:
            break;
    }

    return _connectionState == CONNECTED;
}

bool AcaiaArduinoBLE::isConnecting() {
    return _connectionState == SCANNING || _connectionState == CONNECTING ||
           _connectionState == DISCOVERING || _connectionState == CONFIGURING;
}

bool AcaiaArduinoBLE::tare() {
    if (_write.writeValue((_type == GENERIC ? TARE_GENERIC : TARE_ACAIA), 6)) {
          if (_debug) Serial.println("Tare write successful");
          return true;
    }
    else{
        _connected = false;
        if (_debug) Serial.println("Tare write failed");
        return false;
    }
}

bool AcaiaArduinoBLE::startTimer() {
    if (_write.writeValue((_type == GENERIC ? START_TIMER_GENERIC : START_TIMER),
                         (_type == GENERIC ? 6                   : 7))) {
        if (_debug) Serial.println("Start timer write successful");
        return true;
    }
    else {
        _connected = false;
        if (_debug) Serial.println("Start timer write failed");
        return false;
    }
}

bool AcaiaArduinoBLE::stopTimer() {
    if (_write.writeValue((_type == GENERIC ? STOP_TIMER_GENERIC : STOP_TIMER),
                         (_type == GENERIC ? 6                  : 7 ))) {
        if (_debug) Serial.println("stop timer write successful");
        return true;
    }
    else {
        _connected = false;
        if (_debug) Serial.println("stop timer write failed");
        return false;
    }
}

bool AcaiaArduinoBLE::resetTimer() {
    if (_write.writeValue((_type == GENERIC ? RESET_TIMER_GENERIC : RESET_TIMER),
                         (_type == GENERIC ? 6                  : 7 ))) {
        if (_debug) Serial.println("Reset timer write successful");
        return true;
    }
    else {
        _connected = false;
        if (_debug) Serial.println("Reset timer write failed");
        return false;
    }
}

bool AcaiaArduinoBLE::heartbeat() {
    if (_write.writeValue(HEARTBEAT, 7)) {
        _lastHeartBeat = millis();
        return true;
    }
    else {
        _connected = false;
        return false;
    }
}

float AcaiaArduinoBLE::getWeight() {
    return _currentWeight;
}

bool AcaiaArduinoBLE::heartbeatRequired() {
    if (_type == OLD || _type == NEW) {
        return (millis() - _lastHeartBeat) > HEARTBEAT_PERIOD_MS;
    }
    else{
        return 0;
    }
}

bool AcaiaArduinoBLE::isConnected() {
    return _connected;
}

bool AcaiaArduinoBLE::newWeightAvailable() {
    bool newWeightPacket = false;

    //check how long its been since we last got a response
    if (_lastPacket && millis()-_lastPacket > MAX_PACKET_PERIOD_MS) {
        if (_debug) Serial.println("Scale disconnected - timeout!");
        //reset connection
        _connected = false;
        _connectionState = FAILED;
        BLE.disconnect();
        return false;
    }
    else if (_read.valueUpdated()) {
        byte input[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
        int l = _read.valueLength();

        // Get packet
        if (10 >= l ||                      //10 byte packets for pre-2021 lunar
          (13 == l && OLD == _type) ||      //13 byte packets for original pearl AP001
          (13 >= l && OLD != _type) ||      //13 byte packets for pyxis and older lunar 2021 fw
          (14 == l && OLD == _type) ||      //14 byte packets for lunar 2021 AL008
          (17 == l && NEW == _type) ||      //17 byte packets for newer lunar 2021 fw
          (20 == l && GENERIC == _type)     //18 byte packets for generic scales
        ) {
            _read.readValue(input, (l > 13) ? 13 : l); // readValue() seems to crash whenever l > weight packet (10, 13 or 18)

            if (_debug) {
                Serial.print(l);
                Serial.print(": 0x");

                printData(input, l);
                Serial.println();
            }
        }

        // Parse New style data packet
        if (((NEW == _type && (13 == l || 17 == l)) || (OLD == _type && 13 == l)) && input[4] == 0x05) {
            //Grab weight bytes (5 and 6)
            // apply scaling based on the unit byte (9)
            // get sign byte (10)
            _currentWeight = (((input[6] & 0xff) << 8) + (input[5] & 0xff))
                            / pow(10,input[9])
                            * ((input[10] & 0x02) ? -1 : 1);
            newWeightPacket = true;

        // Parse old style data packet
        }
        else if ( OLD == _type && (l == 10 || l == 14)) {
            //Grab weight bytes (2 and 3),
            // apply scaling based on the unit byte (6)
            // get sign byte (7)
            _currentWeight = (((input[3] & 0xff) << 8) + (input[2] & 0xff))
                            / pow(10, input[6])
                            * ((input[7] & 0x02) ? -1 : 1);
            newWeightPacket = true;

        }else if ( GENERIC == _type && l == 20) {
            //Grab weight bytes (3-8),
            // get sign byte (2)
            _currentWeight = (( input[7] << 16) | (input[8] << 8) | input[9]);

            if (input[6] == 45) { // Check if the value is negative
                _currentWeight = -_currentWeight;
            }
            _currentWeight = _currentWeight / 100;
            newWeightPacket = true;
        }
        if (newWeightPacket) {
            if (_lastPacket) {
                _packetPeriod = millis() - _lastPacket;
            }
            _lastPacket = millis();
        }
        return newWeightPacket;
    }
    else{
        return false;
    }
}

bool AcaiaArduinoBLE::isScaleName(String name) {
    String nameShort = name.substring(0,5);

    return nameShort == "CINCO"
        || nameShort == "ACAIA"
        || nameShort == "PYXIS"
        || nameShort == "LUNAR"
        || nameShort == "PEARL"
        || nameShort == "PROCH"
        || nameShort == "BOOKO";
}

void AcaiaArduinoBLE::exploreService(BLEService service) {
  // print the UUID of the service
  if (_debug) Serial.print("Service ");
  Serial.println(service.uuid());

  // loop the characteristics of the service and explore each
  for (int i = 0; i < service.characteristicCount(); i++) {
    BLECharacteristic characteristic = service.characteristic(i);

    exploreCharacteristic(characteristic);
  }
}

void AcaiaArduinoBLE::exploreCharacteristic(BLECharacteristic characteristic) {
  // print the UUID and properties of the characteristic
    if (_debug) {
        Serial.print("\tCharacteristic ");
        Serial.print(characteristic.uuid());
        Serial.print(", properties 0x");
        Serial.print(characteristic.properties(), HEX);
    }

  // check if the characteristic is readable
  if (characteristic.canRead()) {
    // read the characteristic value
    characteristic.read();

    if (characteristic.valueLength() > 0) {
      // print out the value of the characteristic
      if (_debug) Serial.print(", value 0x");
      printData(characteristic.value(), characteristic.valueLength());
    }
  }
  Serial.println();

  // loop the descriptors of the characteristic and explore each
  for (int i = 0; i < characteristic.descriptorCount(); i++) {
    BLEDescriptor descriptor = characteristic.descriptor(i);
    exploreDescriptor(descriptor);
  }
}

void AcaiaArduinoBLE::exploreDescriptor(BLEDescriptor descriptor) {
  // print the UUID of the descriptor
  if (_debug) Serial.print("\t\tDescriptor ");
  Serial.print(descriptor.uuid());

  // read the descriptor value
  descriptor.read();

  // print out the value of the descriptor
  if (_debug) Serial.print(", value 0x");
  printData(descriptor.value(), descriptor.valueLength());

  Serial.println();
}

void AcaiaArduinoBLE::printData(const unsigned char data[], int length) {
  for (int i = 0; i < length; i++) {
    unsigned char b = data[i];

    if (b < 16) {
      if (_debug) Serial.print("0");
    }

    Serial.print(b, HEX);
  }
}
