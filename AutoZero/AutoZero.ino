/*
   タイトル：MEMS触覚センサ用アンプ(秋田アンプ)の自動チューニングコード　ArduinoIDEシリアルプロッタ用
   説明    :MEMS触覚センサの電圧とDAC(MAX5816)の電圧の差を増幅させるアンプ(秋田アンプ)を自動調整する．調整には秋田アンプの「出力が4Vから0Vに変化するまでに，DACに入れる値は4つ分しか変化しない」
   ことを利用する．

   作成者  ：Sota Tsubokura

   変更履歴：2021 06/04 : ver1.0
            2021 06/07 : ver1.01 bugfix シリアルプロッタ向けにprintの仕方を変更 軸が死んでる時にする処理を追加
            2021 06/07 : ver1.02 軸が死んでる時にする処理の条件判定がミスってたので修正
            2021 06/18 : ver1.1 軸が死んでる時にする処理を改良．軸が死んでいても3軸の値が出力されるように．また軸の状態によってLチカさせるパターンを変える
            2021 08/04 : ver1.1.1 シチズン社提供用に整形

*/
#include <Wire.h>

//  DAC用の制御コード

#define I2C_MAX5816_ADDR 0x0f
#define CH 4
#define MEASURETIMES 16//ひとつのDA値につき何回ADを読み込むか
#define CHANNELSIZE 3
#define CANDINATESSIZE 20
#define TARGETVOL 400//2V in 10bit value.1024 = 5V, 0=0V Tuningのゴール．アンプの出力&ArduinoのAD入力範囲が0~4Vなのでその半分くらいが目標．

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

void set_dac(int ch, int v) {
  MAX5816_write_command(0x18 | ch, v >> 4, v << 4);
}

void set_volt(int ch, int v) {
  //vは4096(5V) ~ 0(0V)の範囲.volt表記から直す際の量子化誤差を防ぐ.
  set_dac(ch, v);
}


// ADコンバータの時間閉活用のリングバッファ
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

//ADを複数回読む、時間平均をウィンドウラップしない

void Read_ADMultipule(int ADAverage[MEASURETIMES]) {
  int i;

  for (i = 0; i < MEASURETIMES; i++) {
    Read_AD(ADAverage);
  }

  return (1);
}

//DACに入れる適切な値を返す関数
//int candinates[CHANNELSIZE][CANDINATESSIZE]　候補となるアンプ出力値
//int voltvals[CHANNELSIZE][CANDINATESSIZE]　ペアとなるDAC値
//int channel  ：調整するチャンネル

int get_properDAvalue(int candinates[CHANNELSIZE][CANDINATESSIZE], int voltvals[CHANNELSIZE][CANDINATESSIZE], int channel) {
  int properindex = 0;
 
  // CANDINATESSIZE(20)の候補の中でもっとも目標値との誤差の少ない設定を探す
  // 絶対値で獲っているから，最初は大きく、最少を経てまた大きくなるから
  
  for (int i = 0; i < CANDINATESSIZE; i++) {
    if (candinates[channel][i] < candinates[channel][i - 1]) {
      properindex = i;
    }
  }

  //もっとも誤差の大きいDACの設定値を返す.
  return voltvals[channel][properindex];
}

//適切な電圧を出力する可能性がある候補点を取得する関数
//CANDINATESSIZE 20　候補点数　（単位はDACのLSB)
//int candinates[CHANNELSIZE][CANDINATESSIZE]　出力の設定目標値との差の絶対値を記録
//int voltvals[CHANNELSIZE][CANDINATESSIZE], 　設定の際のDAC値
//int volt：　4−＞0V　に出力が変わったときのDAC設定値（0−4096で5LSBづつ増加している）
//int channel：調整するチャンネル

void make_candinates(int candinates[CHANNELSIZE][CANDINATESSIZE], int voltvals[CHANNELSIZE][CANDINATESSIZE], int volt, int channel) {
  int ADAverage[CHANNELSIZE];
  volt = volt - (CANDINATESSIZE >> 1);//変曲点到達後，ある程度戻る　ここでは候補点の数の半分の10点だけ戻る.2.459Vだったら2.449Vにする
                                      //DACのLSBで4V->0Vに変化するのは4LSBくらい.最初の探索で5LSB動いている.
                                      //DACのLSBでCANDINATESSIZE（20LSB）分の設定目標からの誤差を詳しく調べていく
  for (int i = 0; i < CANDINATESSIZE; i++) {
    set_volt(channel, volt);   //　DACに初期値設定
    Read_ADMultipule(ADAverage); // ADよみとり
    candinates[channel][i] = abs(ADAverage[channel] - TARGETVOL); //設定目標との差を記録しておく
    voltvals[channel][i] = volt; // 設定値を記録しておく

    volt += 1;  //DACを1LSB(1mV)を大きくする
  }

  return (1);
}

//  初期化処理

