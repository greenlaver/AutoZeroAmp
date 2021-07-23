//AD Converter 拡張
// 2021/07/12
// Haruo Noma

// 2021/07/13に船橋がコメント追加
// 2021/07/15 シンプルゼロ校正プログラム追加 written by aonrjp

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

// ADC RingBuffer

const int numReadings = 16;
const int Shift = 4;          //上記numReafingsの分、シフトする量。numReafings=2^shift とする。
int readings[3][numReadings]; // the readings from the analog input
int readIndex = 0;            // the index of the current reading
int total[3] = {0, 0, 0};     // the running total
int ledPin = 13;

void Read_AD(int *a)
{
  int i, j;

  for (i = 0; i < 3; i++)
  {
    total[i] = total[i] - readings[i][readIndex]; // subtract the last reading:
    readings[i][readIndex] = analogRead(i) & 0x03ff;
    total[i] = total[i] + readings[i][readIndex]; // add the reading to the total:
  }

  readIndex = readIndex + 1; // advance to the next position in the array:
  if (readIndex >= numReadings)
  {                // if we're at the end of the array...
    readIndex = 0; // ...wrap around to the beginning:
  }

  for (i = 0; i < 3; i++)
  {
    a[i] = total[i] >> Shift; // calculate the average:
  }

  return (1);
}

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
    set_volt(dac_ch[i], 2);
    pinMode(swPins[i], INPUT);
#ifndef LED_OFF
   pinMode(ledPins[i], OUTPUT);
#endif
  }

  // DACきめうち
  set_dac(dac_ch[0], 32952);
  set_dac(dac_ch[1], 23421);
  set_dac(dac_ch[2], 23411);

  Serial.println("CH1, CH2, CH3,");

  for (int i = 0; i < 3; i++)
  {
    // ゼロ校正
    set_dac(dac_ch[i], searchZeroPoint(i));
    delay(100);
    // 無負荷時アナログ基準値
    ref_val[i] = analog_ave(analogPins[i], 32);
  }
}

void loop()
{
  for (int i = 0; i < 3; i++)
  {
    // Serial.print(analogRead(analogPins[i]) - ref_val[i]);
//     Serial.print(analogRead(analogPins[i]));
    Serial.print(analog_ave(analogPins[i], 8));
    Serial.print(",");
  }
  Serial.println();
}

unsigned int searchZeroPoint(int ch)
{
  unsigned int start_point = 2.0 / 5.0 * 0xffff;
  unsigned int threshold = 500;
  unsigned int sample = 8;

  for (unsigned int i = start_point; i < 0xffff; i++)
  {
    // 測定用にDAC設定を無効化（searchZeroPoint関数はLED光らせるだけになる）
//    set_dac(dac_ch[ch], i);
    delay(2);

#ifdef DEBUG
    Serial.print(ch);
    Serial.print("\t");
    Serial.print(analogRead(analogPins[ch]));
    Serial.print("\t");
    Serial.print(i);
    Serial.print("\t");
    Serial.println(5.0*(double)i/(double)0xffff);
#endif

    // 校正チェック
//    if (analog_ave(analogPins[ch], sample) < threshold)
//    {
//#ifndef LED_OFF
//      digitalWrite(ledPins[ch], HIGH);
//#endif
//      return i;
//    }

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

  return 0;
}
// hogetarou
