/*
  AcaiaArduinoBLE.h - Library for connecting to 
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.

  Pio Baettig: Adding Felicita Arc support 
  A-TWJ: Fixing Felicita Arc bug (issue #15)
  
  Known Bugs:
    * Only supports Grams
*/
#ifndef AcaiaArduinoBLE_h
#define AcaiaArduinoBLE_h

#define LIBRARY_VERSION        "3.4.0"
#define WRITE_CHAR_OLD_VERSION "2a80"
#define READ_CHAR_OLD_VERSION  "2a80"
#define WRITE_CHAR_NEW_VERSION "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR_NEW_VERSION  "49535343-1e4d-4bd9-ba61-23c647249616"
#define WRITE_CHAR_GENERIC     "ff12"
#define READ_CHAR_GENERIC      "ff11"
#define WRITE_CHAR_FELICITA    "ffe1"   // Felicita Arc: single ffe1 char (service ffe0) for both
#define READ_CHAR_FELICITA     "ffe1"   //   notify (weight) and write (commands)
#define WRITE_CHAR_ECLAIR      "4F9A45BA-8E1B-4E07-E157-0814D393B968"
#define READ_CHAR_ECLAIR       "AD736C5F-BBC9-1F96-D304-CB5D5F41E160"
#define HEARTBEAT_PERIOD_MS     2750
#define MAX_PACKET_PERIOD_MS    5000

#include "Arduino.h"
#include <ArduinoBLE.h>

enum scale_type{
    OLD,     // Lunar (pre-2021)
    NEW,     // Lunar (2021), Pyxis
    GENERIC, // BooKoo Themis, Decent, etc (ff11/ff12, binary protocol)
    FELICITA, // Felicita Arc (ffe1, single-byte ASCII protocol)
    ECLAIR   // AtomHeart Eclair (custom service/characteristics)
};

class AcaiaArduinoBLE{
    public:
        AcaiaArduinoBLE(bool debug);
        bool init(String = "");
        bool tare();
        bool startTimer();
        bool stopTimer();
        bool resetTimer();
        bool tareStartTimer();
        bool beep();
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
