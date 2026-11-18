#pragma once

#include <Arduino.h>

enum class GradientType { NONE, BRIGHTNESS, TEMPERATURE };
enum class TouchEvent {
  NONE, SINGLE_TAP, DOUBLE_TAP, STEP_UP, STEP_DOWN
};

// Un'istanza per ciascun modulo touch digitale, attivo HIGH.
class TouchModule {
 public:
  void Begin(uint8_t pin, GradientType type);
  TouchEvent Update(uint32_t now, bool lampOn);

 private:
  uint8_t touchPin = 0;
  GradientType gradientType = GradientType::NONE;
  int direzione = -1;
  uint32_t dimStepMs = 0;
  bool touchStable = LOW;
  bool touchPrevious = LOW;
  bool rawPrevious = LOW;
  bool waitingSecondTap = false;
  bool releasedFirstTap = false;
  bool longPress = false;
  bool doubleTapDetected = false;
  bool waitingInitialRelease = true;
  uint32_t lastChange = 0;
  uint32_t firstTapTime = 0;
  uint32_t pressStart = 0;
  uint32_t lastDimTime = 0;
};
