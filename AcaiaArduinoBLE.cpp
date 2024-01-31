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

#define FELICITA_ARC
//#define ACAIA


byte IDENTIFY[20]             = { 0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d};
byte HEARTBEAT[7]             = { 0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00 };
byte NOTIFICATION_REQUEST[14] = { 0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06 };
#ifdef ACAIA
byte TARE[6]                  = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00};
#endif
#ifdef FELICITA_ARC
byte TARE[1]= {0x54};
#endif

AcaiaArduinoBLE::AcaiaArduinoBLE(){
    _currentWeight = 0;
    _connected = false;
}

bool AcaiaArduinoBLE::init(String mac){
  Serial.println("Starting init");

#ifdef ACAIA
  Serial.println("Searching for ACAIA");
#endif
#ifdef FELICITA_ARC
  Serial.println("Searching for FELICITA Arc");
#endif
		 
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
#ifdef ACAIA
            if(peripheral.characteristic(READ_CHAR_OLD_VERSION).canSubscribe()){
                Serial.println("Old version Acaia Detected");
                _type = OLD;
                
            }else if(peripheral.characteristic(READ_CHAR_NEW_VERSION).canSubscribe()){
                Serial.println("New version Acaia Detected");
                _type = NEW;
            }
            _write = peripheral.characteristic(_type == OLD ? WRITE_CHAR_OLD_VERSION : WRITE_CHAR_NEW_VERSION);
            _read = peripheral.characteristic( _type == OLD ? READ_CHAR_OLD_VERSION  : READ_CHAR_NEW_VERSION );
#endif
#ifdef FELICITA_ARC
	    if(peripheral.characteristic(READ_CHAR_FELICITA).canSubscribe()){
                Serial.println("Felicita Arc Detected");
                _type = FELICITA;
            }
            else{
                Serial.println("unable to subscribe to READ: Felicita");
                return false;
            }
	    _write = peripheral.characteristic(WRITE_CHAR_FELICITA);
	    _read = peripheral.characteristic(READ_CHAR_FELICITA);
	    Serial.println("Peripheral Characteristics Set Felicita");
#endif
	    
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
#ifdef ACAIA
    Serial.println("failed to find Acaia Device");
#endif
#ifdef FELICITA_ARC
    Serial.println("failed to find Felicita Device");
#endif
    return false;    
}

bool AcaiaArduinoBLE::tare(){
    if(_write.writeValue(TARE, 20)){
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
    return (millis() - _lastHeartBeat) > HEARTBEAT_PERIOD_MS;
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
        _currentWeight = (((input[6] & 0xff) << 8) + (input[5] & 0xff))/100.0;
        return true;
    }else if(OLD == _type 
      && _read.valueUpdated() 
      && _read.valueLength() == 10
      && _read.readValue(input,10) 
      ){
        _currentWeight = (((input[3] & 0xff) << 8) + (input[2] & 0xff))/100.0;
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
