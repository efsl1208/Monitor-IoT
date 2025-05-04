#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "time.h"
#include <RTClib.h>
#include <sys/time.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include "mbedtls/aes.h"
#include "mbedtls/ctr_drbg.h"
#include <HTTPClient.h>
// #include "miniz.h"
// #include "miniz.c"
//#include <ESP32_MySQL.h>


/*    Pins usage
/     0: RTC Interrupts
/     2: Led
/     4: DHT
/     5: CS
/     8: Reboot HW
/     15: Airflow
/     16: Precipitation
/     17: Temp DS18B20
/     18: CLK/SCK
/     19: MISO
/     21: I2C/SDA
/     22: I2C/SCL
/     23: MOSI
/     34: ADC (Solar Irr)
/     
*/

// PIN definitions, etc
#define RTCINTERRUPTPIN 13            // for consistent readings
#define TOKEN_EXPIRE_MIN 10
#define DHTPIN 4
#define DHTTYPE DHT11
#define DSBPIN 17
#define ADCPIN 34           // ADC1_6
#define AIRFLOWPIN 15
#define PRECIPPIN 16
//#define RESETPIN 8          // Connected to EN
#define BATTERYPIN 39
// Wind Direction             
#define WINDNPIN 35
#define WINDSPIN 32
#define WINDEPIN 33
#define WINDWPIN 25
#define WINDNEPIN 26
#define WINDNWPIN 27
#define WINDSEPIN 14
#define WINDSWPIN 12


// Buffers & compression
char bigBuffer[25000] = "";      
// static unsigned char *winBuffer[1024];
// TampCompressor compressor;

// Search for parameter in HTTP methods
const char* PARAM_INPUT_0 = "mode";
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";
const char* PARAM_INPUT_5 = "admin_pass";
const char* PARAM_DATA_0 = "inicio";
const char* PARAM_DATA_1 = "final";
const char* PARAM_DATA_2 = "variable";
const char* PARAM_DATA_3 = "frecuencia";
const char* PARAM_CONFIG_0 = "red";
const char* PARAM_CONFIG_1 = "calibracion";
const char* PARAM_CONFIG_2 = "datos";
const char* PARAM_CONFIG_3 = "alarmas";
const char* PARAM_CONFIG_4 = "servidor";
const char* PARAM_CONFIG_5 = "autenticacion";
const char* PARAM_CONFIG_6 = "reset";
const char* PARAM_SEC = "seccion";
const char* PARA_DL_0 = "anio";
const char* PARA_DL_1 = "mes";
const char* PARA_DL_2 = "dia";
const char* PARA_DL_3 = "variable";

const char* anual = "anual";
const char* monthly = "mensual";
const char* daily = "diario";

// RTC & time variables

RTC_DS3231 rtc;
int baseTime = 1000;                  // base sample time for real time measurements 
int realTimeC = 0;
int databaseTimer = 1000 * 60 * 5;    // After n minutes have passed, tries to send data
int databaseC = 0;

volatile bool alarmRTC = false;       // flag for alarm

int computeTime = 0;                  // Time in ms since last measurement compute for airflow & precipitation
unsigned long reconnectInt = 120000; 
int reconnectCounter = 0;

// R/W Variables
String rawTextLine;

// Variables to save values from HTML form
String mode;
String ssid;
String pass;
String ip;
String gateway;
String modeName = "mode";
String ssidName = "ssid";
String passName = "pass";
String ipName = "ip";
String gatewayName = "gateway";

String* csvNetworkConfig[5] = {&mode, &ssid, &pass, &ip, &gateway};
//String* csvNetworkConfig[4] = {&mode, &ssid, &ip, &gateway};

String* csvNetworkConfigName[5] = {&modeName, &ssidName, &passName, &ipName, &gatewayName};

// Auth variables
String admin_pass;
String admin_pass_name = "admin_pass";
String* csvAuthConfig[1] = {&admin_pass};
String* csvAuthConfigName[1] = {&admin_pass_name};

// variables for server config
String url = "0.0.0.0";
String port = "0";
String urlName = "url";
String portName = "puerto";

String* csvServerConfig[2] = {&url, &port};
String* csvServerConfigName[2] = {&urlName, &portName};

// Variables for config, depracated
String upperHourLimitS = "0";       // <= 0     defaults
String lowerHourLimitS = "23";      // >= 23
String upperMinLimitS = "5";        // <= 5
String lowerMinLimitS = "55";       // >= 55
int upperHourLimit = 0;       // <= 0     defaults
int lowerHourLimit = 23;      // >= 23
int upperMinLimit = 5;        // <= 5
int lowerMinLimit = 55;       // >= 55

String sampleTimeString = "5";     // default sample time in minutes
String gmtString = "-14400";
int sampleTime = 300000;
long gmtOffset = -14400;
String sampleTimeStringName = "p_muestreo";
String gmtStringName = "z_horaria";

//String* csvTimeConfig[5] = {&sampleTimeString, &upperHourLimitS, &lowerHourLimitS, &upperMinLimitS, &lowerMinLimitS};
String* csvTimeConfig[2] = {&sampleTimeString, &gmtString};
String* csvTimeConfigName[2] = {&sampleTimeStringName, &gmtStringName};

// Database info
// String databaseName = "";
// String tableName = "";
// String indexColName = "";
// String variableColName = "";
// String valueColName = "";
// String timestampColName = "";
// String* databaseNames[6] = {&databaseName, &tableName, &indexColName, &variableColName, &valueColName, &timestampColName};
// float databaseValues[740] = {-1};
// unsigned long databaseTimestamps[740] = {0};

// long databaseOffset = 0;
// long databaseSentTodayOffset = 0;

unsigned long lastInsertTimestamp = 1740801600;    //2025-03-01 YYYY-MM-DD
WiFiClient client;
HTTPClient http;

String maxHumidString  = "9999";
String maxTempString  = "9999";
String maxSolarString  = "9999";
String maxAirflowString  = "9999";
String maxPrecipString  = "9999";
String maxPressureString  = "9999";
String minHumidString  = "-1";
String minTempString  = "-1";
String minSolarString  = "-1";
String minAirflowString  = "-1";
String minPrecipString  = "-1";
String minPressureString  = "-1";

String maxHumidStringName  = "humedad_alto";
String maxTempStringName  = "temperatura_alto";
String maxSolarStringName  = "solar_alto";
String maxAirflowStringName  = "velocidad_alto";
String maxPrecipStringName  = "pluviosidad_alto";
String maxPressureStringName  = "presion_alto";
String minHumidStringName  = "humedad_bajo";
String minTempStringName  = "temperatura_bajo";
String minSolarStringName  = "solar_bajo";
String minAirflowStringName  = "velocidad_bajo";
String minPrecipStringName  = "pluviosidad_bajo";
String minPressureStringName  = "presion_bajo";

String humidAlarm = "0";
String tempAlarm = "0";
String solarAlarm = "0";
String airflowAlarm = "0";
String precipAlarm = "0";
String pressureAlarm = "0";

// long t_start_temp = 0;
// long t_start_humid = 0;
// long t_start_solar = 0;
// long t_start_airflow = 0;
// long t_start_precip = 0;
// long t_start_pressure = 0;

bool alarmEnabled[6] = {false};

String* csvAlarmConfig[12] = {
  &minHumidString, &minTempString, &minSolarString, &minAirflowString, &minPrecipString, &minPressureString,
  &maxHumidString, &maxTempString, &maxSolarString, &maxAirflowString, &maxPrecipString, &maxPressureString
};
String* csvAlarmConfigName[12] = {
  &minHumidStringName, &minTempStringName, &minSolarStringName, &minAirflowStringName, &minPrecipStringName, &minPressureStringName,
  &maxHumidStringName, &maxTempStringName, &maxSolarStringName, &maxAirflowStringName, &maxPrecipStringName, &maxPressureStringName
};
String* csvAlarmEnabled[6] = {&humidAlarm, &tempAlarm, &solarAlarm, &airflowAlarm, &precipAlarm, &pressureAlarm};
// long* alarmStart[6] = {&t_start_humid, &t_start_temp, &t_start_solar, &t_start_airflow, &t_start_precip, &t_start_pressure};

// Variables for calibration
String solarRatioString = "1.0";
String airflowRatioString = "1.0";
String precipRatioString = "1.0";
float solarRatio = 1.0;
float airflowRatio = 1.0;
float precipRatio = 1.0;
String solarRatioStringName = "solar";
String airflowRatioStringName = "anemometro";
String precipRatioStringName = "pluviometro";

String* csvCalibrationConfig[3] = {&solarRatioString, &precipRatioString, &airflowRatioString};
String* csvCalibrationConfigName[3] = {&solarRatioStringName, &precipRatioStringName, &airflowRatioStringName};

// POST variables
String postRequestName[13] = {""};
String postRequestValue[13] = {""};

// Encrypt vars
char tempMessage[64] = "";
char encryptedMessage[64] = "";
char eKey[16] = {0x45, 0x73, 0x74, 0x61, 0x63, 0x69, 0x6F, 0x6E, 0x4D, 0x65, 0x74, 0x65, 0x6F, 0x49, 0x6F, 0x54};//"EstacionMeteoIoT";
char eIv[16] = "";
char eIvNetwork[16] = "";
char eIvAuth[16] = ""; 

// Tokens

bool tokenActive[5] = {false};
String tokenValue[5] = {""};
long tokenExpire[5] = {0};

// File paths to save input values permanently
const char* networkConfigPath = "/config/red.conf";
const char* authConfigPath = "/config/autenticacion.conf";
const char* serverConfigPath = "/config/servidor.conf";
const char* timeConfigPath = "/config/tomaDatos.conf";
const char* alarmConfigPath = "/config/alarmas.conf";
const char* enabledAlarmConfigPath = "/config/alarmasActivas.conf";
const char* calibrationConfigPath = "/config/calibracion.conf";
const char* ivPathNetwork = "/config/sys0";
const char* ivPathAuth = "/config/sys1";
const char* alarmHistPath = "/misc/alarmHist.JSONL";
const char* lastInsertPath = "/config/sys2";

const char* configPath = "/config/config.conf";
const char* credentialsPath = "/config/credentials.conf";
const char* credentialsPathBackup = "/credentialsBackup.txt";
const char* logsPathConst = "/logs.txt";
const char* logsPathBacup = "/logsBackup.txt";

char logsPath[60] = "/logs.JSONL";
char avgPath[60] = "/avg.JSONL";
char monthAvgPath[60] = "/monthAvg.JSONL";
char maxPath[60] = "/max.JSONL";
char minPath[60] = "/min.JSONL";
char datePath[30];
char currentDateChar[20] = "";
char olderDateChar[20] = "";

// Logic variables
bool should_restart = false;
bool factReset = false;
bool isInitSD = false;
bool isRTC = false;
int batteryLevel = 0;
int statusC = 0;

// Time variables
const char *ntpServer = "pool.ntp.org";
const char *ntpServer1 = "time.nist.gov";
unsigned long epochTime_1 = 0;
unsigned long epochTime_2 = 0;
time_t now;
struct tm tmstruct;
DateTime date;
int oldestYear = 2025;
int oldestMonth = 1;
int oldestDay = 1;
int currentDay = -1;
int pastMonth = 1;
int pastYear = 1;
int pastDay = 1;
//int* oldestDate[3] = {&oldestYear, &oldestMonth, &oldestDay}; 

// DHT config
DHT dht (DHTPIN, DHTTYPE);
// // DHT variables, should delete?
// float dhtVar[2] = {0.0};

// BMP280
bool isBMP = false;
Adafruit_BMP280 bmp;    // 0x76 address

// DS18B20
//DallasTemperature tempDSB;
OneWire oneWire(DSBPIN);
DallasTemperature tempDSB(&oneWire);

// Solar irr variables
int intADCRead = -1;
float solarIrr = 0.0;       // Solar irradiance index W/m^2

// Airflow variables
volatile int timesAirflowSwitch = 0;
volatile int* ptrTimesAirflowSwitch = &timesAirflowSwitch;
int airFlowSwitchTime = 0;
int prevAirFlowSwitchTime = 0;
//float airflow = 0.0;

// Precipitation variables
volatile int timesPrecipSwitch = 0;
volatile int* ptrTimesPrecipSwitch = &timesPrecipSwitch;
int precipSwitchTime = 0;
int prevPrecipSwitchTime = 0;
//float pecip = 0.0;

// Local readings
float tempDHT = -1;
float humid = -1;
float temp = -1;
float solar = -1;
float airflow = -1;
float windDir = -1;
float precip = -1;
float pressure = -1;
//float* readings[8] = {&tempDHT, &humidDHT, &tempDSB_reading, &solar, &airflow, &windDir, &precip, &pressure};
float* readings[7] = {&humid, &temp, &solar, &airflow, &precip, &pressure, &windDir};

// Backup arrays in memory, when SD is not available
unsigned long backupTimestamp[20] = {0};
float backupValues[7][20];

// Variables to send data to UI chart
String start;
String end;
String sensor;
String frequency;

// Download variables
String yearDL;
String monthDL;
String dayDL;
String varDL;
int monthIntDL = -1;
int dayIntDL = -1;
bool isParamDL[4] = {false};

// Max readings
float maxTempDHT = -100;
float maxHumid = -100;
float maxTemp = -100;
float maxSolar = -100;
float maxAirflow = -100;
//float maxWindDir = -1;
float maxPrecip = -100;
float maxPressure = -100;
//float* maxReadings[7] = {&maxHumid, &maxTemp, &maxSolar, &maxAirflow, &maxWindDir, &maxPrecip, &maxPressure};
float* maxReadings[6] = {&maxHumid, &maxTemp, &maxSolar, &maxAirflow, &maxPrecip, &maxPressure};

// Min readings
float minTempDHT = 9999;
float minHumid = 9999;
float minTemp = 9999;
float minSolar = 9999;
float minAirflow = 9999;
//float minWindDir = 9999;
float minPrecip = 9999;
float minPressure = 9999;
//float* minReadings[7] = {&minHumid, &minTemp, &minSolar, &minAirflow, &minWindDir, &minPrecip, &minPressure};
float* minReadings[6] = {&minHumid, &minTemp, &minSolar, &minAirflow, &minPrecip, &minPressure};

// Alarm limits
float maxHumidLimit = 9999;
float maxTempLimit = 9999;
float maxSolarLimit = 9999;
float maxAirflowLimit = 9999;
float maxPrecipLimit = 9999;
float maxPressureLimit = 9999;
float* maxReadingsLimit[6] = {&maxHumidLimit, &maxTempLimit, &maxSolarLimit, &maxAirflowLimit, &maxPrecipLimit, &maxPressureLimit};

// Alarm limits
float minHumidLimit = -100;
float minTempLimit = -100;
float minSolarLimit = -100;
float minAirflowLimit = -100;
float minPrecipLimit = -100;
float minPressureLimit = -100;
float* minReadingsLimit[6] = {&minHumidLimit, &minTempLimit, &minSolarLimit, &minAirflowLimit, &minPrecipLimit, &minPressureLimit};

// Alarm max value
float alarmMaxValue[12] = {9999,9999,9999,9999,9999,9999,-100,-100,-100,-100,-100,-100};

// Alarm control
bool activeAlarm[12] = {false};
long unsigned startAlarmTime[12] = {0};
long unsigned endAlarmTime[12] = {0};
bool sensorAlarm = false;