int set_v[CHANNELSIZE] = {0, 0, 0};
int ChannelBroken[CHANNELSIZE] = {0, 0, 0};
void setup() {
  int ChannelEnd[CHANNELSIZE] = {1, 1, 1};
  int FallCheck[CHANNELSIZE] = {0, 0, 0};
  int TuneEnd = 1;
  int init_v = 400;//2Vくらい in 12bit value 4096=5V, 0=0V.開始直後はアンプからの出力が上限値になるようにする
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

  set_volt(4, init_v); //DAの初期設定、すべてのチャンネルに適用

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
      if (ThreshholdTime < cnt && cnt <  ThreshholdTime + 100 && ADAverage[channel] < 5) {


        //10回ADから0が返ってきたら死んでる扱いしてチューニング処理をやめさせる．
        digitalWrite(ledPin, LOW);
        delay(10);
        digitalWrite(ledPin, HIGH);
        delay(10);
        FallCheck[channel] += 1;

        if (FallCheck[channel] > 80) {

          ChannelEnd[channel] = 0;
          ChannelBroken[channel]  = 1;

        }
      }


      if (ChannelEnd[channel] != 0) {
        //        未処理チャンネルのみチューニングを続ける

        //　初期は4Vが出力なんで，DACを2Vからちょっとづつあげていく
        if (ADAverage[channel] > TARGETVOL) {
          set_v[channel] += 5; // 4096LSBで0−5V　5LSBは　
          set_volt(channel, set_v[channel]);
        }
        //  DAC出力が適正値を超えると出力が0Vに張り付くと・・・ちょっと戻してDACを適性値にする
        if (ADAverage[channel] < TARGETVOL) {
            // 4V-0Vに完全に変化する変曲点の手前に2Vの出力になるDAC値があるはず
            
            // 過去の20LSBに目標値である約2Vの出力を行うDACがあるんで，疎の候補を列挙する
          make_candinates(candinates, voltvals, set_v[channel], channel);
          
            // 候補からもっとも目標値に近い出力を出すDAC値を探索する
          properDAvalue[channel] = get_properDAvalue(candinates, voltvals, channel);
          
          set_v[channel] = properDAvalue[channel];   // 適性値を保管する
          set_volt(channel, set_v[channel]);         // 適性値をDACにセットする
          ChannelEnd[channel] = 0;                   //　当該チャンネルの設定終了

        }
      }
    }

    //　全チャンネルの終了チェック
    if (ChannelEnd[0] | ChannelEnd[1] | ChannelEnd[2]) {
      //      Serial.println("Tuning");    //未調整中
    } else {
      Serial.println("Tuning End:");   //調整終了結果の表示

//    for (int l = 0; l < CHANNELSIZE; ++l) {
//        Serial.print("Ch "); Serial.print(l); Serial.print(": DA-Set ");
//        Serial.print(set_v[l]); Serial.print(": Current AD");
//        Serial.println(ADAverage[l]);
//    }

      TuneEnd = 0;
      digitalWrite(ledPin, LOW);
      break;//Tuning 終了
    }

    delay(1);
  }
}

void loop() {
  int ADAverage[CHANNELSIZE] = {}; //読み込み値
  int FallCheck[CHANNELSIZE] = {};

  int stat_cnt = 0;     // LED点滅用カウンタ
  int Threshhord = 390; // 2Vに相当
  int fallcheck_cnt = 0;// 断線チェック用カウンタ

  //これが真の処理
  //今は読み込み表示のみ

  while (1) {
    //各チャンネルからサンプリング
    Read_AD(ADAverage);

    //平均化計測値表示　ArduinoIDEのシリアルプロッタを使えば変化がわかりやすい
    for (int i = 0; i < 3; ++i) {
      //      set_volt(i, set_v[i]);
      Serial.print(ADAverage[i]);
      Serial.print(" ,");
    }

    Serial.println(" ");

    //全部生きていると高速点滅，ひとつでも死んでいるとゆっくり点滅

    if (ChannelBroken[0] | ChannelBroken[1] | ChannelBroken[2]) {

      if (stat_cnt == 0) {
        digitalWrite(ledPin, HIGH);
      }

      if (stat_cnt  == 20) {
        digitalWrite(ledPin, LOW);
      }

      ++stat_cnt;

      if (stat_cnt  >= 40) {
        stat_cnt = 0;
      }

    } else {

      if (stat_cnt == 0) {
        digitalWrite(ledPin, HIGH);
      }

      if (stat_cnt  == 200) {
        digitalWrite(ledPin, LOW);
      }

      ++stat_cnt;

      if (stat_cnt  >= 400) {
        stat_cnt = 0;
      }
    }

    //追加で死んでいないか確認する

    if (stat_cnt > 50) {

      for (int i = 0; i < CHANNELSIZE; i++) {
        if (abs(ADAverage[i] - TARGETVOL) > Threshhord) { // 加圧による変動は0.5V (100LSB程度)だから
          FallCheck[i] += 1;
        }

        if (FallCheck[i] > 50) {
          ChannelBroken[i] = 1;

        }
      }

    }

  }
}
