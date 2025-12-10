#include "CoinHandler.h"

// === Task Handle ===
TaskHandle_t coinTaskHandle;

// === Coin Variables ===
volatile int pulseCount = 0;
unsigned long lastPulseTime = 0;
int totalPesos = 0;
const unsigned long coinTimeout = 150;  // ms

// === Forward declaration ===
void coinTask(void *pvParameters);

// === Public API ===
void startCoinTask() {
  xTaskCreatePinnedToCore(
    coinTask,
    "CoinTask",
    4096,
    NULL,
    1,
    &coinTaskHandle,
    1
  );
  Serial.println("[CoinHandler] Task started.");
}

int getTotalPesos() {
  return totalPesos;
}

void resetTotalPesos() {
  totalPesos = 0;
}

// === Coin Task Implementation ===
void coinTask(void *pvParameters) {
  bool lastState = HIGH;
  unsigned long lowStartTime = 0;
  const unsigned long debounceTime = 10;
  const unsigned long minPulseWidth = 15;
  static unsigned long lastChangeTime = 0;

  pinMode(COIN_PIN, INPUT_PULLUP);

  for (;;) {
    bool currentState = digitalRead(COIN_PIN);
    unsigned long now = millis();

    // === Detect falling edge ===
    if (lastState == HIGH && currentState == LOW) {
      if (now - lastChangeTime > debounceTime)
        lowStartTime = now;
      lastChangeTime = now;
    }

    // === Detect rising edge ===
    if (lastState == LOW && currentState == HIGH) {
      unsigned long pulseWidth = now - lowStartTime;
      if (pulseWidth >= minPulseWidth) {
        pulseCount++;
        lastPulseTime = now;
       // Serial.printf("[CoinHandler] Valid pulse detected: %d\n", pulseCount);
      }
      lastChangeTime = now;
    }

    lastState = currentState;

    // === Evaluate after timeout ===
    if (pulseCount > 0 && (now - lastPulseTime) > coinTimeout) {
      int value = 0;
      if (pulseCount >= 1 && pulseCount <= 3) value = 1;
      else if (pulseCount >= 5 && pulseCount <= 7) value = 5;
      else if (pulseCount >= 10 && pulseCount <= 14) value = 10;

      if (value > 0) {
        totalPesos += value;
        Serial.printf("[CoinHandler] Detected ₱%d → Total = ₱%d\n", value, totalPesos);
      }
      pulseCount = 0;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}
