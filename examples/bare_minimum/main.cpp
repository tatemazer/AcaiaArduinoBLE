#include <Arduino.h>

//The timeout has been set very tightely for "generic" scales. The Bookoo Themis Mini handles 500ms. If you have issues with your scale giving a timeout use, increase before you call the header.
//#define MAX_PACKET_PERIOD_GENERIC_MS 1000
#include <AcaiaArduinoBLE.h>

//true doesnt work...
#define DEBUG false

bool scaleConnected = false;
unsigned long lastTare = 0;


AcaiaArduinoBLE scale(DEBUG);
uint8_t goalWeight = 100;      // Goal Weight to be read from EEPROM

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("Scale Interface test");

  // initialize the Bluetooth® Low Energy hardware
  BLE.begin();
  // Optionally add your Mac Address as an argument: acaia.init("##:##:##:##:##:##");

  //this is basically not useful, since it will always connect during 'loop()'
    if(scale.init()) { 
    scale.tare();
    scale.tare();
    }
}

void loop() {
  // reconnect the scale if its not connected. Note this is an expensive function. Has a self reset of 50ms.
  if(scale.isConnected() == false) { 
    scale.init();
  }

  // Send a heartbeat message to the scale periodically to maintain connection if necessary.
  if (scale.heartbeatRequired()) {
    scale.heartbeat();
  }

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if (scale.isConnected() && scale.newWeightAvailable()) {
    Serial.println(scale.getWeight());
  }

  // To test ; tare the scale every 10 seconds
  // This is not necessary, but useful for testing purposes
  if(lastTare + 10000 < millis()) { 
    lastTare = millis();
    //there is a limit to the amount of commands you can send within a timeframe. The library does not handle this. YMMV 
    //note that "tare" temporary locks up the BOOKOO weight untill its stabilized. 
    //Acaia tares almost instantly, even when not stable. Note this may cause the tare not to be zeroed properly when the weight is not stable.
    scale.tare();
    delay(50);
    scale.stopTimer();
    delay(50);
    scale.resetTimer();
    delay(50);
    scale.startTimer();
  }
}