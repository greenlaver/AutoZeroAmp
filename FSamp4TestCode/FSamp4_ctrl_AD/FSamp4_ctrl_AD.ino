//AD Converter 拡張
// 2021/07/12
// Haruo Noma 

#include <Wire.h>

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

#define CH 0

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
  while(!Wire.available()); dh = Wire.read();
  while(!Wire.available()); dl = Wire.read();
  Wire.endTransmission();
  return(dh << 8 | dl);
  
}

void set_dac(int ch, unsigned int v)
{
#ifdef defined(USE_MAX5696)
 DAC_write_command(0x18 | ch, v >> 4, v << 4);
#elif defined(USE_AD5696)
 DAC_write_command(0x30 | (ch+1), v >> 8, v & 0xff);
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



double v = 2.5;

void setup() {
  Serial.begin(9600);
  pinMode(SW0, INPUT_PULLUP);
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(LED0, OUTPUT); digitalWrite(LED0, 0);
  pinMode(LED1, OUTPUT); digitalWrite(LED1, 1);
  pinMode(LED2, OUTPUT); digitalWrite(LED2, 2);

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

  set_volt(CH, v);
}

int i = 0;

void loop() {

int ADData[3];
int ina;
float inaf;
  
  if (digitalRead(SW0) == 0) digitalWrite(LED0, 1); else digitalWrite(LED0, 0);
  if (digitalRead(SW1) == 0) digitalWrite(LED1, 1); else digitalWrite(LED1, 0);
  if (digitalRead(SW2) == 0) digitalWrite(LED2, 1); else digitalWrite(LED2, 0);

  Read_AD(ADData);
  
  while(Serial.available()){
    char c = Serial.read();
    if (c == '1') v -= 0.05;
    else if (c == '2') v -= 0.005;
    else if (c == '3') v -= 0.001;
    else if (c == '4') v -= 0.0005;
    else if (c == '5') v += 0.0005;
    else if (c == '6') v += 0.001;
    else if (c == '7') v += 0.005;
    else if (c == '8') v += 0.05;
    Serial.print(v,5); Serial.print(' '); 

    ina=analogRead(3);
    inaf=(float)ina/200;
    Serial.print(ina); Serial.print(' ');Serial.print(inaf,5); Serial.print(' ');
    Serial.print(ADData[0]); Serial.print(' '); Serial.print(ADData[1]); Serial.print(' ');Serial.println(ADData[2]);

    set_volt(CH, v);
  }
}
