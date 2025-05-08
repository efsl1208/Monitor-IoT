float readPrecip(volatile int* timesSwitchActivated, int computeTime, float precipRatio){
  float currentRead = -1;
  currentRead = (*timesSwitchActivated / ((millis() - computeTime) / 1000) ) * precipRatio;   // Time between measurements can vary
  *timesSwitchActivated = 0;
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