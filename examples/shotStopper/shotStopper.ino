/*
  shotStopper.ino - Example of using an acaia scale to brew by weight with an espresso machine

  Immediately Connects to a nearby acaia scale, 
  tare's the scale when the "in" gpio is triggered (active low),
  and then triggers the "out" gpio to stop the shot once ( END_WEIGHT - WEIGHT OFFSET ) is achieved.

  Tested on a Acaia Pyxis, Arduino nano ESP32, and La Marzocco GS3

  Created by Tate Mazer, 2023.

  Released under the MIT license.

  https://github.com/tatemazer/AcaiaArduinoBLE

*/

#include <AcaiaArduinoBLE.h>

#define MAX_OFFSET 5                // In case an error in brewing occured
#define MIN_SHOT_DURATION_MS 3000   //Useful for flushing the group.
                                    // This ensure that the system will ignore
                                    // "shots" that last less than this duration
#define MAX_SHOT_DURATION_MS 50000  //Primarily useful for latching switches, since user
                                    // looses control of the paddle once the system
                                    // latches.
#define BUTTON_READ_PERIOD_MS 25

//User defined***
#define END_WEIGHT 50         //Goal Weight
#define MOMENTARY true        //Define brew switch style. 
                              // True for momentary switches such as GS3 AV, Silvia Pro
                              // false for latching switches such as Linea Mini/Micra
float weightOffset = -1.5;    //Weight to stop shot.  
                              // Will change during runtime in 
                              // response to observed error
//***************

AcaiaArduinoBLE acaia;
float currentWeight = 0;
float error = 0;

// button 
int in = 10;
int out = 11;
bool buttonPressed = false; //physical status of button
bool buttonLatched = false; //electrical status of button
unsigned long lastButtonRead_ms = 0;
int newButtonState = 0;

bool brewing = false;
unsigned long shotStart_ms = 0;
unsigned long shotEnd_ms = 0;

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);

  // make the pushbutton's pin an input:
  pinMode(in, INPUT_PULLUP);
  pinMode(out, OUTPUT);

  // Uncomment any of the following lines to connect to your scale, optionally add your Mac Address as an argument:

  //acaia.init("ACAIA","##:##:##:##:##:##");
  //acaia.init("FELICITA_ARC","##:##:##:##:##:##");
  acaia.init("FELICITA_ARC");
  //acaia.init("ACAIA");
  
}

void loop() {

  // Send a heartbeat message to the acaia periodically to maintain connection
  if(acaia.heartbeatRequired()){
    acaia.heartbeat();
  }

  // Reset local weight value in case disconnected. 
  if(!acaia.isConnected()){
    currentWeight = 0;
  }

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if(acaia.newWeightAvailable()){
    currentWeight = acaia.getWeight();
    Serial.println(currentWeight);
  }

  // Read button every period
  if(millis() > (lastButtonRead_ms + BUTTON_READ_PERIOD_MS) ){
    lastButtonRead_ms = millis();
    newButtonState = !digitalRead(in); //Active Low
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
    
  // button held. Take over for the rest of the shot.
  else if(!MOMENTARY 
  && brewing 
  && !buttonLatched 
  && millis() > (shotStart_ms + MIN_SHOT_DURATION_MS) 
  ){
    buttonLatched = true;
    Serial.println("Button Latched");
    digitalWrite(out,HIGH); Serial.println("wrote high");
    // Get the acaia to beep to inform user.
    acaia.tare();
  }

  //button released
  else if(!buttonLatched 
  && !newButtonState 
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
  && currentWeight >= (END_WEIGHT + weightOffset)
  && millis() > (shotStart_ms + MIN_SHOT_DURATION_MS) 
  ){
    Serial.println("weight achieved");
    brewing = false;
    
    setBrewingState(brewing); 
  }

  //Detect error of shot
  if(shotStart_ms 
  && shotEnd_ms 
  && currentWeight >= (END_WEIGHT + weightOffset)
  && millis() > (shotEnd_ms + 3000) ){
    shotStart_ms = 0;
    shotEnd_ms = 0;

    Serial.print("I detected a final weight of ");
    Serial.print(currentWeight);
    Serial.print("g. The goal was ");
    Serial.print(END_WEIGHT);
    Serial.print("g with an offset of ");
    Serial.print(weightOffset);

    if( abs(END_WEIGHT - currentWeight) > MAX_OFFSET ){
      Serial.print("g. Error assumed. Offset unchanged. ");
    }
    else{
      Serial.print("g. Next time I'll create an offset of ");
      weightOffset += END_WEIGHT - currentWeight;
      Serial.print(weightOffset);
    }
    Serial.println();

  }
}

void setBrewingState(bool brewing){
  if(brewing){
    Serial.println("shot started");
    shotStart_ms = millis();
    acaia.tare();
    
  }else{
    Serial.println("ShotEnded");
    shotEnd_ms = millis();
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
