float readTemp(){
  float t = dht.readTemperature();
  Serial.println("Temperature: " , t, "°C");
  return t;
}

float readHumid(){
  float h = dht.readHumidity();
  Serial.println("Humidity: ", h, "%");
  return h;
}

void saveTemp(fs::FS& fs, const char* path, unsigned long epochTime){
  appendFileSD(fs, path, "temp");
  appendFileSD(fs, path, ",");
  appendFileSD(fs, path, readTemp());
  appendFileSD(fs, path, ",");
  appendFileSD(fs, path, epochTime);
  appendFileSD(fs, path, "\n");
}

void saveHumid(fs::FS& fs, const char* path, unsigned long epochTime){
  appendFileSD(fs, path, "humid");
  appendFileSD(fs, path, ",");
  appendFileSD(fs, path, readHumid());
  appendFileSD(fs, path, ",");
  appendFileSD(fs, path, epochTime);
  appendFileSD(fs, path, "\n");
}