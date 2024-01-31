/*
  AcaiaArduinoBLE.h - Library for connecting to 
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.

  Starting from V 1.2, WIP: 
  Pio Baettig: Adding Felicita Arc support 

  Known Bugs:
    * Only supports Grams and positive weight values
    * Reconnection is unreliable. Power Cycle is best.
*/
#ifndef AcaiaArduinoBLE_h
#define AcaiaArduinoBLE_h

#define FELICITA_ARC
//#define ACAIA

#ifdef ACAIA
#define WRITE_CHAR_OLD_VERSION "2a80"
#define READ_CHAR_OLD_VERSION  "2a80"
#define WRITE_CHAR_NEW_VERSION "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR_NEW_VERSION  "49535343-1e4d-4bd9-ba61-23c647249616"
#endif
#ifdef FELICITA_ARC
#define WRITE_CHAR_FELICITA "ffe0"
#define READ_CHAR_FELICITA  "ffe0"
//#define READ_CHAR_FELICITA "00002a05-0000-1000-8000-00805f9b34fb"
//#define READ_CHAR_FELICITA "0000ffe1-0000-1000-8000-00805f9b34fb"
 //#define WRITE_CHAR_FELICITA  "0000ffe0-0000-1000-8000-00805f9b34fb"
#endif
#define HEARTBEAT_PERIOD_MS 2750


#include "Arduino.h"
#include <ArduinoBLE.h>

enum scale_type{
    OLD, // Lunar (pre-2021)
    NEW,  // Lunar (2021), Pyxis
    FELICITA //Felicita Arc
};

class AcaiaArduinoBLE{
    public:
        AcaiaArduinoBLE();
        bool init(String = "");
        bool tare();
        bool heartbeat();
        float getWeight();
        bool heartbeatRequired();
        bool isConnected();
        bool newWeightAvailable();
    private:
        bool isAcaiaName(String);
        float               _currentWeight;
        BLECharacteristic   _write;
        BLECharacteristic   _read;
        long                _lastHeartBeat;
        bool                _connected;
        scale_type          _type;


};

#endif
