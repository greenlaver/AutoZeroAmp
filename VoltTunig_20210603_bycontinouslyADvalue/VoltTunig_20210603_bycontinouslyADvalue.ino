#include <Wire.h>

#define I2C_MAX5816_ADDR 0x0f
#define CH 4


void MAX5816_write_command(byte cmd, byte dh, byte dl)
{
  //  M5.begin();
  //  M5.Power.begin();
  Wire.beginTransmission(I2C_MAX5816_ADDR);
  Wire.write(cmd);
  Wire.write(dh);
  Wire.write(dl);
  Wire.endTransmission();
}

uint16_t MAX5816_read_reg(byte cmd)
{
  uint8_t dh, dl;
  Wire.beginTransmission(I2C_MAX5816_ADDR);
  Wire.write(cmd);
  Wire.endTransmission(false);
  Wire.requestFrom(I2C_MAX5816_ADDR, 2);
  while (!Wire.available()); dh = Wire.read();
  while (!Wire.available()); dl = Wire.read();
  Wire.endTransmission();
  return (dh << 8 | dl);

}

void set_dac(int ch, int v)
{
  MAX5816_write_command(0x18 | ch, v >> 4, v << 4);
}

void set_volt(int ch, float v)
{
  // Vref = 4.096V / 1LSB=1mV
  set_dac(ch, (v * 1000));
}

const int numReadings = 16;
const int Shift = 4;    //上記numReafingsの分、シフトする量。numReafings=2^shift とする。
int readings[3][numReadings];      // the readings from the analog input
int readIndex = 0;                 // the index of the current reading
int total[3] = {0, 0, 0};          // the running total
int ledPin = 13;      

void ADRead(int *a ) {
  int i, j;

  
  for (i = 0; i < 3; i++) {
    total[i] = total[i] - readings[i][readIndex];          // subtract the last reading:
    readings[i][readIndex] = analogRead(i) & 0x03ff;  
    total[i] = total[i] + readings[i][readIndex];          // add the reading to the total:
  }
  
  readIndex = readIndex + 1;                    // advance to the next position in the array:
  if (readIndex >= numReadings) {               // if we're at the end of the array...
    readIndex = 0;                              // ...wrap around to the beginning:
  }

  for (i = 0; i < 3; i++) {
    a[i] = total[i] >> Shift;                  // calculate the average:
  }
  return (1);
}

//ADを複数回読む操作をwrapする関数
void ADRead_Multipule(int *a, int times){
  int i;

  for(i = 0; i<times; times++){
    ADRead(*a);
  }

  return (1);
}

float set_v[3];

int ADAvarege[3]; 

