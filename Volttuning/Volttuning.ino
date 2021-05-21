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

float v = 2.3;
float differnce;

void setup() {
  Serial.begin(9600);
  Wire.begin();
//  MAX5816_write_command(0x20, 0x00, 0x0f); // POWER all DAC as normal
//  MAX5816_write_command(0x30, 0x00, 0x0f); // CONFIG all DAC as transparent
  MAX5816_write_command(0x38, 0x00, 0x07); // REF as 4.096V, always ON

  set_volt(CH, v);
}

uint8_t ADchannel = 2;
uint8_t index = 0;
int difarraysize = 10;
int difavarege;
int threshold = 50;
int ADvalue;
int targetvol = 512; //in 10bit value 

int differences[100];

void loop() {
  while(true){
   
      ADvalue = analogRead( ADchannel ) & 0x03ff;//0x03ffとのandを取ることにより上位bitの影響をなくす : MAX5816のreference参照
      differnce = ADvalue - targetvol;
      differences[index] = differnce;

//      Serial.println("index");
//      Serial.println(index);
//
      Serial.println("AD");
      Serial.println( ADvalue );

      Serial.println("dif");
      Serial.println( differnce );
      
      if (index != difarraysize){
        index++;
      
    } else if (index == difarraysize) {
       int sum = 0;
       
       for (int j = 0;j <= difarraysize;j++){
          sum += differences[j];
       }

       Serial.println("sum");
       Serial.println( sum );
       
       difavarege = sum ;
       Serial.println("difavarege");
       Serial.println(difavarege);
       Serial.println("v");
       Serial.println(v);
       
       if (difavarege > threshold * difarraysize){
        v += 0.001;
      } else if (difavarege < -threshold * difarraysize){
        v -= 0.001;
      } else if (-threshold * difarraysize <= difavarege && difavarege <= threshold * difarraysize){
        v += 0;

        if (ADchannel != 3){
           ADchannel += 1;
           v = 2.3;
           
        } else {
           ADchannel = 0;
           Serial.println("end");
           delay(1000000);
        }
       
      }
      set_volt(ADchannel - 1, v);
      
      index = 0;
    }
    

    delay(100);
  }
   Serial.println("end");
   delay(1000);
}
