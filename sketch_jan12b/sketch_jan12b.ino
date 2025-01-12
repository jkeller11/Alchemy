#include <Wire.h>
#define SDA_PIN 2  // Define custom SDA pin
#define SCL_PIN 3  // Define custom SCL pin


void setup() {
  Wire.begin(); // Start I2C as master
  Serial.begin(9600);  // Start serial communication for debugging
}

void loop() {
  sendData(0x60); //0x60 is weightx100 in big endian
  printWeight();
  //delay(500);
}

void sendData(byte data) {
  Wire.beginTransmission(0x26);  // Address of the Scale
  Wire.write(data);  // Send a byte
  Wire.endTransmission();  // End transmission
}

void printWeight() {
  byte weightData[4];  //define empty array
  Wire.requestFrom(0x26, 16);  // Request 4 bytes from Scale
  
  for(int x = 0; x < 4; x++){
    weightData[x] = Wire.read();  // Read byte
  }

  float weight = (weightData[0] + (weightData[1] *256) + (weightData[2] *65536) + (weightData[3] *16777216))/100;
  Serial.print("Weight: ");
  Serial.print(weight);
  Serial.println("g");
  
}
