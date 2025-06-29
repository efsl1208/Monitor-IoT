float readTempDHT(DHT dht){
  float t = dht.readTemperature();
  return t;
}

float readHumidDHT(DHT dht){
  float h = dht.readHumidity();
  return h;
}

void saveTemp(fs::FS& fs, const char* path, unsigned long epochTime){
  char buffer[30] = "";
  appendFileSD(fs, path, "temp");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readTempDHT());
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}

void saveHumid(fs::FS& fs, const char* path, unsigned long epochTime){
  char buffer[30] = "";
  appendFileSD(fs, path, "humid");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readHumidDHT());
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}