float readSolarIrr(int adcPin, float solarIrrRatio){
  int v = analogReadMilliVolts(adcPin);
  // Serial.print("voltage read: ");
  // Serial.println(v);
  return v*solarIrrRatio;
}

void saveSolarIrr(fs::FS& fs, const char* path, unsigned long epochTime, int adcPin, float solarIrrRatio){
  char buffer[30] = "";
  appendFileSD(fs, path, "solar");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readSolarIrr(adcPin, solarIrrRatio));
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}