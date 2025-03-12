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

#define FIRMWARE_VERSION 1
#define MAX_OFFSET 5                // In case an error in brewing occured
#define BUTTON_READ_PERIOD_MS 5

#define EEPROM_SIZE 9
#define SIGNATURE_ADDR 0 // Use the first byte to store a magic number/signature to know if the memory has been initialized
#define WEIGHT_ADDR 1 // Use the second byte of EEPROM to store the goal weight
#define OFFSET_ADDR 2 
#define MOMENTARY_ADDR 3 
#define REEDSWITCH_ADDR 4 
#define AUTOTARE_ADDR 5 
#define MIN_SHOT_DURATION_S_ADDR 6
#define MAX_SHOT_DURATION_S_ADDR 7 
#define DRIP_DELAY_S_ADDR 8
#define SIGNATURE_VALUE 0xAA

#define DEBUG false

#define N 10                        // Number of datapoints used to calculate trend line

// Board Hardware 
#ifdef ARDUINO_ESP32S3_DEV
  #define LED_RED     46
  #define LED_BLUE    45
  #define LED_GREEN   47
  #define LED_BUILTIN 48
  #define IN          21
  #define OUT         38
  #define REED_IN     18
#else //todo: find nano esp32 identifier
  //LED's are defined by framework
  #define IN          10
  #define OUT         11
  #define REED_IN     9
#endif 

#define BUTTON_STATE_ARRAY_LENGTH 31

// configuration variables
bool reedSwitch = false;            // Set to true if the brew state is being determined 
                                    // by a reed switch attached to the brew solenoid
bool momentary = false;             // Define brew switch style. 
                                    // True for momentary switches such as GS3 AV, Silvia Pro
                                    // false for latching switches such as Linea Mini/Micra
bool autoTare = true;               // Automatically tare when shot is started 
                                    // and 3 seconds after a latching switch brew 
                                    // (as defined by MOMENTARY)
uint8_t minShotDurationS = 3;       // Useful for flushing the group.
                                    // This ensure that the system will ignore
                                    // "shots" that last less than this duration
uint8_t maxShotDurationS = 50;      // Primarily useful for latching switches, since user
                                    // looses control of the paddle once the system
                                    // latches.
uint8_t dripDelayS = 3;             // Time after the shot ended to measure the final weight

typedef enum {BUTTON, WEIGHT, TIME, UNDEF} ENDTYPE;

// RGB Colors {Red,Green,Blue}
int RED[3] = {255, 0, 0};
int GREEN[3] = {0, 255, 0};
int BLUE[3] = {0, 0, 255};
int MAGENTA[3] = {255, 0, 255};
int CYAN[3] = {0, 255, 255};
int YELLOW[3] = {255, 255, 0};
int WHITE[3] = {255, 255, 255};
int OFF[3] = {0,0,0};
int currentColor[3] = {0,0,0};

AcaiaArduinoBLE scale(DEBUG);
float currentWeight = 0;
uint8_t goalWeight = 0;      // Goal Weight to be read from EEPROM
float weightOffset = 0;
float error = 0;
int buttonArr[BUTTON_STATE_ARRAY_LENGTH];            // last 4 readings of the button

// button 
int in = reedSwitch ? REED_IN : IN;
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
  ENDTYPE end;
};

//Initialize shot
Shot shot = {0,0,0,0,{},{},0,false,ENDTYPE::UNDEF};

//BLE peripheral device
BLEService shotStopperService("0x0FFE"); // create service
BLEByteCharacteristic weightCharacteristic("0xFF11",  BLEWrite | BLERead);
BLEByteCharacteristic reedSwitchCharacteristic("0xFF12",  BLEWrite | BLERead);
BLEByteCharacteristic momentaryCharacteristic("0xFF13",  BLEWrite | BLERead);
BLEByteCharacteristic autoTareCharacteristic("0xFF14",  BLEWrite | BLERead);
BLEByteCharacteristic minShotDurationSCharacteristic("0xFF15",  BLEWrite | BLERead);
BLEByteCharacteristic maxShotDurationSCharacteristic("0xFF16",  BLEWrite | BLERead);
BLEByteCharacteristic dripDelaySCharacteristic("0xFF17",  BLEWrite | BLERead);
BLEByteCharacteristic firmwareVersionCharacteristic("0xFF18",  BLERead);
BLEByteCharacteristic scaleStatusCharacteristic("0xFF19",  BLERead | BLENotify);

