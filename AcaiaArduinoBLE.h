/*
  AcaiaArduinoBLE.h - Library for connecting to 
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.

  Known Bugs:
    * Only supports Grams and positive weight values
    * Reconnection is unreliable. Power Cycle is best.
*/
#ifndef AcaiaArduinoBLE_h
#define AcaiaArduinoBLE_h

#define WRITE_CHAR "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR  "49535343-1e4d-4bd9-ba61-23c647249616"
#define HEARTBEAT_PERIOD_MS 2750

#include "Arduino.h"
#include <ArduinoBLE.h>

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


};

#endif