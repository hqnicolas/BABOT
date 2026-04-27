#pragma once

const char *buttonPressToString(ButtonPress press) {
  switch (press) {
    case NO_PRESS:
      return "none";
    case SINGLE_PRESS:
      return "single";
    case DOUBLE_PRESS:
      return "double";
    case TRIPLE_PRESS:
      return "triple";
    case QUAD_PRESS:
      return "quad";
    case LONG_PRESS:
      return "long";
    default:
      return "unknown";
  }
}

uint32_t makeColor(uint8_t r, uint8_t g, uint8_t b) {
  return 0;
}

void setStatusColor(uint32_t color) {
}

void blinkStatusColor(unsigned long interval, uint32_t color) {
}

void blinkDoubleFlash(uint32_t color) {
}


ButtonPress checkButton(bool calibrationMode) {
  static bool lastButtonState = HIGH;

  if (!isPinAssigned(babot::kBoardPins.buttonPin)) {
    return NO_PRESS;
  }

  const bool currentButtonState = digitalRead(babot::kBoardPins.buttonPin);
  const unsigned long now = millis();
  ButtonPress result = NO_PRESS;

  if (!gButtonArmed) {
    if (currentButtonState == HIGH && now - gBootGuardStartTime >= babot::kBootReleaseGuardMs) {
      gButtonArmed = true;
    }
    lastButtonState = currentButtonState;
    return NO_PRESS;
  }

  if (currentButtonState == LOW && lastButtonState == HIGH) {
    gButtonDownTime = now;
  }

  if (currentButtonState == HIGH && lastButtonState == LOW) {
    const unsigned long pressDuration = now - gButtonDownTime;
    if (pressDuration >= babot::kLongPressTimeMs) {
      result = LONG_PRESS;
      gPressCount = 0;
    } else {
      ++gPressCount;
      gLastPressTime = now;
    }
  }

  if (gPressCount > 0 && (calibrationMode || now - gLastPressTime > babot::kDoublePressGapMs)) {
    if (gPressCount == 1) {
      result = SINGLE_PRESS;
    } else if (gPressCount == 2) {
      result = DOUBLE_PRESS;
    } else if (gPressCount == 3) {
      result = TRIPLE_PRESS;
    } else if (gPressCount >= 4) {
      result = QUAD_PRESS;
    }
    gPressCount = 0;
  }

  lastButtonState = currentButtonState;
  return result;
}
