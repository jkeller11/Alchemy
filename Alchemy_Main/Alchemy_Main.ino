//Created by James D Keller 7/16/2024
//Intended for Project Alchemy beta test machine for the LSHS
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoRS485.h>
#include <ArduinoModbus.h>
#include <P1AM.h>
#include <arduino-timer.h>

#define relayCard 1
#define comboCard 2
#define pwmCard 3

// MAC Address for P1AM-200 PLC
byte mac[] = { 0x60, 0x52, 0xD0, 0x07, 0xF5, 0x67 };

// IP address for P1AM-200 PLC
IPAddress ip(192, 168, 1, 178);

//Global Variables
boolean MB_C[16];     //Modbus Coil Bits          R/W
boolean MB_I[16];     //Modbus Input Bits         R       
int MB_HR[16];        //Modbus Holding Registers  R
int MB_IR[16];        //Modbus Input Registers    R/W
int mixTime = 180000;  //3 minute mix time

EthernetServer server(502);
EthernetClient clients[8];
ModbusTCPServer modbusTCPServer;
// auto timer = timer_create_default(); // create a timer with default settings for timing mixes


void setup() {
  while (!P1.init()){} ; //Wait for P1 Modules to Sign on   
  Ethernet.begin(mac, ip);
  server.begin(); 

  //Serial1.begin(9600);

  if (!modbusTCPServer.begin()) {while (1);} //Start the Modbus TCP server

  modbusTCPServer.configureCoils(0x00, 32);             //Coils
  modbusTCPServer.configureDiscreteInputs(0x00, 32);    //Discrete Inputs
  modbusTCPServer.configureHoldingRegisters(0x00, 16);  //Holding Register Words
  modbusTCPServer.configureInputRegisters(0x00, 16);    //Input Register Words
  modbusTCPServer.coilWrite(13, 1);                     //Setting E-Stop 
  
  // timer.every(1000, timeTickModBus);                    //Call timeTickModBus every 1 second

}

void loop() {
  
  ModBusTCPService();        // Handles TCP Modbus connection to HMI
  updateArrays();            // Updates Coils, Input Bits, Holding & Input Registers on PLC
  updatemodbusTCPServer();
  
  if(!MB_C[13]){ //check if E-Stop has been pressed
    reset();
  }

  // //Check for Clean Mode
  // if(MB_C[10]){
  //   CleanMode();
  // }
  
  // //Check for Engineering Mode
  // if(MB_C[11]){

  // }

  // //Check for Production Mode
  // if(MB_C[12]){

  // }
  updatemodbusTCPServer(); //Updates modbus server values
  updateEquipmentStates();  //Write to PLC cards
}

///////////////////////////////////////Clean Mode///////////////////////////////////////////////////////////////////////////////////////////////
void CleanMode(){
  while(1){
    ModBusTCPService();
    
    if(!MB_C[10]){ //check if CleanMode ended
      break;
    }
	
	

    updatemodbusTCPServer(); //Updates Coils, Input Bits, Holding & Input Registers

    //start timer and mix
    //run pump for 3 minutes flush each valvue in order for 30 sec
    //allow manual operation of flushing
    //pop up recommend cleaing message
    

  }
  reset(1); //reset without E-Stop Check
}

///////////////////////////////////////Engineering Mode///////////////////////////////////////////////////////////////////////////////////////////
void EngineeringMode(){//For troubleshooting purposes
  reset();

  while(1){
    ModBusTCPService();
    if(!MB_C[11]){
      break;
    }

    updatemodbusTCPServer(); //Updates Coils, Input Bits, Holding & Input Registers




  }
  reset();
}
///////////////////////////////////////Production Mode////////////////////////////////////////////////////////////////////////////////////////////
void ProductionMode(){
  while(1){
    ModBusTCPService();
    if(!MB_C[12]){
      break;
    }

    updatemodbusTCPServer(); //Updates Coils, Input Bits, Holding & Input Registers

    //allow entry of # of bottles
    //calc flow rate via recipe on HMI
    //Prompt to confirm medicine added
    //Start time and mix for X minuites... allow for canceling ealry
    //once canceled or mixing fnished prompt to begin
    //start cycle of dispensing group one (time via flowrate estimate)  jog to fill remainder
    //confirm next group 2 start
    //repeat until bottle # reached or canceled ealry
    //Once tranitioning to complete message to go to manual mode and finish dispensing
    

  }
  reset();
}

