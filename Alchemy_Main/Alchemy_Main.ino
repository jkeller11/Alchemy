//Created by James D Keller 7/16/2024
//Intended for Project Alchemy beta test machine for the LSHS office
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoRS485.h>
#include <ArduinoModbus.h>
#include <P1AM.h>
#include "Functions.h"

// MAC Address for P1AM-200 PLC
byte mac[] = { 0x60, 0x52, 0xD0, 0x07, 0xF5, 0x67 };

// IP address for P1AM-200 PLC
IPAddress ip(192, 168, 1, 178);

//Global Variables
boolean MB_C[16];  //Modbus Coil Bits
boolean MB_I[32];  //Modbus Input Bits
int MB_HR[16];     //Modbus Holding Registers
int MB_IR[16];     //Modbus Input Registers
int PWMpin = 2;
int PumpDirectionPin = 3;
int pumpDirection = 0;

//Global Objects
EthernetServer server(502);
EthernetClient clients[8];
ModbusTCPServer modbusTCPServer;
int client_cnt;

void setup() {
  Serial.begin(115200);
  Serial.println("Modbus TCP Server and Module I/O Example");
  while (!P1.init()) {};  //Wait for P1 Modules to Sign on
  Ethernet.begin(mac, ip);
  server.begin();  // start the server to begin listening

  if (!modbusTCPServer.begin()) {  // start the Modbus TCP server
    Serial.println("Failed to start Modbus TCP Server!");
    while (1)
      ;  //If it can't be started no need to contine, stay here forever.
  }

  modbusTCPServer.configureCoils(0x00, 32);             //Coils
  modbusTCPServer.configureDiscreteInputs(0x00, 32);    //Discrete Inputs
  modbusTCPServer.configureHoldingRegisters(0x00, 16);  //Holding Register Words
  modbusTCPServer.configureInputRegisters(0x00, 16);    //Input Register Words

  Serial.println("Done with setup()");
  pinMode(PumpDirectionPin, OUTPUT);
}

void loop() {
  EthernetClient newClient = server.accept();  //listen for incoming clients
  if (newClient) {                             //process new connection if possible
    for (byte i = 0; i < 8; i++) {             //Eight connections possible, find first available.
      if (!clients[i]) {
        clients[i] = newClient;
        client_cnt++;
        Serial.print("Client Accept:");  //a new client connected
        Serial.print(newClient.remoteIP());
        Serial.print(" , Total:");
        Serial.println(client_cnt);
        break;
      }
    }
  }

  //If there are packets available, receive them and process them.
  for (byte i = 0; i < 8; i++) {
    if (clients[i].available()) {          //if data is available
      modbusTCPServer.accept(clients[i]);  //accept that data
      modbusTCPServer.poll();              // service any Modbus TCP requests, while client connected
    }
  }
  // Stop any clients which are disconnected
  for (byte i = 0; i < 8; i++) {
    if (clients[i] && !clients[i].connected()) {
      clients[i].stop();
      client_cnt--;
      Serial.print("Client Stopped, Total: ");
      Serial.println(client_cnt);
    }
  }

  //Read current state of the Modbus arrays
  updateCoils();
  updateInputs();
  updateHoldingRegisters();
  updateInputRegisters();

  //Saftey For Pump
  if ((MB_C[7] == false) && (MB_HR[0] != 0)) { safteyPumpSpeed(); }

  //Write Discrete outputs to P1-08TRS & P1-15CDD1
  for (int i = 0; i < 8; i++) {
    P1.writeDiscrete(MB_C[i], 1, i + 1);  //Data,Slot,Channel ... Channel is one-based.
  }
  for (int i = 8; i < 15; i++) {
    P1.writeDiscrete(MB_C[i], 2, i - 7);  //Data,Slot,Channel ... Channel is one-based.
  }

  //Pump Speed
  setPumpSpeed(MB_HR[0]);

  //Pump Direction
  if (pumpDirection != MB_C[9]) {
    pumpDirection = MB_C[9];
    setPumpDirection(MB_C[9], MB_HR[0]);
  }
}