#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "DHT.h"
#include "time.h"
#include <RTClib.h>
#include <sys/time.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>

/*    Pins usage
/     2: Led
/     4: DHT
/     5: CS
/     13: Airflow
/     14: Precipitation
/     17: Temp DS18B20
/     18: CLK
/     19: MISO
/     21: RTC
/     22: RTC
/     23: MOSI
/     34: ADC (Solar Irr)
*/


char bigBuffer[50400] = "";      // feels wrong

// RTC & time variables
String sampleTimeString = "5000";     // default sample time in ms
int sampleTime = 5000;
RTC_DS3231 rtc;
int baseTime = 1000;                  // base sample time for real time measurements 
int realTimeC = 0;
#define RTCINTERRUPTPIN 15            // for consistent readings
volatile bool alarmRTC = false;       // flag for alarm
long gmtOffset = -14400;

// Variables to save values from HTML form
String mode;
String ssid;
String pass;
String ip;
String gateway;
String admin_pass;

// R/W Variables
String rawTextLine;
String* csv_variables[6] = {&mode, &ssid, &pass, &ip, &gateway, &admin_pass};

// Variables for config
String upperHourLimitS = "0";       // <= 0     defaults
String lowerHourLimitS = "23";      // >= 23
String upperMinLimitS = "5";        // <= 5
String lowerMinLimitS = "55";       // >= 55
String gmtString = "-144000";

int upperHourLimit = 0;       // <= 0     defaults
int lowerHourLimit = 23;      // >= 23
int upperMinLimit = 5;        // <= 5
int lowerMinLimit = 55;       // >= 55

//String* csvTimeConfig[5] = {&sampleTimeString, &upperHourLimitS, &lowerHourLimitS, &upperMinLimitS, &lowerMinLimitS};
String* csvTimeConfig[2] = {&sampleTimeString, &gmtString};

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
String* csvAlarmConfig[12] = {
  &minHumidString, &minTempString, &minSolarString, &minAirflowString, &minPrecipString, &minPressureString,
  &maxHumidString, &maxTempString, &maxSolarString, &maxAirflowString, &maxPrecipString, &maxPressureString
  };

// File paths to save input values permanently
const char* credentialsPath = "/config/credentials.conf";
const char* credentialsPathBackup = "/credentialsBackup.txt";
const char* logsPathConst = "/logs.txt";
const char* logsPathBacup = "/logsBackup.txt";
const char* configPath = "/config/config.conf";
const char* alarmConfigPath = "/config/alarm.conf";
char logsPath[60] = "/logs.JSONL";
char avgPath[60] = "/avg.JSONL";
char monthAvgPath[60] = "/monthAvg.JSONL";
char maxPath[60] = "/max.JSONL";
char minPath[60] = "/min.JSONL";
char datePath[30];
char currentDateChar[20] = "";

// Logic variables
bool should_restart = false;
bool isInitSD = false;
bool isRTC = false;

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
//int* oldestDate[3] = {&oldestYear, &oldestMonth, &oldestDay}; 

// DHT definitions & config
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht (DHTPIN, DHTTYPE);
// DHT variables, should delete?
float dhtVar[2] = {0.0};

// DS18B20
#define DSBPIN 17
//DallasTemperature tempDSB;
OneWire oneWire(DSBPIN);
DallasTemperature tempDSB(&oneWire);

// Solar rad measurements
#define ADCPIN 34           // ADC1_6
// Solar irr variables
int intADCRead = -1;
float solarIrr = 0.0;       // Solar irradiance index W/m^2
float solarIrrRatio = 1.0;

// Airflow measurement
#define AIRFLOWPIN 13
// Airflow variables
volatile int timesAirflowSwitch = 0;
volatile int* ptrTimesAirflowSwitch = &timesAirflowSwitch;
int airFlowSwitchTime = 0;
int prevAirFlowSwitchTime = 0;
//float airflow = 0.0;
float airflowRatio = 1.0;

// Wind Direction             pin 35 placeholder
#define WINDNPIN 35
#define WINDSPIN 35
#define WINDEPIN 35
#define WINDWPIN 35
#define WINDNEPIN 35
#define WINDNWPIN 35
#define WINDSEPIN 35
#define WINDSWPIN 35

// Precipitation measurement
#define PRECIPPIN 14
// Precipitation variables
volatile int timesPrecipSwitch = 0;
volatile int* ptrTimesPrecipSwitch = &timesPrecipSwitch;
int precipSwitchTime = 0;
int prevPrecipSwitchTime = 0;
//float pecip = 0.0;
float precipRatio = 0.0;

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

// Variables to send data
String start;
String end;
String sensor;
String frequency;

// Max readings
float maxTempDHT = -1;
float maxHumid = -1;
float maxTemp = -1;
float maxSolar = -1;
float maxAirflow = -1;
//float maxWindDir = -1;
float maxPrecip = -1;
float maxPressure = -1;
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
float minHumidLimit = -1;
float minTempLimit = -1;
float minSolarLimit = -1;
float minAirflowLimit = -1;
float minPrecipLimit = -1;
float minPressureLimit = -1;
float* minReadingsLimit[6] = {&minHumidLimit, &minTempLimit, &minSolarLimit, &minAirflowLimit, &minPrecipLimit, &minPressureLimit};

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
extern void writeFileFS();
extern void deleteFileFS();
extern void listDirFS();
// SD
extern void initSD();
extern void listDirSD();
extern String readFileSD();
extern String readLineSD();
extern void writeFileSD();
extern void appendFileSD();
extern void appendFileDebugSD();
extern void appendCSVFileSD();
extern void writeJsonlSD();
extern void writeJsonlSDDebug();
extern void writeSingleSensorSD();
extern void deleteFileSD();
// Solar Irradiance
extern float readSolarIrr();
extern void saveSolarIrr();
// DHT
extern float readTemp();
extern float readHumid();
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
extern String readWindDir();
extern float readWindDirFloat();
extern void saveWindDir();
// Rain gauge
extern float readPrecip();
extern void savePrecip();

