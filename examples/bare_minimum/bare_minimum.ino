#include <AcaiaArduinoBLE.h>


AcaiaArduinoBLE acaia;
void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("Scale Interface test");
  // Uncomment any of the following lines to connect to your scale, optionally add your Mac Address as an argument:

  //acaia.init("ACAIA","##:##:##:##:##:##");
  //acaia.init("FELICITA_ARC","##:##:##:##:##:##");
  acaia.init("FELICITA_ARC");
  //acaia.init("ACAIA");

  acaia.tare();
}

void loop() {
  // Send a heartbeat message to the acaia periodically to maintain connection
  if (acaia.heartbeatRequired()) {
    acaia.heartbeat();
  }

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if (acaia.newWeightAvailable()) {
    Serial.println(acaia.getWeight());
  }
}
