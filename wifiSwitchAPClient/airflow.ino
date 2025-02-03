float readAirflow(int* timesSwitchActivated, int sampleTime, float airflowRatio){
  float currentRead = -1;
  currentRead = (*timesSwitchActivated / (sampleTime / 1000)) * airflowRatio;   // Sample time in ms
  *timesSwitchActivated = 0;
  Serial.print("Airflow read: ");
  Serial.println(currentRead);
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