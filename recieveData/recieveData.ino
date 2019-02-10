#define LED 8
#include<SPIMemory.h>

SPIFlash flash;
//SPIFlash flash;
volatile uint64_t add;
int i;
volatile uint8_t value;

void setup()
{
  add = 0;
  Serial.begin(57600);
  pinMode(LED,OUTPUT);
  digitalWrite(LED,HIGH);
  while(!Serial);
  delay(50);
  digitalWrite(LED,LOW);
  flash.begin();
  flash.eraseChip();
  //int number = 0;
}

void loop()
{
  while(Serial.available()==0);
  String value = Serial.readStringUntil('\n');
  char t = Serial.read();
  //Serial.setTimeout(0.01);
  int val = value.toInt();
  flash.writeByte(add, val);
  add++;
}

