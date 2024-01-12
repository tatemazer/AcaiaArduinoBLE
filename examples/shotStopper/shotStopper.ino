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

#define MAX_OFFSET 5          // In case an error in brewing occured

//User defined***
#define END_WEIGHT 50         //Goal Weight
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
bool buttonPressed = false;
unsigned long lastButtonRead = 0;
unsigned long buttonReadPeriod_ms = 25;
int lastButtonState = 1;

bool brewing = false;
unsigned long shotStart_ms = 0;
unsigned long shotEnd_ms = 0;

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);

  // make the pushbutton's pin an input:
  pinMode(in, INPUT_PULLUP);
  pinMode(out, OUTPUT);

  acaia.init(); 
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
    Serial.println(acaia.getWeight());
  }

  // Read button every ButtonReadPeriod_ms
  if(millis() > (lastButtonRead + buttonReadPeriod_ms) ){
    lastButtonRead = millis();
    lastButtonState = digitalRead(in);

    //button pressed
    if( !lastButtonState && buttonPressed == false ){
      Serial.println("ButtonPressed");
      buttonPressed = true;
    }

    //button released
    else if(lastButtonState &&  buttonPressed == true ){
      Serial.println("ButtonReleased");
      buttonPressed = false;
      brewing = !brewing;
      if(brewing){
        Serial.println("shot started");
        shotStart_ms = millis();
        acaia.tare();
      }
    }
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
  && millis() > (shotStart_ms + 3000) 
  ){
    brewing = false;
    Serial.println("ShotEnded");
    shotEnd_ms = millis();
    digitalWrite(out,HIGH);
    delay(300);
    digitalWrite(out,LOW);
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