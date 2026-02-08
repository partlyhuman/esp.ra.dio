#pragma once
#include <Arduino.h>

// Note only some GPIO can be used as wake sources
gpio_num_t wakeGpio = GPIO_NUM_0;

// Not for production: drive these low as ground
uint8_t pinExtraGrounds[]{4, 21};

// buttons A, B, C, D, SEL, START
uint8_t buttonPins[]{10, 9, 8, 7, 6, 5};

// dirs LEFT, RIGHT, DOWN, UP
uint8_t directionPins[]{3, 2, 1, 0};
