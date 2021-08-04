/*
   タイトル：MEMS触覚センサ用アンプ(秋田アンプ)の自動チューニングコード　ArduinoIDEシリアルプロッタ用
   説明    :MEMS触覚センサの電圧とDAC(MAX5816)の電圧の差を増幅させるアンプ(秋田アンプ)を自動調整する．調整には秋田アンプの「出力が4Vから0Vに変化するまでに，DACに入れる値は4つ分しか変化しない」
   ことを利用する．

   作成者  ：Sota Tsubokura

   変更履歴：2021 06/04 : ver1.0
            2021 06/07 : ver1.01 bugfix シリアルプロッタ向けにprintの仕方を変更 軸が死んでる時にする処理を追加
            2021 06/07 : ver1.02 軸が死んでる時にする処理の条件判定がミスってたので修正
            2021 06/18 : ver1.1 軸が死んでる時にする処理を改良．軸が死んでいても3軸の値が出力されるように．また軸の状態によってLチカさせるパターンを変える
            2021 08/04 : ver1.11 mainloopでのset_voltを消す，コメントの整理，通常計測時のLチカを止めるなどリファクタリングを実行


*/
#include <Wire.h>
#define I2C_MAX5816_ADDR 0x0f
#define CH 4
#define MEASURETIMES 16//ひとつのDA値につき何回ADを読み込むか
#define CHANNELSIZE 3
#define CANDINATESSIZE 20
#define LEDPIN 13
#define TARGETVOL 400//in 10bit value.1024 = 5V, 0=0V Tuningのゴール．アンプの出力&ArduinoのAD入力範囲が0~4Vなのでその半分くらいが目標．

void MAX5816_write_command(byte cmd, byte dh, byte dl) {
  Wire.beginTransmission(I2C_MAX5816_ADDR);
  Wire.write(cmd);
  Wire.write(dh);
  Wire.write(dl);
  Wire.endTransmission();
}

