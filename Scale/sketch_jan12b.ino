#include <Wire.h>
//#include <SparkFun_I2C_Mux_Arduino_Library.h>
#define SDA_PIN 2  // Define custom SDA pin
#define SCL_PIN 3  // Define custom SCL pin


void setup() {
  Wire.begin(); // Start I2C as master
  Serial.begin(9600);  // Start serial communication for debugging
}

void loop() {
  //Get transmission to start weighing
  
  //calibrate with bottles on then start getting weight until end tranmission is received
  getWeight();
  //tear();
  //delay(1000);

  
}

void getWeight() {
  Wire.beginTransmission(0x26);
  Wire.write(0x60);

  byte weightData[4];  //define empty array
  Wire.requestFrom(0x26, 16);  // Request 4 bytes from Scale

  for(int x = 0; x < 4; x++){
    weightData[x] = Wire.read();  // Read byte
  }

  Wire.endTransmission();  // End transmission

  int weight = (weightData[0] + (weightData[1] << 8) + (weightData[2] << 16) + (weightData[3] << 24))/100;
  Serial.print("Weight: ");
  Serial.print(weight);
  Serial.println("g");
}

void tear(){
  Wire.beginTransmission(0x26);  // Address of the Scale
  Wire.write(0x50); //offset reg
  Wire.write(1); //reset offset
  Wire.endTransmission();  // End transmission
}