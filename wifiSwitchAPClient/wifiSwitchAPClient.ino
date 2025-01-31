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

// RTC & time variables
String sampleTimeString = "5000";    // default sample time in ms
int sampleTime = 5000;
RTC_DS3231 rtc;

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

int upperHourLimit = 0;       // <= 0     defaults
int lowerHourLimit = 23;      // >= 23
int upperMinLimit = 5;        // <= 5
int lowerMinLimit = 55;       // >= 55

String* csvTimeConfig[5] = {&sampleTimeString, &upperHourLimitS, &lowerHourLimitS, &upperMinLimitS, &lowerMinLimitS};

// File paths to save input values permanently
const char* credentialsPath = "/credentials.txt";
const char* credentialsPathBackup = "/credentialsBackup.txt";
const char* logsPathConst = "/logs.txt";
const char* logsPathBacup = "/logsBackup.txt";
const char* configPath = "/config.txt";
char logsPath[60] = "/logs.csv";
char datePath[30];

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
float solarIrrRatio = 0.0;

// Airflow measurement
#define AIRFLOWPIN 13
// Airflow variables
int timesAirflowSwitch = 0;
int* ptrTimesAirflowSwitch = &timesAirflowSwitch;
float airflow = 0.0;
float airflowRatio = 0.0;

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
int timesPrecipSwitch = 0;
int* ptrTimesPrecipSwitch = &timesPrecipSwitch;
float pecip = 0.0;
float precipRatio = 0.0;

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
extern void readFileSD();
extern void writeFileSD();
extern void appendFileSD();
extern void appendFileDebugSD();
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
extern void saveWindDir();
// Rain gauge
extern float readPrecip();
extern void savePrecip();

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_0 = "mode";
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";
const char* PARAM_INPUT_5 = "admin_pass";

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
    char buffer[50] = "";

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

// SD write a csv array
void appendCSVFileSD(fs::FS& fs, const char* path, String* text_vars[], int qty){
  char buffer[50] = "";
  Serial.print("Size of csv_variables: ");
  Serial.println(sizeof(text_vars) / sizeof(text_vars[0]));

  for(int i = 0; i < qty; i++){ 
    strcpy(buffer, text_vars[i]->c_str());
    appendFileSD(fs, path, buffer);
    if((i + 1) < sizeof(text_vars)){
      appendFileSD(fs, path, ",");
    } else {
      appendFileSD(fs, path, "\n"); 
    }
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

// Sets time from epoch
void manualTimeSet(unsigned long epochTime){
  struct timeval tv;
  tv.tv_sec = epochTime;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
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

// Change date path, "YY/MM/DD/TYPE/logs.csv"
void changeDatePath(){
  DateTime dateYMD = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  char buffer[30] = "";
  strcpy(datePath, "/");
  sprintf(buffer, "%d", dateYMD.year());
  strcat(datePath, buffer);
  if(!SD.exists(datePath)) createDirSD(SD, datePath);
  sprintf(buffer, "%d", dateYMD.month());
  strcat(datePath, "/");
  strcat(datePath, buffer);
  if(!SD.exists(datePath)) createDirSD(SD, datePath);
  sprintf(buffer, "%d", dateYMD.day());
  strcat(datePath, "/");
  strcat(datePath, buffer);
  if(!SD.exists(datePath)) createDirSD(SD, datePath);

  Serial.print("Date path: ");
  Serial.println(datePath);
}

// Returns path given the type of data to save
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
void IRAM_ATTR isrAirflow(){
  timesAirflowSwitch++;
}
// Precipitation interrupt
void IRAM_ATTR isrPrecip(){
  timesPrecipSwitch++;
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
  attachInterrupt(AIRFLOWPIN, isrAirflow, FALLING);         // Might change later falling -> rising
  attachInterrupt(PRECIPPIN, isrPrecip, FALLING);
  
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
  rawTextLine = readFileSD(SD, configPath);
  parse_csv(csvTimeConfig, &rawTextLine, ",", 5);

  sampleTime = atoi(sampleTimeString.c_str());
  upperHourLimit = atoi(upperHourLimitS.c_str());       
  lowerHourLimit = atoi(lowerHourLimitS.c_str());      
  upperMinLimit =  atoi(upperMinLimitS.c_str());       
  lowerMinLimit =  atoi(lowerMinLimitS.c_str());       

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
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/setup.html", "text/html"); // processor -> ledState string replaced, testing
    });
    server.serveStatic("/", LittleFS, "/");

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
    configTime(0, 0, ntpServer, ntpServer1);
    Serial.println("Time tried to be set...");
    printLocalTime();
    delay(5000);
    getLocalTime(&tmstruct);
    if(isRTC) rtcAdjust(DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec)); 
    
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
      request->send(LittleFS, "/setup.html", "text/html");
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
      request->send(LittleFS, "/setup.html", "text/html");
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
          appendCSVFileSD(SD, credentialsPathBackup, csv_variables, 6);
        }
        
      }
      request->send(200, "text/plain", "Done. Restarting with new settings.");
      should_restart = true;
    });
    server.begin();
  }

  // Creating paths
  getLocalTime(&tmstruct);
  changeDatePath();
}

