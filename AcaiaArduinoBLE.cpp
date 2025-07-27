/*
  AcaiaArduinoBLE.cpp - ESP32 BLE Library for connecting to
  Acaia Scales and compatible devices using NimBLE.
  Created by Tate Mazer, December 13, 2023.
  ESP32 fork with NimBLE support for smaller footprint.

  Adding Generic Scale Support, Pio Baettig
*/
#include "AcaiaArduinoBLE.h"
#include "Arduino.h"

#include <utility>

const byte IDENTIFY[20] = {0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d};
const byte HEARTBEAT[7] = {0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00};
const byte NOTIFICATION_REQUEST[14] = {0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06};
const byte START_TIMER[7] = {0xef, 0xdd, 0x0d, 0x00, 0x00, 0x00, 0x00};
const byte STOP_TIMER[7] = {0xef, 0xdd, 0x0d, 0x00, 0x02, 0x00, 0x02};
const byte RESET_TIMER[7] = {0xef, 0xdd, 0x0d, 0x00, 0x01, 0x00, 0x01};
const byte TARE_ACAIA[6] = {0xef, 0xdd, 0x04, 0x00, 0x00, 0x00};
const byte TARE_GENERIC[6] = {0x03, 0x0a, 0x01, 0x00, 0x00, 0x08};
const byte START_TIMER_GENERIC[6] = {0x03, 0x0a, 0x04, 0x00, 0x00, 0x0a};
const byte STOP_TIMER_GENERIC[6] = {0x03, 0x0a, 0x05, 0x00, 0x00, 0x0d};
const byte RESET_TIMER_GENERIC[6] = {0x03, 0x0a, 0x06, 0x00, 0x00, 0x0c};
const byte START_TIMER_DECENT[7] = {0x03, 0x0b, 0x03, 0x00, 0x00, 0x00, 0x08};
const byte STOP_TIMER_DECENT[7] = {0x03, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x0b};
const byte RESET_TIMER_DECENT[7] = {0x03, 0x0b, 0x02, 0x00, 0x00, 0x00, 0x09};

// Static instance for callback
AcaiaArduinoBLE* AcaiaArduinoBLE::_instance = nullptr;

// Callback implementations
void MyAdvertisedDeviceCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (_debug) {
        Serial.print("Found ");
        Serial.print(advertisedDevice->getAddress().toString().c_str());
        Serial.print(" '");
        Serial.print(advertisedDevice->getName().c_str());
        Serial.println("'");
    }

    // Check if this is a supported scale
    if (isSupportedScale(advertisedDevice->getName().c_str())) {
        if (_targetMac != "" && String(advertisedDevice->getAddress().toString().c_str()) != _targetMac) {
            return;
        }

        if (_debug) Serial.println("Scale found, stopping scan...");

        _deviceFound = true;
        _foundDeviceAddress = advertisedDevice->getAddress();
        _foundDeviceName = advertisedDevice->getName().c_str();

        NimBLEDevice::getScan()->stop();
    }
}

void MyAdvertisedDeviceCallbacks::onScanEnd(const NimBLEScanResults& scanResults, const int reason) {
    if (_debug) {
        Serial.print("Scan ended, reason: ");
        Serial.println(reason);
    }
}

bool MyAdvertisedDeviceCallbacks::isSupportedScale(const String& name) {
    String normalizedName = name;
    normalizedName.trim();
    normalizedName.toUpperCase();

    return normalizedName.startsWith("ACAIA") || normalizedName.startsWith("LUNAR") || normalizedName.startsWith("PYXIS") || normalizedName.startsWith("PEARL") || normalizedName.startsWith("CINCO") ||
           normalizedName.startsWith("PROCH") || normalizedName.startsWith("BOOKOO") || normalizedName.startsWith("DECENT") || normalizedName.startsWith("ESPRESSISCALE");
}

void MyClientCallback::onConnect(NimBLEClient* pclient) {
    if (_debug) Serial.println("Client connected");
}

void MyClientCallback::onDisconnect(NimBLEClient* pclient, const int reason) {
    if (_debug) {
        Serial.print("Client disconnected, reason: ");
        Serial.println(reason);
    }
}

