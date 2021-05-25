#include <SPI.h>
#include <SD.h>
#include <MsTimer2.h>

const int chipSelect = 4;

const int numReadings = 16;  //　配列の数

int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average
int inputPin = A1;  

const int numReadings2 = 16;

int readings2[numReadings2];      // the readings from the analog input
int readIndex2 = 0;              // the index of the current reading
int total2 = 0;                  // the running total
int average2 = 0;                // the average
int inputPin2 = A2;

const int numReadings3 = 16;

int readings3[numReadings3];      // the readings from the analog input
int readIndex3 = 0;              // the index of the current reading
int total3 = 0;                  // the running total
int average3 = 0;                // the average
int inputPin3 = A3;

int count = 0;
int before =0;

void flash()
{
  total = total - readings[readIndex];          // subtract the last reading:
  readings[readIndex] = analogRead(inputPin);   // read from the sensor:
  total = total + readings[readIndex];          // add the reading to the total:
  readIndex = readIndex + 1;                    // advance to the next position in the array:

  if (readIndex >= numReadings) {               // if we're at the end of the array...
    readIndex = 0;                              // ...wrap around to the beginning:
  }

  average = total >>4; //ビットシフトで割り算をする


  total2 = total2 - readings2[readIndex2];          // subtract the last reading:
  readings2[readIndex2] = analogRead(inputPin2);   // read from the sensor:
  total2 = total2 + readings2[readIndex2];          // add the reading to the total:
  readIndex2 = readIndex2 + 1;                    // advance to the next position in the array:

  if (readIndex2 >= numReadings2) {               // if we're at the end of the array...
    readIndex2 = 0;                              // ...wrap around to the beginning:
  }

  average2 = total2 >>4; //ビットシフトで割り算をする

  
  total3 = total3 - readings3[readIndex3];          // subtract the last reading:
  readings3[readIndex3] = analogRead(inputPin3);   // read from the sensor:
  total3 = total3 + readings3[readIndex3];          // add the reading to the total:
  readIndex3 = readIndex3 + 1;                    // advance to the next position in the array:

  if (readIndex3 >= numReadings3) {               // if we're at the end of the array...
    readIndex3 = 0;                              // ...wrap around to the beginning:
  }

  average3 = total3 >>4; //ビットシフトで割り算をする

  count = ++count;
}

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

  for (int thisReading = 0; thisReading < numReadings; thisReading++) { //初期化
    readings[thisReading] = 0;
    readings2[thisReading] = 0;
    readings3[thisReading] = 0;
  }

  MsTimer2::set(500, flash); // 500ms period
  MsTimer2::start();
}


void loop()
{
  if(before < count){
    File dataFile = SD.open("datalog.txt", FILE_WRITE);

    if (dataFile) {
      dataFile.print("A1ピンの値：");
      dataFile.print(average);
      dataFile.print(",");
      dataFile.print("A2ピンの値：");
      dataFile.print(average2);
      dataFile.print(",");
      dataFile.print("A3ピンの値：");
      dataFile.println(average3);
      dataFile.close();
      // シリアルポートにも出力
//      Serial.print("A1ピンの値：");
//      Serial.print(average);
//      Serial.print(",");
//      Serial.print("A2ピンの値：");
//      Serial.print(average2);
//      Serial.print(",");
//      Serial.print("A3ピンの値：");
//      Serial.println(average3);
    }
    // ファイルが開けなかったらエラーを出力
    else {
      Serial.println(F("error opening datalog.txt"));
    } 
     
  }
  before = ++before;
}
