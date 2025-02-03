String readWindDir(int N, int S, int E, int W, int NE, int NW, int SE, int SW){
  
  bool dir_nsew[4]  = {digitalRead(N),digitalRead(S),digitalRead(E),digitalRead(W)};
  bool dir_cross[4] = {digitalRead(NE),digitalRead(NW),digitalRead(SE),digitalRead(SW)};
  int c = 0;
  int dir = 0;

  for(int i = 0; i < 4; i++){
    if(dir_nsew[i]){
      c++;
      dir = i;
    } 
  }
  if(c > 1){
    for(int i = 0; i < 4; i++){
      if(dir_cross[i]){
        dir = i;
      } 
    }
    switch(dir){
      case 0: return "NE";
      case 1: return "NW";
      case 2: return "SE";
      case 3: return "SW";
    }
  }
  switch(dir){
      case 0: return "N";
      case 1: return "S";
      case 2: return "E";
      case 3: return "W";
  }

}

float readWindDirFloat(int N, int S, int E, int W, int NE, int NW, int SE, int SW){
  
  bool dir_nsew[4]  = {digitalRead(N),digitalRead(S),digitalRead(E),digitalRead(W)};
  bool dir_cross[4] = {digitalRead(NE),digitalRead(NW),digitalRead(SE),digitalRead(SW)};
  int c = 0;
  int dir = -1;

  for(int i = 0; i < 4; i++){
    if(dir_nsew[i]){
      c++;
      dir = i;
    } 
  }
  if(c > 1){
    for(int i = 0; i < 4; i++){
      if(dir_cross[i]){
        dir = i;
      } 
    }
    switch(dir){
      case 0: return 0;
      case 1: return 1;
      case 2: return 2;
      case 3: return 3;
    }
  }
  switch(dir){
      case 0: return 4;
      case 1: return 5;
      case 2: return 6;
      case 3: return 7;
      default: return -1;
  }

}

void saveWindDir(fs::FS& fs, const char* path, unsigned long epochTime, int N, int S, int E, int W, int NE, int NW, int SE, int SW){
  char buffer[30] = "";
  appendFileSD(fs, path, "windDir");
  appendFileSD(fs, path, ",");
  sprintf(buffer, "%f", readWindDir(N,S,E,W,NE,NW,SE,SW));
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, ",");
  strcpy(buffer, "");
  sprintf(buffer, "%lu", epochTime);      //%lu unsigned long integer
  appendFileSD(fs, path, buffer);
  appendFileSD(fs, path, "\n");
}