// AsyncWebServer object on port 80
AsyncWebServer server(80);
// Web socket
AsyncWebSocket ws("/ws");

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_0 = "mode";
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";
const char* PARAM_INPUT_5 = "admin_pass";
const char* PARAM_DATA_0 = "start";
const char* PARAM_DATA_1 = "end";
const char* PARAM_DATA_2 = "sensor";
const char* PARAM_DATA_3 = "frequency";
// IP variables
IPAddress localIP;

// Set your Gateway IP address
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
void parse_csv(String* text_vars[], String* csv, char* del, int qty){
    char buffer[100] = "";

    strcpy(buffer, csv->c_str());   //copy text into buffer
    Serial.print("String copied to buffer: ");
    Serial.println(buffer);

    char* p = strtok(buffer, del);
    Serial.print("First item read: ");
    Serial.println(p);

    for(int i = 0; i < qty; i++){   //qty = text_vars length
        if(p) {
          *text_vars[i] = p;    //if p exists
          Serial.print("p value saved: ");
          Serial.println(p);
        } else{
          *text_vars[i] = "";
        } 
        p = strtok(NULL, del);      //NULL pointer, strtok pointer to last string
    }
}    

// Config Factory reset
void resetConfig(){
  writeFileFS(LittleFS, credentialsPath, "");
}

// Gets time in EPOCH
unsigned long getTimeEpoch(){
  if (!getLocalTime(&tmstruct)) {
    Serial.println("Failed to obtain time");
    return(0);  //returns 0 if failed to obtain time
  }
  time(&now);
  return now;
}

// Return epoch for given day
unsigned long getTimeEpoch(int year, int month, int day){
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
  for(int i = 1970; i < year; i++){
    rtn += (337 + daysInMonth(i, 2))*86400;     // 86400 secs in a day
  }
  for(int i = 1; i < month; i++){
    rtn += (daysInMonth(year, i))*86400;
  }
  rtn += (day - 1) * 86400;
  return rtn;
}

// Sets time from epoch
void manualTimeSet(unsigned long epochTime){
  struct timeval tv;
  tv.tv_sec = epochTime;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
}

// Updates time for date management
void updateTime(){
  // Updating Time Structure
  if(isRTC){
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
void initWS(){
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Sending readings
void sendReadings(String data){
  ws.textAll(data);
}

// Sending alarms?

// Current readings to String
String currentReadingsString(float* values[], String* identifier[], int arrayLength, int epochTime){
  // Serial.print("Preparing data for ws...");
  char buffer[600] = "";    // Might be overkill
  char nBuffer[20] = "";    // Name buffer
  String reads = "";
  //strcpy(buffer, "{tipo: \"sensorData\", data:{t:");       // Timestamp name hardcoded to "t", maybe change
  strcpy(buffer, "{ \"type\": \"sensorData\", \"data\": { ");       
  //sprintf(nBuffer, "%lu", epochTime);
  //strcat(buffer, nBuffer);
  //strcat(buffer, ", {");

  for(int i = 0; i < arrayLength; i++){
    strcat(buffer, "\"");
    strcpy(nBuffer, identifier[i+1]->c_str());
    strcat(buffer, nBuffer);
    strcat(buffer, "\": { \"t\": ");          // Timestamp name hardcoded to "t", maybe change
    sprintf(nBuffer, "%lu", epochTime);
    strcat(buffer, nBuffer);
    strcat(buffer, ", \"sensor\": \"");
    strcpy(nBuffer, identifier[i+1]->c_str());
    strcat(buffer, nBuffer);
    strcat(buffer, "\", \"valor\": ");
    sprintf(nBuffer, "%.2f", *values[i]);
    strcat(buffer, nBuffer);
    strcat(buffer, "}");
    if(i+1<arrayLength) strcat(buffer, ", ");
  }
  strcat(buffer, "}}");
  reads = buffer;
  // Serial.println(reads);
  return reads;
}

// Alarm to String
String alarmString(float* values[], String* identifier[], int i, bool eventHL){     // eventHL true -> bajo, eventHL false -> alto
  Serial.print("Preparing alarm String for ws...");
  char buffer[300] = "";
  char nBuffer[20] = "";
  String alarm = "";
  // char eventType[10] = "";
  // if(eventHL){
  //   eventType = "bajo";
  // } else {
  //   eventType = "alto";
  // }
  strcpy(buffer, "{ \"type\": \"criticalEvent\", \"data\": { \"sensor\": \"");
  strcpy(nBuffer, identifier[i+1]->c_str());
  strcat(buffer, nBuffer);
  strcat(buffer, "\", \"value\": ");
  sprintf(nBuffer, "%.2f", *values[i]);
  strcat(buffer, nBuffer);
  strcat(buffer, ", \"eventType\": \"");
  if(eventHL){
    strcat(buffer, "bajo");
  } else {
    strcat(buffer, "alto");
  }
  strcat(buffer, "\" } }");
  alarm = buffer;
  Serial.println(alarm);
  return alarm;
}

// WS event handler
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
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
      Serial.println("test message from ws recieved...");
    }
  }
}

