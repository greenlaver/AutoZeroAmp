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
    a[i] = total[i] /16;                  // calculate the average:
  }
  return (1);
}

float set_v[3];

int times;

void setup() {
  int i, j, k, l;
  int ChannelEnd[3] = {1, 1, 1};
  int Threshold = 50; //Tunengのゴールとなる誤差　bit表記、”5”で　5*1000/1024*5=25mV　くらい
  int Targetvol = 400; //in 10bit value Tuningのゴール,1.5V相当で
  int TuneEnd = 1;
  float init_v = 2.3;
  int ADAvarege[3]; //読み込み値

  Serial.begin(9600);
  Wire.begin();
  //  MAX5816_write_command(0x20, 0x00, 0x0f); // POWER all DAC as normal
  //  MAX5816_write_command(0x30, 0x00, 0x0f); // CONFIG all DAC as transparent
  MAX5816_write_command(0x38, 0x00, 0x07); // REF as 4.096V, always ON

  set_volt(4, init_v); //DAの初期設定
  //DAの設定変数に初期値をコピーする
  for (i = 0; i < 3; ++i) {
    set_v[i] = init_v;
  }

  Serial.println("start");

  //チューニング処理開始
  while (TuneEnd) {
    times++;
    //各チャンネルから読み込み
    ADRead(ADAvarege);

    //readings[i][readIndex]に変な値が残るのを防ぐための処理
    if (times < 50){
        Serial.println(times);
       continue;
    }
    
    //平均化計測値表示
//    for (i = 0; i < 3; ++i) {
//      Serial.print( abs(ADAvarege[j] - Targetvol) );
//      Serial.print(",");
//    }

    //Tuning処理
    for (j = 0; j < 3; ++j) {
      //      Serial.println("abc");
      if (ChannelEnd[j] != 0) {
        //未処理チャンネルのみチューニングを続ける
        Serial.print( ADAvarege[j] - Targetvol );
        Serial.print(",");
        
        if ( abs(ADAvarege[j] - Targetvol) > Threshold ){
          set_v[j] = set_v[j] += 0.0005;
          set_volt(j, set_v[j]);
        }else {
          ChannelEnd[j] = 0; //Channl j の　Tuning終了
          Serial.print("channel : ");
          Serial.print(j);
          Serial.println("end");
        }

      }
    }

    //　全チャンネルの終了チェック
    if (ChannelEnd[0] | ChannelEnd[1] | ChannelEnd[2]) {
      Serial.println("Tuning");    //未調整中
    } else {
      Serial.print("Tuning End:");   //調整終了結果の表示
      for (k = 0; k < 3; ++k) {
        Serial.print("Ch"); Serial.print(k); Serial.print(": ");
        Serial.print(set_v[k]); Serial.print(": ");
        Serial.println(ADAvarege[k]);
      }
      TuneEnd = 0;
      break;
    }

    delay(100);
  } //Tuning 終了
}


void loop() {
  int i, j;
  int ADAvarege[3]; //読み込み値

  //これが真の処理
  //今は読み込み表示のみ

  while (1) {
    //各チャンネルから　Difarraysize回さんぷりんぐ
    ADRead(ADAvarege);

    //平均化計測値表示
    for (i = 0; i < 3; ++i) {
      set_volt(i, set_v[i]);
      Serial.print(ADAvarege[i]);
      Serial.print(",");
    }

    
    delay(500);
    Serial.println();
  }


}
