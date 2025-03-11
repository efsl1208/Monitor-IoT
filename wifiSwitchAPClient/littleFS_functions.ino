// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Read file from LittleFS
String readFileFS(fs::FS& fs, const char* path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    //return String();
    return "";  //test
  }

  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

// Assumes file is open
String readLineFS(File file){
  while(file.available()){
    return file.readStringUntil('\n');
  }
  return "";
}

String readMultipleLinesFS(fs::FS &fs, const char *path) {
  char fileContent[512] = "";
  String line = "init";
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }
  while(line != ""){
    line = readLineFS(file);
    if(line != "") strcat(fileContent, line.c_str());
  }
  file.close();
  return fileContent;
}


// Write file to LittleFS
void writeFileFS(fs::FS& fs, const char* path, const char* message) {
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

void writeCharArrayFS(fs::FS &fs, const char *path, char *message, int length) {
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

// Delete file LittleFS
void deleteFileFS(fs::FS& fs, const char* path) {
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("- file deleted");
  } else {
    Serial.println("- delete failed");
  }
}

// List dir LittleFS
void listDirFS(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDirFS(fs, file.path(), levels - 1);
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