// Main class implementation
AcaiaArduinoBLE::AcaiaArduinoBLE(const bool debug) {
    _debug = debug;
    _currentWeight = 0;
    _connected = false;
    _connectionState = IDLE;
    _connectionStartTime = 0;
    _targetMac = "";
    _newWeightAvailable = false;
    _pClient = nullptr;
    _pWriteCharacteristic = nullptr;
    _pReadCharacteristic = nullptr;
    _pBLEScan = nullptr;
    _pAdvertisedDeviceCallbacks = nullptr;
    _pClientCallback = nullptr;
    _lastHeartBeat = 0;
    _type = OLD;
    _lastPacket = 0;
    _cleanupComplete = false;
    _connectionAttempts = 0;
    decent_scale_tare_counter = 0;
    _lastScanClear = 0;

    _instance = this; // Set static instance for callbacks
}

AcaiaArduinoBLE::~AcaiaArduinoBLE() {
    cleanup();
}

void AcaiaArduinoBLE::cleanup() {
    if (_cleanupComplete) {
        return;
    }

    _cleanupComplete = true;

    // Stop scanning first
    if (_pBLEScan) {
        _pBLEScan->stop();
        clearScanResults();
        _pBLEScan = nullptr;
    }

    // Disconnect and clean up client
    if (_pClient) {
        if (_pClient->isConnected()) {
            _pClient->disconnect();
        }

        // Give time for proper disconnect
        delay(100);

        // Clear characteristics
        _pWriteCharacteristic = nullptr;
        _pReadCharacteristic = nullptr;

        // Let NimBLE handle the client cleanup
        _pClient = nullptr;
    }

    // Clean up callbacks
    delete _pAdvertisedDeviceCallbacks;
    _pAdvertisedDeviceCallbacks = nullptr;

    delete _pClientCallback;
    _pClientCallback = nullptr;

    // Reset connection state
    _connected = false;
    _connectionState = IDLE;

    if (_instance == this) {
        _instance = nullptr;
    }
}

bool AcaiaArduinoBLE::init(const String& mac) {
    if (_debug) Serial.println("Initializing NimBLE...");

    // Clean up any existing state first
    cleanup();
    _cleanupComplete = false;

    _instance = this;

    NimBLEDevice::init("");

    // Set BLE power to maximum for better connection reliability
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    _targetMac = mac;
    _connectionStartTime = millis();
    _connectionState = SCANNING;
    _lastPacket = 0;
    _connected = false;
    _connectionAttempts = 0;

    // Create callbacks
    _pAdvertisedDeviceCallbacks = new MyAdvertisedDeviceCallbacks();
    _pAdvertisedDeviceCallbacks->setTargetMac(_targetMac);
    _pAdvertisedDeviceCallbacks->setDebug(_debug);

    _pClientCallback = new MyClientCallback();
    _pClientCallback->setDebug(_debug);

    // Start scanning
    _pBLEScan = NimBLEDevice::getScan();

    if (!_pBLEScan) {
        if (_debug) Serial.println("Failed to get BLE scan object");
        return false;
    }

    // From https://github.com/gaggimate/esp-arduino-ble-scales/blob/main/src/remote_scales.cpp
    // CRITICAL: Set true to prevent storing unwanted devices and avoid memory leaks
    _pBLEScan->setScanCallbacks(_pAdvertisedDeviceCallbacks, true);
    _pBLEScan->setActiveScan(true);
    _pBLEScan->setInterval(500);			// Use the more conservative values
    _pBLEScan->setWindow(100);
    _pBLEScan->setMaxResults(0);			// No limit on results
    _pBLEScan->setDuplicateFilter(false);	// Allow duplicates for better detection


    if (_debug) Serial.println("Starting BLE scan...");

    if (!_pBLEScan->start(0)) { // 0 = scan indefinitely
        if (_debug) Serial.println("Failed to start BLE scan");
        return false;
    }

    if (_debug) Serial.println("Starting connection process...");
    return true;
}

