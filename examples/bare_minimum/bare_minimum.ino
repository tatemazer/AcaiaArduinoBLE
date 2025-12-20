#include <AcaiaArduinoBLE.h>

#define DEBUG false

AcaiaArduinoBLE scale(DEBUG);
void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("Scale Interface test");

  // initialize the Bluetooth® Low Energy hardware
  // Optionally add your Mac Address as an argument: acaia.init("##:##:##:##:##:##");
  scale.init();
  scale.updateConnection();
  scale.tare();
}

void loop() {

  scale.updateConnection();

  // Send a heartbeat message to the acaia periodically to maintain connection
  if (scale.heartbeatRequired()) {
    scale.heartbeat();
  }

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if (scale.newWeightAvailable()) {
    Serial.println(scale.getWeight());
  }
}
