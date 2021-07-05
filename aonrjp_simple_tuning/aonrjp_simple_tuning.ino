#include <Wire.h>

#define I2C_MAX5816_ADDR 0x0f
//#define DEBUG 1

int analogPins[3] = {A0, A1, A2};
int ref_val[3] = {0, 0, 0};

void MAX5816_write_command(byte cmd, byte dh, byte dl)
{
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
  while (!Wire.available())
    ;
  dh = Wire.read();
  while (!Wire.available())
    ;
  dl = Wire.read();
  Wire.endTransmission();
  return (dh << 8 | dl);
}

// vは4096(5V) ~ 0(0V)の範囲
void set_dac(int ch, int v)
{
  if(v < 0) return;
  
  MAX5816_write_command(0x18 | ch, v >> 4, v << 4);
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

  //  MAX5816_write_command(0x20, 0x00, 0x0f); // POWER all DAC as normal
  //  MAX5816_write_command(0x30, 0x00, 0x0f); // CONFIG all DAC as transparent
  MAX5816_write_command(0x38, 0x00, 0x07); // REF as 4.096V, always ON

  for (int i = 0; i < 4; ++i)
  {
    set_dac(i, 2047);
    pinMode(i, INPUT);
  }

  Serial.println("CH1, CH2, CH3,");

  for (int i = 0; i < 3; i++) {
    // ゼロ校正
    set_dac(i, searchZeroPoint(i));
    delay(100);
    // 無負荷時アナログ基準値
    ref_val[i] = analog_ave(analogPins[i], 32);
  }  
}

void loop()
{
  for (int i = 0; i < 3; i++) {
    // Serial.print(analogRead(analogPins[i]) - ref_val[i]);
    Serial.print(analogRead(analogPins[i]));
    Serial.print(",");
  }
  Serial.println();
}

int searchZeroPoint(int ch) {
  int start_point = 2230;
  int threshold = 500;

  for (int i = start_point; i < 4095; i++) {
    set_dac(ch, i);
    delay(2);

    // 校正チェック
    if (analogRead(analogPins[ch]) < threshold) {
      return i - 1;
    }
  }

  return -1;
}