bool AcaiaArduinoBLE::updateConnection() {
    // Reset attempt counter on successful connection
    if (_connectionState == CONNECTED && _connectionAttempts > 0) {
        _connectionAttempts = 0;
    }

    switch (_connectionState) {
        case SCANNING:
            // Clear scan results every 30 seconds during long scans to prevent memory buildup
            if (millis() - _lastScanClear > 30000) {
                clearScanResults();
                _lastScanClear = millis();
            }

            // Reduced timeout for scanning - 15 seconds instead of 30
            if (millis() - _connectionStartTime > 15000) {
                if (_debug) Serial.println("Scan timeout - no scales found");
                _connectionState = FAILED;
                _connectionStartTime = millis();
                break;
            }

            if (_pAdvertisedDeviceCallbacks->hasFoundDevice()) {
                if (_debug) Serial.println("Scale found, attempting connection...");
                _connectionState = CONNECTING;
                _connectionStartTime = millis(); // Reset timeout for connection phase
            }

            break;

        case CONNECTING:
            {
                // Reduced timeout for connecting - 8 seconds instead of 10
                if (millis() - _connectionStartTime > 8000) {
                    if (_debug) Serial.println("Connection timeout");
                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                    break;
                }

                if (_debug) Serial.println("Connecting...");

                // Clean up any existing client first
                if (_pClient) {
                    if (_pClient->isConnected()) {
                        _pClient->disconnect();
                    }
                    delay(200);
                    _pClient = nullptr;
                }

                // Create client
                _pClient = NimBLEDevice::createClient();

                if (!_pClient) {
                    if (_debug) Serial.println("Failed to create client - will retry");
                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                    break;
                }

                // Set client callbacks
                if (_pClientCallback) {
                    _pClient->setClientCallbacks(_pClientCallback, false);
                }

                // Try connection with explicit timeout
                NimBLEAddress scaleAddress = _pAdvertisedDeviceCallbacks->getFoundDeviceAddress();

                if (_debug) {
                    Serial.print("Attempting connection to: ");
                    Serial.println(scaleAddress.toString().c_str());
                }

                // Use blocking connect with timeout
                bool connected = false;

                try {
                    connected = _pClient->connect(scaleAddress, false); // false = not delete on disconnect
                } catch (...) {
                    if (_debug) Serial.println("Exception during connection attempt");
                    connected = false;
                }

                if (connected && _pClient->isConnected()) {
                    if (_debug) Serial.println("Connected!");
                    _connectionState = DISCOVERING;
                    _connectionStartTime = millis();
                }
                else {
                    if (_debug) Serial.println("Failed to connect!");

                    // Connection failed - properly clean up
                    if (_pClient) {
                        _pClient = nullptr;
                    }

                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                }

                break;
            }

        case DISCOVERING:
            {
                // Reduced timeout for discovery - 3 seconds instead of 5
                if (millis() - _connectionStartTime > 3000) {
                    if (_debug) Serial.println("Service discovery timeout");
                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                    break;
                }

                if (_debug) Serial.println("Discovering services...");

                const std::vector<NimBLERemoteService*> services = _pClient->getServices(true);

                if (_debug) {
                    Serial.print("Found ");
                    Serial.print(services.size());
                    Serial.println(" services");
                }

                // Try different service UUIDs to determine scale type
                const NimBLERemoteService* pService = nullptr;

                // Try OLD acaia version
                for (const auto service : services) {
                    std::vector<NimBLERemoteCharacteristic*> chars = service->getCharacteristics(true);

                    for (const auto ch : chars) {
                        if (ch->getUUID().equals(NimBLEUUID(READ_CHAR_OLD_VERSION))) {
                            pService = service;
                            _type = OLD;
                            _pWriteCharacteristic = ch;
                            _pReadCharacteristic = ch; // Same characteristic for old version
                            if (_debug) Serial.println("Old version Acaia detected");
                            break;
                        }
                    }

                    if (pService) break;
                }

                // Try NEW acaia version
                if (!pService) {
                    pService = _pClient->getService(NimBLEUUID(WRITE_CHAR_NEW_VERSION));

                    if (pService) {
                        _type = NEW;
                        _pWriteCharacteristic = pService->getCharacteristic(NimBLEUUID(WRITE_CHAR_NEW_VERSION));
                        _pReadCharacteristic = pService->getCharacteristic(NimBLEUUID(READ_CHAR_NEW_VERSION));
                        if (_debug) Serial.println("New version Acaia detected");
                    }
                }

                // Try GENERIC scale
                if (!pService) {
                    pService = _pClient->getService(NimBLEUUID(SUUID_GENERIC));

                    if (pService) {
                        _type = GENERIC;
                        _pWriteCharacteristic = pService->getCharacteristic(NimBLEUUID(WRITE_CHAR_GENERIC));
                        _pReadCharacteristic = pService->getCharacteristic(NimBLEUUID(READ_CHAR_GENERIC));
                        if (_debug) Serial.println("Generic scale detected");
                    }
                }

                // Try DECENT/EspressiScale
                if (!pService) {
                    pService = _pClient->getService(NimBLEUUID(SUUID_DECENTSCALE));

                    if (pService) {
                        _type = DECENT;
                        _pWriteCharacteristic = pService->getCharacteristic(NimBLEUUID(WRITE_CHAR_DECENT));
                        _pReadCharacteristic = pService->getCharacteristic(NimBLEUUID(READ_CHAR_DECENT));
                        if (_debug) Serial.println("Decent/EspressiScale detected");
                    }
                }

                if (pService && _pWriteCharacteristic && _pReadCharacteristic) {
                    if (_debug) Serial.println("Service and characteristics found");
                    _connectionState = CONFIGURING;
                    _connectionStartTime = millis(); // Reset timeout for configuration phase
                }
                else {
                    if (_debug) Serial.println("Failed to find service or characteristics");
                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                }

                break;
            }

        case CONFIGURING:
            // Reduced timeout for configuring - 3 seconds instead of 5
            if (millis() - _connectionStartTime > 3000) {
                if (_debug) Serial.println("Configuration timeout");
                _connectionState = FAILED;
                _connectionStartTime = millis();
                break;
            }

            if (_debug) Serial.println("Configuring scale...");

            // Register for notifications
            if (_pReadCharacteristic->canNotify()) {
                if (!_pReadCharacteristic->subscribe(true, staticNotifyCallback)) {
                    if (_debug) Serial.println("Failed to register for notifications");
                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                    break;
                }
                if (_debug) Serial.println("Registered for notifications");
            }
            else {
                if (_debug) Serial.println("Cannot register for notifications");
                _connectionState = FAILED;
                _connectionStartTime = millis();
                break;
            }

            // Send identify command (except for GENERIC scales)
            if (_type != GENERIC) {
                if (!_pWriteCharacteristic->writeValue(IDENTIFY, 20, false)) {
                    if (_debug) Serial.println("Failed to send identify command");
                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                    break;
                }

                if (_debug) Serial.println("Identify command sent");
                delay(200); // Give the scale time to process
            }

            // Send notification request (except for GENERIC scales)
            if (_type != GENERIC) {
                if (!_pWriteCharacteristic->writeValue(NOTIFICATION_REQUEST, 14, false)) {
                    if (_debug) Serial.println("Failed to send notification request");
                    _connectionState = FAILED;
                    _connectionStartTime = millis();
                    break;
                }

                if (_debug) Serial.println("Notification request sent");
                delay(200); // Give the scale time to process
            }

            _connected = true;
            _lastPacket = 0;
            _connectionState = CONNECTED;

            if (_debug) Serial.println("Scale connection completed successfully!");

            return true;

        case CONNECTED:
            // Check if the client is still connected
            if (!_pClient || !_pClient->isConnected()) {
                if (_debug) Serial.println("BLE client disconnected");
                _connectionState = FAILED;
                _connected = false;
                _connectionStartTime = millis();
                break;
            }

            if (_lastPacket > 0 && millis() - _lastPacket > MAX_PACKET_PERIOD_MS) {
                if (_debug) Serial.println("Scale data timeout");
                _connectionState = FAILED;
                _connected = false;
                _connectionStartTime = millis();
                break;
            }

            // Send periodic heartbeat for OLD scales to keep connection alive
            if (_type == OLD) {
                static unsigned long lastHeartbeat = 0;
                if (millis() - lastHeartbeat > 4000) { // Send heartbeat every 4 seconds
                    if (_pWriteCharacteristic) {
                        if (_pWriteCharacteristic->writeValue(HEARTBEAT, 20, false)) {
                            if (_debug) Serial.println("Heartbeat sent: success");
                        }
                        else {
                            if (_debug) Serial.println("Heartbeat sent: failed");
                        }
                    }

                    lastHeartbeat = millis();
                }
            }

            break;

        case FAILED:
            {
                _connectionAttempts++;

                // More aggressive reconnection timing
                unsigned long backoffTime = 1000;                 // Start with 1 second
                if (_connectionAttempts > 3) backoffTime = 2000;  // 2s after 3 attempts
                if (_connectionAttempts > 6) backoffTime = 3000;  // 3s after 6 attempts
                if (_connectionAttempts > 10) backoffTime = 5000; // 5s after 10 attempts

                if (millis() - _connectionStartTime > backoffTime) {
                    if (_debug) {
                        Serial.print("Auto-reconnecting (attempt ");
                        Serial.print(_connectionAttempts);
                        Serial.println(")...");
                    }

                    // Clear any accumulated scan results before restart
                    clearScanResults();

                    // More aggressive cleanup after multiple failures
                    if (_connectionAttempts > 8) {
                        if (_debug) Serial.println("Multiple failures - performing deep cleanup");

                        // Stop scan first
                        if (_pBLEScan) {
                            _pBLEScan->stop();
                            delay(100);
                        }

                        // Clean up client
                        if (_pClient) {
                            if (_pClient->isConnected()) {
                                _pClient->disconnect();
                            }
                            delay(100);
                            _pClient = nullptr;
                        }

                        // Reset NimBLE stack
                        NimBLEDevice::deinit(true);
                        delay(500);
                        NimBLEDevice::init("");
                        delay(200);

                        // Recreate scan object
                        _pBLEScan = NimBLEDevice::getScan();

                        if (_pBLEScan) {
                            _pBLEScan->setScanCallbacks(_pAdvertisedDeviceCallbacks, true);
                            _pBLEScan->setActiveScan(true);
                            _pBLEScan->setInterval(500);
                            _pBLEScan->setWindow(100);
                            _pBLEScan->setMaxResults(0);
                            _pBLEScan->setDuplicateFilter(false);
                        }

                        // Reset attempt counter after deep cleanup
                        _connectionAttempts = 0;
                    }
                    else {
                        // Light cleanup for early failures
                        if (_pClient) {
                            if (_pClient->isConnected()) {
                                _pClient->disconnect();
                            }
                            delay(100);
                            _pClient = nullptr;
                        }
                    }

                    _pAdvertisedDeviceCallbacks->clearFoundDevice();

                    _connectionState = SCANNING;
                    _connectionStartTime = millis();
                    _connected = false;

                    // Restart scan
                    if (_pBLEScan) {
                        _pBLEScan->start(0);
                    }
                }
                return false;
            }

        case IDLE:
            if (millis() - _connectionStartTime > 500) { // Quick transition to scanning
                _connectionState = SCANNING;
                _connectionStartTime = millis();
            }
            break;

        default:
            break;
    }

    return _connectionState == CONNECTED;
}

