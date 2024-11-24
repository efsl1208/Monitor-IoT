/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
//#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_0 = "mode";
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";
const char* PARAM_INPUT_5 = "admin_pass";

//Variables to save values from HTML form
String mode = "";
String ssid = "";
String pass = "";
String ip = "";
String gateway = "";
String admin_pass = "";

String credentials[4], rawTextLine;

// File paths to save input values permanently
/*const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";*/


const char* credentialsPath = "/credentials.txt";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 255, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledPin = 2;
// Stores LED state

String ledState;

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

// Read File from LittleFS
/*bool readFile(fs::FS &fs, const char * path, String credentials[]){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return false;
  }
  
  String fileContent;
  uint8_t i = 0;
  while(file.available()){
    credentials[i]= file.readStringUntil(';');
    i++;
    if(i == 4) break;   
  }
  return true;
}*/

String readFile(fs::FS& fs, const char* path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}


// Write file to LittleFS
void writeFile(fs::FS& fs, const char* path, const char* message) {
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

void deleteFile(fs::FS& fs, const char* path) {
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("- file deleted");
  } else {
    Serial.println("- delete failed");
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

  if (!WiFi.config(localIP, localGateway, subnet)) {
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
String processor(const String& var) {
  if (var == "STATE") {
    if (digitalRead(ledPin)) {
      ledState = "ON";
    } else {
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  initLittleFS();

  listDir(LittleFS, "/", 1);

  //deleteFile(LittleFS, credentialsPath);

  // Set GPIO 2 as an OUTPUT
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  rawTextLine = readFile(LittleFS, credentialsPath);
  char rawTextLineP[rawTextLine.length()];
  strcpy(rawTextLineP, rawTextLine.c_str());
  
  Serial.println("rawTextLineP :");
  Serial.println(rawTextLineP);
  
  //mode select
  if (rawTextLine != "") {
    Serial.println("trying to read mode...");
    mode = String(strtok(rawTextLineP, ","));
    Serial.print("mode read: ");
    Serial.print(mode);
  }
  if (rawTextLine != "" && mode != "2") {
    //Parse saved config
    ssid = String(strtok(NULL, ","));
    pass = String(strtok(NULL, ","));
    ip = String(strtok(NULL, ","));
    gateway = String(strtok(NULL, ","));
  } else if (mode == "2") {
    mode = "0";
    ssid = String(strtok(NULL, ","));
    pass = String(strtok(NULL, ","));
    ip = String(strtok(NULL, ","));
    gateway = String(strtok(NULL, ","));
    Serial.println("mode set to 0");
  } else {
    mode = "2";  //0 -> AP Mode, 1 -> STA Mode, 2 -> No config.
    ssid = String();
    pass = String();
    ip = String();
    gateway = String();
    Serial.println("mode set to 2");
  }
  


  Serial.println(mode);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  // Mode = 0, AP MODE, Mode = 1 STATION MODE
  if (mode == "1" && initWiFi()) {
    //STATION Mode
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", LittleFS, "/");

    // Route to set GPIO state to HIGH
    server.on("/on", HTTP_GET, [](AsyncWebServerRequest* request) {
      digitalWrite(ledPin, HIGH);
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });

    // Route to set GPIO state to LOW
    server.on("/off", HTTP_GET, [](AsyncWebServerRequest* request) {
      digitalWrite(ledPin, LOW);
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });
    server.begin();
  } else if (mode == "0") {
    // AP Mode
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    // if (ssid != "") WiFi.softAP(ssid.c_str(), NULL);
    // else 
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    Serial.print("SSID: ");
    Serial.println(ssid);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });

    server.serveStatic("/", LittleFS, "/");

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
        }
      }
      sprintf(buffer, "%s,%s,%s,%s,%s", mode.c_str(), ssid.c_str(), pass.c_str(), ip.c_str(), gateway.c_str());
      Serial.print("Buffer: ");
      Serial.println(buffer);
      writeFile(LittleFS, credentialsPath, buffer);


      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      Serial.println("restarting...");
      ESP.restart();
    });
    server.begin();

    
  } else {
    //"No config" AP mode
    Serial.println("Setting AP (Access Point) config mode");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });

    server.serveStatic("/", LittleFS, "/");

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
        }
      }
      if (mode != "0" && mode != "1") {
        //Error, mode needs to be set to either 1 or 0
        request->send(400, "text/plain", "Bad request, mode not set");
        mode = "0";
        Serial.println("mode set to 0, no mode was set");
      } else if (mode == "0") {
        //Mode = 0, AP Mode
        if (ssid == "") {
          ssid = "ESP-32 WeatherStation";
        }

        if (pass == "") {
          pass = "NULL";
        }

      } else {
        //Mode = 1, STATION Mode
        if (ssid == "") {
          //Error, SSID needs to be set
          request->send(400, "text/plain", "Bad request, SSID not set");
        }

        if (pass == "") {
          pass = "NULL";
        }

      }


      sprintf(buffer, "%s,%s,%s,%s,%s", mode.c_str(), ssid.c_str(), pass.c_str(), ip.c_str(), gateway.c_str());
      Serial.print("Buffer: ");
      Serial.println(buffer);
      writeFile(LittleFS, credentialsPath, buffer);


      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      Serial.println("restarting...");
      ESP.restart();
    });

    server.begin();
  }
}

void loop() {
}
