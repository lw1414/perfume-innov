#ifndef CLIHANDLER_H
#define CLIHANDLER_H

#include <Arduino.h>

namespace CLIHandler {
  void init();
  void handleSerial();
  void processCommand(String cmd);
  void printHelp();
}

#endif
