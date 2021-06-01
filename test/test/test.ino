#include <SPI.h>
#include <SD.h>

const int chipSelect = 8;

const int numReadings = 16;

int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average

int inputPin = A1;





void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // USBケーブルが接続されるのを待つ。この待ちループは Leonardo のみ必要。
  }
  Serial.print(F("Initializing SD card..."));

  pinMode(SS, OUTPUT);

  if (!SD.begin(chipSelect)) {
    Serial.println(F("Card failed, or not present"));
    // 失敗、何もしない
    while(1);
  }
  Serial.println(F("ok."));

  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
}

long start;

void loop() {
  total = total - readings[readIndex];          // subtract the last reading:
  readings[readIndex] = analogRead(inputPin);   // read from the sensor:
  total = total + readings[readIndex];          // add the reading to the total:
  readIndex = readIndex + 1;                    // advance to the next position in the array:

  if (readIndex >= numReadings) {               // if we're at the end of the array...
    readIndex = 0;                              // ...wrap around to the beginning:
  }

  average = total >>4; //ビットシフトで割り算をする
  
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  if (dataFile) {
    dataFile.println(average);
    dataFile.close();
    // シリアルポートにも出力
    Serial.print("電圧の値:");
    Serial.println(average);
  }
  // ファイルが開けなかったらエラーを出力
  else {
    Serial.println(F("error opening datalog.txt"));
  } 

  // 一秒待つ
  delay(1000);


  
}
