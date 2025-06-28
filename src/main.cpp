#include <Arduino.h>

void setup(){
Serial.begin(115200);
Serial2.begin(115200);
Serial.print("hello world");
delay(1000);
}

void loop(){

Serial.print("test");
Serial2.println("PHET");

if(Serial2.available()){
  char a = Serial2.read();
  Serial.println(a);

}

delay(500);




}