//AD Converter 拡張
// 2021/07/12
// Haruo Noma

// 2021/07/13 船橋がコメント追加
// 2021/07/15 シンプルゼロ校正プログラム追加 written by aonrjp
// 2021/07/23 船橋くんへの引き渡し用にコード整備

#include <Wire.h>

//#define USE_MAX5816
#define USE_AD5696

// #define LED_OFF // LED点滅無効化
#define DEBUG   // Enable DEBUG print

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

// #define CH 2 //CH 1だとrefA CH 2 だと　refB CH 4　だと　refC 15だと全部
int analogPins[3] = {A2, A1, A0};
int ledPins[3] = {LED0, LED1, LED2};
int swPins[3] = {SW0, SW1, SW2};
int ref_val[3] = {0, 0, 0};
int dac_ch[3] = {1, 2, 4};

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
  while (!Wire.available())
    ;
  dh = Wire.read();
  while (!Wire.available())
    ;
  dl = Wire.read();
  Wire.endTransmission();
  return (dh << 8 | dl);
}

void set_dac(int ch, unsigned int v)
{
#ifdef defined(USE_MAX5696)
  DAC_write_command(0x18 | ch, v >> 4, v << 4);
#elif defined(USE_AD5696)
  DAC_write_command(0x30 | (ch), v >> 8, v & 0xff); //+1消しました
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

/// analog_ave
/// 
/// pinで指定したAnalog入力ピンの値をsample分とって平均値を返す
int analog_ave(int pin, int sample) {
  long sum = 0;
  for(int i=0; i<sample; i++) {
    sum += analogRead(pin);
  }

  return sum / sample;
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();

  delay(100);

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

  for (int i = 0; i < 4; ++i)
  {
    // DAC初期値設定
    set_volt(dac_ch[i], 2.0);
    // スイッチ入力用ピンモード設定
    pinMode(swPins[i], INPUT);
#ifndef LED_OFF
    // LED出力ピン設定
    pinMode(ledPins[i], OUTPUT);
#endif
  }

  // シリアルプロッタの凡例表示
  Serial.println("CH1, CH2, CH3,");

  // 3ch分のゼロ校正を実施
  for (int i = 0; i < 3; i++)
  {
    // ゼロ校正
    set_dac(dac_ch[i], searchZeroPoint(i));
    // 無負荷時のアンプ出力値を読み出し
    ref_val[i] = analog_ave(analogPins[i], 32);
  }
}

void loop()
{
  // シリアルプロッタ向けに3ch表示
  for (int i = 0; i < 3; i++)
  {
    // Serial.print(analogRead(analogPins[i])); // 生値表示
    Serial.print(analog_ave(analogPins[i], 8)); // サンプル平均値表示
    Serial.print(",");
  }
  Serial.println();
}

/// searchZeroPoint
/// 
/// DACの出力電圧をstart_pointから順に設定し，アンプの出力電圧が
/// 閾値電圧より低くなった時点の電圧を校正電圧値として返す．
unsigned int searchZeroPoint(int ch)
{
  // DACの電圧設定開始ポイント : [volt] / [ref] * [resolution](16bit)
  unsigned int start_point = 2.0 / 5.0 * 0xffff;
  // アンプ出力電圧の閾値（10bitアナログ入力）
  unsigned int threshold = 2.4 / 5.0 * 0x3ff;
  // 
  unsigned int sample = 8;

  
  for (unsigned int i = start_point; i < 0xffff; i++)
  {
    // DACの出力電圧設定
    set_dac(dac_ch[ch], i);

    // DACの設定値が反映されるまで待機（必要ないかも）
    delay(1);

// デバッグ用シリアル表示
//   例: [ch], [アナログ入力], []
#ifdef DEBUG
    Serial.print(ch);
    Serial.print("\t");
    Serial.print(analogRead(analogPins[ch]));
    Serial.print("\t");
    Serial.print(i);
    Serial.print("\t");
    Serial.println(5.0*(double)i/(double)0xffff);
#endif

    // アンプ出力電圧が閾値以下であればDACの設定値を返す
   if (analog_ave(analogPins[ch], sample) < threshold)
   {
#ifndef LED_OFF
     digitalWrite(ledPins[ch], HIGH);
#endif
     return i;
   }

#ifndef LED_OFF
    // 校正中はLED点滅させる
    if(i % 100 == 0) {
     digitalWrite(ledPins[ch], !digitalRead(ledPins[ch]));
    }
#endif
  }

#ifndef LED_OFF
  digitalWrite(ledPins[ch], LOW);
#endif

  // どの値をDACに設定してもアンプの出力が閾値以下にならなかった場合は0を返す
  return 0;
}