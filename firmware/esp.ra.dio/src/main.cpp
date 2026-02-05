/*
 * This code programs a number of pins on an ESP32 as buttons on a BLE gamepad
 *
 * It uses arrays to cut down on code
 *
 * Uses the Bounce2 library to debounce all buttons
 *
 * Uses the rose/fell states of the Bounce instance to track button states
 *
 * Before using, adjust the BUTTON_COUNT, buttonPins and physicalButtons to suit
 * your senario
 *
 */

#define BOUNCE_WITH_PROMPT_DETECTION  // Make button state changes available
                                      // immediately

#include <Arduino.h>
#include <BleGamepad.h>  // https://github.com/lemmingDev/ESP32-BLE-Gamepad
#include <Bounce2.h>     // https://github.com/thomasfredericks/Bounce2

// #include <algorithm>
// #include <array>
// #include <pair>

#define PIN_EXTRA_GROUND 4
#define BUTTON_COUNT 6
#define AXIS_MIN 0
#define AXIS_MAX INT16_MAX - 1
#define AXIS_MIDDLE AXIS_MIN + (AXIS_MAX - AXIS_MIN) / 2
typedef uint8_t pin_number_t;
typedef uint8_t button_number_t;
// constexpr std::array<std::pair<button_number_t, pin_number_t>,
// BUTTON_COUNT>{};
const button_number_t O_HAT = 64;
const button_number_t O_SPECIAL = 128;
pin_number_t buttonPins[BUTTON_COUNT]{5, 6, 10, 9, 8, 7};
button_number_t physicalButtons[BUTTON_COUNT]{O_SPECIAL + SELECT_BUTTON,
                                              O_SPECIAL + START_BUTTON,
                                              BUTTON_1,
                                              BUTTON_2,
                                              BUTTON_3,
                                              BUTTON_4};
enum Direction { DIR_LEFT, DIR_RIGHT, DIR_DOWN, DIR_UP, DIR_COUNT };
pin_number_t directionPins[4]{3, 2, 1, 0};

Bounce debouncers[BUTTON_COUNT];
BleGamepad bleGamepad("ESP.RA.DIO Joystick", "Partlyhuman");

void setup() {
  Serial.begin(115200);

#ifdef PIN_EXTRA_GROUND
  // NOT THE GREATEST THING, DEMO ONLY, MAKE AN EXTRA GROUND, PROCEED WITH
  // CAUTION
  pinMode(PIN_EXTRA_GROUND, OUTPUT);
  digitalWrite(PIN_EXTRA_GROUND, LOW);
#endif

  for (size_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);

    debouncers[i] = Bounce();
    debouncers[i].attach(buttonPins[i]);
    // After setting up the button, setup the
    // Bounce instance :
    debouncers[i].interval(5);
  }
  for (size_t i = 0; i < 4; i++) {
    pinMode(directionPins[i], INPUT_PULLUP);
  }

  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(false);
  // bleGamepadConfig.setControllerType(CONTROLLER_TYPE_JOYSTICK);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  // bleGamepadConfig.setVid(); // Allow default espressif VID
  bleGamepadConfig.setPid(0xe388);  // My bluetooth spinner starts at 0xe389
  // bleGamepadConfig.setTXPowerLevel(
  // txPowerLevel);  // Defaults to 9 if not set. (Range: -12 to 9 dBm)
  // bleGamepadConfig.setModelNumber("1.0");
  // bleGamepadConfig.setFirmwareRevision("2.0");
  // bleGamepadConfig.setHardwareRevision("1.7");
  // Only XY axes
  bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false,
                                false);
  bleGamepadConfig.setWhichSpecialButtons(true, true, false, false, false,
                                          false, false, false);
  bleGamepadConfig.setHatSwitchCount(1);
  // bleGamepadConfig.setAxesMin(0);
  // bleGamepadConfig.setAxesMax(AXIS_MAX);
  bleGamepadConfig.setButtonCount(BUTTON_COUNT);
  bleGamepadConfig.setAutoReport(false);
  bleGamepad.begin(&bleGamepadConfig);

  // bleGamepad.setAxes(AXIS_MIDDLE, AXIS_MIDDLE);
}

void loop() {
  if (!bleGamepad.isConnected()) {
    // Serial.print(".");
    return;
  }

  bool sendReport = false;

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    debouncers[i].update();
    if (debouncers[i].fell()) {
      auto button = physicalButtons[i];
      if (button >= O_SPECIAL) {
        button -= O_SPECIAL;
        bleGamepad.pressSpecialButton(button);
      } else {
        bleGamepad.press(button);
      }
      sendReport = true;
    } else if (debouncers[i].rose()) {
      auto button = physicalButtons[i];
      if (button >= O_SPECIAL) {
        button -= O_SPECIAL;
        bleGamepad.releaseSpecialButton(button);
      } else {
        bleGamepad.release(button);
      }
      sendReport = true;
    }
  }

  // let these be -1, 0, 1
  static int8_t lastX, lastY;
  int8_t x = 0, y = 0;
  for (int dir = 0; dir < DIR_COUNT; dir++) {
    if (digitalRead(directionPins[dir]) == LOW) {
      switch ((Direction)dir) {
        case DIR_LEFT:
          x--;
          break;
        case DIR_RIGHT:
          x++;
          break;
        case DIR_UP:
          y--;
          break;
        case DIR_DOWN:
          y++;
          break;
      }
    }
  }

  if (x != lastX || y != lastY) {
    lastX = x;
    lastY = y;

    auto hat = HAT_CENTERED;
    if (x < 0) {
      hat = y == 0 ? HAT_LEFT : (y > 0 ? HAT_DOWN_LEFT : HAT_UP_LEFT);
    } else if (x == 0) {
      hat = y == 0 ? HAT_CENTERED : (y > 0 ? HAT_DOWN : HAT_UP);
    } else if (x > 0) {
      hat = y == 0 ? HAT_RIGHT : (y > 0 ? HAT_DOWN_RIGHT : HAT_UP_RIGHT);
    }
    bleGamepad.setHat1(hat);

    // bleGamepad.setAxes(x, y);
    sendReport = true;
  }

  if (sendReport) {
    bleGamepad.sendReport();
  }

  delay(20);  // (Un)comment to remove/add delay between loops
}
