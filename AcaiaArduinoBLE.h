/*
  AcaiaArduinoBLE.h - Library for connecting to 
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.

  Pio Baettig: Adding Felicita Arc support 

  Known Bugs:
    * Only supports Grams
*/
#ifndef AcaiaArduinoBLE_h
#define AcaiaArduinoBLE_h

#define WRITE_CHAR_OLD_VERSION "2a80"
#define READ_CHAR_OLD_VERSION  "2a80"
#define WRITE_CHAR_NEW_VERSION "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR_NEW_VERSION  "49535343-1e4d-4bd9-ba61-23c647249616"
#define WRITE_CHAR_GENERIC     "ff12"
#define READ_CHAR_GENERIC      "ff11"
#define HEARTBEAT_PERIOD_MS     2750
#ifndef MAX_PACKET_PERIOD_GENERIC_MS
    #define MAX_PACKET_PERIOD_GENERIC_MS 500 
#endif 
#define MAX_PACKET_PERIOD_ACAIA_MS 2000

#include "Arduino.h"
#include <ArduinoBLE.h>

enum scale_type{
    OLD,    // Lunar (pre-2021)
    NEW,    // Lunar (2021), Pyxis
    GENERIC // Felicita Arc, etc
};

class AcaiaArduinoBLE{
    public:
        AcaiaArduinoBLE(bool debug);
        bool init(String = "");
        bool tare();
        bool startTimer();
        bool stopTimer();
        bool resetTimer();
        bool heartbeat();
        float getWeight();
        bool heartbeatRequired();
        bool isConnected();
        bool newWeightAvailable();
    private:
        bool isScaleName(String);

        //debug functions
        void exploreService(BLEService service);
        void exploreCharacteristic(BLECharacteristic characteristic);
        void exploreDescriptor(BLEDescriptor descriptor);
        void printData(const unsigned char data[], int length);

        float               _currentWeight;
        BLECharacteristic   _write;
        BLECharacteristic   _read;
        long                _lastHeartBeat;
        bool                _connected;
        scale_type          _type;
        bool                _debug; 
        long                _packetPeriod;
        long                _lastPacket;
};

#endif
