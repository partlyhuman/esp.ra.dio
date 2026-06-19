#define BOUNCE_WITH_PROMPT_DETECTION
#include <Arduino.h>
#include <Bounce2.h>
#include <esp_sleep.h>

#include "BLEGamepad.h"
#include "blink.h"
#include "config.h"
#include "log.h"

// Note only some GPIO can be used as wake sources
constexpr gpio_num_t wakeGpio = GPIO_NUM_0;

// Not for production: drive these low as ground
constexpr uint8_t pinExtraGrounds[]{};

// buttons A, B, C, D, SEL, START
constexpr uint8_t buttonPins[]{20, 3, 10, 21, 1, 0};
constexpr size_t INDEX_BUTTON_SEL = 4;
constexpr size_t INDEX_BUTTON_START = 5;

// dirs LEFT, RIGHT, DOWN, UP
constexpr uint8_t directionPins[]{4, 5, 6, 7};

static const int SCAN_INTERVAL_MS = 1;
static const int DEBOUNCE_MS = 10;
static const int HOLD_MS = 3000;
static const unsigned long SLEEP_AFTER_MS = 2 * 60 * 1000;  // minutes
static const char *TAG = "Main";

static const uint8_t O_SPECIAL = 64;
static constexpr uint8_t physicalButtons[]{BUTTON_1, BUTTON_2, BUTTON_3,
                                           BUTTON_4, BUTTON_7, BUTTON_8};

enum Direction { DIR_LEFT, DIR_RIGHT, DIR_DOWN, DIR_UP, DIR_COUNT };

constexpr size_t BUTTON_COUNT = sizeof(buttonPins);
static Bounce debouncers[BUTTON_COUNT];
constexpr static bool ADVERTISE_ON_START = true;
// Caution_ long strings here can be too big for advertise blob - watch logs
static BleGamepad gamepad("ESP.RA.DIO", "PH", 100, !ADVERTISE_ON_START);
// static BleGamepad gamepad("ESP.RA.DIO", "PH", 100, false);

void setup() {
#if LOG_LEVEL >= 4
  esp_log_level_set("*", ESP_LOG_VERBOSE);
#endif
  Serial.begin(115200);

  bool woke = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO);

  pinMode(PIN_LED, OUTPUT);
  gpio_hold_dis((gpio_num_t)PIN_LED);
  digitalWrite(PIN_LED, HIGH);

  // for (size_t i = 0; i < sizeof(pinExtraGrounds); i++) {
  //   pinMode(pinExtraGrounds[i], OUTPUT);
  //   digitalWrite(pinExtraGrounds[i], LOW);
  // }

  for (size_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    debouncers[i] = Bounce(buttonPins[i], DEBOUNCE_MS);
  }
  for (size_t i = 0; i < DIR_COUNT; i++) {
    pinMode(directionPins[i], INPUT_PULLUP);
  }

  LOGD(TAG, "Pins setup");

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
  LOGI(TAG, "Setup complete");
}

static inline int8_t direction(Direction dir) {
  return digitalRead(directionPins[dir]) == LOW;
}

static inline bool isHeld(size_t buttonIndex) {
  return debouncers[buttonIndex].read() == LOW &&
         debouncers[buttonIndex].currentDuration() > HOLD_MS;
}

void deepSleep() {
  LOGI(TAG, "Going to sleep");
  stopBlink();
  gamepad.end();

  // Disable any pullups before sleep, except the wake trigger. is this
  // necessary????
  // for (size_t i = 0; i < sizeof(pinExtraGrounds); i++) {
  //   pinMode(pinExtraGrounds[i], INPUT);
  // }
  for (size_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT);
  }
  for (size_t i = 0; i < DIR_COUNT; i++) {
    pinMode(directionPins[i], INPUT);
  }

  // keep LED off
  digitalWrite(PIN_LED, LOW);
  gpio_deep_sleep_hold_en();
  if (ESP_OK != gpio_hold_en((gpio_num_t)PIN_LED)) {
    LOGE(TAG, "Failed to hold LED pin for sleep");
  }

  // setup wake pin
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
  vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));

  static unsigned long lastConnectedTime = millis();
  if (!gamepad.isConnected()) {
    startBlink();

    auto idleFor = millis() - lastConnectedTime;
    if (idleFor > SLEEP_AFTER_MS) {
      LOGI(TAG, "Idle for %ld sec, sleeping.", idleFor / 1000);
      deepSleep();
      return;
    }
  } else {
    lastConnectedTime = millis();
    stopBlink();
  }
  // CHANGE: now falling through, to update all debouncers etc and watch for
  // special key combos whether we are connected or not

  bool sendReport = false;

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    debouncers[i].update();
    if (debouncers[i].fell()) {
      auto button = physicalButtons[i];
      if (button >= O_SPECIAL) {
        button -= O_SPECIAL;
        LOGD(TAG, "SPECIAL BTN %d", button);
        gamepad.pressSpecialButton(button);
      } else {
        LOGD(TAG, "BTN %d", button);
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
    // Already checks if connected
    gamepad.sendReport();
  }

  if (isHeld(INDEX_BUTTON_SEL) && isHeld(INDEX_BUTTON_START)) {
    LOGI(TAG, "Held START+SEL, clearing pairs and entering 'pairing mode'");
    gamepad.deleteAllBonds(false);
    LOGI(TAG, "Entering pairing mode");
    startBlink();
    gamepad.enterPairingMode();
    stopBlink();
  }
}
