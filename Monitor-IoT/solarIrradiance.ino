float readSolarIrr(int adcPin, float solarIrrRatio){
  int v = analogReadMilliVolts(adcPin);
  return v*solarIrrRatio;
}

float readPower(int adcPin, float ratio) {
  int v = analogReadMilliVolts(adcPin);
  return ( (v - 142) * (v - 142) / (1.5 * 1000) ) * ratio ;
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