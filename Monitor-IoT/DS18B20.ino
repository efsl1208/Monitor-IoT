float readDSBTemp(DallasTemperature sensor){
  sensor.requestTemperatures();
  float t = sensor.getTempCByIndex(0);
  return t;
}

void saveDSBTemp(fs::FS& fs, const char* path, unsigned long epochTime, DallasTemperature sensor){
  char buffer[30] = "";
  appendFileSD(fs, path, "temperature DS18B20");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readDSBTemp(sensor));
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);      //%lu unsigned long integer
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}