// Variable names
String tempDHTName = "TemperatureDHT";
String humidName = "humedad";
String tempName = "temperatura";
String solarName = "solar";
String airflowName = "velocidad";
String windDirName = "direccion";
String precipName = "pluviosidad";
String pressName = "presion";
String timestName = "t";
//String* variables[9] = {&timestName, &tempDHTName, &humidDHTName, &tempDSBName, &solarName, &airflowName, &windDirName, &precipName, &pressName};
//String* variablesMaxMin[8] =  {&timestName, &tempDHTName, &humidDHTName, &tempDSBName, &solarName, &airflowName, &precipName, &pressName};
String* variableName[8] = {&timestName, &humidName, &tempName, &solarName, &airflowName, &precipName, &pressName, &windDirName};
String* variablesMaxMinName[7] =  {&timestName, &humidName, &tempName, &solarName, &airflowName, &precipName, &pressName};

// Average computation
//int nReadings[8];
//float cReadings[8];
int nReadings[7];
float cReadings[7];
float avgTempDHT = -1;
float avgHumid = -1;
float avgTemp = -1;
float avgSolar = -1;
float avgAirflow = -1;
float avgWindDir = -1;        // check?
float avgPrecip = -1;
float avgPressure = -1;
//float* avgReadings[8] = {&avgTempDHT, &avgHumidDHT, &avgTempDSB_reading, &avgSolar, &avgAirflow, &avgWindDir, &avgPrecip, &avgPressure};
float* avgReadings[7] = {&avgHumid, &avgTemp, &avgSolar, &avgAirflow, &avgPrecip, &avgPressure, &avgWindDir};

// Extern functions
// RTC DS3231
extern void rtcInit();
extern void rtcAdjust();
extern unsigned long getEpochRTC();
extern void rtcPrintTime();
// LittleFS
extern void initLittleFS();
extern String readFileFS();
extern String readMultipleLinesFS();
extern String readLineFS();
extern void writeFileFS();
extern void writeCharArrayFS();
extern void deleteFileFS();
extern void listDirFS();
// SD
extern void initSD();
extern void listDirSD();
extern String readFileSD();
extern String readLineSD();
extern String readMultipleLinesSD();
extern void writeFileSD();
extern void writeCharArraySD();
extern void appendFileSD();
extern void appendFileDebugSD();
extern void appendCSVFileSD();
extern void writeJsonlSD();
extern void writeJsonlSDDebug();
extern void writeSingleSensorSD();
extern void deleteFileSD();
// Solar Irradiance
extern float readSolarIrr();
extern float readPower();
extern void saveSolarIrr();
// DHT
extern float readTempDHT();
extern float readHumidDHT();
extern void saveTemp();
extern void saveHumid();
// DS18B200
extern DallasTemperature initDSBTemp();
extern float readDSBTemp();
extern void saveDSBTemp();
// Airflow
extern float readAirflow();
extern void saveAirflow();
// Wind direction
extern float readWindDir();
extern float readWindDirFloat();      // depracated
extern void saveWindDir();
// Rain gauge
extern float readPrecip();
extern void savePrecip();
// BMP280
extern float readPressure();
extern void settingsBMP();

// AsyncWebServer object on port 80
AsyncWebServer server(80);
// Web socket
AsyncWebSocket ws("/tiemporeal");

// Network variables
IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8,8,8,8);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledPin = 2;
// Stores LED state
String ledState = "OFF";

// Parse values of text_vars as CSV text
void parse_csv(String* text_vars[], String* csv, char* del, int qty) {
    char buffer[512] = "";

    strcpy(buffer, csv->c_str());   //copy text into buffer
    Serial.print("String copied to buffer: ");
    Serial.println(buffer);

    char* p = strtok(buffer, del);
    // Serial.print("First item read: ");
    // Serial.println(p);

    for(int i = 0; i < qty; i++) {   //qty = text_vars length
        if (p) {
          *text_vars[i] = p;    //if p exists
          // Serial.print("p value saved: ");
          // Serial.println(p);
        } else{
          *text_vars[i] = "";
        } 
        p = strtok(NULL, del);      //NULL pointer, strtok pointer to last string
    }
}    

// Config Factory reset
void resetConfig() {
  writeFileFS(LittleFS, networkConfigPath, "");
  writeFileFS(LittleFS, authConfigPath, "");
  writeFileFS(LittleFS, serverConfigPath, "");
  writeFileFS(LittleFS, timeConfigPath, "");
  writeFileFS(LittleFS, alarmConfigPath, "");
  writeFileFS(LittleFS, enabledAlarmConfigPath, "");
  writeFileFS(LittleFS, calibrationConfigPath, "");
  writeFileFS(LittleFS, ivPathNetwork, "");
  writeFileFS(LittleFS, ivPathAuth, "");
  if (isInitSD) {
    writeFileSD(SD, networkConfigPath, "");
    writeFileSD(SD, authConfigPath, "");
    writeFileSD(SD, serverConfigPath, "");
    writeFileSD(SD, timeConfigPath, "");
    writeFileSD(SD, alarmConfigPath, "");
    writeFileSD(SD, enabledAlarmConfigPath, "");
    writeFileSD(SD, calibrationConfigPath, "");
    writeFileSD(SD, ivPathNetwork, "");
    writeFileSD(SD, ivPathAuth, "");
  }
  factReset = true;
}

// Gets time in EPOCH
unsigned long getTimeEpoch(long offset) {
  if (!getLocalTime(&tmstruct)) {
    Serial.println("Failed to obtain time");
    return(0);  //returns 0 if failed to obtain time
  }
  time(&now);
  return now - offset;
}

// Return epoch for given day
unsigned long getTimeEpoch(int year, int month, int day) {
  unsigned long rtn = 0;
  // struct tm tv;
  // //tv.tm_sec = 0;
  // //tv.tm_usec = 0;
  // Serial.println("Trying to assign y/m/d");
  // tv.tm_year = year;
  // tv.tm_mon = month;
  // tv.tm_mday = day;
  // Serial.println("tv assigned...");
  // time_t tt = mktime(&tv);
  // Serial.println("tt assigned...");
  // //settimeofday(&tv, NULL);
  // rtn = tt;
  // Serial.print("tt value: ");
  // Serial.println(tt);
  // //updateTime();
  for(int i = 1970; i < year; i++) {
    rtn += (337 + daysInMonth(i, 2))*86400;     // 86400 secs in a day
  }
  for(int i = 1; i < month; i++) {
    rtn += (daysInMonth(year, i))*86400;
  }
  rtn += (day - 1) * 86400;
  return rtn;
}

// Sets time from epoch
void manualTimeSet(unsigned long epochTime) {
  struct timeval tv;
  tv.tv_sec = epochTime;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
}

// Updates time for date management
void updateTime() {
  // Updating Time Structure
  if (isRTC) {
    date = rtc.now();
  } else {
    getLocalTime(&tmstruct);
    date = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  }
}

