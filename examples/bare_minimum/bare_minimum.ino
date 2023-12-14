#include <AcaiaArduinoBLE.h>


//"34:81:f4:53:1e:83"

AcaiaArduinoBLE acaia;
void setup() {
  // Optionally add your Mac Address as an argument: acaia.init("##:##:##:##:##:##");
  acaia.init(); 
}

void loop() {

  // Send a heartbeat message to the acaia periodically to maintain connection
  if(acaia.heartbeatRequired()){
    acaia.heartbeat();
  }

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if(acaia.newWeightAvailable()){
    Serial.println(acaia.getWeight());
  }
}
