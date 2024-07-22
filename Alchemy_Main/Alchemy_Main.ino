//Created by James D Keller 7/16/2024
//Intended for Project Alchemy beta test machine for the LSHS office
//**TO DO** Project Description 

#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoRS485.h> 
#include <ArduinoModbus.h>
#include <P1AM.h>
#include <Functions.h>

// MAC Address for P1AM-200 PLC
byte mac[] = { 0x60, 0x52, 0xD0, 0x07, 0xF5, 0x67};

// IP address for P1AM-200 PLC
IPAddress ip(192, 168, 1, 178);

boolean MB_C[16]; //Modbus Coil Bits
boolean MB_I[32]; //Modbus Input Bits
int MB_HR[16];    //Modbus Holding Registers
int MB_IR[16];    //Modbus Input Registers
 
EthernetServer server(502);
EthernetClient clients[8];
ModbusTCPServer modbusTCPServer;
int client_cnt;
 
void setup() {
  Serial.begin(115200);
  Serial.println("Modbus TCP Server and Module I/O Example");
  while (!P1.init()){} ; //Wait for P1 Modules to Sign on   
  Ethernet.begin(mac, ip);
  server.begin(); // start the server to begin listening
 
  if (!modbusTCPServer.begin()) { // start the Modbus TCP server
    Serial.println("Failed to start Modbus TCP Server!");
    while (1); //If it can't be started no need to contine, stay here forever.
  }
 
  modbusTCPServer.configureCoils(0x00, 16);             //Coils
  // modbusTCPServer.configureDiscreteInputs(0x00, 32);    //Discrete Inputs
  // modbusTCPServer.configureHoldingRegisters(0x00, 16); //Holding Register Words
  // modbusTCPServer.configureInputRegisters(0x00, 16);   //Input Register Words
 
  Serial.println("Done with setup()");
}
 
void loop() {
  EthernetClient newClient = server.accept(); //listen for incoming clients
  if (newClient) { //process new connection if possible
  for (byte i = 0; i < 8; i++) { //Eight connections possible, find first available.
    if (!clients[i]) {
      clients[i] = newClient;
      client_cnt++;
      Serial.print("Client Accept:"); //a new client connected
      Serial.print(newClient.remoteIP());
      Serial.print(" , Total:");
      Serial.println(client_cnt);
      break;
      }
    }
  }

  //If there are packets available, receive them and process them.
  for (byte i = 0; i < 8; i++) {
    if (clients[i].available()) { //if data is available
      modbusTCPServer.accept(clients[i]); //accept that data
      modbusTCPServer.poll();// service any Modbus TCP requests, while client connected
      }
  }
  for (byte i = 0; i < 8; i++) { // Stop any clients which are disconnected
    if (clients[i] && !clients[i].connected()) {
      clients[i].stop();
      client_cnt--;
      Serial.print("Client Stopped, Total: ");
      Serial.println(client_cnt);
    }
  }

  updateCoils(); //Read current state of the Modbus Coils into MB_C[]
  
  for (int i=0;i<8;i++){
    P1.writeDiscrete(MB_C[i],1,i+1);//Data,Slot,Channel ... Channel is one-based.
  }
}
 
void updateCoils() {//Read the Coil bits from the Modbus Library
  for (int i=0;i<16;i++){
    MB_C[i] = modbusTCPServer.coilRead(i);
  }
}
 
void updateInputs() { //Write the Input bits to the Modbus Library
  for (int i=0;i<32;i++){
    modbusTCPServer.discreteInputWrite(i,MB_I[i]);
  }
}
 
void updateHoldingRegisters() {//Read the Holding Register words from the Modbus Library
  for (int i=0;i<16;i++){
    MB_HR[i] = modbusTCPServer.holdingRegisterRead(i);
  }
}
 
void updateInputRegisters() { //Write the Input Registers to the Modbus Library
  for (int i=0;i<16;i++){
    modbusTCPServer.inputRegisterWrite(i,MB_IR[i]);
  }
}