// Initialize WiFi in STATION MODE
bool initWiFi() {
  if (ssid == "" || ip == "") {
    Serial.println("Undefined SSID or IP address or mode set to AP.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  Serial.println(ip.c_str());

  if (!WiFi.config(localIP, localGateway, subnet, dns)) {
    Serial.println("STA Failed to configure");
    return false;
  }

  Serial.println("Setting up");

  if (pass == "NULL") WiFi.begin(ssid.c_str(), NULL);
  else WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

// Init websocket
void initWS() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Sending readings
void sendMessageWS(String data) {
  ws.textAll(data);
}

// Current readings to String
String currentReadingsString(float* values[], String* identifier[], int arrayLength, int epochTime) {
  // Serial.print("Preparing data for ws...");
  char buffer[600] = "";    // Might be overkill
  char nBuffer[20] = "";    // Name buffer
  String reads = "";
  //strcpy(buffer, "{tipo: \"sensorData\", data:{t:");       // Timestamp name hardcoded to "t", maybe change
  strcpy(buffer, "{ \"tipo\": \"datos\", \"data\": { \"t\": \"");       
  //sprintf (nBuffer, "%lu", epochTime);
  //strcat(buffer, nBuffer);
  //strcat(buffer, ", {");
  sprintf (nBuffer, "%lu", epochTime);
  strcat(buffer, nBuffer);
  strcat(buffer, "\", ");

  for(int i = 0; i < arrayLength; i++) {
    strcat(buffer, "\"");
    strcpy(nBuffer, identifier[i+1]->c_str());
    strcat(buffer, nBuffer);
    strcat(buffer, "\": \"");
    sprintf (nBuffer, "%.2f", *values[i]);
    strcat(buffer, nBuffer);
    strcat(buffer, "\"");
    if (i + 1 < arrayLength) strcat(buffer, ", ");
  }

  // for(int i = 0; i < arrayLength; i++) {
  //   strcat(buffer, "\"");
  //   strcpy(nBuffer, identifier[i+1]->c_str());
  //   strcat(buffer, nBuffer);
  //   strcat(buffer, "\": { \"t\": ");          // Timestamp name hardcoded to "t", maybe change
  //   sprintf (nBuffer, "%lu", epochTime);
  //   strcat(buffer, nBuffer);
  //   strcat(buffer, ", \"variable\": \"");
  //   strcpy(nBuffer, identifier[i+1]->c_str());
  //   strcat(buffer, nBuffer);
  //   strcat(buffer, "\", \"valor\": ");
  //   sprintf (nBuffer, "%.2f", *values[i]);
  //   strcat(buffer, nBuffer);
  //   strcat(buffer, "}");
  //   if (i+1<arrayLength) strcat(buffer, ", ");
  // }

  strcat(buffer, "}}");

  reads = buffer;
  //Serial.println(reads);
  return reads;
}

// Alarm to String
String alarmString(float* values[], String* identifier[], bool end) {     // eventHL true -> bajo, eventHL false -> alto
  Serial.print("Preparing alarm String for ws...");
  char buffer[512] = "";
  char nBuffer[20] = "";
  bool coma = true;
  //String alarm = "";
  // char eventType[10] = "";
  // if (eventHL) {
  //   eventType = "bajo";
  // } else {
  //   eventType = "alto";
  // }
  updateTime();
  strcpy(buffer, "{ \"tipo\": \"alarmas_activas\", \"data\": [");
  for (int i = 0; i < 12; i++) { 
    if (activeAlarm[i]) {
      if (!coma) strcat(buffer, ",");
      strcat(buffer, "{ \"variable\": \"");
      if (i < 6) {
        strcpy(nBuffer, identifier[i+1]->c_str());
      } else {
        strcpy(nBuffer, identifier[i-5]->c_str());
      }
      strcat(buffer, nBuffer);
      strcat(buffer, "\", \"valor\": \"");
      
      if (i < 6) {
        sprintf (nBuffer, "%.2f", *values[i]);
      } else {
        sprintf (nBuffer, "%.2f", *values[i-6]);
      }
      strcat(buffer, nBuffer);
      //strcat(buffer, ", \"eventType\": \"");
      strcat(buffer, "\", \"tipo_evento\": \"");
      if (i < 6) {
        strcat(buffer, "bajo");
      } else {
        strcat(buffer, "alto");
      }
      strcat(buffer, "\", \"t_inicio\": \"");
      sprintf(nBuffer, "%lu", startAlarmTime[i]);
      strcat(buffer, nBuffer);
      strcat(buffer, "\", \"t_actual\": \"");
      sprintf(nBuffer, "%lu", getTimeEpoch(0));
      strcat(buffer, nBuffer);
      strcat(buffer, "\", \"t_final\": \"");
      if (end) {
        strcat(buffer, nBuffer);
      } else {
        strcat(buffer, "0");
      }
      strcat(buffer, "\" }");
      coma = false;
    }
  }
 
  strcat(buffer, "]}");
  //alarm = buffer;
  Serial.println(buffer);
  return buffer;
}

// Save alarm in SD
void saveAlarm(String* identifier[], int i) {     // eventHL true -> bajo, eventHL false -> alto
  Serial.print("Preparing alarm String for saving...");
  char buffer[512] = "";
  char nBuffer[20] = "";
  updateTime();
  //for (int i = 0; i < 12; i++) { 
    if (activeAlarm[i]) {
      strcpy(buffer, "");
      // if (!coma) strcat(buffer, ",");
      strcat(buffer, "{ \"variable\": \"");
      if (i < 6) {
        strcpy(nBuffer, identifier[i+1]->c_str());
      } else {
        strcpy(nBuffer, identifier[i-5]->c_str());
      }
      strcat(buffer, nBuffer);
      strcat(buffer, "\", \"valor_maximo\": \"");
      sprintf (nBuffer, "%.2f", alarmMaxValue[i]);
      strcat(buffer, nBuffer);
      //strcat(buffer, ", \"eventType\": \"");
      strcat(buffer, "\", \"tipo_evento\": \"");
      if (i < 6) {
        strcat(buffer, "bajo");
      } else {
        strcat(buffer, "alto");
      }
      strcat(buffer, "\", \"t_inicio\": \"");
      sprintf(nBuffer, "%lu", startAlarmTime[i]);
      strcat(buffer, nBuffer);
      strcat(buffer, "\", \"t_final\": \"");
      sprintf(nBuffer, "%lu", getTimeEpoch(0));
      strcat(buffer, nBuffer);
      strcat(buffer, "\" }\n");
      Serial.println(buffer);
      appendFileSD(SD, alarmHistPath, buffer);
    }
  //}
}

// Reads old alarms
String histAlarm() {
  String line = "init";
  bool coma = true;
  File file = SD.open(alarmHistPath);
  if (!file) {
    Serial.println("Failed to open file to send alarms data...");
    return "";
  }
  //strcpy(buffer, "{ \"data\": [");
  strcpy(bigBuffer, "{ \"tipo\": \"alarmas_historico\", \"data\": [");
  while (!(line == "")) {
    // line = "";
    line = readLineSD(file);
    if(!coma && !(line == "")) strcat(bigBuffer, ",");
    strcat(bigBuffer, line.c_str());
    coma = false;

  }
  // esp_task_wdt_reset();
  
  file.close();
  strcat(bigBuffer, "]}");
  String rtn = bigBuffer;
  strcpy(bigBuffer, "");
  Serial.print("String to send to client: ");
  Serial.println(rtn);
  return rtn;
}

// Updates historic
String histAlarmUpd() {
  Serial.print("Preparing string for historic...");
  char buffer[512] = "";
  char nBuffer[20] = "";
  bool coma = true;
  updateTime();
  strcpy(buffer, "{ \"tipo\": \"alarmas_historico_act\", \"data\": [");
  for (int i = 0; i < 12; i++) { 
    if (activeAlarm[i]) {
      // strcpy(buffer, "");
      if (!coma) strcat(buffer, ",");
      strcat(buffer, "{ \"variable\": \"");
      if (i < 6) {
        strcpy(nBuffer, variableName[i+1]->c_str());
      } else {
        strcpy(nBuffer, variableName[i-5]->c_str());
      }
      strcat(buffer, nBuffer);
      strcat(buffer, "\", \"valor_maximo\": \"");
      sprintf (nBuffer, "%.2f", alarmMaxValue[i]);
      strcat(buffer, nBuffer);
      //strcat(buffer, ", \"eventType\": \"");
      strcat(buffer, "\", \"tipo_evento\": \"");
      if (i < 6) {
        strcat(buffer, "bajo");
      } else {
        strcat(buffer, "alto");
      }
      strcat(buffer, "\", \"t_inicio\": \"");
      sprintf(nBuffer, "%lu", startAlarmTime[i]);
      strcat(buffer, nBuffer);
      strcat(buffer, "\", \"t_final\": \"");
      sprintf(nBuffer, "%lu", getTimeEpoch(0));
      strcat(buffer, nBuffer);
      strcat(buffer, "\" }");
      
      coma = false;
    }
  }
  strcat(buffer, "]}");
  Serial.println(buffer);
  return buffer;
}

// Alarms to JSON
String alarmToJsonString() {
  char rtn[512] = "";
  char nBuffer[10];
  bool coma = true;
  strcpy(rtn, "[");
  for (int i = 0; i < 6; i++) {
    if (alarmEnabled[i]) {
      if(!coma) strcat(rtn, " , ");
      strcat(rtn, "{ \"variable\": \"");
      strcat(rtn, variableName[i + 1]->c_str());
      strcat(rtn, "\", \"alto\": \"");
      sprintf(nBuffer, "%.2f", *maxReadingsLimit[i]);
      strcat(rtn, nBuffer);
      strcat(rtn, "\", \"bajo\": \"");
      sprintf(nBuffer, "%.2f", *minReadingsLimit[i]);
      strcat(rtn, nBuffer);
      strcat(rtn, "\" }");
      coma = false;
    }
  }
  strcat(rtn, "]");
  Serial.print("Alarm String: ");
  Serial.println(rtn);
  return rtn;
}

// String from config values
String arrayToJsonString(String* identifier[], String* field[], int arrayLength) {
  //String rtn = "";
  char buffer[600] = "";
  //char tempBuffer[30] = "";
  bool coma = true;
  strcpy(buffer, "{");
  for(int i = 0; i < arrayLength; i++) {
    if (!coma) {
      strcat(buffer, ",");
      coma = true;
    }
    strcat(buffer, "\"");
    strcat(buffer, identifier[i]->c_str());
    strcat(buffer, "\": \"");
    strcat(buffer, field[i]->c_str());
    strcat(buffer, "\"");
    coma = false;
  }
  strcat(buffer, "}");

  Serial.print("String to return: ");
  Serial.println(buffer);

  return buffer;
}

// WS event handler
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf ("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      client->text(histAlarm());
      client->text(statusBar());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf ("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWSMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// WS message handler
void handleWSMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    // Check message
    if (strcmp((char*)data, "test message") == 0) {
      Serial.println("test message from ws received...");
    }
  }
}

// Check oldest date registered
void oldestDate() {
  //String oldestD = "";
  char yearPath[10] = "/2025";
  char monthPath[15] = "/2025/1";
  char dayPath[60] = "/2025/1/1";
  char buffer[10] = "";
  int compYear = date.year();       // Compare values
  int compMonth = 1;
  int compDay = 1;
  int nC = 0;        // Control
  // Checks for current year
  strcpy(yearPath, "/");
  sprintf (buffer, "%d", compYear);
  strcat(yearPath, buffer);
  while (SD.exists(yearPath) || nC < 20) {
    
    if (SD.exists(yearPath)) {
      oldestYear = compYear;
      // Serial.print("found older year: ");
      // Serial.println(oldestYear);
    }
    compYear += -1;
    nC++;
    strcpy(yearPath, "/");
    sprintf (buffer, "%d", compYear);
    strcat(yearPath, buffer);
    
  }
  // Checks for oldest month registered
  strcpy(yearPath, "/");
  sprintf (buffer, "%d", oldestYear);
  strcat(yearPath, buffer);
  strcpy(monthPath, yearPath);
  strcat(monthPath, "/");
  sprintf (buffer, "%d", compMonth);
  strcat(monthPath, buffer);
  nC = 1;
  while (!SD.exists(monthPath) && nC < 12) {
    compMonth += 1;
    nC++;
    oldestMonth = compMonth;
    strcpy(monthPath, yearPath);
    strcat(monthPath, "/");
    sprintf (buffer, "%d", compMonth);
    strcat(monthPath, buffer);
    
    // Serial.print("found older month: ");
    // Serial.println(buffer);
  }
  //Checks for oldest day registered
  strcpy(monthPath, yearPath);
  strcat(monthPath, "/");
  sprintf (buffer, "%d", oldestMonth);
  strcat(monthPath, buffer);
  strcpy(dayPath, monthPath);
  strcat(dayPath, "/");
  sprintf (buffer, "%d", compDay);
  strcat(dayPath, buffer);
  //c = 1;
  // while (!SD.exists(dayPath) && compDay < 31) {
  //   compDay += 1;
  //   //c++;
  //   oldestDay = compDay;
  //   strcpy(dayPath, monthPath);
  //   strcat(dayPath, "/");
  //   sprintf (buffer,"%d", oldestYear);
  //   strcat(dayPath, buffer);
  //   strcat(dayPath, "-");
  //   sprintf (buffer, "%02d", oldestMonth);
  //   strcat(dayPath, buffer);
  //   strcat(dayPath, "-");
  //   sprintf (buffer, "%02d", compDay);
  //   strcat(dayPath, buffer);
  //   strcat(dayPath, "_");
  //   strcat(dayPath, variableName[0]->c_str());
  //   strcat(dayPath, ".JSONL");

  //   Serial.print("found older day: ");
  //   Serial.println(buffer);
  // }
  for(int i = 0; i < 31; i++) { 
    
    strcpy(dayPath, monthPath);
    strcat(dayPath, "/");
    sprintf (buffer,"%d", oldestYear);
    strcat(dayPath, buffer);
    strcat(dayPath, "-");
    sprintf (buffer, "%02d", oldestMonth);
    strcat(dayPath, buffer);
    strcat(dayPath, "-");
    sprintf (buffer, "%02d", i + 1);
    strcat(dayPath, buffer);
    strcat(dayPath, "_");
    strcat(dayPath, variableName[1]->c_str());
    strcat(dayPath, ".JSONL");
    oldestDay = i + 1;
    // Serial.print("found older day: ");
    // Serial.println(i + 1);
    //Serial.println(dayPath);
    if (SD.exists(dayPath)) {
      i = 31;
    }

  }
  Serial.print("Oldest date: ");
  Serial.print(oldestYear);
  Serial.print("-");
  Serial.print(oldestMonth);
  Serial.print("-");
  Serial.println(oldestDay);
}

// Oldest date available to String
String oldestDateString() {
  Serial.print("Preparing date String for HTTP GET...");
  char buffer[300] = "";
  char nBuffer[20] = "";
  String oldest = "";
  // char eventType[10] = "";
  // if (eventHL) {
  //   eventType = "bajo";
  // } else {
  //   eventType = "alto";
  // }
  strcpy(buffer, "[\"");
  sprintf (nBuffer, "%d", oldestYear);
  strcat(buffer, nBuffer);
  strcat(buffer, "-");
  sprintf (nBuffer, "%02d", oldestMonth);
  strcat(buffer,nBuffer);
  strcat(buffer, "-");
  sprintf (nBuffer, "%02d", oldestDay);
  strcat(buffer, nBuffer);
  strcat(buffer, "\", \"");
  sprintf (nBuffer, "%d", date.year());
  strcat(buffer, nBuffer);
  strcat(buffer, "-");
  sprintf (nBuffer, "%02d", date.month());
  strcat(buffer,nBuffer);
  strcat(buffer, "-");
  sprintf (nBuffer, "%02d", date.day());
  strcat(buffer, nBuffer);
  strcat(buffer, "\"]");

  oldest = buffer;
  Serial.println(oldest);
  return oldest;
}


// Replaces placeholder with LED state value
// String processor(const String& var) {
//   if (var == "STATE") {
//     if (digitalRead(ledPin)) {
//       ledState = "ON";
//     } else {
//       ledState = "OFF";
//     }
//     return ledState;
//   }
//   return String();
// }

void blink() {
  digitalWrite(ledPin, HIGH);
  delay(1000);
  digitalWrite(ledPin, LOW);
  delay(1000);
}

// Change date path, "YY/MM/..."
void changeDatePath() {
  //DateTime dateYMD = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  char buffer[30] = "";
  strcpy(datePath, "/");
  sprintf (buffer, "%d", date.year());
  strcat(datePath, buffer);
  if (!SD.exists(datePath)) createDirSD(SD, datePath);
  sprintf (buffer, "%d", date.month());
  strcat(datePath, "/");
  strcat(datePath, buffer);
  if (!SD.exists(datePath)) createDirSD(SD, datePath);
  // sprintf (buffer, "%d", dateYMD.day());                  
  // strcat(datePath, "/");
  // strcat(datePath, buffer);
  // if (!SD.exists(datePath)) createDirSD(SD, datePath);

  Serial.print("Date path: ");
  Serial.println(datePath);
}

// Returns path given the type of data to save, depracated
char* getTypePath(int type) {                // 0: temp DHT, 1: humid DHT, 2: temp DSB, 3: solar irr, 4: airflow, 5: windDir, 6: precip, 7: pressure
  strcpy(logsPath, datePath);

  char dataType[30] = "default";
  switch(type) {
    case 0:
      strcpy(dataType, "/tempDHT");
      break;
    
    case 1:
      strcpy(dataType, "/humid");
      break;
    
    case 2:
      strcpy(dataType, "/tempDSB");
      break;
    
    case 3:
      strcpy(dataType, "/solar");
      break;
    
    case 4:
      strcpy(dataType, "/airflow");
      break;
    
    case 5:
      strcpy(dataType, "/windDir");
      break;
    
    case 6:
      strcpy(dataType, "/precip");
      break;
      
    case 7:
      strcpy(dataType, "/pressure");
      break;
  }
  strcat(logsPath, dataType);
  if (!SD.exists(logsPath)) createDirSD(SD, logsPath);
  strcat(logsPath, "/logs.csv");
  Serial.print("saving to: ");
  Serial.println(logsPath);
  return logsPath;
}

// Returns date YYYY-MM-DD
void currentDate() {
  Serial.println("trying to get current date...");
  char buffer[20] = "";
  char b[5] = "";
  //strcpy(buffer, datePath);
  strcpy(buffer, "");
  sprintf (b, "%d", date.year());
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf (b, "%02d", date.month());
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf (b, "%02d", date.day());
  strcat(buffer, b);
  Serial.print("date fetch: ");
  Serial.println(buffer);
  strcpy(currentDateChar, buffer);
  //return buffer;
}

// Returns date YYYY-MM-DD
void pastDate(int year, int month, int day) {
  Serial.println("trying to get past date...");
  char buffer[20] = "";
  char b[5] = "";
  //strcpy(buffer, datePath);
  strcpy(buffer, "");
  sprintf (b, "%d", year);
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf (b, "%02d", month);
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf (b, "%02d", day);
  strcat(buffer, b);
  Serial.print("date fetch: ");
  Serial.println(buffer);
  strcpy(olderDateChar, buffer);
  //return buffer;
}

// Returns current month YYYY-MM
void currentMonth() {
  Serial.println("trying to get current month...");
  char buffer[20] = "";
  char b[5] = "";
  //strcpy(buffer, datePath);
  strcpy(buffer, "");
  sprintf (b, "%d", date.year());
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf (b, "%02d", date.month());
  strcat(buffer, b);
  strcat(buffer, "-");
  Serial.print("month fetch: ");
  Serial.println(buffer);
  strcpy(currentDateChar, buffer);
}

// Changes logs path, .../YYYY-MM-DD.JSONL
void changeLogsPath() {
  //DateTime dateYMD = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(logsPath, datePath);
  strcat(logsPath, "/");
  sprintf (buffer, "%d", date.year());
  strcat(logsPath, buffer);
  strcat(logsPath, "-");
  sprintf (buffer, "%02d", date.month());
  strcat(logsPath, buffer);
  strcat(logsPath, "-");
  sprintf (buffer, "%02d", date.day());
  strcat(logsPath, buffer);
  strcat(logsPath, ".JSONL");       

  //strcat(logsPath, "/logs.csv");
  // if (!SD.exists(logsPath)) {
  //   appendCSVFileSD(SD, logsPath, variables, 9);
  // }

  Serial.print("saving to: ");
  Serial.println(logsPath);

  //return logsPath;
}

// Changes averages log path, .../YYYY-MM-DD_avg.JSONL
void changeAvgPath() {

  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(avgPath, datePath);
  strcat(avgPath, "/");
  sprintf (buffer, "%d", date.year());
  strcat(avgPath, buffer);
  strcat(avgPath, "-");
  sprintf (buffer, "%02d", date.month());
  strcat(avgPath, buffer);
  strcat(avgPath, "-");
  sprintf (buffer, "%02d", date.day());
  strcat(avgPath, buffer);
  strcat(avgPath, "_avg.JSONL");       

  Serial.print("saving averages to: ");
  Serial.println(avgPath);

}

void changeMonthAvgPath() {
  char buffer[5] = "";
  
  strcpy(monthAvgPath, "/");
  sprintf (buffer, "%d", date.year());
  strcat(monthAvgPath, buffer);
  strcat(monthAvgPath, "/");
  strcat(monthAvgPath, buffer);
  strcat(monthAvgPath, "-");
  sprintf (buffer, "%02d", date.month());
  strcat(monthAvgPath, buffer);
  strcat(monthAvgPath, "_monthAvg.JSONL");

  Serial.print("saving month avg values to: ");
  Serial.println(monthAvgPath);

}

// Changes max readings log path, .../YYYY-MM-DD_max.JSONL
void changeMaxPath() {

  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(maxPath, datePath);
  strcat(maxPath, "/");
  sprintf (buffer, "%d", date.year());
  strcat(maxPath, buffer);
  strcat(maxPath, "-");
  sprintf (buffer, "%02d", date.month());
  strcat(maxPath, buffer);
  strcat(maxPath, "-");
  sprintf (buffer, "%02d", date.day());
  strcat(maxPath, buffer);
  strcat(maxPath, "_max.JSONL");       

  Serial.print("saving max values to: ");
  Serial.println(maxPath);

}

// Changes min readings log path, .../YYYY-MM-DD_min.JSONL
void changeMinPath() {

  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(minPath, datePath);
  strcat(minPath, "/");
  sprintf (buffer, "%d", date.year());
  strcat(minPath, buffer);
  strcat(minPath, "-");
  sprintf (buffer, "%02d", date.month());
  strcat(minPath, buffer);
  strcat(minPath, "-");
  sprintf (buffer, "%02d", date.day());
  strcat(minPath, buffer);
  strcat(minPath, "_min.JSONL");       

  Serial.print("saving min values to: ");
  Serial.println(minPath);

}

// Splits and returns String array, ignores {} and "
void splitString(String text, String array[], char del) {
  byte y = 0;
  for(byte i = 0; i < text.length(); i++) {
    if (text.charAt(i) == del) {
      y++;
    } else if (!(text.charAt(i) == '{' || text.charAt(i) == '}' || text.charAt(i) == '"')) {
      array[y] += text.charAt(i);
    }
  }
}

// Computes monthly average from SD data
void monthlyAverage(int year, int month) {      // /YYYY/MM/YYYY-MM-    dd_avg.JSONL
  char path[60] = "";
  char intBuffer[10] = "";
  char tempPath[60] = "";
  strcpy(path, "/");
  sprintf (intBuffer, "%d", year);
  strcat(path, intBuffer);
  strcat(path, "/");
  sprintf (intBuffer, "%d", month);
  strcat(path, intBuffer);
  strcat(path, "/");
  sprintf (intBuffer, "%d", year);
  strcat(path, intBuffer);
  strcat(path, "-");
  sprintf (intBuffer, "%02d", month);
  strcat(path, intBuffer);
  strcat(path, "-");

  for(int i = 0; i < 7; i++) {   // Resets variables
    cReadings[i] = 0;
    nReadings[i] = 0;
  }  

  for(int i = 1; i < daysInMonth(year, month) + 1; i++) {
    strcpy(tempPath, path);
    sprintf (intBuffer, "%02d", i);
    strcat(tempPath, intBuffer);
    strcat(tempPath, "_avg.JSONL");

    if (SD.exists(tempPath)) {
      File file = SD.open(tempPath);
      if (!file) {
        Serial.println("Failed to open file for reading and computing monthly avg...");
        return;
      }
      String line = "init";
      while (!(line == "")) {
        String group[5];
        String individual[5][5];
        line = "";
        line = readLineSD(file);
        splitString(line, group, ',');
        for(int i = 0; i < 3; i++) {
          splitString(group[i],individual[i],':');
        }

        // Serial.print("Line read: ");
        // Serial.println(line);

        // Serial.print("Value at group 2: ");
        // Serial.println(group[1]);
        
        // Serial.print("Value at individual 3, 2: ");
        // Serial.println(individual[2][1]);

        for(int i = 0; i < 6; i++) {
          if (individual[1][1] == *variableName[i + 1]) {
            nReadings[i]++;
            cReadings[i] += atof (individual[2][1].c_str());
          }
        }
      }
      file.close();
    }
  }
  for(int i = 0; i < 7; i++) {
    if (nReadings[i] > 0) {
      *avgReadings[i] = cReadings[i]/nReadings[i];
      nReadings[i] = 0;
    } 
    // else {
    //   *avgReadings[i] = -1;
    // }
  } 

  Serial.print("Monthly averages computed..."); 



}

// Computes average from SD data
void dailyAverage(char* path) {
  File file = SD.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading and computing avg...");
    return;
  }
  String line = "init";
  

  for(int i = 0; i < 7; i++) {
    cReadings[i] = 0;
    nReadings[i] = 0;
  }  
  
  while (!(line == "")) {
    String group[5];
    String individual[5][5];
    line = "";
    line = readLineSD(file);
    splitString(line, group, ',');
    for(int i = 0; i < 3; i++) {
      splitString(group[i],individual[i],':');
    }

    // Serial.print("Line read: ");
    // Serial.println(line);

    // Serial.print("Value at group 2: ");
    // Serial.println(group[1]);
    
    // Serial.print("Value at individual 3, 2: ");
    // Serial.println(individual[2][1]);

    for(int i = 0; i < 7; i++) {
      if (individual[1][1] == *variableName[i + 1]) {
        nReadings[i]++;
        cReadings[i] += atof (individual[2][1].c_str());
      }
    }
    //esp_task_wdt_reset();
  }

  for(int i = 0; i < 7; i++) {
    if (nReadings[i] > 0) {
      *avgReadings[i] = cReadings[i]/nReadings[i];
      nReadings[i] = 0;
    } 
    // else {
    //   *avgReadings[i] = -1;
    // }
  }  
  


  Serial.print("Averages computed..."); 
  file.close();
}

// Returns data from specified sensor at a given frequency in String
String sensorData(String startTime, String endTime, String variable, String frequency) {      
  String dataString = "";
  String line = "init";
  //Serial.print("Trying to create big buffer...");
  //char bigBuffer[50400] = "";      // feels wrong
  //Serial.println("Done!");
  char pathBuffer[40] = "";
  char dBuffer[10] = "";
  
  //time_t selectedDate = startTime.toInt();
  bool coma = false;
  
  // Timestamp to date
  DateTime selectedDate = DateTime(strtoul(startTime.c_str(), NULL, 0));
  Serial.println(strtoul(startTime.c_str(), NULL, 0));
  
  int selectedDay = selectedDate.day();
  int selectedMonth = selectedDate.month();
  int selectedYear = selectedDate.year();
  
  strcpy(pathBuffer, "/");
  sprintf (dBuffer, "%d", selectedYear);
  strcat(pathBuffer, dBuffer);
  if (frequency != anual) {
    strcat(pathBuffer, "/");
    sprintf (dBuffer, "%d", selectedMonth);
    strcat(pathBuffer, dBuffer); 
    // Read daily avgs for selected month
  } else {    // "anual"
    strcat(pathBuffer, "/");
    sprintf (dBuffer, "%d", selectedYear);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    char yearPathBuffer[60] = "";
    strcpy(bigBuffer, "{ \"data\": [");
    //strcpy(bigBuffer, "[ ");
    coma = true;
    for(int i = 1; i < 13; i++) {
      esp_task_wdt_reset();
      strcpy(yearPathBuffer, pathBuffer);
      sprintf (dBuffer, "%02d", i);
      strcat(yearPathBuffer, dBuffer);
      strcat(yearPathBuffer, "_monthAvg.JSONL");

      if (SD.exists(yearPathBuffer)) {
        File file = SD.open(yearPathBuffer);
        if (!file) {
          Serial.println("Failed to open file to send anual data...");
          //return "";
        }
        //

        while (!(line == "")) {
          String group[5];
          String individual[5][5];
          line = "";
          line = readLineSD(file);
          splitString(line, group, ',');
          for(int i = 0; i < 3; i++) {
            splitString(group[i],individual[i],':');
          }

          if (individual[1][1] == variable) {   // Checks for selected variable
            if (!coma) {
              strcat(bigBuffer, ",");            // if there is no coma before new data, prints one
              coma = true;
            }
            strcat(bigBuffer, line.c_str());
            coma = false;
          }
          //esp_task_wdt_reset();
        }
        esp_task_wdt_reset();
        
        file.close();
      }
      
    }
    //strcat(bigBuffer, "]");
    strcat(bigBuffer, "]}");
    Serial.print("Big buffer anual: ");
    Serial.println(bigBuffer); 
  }
  

  // Fetch all values for a specified date
  if (frequency == daily) {
    strcat(pathBuffer, "/");
    sprintf (dBuffer, "%d", selectedYear);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    sprintf (dBuffer, "%02d", selectedMonth);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    sprintf (dBuffer, "%02d", selectedDay);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "_");
    strcat(pathBuffer, variable.c_str());
    strcat(pathBuffer, ".JSONL");

    Serial.println(pathBuffer);

    File file = SD.open(pathBuffer);
    if (!file) {
      Serial.println("Failed to open file to send daily data...");
      return "";
    }
    //strcpy(buffer, "{ \"data\": [");
    strcpy(bigBuffer, "{ \"data\": [");
    //strcpy(bigBuffer, "[ ");
    coma = true;
    while (!(line == "")) {
      String group[5];
      String individual[5][5];
      line = "";
      line = readLineSD(file);
      splitString(line, group, ',');
      for(int i = 0; i < 3; i++) {
        splitString(group[i],individual[i],':');
      }

      if (individual[1][1] == variable) {   // Checks for selected variable
        if (!coma) {
          strcat(bigBuffer, ",");            // if there is no coma before new data, prints one
          coma = true;
        }
        strcat(bigBuffer, line.c_str());
        coma = false;
      }
      esp_task_wdt_reset();
    }
    strcat(bigBuffer, "]}"); 
    //strcat(bigBuffer, "]"); 
    file.close();

  } else if (frequency == monthly) {

    strcat(pathBuffer, "/");
    sprintf (dBuffer, "%d", selectedYear);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    sprintf (dBuffer, "%02d", selectedMonth);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    //strcpy(bigBuffer, "[ ");
    strcpy(bigBuffer, "{ \"data\": [");
    coma = true;
    for(int i = 1; i < daysInMonth(selectedYear, selectedMonth) + 1; i++) {
      line = "init";
      esp_task_wdt_reset();
      char monthPathBuffer[60];
      char dayBuffer[10];
      strcpy(monthPathBuffer, pathBuffer);
      sprintf (dayBuffer, "%02d", i);
      strcat(monthPathBuffer, dayBuffer);
      strcat(monthPathBuffer, "_avg.JSONL");
      /////////////////////////////////////////////////////////////////////////////////////
      //Serial.println(monthPathBuffer);

      //if (selectedYear == date.year() && selectedMonth == date.month() && i == date.day() && SD.exists(monthPathBuffer)) deleteFileSD(SD, monthPathBuffer);  // Always compute current day avg
      
      // if (!SD.exists(monthPathBuffer)) {
      //   Serial.println("Failed to open file to read average data...");
      //   // Try to compute avg for selected day of the month, if fails save "" to response String
      //   //return "";
              
      //   for(int j = 0; j < 6; j++) {  
      //     char b[60] = ""; 
      //     strcpy(b, pathBuffer);
      //     strcat(b, dayBuffer);
      //     strcat(b, "_");
      //     strcat(b, variableName[j+1]->c_str());
      //     strcat(b, ".JSONL");
      //     Serial.print("computing file for: ");
      //     Serial.println(b);
      //     dailyAverage(b);
      //   }
      //   Serial.print("trying to get epoch time... ");
      //   unsigned long epochTimeTemp = getTimeEpoch(selectedYear, selectedMonth, i);
      //   Serial.println(epochTimeTemp);
      //   Serial.println("Trying to write new avg...");
      //   writeJsonlSDDebug(SD, monthPathBuffer, avgReadings, variableName, 6, epochTimeTemp);
        
      // }
      if (SD.exists(monthPathBuffer)) {
        File file = SD.open(monthPathBuffer);
        if (!file) {
          Serial.println("Failed to open file to send monthly data...");
          //return "";
        }
        //strcpy(buffer, "{ \"data\": [");

        while (!(line == "")) {
          String group[5];
          String individual[5][5];
          line = "";
          line = readLineSD(file);
          splitString(line, group, ',');
          for(int i = 0; i < 3; i++) {
            splitString(group[i],individual[i],':');
          }

          if (individual[1][1] == variable) {   // Checks for selected variable
            if (!coma) {
              strcat(bigBuffer, ",");            // if there is no coma before new data, prints one
              coma = true;
            }
            strcat(bigBuffer, line.c_str());
            coma = false;
          }
          //esp_task_wdt_reset();
        }
        esp_task_wdt_reset();
        
        file.close();
      }
    }
    strcat(bigBuffer, "]}"); 
    //strcat(bigBuffer, "]"); 
  }

  Serial.print("Data read...");
  //Serial.println(bigBuffer); 
  //esp_task_wdt_reset();
  return bigBuffer;
}

// Handles POST request
void handlePostRequest(String postName[], String postValue[], int requestLength) {
  bool requestFound = false;
  int index = 0;
  int caseSelect = -1;
  char buffer[100] = "";
  char nBuffer[10] = "";
  char boolBuffer[10] = "";

  while (!requestFound && index < requestLength) {
    if(postName[index] == PARAM_SEC) {

      requestFound = true;
      index += -1;
    }
    index++;
  }

  if(!requestFound) {
    Serial.println("Request not found!");
    return;
  }
  Serial.print("Post value: ");
  Serial.println(postValue[index]);
  
  if(postValue[index] == PARAM_CONFIG_0) caseSelect = 0;  //network
  if(postValue[index] == PARAM_CONFIG_1) caseSelect = 1;  //calibration
  if(postValue[index] == PARAM_CONFIG_2) caseSelect = 2;  //timing
  if(postValue[index] == PARAM_CONFIG_3) caseSelect = 3;  //alarms
  if(postValue[index] == PARAM_CONFIG_4) caseSelect = 4;  //server
  if(postValue[index] == PARAM_CONFIG_5) caseSelect = 5;  //auth
  if(postValue[index] == PARAM_CONFIG_6) caseSelect = 6;  //factoryRestore       
  
  // Saving parameters        
  switch(caseSelect) {
    case 0:
      for(int i = 0; i < requestLength; i++) {
        Serial.println(postName[i]);
        Serial.println(postValue[i]);
        if(postName[i] == *csvNetworkConfigName[0]) { //mode
          mode = postValue[i].c_str();
        }
        if(postName[i] == *csvNetworkConfigName[1]) { //ssid
          ssid = postValue[i].c_str();
        }
        if(postName[i] == *csvNetworkConfigName[2]) { //pass/////////////////////////////////////////////////////////////////////////////////////////////////
          pass = postValue[i].c_str();
          //encryptAES((unsigned const char*)eKey, pass.c_str(), passBuffer);
          if(pass == "") pass = "\"NULL\"";
        }
        if(postName[i] == *csvNetworkConfigName[3]) { //ip
          ip = postValue[i].c_str();
        }
        if(postName[i] == *csvNetworkConfigName[4]) { //gateway
          gateway = postValue[i].c_str();
        }
      }
      //decryptAES((unsigned const char*)eKey, eIv, passBuffer, tempMessage);
      //Serial.println(tempMessage);
      // sprintf (buffer, "%s,%s,%s,%s,%s", mode.c_str(), ssid.c_str(), pass.c_str(), ip.c_str(), gateway.c_str());
      // Serial.println(buffer);     
      // // LittleFS write
      // // writeFileFS(LittleFS, networkConfigPath, buffer);
      // // SD write
      // if (isInitSD) writeFileSD(SD, networkConfigPath, buffer);


      sprintf (buffer, "%s,%s,%s,%s,%s", mode.c_str(), ssid.c_str(), pass.c_str(), ip.c_str(), gateway.c_str());
      Serial.print("Write buffer: ");
      Serial.println(buffer);
      // Serial.print("Iv generated: ");
      // Serial.println(eIv);
      // LittleFS
      // writeFileFS(LittleFS, networkConfigPath, buffer);
      // writeFileFS(LittleFS, ivPathNetwork, eIv);
      if (isInitSD) {
        writeFileSD(SD, networkConfigPath, buffer);
        // writeFileSD(SD, ivPathNetwork, eIv);
      }

      should_restart = true;
      break;
    case 1:
      for(int i = 0; i < requestLength; i++) {
        if(postName[i] == *csvCalibrationConfigName[0]) { //solar
          solarRatioString = postValue[i].c_str();
        }
        if(postName[i] == *csvCalibrationConfigName[1]) { //precip
          precipRatioString = postValue[i].c_str();
        }
        if(postName[i] == *csvCalibrationConfigName[2]) { //airflow
          airflowRatioString = postValue[i].c_str();
        }
      }
      // Calibration config to float
      solarRatio = atof (solarRatioString.c_str());
      precipRatio = atof (precipRatioString.c_str());
      airflowRatio = atof (airflowRatioString.c_str());
      // Saving
      sprintf (buffer, "%s,%s,%s", solarRatioString.c_str(), precipRatioString.c_str(), airflowRatioString.c_str());
      Serial.println(buffer);     
      // LittleFS write
      // writeFileFS(LittleFS, calibrationConfigPath, buffer);
      // SD write
      if (isInitSD) writeFileSD(SD, calibrationConfigPath, buffer);
      break;
    case 2:
      for(int i = 0; i < requestLength; i++) {
        if(postName[i] == *csvTimeConfigName[0]) { //sample time
          sampleTimeString = postValue[i].c_str();
        }
        if(postName[i] == *csvTimeConfigName[1]) { //time zone
          gmtString = postValue[i].c_str();
        }
      }
      // String to int timings
      sampleTime = atoi(sampleTimeString.c_str()) * 60000;
      gmtOffset = atoi(gmtString.c_str());
      // Saving
      sprintf (buffer, "%s,%s", sampleTimeString.c_str(), gmtString.c_str());
      Serial.println(buffer);     
      // LittleFS write
      // writeFileFS(LittleFS, timeConfigPath, buffer);
      // SD write
      if (isInitSD) writeFileSD(SD, timeConfigPath, buffer);
      break;
    case 3:
      resetAlarms();
      // set alarms from request
      for(int i = 0; i < requestLength; i++) {
        Serial.print("Param name: ");
        Serial.println(postName[i]);
        Serial.print("Param value: ");
        Serial.println(postValue[i]);
        for(int j = 0; j < 6; j++) {
          if(postName[i] == *csvAlarmConfigName[j + 6]) {     // Checking for "variable_alto"
            Serial.print("Found value for: ");
            Serial.println(*variableName[j+1]);
            *csvAlarmConfig[j] = postValue[i - 1].c_str();
            *csvAlarmConfig[j + 6] = postValue[i].c_str();
            *minReadingsLimit[j] = atof(csvAlarmConfig[j]->c_str());    // Low value
            
            *maxReadingsLimit[j] = atof(csvAlarmConfig[j + 6]->c_str());        // High value
            
            Serial.println(*minReadingsLimit[j]);
            Serial.println(*maxReadingsLimit[j]);

            alarmEnabled[j] = true;
          }
        }
        esp_task_wdt_reset();
      }
      strcpy(buffer, "");
      strcpy(boolBuffer, "");
      for (int i = 0; i < 6; i++) {
        Serial.println(i);
        sprintf(nBuffer, "%.2f", *minReadingsLimit[i]);
        strcat(buffer, nBuffer);
        strcat(buffer, ",");
        
        const char* tempBuffer = alarmEnabled[i] ? "1" : "0";
        strcat(boolBuffer, tempBuffer);
        if(i < 5) {
          //strcat(buffer, ",");
          strcat(boolBuffer, ",");
        }
      }
      for (int i = 0; i < 6; i++) {
        sprintf(nBuffer, "%.2f", *maxReadingsLimit[i]);
        strcat(buffer, nBuffer);
        if(i < 5) strcat(buffer, ",");
      }
      if(isInitSD) {
        Serial.println(buffer);
        Serial.println(boolBuffer);
        writeFileSD(SD, alarmConfigPath, buffer);
        writeFileSD(SD, enabledAlarmConfigPath, boolBuffer);
      }
      break;
    case 4:
      for(int i = 0; i < requestLength; i++) {
        if(postName[i] == *csvServerConfigName[0]) { //url
          url = postValue[i].c_str();
        }
        if(postName[i] == *csvServerConfigName[1]) { //port
          port = postValue[i].c_str();
        }
      }
      // Saving
      sprintf (buffer, "%s,%s", url.c_str(), port.c_str());
      Serial.println(buffer);     
      // LittleFS write
      // writeFileFS(LittleFS, serverConfigPath, buffer);
      // SD write
      if (isInitSD) writeFileSD(SD, serverConfigPath, buffer);
      //should_restart = true;
      break;
    case 5:
      for(int i = 0; i < requestLength; i++) {
        if(postName[i] == *csvAuthConfigName[0]) { //admin password
          admin_pass = postValue[i].c_str();
        }
      }
      //LittleFS write
      // writeFileFS(LittleFS, authConfigPath, admin_pass.c_str());
      //SD write
      //if(isInitSD) writeFileSD(SD, authConfigPath, admin_pass.c_str()); 
      char buff[64];
      //String encryptedAdminPass = 
      encryptAES((unsigned const char*)eKey, admin_pass.c_str(), buff);
     
      // for (int j = 0; j < 64; j++) {
      //   buff[j] = encryptedAdminPass.c_str()[j];
      // }
      // LittleFS
      // writeFileFS(LittleFS, authConfigPath, encryptAES((unsigned const char*)eKey, admin_pass).c_str()); 
      // writeFileFS(LittleFS, ivPathAuth, eIv);
      if (isInitSD) {
        Serial.print("Encrypted message buffer: ");
        Serial.println(buff);
        writeCharArraySD(SD, authConfigPath, buff, 64); 
        writeCharArraySD(SD, ivPathAuth, eIv, 16);
      }

      break;
    case 6: // Factory reset
    resetConfig();
    factReset = true;
  }
}

// Sends POST request to server to dump data
void sendPostToDatabase(unsigned long timestamp, String* identifier[])  {   //Testing //////////////////////////////////////////////////////////////////////
  updateTime();

  String message;
  unsigned long tempTimestamp = timestamp;
  unsigned long tomorrowTimestamp = getTimeEpoch(0-gmtOffset) + 86400;
  String line = "init";
  bool coma = true;
  bool connOK = true;
  char buffer[20] = "";
  char dataPathDate[30] = "";
  char dataPath[60] = "";
  bool emptyMessage = true;
  // Timestamp to date
  DateTime lastInsertDate = DateTime(tempTimestamp);
 
  DateTime tomorrow = DateTime(tomorrowTimestamp);
  
  int lastInsertDay = lastInsertDate.day();
  int lastInsertMonth = lastInsertDate.month();
  int lastInsertYear = lastInsertDate.year();
  
  int currentDay = tomorrow.day();
  int currentMonth = tomorrow.month();
  int currentYear = tomorrow.year();
  Serial.println((String)"http://" + url + ":" + port + "/data");
  http.begin(client, (String)"http://" + url + ":" + port + "/data");
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = 500;

  //Troubleshooting
  Serial.println(lastInsertDay);
  Serial.println(lastInsertMonth);
  Serial.println(lastInsertYear);

  Serial.println(currentDay);
  Serial.println(currentMonth);
  Serial.println(currentYear);
  
  Serial.println(tempTimestamp);
  Serial.println(getTimeEpoch(0-gmtOffset));
  
  while ( ( (lastInsertDay != currentDay) || (lastInsertMonth != currentMonth) || (lastInsertYear != currentYear) ) && (tempTimestamp < tomorrowTimestamp) ) {
    // Prepare path
    strcpy(dataPathDate, "/");
    sprintf(buffer, "%d", lastInsertYear);
    strcat(dataPathDate, buffer);
    strcat(dataPathDate, "/");
    sprintf(buffer, "%d", lastInsertMonth);
    strcat(dataPathDate, buffer);
    strcat(dataPathDate, "/");
    sprintf(buffer, "%d", lastInsertYear);
    strcat(dataPathDate, buffer);
    strcat(dataPathDate, "-");
    sprintf(buffer, "%02d", lastInsertMonth);
    strcat(dataPathDate, buffer);
    strcat(dataPathDate, "-");
    sprintf(buffer, "%02d", lastInsertDay);
    strcat(dataPathDate, buffer);
    strcat(dataPathDate, "_");
    Serial.print("Date changed: ");
    Serial.println(dataPathDate);
    // Read data for each sensor
    for (int i = 0; i < 7; i++) {
      // strcpy(bigBuffer, "[");
      message = "[";
      strcpy(dataPath, dataPathDate);
      strcat(dataPath, identifier[i + 1]->c_str());
      strcat(dataPath, ".JSONL");
      File file = SD.open(dataPath);
      if (!file) {
        Serial.println("Failed to open file to fetch data...");
        //return "";
      } else {
        while (line != "") {
          String group[5];
          String individual[5][5];
          line = "";
          line = readLineSD(file);
          // Check for timestamp, don't send if it's lower than the last sent
          // Serial.println(line);

          splitString(line, group, ',');
          splitString(group[0],individual[0],':');

          // Serial.println(strtoul(individual[0][1].c_str(), NULL, 0));
          //Serial.println(timestamp - gmtOffset);

          if (strtoul(individual[0][1].c_str(), NULL, 0) >= (timestamp - gmtOffset)) {   // >= In case the value was not saved at the time 
            // if (!coma && line != "") strcat(bigBuffer, ",");
            // strcat(bigBuffer, line.c_str());
            if (!coma && line != "") message += ",";
            message += line;
            coma = false;
            emptyMessage = false;
            //Serial.println(message);
          }        
        }
      }
      line = "init";
      file.close();    
      // strcat(bigBuffer, "]");
      message += "]";
      // Serial.println(bigBuffer);
      Serial.println(message);
      coma = true;
      // Send data
      if(message != "[]"){
        httpResponseCode = http.POST(message);
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);   
        if(httpResponseCode < 200 || httpResponseCode > 399) {
        Serial.println("Exiting...");
        i = 8;
        connOK = false;
        }
      } else {
        emptyMessage = true;
      }
      

    }  
    // Save timestamp of insert if successful
    if (connOK) {
      if(isRTC) {
        lastInsertTimestamp = getEpochRTC(0);
      } else {
        lastInsertTimestamp = getTimeEpoch(0-gmtOffset);
      }
      sprintf(buffer, "%lu", lastInsertTimestamp);
      writeFileSD(SD, lastInsertPath, buffer);
      Serial.println("New timestamp saved");
    } else if(!emptyMessage) {
      http.end();
      Serial.println("Error sending to HTTP");
      return;
    }
    // +24h comparison timestamp
    tempTimestamp += 86400;
    lastInsertDate = DateTime(tempTimestamp);
  
    lastInsertDay = lastInsertDate.day();
    lastInsertMonth = lastInsertDate.month();
    lastInsertYear = lastInsertDate.year();
  }

  http.end();

}

