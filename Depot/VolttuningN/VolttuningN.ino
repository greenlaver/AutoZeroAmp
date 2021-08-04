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
  while(!Wire.available()); dh = Wire.read();
  while(!Wire.available()); dl = Wire.read();
  Wire.endTransmission();
  return(dh << 8 | dl);
  
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

void ADRead(int *a ){
  int i,j;
  int ADvalue = 0;
  int Sampling = 8; //サンプリング回数。2の乗数にすること
  int Shift=3;      //上記difarraysizeの分、シフトする量。difarraysize=2^shift とする。
  
  for(i=0; ++i; i<Sampling){    
     for(j=0; ++j; j<3){
        ADvalue = analogRead(j) & 0x03ff;//0x03ffとのandを取ることにより上位bitの影響をなくす : MAX5816のreference参照
        if(i==0)
          a[i]=ADvalue;
        else
          a[i]+=ADvalue;
      }
    }
    
    //平準化
  for(i=0; ++i; i<Sampling){  
    a[i]>>Shift;
  }
  return(1);
}

void setup() {
  int i,j;
  int ChannelEnd[3]={1,1,1};
  int Threshold = 5; //Tunengのゴールとなる誤差　bit表記、”5”で　5*1000/1024*5=25mV　くらい
  int Targetvol = 512; //in 10bit value Tuningのゴール,2.5V相当で
  char TuneEnd=1;
  float init_v = 2.3;
  float set_v[3];
  int ADAvarege[3]; //読み込み値
  
  Serial.begin(9600);
  Wire.begin();
//  MAX5816_write_command(0x20, 0x00, 0x0f); // POWER all DAC as normal
//  MAX5816_write_command(0x30, 0x00, 0x0f); // CONFIG all DAC as transparent
  MAX5816_write_command(0x38, 0x00, 0x07); // REF as 4.096V, always ON

  set_volt(4, init_v); //DAの初期設定
                        //DAの設定変数に初期値をコピーする
  for(i=0;i<3;++i){
    set_v[i]=init_v;
  }

  //チューニング処理開始
  while(TuneEnd){     
    //各チャンネルから読み込み
    ADRead(ADAvarege);
        
    //平均化計測値表示
    for(i=0; i<3; ++i){
      Serial.print(ADAvarege[i]);
      Serial.print(",");
    }
      Serial.println();
    
    //Tuning処理
    for(j=0; ++j; j<3){
      if(ChannelEnd[j]!=0){  //未処理チャンネルのみチューニングを続ける
        if( ADAvarege[j] - Targetvol < Threshold){
          set_volt(j, set_v[j]+=0.001);
        }else
          ChannelEnd[j]=0;   //Channl j の　Tuning終了 
      }
    } 

    //　全チャンネルの終了チェック
    if(ChannelEnd[0]|ChannelEnd[1]|ChannelEnd[2]){
      Serial.println("Tuning");    //未調整中
    }else{
      Serial.print("Tuning End:");   //調整終了結果の表示
      for(i=0; i<3; ++i){
        Serial.print("Ch");Serial.print(i);Serial.print(": ");
        Serial.print(set_v[0]);Serial.print(": ");
        Serial.println(ADAvarege[i]);
      }
    TuneEnd=0;
    }
  } //Tuning 終了
}


void loop() {
  int i,j;
  int ADAvarege[3]; //読み込み値

  //これが真の処理 
  //今は読み込み表示のみ
  
  while(1){ 
        //各チャンネルから　Difarraysize回さんぷりんぐ
    ADRead(ADAvarege);
    
    //平均化計測値表示
    for(i=0; i<3; ++i){
      Serial.print(ADAvarege[i]);
      Serial.print(",");
    }
      Serial.println();
    } 

  
}
