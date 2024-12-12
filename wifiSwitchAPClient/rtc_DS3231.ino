void rtcInit(bool *isRTC){
  if(!rtc.begin()){
    Serial.println("RTC module failure");
    *isRTC = false;
    return;
  }
  Serial.println("RTC module connected");
  *isRTC = true;
}

void rtcAdjust(int year, int month, int day, int hour, int minute, int second){
  rtc.adjust(year, month, day, hour, minute, second);
  Serial.println("RTC time adjusted to (UTC):");
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
}

unsigned long getEpochRTC(){
  return rtc.now();
}