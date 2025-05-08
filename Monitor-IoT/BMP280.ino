float readPressure(Adafruit_BMP280 *bmp) {
  float f = bmp->readPressure();  // Pa
  return f/101325;   // atm
}

void settingsBMP(Adafruit_BMP280 *bmp) {
    bmp->setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
}