bool AcaiaArduinoBLE::isConnecting() const {
    return _connectionState == SCANNING || _connectionState == CONNECTING || _connectionState == DISCOVERING || _connectionState == CONFIGURING;
}

bool AcaiaArduinoBLE::tare() {
    if (!_connected || !_pWriteCharacteristic) return false;

    bool success = false;

    if (_type == DECENT) {
        // Increment tare counter (0-255, wraps around)
        decent_scale_tare_counter = (decent_scale_tare_counter + 1) % 256;

        uint8_t cmd[7] = {0x03, 0x0F, decent_scale_tare_counter, 0x00, 0x00, 0x00, 0x00};
        cmd[6] = 0x0F ^ decent_scale_tare_counter; // XOR checksum

        success = _pWriteCharacteristic->writeValue(cmd, sizeof(cmd), false);

        if (_debug) {
            Serial.print("Decent Scale tare command sent: ");
            Serial.println(success ? "success" : "failed");
        }
    }
    else {
        if (_type == GENERIC) {
            success = _pWriteCharacteristic->writeValue(TARE_GENERIC, sizeof(TARE_GENERIC), false);
        }
        else {
            success = _pWriteCharacteristic->writeValue(TARE_ACAIA, sizeof(TARE_ACAIA), false);
        }

        if (_debug) {
            Serial.print("Tare command sent: ");
            Serial.println(success ? "success" : "failed");
        }
    }

    return success;
}

