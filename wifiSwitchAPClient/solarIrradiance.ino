float readSolarIrr(int adcPin, float solarIrrRatio){
  int v = analogReadMilliVolts(adcPin);
  Serial.print("voltage read: ");
  Serial.println(v);
  return v*solarIrrRatio;
}