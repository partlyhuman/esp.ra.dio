#define BOUNCE_WITH_PROMPT_DETECTION
#include <Arduino.h>
#include <BLEGamepad.h>
#include <Bounce2.h>
#include <esp_sleep.h>

#include "config.h"
#include "log.h"

static const int FPS = 60;
static const int DEBOUNCE_MS = 5;
static const unsigned long SLEEP_AFTER_MS = 1000 * 60 * 1;
static const char *TAG = "Main";

static const uint8_t O_SPECIAL = 64;
static uint8_t physicalButtons[]{BUTTON_1, BUTTON_2, BUTTON_3,
                                 BUTTON_4, BUTTON_7, BUTTON_8};

enum Direction { DIR_LEFT, DIR_RIGHT, DIR_DOWN, DIR_UP, DIR_COUNT };

constexpr size_t BUTTON_COUNT = sizeof(buttonPins);
static Bounce debouncers[BUTTON_COUNT];
static BleGamepad gamepad("ESP.RA.DIO Joystick", "Partlyhuman");

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);

  bool woke = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO);

  for (size_t i = 0; i < sizeof(pinExtraGrounds); i++) {
    pinMode(pinExtraGrounds[i], OUTPUT);
    digitalWrite(pinExtraGrounds[i], LOW);
  }

  for (size_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);

    debouncers[i] = Bounce();
    debouncers[i].attach(buttonPins[i]);
    debouncers[i].interval(DEBOUNCE_MS);
  }
  for (size_t i = 0; i < DIR_COUNT; i++) {
    pinMode(directionPins[i], INPUT_PULLUP);
  }

  BleGamepadConfiguration config;
  config.setAutoReport(false);
  config.setControllerType(CONTROLLER_TYPE_JOYSTICK);
  // Allow default espressif VID
  // config.setVid();
  // My bluetooth spinner starts at 0xe389
  config.setPid(0xe388);
  // The only valid values are: -12, -9, -6, -3, 0, 3, 6 and 9
  // Max of 9 is default
  config.setTXPowerLevel(6);
  config.setWhichSpecialButtons(false, false, false, false, false, false, false,
                                false);
  // might consider adding 2 axes that are unused, or having an analog mode
  // configurable
  config.setWhichAxes(false, false, false, false, false, false, false, false);
  config.setHatSwitchCount(1);
  // config.setAxesMin(0);
  // config.setAxesMax(AXIS_MAX);
  config.setButtonCount(8);
  config.setAutoReport(false);
  gamepad.begin(&config);

  // gamepad.setAxes(AXIS_MIDDLE, AXIS_MIDDLE);
}

static inline int8_t direction(Direction dir) {
  return digitalRead(directionPins[dir]) == LOW;
}

void deepSleep() {
  gamepad.end();

  // Disable any pullups before sleep, except the wake trigger. is this
  // necessary????
  for (size_t i = 0; i < sizeof(pinExtraGrounds); i++) {
    pinMode(pinExtraGrounds[i], INPUT);
  }
  for (size_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT);
  }
  for (size_t i = 0; i < DIR_COUNT; i++) {
    pinMode(directionPins[i], INPUT);
  }

  // This doesn't work because we're using a fake ground that stops getting
  // driven low
  if (ESP_OK != gpio_pullup_en(wakeGpio)) {
    LOGE(TAG, "Failed to pullup");
  }
  if (ESP_OK != esp_deep_sleep_enable_gpio_wakeup(1 << wakeGpio,
                                                  ESP_GPIO_WAKEUP_GPIO_LOW)) {
    LOGE(TAG, "Failed to enable GPIO wakeup");
  }
  LOGI(TAG, "Shutdown");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {
  static unsigned long lastConnectedTime = millis();
  if (!gamepad.isConnected()) {
    auto idleFor = millis() - lastConnectedTime;
    if (idleFor > SLEEP_AFTER_MS) {
      LOGI(TAG, "Idle for %ld sec, sleeping.", idleFor / 1000);
      deepSleep();
    }
    return;
  }

  lastConnectedTime = millis();
  bool sendReport = false;

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    debouncers[i].update();
    if (debouncers[i].fell()) {
      auto button = physicalButtons[i];
      if (button >= O_SPECIAL) {
        button -= O_SPECIAL;
        LOGV(TAG, "SPECIAL BTN %d", button);
        gamepad.pressSpecialButton(button);
      } else {
        LOGV(TAG, "BTN %d", button);
        gamepad.press(button);
      }
      sendReport = true;
    } else if (debouncers[i].rose()) {
      auto button = physicalButtons[i];
      if (button >= O_SPECIAL) {
        button -= O_SPECIAL;
        gamepad.releaseSpecialButton(button);
      } else {
        gamepad.release(button);
      }
      sendReport = true;
    }
  }

  // let these be -1, 0, 1, can then map to axes later
  static int8_t lastX, lastY;
  int8_t x = direction(DIR_RIGHT) - direction(DIR_LEFT);
  int8_t y = direction(DIR_DOWN) - direction(DIR_UP);

  if (x != lastX || y != lastY) {
    lastX = x;
    lastY = y;
    sendReport = true;

    auto hat = HAT_CENTERED;
    if (x < 0) {
      hat = y == 0 ? HAT_LEFT : (y > 0 ? HAT_DOWN_LEFT : HAT_UP_LEFT);
    } else if (x == 0) {
      hat = y == 0 ? HAT_CENTERED : (y > 0 ? HAT_DOWN : HAT_UP);
    } else if (x > 0) {
      hat = y == 0 ? HAT_RIGHT : (y > 0 ? HAT_DOWN_RIGHT : HAT_UP_RIGHT);
    }
    gamepad.setHat1(hat);
  }

  if (sendReport) {
    gamepad.sendReport();
  }

  delay(1000 / FPS);
}
