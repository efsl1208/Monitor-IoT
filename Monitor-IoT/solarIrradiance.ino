float readSolarIrr(int adcPin, float solarIrrRatio){
  int v = analogReadMilliVolts(adcPin);
  return v*solarIrrRatio;
}

float readPower(int adcPin, float ratio) {
  int v = 0;
  for (int i = 0; i < 5; i++) {
    v = v + analogReadMilliVolts(adcPin);
    delay(10);
  }
  v = v / 5;
  return ( (v - 142) * (v - 142) * 9 / (130) ) * ratio ;
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