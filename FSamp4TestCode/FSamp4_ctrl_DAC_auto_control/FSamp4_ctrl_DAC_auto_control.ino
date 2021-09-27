/*
Fsamp4のDACを手動で調整するコード
2021_9/2（木） 記入者：船橋　佑　新Fsamp4基板（赤色）の動作確認に使用
2021_9/24（金）記入者：船橋　佑　1チャンネルだけ自動でDAC調整するコード作成。二分法で行う。未完成である。
2021_9/25（土）記入者：船橋　佑　1チャンネルだけ自動でDAC調整するコード作成が完成。原因はおそらくset_volt後にすぐReadAD2をしたから。間にdelay()を入れると正常に動作した。
　　　　　　　　　　　　　　　　　 追記：3チャンネル同時自動調整コードが完成した。あとはコードの関数化と計測中のエラーをLED点灯で知らせるよう追加する予定。
2021_9/27（月）記入者：船橋　佑　3チャンネル同時自動調整コードの関数化に成功。自動計測。キーボードでsを打つと計測終了。
　　　　　　　　　　　　　　　　　 追記：LEDコード追加。調整中は点灯しており、各chごとに調整が終わると消灯する。
 */


//AD Converter 拡張
// 2021/07/12
// Haruo Noma

//2021_7/13に船橋がコメント追加
//2021_9/7に船橋がコメント追加

#include <Wire.h>

//#define USE_MAX5816
#define USE_AD5696

#if defined(USE_MAX5696)
#define I2C_ADDR 0x0f
#elif defined(USE_AD5696)
#define I2C_ADDR 0x0c // A1=A0=0
#endif

//#define LED0 2
//#define LED1 3
//#define LED2 4
#define SW0 5
#define SW1 6
#define SW2 7

int LED[] = {2,3,4};
int CH[] = {1,2,4};    //調整するDACの場所を変更　　CH 1だとrefA  CH 2だとrefB 　CH 4だとrefC  CH 8だとVE    15だと全部（詳しくはGitのFsamp4の部品資料に記載。）


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

// ADC RingBuffer

const int numReadings = 16;
const int Shift = 4;    //上記numReafingsの分、シフトする量。numReafings=2^shift とする。
int readings[3][numReadings];      // the readings from the analog input
int readings2[3][numReadings];      // the readings from the analog input
int readIndex = 0;                 // the index of the current reading
int readIndex2 = 0;                 // the index of the current reading
int total[3] = {0, 0, 0};          // the running total
int total2[3] = {0, 0, 0};          // the running total
int ledPin = 13;

double VE = 4.5;
double v0 = 2.0;  //DACの大まかな初期値を設定。目安はVEの半分よりも少ないくらい。
int ADData[3];  //読み取ったデータを格納する配列。

void Read_AD(int *a ) {    //DAC調整用の関数。リングバッファを使用
  int i, j;

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


#define STACK_NUM 8

void Read_AD2(int *a ) {  //計測用の関数。過去のデータを参照せずにフィルタリングしてデータを格納している。
  int i, j, k;
  int stack[3]={0,0,0};

  for(k=0;k<STACK_NUM;++k){
    for (i = 0, j = 2; i < 3; i++,j--) {  //アンプの出力OUTAがA2に入り、OUTCがA0に入っているため配列に入れる順番を逆さにして入れている（詳しくは回路図参照）
      stack[i] += analogRead(j) & 0x03ff;
    }
  }

  for (i = 0; i < 3; i++) {
    a[i]=stack[i]/STACK_NUM;
  }
}

void auto_control(int *x, int *y, int *n){    //DAC調整関数。VE=4.5V, V0(DAC初期値)=2.0V, V1(離れた値)=2.5V, V2(目標値)=2.25Vで調整。
  double v1;
  double v2;
  int VEbit = VE/2*1024/5;   //VE=4.5Vの時のADの値をbitで表記;
  int z= 10;                //目標値の誤差範囲をbitで表記
  int i;
  Read_AD2(ADData);        //初期化した値をとってくる
  for(i=0; i<3; i++){      //3チャンネル分の調整を行う。
    v0 = 2.0;
    v1 = v0 + 0.5;
    while(abs(VEbit-x[i]) > z){
      v2 = (v1+v0)/2;    //2点の中点をとってくる
      set_volt(y[i], v2);
      delay(50);         //delay()がないとset_voltがすぐ反映されない。
      Read_AD2(ADData);  //ADの電圧読み取りメソッド呼び出し（調整用）
      if(x[i] >= VEbit){  //とった中点のAD出力が 目標値(460bit)よりも高い場合は
        v0 = v2;               //中点を下限に
      }
      else {                   //そうでなければ
        v1 = v2;               //中点を上限に
      }
    }
    digitalWrite(n[i], LOW);     //調整が終わったら、そのchに対応したLEDを消灯
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SW0, INPUT_PULLUP);
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
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
  set_volt(CH[0], v0);
  set_volt(CH[1], v0);
  set_volt(CH[2], v0);
  set_volt(8, VE);  //直接VEを4.5と設定。

  auto_control(ADData,CH,LED);    //DAC調整関数呼び出し。
  
}

int print_flag = 0;

void loop() {              //計測用
  char c = Serial.read(); 
  if(print_flag==0){
    Read_AD(ADData);
    Serial.print(ADData[0]); Serial.print(' '); Serial.print(ADData[1]); Serial.print(' '); Serial.println(ADData[2]);
  }
  if(c == 's'){      //キーボードでsが入力されると
    print_flag = 1;  //計測終了
  }
}
