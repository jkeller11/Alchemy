//Created by James D Keller 7/16/2024
//Intended for Project Alchemy beta test machine for the LSHS
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoRS485.h>
#include <ArduinoModbus.h>
#include <P1AM.h>
#include "Functions.h"

#define relayCard = 1
#define analogCard = 2
#define pwmCard = 3

// MAC Address for P1AM-200 PLC
byte mac[] = { 0x60, 0x52, 0xD0, 0x07, 0xF5, 0x67 };

// IP address for P1AM-200 PLC
IPAddress ip(192, 168, 1, 178);

//Global Variables
boolean MB_C[16];  //Modbus Coil Bits          R/W
boolean MB_I[32];  //Modbus Input Bits         R       
int MB_HR[16];     //Modbus Holding Registers  R
int MB_IR[16];     //Modbus Input Registers    R/W
int PumpDirectionPin = 3;

EthernetServer server(502);
EthernetClient clients[8];
ModbusTCPServer modbusTCPServer;

void setup() {
  while (!P1.init()){} ; //Wait for P1 Modules to Sign on   
  Ethernet.begin(mac, ip);
  server.begin(); 

  if (!modbusTCPServer.begin()) {while (1);} //Start the Modbus TCP server

  modbusTCPServer.configureCoils(0x00, 32);             //Coils
  modbusTCPServer.configureDiscreteInputs(0x00, 32);    //Discrete Inputs
  modbusTCPServer.configureHoldingRegisters(0x00, 16);  //Holding Register Words
  modbusTCPServer.configureInputRegisters(0x00, 16);    //Input Register Words
}

void loop() {

  ModBusTCPService();        // Handles TCP Modbus connection to HMI
  updateCoils();             // Updates Coil Array with values from Modbus server
  updateInputs();            // Updates Input Array with values from Modbus server
  updateHoldingRegisters();  // Updates HR Array with values from Modbus server
  updateInputRegisters();    // Updates IR Array with values from Modbus server
  
  //Check for Service Mode
  if(MB_C[10]){

  }
  
  //Check for Engineering Mode
  if(MB_C[11]){

  }

  //Check for Production Mode
  if(MB_C[12]){

  }

  // //Write Discrete outputs to relay card for Solenoids and Pump
  // for (int i = 0; i <= 8; i++) {
  //   P1.writeDiscrete(MB_C[i], 1, i + 1);  //Data,Slot,Channel ... Channel is one-based.
  // }
  // P1.writeDiscrete(MB_C[9], 2, 1); //Write Mixer State
  
  
  // P1.writeDiscrete(MB_C[10], 2, 2); //Write Pump Direction
  
  // //Mixer and Pump Code
  // setPumpSpeed(MB_HR[0]); //Pump Speed

}
///////////////////////////////////////Service Mode///////////////////////////////////////////////////////////////////////////////////////////////
void ServiceMode(){
  while(1){

  }
  reset();
}
///////////////////////////////////////Engineering Mode///////////////////////////////////////////////////////////////////////////////////////////
void EngineeringMode(){//For troubleshooting purposes
  while(1){
    updateCoils();             // Updates Coil Array with values from Modbus server
    updateInputs();            // Updates Input Array with values from Modbus server
    updateHoldingRegisters();  // Updates HR Array with values from Modbus server
    updateInputRegisters();    // Updates IR Array with values from Modbus server

  }
  reset();
}
///////////////////////////////////////Production Mode////////////////////////////////////////////////////////////////////////////////////////////
void ProductionMode(){
  while(1){

  }
  reset();
}

///////////////////////////////////////Equipment Functions////////////////////////////////////////////////////////////////////////////////////////
void reset(){//set all Modbus data back to default
  for(int x = 0; x <16; x++){
    modbusTCPServer.coilWrite(x,0);
    modbusTCPServer.holdingRegisterWrite(x,0);
  }
  
  while(1){
    updateInputs();
    if(MB_I[0] == 1){ //check if E-Stop has been reset
      break; 
    }
  }
}

void setPumpSpeed(int speed) {
  int dutyCycle = map(speed, 0, 100, 255, 0);

  P1.writePWM(dutyCycle, 20000, 3, 1);
  modbusTCPServer.holdingRegisterWrite(0, speed);
}


void setPumpDirection(int direction, int speed) {
  if (speed != 0) {

  }
  else{

  }

}

////////////////////////////////////////Utility Functions/////////////////////////////////////////////////////////////////////////////////////////
void ModBusTCPService(){
  EthernetClient newClient = server.accept(); //listen for incoming clients
  
  if (newClient) { //process new connection if possible
    for (byte i = 0; i < 8; i++) { //Eight connections possible, find first available.
      if (!clients[i]) {
        clients[i] = newClient;
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
    }
  }
}


void updateCoils() {  //Read the Coil bits from the Modbus Library
  for (int i = 0; i < 16; i++) {
    MB_C[i] = modbusTCPServer.coilRead(i);
  }
}

void updateInputs() {  //Write the Input bits to the Modbus Library
  for (int i = 0; i < 32; i++) {
    modbusTCPServer.discreteInputWrite(i, MB_I[i]);
  }
}

void updateHoldingRegisters() {  //Read the Holding Register words from the Modbus Library
  for (int i = 0; i < 16; i++) {
    MB_HR[i] = modbusTCPServer.holdingRegisterRead(i);
  }
}

void updateInputRegisters() {  //Write the Input Registers to the Modbus Library
  for (int i = 0; i < 16; i++) {
    modbusTCPServer.inputRegisterWrite(i, MB_IR[i]);
  }
}