// Sends POST using data in memory if no SD is detected
void sendPostBackup(String* identifier[]) {
  String message = "";
  char buffer[128];
  char tNameBuffer[20] = "{\"t\": \"";
  char bodyBuffer[40];
  char nBuffer[20];
  int dataSent = 0;
  int httpResponseCode = 500;
  bool connOK = true;

  Serial.println((String)"http://" + url + ":" + port + "/data");
  http.begin(client, (String)"http://" + url + ":" + port + "/data");
  http.addHeader("Content-Type", "application/json");

  for (int i = 0; i < 7; i++) {
    message = "[";
    strcpy(bodyBuffer, "\" ,\"variable\":\"");
    strcat(bodyBuffer, identifier[i + 1]->c_str());
    strcat(bodyBuffer, "\", \"valor\":\"");
    
    for (int j = 0; j < 20; j++) {
      if (backupTimestamp[j] > 0) {
        if (j > 0) message += ",";
        strcpy(buffer, tNameBuffer);
        sprintf(nBuffer, "%lu", backupTimestamp[j]);
        strcat(buffer, nBuffer);
        strcat(buffer, bodyBuffer);
        sprintf(nBuffer, "%.3f", backupValues[i][j]);
        strcat(buffer, "\"}");
        message += buffer;
        dataSent++;
      }
    }
    message += "]";
    Serial.println(message);
    httpResponseCode = http.POST(message);
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    if(httpResponseCode < 200 || httpResponseCode > 399) {
      connOK = false;
    }
  }
  // If all data is successfully sent, remove data from arrays
  if (connOK && dataSent < 20) {
    for (int i = 0; i < 20 - dataSent; i++) {
      backupTimestamp[i] = backupTimestamp[dataSent + i - 1];
      for (int j = 0; j < 7; j++) {
        backupValues[j][i] = backupValues[j][dataSent + i - 1];
      }
    }
  } else if (connOK) {
    // Defaults arrays
    for (int i = 0; i < 20; i++) {
      backupTimestamp[i] = 0;
      for (int j = 0; j < 7; j++) {
        backupValues[j][i] = -1;
      }
    }
  }

  http.end();
}

