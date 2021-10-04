/*
Fsamp4のDACを手動で調整するコード
2021_9/2（木） 記入者：船橋　佑　新Fsamp4基板（赤色）の動作確認に使用
2021_9/24（金）記入者：船橋　佑　1チャンネルだけ自動でDAC調整するコード作成。二分法で行う。未完成である。
2021_9/25（土）記入者：船橋　佑　1チャンネルだけ自動でDAC調整するコード作成が完成。原因はおそらくset_volt後にすぐReadAD2をしたから。間にdelay()を入れると正常に動作した。
　　　　　　　　　　　　　　　　　 追記：3チャンネル同時自動調整コードが完成した。あとはコードの関数化と計測中のエラーをLED点灯で知らせるよう追加する予定。
2021_9/27（月）記入者：船橋　佑　3チャンネル同時自動調整コードの関数化に成功。自動計測。キーボードでsを打つと計測終了。
　　　　　　　　　　　　　　　　　 追記：LEDコード追加。調整中は点灯しており、各chごとに調整が終わると消灯する。
2021_9/28（火）記入者：船橋　佑　コードをreadableにするべく整形した。具体的に、むやみにグローバル変数を使用せず、#defineで定義をなるべくした。また、3チャンネル分一気に調整するのではなく。調整と3チャンネルに行う処理を分けた。
2021_9/29（水）記入者：船橋　佑　さらに整形。staticを使用することで、関数の中でしか使用しないが値を保持したい変数を関数の中で定義した。
                             追記：計測中に各チャンネルに対応したボタンを押すことで再調整するコード追加。
2021_10/1（金）記入者：船橋　佑　現在未完成。エラー処理は正しく動いたが、今度は正常時の挙動がおかしくなった。
 */


//AD Converter 拡張
// 2021/07/12
// Haruo Noma

//2021_7/13に船橋がコメント追加
//2021_9/7に船橋がコメント追加

#include <Wire.h>
//#include <FSamp5.h>

//#define USE_MAX5816
#define USE_AD5696

#if defined(USE_MAX5696)
#define I2C_ADDR 0x0f
#elif defined(USE_AD5696)
#define I2C_ADDR 0x0c // A1=A0=0
#endif

#define LED0 2
#define LED1 3
#define LED2 4

#define SW0 5
#define SW1 6
#define SW2 7

#define REFA 1
#define REFB 2
#define REFC 4
#define EXCITE 8

#define VE 4.5
#define INITvl 2.0  //DACの初期値であり、二分法の下限を設定。目安はVEの半分よりも少ないくらい。
#define INITvh 2.5  //DACの二分法の上限の値。

int LED[] = {LED0, LED1, LED2};           //Arduinoのピン配置でそれぞれ2番ピン、3番ピン、4番ピンに当たる。
int CH[] = {REFA, REFB, REFC, EXCITE};    //調整するDACの場所を変更　　CH 1だとrefA  CH 2だとrefB 　CH 4だとrefC  CH 8だとVE    15だと全部（詳しくはGitのFsamp4の部品資料に記載。）
int BUTTON[] = {SW0, SW1, SW2};


void DAC_write_command(byte cmd, byte dh, byte dl)
{
  //  Serial.print(cmd, HEX); Serial.print(' '); Serial.print(dh, HEX); Serial.print(' '); Serial.println(dl, HEX);
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(cmd);
  Wire.write(dh);
  Wire.write(dl);
  Wire.endTransmission();
}


uint16_t DAC_read_reg(byte cmd)
{
  uint8_t dh, dl;
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(cmd);
  Wire.endTransmission(false);
  Wire.requestFrom(I2C_ADDR, 2);
  while (!Wire.available()); dh = Wire.read();
  while (!Wire.available()); dl = Wire.read();
  Wire.endTransmission();
  return (dh << 8 | dl);
}


void set_dac(int ch, unsigned int v)
{
#ifdef defined(USE_MAX5696)
  DAC_write_command(0x18 | ch, v >> 4, v << 4);
#elif defined(USE_AD5696)
  DAC_write_command(0x30 | (ch), v >> 8, v & 0xff);   //+1消しました
#endif
}


void set_volt(int ch, double v)
{
#if defined(USE_MAX5696)
  // Vref = 4.096V / 1LSB=1mV
  set_dac(ch, (unsigned int)(v * 1000));
#elif defined(USE_AD5696)
  // Vref=2.5V, Gain=2(GAIN=1) -> Vout = 5.0 * (data / 2^16)
  // 1LSB=5V/2^16=76uV
  set_dac(ch, (unsigned int)((v / 5.0) * 65535.0));
#endif
}


