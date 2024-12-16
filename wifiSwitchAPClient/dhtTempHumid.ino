float readTemp(){
  float t = dht.readTemperature();
  Serial.println("Temperature: ");
  Serial.println(t);
  Serial.println("°C");
  return t;
}

float readHumid(){
  float h = dht.readHumidity();
  Serial.println("Humidity: ");
  Serial.println(h);
  Serial.println("%");
  return h;
}

void saveTemp(fs::FS& fs, const char* path, unsigned long epochTime){
  char buffer[30] = "";
  appendFileSD(fs, path, "temp");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readTemp());
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
  sprintf(buffer, "%f", readHumid());
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}