enum ScaleStatus {
  STATUS_DISCONNECTED = 0,
  STATUS_CONNECTED = 1,
};

uint8_t lastScaleStatus = 255; // Invalid initial value to force first update

void updateScaleStatus(uint8_t newScaleStatus) {
  if (newScaleStatus != lastScaleStatus) {
    scaleStatusCharacteristic.writeValue(newScaleStatus);
    lastScaleStatus = newScaleStatus;
  }
}

void loadOrInitEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(SIGNATURE_ADDR) != SIGNATURE_VALUE) {
    goalWeight = 36;
    weightOffset = 1.5;
    EEPROM.write(SIGNATURE_ADDR, SIGNATURE_VALUE);
    EEPROM.write(WEIGHT_ADDR, goalWeight);
    EEPROM.write(OFFSET_ADDR, (uint8_t)(weightOffset * 10));
    EEPROM.write(MOMENTARY_ADDR, momentary ? 1 : 0);
    EEPROM.write(REEDSWITCH_ADDR, reedSwitch ? 1 : 0);
    EEPROM.write(AUTOTARE_ADDR, autoTare ? 1 : 0);
    EEPROM.write(MIN_SHOT_DURATION_S_ADDR, minShotDurationS);
    EEPROM.write(MAX_SHOT_DURATION_S_ADDR, maxShotDurationS);
    EEPROM.write(DRIP_DELAY_S_ADDR, dripDelayS);
    EEPROM.commit();
    Serial.println("EEPROM initialized with defaults");
  } else {
    goalWeight = EEPROM.read(WEIGHT_ADDR);
    weightOffset = EEPROM.read(OFFSET_ADDR) / 10.0;
    Serial.print("Goal Weight retrieved: ");
    Serial.println(goalWeight);
    Serial.print("Offset retrieved: ");
    Serial.println(weightOffset);
    momentary = EEPROM.read(MOMENTARY_ADDR) != 0;
    reedSwitch = EEPROM.read(REEDSWITCH_ADDR) != 0;
    in = reedSwitch ? REED_IN : IN;
    autoTare = EEPROM.read(AUTOTARE_ADDR) != 0;
    minShotDurationS = EEPROM.read(MIN_SHOT_DURATION_S_ADDR);
    maxShotDurationS = EEPROM.read(MAX_SHOT_DURATION_S_ADDR);
    dripDelayS = EEPROM.read(DRIP_DELAY_S_ADDR);
  }
}

void initializeBLE() {
  BLE.begin();
  BLE.setLocalName("shotStopper");
  BLE.setAdvertisedService(shotStopperService);
  shotStopperService.addCharacteristic(weightCharacteristic);
  shotStopperService.addCharacteristic(momentaryCharacteristic);
  shotStopperService.addCharacteristic(reedSwitchCharacteristic);
  shotStopperService.addCharacteristic(autoTareCharacteristic);
  shotStopperService.addCharacteristic(minShotDurationSCharacteristic);
  shotStopperService.addCharacteristic(maxShotDurationSCharacteristic);
  shotStopperService.addCharacteristic(dripDelaySCharacteristic);
  shotStopperService.addCharacteristic(firmwareVersionCharacteristic);
  shotStopperService.addCharacteristic(scaleStatusCharacteristic);
  BLE.addService(shotStopperService);
  weightCharacteristic.writeValue(goalWeight);
  momentaryCharacteristic.writeValue(momentary ? 1 : 0);
  reedSwitchCharacteristic.writeValue(reedSwitch ? 1 : 0);
  autoTareCharacteristic.writeValue(autoTare ? 1 : 0);
  minShotDurationSCharacteristic.writeValue(minShotDurationS);
  maxShotDurationSCharacteristic.writeValue(maxShotDurationS);
  dripDelaySCharacteristic.writeValue(dripDelayS);
  firmwareVersionCharacteristic.writeValue(FIRMWARE_VERSION);
  scaleStatusCharacteristic.writeValue(STATUS_DISCONNECTED);
  BLE.advertise();
  Serial.println("Bluetooth® device active, waiting for connections...");
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  BLE.advertise();
}

