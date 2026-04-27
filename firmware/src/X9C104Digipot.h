#pragma once

#include <Arduino.h>

#include "BoardConfig.h"

namespace babot {

class X9C104Digipot {
 public:
  X9C104Digipot(int chipSelectPin, int incrementPin, int upDownPin);

  bool begin(bool enableOutput = true);
  void setTap(uint8_t targetTap, bool persist = false);
  uint8_t currentTap() const;
  bool dummyModeActive() const;

 private:
  void homeToZero();
  void step(bool up);
  void persistPosition();

  int chipSelectPin_;
  int incrementPin_;
  int upDownPin_;
  int currentTap_;
  bool outputEnabled_;
};

}
