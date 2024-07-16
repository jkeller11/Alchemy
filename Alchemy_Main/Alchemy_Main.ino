//Created by James D Keller 7/16/2024
//Intended for Project Alchemy beta test machine for the LSHS office
//**TO DO** Project Description 

#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoRS485.h> 
#include <ArduinoModbus.h>
#include <P1AM.h>

// MAC Address for P1AM-200 PLC
byte mac[] = { 0x60, 0x52, 0xD0, 0x07, 0xF5, 0x67};

// IP address for P1AM-200 PLC
IPAddress ip(192, 168, 1, 178);

EthernetServer server(502); //TCP Port Number
EthernetClient clients[8];
ModbusTCPServer modbusTCPServer;
int client_cnt;

void setup(){
  Serial.begin(115200);
  while (!P1.init()){} ; //Wait for P1 Modules 
  Ethernet.begin(mac, ip);
  server.begin(); // start Modbus TCP server
}

void loop(){
  
}