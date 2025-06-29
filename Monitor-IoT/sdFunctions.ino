void initSD(bool *isInitSD){
  // SPI.begin(18, 19, 23, 5); // CLK = PIN 18, MISO = PIN 19, MOSI = PIN 23, CS = PIN 5
  delay(500);
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    *isInitSD = false;
    return;
  }
  *isInitSD = true;
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
}

void listDirSD(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDirSD(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDirSD(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDirSD(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

String readFileSD(fs::FS &fs, const char *path) {
  String fileContent;
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    //Serial.write(fileContent);
    break;
  }
  file.close();
  return fileContent;
}

// Assumes file is open
String readLineSD(File file){
  while(file.available()){
    return file.readStringUntil('\n');
  }
  return "";
}

String readMultipleLinesSD(fs::FS &fs, const char *path) {
  char fileContent[512] = "";
  String line = "init";
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }
  while(line != ""){
    line = readLineSD(file);
    if(line != "") strcat(fileContent, line.c_str());
  }
  file.close();
  return fileContent;
}

void writeFileSD(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void writeCharArraySD(fs::FS &fs, const char *path, char *message, int length) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  for (int i = 0; i < length; i++) {
    if(file.print(message[i])) {
      Serial.println("Char written");
    } else {
      Serial.println("Char failed");
    }
  } 
  file.close();
}

void appendFileSD(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.print(message);
  file.close();
}

void appendFileDebugSD(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
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

// SD save readings in JSONL format
void writeJsonlSD(fs::FS& fs, const char* path, float* values[], String* identifier[], int arrayLength, int timeStamp){
  char buffer[100] = "";
  char nBuffer[20] = "";
  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Couldn't open file!");
    return;
  } 

  for(int i = 0; i < arrayLength; i++){
    strcpy(buffer, "{\"t\": \"");
    sprintf(nBuffer, "%lu",timeStamp);
    strcat(buffer, nBuffer);
    strcat(buffer, "\", \"variable\":\"");
    strcpy(nBuffer, identifier[i+1]->c_str());
    strcat(buffer, nBuffer);
    strcat(buffer, "\", \"valor\": \"");
    //strcpy(nBuffer, "");
    sprintf(nBuffer, "%.3f", *values[i]);
    strcat(buffer, nBuffer);
    strcat(buffer, "\"}\n");

    file.print(buffer);
  }
  file.close();
  Serial.print("File written at: ");
  Serial.println(path);
}

// SD save readings in JSONL format
void writeJsonlSDDebug(fs::FS& fs, const char* path, float* values[], String* identifier[], int arrayLength, int timeStamp){
  Serial.println("Trying to write JSONL...");
  char buffer[100] = "";
  char nBuffer[20] = "";
  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Couldn't open file!");
    return;
  } 

  for(int i = 0; i < arrayLength; i++){
    strcpy(buffer, "{\"t\": \"");
    sprintf(nBuffer, "%lu",timeStamp);
    strcat(buffer, nBuffer);
    strcat(buffer, "\", \"variable\":\"");
    strcpy(nBuffer, identifier[i+1]->c_str());
    strcat(buffer, nBuffer);
    strcat(buffer, "\", \"valor\": \"");
    sprintf(nBuffer, "%.2f", *values[i]);
    strcat(buffer, nBuffer);
    strcat(buffer, "\"}\n");
    
    file.print(buffer);
    Serial.print("Text to save: ");
    Serial.println(buffer);
  }
  file.close();
  Serial.print("File written at: ");
  Serial.println(path);
}


// SD save single read in JSON
void writeSingleSensorSD(fs::FS& fs, const char* path, float value, String identifier, int timeStamp){
  Serial.println("Trying to write single JSONL...");
  char buffer[100] = "";
  char nBuffer[20] = "";
  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Couldn't open file!");
    return;
  } 
  strcpy(buffer, "{\"t\": \"");
  sprintf(nBuffer, "%lu",timeStamp);
  strcat(buffer, nBuffer);
  strcat(buffer, "\", \"variable\":\"");
  strcpy(nBuffer, identifier.c_str());
  strcat(buffer, nBuffer);
  strcat(buffer, "\", \"valor\": \"");
  sprintf(nBuffer, "%.2f", value);
  strcat(buffer, nBuffer);
  strcat(buffer, "\"}\n");

  file.print(buffer);
  
  file.close();
  Serial.print("File written at: ");
  Serial.println(path);
}

void renameFileSD(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFileSD(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}