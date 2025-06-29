float readAirflow(float timesSwitchActivated, int computeTime, float airflowRatio){
  float currentRead = -1;
  float n = 1.0 * ((millis() - computeTime));
  currentRead = ((timesSwitchActivated * 1000) / n ) * airflowRatio;   // Time between measurements can vary
  return currentRead;
}
