#include "RelayHandler.h"
// global ShiftRegister instance
ShiftRegister OUTPUT_CONTROL_PORT(OUT_CTRL_DIN, OUT_CTRL_CS, OUT_CTRL_CLK, 1);
// global RelayHandler instance
RelayHandler relayHandler(OUTPUT_CONTROL_PORT);  // ✅ pass the ShiftRegister reference

RelayHandler::RelayHandler(ShiftRegister &outputPort)
  : _outputPort(outputPort) {
  for (int i = 0; i < NUM_RELAYS; i++) {
    relayActive[i] = false;
    relayStartTime[i] = 0;
    relayTargetDuration[i] = 0;
  }
}

void RelayHandler::begin() {
  _outputPort.setBitOrder(LSBFIRST);
  _outputPort.setAll(BIT_OFF);
  _outputPort.updateRegisters();
  updateDispenseStatusBits();  // show initial dispense status on bits 5-8
}

void RelayHandler::activateRelayAsync(int relayNum, unsigned long baseDurationMs) {
  if (relayNum < 1 || relayNum > NUM_RELAYS) return;
  relayNum--;

  unsigned long relayPrice = relayPrices[relayNum];  // read from SystemConfig
  int pesosInserted = getTotalPesos();

  Serial.printf("Debug: relayPrice=%lu, pesosInserted=%d, baseDuration=%lu\n",
                relayPrice, pesosInserted, baseDurationMs);

  // === Fixed base duration ===
  unsigned long actualDurationMs = (unsigned long)(((float)pesosInserted / (float)relayPrice) * baseDurationMs);


  relayActive[relayNum] = true;
  relayStartTime[relayNum] = millis();
  relayTargetDuration[relayNum] = actualDurationMs;

  Serial.printf("\nRelay %d ON for %lu ms (Base: %lu ms, Price: ₱%lu, Inserted: ₱%d)\n",
                relayNum + 1, actualDurationMs, baseDurationMs, relayPrice, pesosInserted);

  _outputPort.setBit(relayNum + 1, BIT_ON);
  _outputPort.updateRegisters();

  publishRelayEventMQTT(relayNum + 1, pesosInserted, "ON");
  resetTotalPesos();
}




void RelayHandler::update() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relayActive[i] && (now - relayStartTime[i] >= relayTargetDuration[i])) {
      relayActive[i] = false;
      _outputPort.setBit(i + 1, BIT_OFF);
      _outputPort.updateRegisters();
      Serial.printf("Relay %d OFF | Credit after dispense: ₱%d\n\n", i + 1, getTotalPesos());
    }
  }
  updateDispenseStatusBits();  // continuously reflect dispenseStatus on bits 5-8
}

void RelayHandler::activateShiftBit(int bitNum, bool on) {
  if (bitNum < 5 || bitNum > 8) return;
  _outputPort.setBit(bitNum, on ? BIT_ON : BIT_OFF);
  _outputPort.updateRegisters();
}

void RelayHandler::updateDispenseStatusBits() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    // bit5..bit8 mirrors dispenseStatus[i] (1=ON, 0=OFF)
    activateShiftBit(5 + i, dispenseStatus[i] == 1);
  }
}

void RelayHandler::saveRelayConfigToEEPROM(int relayNum) {
  saveRelayDurationToEEPROM(relayNum + 1, relayDurations[relayNum]);
  saveDispenseStatusToEEPROM(relayNum + 1, dispenseStatus[relayNum]);
}
