float readPrecip(volatile int* timesSwitchActivated, int sampleTime, float precipRatio){
  float currentRead = -1;
  currentRead = (*timesSwitchActivated / (sampleTime / 1000)) * precipRatio;   // Sample time in ms
  *timesSwitchActivated = 0;
  // Serial.print("Precipitation read: ");
  // Serial.println(currentRead);
  return currentRead;
}

void savePrecip(fs::FS& fs, const char* path, unsigned long epochTime, int* timesSwitchActivated, int sampleTime, float precipRatio){
  char buffer[30] = "";
  appendFileSD(fs, path, "airflow");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readPrecip(timesSwitchActivated, sampleTime, precipRatio));
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);      //%lu unsigned long integer
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}