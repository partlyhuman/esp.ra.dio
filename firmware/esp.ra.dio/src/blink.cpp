#include "blink.h"

#include <Arduino.h>

#include "config.h"

static TaskHandle_t blinkTaskHandle = nullptr;
static unsigned long blinkIntervalMs = 750;

void blinkTask(void *_) {
  TickType_t delayTicks = pdMS_TO_TICKS(blinkIntervalMs);

  while (true) {
    digitalWrite(PIN_LED, HIGH);
    vTaskDelay(delayTicks);

    digitalWrite(PIN_LED, LOW);
    vTaskDelay(delayTicks);
  }
}

void startBlink() {
  if (!isBlinking()) {
    xTaskCreate(blinkTask, "blinkTask", 2048, nullptr, 1, &blinkTaskHandle);
  }
}

void stopBlink() {
  if (isBlinking()) {
    TaskHandle_t tmp = blinkTaskHandle;
    blinkTaskHandle = nullptr;
    vTaskDelete(tmp);
    digitalWrite(PIN_LED, HIGH);
  }
}

bool isBlinking() { return blinkTaskHandle != nullptr; }