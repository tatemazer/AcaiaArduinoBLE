#include <AcaiaArduinoBLE.h>

AcaiaArduinoBLE acaia;
void setup() {
  Serial.begin(115200);
  while(!Serial){}
  Serial.println("Scale Interface test");
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
