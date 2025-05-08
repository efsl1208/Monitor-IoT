float readAirflow(volatile int* timesSwitchActivated, int computeTime, float airflowRatio){
  float currentRead = -1;
  currentRead = (*timesSwitchActivated / ((millis() - computeTime) / 1000) ) * airflowRatio;   // Time between measurements can vary
  *timesSwitchActivated = 0;
  return currentRead;
}

void saveAirflow(fs::FS& fs, const char* path, unsigned long epochTime, int* timesSwitchActivated, int sampleTime, float airflowRatio){
  char buffer[30] = "";
  appendFileSD(fs, path, "airflow");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readAirflow(timesSwitchActivated, sampleTime, airflowRatio));
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);      //%lu unsigned long integer
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}