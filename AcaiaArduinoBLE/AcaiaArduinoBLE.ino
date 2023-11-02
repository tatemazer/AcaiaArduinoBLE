/*
  AcaiaArduinoBLE.ino - Example of connecting an acaia scale to an arduino device using the ArduinoBLE library

  Connects to an acaia scale named ACAIA_NAME, and sends a TARE command every 3 seconds.

  Tested on a Acaia Pyxis

  Known issue: unable to receive weight information. The datastream does not come through.

  Created by Tate Mazer, 2023.

  Released under the MIT license.

  https://github.com/tatemazer/AcaiaArduinoBLE

*/

#define ACAIA_NAME "CINCO531E83"
#define WRITE_CHAR "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR  "49535343-1e4d-4bd9-ba61-23c647249616"

byte identify[21] = { 0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d, 0x00};
byte heartbeat[7] = { 0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00 };
byte tare[20]     = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    
#include <ArduinoBLE.h>

void setup() {
  Serial.begin(9600);
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1);
  }
  BLE.scan();
}

void loop() {
  BLEDevice peripheral = BLE.available();
  if (peripheral && peripheral.localName() == ACAIA_NAME) {
    BLE.stopScan();

    Serial.println("Connecting ...");
    if (peripheral.connect()) {
      Serial.println("Connected");
    } else {
      Serial.println("Failed to connect!");
      return;
    }

    Serial.println("Discovering attributes ...");
    if (peripheral.discoverAttributes()) {
      Serial.println("Attributes discovered");
    } else {
      Serial.println("Attribute discovery failed!");
      peripheral.disconnect();
      return;
    }

    BLECharacteristic write = peripheral.characteristic(WRITE_CHAR);
    BLECharacteristic read = peripheral.characteristic(READ_CHAR);

    if(!read.canSubscribe()){
      Serial.println("unable to subscribe to READ");
      return;
    }else if(!read.subscribe()){
      Serial.println("subscription failed");
      return;
    }else {
      Serial.println("subscribed!");
    }
  
    if(write.writeValue(identify, 20))
      Serial.println("identify write successful");

    unsigned long lastReset = millis();

    do{
      /*
      //this doesnt seem to work, values are nonsensical and they only come 
      // when the tare/heartbeat is written.
      if(read.valueUpdated()){

        int length = read.valueLength();
        if(length>0){
          Serial.print("new datapoint of length ");
          Serial.println(length);
          byte input[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
          read.readValue(input, 13);
          for(int i=0; i<length; i++){
            char hexChar[2];
            sprintf(hexChar, "%02X ", input[i]);
            Serial.print(hexChar);
          }
          Serial.println();
        }
      }
      */
      if(millis() - lastReset > 3000 && write.writeValue(heartbeat, 7)){
        //Serial.println("heartbeat write successful");
        lastReset = millis();

        if(write.writeValue(tare, 20)){
          Serial.println("tare write successful");
        }
      }
        

    }while(peripheral.connected());
    Serial.println("Disconnecting ...");
    peripheral.disconnect();
    Serial.println("Disconnected");

    // peripheral disconnected, we are done
    while (1) {
      // do nothing
    }
  }
}