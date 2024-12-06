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