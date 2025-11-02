//Created by James D Keller 7/16/2024
//Intended for Project Alchemy beta test machine for the LSHS
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoRS485.h>
#include <ArduinoModbus.h>
#include <P1AM.h>
#include <FIFObuf.h>

#define relayCard 1
#define comboCard 2
#define pwmCard 3

// MAC Address for P1AM-200 PLC
byte mac[] = { 0x60, 0x52, 0xD0, 0x07, 0xF5, 0x67 };

// IP address for P1AM-200 PLC
IPAddress ip(192, 168, 1, 178);

//Global Variables
boolean MB_C[32];     //Modbus Coil Bits          R/W   
int MB_HR[32];        //Modbus Holding Registers  R

EthernetServer server(502);
EthernetClient clients[8];
ModbusTCPServer modbusTCPServer;

void setup() {
  while (!P1.init()){} ; //Wait for P1 Modules to Sign on   
  Ethernet.begin(mac, ip);
  server.begin(); 

  if (!modbusTCPServer.begin()) {while (1);} //Start the Modbus TCP server

  modbusTCPServer.configureCoils(0x00, 32);             //Coils
  modbusTCPServer.configureHoldingRegisters(0x00, 32);    //Holding Registers

  modbusTCPServer.coilWrite(9, 1); //Set Pump to CW
}

void loop() {
  ModBusTCPService();
  //updatemodbusTCPServer();        // Handles TCP Modbus connection to HMI
  updateArrays();            // Updates Coils, Input Bits, Holding & Input Registers on PLC
  
  if(!MB_C[13]){ //check if E-Stop has been pressed
    reset();
  }

  //Check for Clean Mode
  if(MB_C[10]){
    CleanMode();
  }

  // //Check for Production Mode
  if(MB_C[12]){
    reset(3);
    ProductionMode();
  }

  updatemodbusTCPServer(); //Updates modbus server values
  updateEquipmentStates();  //Write to PLC cards
}

///////////////////////////////////////Clean Mode///////////////////////////////////////////////////////////////////////////////////////////////
void CleanMode(){
  unsigned long currentTime = 0;
  unsigned long startTime = 0;
  bool dispenseLoopReady = true;

  setSingleOutput(8, comboCard, 1, 1); //turn mixer on

  //update clock and count for 1 minutes
  while(MB_HR[12] < 1){//Mixing Loop - check if time has been reached
    ModBusTCPService();
    updatemodbusTCPServer();
    updateArrays();           
    
    
    if(!MB_C[10]){ //check if CleanMode ended cancel clean mode
      dispenseLoopReady = false;
      reset();
      break;
    }
    if(!MB_C[13]){//check if E-stop has been pressed
      dispenseLoopReady = false;
      reset(1);
      break;
    }
    
    //Handles updating clock
    currentTime = millis();
    if(currentTime - startTime >= 1000){ // increment second or hour tag
      if(MB_HR[11] == 59){
        MB_HR[11] = 0;
        MB_HR[12]++;
      }else{
        MB_HR[11]++;
      }
      startTime = currentTime;
    }

    updatemodbusTCPServer(); //Updates Coils, Input Bits, Holding & Input Registers
  }

  
  setPumpSpeed(100);
  setSingleOutput(7, relayCard, 1, 8); //turn on Pump
  setSingleOutput(0, relayCard, 1, 1); //Open Main Valve
  
  int valve = 6;
  currentTime = 0;
  unsigned long startTimeSec = 0;
  unsigned long startTimeHalfMin = 0;

  while(dispenseLoopReady){//Dispensing Loop
    ModBusTCPService();
    updateArrays();   
    
    if(!MB_C[10]){ //check if CleanMode ended
      break;
    }
    if(!MB_C[13]){//check if E-stop has been pressed
      reset();
      break;;
    }
    
    //Handles updating clock
    currentTime = millis();
    if(currentTime - startTimeSec >= 1000){ // increment second or hour tag
      if(MB_HR[11] == 59){
        MB_HR[11] = 0;
        MB_HR[12]++;
      }else{
        MB_HR[11]++;
      }
      startTimeSec = currentTime;
    }

    //Open valves right to left
    if((currentTime - startTimeHalfMin >= 30000) && (valve > 0)){
      
      //Check if OOS
      if(!MB_C[(valve + 16)]){
        setSingleOutput(valve, relayCard, 1, valve+1); //open valve
      }
      valve--;
      startTimeHalfMin = currentTime;
    }
    //closes valves right to left
    else if((currentTime - startTimeHalfMin >= 30000) && (valve < 0)){
      setSingleOutput((valve*-1), relayCard, 0, (valve*-1)+1); //close valve
      startTimeHalfMin = currentTime;
      valve++;
    }

    if(valve == 0){ //Swap back to firs valve
      valve = -6;
    }

    updatemodbusTCPServer(); //Updates Coils, Input Bits, Holding & Input Registers
    updateEquipmentStates(); //Update PLC cards

    if(!MB_C[1] && !MB_C[2] && !MB_C[3] && !MB_C[4] && !MB_C[5] && !MB_C[6]){//once all valves are closed
      dispenseLoopReady = false;
    }
  }

  reset(1); //reset without E-Stop Check
}