// Check oldest date registered
void oldestDate(){
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
  sprintf(buffer, "%d", compYear);
  strcat(yearPath, buffer);
  while(SD.exists(yearPath) || nC < 20){
    
    if(SD.exists(yearPath)){
      oldestYear = compYear;
      // Serial.print("found older year: ");
      // Serial.println(oldestYear);
    }
    compYear += -1;
    nC++;
    strcpy(yearPath, "/");
    sprintf(buffer, "%d", compYear);
    strcat(yearPath, buffer);
    
  }
  // Checks for oldest month registered
  strcpy(yearPath, "/");
  sprintf(buffer, "%d", oldestYear);
  strcat(yearPath, buffer);
  strcpy(monthPath, yearPath);
  strcat(monthPath, "/");
  sprintf(buffer, "%d", compMonth);
  strcat(monthPath, buffer);
  nC = 1;
  while(!SD.exists(monthPath) && nC < 12){
    compMonth += 1;
    nC++;
    oldestMonth = compMonth;
    strcpy(monthPath, yearPath);
    strcat(monthPath, "/");
    sprintf(buffer, "%d", compMonth);
    strcat(monthPath, buffer);
    
    // Serial.print("found older month: ");
    // Serial.println(buffer);
  }
  //Checks for oldest day registered
  strcpy(monthPath, yearPath);
  strcat(monthPath, "/");
  sprintf(buffer, "%d", oldestMonth);
  strcat(monthPath, buffer);
  strcpy(dayPath, monthPath);
  strcat(dayPath, "/");
  sprintf(buffer, "%d", compDay);
  strcat(dayPath, buffer);
  //c = 1;
  // while(!SD.exists(dayPath) && compDay < 31){
  //   compDay += 1;
  //   //c++;
  //   oldestDay = compDay;
  //   strcpy(dayPath, monthPath);
  //   strcat(dayPath, "/");
  //   sprintf(buffer,"%d", oldestYear);
  //   strcat(dayPath, buffer);
  //   strcat(dayPath, "-");
  //   sprintf(buffer, "%02d", oldestMonth);
  //   strcat(dayPath, buffer);
  //   strcat(dayPath, "-");
  //   sprintf(buffer, "%02d", compDay);
  //   strcat(dayPath, buffer);
  //   strcat(dayPath, "_");
  //   strcat(dayPath, variableName[0]->c_str());
  //   strcat(dayPath, ".JSONL");

  //   Serial.print("found older day: ");
  //   Serial.println(buffer);
  // }
  for(int i = 0; i < 31; i++){ 
    
    strcpy(dayPath, monthPath);
    strcat(dayPath, "/");
    sprintf(buffer,"%d", oldestYear);
    strcat(dayPath, buffer);
    strcat(dayPath, "-");
    sprintf(buffer, "%02d", oldestMonth);
    strcat(dayPath, buffer);
    strcat(dayPath, "-");
    sprintf(buffer, "%02d", i + 1);
    strcat(dayPath, buffer);
    strcat(dayPath, "_");
    strcat(dayPath, variableName[0]->c_str());
    strcat(dayPath, ".JSONL");
    oldestDay = i + 1;
    // Serial.print("found older day: ");
    // Serial.println(i + 1);
    if(SD.exists(dayPath)){
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
String oldestDateString(){
  Serial.print("Preparing date String for HTTP GET...");
  char buffer[300] = "";
  char nBuffer[20] = "";
  String oldest = "";
  // char eventType[10] = "";
  // if(eventHL){
  //   eventType = "bajo";
  // } else {
  //   eventType = "alto";
  // }
  strcpy(buffer, "[\"");
  sprintf(nBuffer, "%d", oldestYear);
  strcat(buffer, nBuffer);
  strcat(buffer, "-");
  sprintf(nBuffer, "%02d", oldestMonth);
  strcat(buffer,nBuffer);
  strcat(buffer, "-");
  sprintf(nBuffer, "%02d", oldestDay);
  strcat(buffer, nBuffer);
  strcat(buffer, "\", \"");
  sprintf(nBuffer, "%d", date.year());
  strcat(buffer, nBuffer);
  strcat(buffer, "-");
  sprintf(nBuffer, "%02d", date.month());
  strcat(buffer,nBuffer);
  strcat(buffer, "-");
  sprintf(nBuffer, "%02d", date.day());
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

void blink(){
  digitalWrite(ledPin, HIGH);
  delay(1000);
  digitalWrite(ledPin, LOW);
  delay(1000);
}

// Change date path, "YY/MM/..."
void changeDatePath(){
  //DateTime dateYMD = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  char buffer[30] = "";
  strcpy(datePath, "/");
  sprintf(buffer, "%d", date.year());
  strcat(datePath, buffer);
  if(!SD.exists(datePath)) createDirSD(SD, datePath);
  sprintf(buffer, "%d", date.month());
  strcat(datePath, "/");
  strcat(datePath, buffer);
  if(!SD.exists(datePath)) createDirSD(SD, datePath);
  // sprintf(buffer, "%d", dateYMD.day());                  
  // strcat(datePath, "/");
  // strcat(datePath, buffer);
  // if(!SD.exists(datePath)) createDirSD(SD, datePath);

  Serial.print("Date path: ");
  Serial.println(datePath);
}

// Returns path given the type of data to save, depracated
char* getTypePath(int type){                // 0: temp DHT, 1: humid DHT, 2: temp DSB, 3: solar irr, 4: airflow, 5: windDir, 6: precip, 7: pressure
  strcpy(logsPath, datePath);

  char dataType[30] = "default";
  switch(type){
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
  if(!SD.exists(logsPath)) createDirSD(SD, logsPath);
  strcat(logsPath, "/logs.csv");
  Serial.print("saving to: ");
  Serial.println(logsPath);
  return logsPath;
}

// Returns date YYYY-MM-DD
void currentDate(){
  Serial.println("trying to get current date...");
  char buffer[20] = "";
  char b[5] = "";
  //strcpy(buffer, datePath);
  strcpy(buffer, "");
  sprintf(b, "%d", date.year());
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf(b, "%02d", date.month());
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf(b, "%02d", date.day());
  strcat(buffer, b);
  Serial.print("date fetch: ");
  Serial.println(buffer);
  strcpy(currentDateChar, buffer);
  //return buffer;
}

// Returns current month YYYY-MM
void currentMonth(){
  Serial.println("trying to get current month...");
  char buffer[20] = "";
  char b[5] = "";
  //strcpy(buffer, datePath);
  strcpy(buffer, "");
  sprintf(b, "%d", date.year());
  strcat(buffer, b);
  strcat(buffer, "-");
  sprintf(b, "%02d", date.month());
  strcat(buffer, b);
  strcat(buffer, "-");
  Serial.print("month fetch: ");
  Serial.println(buffer);
  strcpy(currentDateChar, buffer);
}

// Changes logs path, .../YYYY-MM-DD.JSONL
void changeLogsPath(){
  //DateTime dateYMD = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(logsPath, datePath);
  strcat(logsPath, "/");
  sprintf(buffer, "%d", date.year());
  strcat(logsPath, buffer);
  strcat(logsPath, "-");
  sprintf(buffer, "%02d", date.month());
  strcat(logsPath, buffer);
  strcat(logsPath, "-");
  sprintf(buffer, "%02d", date.day());
  strcat(logsPath, buffer);
  strcat(logsPath, ".JSONL");       

  //strcat(logsPath, "/logs.csv");
  // if(!SD.exists(logsPath)) {
  //   appendCSVFileSD(SD, logsPath, variables, 9);
  // }

  Serial.print("saving to: ");
  Serial.println(logsPath);

  //return logsPath;
}

// Changes averages log path, .../YYYY-MM-DD_avg.JSONL
void changeAvgPath(){

  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(avgPath, datePath);
  strcat(avgPath, "/");
  sprintf(buffer, "%d", date.year());
  strcat(avgPath, buffer);
  strcat(avgPath, "-");
  sprintf(buffer, "%02d", date.month());
  strcat(avgPath, buffer);
  strcat(avgPath, "-");
  sprintf(buffer, "%02d", date.day());
  strcat(avgPath, buffer);
  strcat(avgPath, "_avg.JSONL");       

  Serial.print("saving averages to: ");
  Serial.println(avgPath);

}

void changeMonthAvgPath(){
  char buffer[5] = "";
  
  strcpy(monthAvgPath, "/");
  sprintf(buffer, "%d", date.year());
  strcat(monthAvgPath, buffer);
  strcat(monthAvgPath, "/");
  strcat(monthAvgPath, buffer);
  strcat(monthAvgPath, "-");
  sprintf(buffer, "%02d", date.month());
  strcat(monthAvgPath, buffer);
  strcat(monthAvgPath, "_monthAvg.JSONL");

  Serial.print("saving month avg values to: ");
  Serial.println(monthAvgPath);

}

// Changes max readings log path, .../YYYY-MM-DD_max.JSONL
void changeMaxPath(){

  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(maxPath, datePath);
  strcat(maxPath, "/");
  sprintf(buffer, "%d", date.year());
  strcat(maxPath, buffer);
  strcat(maxPath, "-");
  sprintf(buffer, "%02d", date.month());
  strcat(maxPath, buffer);
  strcat(maxPath, "-");
  sprintf(buffer, "%02d", date.day());
  strcat(maxPath, buffer);
  strcat(maxPath, "_max.JSONL");       

  Serial.print("saving max values to: ");
  Serial.println(maxPath);

}

// Changes min readings log path, .../YYYY-MM-DD_min.JSONL
void changeMinPath(){

  char buffer[5] = "";    //"YYYY-MM-DD"
  strcpy(minPath, datePath);
  strcat(minPath, "/");
  sprintf(buffer, "%d", date.year());
  strcat(minPath, buffer);
  strcat(minPath, "-");
  sprintf(buffer, "%02d", date.month());
  strcat(minPath, buffer);
  strcat(minPath, "-");
  sprintf(buffer, "%02d", date.day());
  strcat(minPath, buffer);
  strcat(minPath, "_min.JSONL");       

  Serial.print("saving min values to: ");
  Serial.println(minPath);

}

// Splits and returns String array, ignores {}
void splitString(String text, String array[], char del){
  byte y = 0;
  for(byte i = 0; i < text.length(); i++){
    if(text.charAt(i) == del){
      y++;
    } else if(!(text.charAt(i) == '{' || text.charAt(i) == '}' || text.charAt(i) == '"')){
      array[y] += text.charAt(i);
    }
  }
}

// Computes monthly average from SD data
void monthlyAverage(int year, int month){      // /YYYY/MM/YYYY-MM-    dd_avg.JSONL
  char path[60] = "";
  char intBuffer[10] = "";
  char tempPath[60] = "";
  strcpy(path, "/");
  sprintf(intBuffer, "%d", year);
  strcat(path, intBuffer);
  strcat(path, "/");
  sprintf(intBuffer, "%d", month);
  strcat(path, intBuffer);
  strcat(path, "/");
  sprintf(intBuffer, "%d", year);
  strcat(path, intBuffer);
  strcat(path, "-");
  sprintf(intBuffer, "%02d", month);
  strcat(path, intBuffer);
  strcat(path, "-");

  for(int i = 0; i < 7; i++){   // Resets variables
    cReadings[i] = 0;
    nReadings[i] = 0;
  }  

  for(int i = 1; i < daysInMonth(year, month) + 1; i++){
    strcpy(tempPath, path);
    sprintf(intBuffer, "%02d", i);
    strcat(tempPath, intBuffer);
    strcat(tempPath, "_avg.JSONL");

    if(SD.exists(tempPath)){
      File file = SD.open(tempPath);
      if(!file){
        Serial.println("Failed to open file for reading and computing monthly avg...");
        return;
      }
      String line = "init";
      while(!(line == "")){
        String group[5];
        String individual[5][5];
        line = "";
        line = readLineSD(file);
        splitString(line, group, ',');
        for(int i = 0; i < 3; i++){
          splitString(group[i],individual[i],':');
        }

        // Serial.print("Line read: ");
        // Serial.println(line);

        // Serial.print("Value at group 2: ");
        // Serial.println(group[1]);
        
        // Serial.print("Value at individual 3, 2: ");
        // Serial.println(individual[2][1]);

        for(int i = 0; i < 6; i++){
          if(individual[1][1] == *variableName[i + 1]){
            nReadings[i]++;
            cReadings[i] += atof(individual[2][1].c_str());
          }
        }
      }
      file.close();
    }
  }
  for(int i = 0; i < 7; i++){
    if(nReadings[i] > 0) {
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
void dailyAverage(char* path){
  File file = SD.open(path);
  if(!file){
    Serial.println("Failed to open file for reading and computing avg...");
    return;
  }
  String line = "init";
  

  for(int i = 0; i < 7; i++){
    cReadings[i] = 0;
    nReadings[i] = 0;
  }  
  
  while(!(line == "")){
    String group[5];
    String individual[5][5];
    line = "";
    line = readLineSD(file);
    splitString(line, group, ',');
    for(int i = 0; i < 3; i++){
      splitString(group[i],individual[i],':');
    }

    // Serial.print("Line read: ");
    // Serial.println(line);

    // Serial.print("Value at group 2: ");
    // Serial.println(group[1]);
    
    // Serial.print("Value at individual 3, 2: ");
    // Serial.println(individual[2][1]);

    for(int i = 0; i < 7; i++){
      if(individual[1][1] == *variableName[i + 1]){
        nReadings[i]++;
        cReadings[i] += atof(individual[2][1].c_str());
      }
    }
    //esp_task_wdt_reset();
  }

  for(int i = 0; i < 7; i++){
    if(nReadings[i] > 0) {
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

// Returns data from specified sensor in String
String sensorData(String startTime, String endTime, String variable, String frequency){      
  String dataString = "";
  String line = "init";
  //Serial.print("Trying to create big buffer...");
  //char bigBuffer[50400] = "";      // feels wrong
  //Serial.println("Done!");
  char pathBuffer[40] = "";
  char dBuffer[10] = "";
  Serial.println(strtoul(startTime.c_str(), NULL, 0));
  DateTime selectedDate = DateTime(strtoul(startTime.c_str(), NULL, 0));
  //time_t selectedDate = startTime.toInt();
  bool coma = false;
  int selectedDay = selectedDate.day();
  int selectedMonth = selectedDate.month();
  int selectedYear = selectedDate.year();
  
  strcpy(pathBuffer, "/");
  sprintf(dBuffer, "%d", selectedYear);
  strcat(pathBuffer, dBuffer);
  if(frequency != "anual"){
    strcat(pathBuffer, "/");
    sprintf(dBuffer, "%d", selectedMonth);
    strcat(pathBuffer, dBuffer); 
    // Read daily avgs for selected month
  } else {    // "anual"
    strcat(pathBuffer, "/");
    sprintf(dBuffer, "%d", selectedYear);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    char yearPathBuffer[60] = "";

    strcpy(bigBuffer, "[ ");
    coma = true;
    for(int i = 1; i < 13; i++){
      esp_task_wdt_reset();
      strcpy(yearPathBuffer, pathBuffer);
      sprintf(dBuffer, "%02d", i);
      strcat(yearPathBuffer, dBuffer);
      strcat(yearPathBuffer, "_monthAvg.JSONL");

      if(SD.exists(yearPathBuffer)){
        File file = SD.open(yearPathBuffer);
        if(!file){
          Serial.println("Failed to open file to send anual data...");
          //return "";
        }
        //strcpy(buffer, "{ \"data\": [");

        while(!(line == "")){
          String group[5];
          String individual[5][5];
          line = "";
          line = readLineSD(file);
          splitString(line, group, ',');
          for(int i = 0; i < 3; i++){
            splitString(group[i],individual[i],':');
          }

          if(individual[1][1] == variable){   // Checks for selected variable
            if(!coma){
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
    strcat(bigBuffer, "]");
    Serial.print("Big buffer anual: ");
    Serial.println(bigBuffer); 
  }
  

  // Fetch all values for a specified date
  if(frequency == "daily"){
    strcat(pathBuffer, "/");
    sprintf(dBuffer, "%d", selectedYear);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    sprintf(dBuffer, "%02d", selectedMonth);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    sprintf(dBuffer, "%02d", selectedDay);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "_");
    strcat(pathBuffer, variable.c_str());
    strcat(pathBuffer, ".JSONL");

    Serial.println(pathBuffer);

    File file = SD.open(pathBuffer);
    if(!file){
      Serial.println("Failed to open file to send daily data...");
      return "";
    }
    //strcpy(buffer, "{ \"data\": [");
    strcpy(bigBuffer, "[ ");
    coma = true;
    while(!(line == "")){
      String group[5];
      String individual[5][5];
      line = "";
      line = readLineSD(file);
      splitString(line, group, ',');
      for(int i = 0; i < 3; i++){
        splitString(group[i],individual[i],':');
      }

      if(individual[1][1] == variable){   // Checks for selected variable
        if(!coma){
          strcat(bigBuffer, ",");            // if there is no coma before new data, prints one
          coma = true;
        }
        strcat(bigBuffer, line.c_str());
        coma = false;
      }
      esp_task_wdt_reset();
    }
    strcat(bigBuffer, "]");  
    file.close();

  } else if (frequency == "monthly"){

    strcat(pathBuffer, "/");
    sprintf(dBuffer, "%d", selectedYear);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    sprintf(dBuffer, "%02d", selectedMonth);
    strcat(pathBuffer, dBuffer);
    strcat(pathBuffer, "-");
    strcpy(bigBuffer, "[ ");
    coma = true;
    for(int i = 1; i < daysInMonth(selectedYear, selectedMonth) + 1; i++){
      line = "init";
      esp_task_wdt_reset();
      char monthPathBuffer[60];
      char dayBuffer[10];
      strcpy(monthPathBuffer, pathBuffer);
      sprintf(dayBuffer, "%02d", i);
      strcat(monthPathBuffer, dayBuffer);
      strcat(monthPathBuffer, "_avg.JSONL");
      /////////////////////////////////////////////////////////////////////////////////////
      Serial.println(monthPathBuffer);

      //if(selectedYear == date.year() && selectedMonth == date.month() && i == date.day() && SD.exists(monthPathBuffer)) deleteFileSD(SD, monthPathBuffer);  // Always compute current day avg
      
      // if(!SD.exists(monthPathBuffer)){
      //   Serial.println("Failed to open file to read average data...");
      //   // Try to compute avg for selected day of the month, if fails save "" to response String
      //   //return "";
              
      //   for(int j = 0; j < 6; j++){  
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
      if(SD.exists(monthPathBuffer)){
        File file = SD.open(monthPathBuffer);
        if(!file){
          Serial.println("Failed to open file to send monthly data...");
          //return "";
        }
        //strcpy(buffer, "{ \"data\": [");

        while(!(line == "")){
          String group[5];
          String individual[5][5];
          line = "";
          line = readLineSD(file);
          splitString(line, group, ',');
          for(int i = 0; i < 3; i++){
            splitString(group[i],individual[i],':');
          }

          if(individual[1][1] == variable){   // Checks for selected variable
            if(!coma){
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
    strcat(bigBuffer, "]");  
  }

  Serial.print("Data read..."); 
  //esp_task_wdt_reset();
  return bigBuffer;
}

int daysInMonth(int selectedYear, int selectedMonth){
  int rtnValue = 0;
  if(selectedMonth == 2){
    if(selectedYear % 4 == 0 && (selectedYear % 100 != 0 || selectedYear % 400 == 0)){
      rtnValue = 29;
    } else {
      rtnValue = 28;
    }
  } else if(selectedMonth == 4 || selectedMonth == 6 || selectedMonth == 9 || selectedMonth == 11){
    rtnValue = 30;
  } else {
    rtnValue = 31;
  }
  return rtnValue;
}

//testing
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
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
void IRAM_ATTR airflowISR(){
  airFlowSwitchTime = millis();
  if(airFlowSwitchTime - prevAirFlowSwitchTime > 100){
    timesAirflowSwitch++;
    prevAirFlowSwitchTime = airFlowSwitchTime;
  }

}
// Precipitation interrupt
void IRAM_ATTR precipISR(){
  precipSwitchTime = millis();
  if(precipSwitchTime - prevPrecipSwitchTime > 100){
    timesPrecipSwitch++;
    prevPrecipSwitchTime = precipSwitchTime;
  }
  
}

// RTC alarm
void IRAM_ATTR rtcISR(){
  alarmRTC = true;
}



void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Set GPIO 2 as an OUTPUT
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Inits
  initSD(&isInitSD);
  dht.begin();
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

  Serial.println("SD directory");
  listDirSD(SD, "/", 4);

  // LittleFS
  listDirFS(LittleFS, "/", 0);
  Serial.println("LittleFS directory");

  // Setting interrupts
  pinMode(AIRFLOWPIN, INPUT);
  pinMode(PRECIPPIN, INPUT);
  pinMode(RTCINTERRUPTPIN, INPUT_PULLUP);
  attachInterrupt(AIRFLOWPIN, airflowISR, FALLING);         // Might change later falling -> rising
  attachInterrupt(PRECIPPIN, precipISR, FALLING);
  attachInterrupt(RTCINTERRUPTPIN, rtcISR, FALLING);

  //SD Working indicator
  if(isInitSD){
    int c = 0;
    while(c<2){
      blink();
      c++;
    }
  }

  // Credentials reading
  // Try to read from SD
  if(isInitSD){
    Serial.println("reading from SD...");
    rawTextLine = readFileSD(SD, credentialsPath);
    Serial.println("file read...");
  } else {
    rawTextLine = readFileFS(LittleFS, credentialsPath);    // Read from LittleFS if SD failed
  }
  // File content saved in csv_variables
  parse_csv(csv_variables, &rawTextLine, ",", 6);

  // Config reading
  if(isInitSD){
    rawTextLine = readFileSD(SD, configPath);
    if(rawTextLine != "") parse_csv(csvTimeConfig, &rawTextLine, ",", 2);
    rawTextLine = readFileSD(SD, alarmConfigPath);
    if(rawTextLine != "") parse_csv(csvAlarmConfig, &rawTextLine, ",", 12);
  }
  // String to int
  sampleTime = atoi(sampleTimeString.c_str());
  gmtOffset = atoi(gmtString.c_str());
  // upperHourLimit = atoi(upperHourLimitS.c_str());       
  // lowerHourLimit = atoi(lowerHourLimitS.c_str());      
  // upperMinLimit =  atoi(upperMinLimitS.c_str());       
  // lowerMinLimit =  atoi(lowerMinLimitS.c_str());
  // Alarm String to float   
  for(int i = 0; i < 6; i++){
    *minReadingsLimit[i] = atof(csvAlarmConfig[i]->c_str());
    *maxReadingsLimit[i] = atof(csvAlarmConfig[i + 6]->c_str());
    Serial.print("Alarm limit low: ");
    Serial.println(*csvAlarmConfig[i]);
    Serial.println(*minReadingsLimit[i]);
    Serial.print("Alarm limit high: ");
    Serial.println(*csvAlarmConfig[i+6]);
    Serial.println(*maxReadingsLimit[i]);
      
  }


  if(mode == "") mode = "2";  //0 -> AP Mode, 1 -> STA Mode, 2 -> No config.

  Serial.println(mode);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  // Mode = 0 AP MODE, Mode = 1 STATION MODE
  if (mode == "1" && initWiFi()) {
    /*
    /
    /
    */
    //STATION Mode
    initWS();
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/mainUI/index.html", "text/html"); // main UI loading
    });
    server.serveStatic("/", LittleFS, "/");

    server.on("/dates", HTTP_GET, [](AsyncWebServerRequest* request){
      Serial.println("Trying to send dates...");
      updateTime();
      oldestDate();
      Serial.println("Oldest date fetch!");
      request->send(200, "application/json", oldestDateString());
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request){
      int params = request->params();

      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->name() == PARAM_DATA_0) {
          start = p->value().c_str();
          Serial.print("Start time set to: ");
          Serial.println(start);
        }
        // HTTP GET end time
        end = "0";
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


      // Only daily working
      request->send(200, "application/json", sensorData(start, end, sensor, frequency));
      //esp_task_wdt_reset();
    });


    // // Route to set GPIO state to HIGH
    // server.on("/on", HTTP_GET, [](AsyncWebServerRequest* request) {
    //   digitalWrite(ledPin, HIGH);
    //   request->send(LittleFS, "/index.html", "text/html", false, processor);
    // });

    // // Route to set GPIO state to LOW
    // server.on("/off", HTTP_GET, [](AsyncWebServerRequest* request) {
    //   digitalWrite(ledPin, LOW);
    //   request->send(LittleFS, "/index.html", "text/html", false, processor);
    // });

    // Time config (UTC)
    configTime(gmtOffset, 0, ntpServer, ntpServer1);
    delay(5000);
    Serial.println("Time tried to be set...");
    int timeC = 0;
    struct tm timeinfoControl;
    while(!getLocalTime(&timeinfoControl) && timeC < 10){
      printLocalTime();
      delay(1000);
      timeC++;
      if(timeC == 10) Serial.println("Time sync timed out...");
    }

    getLocalTime(&tmstruct);
    if(isRTC && tmstruct.tm_year+1900 >= rtc.now().year()) rtcAdjust(DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec)); 
    
    server.begin();


  } else if (mode == "0") {
    /*
    /
    /
    */
    // AP Mode
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    if (pass != "") WiFi.softAP(ssid.c_str(), pass.c_str());
    else WiFi.softAP(ssid.c_str(), NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/setup/setup.html", "text/html");
    });

    server.serveStatic("/", LittleFS, "/");

    // Set time from rtc
    if(isRTC) {
      manualTimeSet(getEpochRTC());
    }
    server.begin();


  } else {
    /*
    /   
    /
    */
    //"No config" AP mode
    Serial.println("Setting AP (Access Point) config mode");
    // NULL sets an open Access Point
    WiFi.softAP("Estación Meteorológica IoT", NULL);

    // Set time from rtc
    if(isRTC) manualTimeSet(getEpochRTC());

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
      char buffer[50] = "";
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          if (p->name() == PARAM_INPUT_0) {
            mode = p->value().c_str();
            Serial.print("Mode set to: ");
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
      if (mode != "0" && mode != "1") {
        //Error, mode needs to be set to either 1 or 0
        return request->send(400, "text/plain", "Bad request, wrong mode");
      } else if (ssid == "" || admin_pass == "") {
        //Error, SSID and admin pass needs to be set
        return request->send(400, "text/plain", "Bad request, SSID or admin pass not set");
      } else {
        //Request OK, save config
        sprintf(buffer, "%s,%s,%s,%s,%s,%s", mode.c_str(), ssid.c_str(), pass.c_str(), ip.c_str(), gateway.c_str(), admin_pass.c_str());
        Serial.print("Write buffer: ");
        Serial.println(buffer);
        
        // LittleFS write, disabled while testing
        //writeFileFS(LittleFS, credentialsPath, buffer);

        if(isInitSD){
          // SD write
          writeFileSD(SD, credentialsPath, buffer);
          //appendCSVFileSD(SD, credentialsPathBackup, csv_variables, 6);
        }
        
      }
      request->send(200, "text/plain", "Done. Restarting with new settings.");
      should_restart = true;
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

  // Info
  Serial.print("Current sample time: ");
  Serial.println(sampleTime);
  
  rtc.setAlarm1(DateTime(0,0,0,0,0,1), DS3231_A1_PerSecond);
  alarmRTC = false;
  rtc.clearAlarm(1);
  Serial.println(alarmRTC);

  // Compute today's average before taking data
  for(int i = 0; i < 6; i++){
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
  if(SD.exists(avgPath)) deleteFileSD(SD, avgPath);       // File was used to compute this month's values'
  writeJsonlSD(SD, avgPath, avgReadings, variableName, 6, getTimeEpoch(date.year(), date.month(), date.day()));

  monthlyAverage(date.year(), date.month());
  if(SD.exists(monthAvgPath)) deleteFileSD(SD, monthAvgPath);       // File was used to compute this year's values'
  writeJsonlSD(SD, monthAvgPath, avgReadings, variableName, 6, getTimeEpoch(date.year(), date.month(), 1));
  
}

void loop() {
  if (should_restart) {
    Serial.println("Rebooting...");
    server.end();
    delay(100);
    ESP.restart();
  }
  // if(alarmRTC){ 
  //   Serial.println("flag true....");
  // } else {
  //   Serial.println("flag false!");
  // }
  while((isRTC && alarmRTC) || !isRTC){
    if(!isRTC) delay(baseTime);   // Backup timer
    // Detach interrupts for comms use
    detachInterrupt(AIRFLOWPIN);
    detachInterrupt(PRECIPPIN);

    updateTime();
    
    //solarIrr = readSolarIrr(ADCPIN, 1);

    epochTime_2 = epochTime_1;        // Past time for computing? most likely remove
    if(isRTC) {                       // Prefer ext RTC
      epochTime_1 = getEpochRTC();    // Current time
    } else {
      epochTime_1 = getTimeEpoch();     // Current time
    }

    // saveTemp(SD, logsPath, epochTime_1);
    // saveDSBTemp(SD, logsPath, epochTime_1, tempDSB);
    // saveHumid(SD, logsPath, epochTime_1);
    // saveSolarIrr(SD, logsPath, epochTime_1, ADCPIN, 1.0);
    // //saveAirflow(SD, logsPath, epochTime_1, ptrTimesAirflowSwitch, sampleTime, 1.0);
    // //saveWindDir(SD, logsPath, epochTime_1, WINDNPIN, WINDSPIN, WINDEPIN, WINDWPIN, WINDNEPIN, WINDNWPIN, WINDSEPIN, WINDSWPIN);
    // //savePrecip(SD, logsPath, epochTime_1, ptrTimesPrecipSwitch, sampleTime, 1.0);
    
    // saveTemp(SD, getTypePath(0), epochTime_1);
    // saveDSBTemp(SD, getTypePath(2), epochTime_1, tempDSB);
    // saveHumid(SD, getTypePath(1), epochTime_1);
    // saveSolarIrr(SD, getTypePath(3), epochTime_1, ADCPIN, 1.0);
    //saveAirflow(SD, logsPath, epochTime_1, ptrTimesAirflowSwitch, sampleTime, 1.0);
    //saveWindDir(SD, logsPath, epochTime_1, WINDNPIN, WINDSPIN, WINDEPIN, WINDWPIN, WINDNEPIN, WINDNWPIN, WINDSEPIN, WINDSWPIN);
    //savePrecip(SD, logsPath, epochTime_1, ptrTimesPrecipSwitch, sampleTime, 1.0);


    // Reading sensors
    tempDHT = readTemp();
    humid = readHumid();
    temp = readDSBTemp(tempDSB);
    solar = readSolarIrr(ADCPIN, solarIrrRatio);
    airflow = readAirflow(ptrTimesAirflowSwitch, sampleTime, airflowRatio);
    windDir = readWindDirFloat(WINDNPIN, WINDSPIN, WINDEPIN, WINDWPIN, WINDNEPIN, WINDNWPIN, WINDSEPIN, WINDSWPIN);
    precip = readPrecip(ptrTimesPrecipSwitch, sampleTime, precipRatio);
    // WebSocket sending readings
    sendReadings(currentReadingsString(readings, variableName, 7, epochTime_1));



    // // Checking for max & min, fix pls
    // if(temp > maxTemp) maxTemp = temp;
    // if(temp < minTemp) minTemp = temp;
    // if(tempDHT > maxTempDHT) maxTempDHT = tempDHT;
    // if(tempDHT < minTempDHT) minTempDHT = tempDHT;
    // if(humid > maxHumid) maxHumid = humid;
    // if(humid < minHumid) minHumid = humid;
    // if(solar > maxSolar) maxSolar = solar;
    // if(solar < minSolar) minSolar = solar;
    // if(airflow > maxAirflow) maxAirflow = airflow;
    // if(airflow < minAirflow) minAirflow = airflow;
    // if(precip > maxPrecip) maxPrecip = precip;
    // if(precip < minPrecip) minPrecip = precip;
    // if(pressure > maxPressure) maxPressure = pressure;
    // if(pressure < minPressure) minPressure = pressure;

    for(int i = 0; i < 6; i++){
      if(*readings[i] > *maxReadings[i]) *maxReadings[i] = *readings[i];
      if(*readings[i] < *minReadings[i]) *minReadings[i] = *readings[i];
      // Alarm checking
      if(*readings[i] > *maxReadingsLimit[i]){
        //Send alarm High
        sendReadings(alarmString(readings, variableName, i, false));
        activeAlarm[i + 6] = true; 
        startAlarmTime[i + 6] = epochTime_1;
        sensorAlarm = true;
      }
      if(*readings[i] < *minReadingsLimit[i]){
        //Send alarm Low
        sendReadings(alarmString(readings, variableName, i, true));
        activeAlarm[i] = true;
        startAlarmTime[i] = epochTime_1;
        sensorAlarm = true;
      }
      //Serial.println("Stopped checking...");
    }

    // Checking if value is still is alarm state
    if(sensorAlarm){
      for(int i = 0; i < 6; i++){
        //Serial.println("Checking alarm low");
        if(activeAlarm[i] && (*readings[i] > *minReadingsLimit[i])){
          endAlarmTime[i] = epochTime_1;
          sensorAlarm = false;
        }
      }
      for(int i = 6; i < 12; i++){
        //Serial.println("Checking alarm high");
        if(activeAlarm[i] && (*readings[i - 6] < *maxReadingsLimit[i - 6])){
          endAlarmTime[i] = epochTime_1;
          sensorAlarm = false;
        }
      }
    }

    // Saving readings 
    realTimeC += baseTime;         // Adding +1s
    if((realTimeC >= sampleTime) && isInitSD){
      writeJsonlSD(SD, logsPath, readings, variableName, 7, epochTime_1);
      realTimeC = 0;

      
      // Save in indivdual files
      for(int i = 0; i < 7; i++){
        char b[60] = "";
        //char bn[20] = "";
        strcpy(b, datePath);
        strcat(b, "/");
        currentDate();
        strcat(b, currentDateChar);
        strcat(b, "_");
        // Serial.print("i: ");
        // Serial.println(i);
        // Serial.print("Variable name array: ");
        // Serial.println(variableName[i+1]->c_str());
        strcat(b, variableName[i+1]->c_str());
        //strcpy(bn, *variableName[i+1]->toCharArray(bn, 20));
        //*variableName[i+1]->toCharArray(bn, 20);
        //strcat(b, bn);
        strcat(b, ".JSONL");
        Serial.println(b);
        writeSingleSensorSD(SD, b, *readings[i], *variableName[i+1], epochTime_1);
      }

      // Serial.println("RTC time: ");
      // rtcPrintTime();
      // Serial.println("Local time: ");
      // printLocalTime();
      
      // Serial.print("Current sample time: ");
      // Serial.println(sampleTime);
    }

    // //Calibration
    // saveSolarIrr(SD, "/solarCalibration.txt", epochTime_1, ADCPIN, 1.0);

    // if(analogReadMilliVolts(ADCPIN)>3150){
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
    //if((date.hour() == lowerHourLimit || date.hour() == upperHourLimit) && (date.minute() > lowerMinLimit || date.minute() < upperMinLimit)){
    if(date.day() != currentDay){
      // Compute averages first!
      for(int i = 0; i < 6; i++){
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
      if(SD.exists(avgPath)) deleteFileSD(SD, avgPath);       // File was used to compute this month's values'
      writeJsonlSD(SD, avgPath, avgReadings, variableName, 6, epochTime_1);
      monthlyAverage(date.year(), date.month());
      if(SD.exists(monthAvgPath)) deleteFileSD(SD, monthAvgPath);       // File was used to compute this year's values'
      writeJsonlSD(SD, monthAvgPath, avgReadings, variableName, 6, getTimeEpoch(date.year(), date.month(), 1));

      // Save daily max-min
      writeJsonlSD(SD, maxPath, maxReadings, variablesMaxMinName, 6, epochTime_1);
      writeJsonlSD(SD, minPath, minReadings, variablesMaxMinName, 6, epochTime_1);
      //DateTime date = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
      //Serial.println(date);
      Serial.println(date.hour());
      Serial.println(date.minute());

      //reset max-min
      for(int i = 0; i < 6; i++){
        *maxReadings[i] = -1;
        *minReadings[i] = 9999;
      }

      Serial.println("trying to change dir...");
      currentDay = date.day();
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
    
    alarmRTC = false;
    rtc.clearAlarm(1);
    ws.cleanupClients();
  }
}
