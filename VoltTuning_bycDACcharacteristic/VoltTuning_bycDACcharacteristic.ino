/*
 * タイトル：MEMS触覚センサ用アンプ(秋田アンプ)の自動チューニングコード
 * 説明    :MEMS触覚センサの電圧とDAC(MAX5816)の電圧の差を増幅させるアンプ(秋田アンプ)を自動調整する．調整には秋田アンプの「出力が4Vから0Vに変化するまでに，DACに入れる値は4つ分しか変化しない」
 * ことを利用する．
 *
 * 作成者  ：Sota Tsubokura
 *
 * 変更履歴：2021 06/04 : ver1.0 
 *          2021 06/07 : ver1.01 bugfix シリアルプロッタ向けにprintの仕方を変更 軸が死んでる時にする処理を追加
 *          2021 06/07 : ver1.02 軸が死んでる時にする処理の条件判定がミスってたので修正
 *        
 *
 */
#include <Wire.h>

#define I2C_MAX5816_ADDR 0x0f
#define CH 4
#define MEASURETIMES 16//ひとつのDA値につき何回ADを読み込むか
#define CHANNELSIZE 3
#define CANDINATESSIZE 20
#define TARGETVOL 400//in 10bit value.1024 = 5V, 0=0V Tuningのゴール．アンプの出力&ArduinoのAD入力範囲が0~4Vなのでその半分くらいが目標．

void MAX5816_write_command(byte cmd, byte dh, byte dl){
  Wire.beginTransmission(I2C_MAX5816_ADDR);
  Wire.write(cmd);
  Wire.write(dh);
  Wire.write(dl);
  Wire.endTransmission();
}

