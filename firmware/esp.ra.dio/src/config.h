#pragma once
#include <Arduino.h>

#ifndef PIN_LED
#define PIN_LED 8
#endif

// Note only some GPIO can be used as wake sources
gpio_num_t wakeGpio = GPIO_NUM_0;

// Not for production: drive these low as ground
uint8_t pinExtraGrounds[]{};

// buttons A, B, C, D, SEL, START
uint8_t buttonPins[]{20, 3, 10, 21, 1, 0};

// dirs LEFT, RIGHT, DOWN, UP
uint8_t directionPins[]{4, 5, 6, 7};