// Resets all alarms to default
void resetAlarms(){
  for (int i = 0; i < 6; i++) {
    *maxReadingsLimit[i] = 9999;
    *minReadingsLimit[i] = -100;
    alarmEnabled[i] = false;
  }
  Serial.println("Alarms reset...");
}

int daysInMonth(int selectedYear, int selectedMonth) {
  int rtnValue = 0;
  if (selectedMonth == 2) {
    if (selectedYear % 4 == 0 && (selectedYear % 100 != 0 || selectedYear % 400 == 0)) {
      rtnValue = 29;
    } else {
      rtnValue = 28;
    }
  } else if (selectedMonth == 4 || selectedMonth == 6 || selectedMonth == 9 || selectedMonth == 11) {
    rtnValue = 30;
  } else {
    rtnValue = 31;
  }
  return rtnValue;
}

// Checks sent admin password 
bool adminPasswordOK(String tryPass) {
  char encryptedPass[64];
  //String encryptedPass = "";
  Serial.println("Trying to encrypt response...");
  //encryptedPass = encryptAES((unsigned const char*)eKey, eIvAuth, tryPass.c_str());
  encryptAES((unsigned const char*)eKey, eIvAuth, tryPass.c_str(), encryptedPass);
  Serial.print("Encrypted saved password: ");
  Serial.println(admin_pass);
  Serial.print("Encrypted response: ");
  Serial.println(encryptedPass);
  Serial.print("Decrypted response: ");
  Serial.println(decryptAES((unsigned const char *)eKey, eIvAuth, (const char*)encryptedPass));
  Serial.print("Decrypted saved pass: ");
  Serial.println(decryptAES((unsigned const char *)eKey, eIvAuth, admin_pass.c_str()));
  return decryptAES((unsigned const char *)eKey, eIvAuth, admin_pass.c_str()) == tryPass;
}

// Tokens
// Generate a random token (32 characters)
String generateToken() {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  String token;
  for (int i = 0; i < 32; i++) {
    token += charset[esp_random() % (sizeof(charset) - 1)];
  }
  return token;
}

// Activate token, returns active token index
int activateToken() {
  updateTime();
  for (int i = 0; i < 5; i++) {
    if (tokenValue[i] == "" || tokenExpire[i] > getTimeEpoch(0)){
      tokenValue[i] = generateToken();
      tokenExpire[i] = getTimeEpoch(0) + (TOKEN_EXPIRE_MIN * 60);
      tokenActive[i] = true;
      return i;
    }
  }
  return -1;
}

// Checks if token is still valid, if not, invalidates it
bool isTokenValid(String tokenValueS) {
  updateTime();
  for (int i = 0; i < 5; i++) {
    // Serial.print("Current token check: ");
    // Serial.println(tokenValue[i]);
    if (tokenValue[i] == tokenValueS) {
      // Serial.print("Token exp time: ");
      // Serial.println(tokenExpire[i]);
      // Serial.println(getTimeEpoch(0));
      if(tokenExpire[i] < getTimeEpoch(0)){
        tokenValue[i] = "";
        tokenExpire[i] = 0;
        tokenActive[i] = false;
        // Serial.println("Token exp value was higher than current time...");
        return false;
      }
      // Serial.println("Returning true...");
      return true;
    }
  }
  return false;
}

// Invalidates token
void removeToken(String tokenValueS) {
  updateTime();
  for (int i = 0; i < 5; i++) {
    if (tokenValue[i] == tokenValueS) {
      tokenValue[i] = "";
      tokenExpire[i] = 0;
      tokenActive[i] = false;
    }
  }
}

// Token to JSON
String tokenJsonString(String token) {
  char rtn[50] = "";
  strcpy(rtn, "{\"token\": \"");
  strcat(rtn, token.c_str());
  strcat(rtn, "\"}");
  return rtn;
}

//testing
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}

// Interrupts
// Airflow interrupt
void IRAM_ATTR airflowISR() {
  airFlowSwitchTime = millis();
  if (airFlowSwitchTime - prevAirFlowSwitchTime > 100) {
    timesAirflowSwitch++;
    prevAirFlowSwitchTime = airFlowSwitchTime;
  }

}
// Precipitation interrupt
void IRAM_ATTR precipISR() {
  precipSwitchTime = millis();
  if (precipSwitchTime - prevPrecipSwitchTime > 100) {
    timesPrecipSwitch++;
    prevPrecipSwitchTime = precipSwitchTime;
  }
  
}