bool AcaiaArduinoBLE::startTimer() const {
    if (!_connected || !_pWriteCharacteristic) return false;

    bool success = false;

    if (_type == DECENT) {
        success = _pWriteCharacteristic->writeValue(START_TIMER_DECENT, sizeof(START_TIMER_DECENT), false);
    }
    else if (_type == GENERIC) {
        success = _pWriteCharacteristic->writeValue(START_TIMER_GENERIC, sizeof(START_TIMER_GENERIC), false);
    }
    else {
        success = _pWriteCharacteristic->writeValue(START_TIMER, sizeof(START_TIMER), false);
    }

    if (_debug) {
        Serial.print("Start timer command sent: ");
        Serial.println(success ? "success" : "failed");
    }

    return success;
}

bool AcaiaArduinoBLE::stopTimer() const {
    if (!_connected || !_pWriteCharacteristic) return false;

    bool success = false;

    if (_type == DECENT) {
        success = _pWriteCharacteristic->writeValue(STOP_TIMER_DECENT, sizeof(STOP_TIMER_DECENT), false);
    }
    else if (_type == GENERIC) {
        success = _pWriteCharacteristic->writeValue(STOP_TIMER_GENERIC, sizeof(STOP_TIMER_GENERIC), false);
    }
    else {
        success = _pWriteCharacteristic->writeValue(STOP_TIMER, sizeof(STOP_TIMER), false);
    }

    if (_debug) {
        Serial.print("Stop timer command sent: ");
        Serial.println(success ? "success" : "failed");
    }

    return success;
}