uint16_t MAX5816_read_reg(byte cmd) {
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
//
//void set_dac(int ch, int v) {
//  MAX5816_write_command(0x18 | ch, v >> 4, v << 4);
//}

void set_volt(int ch, int v) {
  //vは4096(5V) ~ 0(0V)の範囲.volt表記から直す際の量子化誤差を防ぐ.
  MAX5816_write_command(0x18 | ch, v >> 4, v << 4);
}

const int numReadings = 16;
int readings[3][numReadings];      // the readings from the analog input
int readIndex = 0;                 // the index of the current reading
int total[3] = {0, 0, 0};          // the running total
void Read_AD(int *a ) {
  const int Shift = 4;    //上記numReafingsの分、シフトする量。numReafings=2^shift とする。
  
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
void Read_ADMultipule(int ADAverage[MEASURETIMES]) {
  int i;
  for (i = 0; i < MEASURETIMES; i++) {
    Read_AD(ADAverage);
  }
  return (1);
}

//DACに入れる適切な値を返す関数
int get_properDAvalue(int candinates[CHANNELSIZE][CANDINATESSIZE], int voltvals[CHANNELSIZE][CANDINATESSIZE], int channel) {
  int properindex = 0;
  for (int i = 0; i < CANDINATESSIZE; i++) {
    if (candinates[channel][i] < candinates[channel][i - 1]) {
      properindex = i;
    }
  }
  return voltvals[channel][properindex];
}

//適切な電圧を出力する可能性がある候補点を取得する関数
void make_candinates(int candinates[CHANNELSIZE][CANDINATESSIZE], int voltvals[CHANNELSIZE][CANDINATESSIZE], int volt, int channel) {
  int ADAverage[CHANNELSIZE];
  volt = volt - (CANDINATESSIZE >> 1);//変曲点到達後，ある程度戻る　ここでは候補点の数の半分だけ戻る
  for (int i = 0; i < CANDINATESSIZE; i++) {
    set_volt(channel, volt);
    Read_ADMultipule(ADAverage);
    candinates[channel][i] = abs(ADAverage[channel] - TARGETVOL);
    voltvals[channel][i] = volt;
    volt += 1;
  }
  return (1);
}

int ChannelBroken[CHANNELSIZE] = {0, 0, 0};
void setup() {
  int ChannelEnd[CHANNELSIZE] = {1, 1, 1};
  int FallCheck[CHANNELSIZE] = {0, 0, 0};
  int TuneEnd = 1;
  int init_v = 400;//in 12bit value 4096=5V, 0=0V.開始直後はアンプからの出力が上限値になるように値を設定している．
  int ThreshholdTime = 100;//軸が死んでいるのかを判別するのに使う定数．リセット後ある程度はアンプからの出力が0になるっぽいので用いている．
  int cnt = 0;

  int ADAverage[CHANNELSIZE] = {}; //読み込み値
  int candinates[CHANNELSIZE][CANDINATESSIZE] = { {}, {}, {} };//変曲点通過以後，最適な電圧である可能性のある電圧値を入れる配列
  int voltvals[CHANNELSIZE][CANDINATESSIZE] = { {}, {}, {} };//変曲点通過以後，最適な電圧を出力しうる時にDAに入力する値の候補値を入れる配列
  int properDAvalue[CHANNELSIZE] = { {}, {}, {} }; //最適な電圧を出力しうる時にDAに入力する値を入れる配列
  int set_v[CHANNELSIZE] = {0, 0, 0};//DACに入れる値を格納する配列
  
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
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);
  
  //チューニング処理
  while (TuneEnd) {
    cnt++;
    Read_ADMultipule(ADAverage); //各チャンネルから読み込み．チューニング中はLEDをオン
    for (int channel = 0; channel < CHANNELSIZE; ++channel) {
      //カンチレバーのどれかの軸が死んでいたら( = 初期値が0に近い値ならば)Lチカさせて知らせる 10回ADから0が返ってきたら死んでる扱いしてチューニング処理をやめさせる．
      //なお，リセット後はアンプからの出力が0になるのでリセット後ある程度経ってから確認することにしている．
      if (ThreshholdTime < cnt && cnt <  ThreshholdTime + 100 && ADAverage[channel] < 5) {
          digitalWrite(LEDPIN, LOW);
          delay(10);
          digitalWrite(LEDPIN, HIGH);
          delay(10);
          FallCheck[channel] += 1;  
          if (FallCheck[channel] > 50){     
            ChannelEnd[channel] = 0;
            ChannelBroken[channel]  = 1;      
        }
      }

      //未処理チャンネルのみチューニングを続ける
      if (ChannelEnd[channel] != 0) {
//        Serial.print("Ch"); 調整されているかどうか様子を見るためのコード群．現在はコメントアウトしている
//        Serial.print(channel);
//        Serial.print(":");
//        Serial.print(ADAverage[channel]);
//        Serial.print(" ");
//        Serial.println(set_v[channel]);
        if (ADAverage[channel] > TARGETVOL) {
          set_v[channel] = set_v[channel] += 5;
          set_volt(channel, set_v[channel]);
        }

        if (ADAverage[channel] < TARGETVOL) {
//          Serial.println("channel");調整が終わったか様子を見るためのコード群．現在はコメントアウトしている
//          Serial.println(channel);
//          Serial.println(ChannelEnd[channel]);
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
      //      Serial.println("Tuning");    //未調整中を示す．現在はコメントアウトしている
    } else {
      //      Serial.println("Tuning End:");   //調整終了結果を表示する　現在はコメントアウトしている
      for (int l = 0; l < CHANNELSIZE; ++l) {
        //        Serial.print("Ch "); Serial.print(l); Serial.print(": DA-Set ");
        //        Serial.print(set_v[l]); Serial.print(": Current AD");
        //        Serial.println(ADAverage[l]);
      }
      TuneEnd = 0;
      digitalWrite(LEDPIN, LOW);
      break;//チューニング終了
    }
    delay(1);
  }
}

void loop() {
  int ADAverage[CHANNELSIZE] = {};//読み込み値を格納する配列
  int FallCheck[CHANNELSIZE] = {};//チャンネルが死んだかどうか確認する用の配列
  int channledie_cnt = 0;
  int Threshhord = 390;//アンプからの出力が適切かどうか判断するための閾値
  int fallcheck_cnt = 0;

  while (true) {
    //各チャンネルからサンプリング
    Read_AD(ADAverage);

    //平均化計測値表示　このコードはArduinoIDEのシリアルプロッタ用なのでそれを使えば変化が分かりやすい
    for (int i = 0; i < 3; ++i) {
      Serial.print(ADAverage[i]);
      Serial.print(" ,");
    }
    Serial.println(" ");

    //死んでるチャンネルがあるかどうかでLEDの点滅パターンを決める　現在はどれも死んでなかったらLチカしないこととしている．
  if (ChannelBroken[0] | ChannelBroken[1] | ChannelBroken[2]) {
    if (channledie_cnt == 0){
      digitalWrite(LEDPIN, HIGH);
    }
    if (channledie_cnt == 20){
      digitalWrite(LEDPIN, LOW);
    }
    ++channledie_cnt;
    if (channledie_cnt >= 40){
      channledie_cnt = 0;
    }
    
  }else{
    
    if (channledie_cnt == 0){
//      digitalWrite(LEDPIN, HIGH);
    }
    if (channledie_cnt == 200){
//      digitalWrite(LEDPIN, LOW);
    }
    ++channledie_cnt;
    if (channledie_cnt >= 400){
      channledie_cnt = 0;
    }
  }

  if(channledie_cnt > 50){
    for (int i = 0; i < CHANNELSIZE; i++) {
      if (abs(ADAverage[i] - TARGETVOL) > Threshhord) {
        FallCheck[i] += 1;
      }
      if(FallCheck[i] > 50){
        ChannelBroken[i] = 1;
    }
   }
  }
 }
}
