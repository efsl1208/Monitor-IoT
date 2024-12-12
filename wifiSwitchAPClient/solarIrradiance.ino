float readSolarIrr(int adcPin, float solarIrrRatio){
  int v = analogReadMilliVolts(adcPin);
  Serial.print("voltage read: ");
  Serial.println(v);
  return v*solarIrrRatio;
}

void saveSolarIrr(fs::FS& fs, const char* path, unsigned long epochTime, int adcPin, float solarIrrRatio){
  appendFileSD(fs, path, "solar");
  appendFileSD(fs, path, ",");
  appendFileSD(fs, path, readSolarIrr(adcPin, solarIrrRatio));
  appendFileSD(fs, path, ",");
  appendFileSD(fs, path, epochTime);
  appendFileSD(fs, path, "\n");
}