/*
  AcaiaArduinoBLE.cpp - Library for connecting to 
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.
#Starting with V 1.20 WIP:
Adding Felicita Arc Support, Pio Baettig
  
*/
#include "Arduino.h"
#include "AcaiaArduinoBLE.h"
#include <ArduinoBLE.h>

//#define FELICITA_ARC
#define ACAIA


byte IDENTIFY[20]             = { 0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d};
byte HEARTBEAT[7]             = { 0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00 };
byte NOTIFICATION_REQUEST[14] = { 0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06 };

byte TARE_ACAIA[6]                  = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00};
byte TARE_FELICITA[1]= {0x54};

AcaiaArduinoBLE::AcaiaArduinoBLE(){
    _currentWeight = 0;
    _connected = false;
}

bool AcaiaArduinoBLE::init(String mac){
    if (!BLE.begin()) {
        Serial.println("Failed to enable BLE!");
        return false;
    }

    if (mac == ""){
        BLE.scan();
    }else if (!BLE.scanForAddress(mac)){
        Serial.print("Failed to find ");
        Serial.println(mac);
        return false;
    }
    
    do{
        BLEDevice peripheral = BLE.available();
        if (peripheral && isAcaiaName(peripheral.localName())) {
            BLE.stopScan();

            Serial.println("Connecting ...");
            if (peripheral.connect()) {
                Serial.println("Connected");
            } else {
                Serial.println("Failed to connect!");
                return false;
            }

            Serial.println("Discovering attributes ...");
            if (peripheral.discoverAttributes()) {
                Serial.println("Attributes discovered");
		Serial.println(peripheral.discoverAttributes());
		Serial.println(peripheral.advertisedServiceUuid());
            } else {
                Serial.println("Attribute discovery failed!");
                peripheral.disconnect();
                return false;
            }

            if(peripheral.characteristic(READ_CHAR_OLD_VERSION).canSubscribe()){
                Serial.println("Old version Acaia Detected");
                _type = OLD;
                _write =peripheral.characteristic(WRITE_CHAR_OLD_VERSION);
                _read =peripheral.characteristic(READ_CHAR_OLD_VERSION);

            }else if(peripheral.characteristic(READ_CHAR_NEW_VERSION).canSubscribe()){
                Serial.println("New version Acaia Detected");
                _type = NEW;
                _write =peripheral.characteristic(WRITE_CHAR_NEW_VERSION);
                _read =peripheral.characteristic(READ_CHAR_NEW_VERSION);

            } else if(peripheral.characteristic(READ_CHAR_FELICITA).canSubscribe()){
                Serial.println("Felicita Arc Detected");
                _type = FELICITA;
	    _write = peripheral.characteristic(WRITE_CHAR_FELICITA);
	    _read = peripheral.characteristic(READ_CHAR_FELICITA);

            }
            else{
                Serial.println("unable to subscribe to READ");
                return false;
            }

//            _write = peripheral.characteristic(_type == OLD ? WRITE_CHAR_OLD_VERSION : WRITE_CHAR_NEW_VERSION);
//            _read = peripheral.characteristic( _type == OLD ? READ_CHAR_OLD_VERSION  : READ_CHAR_NEW_VERSION );

            if(!_read.canSubscribe()){
                Serial.println("unable to subscribe to READ");
                return false;
            }else if(!_read.subscribe()){
                Serial.println("subscription failed");
                return false;
            }else {
                Serial.println("subscribed!");
            }
        
            if(_write.writeValue(IDENTIFY, 20)){
                Serial.println("identify write successful");
            }else{
                Serial.println("identify write failed");
                return false; 
            }
            if(_write.writeValue(NOTIFICATION_REQUEST,14)){
                Serial.println("notification request write successful");
            }else{
                Serial.println("notification request write failed");
                return false; 
            }
            _connected = true;
            return true;
        }
    }while(millis() < 10000);

    Serial.println("failed to find Acaia or Felicita Device");
    return false;    
}

bool AcaiaArduinoBLE::tare(){
    if(_write.writeValue(( _type == OLD || NEW ? TARE_ACAIA  : TARE_FELICITA), 20)){
          Serial.println("tare write successful");
          return true;
    }else{
        _connected = false;
        Serial.println("tare write failed");
        return false;
    }
}

bool AcaiaArduinoBLE::heartbeat(){
    if(_write.writeValue(HEARTBEAT, 7)){
        _lastHeartBeat = millis();
        return true;
    }else{
        _connected = false;
        return false;
    }
}
float AcaiaArduinoBLE::getWeight(){
    return _currentWeight;
}

bool AcaiaArduinoBLE::heartbeatRequired(){
    if(_type == OLD || NEW){
        return (millis() - _lastHeartBeat) > HEARTBEAT_PERIOD_MS;
    } else {
    return 0;
    }
}
bool AcaiaArduinoBLE::isConnected(){
    return _connected;
}
bool AcaiaArduinoBLE::newWeightAvailable(){
    byte input[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
    if(NEW == _type 
      && _read.valueUpdated() 
      && _read.valueLength() == 13 
      && _read.readValue(input,13) 
      && input[4] == 0x05
      ){
        //Grab weight bytes (5 and 6) 
        // apply scaling based on the unit byte (9)
        // get sign byte (10)
        _currentWeight = (((input[6] & 0xff) << 8) + (input[5] & 0xff)) 
                        / pow(10,input[9])
                        * ((input[10] & 0x02) ? -1 : 1);
        return true;
    }else if(OLD == _type 
      && _read.valueUpdated() 
      && _read.valueLength() == 10
      && _read.readValue(input,10) 
      ){
        //Grab weight bytes (2 and 3),
        // apply scaling based on the unit byte (6)
        // get sign byte (7)
        _currentWeight = (((input[3] & 0xff) << 8) + (input[2] & 0xff)) 
                        / pow(10, input[6]) 
                        * ((input[7] & 0x02) ? -1 : 1);
        return true;
    }else if(FELICITA == _type 
      && _read.valueUpdated() 
     // && _read.valueLength() == 13 
      && _read.readValue(input,13) 
      ){
	_currentWeight = ( input[2] == 0x2B ? 1  : -1 )*((input[3] -0x30)*1000 + (input[4] -0x30)*100 + (input[5] -0x30)*10 + (input[6] -0x30)*1 + (input[7] -0x30)*0.1 + (input[8] -0x30)*0.01);
        return true;
    }else{
        return false;
    }
}
#ifdef ACAIA
bool AcaiaArduinoBLE::isAcaiaName(String name){
    String nameShort = name.substring(0,5);

    return nameShort == "CINCO"
        || nameShort == "ACAIA"
        || nameShort == "PYXIS"
        || nameShort == "LUNAR"
        || nameShort == "PROCH";
}
#endif
#ifdef FELICITA_ARC
bool AcaiaArduinoBLE::isAcaiaName(String name){
    String nameShort = name.substring(0,8);
    Serial.println("FELICITA");
    return nameShort == "FELICITA";
}
#endif
