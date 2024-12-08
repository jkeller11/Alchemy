void setPumpSpeed(int speed) {
  int dutyCycle = map(speed, 0, 100, 255, 0);

  analogWrite(PWMpin, dutyCycle);
  modbusTCPServer.holdingRegisterWrite(0, speed);
}

void safteyPumpSpeed() {
  analogWrite(PWMpin, 0);
  modbusTCPServer.holdingRegisterWrite(0, 0);
}

void setPumpDirection(int direction, int speed) {
  if (speed != 0) {

    int y = speed / 4;
    for (int x = 1; x <= 4; x++) {
      setPumpSpeed(speed - (y * x));
      delay(200);
    }

    setPumpSpeed(0);
    delay(200);

    digitalWrite(PumpDirectionPin, direction);

    y = speed / 4;
    for (int x = 1; x <= 4; x++) {
      setPumpSpeed(y * x);
      delay(200);
    }
    setPumpSpeed(speed);
  } else {
    digitalWrite(PumpDirectionPin, direction);
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