///////////////////////////////////////Production Mode////////////////////////////////////////////////////////////////////////////////////////////
void ProductionMode(){
  ModBusTCPService();
  updateArrays(); 

  //Prod Setup
  bool dispeningLock = false;
  bool jogLock = false;
  bool jogmode = false;
  unsigned long startTime = 0;
  int TankRate = 25; //Added flowrate from tank - calculated in excel sheet
  int ConstantRate = 1560; ; //contant from pump - calculated in excel sheet
  float bottleSize;
  float endTime = 0;
  int index = 0;
  float offset[6] = {0,0,0,0,0,0};

  //Set Bottles Enum to "Not Ready"
  for(int x = 1; x <= 6; x++){
    MB_HR[x] = 1;
  }
  
  // FIFO buffer for production mode
  FIFObuf<int> Queue(6);

  modbusTCPServer.coilWrite(9, 1); //Set Pump to CW
  MB_C[9] = 1;
  
  updatemodbusTCPServer(); //Updates modbus server values
  updateEquipmentStates();

  //Start
  while(MB_C[12]){
    ModBusTCPService();
    updateArrays();           
   
    if(!MB_C[13]){//check if E-stop has been pressed
      reset();
      break;
    }

    //Jog Mode On
    if(MB_C[23]){
      setPumpSpeed(90);
      setSingleOutput(7, relayCard, 1, 8); //turn on Pump
      setSingleOutput(0, relayCard, 1, 1); //Open Main Valve

      jogLock = true;
    }
    //Jog Mode Off
    if(jogLock && !MB_C[23]){
      setSingleOutput(7, relayCard, 0, 8); //turn off Pump
      
      for(int x = 0; x<=6; x++){
        setSingleOutput(x, relayCard, 0, x+1); //close valves
      }
      setPumpSpeed(100);

      jogLock = false;
    }

    //Ensuring valves turn off when exsiting jog mode
    if(MB_C[24]){
      jogmode = true;
    }
    if(!MB_C[24] && jogmode){
      for(int x = 0; x<=6; x++){
        setSingleOutput(x, relayCard, 0, x+1); //close valves
      }
    }

    //Offset Logic Assign values
    for(int x = 1; x<=6; x++){
      offset[x] = MB_HR[x+15]
    }

    //Check for readied up stations and push to FIFO Queue
    for(int x = 1; x <= 6; x++){
      if(MB_HR[x] == 2){
        Queue.push(x); //Add to queue when ready
        MB_HR[x]++;    //Increment station state
      }
    }

    //Process Queue if station open, prod not paused and item in queue
    if(Queue.size() != 0 && !dispeningLock && !MB_C[24]){
      dispeningLock = true;

      setPumpSpeed(100);
      setSingleOutput(7, relayCard, 1, 8); //turn on Pump
      setSingleOutput(0, relayCard, 1, 1); //Open Main Valve
      modbusTCPServer.coilWrite(9, 1); //Set Pump to CW

      index = Queue.pop();

      setSingleOutput(index, relayCard, 1, index+1); //Open next up Valve
      MB_HR[index]++; //Increment status to filling

      if(MB_HR[13] == 1){
        bottleSize = 110;
      }
      else if(MB_HR[13] == 2){
        bottleSize = 240;
      }
      else if(MB_HR[13] == 3){
        bottleSize = 600;
      }
    
      int tankVolume = bottleSize * MB_HR[9]; //In MiliLiters

      float flowrate = float(TankRate * float(tankVolume - float((MB_HR[10] * bottleSize)))/3785) + ConstantRate;

      endTime = float(bottleSize / flowrate) * 60000 + offset[index];

      startTime = millis();
    }

    if(((millis() - startTime) > endTime) && dispeningLock){
      
      setSingleOutput(index, relayCard, 0, index+1); //Close next up Valve

      dispeningLock = false;

      MB_HR[10]++; //increment complete counter
      MB_HR[index]++; //Increment status to filled

      //If no other valves are ready or bottle count has been reached -  shut off pump and main solenoid
      if(Queue.size() == 0 || MB_HR[10] == MB_HR[9]){
        setSingleOutput(7, relayCard, 0, 8); //turn off Pump
        setSingleOutput(0, relayCard, 0, 1); //Close Main Valve
      }
    }

    //Check if bottle count is reached
    if(MB_HR[10] == MB_HR[9]){
      MB_C[25] = 1;
      MB_HR[10]--; //decrement bottle count so if no is selected mode can continue
    }

    updatemodbusTCPServer(); //Updates Coils, Input Bits, Holding & Input Registers
    updateEquipmentStates();
  }
  reset(1);//reset without E-Stop Check
}