bool AcaiaArduinoBLE::resetTimer() const {
    if (!_connected || !_pWriteCharacteristic) return false;

    bool success = false;

    if (_type == DECENT) {
        success = _pWriteCharacteristic->writeValue(RESET_TIMER_DECENT, sizeof(RESET_TIMER_DECENT), false);
    }
    else if (_type == GENERIC) {
        success = _pWriteCharacteristic->writeValue(RESET_TIMER_GENERIC, sizeof(RESET_TIMER_GENERIC), false);
    }
    else {
        success = _pWriteCharacteristic->writeValue(RESET_TIMER, sizeof(RESET_TIMER), false);
    }

    if (_debug) {
        Serial.print("Reset timer command sent: ");
        Serial.println(success ? "success" : "failed");
    }

    return success;
}

bool AcaiaArduinoBLE::heartbeat() {
    if (!_connected || !_pWriteCharacteristic) return false;

    const bool success = _pWriteCharacteristic->writeValue(HEARTBEAT, sizeof(HEARTBEAT), false);

    if (success) {
        _lastHeartBeat = millis();
    }

    if (_debug) {
        Serial.print("Heartbeat sent: ");
        Serial.println(success ? "success" : "failed");
    }

    return success;
}

float AcaiaArduinoBLE::getWeight() const {
    return _currentWeight;
}

bool AcaiaArduinoBLE::heartbeatRequired() const {
    if (_type == OLD || _type == NEW) {
        return millis() - _lastHeartBeat > HEARTBEAT_PERIOD_MS;
    }
    return false;
}

bool AcaiaArduinoBLE::isConnected() const {
    return _connected;
}

bool AcaiaArduinoBLE::newWeightAvailable() {
    // Check if the connection is still alive
    if (_lastPacket && millis() - _lastPacket > MAX_PACKET_PERIOD_MS) {
        Serial.println("Connection timeout!");
        _connected = false;
        return false;
    }

    if (_newWeightAvailable) {
        _newWeightAvailable = false; // Reset flag after reading
        return true;
    }

    return false;
}

void AcaiaArduinoBLE::staticNotifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (_instance) {
        _instance->notifyCallback(pData, length);
    }
    else {
        Serial.println("ERROR: _instance is null! Cannot process notification.");
    }
}

