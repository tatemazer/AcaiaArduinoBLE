/*
  AcaiaArduinoBLE.h - Library for connecting to 
  an Acaia Scale using the ArduinoBLE library.
  Created by Tate Mazer, December 13, 2023.
  Released into the public domain.

  Pio Baettig: Adding Felicita Arc support 
*/


#define LIBRARY_VERSION        "3.2.0"
#define WRITE_CHAR_OLD_VERSION "2a80"
#define READ_CHAR_OLD_VERSION  "2a80"
#define WRITE_CHAR_NEW_VERSION "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR_NEW_VERSION  "49535343-1e4d-4bd9-ba61-23c647249616"
#define WRITE_CHAR_GENERIC     "ff12"
#define READ_CHAR_GENERIC      "ff11"
#define HEARTBEAT_PERIOD_MS     2750
#define MAX_PACKET_PERIOD_MS    5000

#include <Arduino.h>

#include <NimBLEDevice.h>

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

static const NimBLEAdvertisedDevice* advDevice;
static bool                          doConnect  = false;
static uint32_t                      scanTimeMs = 5000; /** scan time in milliseconds, 0 = scan forever */

/** Define a class to handle the callbacks when scan events are received */
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        //Serial.printf("Advertised Device found: %s\n", advertisedDevice->getName().c_str());



        if (advertisedDevice->getName().substr(0,5) == "CINCO") {
            Serial.printf("Found Our Service\n");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            advDevice = advertisedDevice;
            /** Ready to connect now */
            doConnect = true;
        }
    }

    /** Callback to process the results of the completed scan or restart it */
    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("Scan Ended, reason: %d, device count: %d; Restarting scan\n", reason, results.getCount());
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    std::string str  = (isNotify == true) ? "Notification" : "Indication";
    str             += " from ";
    str             += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
    str             += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
    str             += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
    str             += ", Value = " + std::string((char*)pData, length);

    //Serial.printf("%s\n", str.c_str());
    //Serial.printf("%i\n", length);
    if(length == 13){
        float currentWeight = (((pData[6] & 0xff) << 8) + (pData[5] & 0xff)) 
                            / pow(10,pData[9])
                            * ((pData[10] & 0x02) ? -1 : 1);

        Serial.printf("%.2f\n", currentWeight);
    }
    

}

/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getCreatedClientCount()) {
        /**
         *  Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient) {
            if (!pClient->connect(advDevice, false)) {
                Serial.printf("Reconnect failed\n");
                return false;
            }
            Serial.printf("Reconnected client\n");
        } else {
            /**
             *  We don't already have a client that knows this device,
             *  check for a client that is disconnected that we can use.
             */
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!pClient) {
        if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.printf("Max clients reached - no more connections available\n");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        Serial.printf("New client created\n");

        //pClient->setClientCallbacks(&clientCallbacks, false);
        /**
         *  Set initial connection parameters:
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
         */
        pClient->setConnectionParams(12, 12, 0, 150);

        /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
        pClient->setConnectTimeout(5 * 1000);

        if (!pClient->connect(advDevice)) {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            Serial.printf("Failed to connect, deleted client\n");
            return false;
        }
    }

    if (!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            Serial.printf("Failed to connect\n");
            return false;
        }
    }

    Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

    /** Now we can read/write/subscribe the characteristics of the services we are interested in */
    NimBLERemoteService*        pSvc = nullptr;
    NimBLERemoteCharacteristic* pWriteChr = nullptr;
    NimBLERemoteCharacteristic* pReadChr = nullptr;

    pSvc = pClient->getService("49535343-fe7d-4ae5-8fa9-9fafd205e455");
    if (pSvc) {
        pWriteChr = pSvc->getCharacteristic(WRITE_CHAR_NEW_VERSION);
        pReadChr  = pSvc->getCharacteristic(READ_CHAR_NEW_VERSION);
    }

    if(pReadChr->canNotify()){
        Serial.printf("can Notify!\n");
        if (!pReadChr->subscribe(true, notifyCB)) {
                pClient->disconnect();
                Serial.printf("can't Subscribe!\n");
                return false;
        }
    }
    if (pWriteChr->canWrite()) {
        //Write Indentify and Notification Request arrays to start receiving data.
        if (pWriteChr->writeValue(IDENTIFY) && pWriteChr->writeValue(NOTIFICATION_REQUEST)) {
            Serial.printf("Wrote new values to: %s\n", pWriteChr->getUUID().toString().c_str());
        } else {
            pClient->disconnect();
            return false;
        }
    }

    Serial.printf("Done with this device!\n");
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.printf("Starting NimBLE Client\n");

    /** Initialize NimBLE and set the device name */
    NimBLEDevice::init("NimBLE-Client");

    /** Optional: set the transmit power */
    //NimBLEDevice::setPower(3); /** 3dbm */

    NimBLEScan* pScan = NimBLEDevice::getScan();

    /** Set the callbacks to call when scan events occur, no duplicates */
    pScan->setScanCallbacks(&scanCallbacks, false);

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(100);
    pScan->setWindow(100);

    /**
     * Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    //pScan->setActiveScan(true);

    /** Start scanning for advertisers */
    pScan->start(scanTimeMs);
    Serial.printf("Scanning for peripherals\n");
}

void loop() {
    /** Loop here until we find a device we want to connect to */
    delay(10);

    if (doConnect) {
        doConnect = false;
        /** Found a device we want to connect to, do it now */
        if (connectToServer()) {
            Serial.printf("Success! we should now be getting notifications, scanning for more!\n");
        } else {
            Serial.printf("Failed to connect, starting scan\n");
        }

        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
}