// RTC alarm
void IRAM_ATTR rtcISR() {
  alarmRTC = true;
}

// Encrypt
// Saves encrypted message in output char[]
void encryptAES(const unsigned char* key, const char* message, char* output) {
  // strcpy(tempMessage, message);
  for (int i = 0; i < 64; i++) {
    tempMessage[i] = message[i];
  }
  //memset(message, 0, sizeof(message));
  char iv[16];
  //int temp = esp_random();
  esp_fill_random(&iv, 16);
  // mbedtls_ctr_drbg_context ctr_drbg;
  // mbedtls_ctr_drbg_init(&ctr_drbg);
  // temp = mbedtls_ctr_drbg_random(&ctr_drbg, (unsigned char*)iv, sizeof(iv));
  // mbedtls_ctr_drbg_free(&ctr_drbg);

  Serial.print("Random iv generated: ");
  Serial.println(iv);
  // Saving generated iv in memory
  // strcpy(eIv, iv);
  for (int i = 0; i < 16; i++) {
    eIv[i] = iv[i];
  }
  Serial.println(eIv);
  esp_aes_context ctx;
  esp_aes_init(&ctx);
  esp_aes_setkey(&ctx, key, 128);
  esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, 64, (unsigned char*)iv, (uint8_t*)tempMessage, (uint8_t*)output);
  memset(tempMessage, 0, 64);
  esp_aes_free(&ctx);

  Serial.print("Message encrypted...");
  Serial.println(output);
}

// Saves encrypted message in output char[]
void encryptAES(const unsigned char* key, const char* iv, const char* message, char* output) {
  // strcpy(tempMessage, message);
  memset(tempMessage, 0, 64);
  for (int i = 0; i < 64; i++) {
    tempMessage[i] = message[i];
  }
  Serial.print("TempMessage: ");
  Serial.println(tempMessage);
  //memset(message, 0, sizeof(message));
  //char iv[16];
  //int temp = esp_random();
  //esp_fill_random(&iv, 16);
  // mbedtls_ctr_drbg_context ctr_drbg;
  // mbedtls_ctr_drbg_init(&ctr_drbg);
  // temp = mbedtls_ctr_drbg_random(&ctr_drbg, (unsigned char*)iv, sizeof(iv));
  // mbedtls_ctr_drbg_free(&ctr_drbg);

  Serial.print("Received IV, saved in eIv: ");
  //Serial.println(iv);
  // Saving generated iv in memory
  // strcpy(eIv, iv);
  for (int i = 0; i < 16; i++) {
    eIv[i] = iv[i];
  }
  Serial.println(eIv);
  esp_aes_context ctx;
  esp_aes_init(&ctx);
  esp_aes_setkey(&ctx, key, 128);
  esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, 64, (unsigned char*)eIv, (uint8_t*)tempMessage, (uint8_t*)output);
  memset(tempMessage, 0, 64);
  esp_aes_free(&ctx);

  Serial.print("Message encrypted...");
  Serial.println(output);
}

// Returns String for encrypted message 
String encryptAES(const unsigned char* key, const char* message) {
  // strcpy(tempMessage, message);
  for (int i = 0; i < 64; i++) {
    tempMessage[i] = message[i];
  }
  //memset(message, 0, sizeof(message));
  char iv[16];
  char output[64] = "";
  //int temp = esp_random();
  esp_fill_random(&iv, 16);
  // mbedtls_ctr_drbg_context ctr_drbg;
  // mbedtls_ctr_drbg_init(&ctr_drbg);
  // temp = mbedtls_ctr_drbg_random(&ctr_drbg, (unsigned char*)iv, sizeof(iv));
  // mbedtls_ctr_drbg_free(&ctr_drbg);

  Serial.print("Random iv generated: ");
  Serial.println(iv);
  // Saving generated iv in memory
  // strcpy(eIv, iv);
  for (int i = 0; i < 16; i++) {
    eIv[i] = iv[i];
  }
  Serial.println(eIv);
  esp_aes_context ctx;
  esp_aes_init(&ctx);
  esp_aes_setkey(&ctx, key, 128);
  esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, 64, (unsigned char*)eIv, (uint8_t*)tempMessage, (uint8_t*)output);
  memset(tempMessage, 0, 64);
  esp_aes_free(&ctx);

  Serial.print("Message encrypted...");
  Serial.println(output);
  return output;
}

// Returns encrypt with given iv
String encryptAES(const unsigned char* key, char* iv, const char* message) {
  // Serial.println("Trying to copy message...");
  memset(tempMessage, 0, 64);
  // strcpy(tempMessage, message);
  for (int i = 0; i < 64; i++) {
    tempMessage[i] = message[i];
  }
  Serial.print("Message copied...");
  Serial.println(tempMessage);
  //char copyIv[16] = "";
  char output[64] = "";
  // Serial.println("Trying to copy IV...");
  memset(eIv, 0, sizeof(eIv));
  // strcpy(eIv, iv);
  for (int i = 0; i < 16; i++) {
    eIv[i] = iv[i];
  }
  Serial.print("IV copied...");
  Serial.println(eIv);

  esp_aes_context ctx;
  esp_aes_init(&ctx);
  esp_aes_setkey(&ctx, key, 128);
  esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, 64, (unsigned char*)eIv, (uint8_t*)tempMessage, (uint8_t*)output);
  memset(tempMessage, 0, 64);
  esp_aes_free(&ctx);

  Serial.println("Message encrypted...");
  return output;
}

// Saves decrypted message in output[]
void decryptAES(const unsigned char* key, char* iv, const char* encrypted, char* output) {
  strcpy(encryptedMessage, (const char*)encrypted);
  //memset(encrypted, 0, enc)
  Serial.print("received IV: ");
  Serial.println(iv);
  //strcpy(eIv, iv);
  for (int i = 0; i < 16; i++) {
    eIv[i] = iv[i];
  }
  esp_aes_context ctx;
  esp_aes_init(&ctx);
  esp_aes_setkey(&ctx, key, 128);
  esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, sizeof(encryptedMessage), (unsigned char*)eIv, (uint8_t*)encryptedMessage, (uint8_t*)output);
  memset(encryptedMessage, 0, sizeof(encryptedMessage));
  esp_aes_free(&ctx);

  Serial.print("Message decrypted...");
  Serial.println(output);

}
// Returns String
String decryptAES(const unsigned char* key, char* iv, const char* encrypted) {
  strcpy(encryptedMessage, (const char*)encrypted);
  char output[64] = "";
  //memset(encrypted, 0, enc)
  Serial.print("received IV: ");
  Serial.println(iv);
  // strcpy(eIv, iv);
  for (int i = 0; i < 16; i++) {
    eIv[i] = iv[i];
  }
  Serial.println("iv copied to eIv");
  esp_aes_context ctx;
  esp_aes_init(&ctx);
  esp_aes_setkey(&ctx, key, 128);
  esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, sizeof(encryptedMessage), (unsigned char*)eIv, (uint8_t*)encryptedMessage, (uint8_t*)output);
  memset(encryptedMessage, 0, sizeof(encryptedMessage));
  esp_aes_free(&ctx);

  Serial.print("Message decrypted...");
  Serial.println(output);
  return output;
}

// Measures battery
int measureBattery() {
    return (int) (analogReadMilliVolts(BATTERYPIN) - 1500 * 1/6);
}

// Status bar String       eg: { "bateria" : "58" , "almacenamiento" : "true" }
String statusBar() {
  char buffer[100];
  char nBuffer[10];
  strcpy(buffer, "{ \"bateria\" : \"");
  sprintf(nBuffer, "%02d", measureBattery);
  strcat(buffer, nBuffer);
  strcat(buffer, "\" , \"almacenamiento\" : \"");
  // checks if SD path still exists, if not, SD is disconnected
  if (SD.exists("/config")) {
    strcat(buffer, "true\" }");
    isInitSD = true;
  } else {
    strcat(buffer, "false\" }");
    isInitSD = false;
  }

  Serial.print("Status bar message: ");
  Serial.println(buffer);
  return buffer;
}

// // File compression
// void compressFile(String year, String month, String day) {
//   String outputPath = year + "-" + month + "-" + day + ".zip";
//   Serial.println("Date values received: ");
//   Serial.println(year);
//   Serial.println(month);
//   Serial.println(day);

//   // zip writer init
//   Serial.print("Starting zip writer...");
//   mz_bool* zipWriter = mz_zip_writer_init(outputPath, 0);
//   if(!zipWriter) return;
//   Serial.println("zip writer OK");

//   // Adding files to the zip archive
//   for(int i = 0; i < 6; i++) {
//     String fileToAdd =  year + "-" + month + "-" + day + "_" + *variableName[i+1] + ".JSONL";
//     Serial.print("Trying to add file: ");
//     Serial.print()
//   }

