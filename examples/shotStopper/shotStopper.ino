/*
  shotStopper.ino - Example of using an acaia scale to brew by weight with an espresso machine

  Immediately Connects to a nearby acaia scale, 
  tare's the scale when the "in" gpio is triggered (active low),
  and then triggers the "out" gpio to stop the shot once ( goalWeight - weightOffset ) is achieved.

  Tested on a Acaia Pyxis, Arduino nano ESP32, and La Marzocco GS3. 

  Note that only the EEPROM library only supports ESP32-based controllers.

  To set the Weight over BLE, use a BLE app such as LightBlue to connect
  to the "shotStopper" BLE device and read/write to the weight characteristic,
  otherwise the weight is defaulted to 36g.

  Created by Tate Mazer, 2023.

  Released under the MIT license.

  https://github.com/tatemazer/AcaiaArduinoBLE

*/

#include <AcaiaArduinoBLE.h>
#include <EEPROM.h>

#define MAX_OFFSET 5                // In case an error in brewing occured
#define MIN_SHOT_DURATION_MS 3000   //Useful for flushing the group.
                                    // This ensure that the system will ignore
                                    // "shots" that last less than this duration
#define MAX_SHOT_DURATION_MS 50000  //Primarily useful for latching switches, since user
                                    // looses control of the paddle once the system
                                    // latches.
#define BUTTON_READ_PERIOD_MS 30

#define EEPROM_SIZE 1  // This is 1-Byte
#define WEIGHT_ADDR 0  // Use the first byte of EEPROM to store the goal weight

//User defined***
#define MOMENTARY false        //Define brew switch style. 
                              // True for momentary switches such as GS3 AV, Silvia Pro
                              // false for latching switches such as Linea Mini/Micra

float weightOffset = -1.5;    //Weight to stop shot.  
                              // Will change during runtime in 
                              // response to observed error
//***************

AcaiaArduinoBLE scale;
float currentWeight = 0;
uint8_t goalWeight = 0;      // Goal Weight to be read from EEPROM
float error = 0;
int buttonArr[4];            // last 4 readings of the button

// button 
int in = 9;
int out = A0;
int outOff = A1;
bool buttonPressed = false; //physical status of button
unsigned long lastButtonRead_ms = 0;
int newButtonState = 0;

bool brewing = false;
unsigned long shotStart_ms = 0;
unsigned long shotEnd_ms = 0;

//BLE peripheral device
BLEService weightService("00002a98-0000-1000-8000-00805f9b34fb"); // create service
BLEByteCharacteristic weightCharacteristic("0x2A98",  BLEWrite | BLERead);

void setup() {
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);

  // Get stored setpoint
  goalWeight = EEPROM.read(WEIGHT_ADDR);
  Serial.print("Goal Weight retrieved: ");
  Serial.println(goalWeight);

  //If eeprom isn't initialized and has an 
  // unreasonable weight, default to 36g.
  if( (goalWeight < 10) || (goalWeight > 200) ){
    goalWeight = 36;
    Serial.print("Goal Weight set to: ");
    Serial.println(goalWeight);
  }
  
  // initialize the GPIO hardware
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(in, INPUT_PULLUP);
  pinMode(out, OUTPUT);
  pinMode(outOff, OUTPUT);

  // initialize the BLE hardware
  BLE.begin();
  BLE.setLocalName("shotStopper");
  BLE.setAdvertisedService(weightService);
  weightService.addCharacteristic(weightCharacteristic);
  BLE.addService(weightService);
  weightCharacteristic.writeValue(goalWeight);
  BLE.advertise();
  Serial.println("Bluetooth® device active, waiting for connections...");
}

