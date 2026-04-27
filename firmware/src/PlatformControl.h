#pragma once

#include <Preferences.h>
#include "third_party/Servo/src/Servo.h"

#include "BoardConfig.h"

namespace babot {

enum PlatformBackend : uint8_t {
  PLATFORM_BACKEND_NONE,
  PLATFORM_BACKEND_REAL,
  PLATFORM_BACKEND_DUMMY
};

struct PlatformInitResult {
  PlatformBackend backend;
  bool preferencesAvailable;
  bool attachSucceededA;
  bool attachSucceededB;
  bool attachSucceededC;
  int channelA;
  int channelB;
  int channelC;
};

class PlatformControl {
 public:
  explicit PlatformControl(const BoardPins &pins);

  PlatformInitResult begin(bool forceDummyBackend = false);
  void movePlatform(float rollDeg, float pitchDeg, float height);
  void moveServos(float a, float b, float c);
  void setServoOffsets(int offsetA, int offsetB, int offsetC);
  void resetServoOffsets();
  bool loadServoOffsets();
  void saveServoOffsets() const;
  int servoOffsetA() const;
  int servoOffsetB() const;
  int servoOffsetC() const;
  bool motionAvailable() const;
  bool dummyBackendActive() const;
  PlatformInitResult lastInitResult() const;

 private:
  void detachAllServos();
  int clampOffset(int value) const;

  const BoardPins &pins_;
  Servo servoA_;
  Servo servoB_;
  Servo servoC_;
  mutable Preferences preferences_;
  int servoOffsetA_;
  int servoOffsetB_;
  int servoOffsetC_;
  PlatformBackend backend_;
  PlatformInitResult lastInitResult_;
};

}
