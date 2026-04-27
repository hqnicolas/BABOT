#include "PlatformControl.h"

#include <math.h>

namespace babot {

namespace {

constexpr char kPreferencesNamespace[] = "babot";
constexpr char kServoOffsetAKey[] = "servo_a";
constexpr char kServoOffsetBKey[] = "servo_b";
constexpr char kServoOffsetCKey[] = "servo_c";

}

PlatformControl::PlatformControl(const BoardPins &pins)
    : pins_(pins),
      servoOffsetA_(kServoOffsetDefault),
      servoOffsetB_(kServoOffsetDefault),
      servoOffsetC_(kServoOffsetDefault),
      backend_(PLATFORM_BACKEND_NONE),
      lastInitResult_{PLATFORM_BACKEND_NONE, false, false, false, false, -1, -1, -1} {}

PlatformInitResult PlatformControl::begin(bool forceDummyBackend) {
  lastInitResult_ = {PLATFORM_BACKEND_NONE, false, false, false, false, -1, -1, -1};
  lastInitResult_.preferencesAvailable = loadServoOffsets();

  if (forceDummyBackend) {
    backend_ = PLATFORM_BACKEND_DUMMY;
    lastInitResult_.backend = backend_;
    detachAllServos();
    return lastInitResult_;
  }

  lastInitResult_.channelA = servoA_.attach(pins_.servoPinA, kServoPulseMinUs, kServoPulseMaxUs);
  lastInitResult_.channelB = servoB_.attach(pins_.servoPinB, kServoPulseMinUs, kServoPulseMaxUs);
  lastInitResult_.channelC = servoC_.attach(pins_.servoPinC, kServoPulseMinUs, kServoPulseMaxUs);
  lastInitResult_.attachSucceededA = lastInitResult_.channelA != INVALID_SERVO;
  lastInitResult_.attachSucceededB = lastInitResult_.channelB != INVALID_SERVO;
  lastInitResult_.attachSucceededC = lastInitResult_.channelC != INVALID_SERVO;

  if (!lastInitResult_.attachSucceededA || !lastInitResult_.attachSucceededB || !lastInitResult_.attachSucceededC) {
    detachAllServos();
    backend_ = PLATFORM_BACKEND_DUMMY;
  } else {
    backend_ = PLATFORM_BACKEND_REAL;
  }

  lastInitResult_.backend = backend_;
  return lastInitResult_;
}

void PlatformControl::movePlatform(float rollDeg, float pitchDeg, float height) {
  if (backend_ != PLATFORM_BACKEND_REAL) {
    return;
  }

  const float roll = rollDeg * kDegToRad;
  const float pitch = -pitchDeg * kDegToRad;  // Negate pitch to match physical Y-axis orientation.
  const float baseAngle[3] = {0.0f, 120.0f * kDegToRad, 240.0f * kDegToRad};
  const float platformPhaseOffset = kPlatformPhaseOffsetDeg * kDegToRad;
  float platformX[3];
  float platformY[3];
  float platformZ[3];
  float angles[3];

  for (int i = 0; i < 3; ++i) {
    const float base = baseAngle[i];
    const float platformAngle = base + platformPhaseOffset;
    const float px = kPlatformRadiusMm * cosf(platformAngle);
    const float py = kPlatformRadiusMm * sinf(platformAngle);
    const float pz = height;

    const float x1 = px * cosf(pitch) + pz * sinf(pitch);
    const float z1 = -px * sinf(pitch) + pz * cosf(pitch);
    const float y1 = py * cosf(roll) - z1 * sinf(roll);
    const float z2 = py * sinf(roll) + z1 * cosf(roll);

    platformX[i] = x1;
    platformY[i] = y1;
    platformZ[i] = z2;
  }

  for (int i = 0; i < 3; ++i) {
    const float base = baseAngle[i];
    const float bx = kBaseRadiusMm * cosf(base);
    const float by = kBaseRadiusMm * sinf(base);
    const float dx = platformX[i] - bx;
    const float dy = platformY[i] - by;
    const float dz = platformZ[i];
    const float dxLocal = dx * cosf(base) + dy * sinf(base);
    const float dyLocal = dz;
    const float distance = sqrtf(dxLocal * dxLocal + dyLocal * dyLocal);
    const float armCosine =
        constrain((kServoArmLengthMm * kServoArmLengthMm + distance * distance -
                   kPassiveLinkLengthMm * kPassiveLinkLengthMm) /
                      (2.0f * kServoArmLengthMm * distance),
                  -1.0f,
                  1.0f);
    const float theta = atan2f(dyLocal, dxLocal) - acosf(armCosine);
    angles[i] = theta * kRadToDeg;
  }

  moveServos(angles[2], angles[0], angles[1]);
}

void PlatformControl::moveServos(float a, float b, float c) {
  if (backend_ != PLATFORM_BACKEND_REAL) {
    return;
  }

  a = constrain(a, kServoMinAngleDeg, kServoMaxAngleDeg);
  b = constrain(b, kServoMinAngleDeg, kServoMaxAngleDeg);
  c = constrain(c, kServoMinAngleDeg, kServoMaxAngleDeg);

  servoA_.write(static_cast<float>(servoOffsetA_) - a);
  servoB_.write(static_cast<float>(servoOffsetB_) - b);
  servoC_.write(static_cast<float>(servoOffsetC_) - c);
}

void PlatformControl::setServoOffsets(int offsetA, int offsetB, int offsetC) {
  servoOffsetA_ = clampOffset(offsetA);
  servoOffsetB_ = clampOffset(offsetB);
  servoOffsetC_ = clampOffset(offsetC);
}

void PlatformControl::resetServoOffsets() {
  setServoOffsets(kServoOffsetDefault, kServoOffsetDefault, kServoOffsetDefault);
}

bool PlatformControl::loadServoOffsets() {
  if (!preferences_.begin(kPreferencesNamespace, true)) {
    resetServoOffsets();
    return false;
  }

  setServoOffsets(preferences_.getInt(kServoOffsetAKey, kServoOffsetDefault),
                  preferences_.getInt(kServoOffsetBKey, kServoOffsetDefault),
                  preferences_.getInt(kServoOffsetCKey, kServoOffsetDefault));
  preferences_.end();
  return true;
}

void PlatformControl::saveServoOffsets() const {
  if (!preferences_.begin(kPreferencesNamespace, false)) {
    return;
  }

  preferences_.putInt(kServoOffsetAKey, servoOffsetA_);
  preferences_.putInt(kServoOffsetBKey, servoOffsetB_);
  preferences_.putInt(kServoOffsetCKey, servoOffsetC_);
  preferences_.end();
}

int PlatformControl::servoOffsetA() const {
  return servoOffsetA_;
}

int PlatformControl::servoOffsetB() const {
  return servoOffsetB_;
}

int PlatformControl::servoOffsetC() const {
  return servoOffsetC_;
}

bool PlatformControl::motionAvailable() const {
  return backend_ == PLATFORM_BACKEND_REAL;
}

bool PlatformControl::dummyBackendActive() const {
  return backend_ == PLATFORM_BACKEND_DUMMY;
}

PlatformInitResult PlatformControl::lastInitResult() const {
  return lastInitResult_;
}

void PlatformControl::detachAllServos() {
  if (servoA_.attached()) {
    servoA_.detach();
  }
  if (servoB_.attached()) {
    servoB_.detach();
  }
  if (servoC_.attached()) {
    servoC_.detach();
  }
}

int PlatformControl::clampOffset(int value) const {
  return constrain(value, kServoOffsetMin, kServoOffsetMax);
}

}
