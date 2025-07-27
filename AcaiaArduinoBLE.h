/*
  AcaiaArduinoBLE.h - Library for connecting to 
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.

  Pio Baettig: Adding Felicita Arc support

  Known Bugs:
    * Only supports Grams
*/
#pragma once

#define LIBRARY_VERSION        "3.2.0"
#define WRITE_CHAR_OLD_VERSION "2a80"
#define READ_CHAR_OLD_VERSION  "2a80"
#define WRITE_CHAR_NEW_VERSION "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR_NEW_VERSION  "49535343-1e4d-4bd9-ba61-23c647249616"
#define WRITE_CHAR_DECENT      "000036F5-0000-1000-8000-00805F9B34FB"
#define READ_CHAR_DECENT       "0000FFF4-0000-1000-8000-00805F9B34FB"
#define SUUID_DECENTSCALE      "0000FFF0-0000-1000-8000-00805F9B34FB"
#define SUUID_GENERIC          "ff10"
#define WRITE_CHAR_GENERIC     "ff12"
#define READ_CHAR_GENERIC      "ff11"
#define HEARTBEAT_PERIOD_MS    2750
#define MAX_PACKET_PERIOD_MS   5000

#include "Arduino.h"
#include <NimBLEAdvertisedDevice.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <NimBLEUtils.h>

#include <utility>

enum scale_type {
    OLD,     // Lunar (pre-2021)
    NEW,     // Lunar (2021), Pyxis
    GENERIC, // Felicita Arc, etc
    DECENT   // Decent Scale + EspressiScale
};

enum ConnectionState {
    IDLE,
    SCANNING,
    CONNECTING,
    DISCOVERING,
    CONFIGURING,
    CONNECTED,
    FAILED
};

class MyAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
    public:
        void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
        void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;

        [[nodiscard]] bool hasFoundDevice() const {
            return _deviceFound;
        }
        [[nodiscard]] NimBLEAddress getFoundDeviceAddress() const {
            return _foundDeviceAddress;
        }
        [[nodiscard]] String getFoundDeviceName() const {
            return _foundDeviceName;
        }
        void clearFoundDevice() {
            _deviceFound = false;
        }
        void setTargetMac(String mac) {
            _targetMac = std::move(mac);
        }
        void setDebug(const bool debug) {
            _debug = debug;
        }

    private:
        bool _deviceFound = false;
        NimBLEAddress _foundDeviceAddress = {};
        String _foundDeviceName;
        String _targetMac = "";
        bool _debug = false;
        static bool isSupportedScale(const String& name);
};

class MyClientCallback : public NimBLEClientCallbacks {
    public:
        void onConnect(NimBLEClient* pclient) override;
        void onDisconnect(NimBLEClient* pclient, int reason) override;
        void setDebug(const bool debug) {
            _debug = debug;
        }

    private:
        bool _debug = false;
};

class AcaiaArduinoBLE {
    public:
        explicit AcaiaArduinoBLE(bool debug);
        ~AcaiaArduinoBLE();
        bool init(const String& = "");
        bool updateConnection();
        [[nodiscard]] bool isConnecting() const;
        void tare();
        void startTimer() const;
        void stopTimer() const;
        void resetTimer() const;
        bool heartbeat();
        [[nodiscard]] float getWeight() const;
        [[nodiscard]] bool heartbeatRequired() const;
        [[nodiscard]] bool isConnected() const;
        bool newWeightAvailable();

    private:
        static void staticNotifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
        void notifyCallback(const uint8_t* pData, size_t length);
        void cleanup();
        void clearScanResults();

        // Debug functions
        void printData(const uint8_t data[], size_t length);

        float _currentWeight;
        NimBLEClient* _pClient;
        NimBLERemoteCharacteristic* _pWriteCharacteristic;
        NimBLERemoteCharacteristic* _pReadCharacteristic;
        NimBLEScan* _pBLEScan;
        MyAdvertisedDeviceCallbacks* _pAdvertisedDeviceCallbacks;
        MyClientCallback* _pClientCallback;

        unsigned long _lastHeartBeat;
        bool _connected;
        scale_type _type;
        bool _debug;
        unsigned long _lastPacket;
        bool _newWeightAvailable;

        uint8_t decent_scale_tare_counter;

        ConnectionState _connectionState;
        unsigned long _connectionStartTime;
        String _targetMac;
        bool _cleanupComplete;
        unsigned long _lastScanClear;

        int _connectionAttempts;

        static AcaiaArduinoBLE* _instance; // For static callback
};
