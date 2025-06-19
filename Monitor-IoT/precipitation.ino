float readPrecip(float timesSwitchActivated, int computeTime, float precipRatio){
  float currentRead = -1;
  float n = 1.0 * ((millis() - computeTime));
  currentRead = ((timesSwitchActivated * 1000) / n ) * precipRatio;   // Time between measurements can vary
  return currentRead;
}