void AcaiaArduinoBLE::notifyCallback(const uint8_t* pData, size_t length) {
    if (_debug) {
        Serial.print("Processing notification, length: ");
        Serial.print(length);
        Serial.print(", data: ");
        printData(pData, length);
        Serial.println();
    }

    if (length > 0) {
        _lastPacket = millis();

        bool newWeightPacket = false;

        // Parse based on scale type using original logic
        if (_type == OLD && (length == 10 || length == 14)) {
            // Parse old style data packet
            // Grab weight bytes (2 and 3),
            // apply scaling based on the unit byte (6)
            // get sign byte (7)

            uint16_t rawWeight = ((pData[3] & 0xff) << 8) + (pData[2] & 0xff);
            uint8_t scalingByte = pData[6];
            uint8_t signByte = pData[7];

            _currentWeight = rawWeight / pow(10, scalingByte) * ((signByte & 0x02) ? -1 : 1);
            newWeightPacket = true;

            if (_debug) {
                Serial.print("OLD scale - raw weight: ");
                Serial.print(rawWeight);
                Serial.print(", scaling: ");
                Serial.print(scalingByte);
                Serial.print(", sign: ");
                Serial.print(signByte);
                Serial.print(", final weight: ");
                Serial.println(_currentWeight);
            }
        }
        else if (((_type == NEW && (length == 13 || length == 17)) || (_type == OLD && length == 13)) && pData[4] == 0x05) {
            // Parse New style data packet
            _currentWeight = (((pData[6] & 0xff) << 8) + (pData[5] & 0xff)) / pow(10, pData[9]) * ((pData[10] & 0x02) ? -1 : 1);
            newWeightPacket = true;

            if (_debug) {
                Serial.print("NEW scale weight: ");
                Serial.println(_currentWeight);
            }
        }
        else if (_type == GENERIC && length == 20) {
            // Parse generic scale data packet
            _currentWeight = ((pData[7] << 16) | (pData[8] << 8) | pData[9]);

            if (pData[6] == 45) { // Check if the value is negative
                _currentWeight = -_currentWeight;
            }
            _currentWeight = _currentWeight / 100;
            newWeightPacket = true;

            if (_debug) {
                Serial.print("GENERIC scale weight: ");
                Serial.println(_currentWeight);
            }
        }
        else if (_type == DECENT) {
            if (_debug) Serial.println("Parsing DECENT/EspressiScale data");

            // EspressiScale/Decent scale data parsing
            // Format: 03 [cmdtype] [data1] [data2] [data3] [data4] [checksum]
            if (length == 7 && pData[0] == 0x03) {
                uint8_t cmdtype = pData[1];
                uint8_t data1 = pData[2];
                uint8_t data2 = pData[3];
                uint8_t data3 = pData[4];
                uint8_t data4 = pData[5];
                uint8_t checksum = pData[6];

                // Verify checksum (XOR of all data bytes)
                uint8_t calculated_checksum = 0x03 ^ cmdtype ^ data1 ^ data2 ^ data3 ^ data4;

                if (_debug) {
                    Serial.print("Decent Scale - cmdtype: 0x");
                    Serial.print(cmdtype, HEX);
                    Serial.print(", data: 0x");
                    Serial.print(data1, HEX);
                    Serial.print(" 0x");
                    Serial.print(data2, HEX);
                    Serial.print(" 0x");
                    Serial.print(data3, HEX);
                    Serial.print(" 0x");
                    Serial.print(data4, HEX);
                    Serial.print(", checksum: 0x");
                    Serial.print(checksum, HEX);
                    Serial.print(" (calc: 0x");
                    Serial.print(calculated_checksum, HEX);
                    Serial.println(")");
                }

                if (checksum == calculated_checksum) {
                    // Command type 0xCE appears to be weight data
                    if (cmdtype == 0xCE) {
                        // Weight is in data1 and data2 as 16-bit value
                        const uint16_t rawWeight = (data1 << 8) | data2;

                        // Convert to grams (assuming 0.1g resolution)
                        const float weight = rawWeight / 10.0f;

                        if (_debug) {
                            Serial.print("Weight data - raw: ");
                            Serial.print(rawWeight);
                            Serial.print(" -> weight: ");
                            Serial.print(weight);
                            Serial.println("g");
                        }

                        // Sanity check the weight value
                        if (weight >= 0 && weight < 5000) {
                            _currentWeight = weight;
                            newWeightPacket = true;

                            if (_debug) {
                                Serial.print("Updated weight to: ");
                                Serial.println(_currentWeight);
                            }
                        }
                    }
                    else if (_debug) {
                        Serial.print("Unknown command type: 0x");
                        Serial.println(cmdtype, HEX);
                    }
                }
                else if (_debug) {
                    Serial.println("Checksum mismatch - ignoring packet");
                }
            }
            else if (_debug) {
                Serial.print("Invalid packet format - length: ");
                Serial.print(length);
                Serial.print(", first byte: 0x");
                Serial.println(pData[0], HEX);
            }
        }

        if (newWeightPacket) {
            _newWeightAvailable = true;

            if (_debug) {
                Serial.print("Weight updated to: ");
                Serial.print(_currentWeight);
                Serial.println("g - flagged as new weight available");
            }
        }
    }
}

void AcaiaArduinoBLE::clearScanResults() {
    if (_pBLEScan) {
        _pBLEScan->clearResults();
        if (_debug) Serial.println("Cleared BLE scan results to free memory");
    }
}

void AcaiaArduinoBLE::printData(const uint8_t data[], const size_t length) {
    for (size_t i = 0; i < length; i++) {
        const uint8_t b = data[i];

        if (b < 16) {
            Serial.print("0");
        }

        Serial.print(b, HEX);

        if (i < length - 1) Serial.print(" ");
    }
}