///////////////////////////////////////Equipment Functions////////////////////////////////////////////////////////////////////////////////////////
void reset(){//set all Modbus data back to default
  for(int x = 0; x <16; x++){

    if(x!=10 or x!=11 or x!=12){ //avoid overwriting phases
      modbusTCPServer.coilWrite(x,0);
    }
    
    modbusTCPServer.coilWrite(x,0);
    modbusTCPServer.holdingRegisterWrite(x,0);
    modbusTCPServer.inputRegisterWrite(x, 0);
    modbusTCPServer.discreteInputWrite(x, 0);
  }

  while(1){
    ModBusTCPService(); 
    updateArrays();
     updateEquipmentStates();

    if(MB_C[13] == 1){ //check if E-Stop has been reset
      break; 
    }
  }
}

void reset(int x){//set all Modbus data back to default takes dummy int to skip E-Stop check
	for(int x = 0; x <16; x++){
		if(x!=10 or x!=11 or x!=12){ //avoid overwriting phases
		  modbusTCPServer.coilWrite(x,0);
		}
	
		modbusTCPServer.coilWrite(x,0);
		modbusTCPServer.holdingRegisterWrite(x,0);
		modbusTCPServer.inputRegisterWrite(x, 0);
		modbusTCPServer.discreteInputWrite(x, 0);
	}
	ModBusTCPService(); 
  updateArrays();
  updateEquipmentStates();
}


void setPumpSpeed(int speed) {
  int dutyCycle = map(speed, 0, 100, 255, 0);

  P1.writePWM(dutyCycle, 20000, 3, 1);            //Write Duty Cycle to pumpo PWM input
  modbusTCPServer.holdingRegisterWrite(0, speed);
}

void updateEquipmentStates(){ //Writes new states to P1AM cards
  //Update Relay Card - Valves & Pump On/Off
  for (int i = 0; i < 8; i++) {
    P1.writeDiscrete(MB_C[i], relayCard, i + 1);  //Data,Slot,Channel ... Channel is one-based.
  }

  P1.writeDiscrete(MB_C[8], comboCard, 1); //Mixer On/Off
  P1.writeDiscrete(MB_C[9], comboCard, 2); //Pump Direction
  

  setPumpSpeed(MB_HR[0]);
}

// bool timeTickModBus(void *){ //Writes to "timer" tag every one second
// 	MB_HR[11]++; //increment to add second
// 	modbusTCPServer.holdingRegisterWrite(11,MB_HR[11]);
// 	return true;
// }

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

void updateArrays(){//Updates Coils, Input Bits, Holding & Input Registers. Reads from modbusTCPServer(HMI)
  for (int i = 0; i < 16; i++) {
    MB_C[i] = modbusTCPServer.coilRead(i);
    MB_HR[i] = modbusTCPServer.holdingRegisterRead(i);
    MB_IR[i] = modbusTCPServer.inputRegisterRead(i);
    MB_I[i] = modbusTCPServer.discreteInputRead(i);
  }

  MB_C[13] = P1.readDiscrete(comboCard,1);   //E-Estop

}

void updatemodbusTCPServer(){ //Write updated coils and holding registersw to modBustTCPServer
  for(int x = 0; x<16; x++){
    modbusTCPServer.holdingRegisterWrite(x,MB_HR[x]);
    modbusTCPServer.coilWrite(x,MB_C[x]);
    modbusTCPServer.discreteInputWrite(x, MB_I[x]);
    modbusTCPServer.inputRegisterWrite(x, MB_IR[x]);
  }
}