//計測中のDACの値を読み取る関数。リングバッファを使用
void Read_AD(int *a ) {    
  int i, j;
  const int numReadings = 16;
  const int Shift = 4;    //上記numReafingsの分、シフトする量。numReafings=2^shift とする。
  static int readings[3][numReadings];      // the readings from the analog input
  static int readings2[3][numReadings];      // the readings from the analog input
  static int readIndex = 0;                 // the index of the current reading
  static int readIndex2 = 0;                 // the index of the current reading
  static int total[3] = {0, 0, 0};          // the running total
  static int total2[3] = {0, 0, 0};          // the running total

  for (i = 0, j = 2; i < 3; i++,j--) {    //アンプの出力OUTAがA2に入り、OUTCがA0に入っているため配列に入れる順番を逆さにして入れている（詳しくは回路図参照）
    total[i] = total[i] - readings[i][readIndex];          // subtract the last reading:
    readings[i][readIndex] = analogRead(j) & 0x03ff;
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


//DAC調整時のDACの値を読み取る関数。過去のデータを参照せずにフィルタリングしてデータを格納している。
void Read_AD2(int *a ) {  
  int i, j, k;
  int stack[3]={0,0,0};
  const int STACK_NUM=8;

  for(k=0;k<STACK_NUM;++k){
    for (i = 0, j = 2; i < 3; i++,j--) {  //アンプの出力OUTAがA2に入り、OUTCがA0に入っているため配列に入れる順番を逆さにして入れている（詳しくは回路図参照）
      stack[i] += analogRead(j) & 0x03ff;
    }
  }
  for (i = 0; i < 3; i++) {
    a[i]=stack[i]/STACK_NUM;
  }
}

//エラー時に呼び出される関数。LED点滅してどのチャンネルが壊れているか知らせる。
void error(int error_channel){
  int i;
  for(i=0; i<5; i++){
    digitalWrite(LED[error_channel],LOW);
    delay(500);
    digitalWrite(LED[error_channel],HIGH);
    delay(500);
  }
  return;
}


//DAC調整関数。二分法によって求める
int auto_control(int channel){    
  float v_low;                  //二分法を行う時のDACの下限を入れる変数
  float v_high;                  //二分法を行う時のDACの上限を入れる変数
  float v_middle;                  //二分法を行う時の中点
  int VE_half_bit = VE/2*1024/5;   //求めたい値（VE=4.5Vの半分の時）のADの値をbitで表記;
  int z= 10;                 //目標値の誤差範囲をbitで表記
  int ADData[3];             //読み取ったデータを格納する配列。
  int control_counter = 0;       //何回調整をしたかカウントする。エラー処理時に使用
  
  Read_AD2(ADData);          //初期化した値をとってくる     
  v_low = INITvl;
  v_high = INITvh;
  while(abs(VE_half_bit-ADData[channel]) > z){
    v_middle = (v_high+v_low)/2;    //2点の中点をとってくる
    set_volt(CH[channel], v_middle);
    delay(50);         //delay()がないとset_voltがすぐ反映されない。
    Read_AD2(ADData);  //ADの電圧読み取りメソッド呼び出し（調整用）
    if(ADData[channel] >= VE_half_bit){  //とった中点のAD出力が 目標値(460bit)よりも高い場合は
      v_low = v_middle;               //中点を下限に
    }
    else {                   //そうでなければ
      v_high = v_middle;               //中点を上限に
    }

    if(ADData[channel] == 0){
      control_counter++;
    }
    
    if(control_counter == 20){
      return(-1);
    }
    
  }
  digitalWrite(LED[channel], LOW);     //調整が終わったら、そのchに対応したLEDを消灯
}


//DACの調整を3チャンネル分行う
int auto_zero_all() {
  for(int ch=0; ch<3; ch++){
    auto_control(ch);
  }
}


void setup() {
  Serial.begin(9600);
  pinMode(BUTTON[0], INPUT_PULLUP);
  pinMode(BUTTON[1], INPUT_PULLUP);
  pinMode(BUTTON[2], INPUT_PULLUP);
  pinMode(LED[0], OUTPUT); digitalWrite(LED[0], HIGH);
  pinMode(LED[1], OUTPUT); digitalWrite(LED[1], HIGH);
  pinMode(LED[2], OUTPUT); digitalWrite(LED[2], HIGH);

  Wire.begin();
#if defined(USE_MAX5696)
  //  DAC_write_command(0x20, 0x00, 0x0f); // POWER all DAC as normal
  //  DAC_write_command(0x30, 0x00, 0x0f); // CONFIG all DAC as transparent
  DAC_write_command(0x38, 0x00, 0x07); // REF as 4.096V, always ON
  //  Serial.println(MAX5816_read_reg(0x38), HEX);
  //  Serial.println(MAX5816_read_reg(0x20), HEX);
#elif defined(USE_AD5696)
  DAC_write_command(0x40, 0x00, 0x00); // Power up all DAC as normal
  DAC_write_command(0x70, 0x00, 0x00); // enable internal reference (default)
#endif
  set_volt(CH[0], INITvl);
  set_volt(CH[1], INITvl);
  set_volt(CH[2], INITvl);
  set_volt(EXCITE, VE);  //直接VEを4.5と設定。

  if(!auto_control_all()) {
    while(true){};
  }
  
}


//計測用
void loop() {
  static int print_flag = 0;     //計測中を表すフラグ
  static int ADmeasure[3];       //計測中にADから読み取ったアンプの出力の値を格納する配列
  char c = Serial.read();        //キーボードからの文字入力を読み取る。基本的に計測終了時に使用
  static int i;                  //エラー処理時にループを回すときのカウンタ
  
  if(print_flag == 0){
    Read_AD(ADmeasure);
    Serial.print(ADmeasure[0]); Serial.print(' '); Serial.print(ADmeasure[1]); Serial.print(' '); Serial.println(ADmeasure[2]);
  }

  //各チャンネルに対応したボタンが押されると、そのチャンネルのDACを再調整する
  if(digitalRead(SW0) == LOW){
    digitalWrite(LED[0], HIGH);
    auto_control(0);
  }else if(digitalRead(SW1) == LOW){
    digitalWrite(LED[1], HIGH);
    auto_control(1);
  }else if(digitalRead(SW2) == LOW){
    digitalWrite(LED[2], HIGH);
    auto_control(2);
  }
  
  if(c == 's'){      //キーボードでsが入力されると
    print_flag = 1;  //計測終了
  }
}