///////////////////////////////////////Equipment Functions////////////////////////////////////////////////////////////////////////////////////////
void reset(){//set all Modbus data back to default and waits for E-Stop reset
  for(int x = 0; x <16; x++){   
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

void reset(int mode){//set all Modbus data back to default can skip some depedning on input
  if(mode == 1){// Full Reset
    for(int x = 0; x <32; x++){
      modbusTCPServer.coilWrite(x,0);
      modbusTCPServer.holdingRegisterWrite(x,0);
      //modbusTCPServer.inputRegisterWrite(x, 0);
      //modbusTCPServer.discreteInputWrite(x, 0);
	  }
  }
  else if(mode == 2){//Full Reset Skipping Mode Tags
    for(int x = 0; x <32; x++){
		
    if(x!=12 or x!=10){ //avoid overwriting phases
		  modbusTCPServer.coilWrite(x,0);
		}
		
		modbusTCPServer.holdingRegisterWrite(x,0);
		//modbusTCPServer.inputRegisterWrite(x, 0);
		//modbusTCPServer.discreteInputWrite(x, 0);
	  }
  }
  else if(mode == 3){
      for(int x = 1; x<=6; x++){
        modbusTCPServer.coilWrite(x,0);
        modbusTCPServer.holdingRegisterWrite(x,0);
      }

      modbusTCPServer.coilWrite(7,0); //pump
      modbusTCPServer.coilWrite(8,0); //Mixer
	}
	
	ModBusTCPService(); 
  updateArrays();
  updateEquipmentStates();
}

void setPumpSpeed(int speed) {

  MB_HR[0] = speed;

  int dutyCycle = map(speed, 100, 0, 255, 0);

  P1.writePWM(dutyCycle, 20000, 3, 1);            //Write Duty Cycle to pump PWM input
  modbusTCPServer.holdingRegisterWrite(0, speed);
}

void updateEquipmentStates(){ //Writes new states to P1AM cards
  //Update Relay Card - Valves & Pump On/Off
  for (int i = 0; i < 8; i++) {
    P1.writeDiscrete(MB_C[i], relayCard, i + 1);  //Data,Slot,Channel ... Channel is one-based.
  }

  P1.writeDiscrete(MB_C[8], comboCard, 1); //Mixer On/Off
  P1.writeDiscrete(MB_C[9], comboCard, 2); //Pump direction

  setPumpSpeed(MB_HR[0]);
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

void updateArrays(){//Updates Coils, Input Bits, Holding & Input Registers. Reads from modbusTCPServer(HMI)
  for (int i = 0; i < 32; i++) {
    MB_C[i] = modbusTCPServer.coilRead(i);
    MB_HR[i] = modbusTCPServer.holdingRegisterRead(i);
  }

  MB_C[13] = P1.readDiscrete(comboCard,1);   //E-Estop

}

void updatemodbusTCPServer(){ //Write updated coils and holding registersw to modBustTCPServer
  for(int x = 0; x<32; x++){
    modbusTCPServer.holdingRegisterWrite(x,MB_HR[x]);
    modbusTCPServer.coilWrite(x,MB_C[x]);
  }
}

void setSingleOutput(int index, int card, int value, int channel){
  MB_C[index] = value;
  P1.writeDiscrete(MB_C[index], card, channel);
  modbusTCPServer.coilWrite(index, MB_C[index]);

}