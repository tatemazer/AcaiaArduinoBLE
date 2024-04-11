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
#define MIN_SHOT_DURATION_S 3       //Useful for flushing the group.
                                    // This ensure that the system will ignore
                                    // "shots" that last less than this duration
#define MAX_SHOT_DURATION_S 50      //Primarily useful for latching switches, since user
                                    // looses control of the paddle once the system
                                    // latches.
#define BUTTON_READ_PERIOD_MS 30
#define DRIP_DELAY_S          3     // Time after the shot ended to measure the final weight

#define EEPROM_SIZE 2  // This is 1-Byte
#define WEIGHT_ADDR 0  // Use the first byte of EEPROM to store the goal weight
#define OFFSET_ADDR 1  

#define N 10                        // Number of datapoints used to calculate trend line

//User defined***
#define MOMENTARY false        //Define brew switch style. 
                              // True for momentary switches such as GS3 AV, Silvia Pro
                              // false for latching switches such as Linea Mini/Micra
#define REEDSWITCH false      // Set to true if the brew state is being determined 
                              //  by a reed switch attached to the brew solenoid
//***************

AcaiaArduinoBLE scale;
float currentWeight = 0;
uint8_t goalWeight = 0;      // Goal Weight to be read from EEPROM
float weightOffset = 0;
float error = 0;
int buttonArr[4];            // last 4 readings of the button

// button 
int in = REEDSWITCH ? 9 : 10;
int out = 11;
bool buttonPressed = false; //physical status of button
bool buttonLatched = false; //electrical status of button
unsigned long lastButtonRead_ms = 0;
int newButtonState = 0;

struct Shot {
  float start_timestamp_s; // Relative to runtime
  float shotTimer;         // Reset when the final drip measurement is made
  float end_s;             // Number of seconds after the shot started
  float expected_end_s;    // Estimated duration of the shot
  float weight[1000];      // A scatter plot of the weight measurements, along with time_s[]
  float time_s[1000];      // Number of seconds after the shot starte
  int datapoints;          // Number of datapoitns in the scatter plot
  bool brewing;            // True when actively brewing, otherwise false
};

//Initialize shot
Shot shot = {0,0,0,0,{},{},0,false};

//BLE peripheral device
BLEService weightService("00002a98-0000-1000-8000-00805f9b34fb"); // create service
BLEByteCharacteristic weightCharacteristic("0x2A98",  BLEWrite | BLERead);

