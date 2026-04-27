#include "X9C104Digipot.h"

namespace babot {

namespace {

constexpr unsigned int kPulseDelayUs = 10;

}

X9C104Digipot::X9C104Digipot(int chipSelectPin, int incrementPin, int upDownPin)
    : chipSelectPin_(chipSelectPin),
      incrementPin_(incrementPin),
      upDownPin_(upDownPin),
      currentTap_(-1),
      outputEnabled_(true) {}

bool X9C104Digipot::begin(bool enableOutput) {
  outputEnabled_ = enableOutput;
  if (!outputEnabled_) {
    if (currentTap_ < 0) {
      currentTap_ = 0;
    }
    return true;
  }

  pinMode(chipSelectPin_, OUTPUT);
  pinMode(incrementPin_, OUTPUT);
  pinMode(upDownPin_, OUTPUT);

  digitalWrite(chipSelectPin_, HIGH);
  digitalWrite(incrementPin_, HIGH);
  digitalWrite(upDownPin_, LOW);
  return true;
}

void X9C104Digipot::setTap(uint8_t targetTap, bool persist) {
  const int clampedTarget = constrain(targetTap, 0, kDigitalPotMaxTap);
  if (!outputEnabled_) {
    currentTap_ = clampedTarget;
    return;
  }

  if (currentTap_ < 0) {
    homeToZero();
  }

  while (currentTap_ < clampedTarget) {
    step(true);
  }

  while (currentTap_ > clampedTarget) {
    step(false);
  }

  if (persist) {
    persistPosition();
  }
}

uint8_t X9C104Digipot::currentTap() const {
  return currentTap_ < 0 ? 0 : static_cast<uint8_t>(currentTap_);
}

bool X9C104Digipot::dummyModeActive() const {
  return !outputEnabled_;
}

void X9C104Digipot::homeToZero() {
  digitalWrite(chipSelectPin_, LOW);
  digitalWrite(upDownPin_, LOW);
  digitalWrite(incrementPin_, HIGH);

  for (uint8_t i = 0; i < kDigitalPotMaxTap; ++i) {
    digitalWrite(incrementPin_, LOW);
    delayMicroseconds(kPulseDelayUs);
    digitalWrite(incrementPin_, HIGH);
    delayMicroseconds(kPulseDelayUs);
  }

  digitalWrite(chipSelectPin_, HIGH);
  currentTap_ = 0;
}

void X9C104Digipot::step(bool up) {
  digitalWrite(chipSelectPin_, LOW);
  digitalWrite(upDownPin_, up ? HIGH : LOW);
  digitalWrite(incrementPin_, LOW);
  delayMicroseconds(kPulseDelayUs);
  digitalWrite(incrementPin_, HIGH);
  delayMicroseconds(kPulseDelayUs);
  digitalWrite(chipSelectPin_, HIGH);

  currentTap_ += up ? 1 : -1;
}

void X9C104Digipot::persistPosition() {
  digitalWrite(chipSelectPin_, LOW);
  digitalWrite(incrementPin_, HIGH);
  delayMicroseconds(kPulseDelayUs);
  digitalWrite(chipSelectPin_, HIGH);
}

}