// }


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  SPI.begin(18, 19, 23, 5); // CLK = PIN 18, MISO = PIN 19, MOSI = PIN 23, CS = PIN 5
  delay(200);
  // Set pins
  pinMode(ledPin, OUTPUT);


  digitalWrite(ledPin, LOW);


  // Inits
  delay(500);
  initSD(&isInitSD);
  dht.begin();
 
  if(!bmp.begin(0x76)) { 
    isBMP = false;
    Serial.println("BME init failed...");
  } else {
    isBMP = true;
    settingsBMP(&bmp);
  }
  initLittleFS();
  rtcInit(&isRTC);
  //tempDSB = initDSBTemp(oneWire);
  tempDSB.begin();
  

  // RTC Config
  rtc.disable32K();
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF);
  rtc.disableAlarm(2);

  // SD debug
  Serial.print("isInitSD: ");
  Serial.println(isInitSD);

  // Serial.println("SD directory");
  // listDirSD(SD, "/", 4);

  // LittleFS
  // listDirFS(LittleFS, "/", 0);
  // Serial.println("LittleFS directory");

  // Setting interrupts
  pinMode(AIRFLOWPIN, INPUT);
  pinMode(PRECIPPIN, INPUT);
  pinMode(RTCINTERRUPTPIN, INPUT_PULLUP);
  attachInterrupt(AIRFLOWPIN, airflowISR, FALLING);         // Might change later falling -> rising
  attachInterrupt(PRECIPPIN, precipISR, FALLING);
  attachInterrupt(RTCINTERRUPTPIN, rtcISR, FALLING);
  computeTime = millis();

  // Input pins
  pinMode(WINDNPIN, INPUT);
  pinMode(WINDSPIN, INPUT);
  pinMode(WINDEPIN, INPUT);
  pinMode(WINDWPIN, INPUT);
  pinMode(WINDNEPIN, INPUT);
  pinMode(WINDNWPIN, INPUT);
  pinMode(WINDSEPIN, INPUT);
  pinMode(WINDSWPIN, INPUT);

  pinMode(ADCPIN, INPUT);
  pinMode(BATTERYPIN, INPUT);

  batteryLevel = measureBattery();


  // SD Working indicator
  if (isInitSD) {
    int blinkC = 0;    
    if (!SD.exists("/config")) createDirSD(SD, "/config");
    if (!SD.exists("/misc")) createDirSD(SD, "/misc");
    while (blinkC<2) {
      blink();
      blinkC++;
    }
  }

  // Config reading
  if (isInitSD) {
    rawTextLine = readFileSD(SD, networkConfigPath);
    if (rawTextLine != "") parse_csv(csvNetworkConfig, &rawTextLine, ",", 5);
    rawTextLine = readMultipleLinesSD(SD, authConfigPath);
    if (rawTextLine != "") admin_pass = rawTextLine;
    rawTextLine = readFileSD(SD, serverConfigPath);
    if (rawTextLine != "") parse_csv(csvServerConfig, &rawTextLine, ",", 2);
    rawTextLine = readFileSD(SD, timeConfigPath);
    if (rawTextLine != "") parse_csv(csvTimeConfig, &rawTextLine, ",", 2);
    rawTextLine = readFileSD(SD, alarmConfigPath);
    if (rawTextLine != "") parse_csv(csvAlarmConfig, &rawTextLine, ",", 12);
    rawTextLine = readFileSD(SD, calibrationConfigPath);
    if (rawTextLine != "") parse_csv(csvCalibrationConfig, &rawTextLine, ",", 3);
    rawTextLine = readFileSD(SD, enabledAlarmConfigPath);
    if (rawTextLine != "") parse_csv(csvAlarmEnabled, &rawTextLine, ",", 6);
    // rawTextLine = readFileSD(SD, ivPathNetwork);
    // if (rawTextLine != "") strcpy(eIvNetwork, rawTextLine.c_str());
    rawTextLine = readMultipleLinesSD(SD, ivPathAuth);
    if (rawTextLine != "") strcpy(eIvAuth, rawTextLine.c_str());
    rawTextLine = readFileSD(SD, lastInsertPath);
    if (rawTextLine != "") lastInsertTimestamp =strtoul(rawTextLine.c_str(), NULL, 0);
  } else {
    rawTextLine = readFileFS(LittleFS, networkConfigPath);
    if (rawTextLine != "") parse_csv(csvNetworkConfig, &rawTextLine, ",", 5);
    rawTextLine = readMultipleLinesFS(LittleFS, authConfigPath);
    if (rawTextLine != "") admin_pass = rawTextLine;
    rawTextLine = readFileFS(LittleFS, serverConfigPath);
    if (rawTextLine != "") parse_csv(csvServerConfig, &rawTextLine, ",", 2);
    rawTextLine = readFileFS(LittleFS, timeConfigPath);
    if (rawTextLine != "") parse_csv(csvTimeConfig, &rawTextLine, ",", 2);
    rawTextLine = readFileFS(LittleFS, alarmConfigPath);
    if (rawTextLine != "") parse_csv(csvAlarmConfig, &rawTextLine, ",", 12);
    rawTextLine = readFileFS(LittleFS, calibrationConfigPath);
    if (rawTextLine != "") parse_csv(csvCalibrationConfig, &rawTextLine, ",", 3);
    rawTextLine = readFileFS(LittleFS, enabledAlarmConfigPath);
    if (rawTextLine != "") parse_csv(csvAlarmEnabled, &rawTextLine, ",", 6);
    // rawTextLine = readFileFS(LittleFS, ivPathNetwork);
    // if (rawTextLine != "") strcpy(eIvNetwork, rawTextLine.c_str());
    rawTextLine = readMultipleLinesFS(LittleFS, ivPathAuth);
    if (rawTextLine != "") strcpy(eIvAuth, rawTextLine.c_str());
    rawTextLine = readFileFS(LittleFS, lastInsertPath);
    if (rawTextLine != "") lastInsertTimestamp =strtoul(rawTextLine.c_str(), NULL, 0);
  }

  // String to int
  sampleTime = atoi(sampleTimeString.c_str()) * 60000;
  gmtOffset = atoi(gmtString.c_str());
    
  // Alarm String to float   
  for(int i = 0; i < 6; i++) {
    *minReadingsLimit[i] = atof (csvAlarmConfig[i]->c_str());
    *maxReadingsLimit[i] = atof (csvAlarmConfig[i + 6]->c_str());
    if (*csvAlarmEnabled[i] == "1") {
      alarmEnabled[i] = true;
      Serial.print("Alarm enabled index: ");
      Serial.println(i);
    }
    // Serial.print("Alarm limit low: ");
    // Serial.println(*csvAlarmConfig[i]);
    // Serial.println(*minReadingsLimit[i]);
    // Serial.print("Alarm limit high: ");
    // Serial.println(*csvAlarmConfig[i+6]);
    // Serial.println(*maxReadingsLimit[i]);
  }

  // Calibration config to float
  solarRatio = atof (solarRatioString.c_str());
  precipRatio = atof (precipRatioString.c_str());
  airflowRatio = atof (airflowRatioString.c_str());

  // AES Testing
  Serial.print("Decripted admin pass: ");
  Serial.println(admin_pass);
  Serial.println(decryptAES((unsigned const char*)eKey, eIvAuth, admin_pass.c_str()));
  String tempPass = decryptAES((unsigned const char*)eKey, eIvAuth, admin_pass.c_str());
  char tempPassBuffer[64];
  memset(tempPassBuffer, 0, 64);
  encryptAES((unsigned const char*)eKey, eIvAuth, tempPass.c_str(), tempPassBuffer);
  
  for (int j = 0; j < 64; j++) {
    admin_pass[j] = tempPassBuffer[j];
  }
  
  Serial.print("admin_pass re-encrypted to: ");
  Serial.println(admin_pass);


  ////////////////////////////////////////////////////////////////////////////////////////////
  if (mode == "") mode = "0";  //2 -> AP Mode, 1 -> STA Mode, 0 -> No config.

  Serial.println(mode);
  Serial.println(ssid);  
  // DecryptPassword
  Serial.println(pass);
  //  Serial.println(ip);
  Serial.println(gateway);

  // HTTP Methods
  if(mode == "1" || mode == "2") {
    //WebSocket
    initWS();
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/mainUI/index.html", "text/html"); // main UI loading
    });
    server.serveStatic("/", LittleFS, "/");

    server.on("/fechas", HTTP_GET, [](AsyncWebServerRequest* request) {
      Serial.println("Trying to send dates...");
      updateTime();
      oldestDate();
      Serial.println("Oldest date fetch!");
      request->send(200, "application/json", oldestDateString());
    });

    server.on("/datos", HTTP_GET, [](AsyncWebServerRequest* request) {
      int params = request->params();

      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->name() == PARAM_DATA_0) {
          start = p->value().c_str();
          Serial.print("Start time set to: ");
          Serial.println(start);
        }
        // HTTP GET end time
        //end = "0";
        if (p->name() == PARAM_DATA_1) {
          end = p->value().c_str();
          Serial.print("End time set to: ");
          Serial.println(end);
        }
        // HTTP GET sensor name
        if (p->name() == PARAM_DATA_2) {
          sensor = p->value().c_str();
          Serial.print("Sensor set to: ");
          Serial.println(sensor);
        }
        // HTTP GET frequency
        if (p->name() == PARAM_DATA_3) {
          frequency = p->value().c_str();
          Serial.print("Frequency: ");
          Serial.println(frequency);
        }
      }

      request->send(200, "application/json", sensorData(start, end, sensor, frequency));
      //esp_task_wdt_reset();
    });

    server.on("/datos/descargar", HTTP_GET, [](AsyncWebServerRequest* request) {
      int params = request->params();
      String downloadPath = "";

      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->name() == PARAM_DL_0) {
          yearDL = p->value().c_str();
          Serial.print("Year: ");
          Serial.println(yearDL);
          isParamDL[0] = true;
        }
        // HTTP GET end time
        if (p->name() == PARAM_DL_1) {
          monthDL = p->value().c_str();
          monthIntDL = atoi(monthDL);
          Serial.print("Month: ");
          Serial.println(monthDL);
          isParamDL[1] = true;
        }
        // HTTP GET sensor name
        if (p->name() == PARAM_DL_2) {
          dayDL = p->value().c_str();
          dayIntDL = atoi(dayDL);
          Serial.print("Day: ");
          Serial.println(dayDL);
          isParamDL[2] = true;
        }
        // HTTP GET frequency
        if (p->name() == PARAM_DL_3) {
          varDL = p->value().c_str();
          Serial.print("Variable selected: ");
          Serial.println(varDL);
          isParamDL[3] = true;
        }
      }

      // Day
      if(isParamDL[2]) {
        downloadPath = "/" + yearDL + "/" + monthIntDL + "/" + yearDL + "-" + monthDL + "-" + dayDL + "_" + varDL + ".JSONL";
      }
      // Month
      if(isParamDL[1] && !isParamDL[2]) {
        downloadPath = "/" + yearDL + "/" + yearDL + "-" + monthDL + "_monthAvg.JSONL";
      }
      // Year
      if(!isParamDL[1]) {
        Serial.println("Year averages are not saved...");
        downloadPath = "";
      }

      if(downloadPath = "") {
        request->send(401, "plain/text", "File not found");
      } else {
        request->send(SD, downloadPath, "text/text", true);
      }
    });

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest* request) {
      Serial.println("Trying to send config data for: ");
      int params = request->params();
      String pVal = "";
      // const AsyncWebParameter* p = request->getParam(params);
      // const char* pVal = p->value().c_str();
      // Serial.print("Parameter received: ");
      // Serial.println(pVal);

      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->name() == PARAM_SEC) {
          pVal = p->value().c_str();
          Serial.print("Seccion: ");
          Serial.println(pVal);
        }
      }
      // Header checking for token
      int headers = request->headers();
      String tempBuff = "";
      String token = "";
      for (int i = 0; i < headers; i++) {
        const AsyncWebHeader* h = request->getHeader(i);
        //if(h->name().c_str()=="ESPSESSIONID"){
        Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
        
        tempBuff = h->name().c_str();
        //Serial.println(tempBuff);
        if (tempBuff == "Authorization") {
          //strcpy(tempBuffer, h->value().c_str());
          Serial.println("Found auth header...");
          tempBuff = h->value().c_str();
          //Serial.println(tempBuff);
          tempBuff.replace("Bearer ","");
          token = tempBuff.c_str();
          Serial.println(token);
        }
        //}
      }
      if(isTokenValid(token)) {        // Enable on ui implementation
      //if(1) {
        if (pVal == PARAM_CONFIG_0) { // network
          request->send(200, "application/json", arrayToJsonString(csvNetworkConfigName, csvNetworkConfig, 5));/////////////////////////////////////////////////////////////////////
        }
        if (pVal == PARAM_CONFIG_1) { // calibration
          request->send(200, "application/json", arrayToJsonString(csvCalibrationConfigName, csvCalibrationConfig, 3));
        }
        if (pVal == PARAM_CONFIG_2) { // sampling
          request->send(200, "application/json", arrayToJsonString(csvTimeConfigName, csvTimeConfig, 2));
        }
        if (pVal == PARAM_CONFIG_3) { // alarms
          request->send(200, "application/json", alarmToJsonString());
        }
        if (pVal == PARAM_CONFIG_4) { // server
          request->send(200, "application/json", arrayToJsonString(csvServerConfigName, csvServerConfig, 2));
        }
        if (pVal == PARAM_CONFIG_5) { // auth
          request->send(200, "application/json", arrayToJsonString(csvAuthConfigName, csvAuthConfig, 1));
        }
        //request->send(200, "text/plain", "Done.");
      } else {
        Serial.println("Token has expired");
        request->send(401, "text/plain", "Invalid token.");
      }      

    });

    server.on("/config", HTTP_POST, [](AsyncWebServerRequest* request) {
      Serial.println("POST request received");
      int params = request->params();
      char buffer[50] = "";
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          postRequestName[i] = p->name().c_str();
          postRequestValue[i] = p->value().c_str();
        }
      }
      // Header checking for token
      int headers = request->headers();
      String tempBuff = "";
      String token = "";
      for (int i = 0; i < headers; i++) {
        const AsyncWebHeader* h = request->getHeader(i);
        //if(h->name().c_str()=="ESPSESSIONID"){
        Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
        
        tempBuff = h->name().c_str();
        //Serial.println(tempBuff);
        if (tempBuff == "Authorization") {
          //strcpy(tempBuffer, h->value().c_str());
          Serial.println("Found auth header...");
          tempBuff = h->value().c_str();
          //Serial.println(tempBuff);
          tempBuff.replace("Bearer ","");
          token = tempBuff.c_str();
          Serial.println(token);
        }
        //}
      }
      if(isTokenValid(token)) {
        handlePostRequest(postRequestName, postRequestValue, params);
        request->send(200, "text/plain", "Done.");
      } else {
        Serial.println("Token has expired");
        request->send(401, "text/plain", "Invalid token.");
      }

      
      //should_restart = true;
    });
    
    server.on("/autenticar", HTTP_POST, [](AsyncWebServerRequest* request) {   ///Change to POST////////////////////////////////////////////////////////////////////////////////////////
      Serial.println("POST request received");
      int params = request->params();
      String tryPass = "";
      int tokenIndex = -1;
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          if (p->name() == PARAM_INPUT_5) {
            tryPass = p->value().c_str();
            Serial.print("received pass is: ");
            Serial.println(tryPass);
          }
        }
      }
      if (adminPasswordOK(tryPass)) { ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      // if (true) {     //Testing
        tokenIndex = activateToken();
        if(tokenIndex > -1) {
          Serial.println("Token activated: ");
          Serial.print(tokenValue[tokenIndex]);
          //request->send(200, "application/json", tokenValue[tokenIndex]);
          AsyncWebServerResponse *response = request->beginResponse(200, "application/json", tokenJsonString(tokenValue[tokenIndex]));
          // char buffer[50] = "Bearer ";
          // strcat(buffer, tokenValue[tokenIndex].c_str());
          //response->addHeader("Authorization", buffer);
	        request->send(response);

        } else {
          request->send(400, "text/plain", "No tokens available.");
        }
      } else {
        request->send(401, "text/plain", "Wrong password.");
      }
      //request->send(400, "text/plain", "Wrong password / invalid format");

    });
  }


  // Mode = 2 AP MODE, Mode = 1 STATION MODE
  if (mode == "1" && initWiFi()) {
    /*
    /
    /
    */
    //STATION Mode

    // Time config (UTC)
    configTime(gmtOffset, 0, ntpServer, ntpServer1);
    delay(5000);
    Serial.println("Time tried to be set...");
    int timeC = 0;
    struct tm timeinfoControl;
    while (!getLocalTime(&timeinfoControl) && timeC < 10) {
      delay(1000);
      timeC++;
      if (timeC == 10) Serial.println("Time sync timed out...");
    }
    if (isRTC && !getLocalTime(&timeinfoControl)) manualTimeSet(getEpochRTC(gmtOffset));
    
    printLocalTime();
    getLocalTime(&tmstruct);
    if (isRTC && tmstruct.tm_year+1900 >= rtc.now().year()) rtcAdjust(DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec)); 
    
    server.begin();


  } else if (mode == "2" || !initWiFi()) {
    /*
    /
    /
    */
    // AP Mode
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point

    IPAddress apIP(192,168,0,1);
    IPAddress apGateway(192,168,0,1);
    WiFi.softAPConfig(apIP, apGateway, subnet);
    Serial.print("AP password: ");
    Serial.println(pass);
    if (pass != "\"NULL\"") {
      WiFi.softAP(ssid.c_str(), pass.c_str()); 
    } else {
      WiFi.softAP(ssid.c_str(), NULL);
      pass = "";
      ip = "192.168.0.1";
      gateway = "192.168.0.1";
    }
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Set time from rtc
    if (isRTC) {
      manualTimeSet(getEpochRTC(gmtOffset));
    }
    server.begin();

  } else {        // mode = 0
    /*
    /   
    /
    */
    //"No config" AP mode
    Serial.println("Setting AP (Access Point) config mode");
    // NULL sets an open Access Point on 192.168.0.1   
    
    IPAddress apIP(192,168,0,1);
    IPAddress apGateway(192,168,0,1);
    WiFi.softAPConfig(apIP, apGateway, subnet);
    WiFi.softAP("Estación Meteorológica IoT", NULL);

    // Set time from rtc
    if (isRTC) manualTimeSet(getEpochRTC(gmtOffset));

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);



    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/setup/setup.html", "text/html");
    });

    server.serveStatic("/", LittleFS, "/");


    // isPost to request params from server
    server.on("/", HTTP_POST, [](AsyncWebServerRequest* request) {
      Serial.println("POST request received");
      int params = request->params();
      char buffer[512] = "";
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          if (p->name() == PARAM_INPUT_0) {
            mode = p->value().c_str();
            Serial.print("Mode set to: ");
            if(mode == "0") mode = "2";
            Serial.println(mode);
          }
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
          }

          if (p->name() == PARAM_INPUT_5) {
            admin_pass = p->value().c_str();
            Serial.print("Admin pass set to: ");
            Serial.println(admin_pass);
          }
        }
      }
      if (mode != "2" && mode != "1") {
        //Error, mode needs to be set to either 1 or 2
        return request->send(400, "text/plain", "Bad request, wrong mode");
      } else if (ssid == "" || admin_pass == "") {
        //Error, SSID and admin pass needs to be set
        return request->send(400, "text/plain", "Bad request, SSID or admin pass not set");
      } else {
        //Request OK, save config
        sprintf (buffer, "%s,%s,%s,%s,%s", mode.c_str(), ssid.c_str(), pass.c_str(), ip.c_str(), gateway.c_str());
        Serial.print("Write buffer: ");
        Serial.println(buffer);
        // Serial.print("Iv generated: ");
        // Serial.println(eIv);
        // Credentials save
        //writeFileFS(LittleFS, networkConfigPath, buffer);
        // writeFileFS(LittleFS, ivPathNetwork, eIv);
        if (isInitSD) {
          writeFileSD(SD, networkConfigPath, buffer);
          // writeFileSD(SD, ivPathNetwork, eIv);
        }
        // Admin pass save
        char buff[64];
        //String encryptedAdminPass = 
        encryptAES((unsigned const char*)eKey, admin_pass.c_str(), buff);
        
        // for (int j = 0; j < 64; j++) {
        // buff[j] = encryptedAdminPass.c_str()[j];
        // }

        //writeFileFS(LittleFS, authConfigPath, encryptedAdminPass.c_str()); 
        writeCharArrayFS(LittleFS, authConfigPath, buff, 64);
        writeCharArrayFS(LittleFS, ivPathAuth, eIv, 16);
        if (isInitSD) {
          writeCharArraySD(SD, authConfigPath, buff, 64); 
          writeCharArraySD(SD, ivPathAuth, eIv, 16);
        }
      }
      should_restart = true;
      request->send(200, "text/plain", "Done. Restarting with new settings.");
      
    });
    server.begin();
  }

  // Creating paths
  updateTime();
  changeDatePath();
  changeLogsPath();
  changeAvgPath();
  changeMonthAvgPath();
  changeMaxPath();
  changeMinPath();
  currentDay = date.day();
  pastYear = date.year();
  pastMonth = date.month();
  pastDay = date.day();

  // Info
  Serial.print("Current sample time: ");
  Serial.println(sampleTime);
  
  // Setting RTC alarm
  rtc.setAlarm1(DateTime(0,0,0,0,0,1), DS3231_A1_PerSecond);
  alarmRTC = false;
  rtc.clearAlarm(1);
  Serial.println(alarmRTC);

  // Compute today's average before taking data
  for(int i = 0; i < 6; i++) {
    char b[60] = "";
    strcpy(b, datePath);
    strcat(b, "/");
    currentDate();
    strcat(b, currentDateChar);
    strcat(b, "_");
    strcat(b, variableName[i+1]->c_str());
    strcat(b, ".JSONL");
    dailyAverage(b);
  }
  if (SD.exists(avgPath)) deleteFileSD(SD, avgPath);       // File was used to compute this month's values'
  writeJsonlSD(SD, avgPath, avgReadings, variableName, 6, getTimeEpoch(date.year(), date.month(), date.day()));

  monthlyAverage(date.year(), date.month());
  if (SD.exists(monthAvgPath)) deleteFileSD(SD, monthAvgPath);       // File was used to compute this year's values'
  writeJsonlSD(SD, monthAvgPath, avgReadings, variableName, 6, getTimeEpoch(date.year(), date.month(), 1));
  
  // Dumping to database on startup
  Serial.print("Trying to send readings to database...");
  if(isInitSD) sendPostToDatabase(lastInsertTimestamp, variableName);
  
  // Compressor init
  // TampConf compConf = {.window = 10, .literal = 8, .use_custom_dictionary = false};
  // tamp_compressor_init(&compressor, &compConf, winBuffer);
  



  Serial.println("Server ready!");
}