void loop() {

  // Connect to scale
  while(!scale.isConnected()){
    scale.init(); 
  }

  // Check for setpoint updates
  BLE.poll();
  if (weightCharacteristic.written()) {
    Serial.print("goal weight updated from ");
    Serial.print(goalWeight);
    Serial.print(" to ");
    goalWeight = weightCharacteristic.value();
    Serial.println(goalWeight);
    EEPROM.write(WEIGHT_ADDR, goalWeight); //1 byte, 0-255
    EEPROM.commit();
  }

  // Send a heartbeat message to the scale periodically to maintain connection
  if(scale.heartbeatRequired()){
    scale.heartbeat();
  }

  // Reset local weight value in case disconnected. 
  if(!scale.isConnected()){
    currentWeight = 0;
  }

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if(scale.newWeightAvailable()){
    currentWeight = scale.getWeight();
    Serial.println(currentWeight);
  }

  // Read button every period
  if(millis() > (lastButtonRead_ms + BUTTON_READ_PERIOD_MS) ){
    lastButtonRead_ms = millis();

    //push back for new entry
    for(int i = 2;i>=0;i--){
      buttonArr[i+1] = buttonArr[i];
    }
    buttonArr[0] = !digitalRead(in); //Active Low

    //only return 1 if contains 1
    newButtonState = false;
    for(int i=0;i<4;i++){
      if(buttonArr[i]){
        newButtonState = true;
        
      }
      //Serial.print(buttonArr[i]);
    }
    //Serial.println();
  }
  
  //button just  pressed
  if(newButtonState && buttonPressed == false ){
    Serial.println("ButtonPressed");
    buttonPressed = true;
    if(!MOMENTARY){
      brewing = true;
      setBrewingState(brewing);
    }
  }

  //button released
  else if( !newButtonState 
  && buttonPressed == true 
  ){
    Serial.println("Button Released");
    buttonPressed = false;
    brewing = !brewing;
    setBrewingState(brewing);
  }
    
  //Max duration reached
  else if(brewing && millis() > (shotStart_ms + MAX_SHOT_DURATION_MS) ){
    brewing = false;
    Serial.println("Max brew duration reached");
    setBrewingState(brewing);
  }

  //Blink LED while brewing
  if(brewing){
    digitalWrite(LED_BUILTIN, (millis()/1000) % 2 == 0);
  }else{
    digitalWrite(LED_BUILTIN, LOW);
  }

  //End shot
  if(brewing 
  && currentWeight >= (goalWeight + weightOffset)
  && millis() > (shotStart_ms + MIN_SHOT_DURATION_MS) 
  ){
    Serial.println("weight achieved");
    brewing = false;
    setBrewingState(brewing); 
  }

  //Detect error of shot
  if(shotStart_ms 
  && shotEnd_ms 
  && currentWeight >= (goalWeight + weightOffset)
  && millis() > (shotEnd_ms + 3000) ){
    shotStart_ms = 0;
    

    Serial.print("I detected a final weight of ");
    Serial.print(currentWeight);
    Serial.print("g. The goal was ");
    Serial.print(goalWeight);
    Serial.print("g with an offset of ");
    Serial.print(weightOffset);

    if( abs(goalWeight - currentWeight) > MAX_OFFSET ){
      Serial.print("g. Error assumed. Offset unchanged. ");
    }
    else{
      Serial.print("g. Next time I'll create an offset of ");
      weightOffset += goalWeight - currentWeight;
      Serial.print(weightOffset);
    }
    Serial.println();
  }

  //Re-enable switch after 10 seconds
  if(shotEnd_ms 
  && millis() > (shotEnd_ms + 10000) ){
    shotEnd_ms = 0;
    digitalWrite(outOff,LOW);
  }
}

void setBrewingState(bool brewing){
  if(brewing){
    Serial.println("shot started");
    shotStart_ms = millis();
    scale.startTimer();
    scale.tare();
    
  }else{
    Serial.println("ShotEnded");
    shotEnd_ms = millis();
    scale.stopTimer();
    if(MOMENTARY){
      //Pulse button to stop brewing
      digitalWrite(out,HIGH);Serial.println("wrote high");
      delay(300);
      digitalWrite(out,LOW);Serial.println("wrote low");
    }else{
      buttonPressed = false;
      Serial.println("Button Unlatched and not pressed");
      digitalWrite(outOff,HIGH); Serial.println("activated relay");
    }
  } 
}