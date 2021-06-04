#include <Wire.h>

#define I2C_MAX5816_ADDR 0x0f
#define CH 4
#define MEASURETIMES 16//ひとつのDA値につき何回ADを読み込むか
#define CHANNELSIZE 3
#define CANDINATESSIZE 10

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

void set_volt(int ch, int v)
{
  //vは4096(5V) ~ 0(0V)の範囲.volt表記から直す際の量子化誤差を防ぐ.
  set_dac(ch, v);
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
void ADRead_Multipule(int *a){
  int i;

  for(i = 0; i < MEASURETIMES; times++){
    ADRead(*a);
  }

  return (1);
}

int get_properDAvalue(int *candinates, int *voltvals, int channel){
  //DACに入れる適切な値を返す
  int properindex = 0;
  
  for(int i = 0; i < CANDINATESSIZE; i++){
    if (candinates[i] < candinates[i - 1]){
      properindex = i;
    }
  }
          
  return voltvals[channel][properindex];
}


void make_mincandinates(int *candinates, int *voltvals, int volt, int channel){
  //適切な電圧である可能性がある候補点を取得する
  int ADAverage[CHANNELSIZE][CANDINATESSIZE]
  volt = volt - (CANDINATESSIZE >> 2);//変曲点到達後，ある程度戻る
  
  for (int i = 0; i < CANDINATESSIZE; i++){
     set_volt(chanel, volt);
     ADRead_Multipule(ADAverage, MEASURETIMES);
     candinates[channel][i] = ADAverage[channel];
     voltvals[channel][i] = volt;
     
     volt += 1;
  }

  return (1);
}


void setup() {
  int ChannelEnd[3] = {1, 1, 1};
  int Targetvol = 500; //in 10bit value Tuningのゴール,1.5V相当で
  int TuneEnd = 1;
  int init_v = 400;//in 12bit value 4096=5V, 0=0V.
  
  int ADAverage[CHANNELSIZE] = {}; //読み込み値
  int candinates[CHANNELSIZE][CANDINATESSIZE] = {};//変曲点通過以後，最適な電圧である可能性のある電圧値を入れる．
  int voltvals[CHANNELSIZE][CANDINATESSIZE] = {};//変曲点通過以後，最適な電圧を出力しうる時にDAに入力する値の候補値を入れる
  int set_v[CHANNELSIZE] = {};
  
  Serial.begin(57600);
  Wire.begin();
  //  MAX5816_write_command(0x20, 0x00, 0x0f); // POWER all DAC as normal
  //  MAX5816_write_command(0x30, 0x00, 0x0f); // CONFIG all DAC as transparent
  MAX5816_write_command(0x38, 0x00, 0x07); // REF as 4.096V, always ON

  set_volt(4, init_v); //DAの初期設定
  //DAの設定変数に初期値をコピーする
  for (int i = 0; i < CHANNELSIZE; ++i) {
    set_v[i] = init_v;
  }
  
  pinMode(ledPin, OUTPUT);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //チューニング処理開始
  while (TuneEnd) {
    digitalWrite(ledPin, HIGH);
    
    //各チャンネルから読み込み
    ADRead_Multipule(ADAverage, MEASURETIMES);

    //Tuning処理
    for (int channel = 0; channel < CHANNELSIZE; ++channel) {
      
      if (ChannelEnd[channel] != 0) {
        
        //未処理チャンネルのみチューニングを続ける
        Serial.print("Ch ");
        Serial.print(channel);
        Serial.print("");
        Serial.print(",");
        
        if (ADRead_Multipule(ADAverage, MEASURETIMES) > Targetvol){
          set_v[channel] = set_v[channel] += 5;
          set_volt(channel, set_v[channel]);
       }

        if(ADRead_Multipule(ADAverage, MEASURETIMES) < Targetvol){
          make_candinates(candinates, voltvals, set_v[channel], channel);
          properDAvalue[channel] = get_properDAvalue(candinates, voltvals, channel);
          set_v[channel] = properDAvalue[channel];
          set_volt(channel, set_v[channel]);
          
          ChannelEnd[channel] = 1;
       }
      }
    }

    //　全チャンネルの終了チェック
    if (ChannelEnd[0] | ChannelEnd[1] | ChannelEnd[2]) {
      Serial.println("Tuning");    //未調整中
    } else {
      Serial.println("Tuning End:");   //調整終了結果の表示
      
      for (int l = 0; l < CHANNELSIZE; ++l) {
        Serial.print("Ch "); Serial.print(l); Serial.print(": DA-Set ");
        Serial.print(set_v[l], 5); Serial.print(": Current AD");
        Serial.println(ADAverage[l]);
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
  int ADAverage[CHANNELSIZE]; //読み込み値

  //これが真の処理
  //今は読み込み表示のみ

  while (1) {
    //各チャンネルから　Difarraysize回さんぷりんぐ
    ADRead_Multipule(ADAverage, MEASURETIMES);
      
      //平均化計測値表示
    for (i = 0; i < 3; ++i) {
      set_volt(i, set_v[i]);
      Serial.print(": Current AD");
      Serial.print(analogRead(i) & 0x03ff);
      Serial.print(ADAverage[i]);
      Serial.print(",");
      }
    
    Serial.println();
    delay(5);
  }
}