uint16_t MAX5816_read_reg(byte cmd){
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

void set_dac(int ch, int v){
  MAX5816_write_command(0x18 | ch, v >> 4, v << 4);
}

void set_volt(int ch, int v){
  //vは4096(5V) ~ 0(0V)の範囲.volt表記から直す際の量子化誤差を防ぐ.
  set_dac(ch, v);
}

const int numReadings = 16;
const int Shift = 4;    //上記numReafingsの分、シフトする量。numReafings=2^shift とする。
int readings[3][numReadings];      // the readings from the analog input
int readIndex = 0;                 // the index of the current reading
int total[3] = {0, 0, 0};          // the running total
int ledPin = 13;      

void Read_AD(int *a ) {
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
void Read_ADMultipule(int ADAverage[MEASURETIMES]){
  int i;

  for(i = 0; i < MEASURETIMES; i++){
    Read_AD(ADAverage);
  }

  return (1);
}

//DACに入れる適切な値を返す関数
int get_properDAvalue(int candinates[CHANNELSIZE][CANDINATESSIZE], int voltvals[CHANNELSIZE][CANDINATESSIZE], int channel){
  int properindex = 0;
  
  for(int i = 0; i < CANDINATESSIZE; i++){
    if (candinates[channel][i] < candinates[channel][i -1]){
      properindex = i;
    }
  }
          
  return voltvals[channel][properindex];
}

//適切な電圧を出力する可能性がある候補点を取得する関数
void make_candinates(int candinates[CHANNELSIZE][CANDINATESSIZE], int voltvals[CHANNELSIZE][CANDINATESSIZE], int volt, int channel){
  int ADAverage[CHANNELSIZE];
  volt = volt - (CANDINATESSIZE >> 1);//変曲点到達後，ある程度戻る　ここでは候補点の数の半分だけ戻る
  
  for (int i = 0; i < CANDINATESSIZE; i++){
     set_volt(channel, volt);
     Read_ADMultipule(ADAverage);
     candinates[channel][i] = abs(ADAverage[channel] - TARGETVOL);
     voltvals[channel][i] = volt;
     
     volt += 1;
  }

  return (1);
}


//カンチレバーのどれかの軸が目標値からある程度外れたらLEDで知らせる関数 開発中
void Check_ADValueinMeasuring(int ADAverage[CHANNELSIZE]){
  int Threshhord = 300;

  for (int i = 0; i < CHANNELSIZE; i++){
    if (abs(ADAverage[i] - TARGETVOL) > Threshhord){
      while(1){
        digitalWrite(ledPin, LOW); 
        delay(100);
        digitalWrite(ledPin, HIGH);
      }
    }
  }
  
  return (1);
}



int set_v[CHANNELSIZE] = {};
void setup() {
  int ChannelEnd[3] = {1, 1, 1};
  int TuneEnd = 1;
  int init_v = 600;//in 12bit value 4096=5V, 0=0V.開始直後はアンプからの出力が上限値になるようにする
  int cnt = 0;
  int ThreshholdTime = 100;//軸が死んでいるのかを判別するのに使う閾値．起動後ある程度はアンプからの出力が0になるっぽいので用いている．
  
  int ADAverage[CHANNELSIZE] = {}; //読み込み値
  int candinates[CHANNELSIZE][CANDINATESSIZE] = { {}, {}, {} };//変曲点通過以後，最適な電圧である可能性のある電圧値を入れる．
  int voltvals[CHANNELSIZE][CANDINATESSIZE] = { {}, {}, {} };//変曲点通過以後，最適な電圧を出力しうる時にDAに入力する値の候補値を入れる
  int properDAvalue[CHANNELSIZE] = { {}, {}, {} }; //最適な電圧を出力しうる時にDAに入力する値
  
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

  digitalWrite(ledPin, HIGH);
  //チューニング処理開始
  while (TuneEnd) {
    cnt++;
    //各チャンネルから読み込み．チューニング中はLEDをオン
    
    Read_ADMultipule(ADAverage);

    //Tuning処理
    for (int channel = 0; channel < CHANNELSIZE; ++channel) {
      
      
      //カンチレバーのどれかの軸が死んでいたら( = 初期値が0ならば)Lチカさせて知らせる
      if(ThreshholdTime < cnt && cnt <  ThreshholdTime + 100 && ADAverage[channel] == 0){
          while(1){
            digitalWrite(ledPin, LOW); 
            delay(100);
            digitalWrite(ledPin, HIGH);
            delay(100);
          }
       }
       
      if (ChannelEnd[channel] != 0) {
        
//        未処理チャンネルのみチューニングを続ける
//        Serial.print("Ch");
//        Serial.print(channel);
//        Serial.print(":");
//        Serial.print(ADAverage[channel]);
//        Serial.print(" ");
//
//        Serial.println(set_v[channel]);
        
        if (ADAverage[channel] > TARGETVOL){
          set_v[channel] = set_v[channel] += 5;
          set_volt(channel, set_v[channel]);
       }

        if(ADAverage[channel] < TARGETVOL){
          make_candinates(candinates, voltvals, set_v[channel], channel);
          properDAvalue[channel] = get_properDAvalue(candinates, voltvals, channel);
          set_v[channel] = properDAvalue[channel];
          set_volt(channel, set_v[channel]);
          ChannelEnd[channel] = 0;
       }

       }
       
      
    }

    //　全チャンネルの終了チェック
    if (ChannelEnd[0] | ChannelEnd[1] | ChannelEnd[2]) {
//      Serial.println("Tuning");    //未調整中
    } else {
//      Serial.println("Tuning End:");   //調整終了結果の表示
      
      for (int l = 0; l < CHANNELSIZE; ++l) {
//        Serial.print("Ch "); Serial.print(l); Serial.print(": DA-Set ");
//        Serial.print(set_v[l]); Serial.print(": Current AD");
//        Serial.println(ADAverage[l]);
      }
      
      TuneEnd = 0;
      digitalWrite(ledPin, LOW); 
      break;//Tuning 終了
    }
    
    delay(1);
  } 
}

void loop() {
  int ADAverage[CHANNELSIZE]; //読み込み値

  //これが真の処理
  //今は読み込み表示のみ
  
    while(1){
    //各チャンネルからサンプリング
      Read_AD(ADAverage);
  
        //平均化計測値表示　ArduinoIDEのシリアルプロッタを使えば変化がわかりやすい
      for (int i = 0; i < 3; ++i) {
        set_volt(i, set_v[i]);
        Serial.print(ADAverage[i]);
        Serial.print(" ,");
        }
        
        Serial.println(" ");
    }
  
}