void setup() {
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);

  // Get stored setpoint and offset
  goalWeight = EEPROM.read(WEIGHT_ADDR);
  weightOffset = EEPROM.read(OFFSET_ADDR)/10.0;
  Serial.print("Goal Weight retrieved: ");
  Serial.println(goalWeight);
  Serial.print("offset retrieved: ");
  Serial.println(goalWeight);

  //If eeprom isn't initialized and has an 
  // unreasonable weight/offset, default to 36g/1.5g
  if( (goalWeight < 10) || (goalWeight > 200) ){
    goalWeight = 36;
    Serial.print("Goal Weight set to: ");
    Serial.println(goalWeight);
  }
  if(weightOffset > MAX_OFFSET){
    weightOffset = 1.5;
    Serial.print("Offset set to: ");
    Serial.println(weightOffset);
  }
  
  // initialize the GPIO hardware
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(in, INPUT_PULLUP);
  pinMode(out, OUTPUT);

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
    currentWeight = 0;
    if(shot.brewing){
      setBrewingState(false);
    }
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

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if(scale.newWeightAvailable()){
    currentWeight = scale.getWeight();

    Serial.print(currentWeight);

    // update shot trajectory
    if(shot.brewing){
      shot.time_s[shot.datapoints] = seconds_f()-shot.start_timestamp_s;
      shot.weight[shot.datapoints] = currentWeight;
      shot.shotTimer = shot.time_s[shot.datapoints];
      shot.datapoints++;

      Serial.print(" ");
      Serial.print(shot.shotTimer);

      //get the likely end time of the shot
      calculateEndTime(&shot);
      Serial.print(" ");
      Serial.print(shot.expected_end_s);
    }
    Serial.println();
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
    // Also assume the button is off for a few milliseconds
    // after the shot is done, there can be residual noise
    // from the reed switch
    newButtonState = 0;
    for(int i=0; i<4; i++){
      if(buttonArr[i]){
        newButtonState = 1;          
      }
      //Serial.print(buttonArr[i]);
    }
    //Serial.println();
    if(REEDSWITCH && !shot.brewing && seconds_f() < (shot.start_timestamp_s + shot.end_s + 0.5)){
      newButtonState = 0;
    }
  }
  
  //button just pressed
  if(newButtonState && buttonPressed == false ){
    Serial.println("ButtonPressed");
    buttonPressed = true;
    if(!MOMENTARY){
      shot.brewing = true;
      setBrewingState(shot.brewing);
    }
  }
    
  // button held. Take over for the rest of the shot.
  else if(!MOMENTARY 
  && shot.brewing 
  && !buttonLatched 
  && (shot.shotTimer > MIN_SHOT_DURATION_S) 
  ){
    buttonLatched = true;
    Serial.println("Button Latched");
    digitalWrite(out,HIGH); Serial.println("wrote high");
    // Get the scale to beep to inform user.
    scale.tare();
  }

  //button released
  else if(!buttonLatched 
  && !newButtonState 
  && buttonPressed == true 
  ){
    Serial.println("Button Released");
    buttonPressed = false;
    shot.brewing = !shot.brewing;
    setBrewingState(shot.brewing);
  }
    
  //Max duration reached
  else if(shot.brewing && shot.shotTimer > MAX_SHOT_DURATION_S ){
    shot.brewing = false;
    Serial.println("Max brew duration reached");
    setBrewingState(shot.brewing);
  }

  //Blink LED while brewing
  if(shot.brewing){
    digitalWrite(LED_BUILTIN, (millis()/1000) % 2 == 0);
  }else{
    digitalWrite(LED_BUILTIN, LOW);
  }

  //End shot
  if(shot.brewing 
  && shot.shotTimer >= shot.expected_end_s
  && shot.shotTimer >  MIN_SHOT_DURATION_S
  ){
    Serial.println("weight achieved");
    shot.brewing = false;
    setBrewingState(shot.brewing); 
  }

  //Detect error of shot
  if(shot.start_timestamp_s
  && shot.end_s
  && currentWeight >= (goalWeight - weightOffset)
  && seconds_f() > shot.start_timestamp_s + shot.end_s + DRIP_DELAY_S){
    shot.start_timestamp_s = 0;
    shot.end_s = 0;

    Serial.print("I detected a final weight of ");
    Serial.print(currentWeight);
    Serial.print("g. The goal was ");
    Serial.print(goalWeight);
    Serial.print("g with a negative offset of ");
    Serial.print(weightOffset);

    if( abs(currentWeight - goalWeight + weightOffset) > MAX_OFFSET ){
      Serial.print("g. Error assumed. Offset unchanged. ");
    }
    else{
      Serial.print("g. Next time I'll create an offset of ");
      weightOffset += currentWeight - goalWeight;
      Serial.print(weightOffset);

      EEPROM.write(OFFSET_ADDR, weightOffset*10); //1 byte, 0-255
      EEPROM.commit();
    }
    Serial.println();
  }
}

void setBrewingState(bool brewing){
  if(brewing){
    Serial.println("shot started");
    shot.start_timestamp_s = seconds_f();
    shot.shotTimer = 0;
    shot.datapoints = 0;
    scale.startTimer();
    scale.tare();
    Serial.println("Weight Timer End");
  }else{
    Serial.println("ShotEnded");
    shot.end_s = seconds_f() - shot.start_timestamp_s;
    scale.stopTimer();
    if(MOMENTARY){
      //Pulse button to stop brewing
      digitalWrite(out,HIGH);Serial.println("wrote high");
      delay(300);
      digitalWrite(out,LOW);Serial.println("wrote low");
    }else{
      buttonLatched = false;
      buttonPressed = false;
      Serial.println("Button Unlatched and not pressed");
      digitalWrite(out,LOW); Serial.println("wrote low");
    }
  } 
}
void calculateEndTime(Shot* s){
  
  // Do not  predict end time if there aren't enough espresso measurements yet
  if( (s->datapoints < N) || (s->weight[s->datapoints-1] < 10) ){
    s->expected_end_s = MAX_SHOT_DURATION_S;
  }
  else{
    //Get line of best fit (y=mx+b) from the last 10 measurements 
    float sumXY = 0, sumX = 0, sumY = 0, sumSquaredX = 0, m = 0, b = 0, meanX = 0, meanY = 0;

    for(int i = s->datapoints - N; i < s->datapoints; i++){
      sumXY+=s->time_s[i]*s->weight[i];
      sumX+=s->time_s[i];
      sumY+=s->weight[i];
      sumSquaredX += ( s->time_s[i] * s->time_s[i] );
    }

    m = (N*sumXY-sumX*sumY) / (N*sumSquaredX-(sumX*sumX));
    meanX = sumX/N;
    meanY = sumY/N;
    b = meanY-m*meanX;

    //Calculate time at which goal weight will be reached (x = (y-b)/m)
    s->expected_end_s = (goalWeight - weightOffset - b)/m; 
  }
}

float seconds_f(){
  return millis()/1000.0;
}