void loop() {

  // Wireless checks
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection lost, trying to reconnect...");
    unsigned long currentMillisDC = millis();
    unsigned long prevMillisDC = 0;

    if(reconnectCounter < 10 && (currentMillisDC - prevMillisDC >= reconnectInt)) {
      WiFi.disconnect();
      WiFi.reconnect();
      reconnectCounter++;
    }

    if reconnect fails, start AP, try to reconnect every hour or so
    if(reconnectCounter >= 10) {
      Serial.println("failed to reconnect over 10 times, try every hour to reconnect");
      reconnectInt = 1800000;
      server.end();
      WiFi.disconnect();
      
      if(!initWiFi()) {

        // AP Mode
        Serial.println("Setting AP (Access Point)");
        // NULL sets an open Access Point

        IPAddress apIP(192,168,0,1);
        IPAddress apGateway(192,168,0,1);
        WiFi.softAPConfig(apIP, apGateway, subnet);
        Serial.print("AP password: ");
        Serial.println(pass);
        if (pass != "\"NULL\"") {
          WiFi.softAP(ssid.c_str(), pass.c_str()); 
        } else {
          WiFi.softAP(ssid.c_str(), NULL);
          pass = "";
          ip = "192.168.0.1";
          gateway = "192.168.0.1";
        }
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP);
      }

    }
  }

  if (should_restart) {
    Serial.println("Rebooting...");
    server.end();
    // Saving all config to LittleFS on successful reboot
    char buffer[100];
    char boolBuffer[20];
    char nBuffer[20];
    // Network
    sprintf (buffer, "%s,%s,%s,%s,%s", mode.c_str(), ssid.c_str(), pass.c_str(), ip.c_str(), gateway.c_str());
    writeFileFS(LittleFS, networkConfigPath, buffer);
    // Calibration
    sprintf (buffer, "%s,%s,%s", solarRatioString.c_str(), precipRatioString.c_str(), airflowRatioString.c_str());
    writeFileFS(LittleFS, calibrationConfigPath, buffer);
    // Time config
    sprintf (buffer, "%s,%s", sampleTimeString.c_str(), gmtString.c_str());
    writeFileFS(LittleFS, timeConfigPath, buffer);
    // Alarms
    strcpy(buffer, "");
    strcpy(boolBuffer, "");
    for (int i = 0; i < 6; i++) {
      Serial.println(i);
      sprintf(nBuffer, "%.2f", *minReadingsLimit[i]);
      strcat(buffer, nBuffer);
      strcat(buffer, ",");
      const char* tempBuffer = alarmEnabled[i] ? "1" : "0";
      strcat(boolBuffer, tempBuffer);
      if(i < 5) strcat(boolBuffer, ",");
    }
    for (int i = 0; i < 6; i++) {
      sprintf(nBuffer, "%.2f", *maxReadingsLimit[i]);
      strcat(buffer, nBuffer);
      if(i < 5) strcat(buffer, ",");
    }
    writeFileFS(LittleFS, alarmConfigPath, buffer);
    writeFileFS(LittleFS, enabledAlarmConfigPath, boolBuffer);
    // Server
    sprintf (buffer, "%s,%s", url.c_str(), port.c_str());
    writeFileFS(LittleFS, serverConfigPath, buffer);

    delay(100);
    // digitalWrite(RESETPIN, LOW);
    ESP.restart();
  }

  if (factReset) {
    Serial.println("Settings set to default, rebooting...");
    server.end();
    delay(100);
    // digitalWrite(RESETPIN, LOW);
    ESP.restart();
  }

  // if (alarmRTC) { 
  //   Serial.println("flag true....");
  // } else {
  //   Serial.println("flag false!");
  // }

  while ((isRTC && alarmRTC) || !isRTC) {
    if (!isRTC) delay(baseTime);   // Backup timer
    // Detach interrupts for comms use
    detachInterrupt(AIRFLOWPIN);
    detachInterrupt(PRECIPPIN);

    updateTime();
    //Serial.println("Server on...");
    //solarIrr = readSolarIrr(ADCPIN, 1);

    epochTime_2 = epochTime_1;        // Past time for computing? most likely remove
    if (isRTC) {                       // Prefer ext RTC
      epochTime_1 = getEpochRTC(gmtOffset);    // Current time in UTC
    } else {
      epochTime_1 = getTimeEpoch(0);     // Current time 
    }

    // Status bar, battery + sd connected
    statusC ++;
    if(statusC >= 30) {
      sendMessageWS(statusBar());
      statusC = 0;
    } 

    // Reading sensors
    //tempDHT = readTemp();
    humid = readHumidDHT(dht);
    temp = readDSBTemp(tempDSB);
    solar = readPower(ADCPIN, solarRatio);
    airflow = readAirflow(ptrTimesAirflowSwitch, computeTime, airflowRatio);
    windDir = readWindDir(WINDNPIN, WINDSPIN, WINDEPIN, WINDWPIN, WINDNEPIN, WINDNWPIN, WINDSEPIN, WINDSWPIN);
    precip = readPrecip(ptrTimesPrecipSwitch, computeTime, precipRatio);
    if (isBMP) pressure = readPressure(&bmp);
    // WebSocket sending readings
    sendMessageWS(currentReadingsString(readings, variableName, 7, epochTime_1));

    for(int i = 0; i < 6; i++) {
      if (*readings[i] > *maxReadings[i]) *maxReadings[i] = *readings[i];
      if (*readings[i] < *minReadings[i]) *minReadings[i] = *readings[i];
      // Alarm checking
      if ((*readings[i] > *maxReadingsLimit[i]) && alarmEnabled[i]) {
        //Send alarm High
        
        
        if(!activeAlarm[i + 6]) startAlarmTime[i + 6] = epochTime_1;
        activeAlarm[i + 6] = true; 
        sensorAlarm = true;
        sendMessageWS(alarmString(readings, variableName, false));
      }
      if ((*readings[i] < *minReadingsLimit[i]) && alarmEnabled[i]) {
        //Send alarm Low
        sendMessageWS(alarmString(readings, variableName, false));
        
        if(!activeAlarm[i]) startAlarmTime[i] = epochTime_1;
        activeAlarm[i] = true;
        sensorAlarm = true;
      }
      //Serial.println("Stopped checking...");
    }

    // Checking if value is still is alarm state
    if (sensorAlarm) {
      for(int i = 0; i < 6; i++) {
        //Serial.println("Checking alarm low");
        if (*readings[i] < alarmMaxValue[i]) alarmMaxValue[i] = *readings[i];
        if (activeAlarm[i] && (*readings[i] > *minReadingsLimit[i])) {
          endAlarmTime[i] = epochTime_1;
          saveAlarm(variableName, i);
          sendMessageWS(alarmString(readings, variableName, true));
          sendMessageWS(histAlarmUpd());
          activeAlarm[i] = false;
          sensorAlarm = false;
        }
      }
      for(int i = 6; i < 12; i++) { // might be a bit inefficient
        //Serial.println("Checking alarm high");
        if (*readings[i - 6] > alarmMaxValue[i]) alarmMaxValue[i] = *readings[i - 6];
        if (activeAlarm[i] && (*readings[i - 6] < *maxReadingsLimit[i - 6])) {
          endAlarmTime[i] = epochTime_1;
          saveAlarm(variableName, i);
          sendMessageWS(alarmString(readings, variableName, true));
          sendMessageWS(histAlarmUpd());
          activeAlarm[i] = false;
          sensorAlarm = false;
        }
      }
    }

    // Timing control
    realTimeC += baseTime;         // Adding +1s
    databaseC += baseTime;

    // Saving readings 
    if ((realTimeC >= sampleTime) && isInitSD) {
      writeJsonlSD(SD, logsPath, readings, variableName, 7, epochTime_1);
      realTimeC = 0;

      
      // Save in separate files
      currentDate();
      for(int i = 0; i < 7; i++) {
        char b[60] = "";
        //char bn[20] = "";
        strcpy(b, datePath);
        strcat(b, "/");
        
        strcat(b, currentDateChar);
        strcat(b, "_");
        // Serial.print("i: ");
        // Serial.println(i);
        // Serial.print("Variable name array: ");
        // Serial.println(variableName[i+1]->c_str());
        strcat(b, variableName[i + 1]->c_str());
        //strcpy(bn, *variableName[i+1]->toCharArray(bn, 20));
        //*variableName[i+1]->toCharArray(bn, 20);
        //strcat(b, bn);
        strcat(b, ".JSONL");
        //Serial.println(b);
        writeSingleSensorSD(SD, b, *readings[i], *variableName[i + 1], epochTime_1);
      }


      // Serial.println("RTC time: ");
      // rtcPrintTime();
      // Serial.println("Local time: ");
      // printLocalTime();
      
      // Serial.print("Current sample time: ");
      // Serial.println(sampleTime);

      // Compute averages to keep updated data
      for(int i = 0; i < 6; i++) {
        char b[60] = "";
        strcpy(b, datePath);
        strcat(b, "/");
        currentDate();
        strcat(b, currentDateChar);
        strcat(b, "_");
        strcat(b, variableName[i+1]->c_str());
        strcat(b, ".JSONL");
        dailyAverage(b);
      }
      if (SD.exists(avgPath)) deleteFileSD(SD, avgPath);       // File was used to compute this month's values'
      writeJsonlSD(SD, avgPath, avgReadings, variableName, 6, getTimeEpoch(date.year(), date.month(), date.day()));

      monthlyAverage(date.year(), date.month());
      if (SD.exists(monthAvgPath)) deleteFileSD(SD, monthAvgPath);       // File was used to compute this year's values'
      writeJsonlSD(SD, monthAvgPath, avgReadings, variableName, 6, getTimeEpoch(date.year(), date.month(), 1));

    } else if (realTimeC >= sampleTime) {   // No SD found, update backup arrays
      int flag = 0;
      for (int i = 0; i < 20; i++) {
        if(backupTimestamp[i] == 0) {
          backupTimestamp[i] = epochTime_1;
          for (int j = 0; j < 7; j++) {
            backupValues[i][j] = *readings[j];
          }
          i = 21;
        }
        flag++;
      }
      if(flag == 20) {  // No free array, remove oldest data
        for (int i = 0; i < 19; i++) {
          backupTimestamp[i] = backupTimestamp[i + 1];
          for (int j = 0; j < 7; j++) {
            backupValues[j][i] = backupValues[j][i + 1];
          }
        }
        backupTimestamp[19] = epochTime_1;
        for (int i = 0 ; i < 7; i++) {
          backupValues[i][19] = *readings[i];
        }
      }
      
    }
    
    // Sending data to server
    if ((databaseC >= databaseTimer) && isInitSD) {
      databaseC = 0;
      sendPostToDatabase(lastInsertTimestamp, variableName);
    } else if (databaseC >= databaseTimer) {
      sendPostBackup(variableName);
    }

    // //Calibration
    // saveSolarIrr(SD, "/solarCalibration.txt", epochTime_1, ADCPIN, 1.0);

    // if (analogReadMilliVolts(ADCPIN)>3150) {
    //   digitalWrite(ledPin, HIGH);
    //   Serial.println("ADC pin is too high!");
    //   appendFileSD(SD, "/solarCalibration.txt", "Pin was too high!");
    // } else {
    //   Serial.println("ADC pin low...");
    // }


    //Serial.println("local time: ");
    //printLocalTime();

    // getLocalTime(&tmstruct);
    //DateTime date = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
    // //Serial.println(date);
    // Serial.println(date.hour());
    // Serial.println(date.minute());

    //////////
    // dailyAverage();
    // writeJsonlSD(SD, avgPath, avgReadings, variables, 8, 0);


    // time is close to midnight, change path + compute averages
    //if ((date.hour() == lowerHourLimit || date.hour() == upperHourLimit) && (date.minute() > lowerMinLimit || date.minute() < upperMinLimit)) {
    if (date.day() != currentDay) {
      // Compute with data from previous day
      pastDate(pastYear, pastMonth, pastDay);
      for(int i = 0; i < 6; i++) {
        char b[60] = "";
        strcpy(b, datePath);
        strcat(b, "/");
        currentDate();
        strcat(b, olderDateChar);
        strcat(b, "_");
        strcat(b, variableName[i+1]->c_str());
        strcat(b, ".JSONL");
        dailyAverage(b);
      }
      if (SD.exists(avgPath)) deleteFileSD(SD, avgPath);       // File was used to compute this month's values'
      writeJsonlSD(SD, avgPath, avgReadings, variableName, 6, getTimeEpoch(pastYear, pastMonth, pastDay));
      monthlyAverage(pastYear, pastMonth); ////////////////////////////////////////////////////check if it's another month? and year, 2 more var assign current computing month-year and compare to actual values
      if (SD.exists(monthAvgPath)) deleteFileSD(SD, monthAvgPath);       // File was used to compute this year's values'
      writeJsonlSD(SD, monthAvgPath, avgReadings, variableName, 6, getTimeEpoch(pastYear, pastMonth, 1));

      // Save daily max-min
      writeJsonlSD(SD, maxPath, maxReadings, variablesMaxMinName, 6, getTimeEpoch(pastYear, pastMonth, pastDay));
      writeJsonlSD(SD, minPath, minReadings, variablesMaxMinName, 6, getTimeEpoch(pastYear, pastMonth, pastDay));
      //DateTime date = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
      //Serial.println(date);
      Serial.println(date.hour());
      Serial.println(date.minute());

      //reset max-min
      for(int i = 0; i < 6; i++) {
        *maxReadings[i] = -1;
        *minReadings[i] = 9999;
      }

      Serial.println("trying to change dir...");
      //currentDate();
      updateTime();
      currentDay = date.day();
      pastYear = date.year();
      pastMonth = date.month();
      pastDay = date.day();
      changeDatePath();
      changeLogsPath();
      changeAvgPath();
      changeMonthAvgPath();
      changeMaxPath();
      changeMinPath();
    }

    // Reattaching interrupts
    attachInterrupt(AIRFLOWPIN, airflowISR, FALLING);         // Might change later falling -> rising
    attachInterrupt(PRECIPPIN, precipISR, FALLING);
    // New start time for measurements
    computeTime = millis();

    alarmRTC = false;
    rtc.clearAlarm(1);
    ws.cleanupClients();
  }
  
}