void setup() {
  int i, j, k, l;
  int ChannelEnd[3] = {1, 1, 1};
  int Threshold = 2; //Tunengのゴールとなる誤差　bit表記、”5”で　5*1000/1024*5=25mV　くらい
  int Targetvol = 500; //in 10bit value Tuningのゴール,1.5V相当で
  int TuneEnd = 1;
  int channelnum = 3;
  int ADvalue = 0;
  int index = 0;
  int measuretimes = 16;//一つのDA値につき何回ADを読み込み時間フィルタをかけるか
  int times = 0;
  int ADAvarege[3]; //読み込み値
  int beforeval[3];//今読み込んでいる一つ前の読み込み値
  int difference[3]  = {};//変曲点通過以前に取得する前後一つずつのDA値に対するAD値の差分
  int differences[3][10] = {};//変曲点通過以後，最適な電圧を出力しうる時のAD値とその目標値の差分を入れる
  int voltvals[3][10]= {};//変曲点通過以後，最適な電圧を出力しうる時にDAに入力する値の候補値を入れる
  int differencesnum = 10;//最適な電圧を出力する時のDA値
  
  float init_v = 1.0;
  float targetDAvol[3]= {};
  

  Serial.begin(57600);
  Wire.begin();
  //  MAX5816_write_command(0x20, 0x00, 0x0f); // POWER all DAC as normal
  //  MAX5816_write_command(0x30, 0x00, 0x0f); // CONFIG all DAC as transparent
  MAX5816_write_command(0x38, 0x00, 0x07); // REF as 4.096V, always ON

  set_volt(4, init_v); //DAの初期設定
  //DAの設定変数に初期値をコピーする
  for (i = 0; i < 3; ++i) {
    set_v[i] = init_v;
  }
  
  pinMode(ledPin, OUTPUT);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //チューニング処理開始
  while (TuneEnd) {
    digitalWrite(ledPin, HIGH);
    times++;
    //各チャンネルから読み込み
    ADRead_Multipule(ADAvarege, measuretimes);

    //readings[i][readIndex]に変な値が残るのを防ぐための処理
    if (times < 50){
       continue;
    }

    
    //Tuning処理
    for (j = 0; j < 3; ++j) {
      
      if (ChannelEnd[j] != 0) {
        
        //未処理チャンネルのみチューニングを続ける
        Serial.print("Ch ");
        Serial.print(j);
        Serial.print(" ");
        Serial.print(",");
        Serial.print(difference[j]);

        if(-5 < difference[j] && difference[j] < 5){
        //変曲点到達以前の処理 到達以前は各DAの値に対してoutputはほぼかわらない
        difference[j] = ADAvarege[j] - beforeval[j];
        beforeval[j] = ADAvarege[j];
        set_v[j] = set_v[j] += 0.005;
        set_volt(j, set_v[j]);
        Serial.print(difference[j]);
        
        } else {
          //変曲点到達以降の処理 変曲点到達時にDA値を少し戻す
          set_v[j] = set_v[j] -= 0.005;  
          
          for(int n = 0; n < differencesnum; n++){
            
            if (n < differencesnum){
              //targetvolとの差が最小であるDA値の候補をdifferencesnum個集める
              differences[j][n] = abs(ADAvarege[j] - Targetvol);
              voltvals[j][n] = set_v[j];
              set_v[j] = set_v[j] += 0.001;
              set_volt(j, set_v[j]);
            } 
  
            if (n == differencesnum - 1){
              for (k = 0;k < differencesnum; k++){
                if (differences[j][k +1]  < differences[j][k]){
                  targetDAvol[j] = voltvals[j][k];
                }
                if (k == differencesnum - 1){
                  ChannelEnd[j] = 0;//channel j のTuning終了
                  break;
                }
              }
            }
          
          }
      }
      
    }
   }
  
   

    //　全チャンネルの終了チェック
    if (ChannelEnd[0] | ChannelEnd[1] | ChannelEnd[2]) {
      Serial.println("Tuning");    //未調整中
    } else {
      Serial.println("Tuning End:");   //調整終了結果の表示
      
      for (l = 0; l < 3; ++l) {
        Serial.print("Ch "); Serial.print(l); Serial.print(": DA-Set ");
        Serial.print(set_v[l], 5); Serial.print(": Current AD");
        Serial.println(ADAvarege[l]);
      }
      
      TuneEnd = 0;
      digitalWrite(ledPin, LOW); 
      break;//Tuning 終了
    }
    
    delay(5);
  } 
}



void loop() {
  int i, j;
  int ADAvarege[3]; //読み込み値
  int measuretimes = 16;

  //これが真の処理
  //今は読み込み表示のみ

  while (1) {
    //各チャンネルから　Difarraysize回さんぷりんぐ
    ADRead_Multipule(ADAvarege, measuretimes);
      
      //平均化計測値表示
    for (i = 0; i < 3; ++i) {
      set_volt(i, set_v[i]);
      Serial.print(": Current AD");
      Serial.print(analogRead(i) & 0x03ff);
      Serial.print(ADAvarege[i]);
      Serial.print(",");
      }
    
    Serial.println();
    delay(5);
  }
}
