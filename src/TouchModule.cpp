#include "TouchModule.h"

namespace {
  constexpr uint32_t DOUBLE_TAP_MS = 500;
  constexpr uint32_t LONG_PRESS_MS = 600;
  constexpr uint32_t DEBOUNCE_MS = 10;
  constexpr uint32_t BRIGHTNESS_STEP_MS = 8; // 100 passi: minimo 800 ms
  constexpr uint32_t TEMPERATURE_STEP_MS = 8; // 100 passi: minimo 2000 ms
  constexpr uint32_t MIN_BRIGHTNESS = 10;
  constexpr uint32_t MAX_BRIGHTNESS = 75;
  constexpr uint32_t MIN_TEMPERATURE = 2700;
  constexpr uint32_t MAX_TEMPERATURE = 6500;

}

void TouchModule::Begin(uint8_t pin, GradientType type) {
  touchPin = pin;
  gradientType = type;
  pinMode(touchPin, INPUT);

  switch (type) {
      case GradientType::NONE:
          dimStepMs = 0;
          break;
      case GradientType::BRIGHTNESS:
          dimStepMs = BRIGHTNESS_STEP_MS;
          break;

      case GradientType::TEMPERATURE:
          dimStepMs = TEMPERATURE_STEP_MS;
          break;
  }

  direzione = -1;

  rawPrevious = digitalRead(touchPin);
  touchStable = rawPrevious;
  touchPrevious = touchStable;

  waitingSecondTap = false;
  releasedFirstTap = false;
  longPress = false;
  doubleTapDetected = false;
  waitingInitialRelease = true;

  lastChange = millis();
  firstTapTime = pressStart = lastDimTime = lastChange;
}

TouchEvent TouchModule::Update(uint32_t now, bool lampOn) {
  TouchEvent event = TouchEvent::NONE;
  const bool raw = digitalRead(touchPin);

  // Debounce originale.
  if (raw != rawPrevious) {
    lastChange = now;
    rawPrevious = raw;
  }

  if (now - lastChange >= DEBOUNCE_MS) {
    touchStable = raw;
  }

  // Se il dito e' gia' appoggiato all'avvio, attendere il rilascio.
  if (waitingInitialRelease) {
    if (touchStable == LOW && now - lastChange >= DEBOUNCE_MS) {
      waitingInitialRelease = false;
    }
    touchPrevious = touchStable;
    return TouchEvent::NONE;
  }

  // Conferma il tap scaduto prima di registrare una nuova pressione.
  if (waitingSecondTap && releasedFirstTap &&
      now - firstTapTime > DOUBLE_TAP_MS) {
    waitingSecondTap = false;
    event = TouchEvent::SINGLE_TAP;
  }

  // Inizio pressione.
  if (touchStable == HIGH && touchPrevious == LOW) {
    pressStart = now;
    longPress = false;
    doubleTapDetected = false;

    if (waitingSecondTap && releasedFirstTap &&
        now - firstTapTime <= DOUBLE_TAP_MS) {
      event = TouchEvent::DOUBLE_TAP;
      waitingSecondTap = false;
      doubleTapDetected = true;
    } else {
      firstTapTime = now;
      waitingSecondTap = true;
      releasedFirstTap = false;
    }
  }

  // Tocco prolungato: invertire la direzione una volta per pressione.
  if (touchStable == HIGH && !longPress &&
      !doubleTapDetected && now - pressStart >= LONG_PRESS_MS) {
    longPress = true;
    waitingSecondTap = false;
    direzione = -direzione;
    lastDimTime = now;
    //event = TouchEvent::LONG_PRESS;
  }

  // Regolazione del gradiente.
  if (gradientType != GradientType::NONE && lampOn &&
    touchStable == HIGH && longPress &&
    now - lastDimTime >= dimStepMs) {
    lastDimTime = now;
    event = direzione > 0 ? TouchEvent::STEP_UP : TouchEvent::STEP_DOWN;
  }

  // Rilascio.
  if (touchStable == LOW && touchPrevious == HIGH) {
    if (longPress) {
      longPress = false;
      //event = TouchEvent::LONG_PRESS_END;
    } else if (waitingSecondTap) {
      releasedFirstTap = true;
    }
    doubleTapDetected = false;
  }

  // Confermare il singolo solo dopo la finestra del doppio tocco.
  if (waitingSecondTap && releasedFirstTap &&
      now - firstTapTime > DOUBLE_TAP_MS) {
    waitingSecondTap = false;
    event = TouchEvent::SINGLE_TAP;
  }

  touchPrevious = touchStable;
  return event;
}
