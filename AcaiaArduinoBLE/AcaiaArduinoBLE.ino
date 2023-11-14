/*
  AcaiaArduinoBLE.ino - Example of connecting an acaia scale to an arduino device using the ArduinoBLE library

  Immediately Connects to an acaia scale named ACAIA_NAME, 
  tare's the scale when the "in" gpio is triggered (active low),
  and then triggers the "out" gpio to stop the shot once ( END_WEIGHT - WEIGHT OFFSET ) is achieved.

  Tested on a Acaia Pyxis, Arduino nano ESP32, and La Marzocco GS3

  Created by Tate Mazer, 2023.

  Released under the MIT license.

  https://github.com/tatemazer/AcaiaArduinoBLE

*/

#include <ArduinoBLE.h>

//Bluetooth
#define ACAIA_NAME "CINCO531E83" // Use the ArduinoBLE Example code "PeripheralExplorer" To find your device's name
#define WRITE_CHAR "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR  "49535343-1e4d-4bd9-ba61-23c647249616"
#define END_WEIGHT 36
#define WEIGHT_OFFSET 3
byte identify[21]             = { 0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d, 0x00};
byte heartbeat[7]             = { 0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00 };
byte tare[20]                 = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
byte notification_request[14] = { 0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06 };
float currentWeight = 0;

// button 
int in = 10;
int out = 11;
bool buttonPressed = false;
unsigned long lastButtonRead = 0;
unsigned long buttonReadPeriod_ms = 25;
int lastButtonStates[] = {1,1,1,1,1};
bool brewing = false;
unsigned long shotStart_ms = 0;

void setup() {
  Serial.begin(9600);

  // make the pushbutton's pin an input:
  pinMode(in, INPUT_PULLUP);
  pinMode(out, OUTPUT);

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
    write.writeValue(notification_request,14);


    byte input[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
    do{

      //Check for new weight messages
      if(read.valueUpdated() && read.valueLength() == 13 && read.readValue(input,13) && input[4] == 0x05){
        /*
        //Print raw hex messages
        for(int i=0; i<13; i++){
            char hexChar[2];
            sprintf(hexChar, "%02X ", input[i]);
            Serial.print(hexChar);
        }
        */
        currentWeight = (((input[6] & 0xff) << 8) + (input[5] & 0xff))/100.0;
        Serial.print(currentWeight);
        Serial.println();
      //}
      }
      
      //Send heartbeat to keep bluetooth connection alive
      if(millis() - lastReset > 2750){
        write.writeValue(heartbeat, 7);
        lastReset = millis();

        /*if(write.writeValue(tare, 20)){
          Serial.println("tare write successful");
        }*/
      }

      
      // Read button every ButtonReadPeriod_ms
    if(millis() > (lastButtonRead + buttonReadPeriod_ms) ){
      lastButtonRead = millis();

      lastButtonStates[4] = lastButtonStates[3];
      lastButtonStates[3] = lastButtonStates[2];
      lastButtonStates[2] = lastButtonStates[1];
      lastButtonStates[1] = lastButtonStates[0];
      lastButtonStates[0] = digitalRead(in);
      //Serial.println(lastButtonStates[0]);

      //button pressed
      if( !(  lastButtonStates[0] 
          &&  lastButtonStates[1]  
          &&  lastButtonStates[2]  
          &&  lastButtonStates[3]  
          &&  lastButtonStates[4])  
          && buttonPressed == false){
        Serial.println("ButtonPressed");
        buttonPressed = true;
      }
      //button released
      else if(lastButtonStates[0] 
          &&  lastButtonStates[1]  
          &&  lastButtonStates[2]  
          &&  lastButtonStates[3]  
          &&  lastButtonStates[4]   
          &&  buttonPressed == true ){
        Serial.println("ButtonReleased");
        buttonPressed = false;
        brewing = !brewing;
        if(brewing){
          Serial.println("shot started");
          shotStart_ms = millis();
          if(write.writeValue(tare, 20)){
            Serial.println("tare write successful");
          }
        }
      }
    }

    //End shot
    if(brewing 
    && currentWeight >= (END_WEIGHT - WEIGHT_OFFSET)
    && millis() > (shotStart_ms + 3000) 
    ){
      brewing = false;
      Serial.println("ShotEnded");
      digitalWrite(out,HIGH);
      delay(300);
      digitalWrite(out,LOW);
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