void loop() {
  if (should_restart) {
    Serial.println("Rebooting...");
    server.end();
    delay(100);
    ESP.restart();
  }
  
  delay(sampleTime);

  // Detach interrupts for comms use
  detachInterrupt(AIRFLOWPIN);
  detachInterrupt(PRECIPPIN);

  //dhtVar[0] = readHumid();
  //dhtVar[1] = readTemp();
  solarIrr = readSolarIrr(ADCPIN, 1);

  epochTime_2 = epochTime_1;        // Past time for computing?
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
  
  saveTemp(SD, getTypePath(0), epochTime_1);
  saveDSBTemp(SD, getTypePath(2), epochTime_1, tempDSB);
  saveHumid(SD, getTypePath(1), epochTime_1);
  saveSolarIrr(SD, getTypePath(3), epochTime_1, ADCPIN, 1.0);
  //saveAirflow(SD, logsPath, epochTime_1, ptrTimesAirflowSwitch, sampleTime, 1.0);
  //saveWindDir(SD, logsPath, epochTime_1, WINDNPIN, WINDSPIN, WINDEPIN, WINDWPIN, WINDNEPIN, WINDNWPIN, WINDSEPIN, WINDSWPIN);
  //savePrecip(SD, logsPath, epochTime_1, ptrTimesPrecipSwitch, sampleTime, 1.0);
  

  //Calibration
  saveSolarIrr(SD, "/solarCalibration.txt", epochTime_1, ADCPIN, 1.0);

  if(analogReadMilliVolts(ADCPIN)>3150){
    digitalWrite(ledPin, HIGH);
    Serial.println("ADC pin is too high!");
    appendFileSD(SD, "/solarCalibration.txt", "Pin was too high!");
  } else {
    Serial.println("ADC pin low...");
  }

  rtcPrintTime();
  Serial.println("Local time: ");
  printLocalTime();

  //Serial.println("local time: ");
  //printLocalTime();

  // Reattaching interrupts
  attachInterrupt(AIRFLOWPIN, isrAirflow, FALLING);         // Might change later falling -> rising
  attachInterrupt(PRECIPPIN, isrPrecip, FALLING);
  

  // getLocalTime(&tmstruct);
  //DateTime date = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  // //Serial.println(date);
  // Serial.println(date.hour());
  // Serial.println(date.minute());

  if(isRTC){
    date = rtc.now();
  } else {
    date = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
  }

  if((date.hour() == lowerHourLimit || date.hour() == upperHourLimit) && (date.minute() > lowerMinLimit || date.minute() < upperMinLimit)){
    // Compute averages first!
    getLocalTime(&tmstruct);
    //DateTime date = DateTime(tmstruct.tm_year + 1900, tmstruct.tm_mon + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
    //Serial.println(date);
    Serial.println(date.hour());
    Serial.println(date.minute());

    Serial.println("trying to change dir...");
    changeDatePath();
  }


}