void pollAndReadBLE() {
  bool updated = false;
  BLE.poll();
  if (weightCharacteristic.written()) {
    Serial.print("goal weight updated from ");
    Serial.print(goalWeight);
    Serial.print(" to ");
    goalWeight = weightCharacteristic.value();
    Serial.println(goalWeight);
    EEPROM.write(WEIGHT_ADDR, goalWeight); //1 byte, 0-255
    updated = true;
    EEPROM.commit();
  }
  if (momentaryCharacteristic.written()) {
    momentary = momentaryCharacteristic.value() != 0;
    EEPROM.write(MOMENTARY_ADDR, momentary ? 1 : 0);
    updated = true;
  }
  if (reedSwitchCharacteristic.written()) {
    reedSwitch = reedSwitchCharacteristic.value() != 0;
    in = reedSwitch ? REED_IN : IN;
    EEPROM.write(REEDSWITCH_ADDR, reedSwitch ? 1 : 0);
    updated = true;
  }
  if (autoTareCharacteristic.written()) {
    autoTare = autoTareCharacteristic.value() != 0;
    EEPROM.write(AUTOTARE_ADDR, autoTare ? 1 : 0);
    updated = true;
  }
  if (minShotDurationSCharacteristic.written()) {
    minShotDurationS = minShotDurationSCharacteristic.value();
    EEPROM.write(MIN_SHOT_DURATION_S_ADDR, minShotDurationS);
    updated = true;
  }
  if (maxShotDurationSCharacteristic.written()) {
    maxShotDurationS = maxShotDurationSCharacteristic.value();
    EEPROM.write(MAX_SHOT_DURATION_S_ADDR, maxShotDurationS);
    updated = true;
  }
  if (dripDelaySCharacteristic.written()) {
    dripDelayS = dripDelaySCharacteristic.value();
    EEPROM.write(DRIP_DELAY_S_ADDR, dripDelayS);
    updated = true;
  }
  if (updated) {
    EEPROM.commit();
  }
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(9600);

  // If eeprom isn't initialized default to 36g/1.5g
  loadOrInitEEPROM();

  // initialize the GPIO hardware
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(in, INPUT_PULLUP);
  pinMode(OUT, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setColor(OFF);

  // initialize the BLE hardware
  initializeBLE();
}



void loop() {
  // Check for setpoint updates
  pollAndReadBLE();

  // Connect to scale
  if (!scale.isConnected()) {
    updateScaleStatus(STATUS_DISCONNECTED);
    setColor(RED);
    scale.init(); 
    currentWeight = 0;
    if(shot.brewing){
      setBrewingState(false);
    }
    if(scale.isConnected()){
      setColor(YELLOW);
      updateScaleStatus(STATUS_CONNECTED);
    } else {
      return;
    }
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

    if(!shot.brewing){
      setColor(GREEN);
    }

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
    for(int i = BUTTON_STATE_ARRAY_LENGTH - 2;i>=0;i--){
      buttonArr[i+1] = buttonArr[i];
    }
    buttonArr[0] = !digitalRead(in); //Active Low

    //only return 1 if contains 1
    // Also assume the button is off for a few milliseconds
    // after the shot is done, there can be residual noise
    // from the reed switch
    newButtonState = 0;
    for(int i=0; i<BUTTON_STATE_ARRAY_LENGTH; i++){
      if(buttonArr[i]){
        newButtonState = 1;          
      }
      //Serial.print(buttonArr[i]);
    }
    //Serial.println();
    if(reedSwitch && !shot.brewing && seconds_f() < (shot.start_timestamp_s + shot.end_s + 0.5)){
      newButtonState = 0;
    }
  }
  
  //button just pressed
  if(newButtonState && buttonPressed == false ){
    Serial.println("ButtonPressed");
    buttonPressed = true;
    if(!momentary){
      shot.brewing = true;
      setBrewingState(shot.brewing);
    }
  }
    
  // button held. Take over for the rest of the shot.
  else if(!momentary 
  && shot.brewing 
  && !buttonLatched 
  && (shot.shotTimer > minShotDurationS) 
  ){
    buttonLatched = true;
    Serial.println("Button Latched");
    digitalWrite(OUT,HIGH); Serial.println("wrote high");
    // Get the scale to beep to inform user.
    if(autoTare){
      scale.tare();
    }
  }

  //button released
  else if(!buttonLatched 
  && !newButtonState 
  && buttonPressed == true 
  ){
    Serial.println("Button Released");
    buttonPressed = false;
    shot.brewing = !shot.brewing;
    if(!shot.brewing){
      shot.end = ENDTYPE::BUTTON;
    }
    setBrewingState(shot.brewing);
  }
    
  //Max duration reached
  else if(shot.brewing && shot.shotTimer > maxShotDurationS ){
    shot.brewing = false;
    Serial.println("Max brew duration reached");
    shot.end = ENDTYPE::TIME;
    setBrewingState(shot.brewing);
  }

  //Blink LED while brewing
  if(shot.brewing){
    setColor( (millis()/1000)%2 ? GREEN : BLUE );
  }

  //End shot
  if(shot.brewing 
  && shot.shotTimer >= shot.expected_end_s
  && shot.shotTimer >  minShotDurationS
  ){
    Serial.println("weight achieved");
    shot.brewing = false;
    shot.end = ENDTYPE::WEIGHT;
    setBrewingState(shot.brewing); 
  }

  //Detect error of shot
  if(shot.start_timestamp_s
  && shot.end_s
  && currentWeight >= (goalWeight - weightOffset)
  && seconds_f() > shot.start_timestamp_s + shot.end_s + dripDelayS){
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
    }else{
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
    scale.resetTimer();
    scale.startTimer();
    if(autoTare){
      scale.tare();
    }
    Serial.println("Weight Timer End");
  }else{
    Serial.print("ShotEnded by ");
    switch (shot.end) {
      case ENDTYPE::TIME:
      Serial.println("time");
      break;
    case ENDTYPE::WEIGHT:
    Serial.println("weight");
      break;
    case ENDTYPE::BUTTON:
    Serial.println("button");
      break;
    case ENDTYPE::UNDEF:
      Serial.println("undef");
      break;
    }

    shot.end_s = seconds_f() - shot.start_timestamp_s;
    scale.stopTimer();
    if(momentary &&
      (ENDTYPE::WEIGHT == shot.end || ENDTYPE::TIME == shot.end)){
      //Pulse button to stop brewing
      digitalWrite(OUT,HIGH);Serial.println("wrote high");
      delay(300);
      digitalWrite(OUT,LOW);Serial.println("wrote low");
    }else if(!momentary){
      buttonLatched = false;
      buttonPressed = false;
      Serial.println("Button Unlatched and not pressed");
      digitalWrite(OUT,LOW); Serial.println("wrote low");
    }
  } 

  // Reset
  shot.end = ENDTYPE::UNDEF;
}
void calculateEndTime(Shot* s){
  
  // Do not  predict end time if there aren't enough espresso measurements yet
  if( (s->datapoints < N) || (s->weight[s->datapoints-1] < 10) ){
    s->expected_end_s = maxShotDurationS;
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

void setColor(int rgb[3]){
  analogWrite(LED_RED,   255-rgb[0] );
  analogWrite(LED_GREEN, 255-rgb[1] );
  analogWrite(LED_BLUE,  255-rgb[2] );
  currentColor[0] = rgb[0];
  currentColor[1] = rgb[1];
  currentColor[2] = rgb[2];
}
