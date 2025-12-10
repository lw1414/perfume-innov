    #ifndef RELAYHANDLER_H
    #define RELAYHANDLER_H

    #include <Arduino.h>
    #include "ShiftRegister.h"
    #include "MQTTMonitor.h"
    #include "CoinHandler.h"

    #define NUM_RELAYS 4

    class RelayHandler {
    public:
        RelayHandler(ShiftRegister &outputPort);

        void begin();
        void update();  // call in loop() to handle relay timing

        void activateRelayAsync(int relayNum, unsigned long durationMs);
        void saveRelayConfigToEEPROM(int relayNum);

        void updateDispenseStatusBits(); // activate shift bits 5-8 according to dispenseStatus

    private:
        ShiftRegister &_outputPort;
        bool relayActive[NUM_RELAYS];
        unsigned long relayStartTime[NUM_RELAYS];
        unsigned long relayTargetDuration[NUM_RELAYS];

        void activateShiftBit(int bitNum, bool on);
    };

    #endif
