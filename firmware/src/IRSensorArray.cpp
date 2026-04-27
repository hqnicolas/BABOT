#include "IRSensorArray.h"

#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace babot {

namespace {

constexpr char kMuxCompensationNamespace[] = "babot_mux";
constexpr char kMuxCompensationReadyKey[] = "ready";
constexpr char kMuxCompensationKeyPrefix[] = "c";

}

IRSensorArray::IRSensorArray(const BoardPins &pins)
    : pins_(pins),
      mux_(pins.mux.s0, pins.mux.s1, pins.mux.s2, pins.mux.s3),
      ambientLight_{0},
      irLight_{0},
      irSignal_{0.0f},
      muxCompensation_{0.0f},
      muxCompensationReady_(false) {
  measurementReady = xSemaphoreCreateBinary();
}

void IRSensorArray::begin() {
  pinMode(pins_.irLedPin, OUTPUT);
  digitalWrite(pins_.irLedPin, HIGH);

#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
#endif
}

void IRSensorArray::measure() {
  // Ambient phase: LED OFF, settle, then read.
  digitalWrite(pins_.irLedPin, HIGH);
  vTaskDelay(pdMS_TO_TICKS(kAmbientSettleDelayMs));
  readMuxBank(mux_, pins_.mux.sig, ambientLight_);

  // IR phase: LED ON, settle, then read.
  digitalWrite(pins_.irLedPin, LOW);
  vTaskDelay(pdMS_TO_TICKS(kIrSettleDelayMs));
  readMuxBank(mux_, pins_.mux.sig, irLight_);

  // Turn LED off and compute final signal.
  digitalWrite(pins_.irLedPin, HIGH);

  for (size_t i = 0; i < kSensorCount; ++i) {
    const float delta = static_cast<float>(irLight_[i] - ambientLight_[i]) + muxCompensation_[i];
    irSignal_[i] = kIrAlpha * delta + (1.0f - kIrAlpha) * irSignal_[i];
  }
}

void IRSensorArray::startTask() {
  if (measurementReady == nullptr) {
    return;
  }
  xTaskCreatePinnedToCore(
      taskEntryPoint,
      "babot-sensor",
      babot::kSensorTaskStackSize,
      this,
      babot::kSensorTaskPriority,
      &taskHandle_,
      babot::kSensorTaskCore);
}

void IRSensorArray::taskEntryPoint(void *parameter) {
  static_cast<IRSensorArray *>(parameter)->taskLoop();
}

void IRSensorArray::taskLoop() {
  for (;;) {
    measure();
    xSemaphoreGive(measurementReady);
  }
}

bool IRSensorArray::loadMuxCompensation() {
  Preferences preferences;
  if (!preferences.begin(kMuxCompensationNamespace, true)) {
    resetMuxCompensation();
    return false;
  }

  const bool ready = preferences.getBool(kMuxCompensationReadyKey, false);
  if (!ready) {
    preferences.end();
    resetMuxCompensation();
    return false;
  }

  for (size_t i = 0; i < kSensorCount; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "%s%02u", kMuxCompensationKeyPrefix, static_cast<unsigned>(i));
    muxCompensation_[i] = preferences.getFloat(key, 0.0f);
  }
  preferences.end();
  muxCompensationReady_ = true;
  return true;
}

bool IRSensorArray::saveMuxCompensation() {
  Preferences preferences;
  if (!preferences.begin(kMuxCompensationNamespace, false)) {
    return false;
  }

  for (size_t i = 0; i < kSensorCount; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "%s%02u", kMuxCompensationKeyPrefix, static_cast<unsigned>(i));
    preferences.putFloat(key, muxCompensation_[i]);
  }
  preferences.putBool(kMuxCompensationReadyKey, muxCompensationReady_);
  preferences.end();
  return true;
}

void IRSensorArray::resetMuxCompensation() {
  for (size_t i = 0; i < kSensorCount; ++i) {
    muxCompensation_[i] = 0.0f;
  }
  muxCompensationReady_ = false;
}

bool IRSensorArray::calibrateMuxCompensation(size_t samples) {
  if (samples == 0) {
    return false;
  }

  // Suspend the background measurement task to get exclusive control of the hardware.
  if (taskHandle_ != nullptr) {
    vTaskSuspend(taskHandle_);
  }

  float sums[kSensorCount] = {0.0f};
  resetMuxCompensation();

  for (size_t sample = 0; sample < samples; ++sample) {
    // Manual measurement sweep (must match measure() logic)
    digitalWrite(pins_.irLedPin, HIGH);
    delay(kAmbientSettleDelayMs);
    readMuxBank(mux_, pins_.mux.sig, ambientLight_);

    digitalWrite(pins_.irLedPin, LOW);
    delay(kIrSettleDelayMs);
    readMuxBank(mux_, pins_.mux.sig, irLight_);
    digitalWrite(pins_.irLedPin, HIGH);

    for (size_t i = 0; i < kSensorCount; ++i) {
      sums[i] += static_cast<float>(irLight_[i] - ambientLight_[i]);
    }
  }

  float mean = 0.0f;
  for (size_t i = 0; i < kSensorCount; ++i) {
    mean += sums[i] / static_cast<float>(samples);
  }
  mean /= static_cast<float>(kSensorCount);

  for (size_t i = 0; i < kSensorCount; ++i) {
    const float channelMean = sums[i] / static_cast<float>(samples);
    muxCompensation_[i] = constrain(mean - channelMean,
                                    -kMuxCompensationMaxOffset,
                                    kMuxCompensationMaxOffset);
    irSignal_[i] = channelMean + muxCompensation_[i];
  }

  muxCompensationReady_ = true;
  const bool saved = saveMuxCompensation();

  // Resume background measurement task.
  if (taskHandle_ != nullptr) {
    vTaskResume(taskHandle_);
  }

  return saved;
}

bool IRSensorArray::muxCompensationReady() const {
  return muxCompensationReady_;
}

void IRSensorArray::injectSyntheticBall(float x, float y) {
  const float gridX = constrain((x / kSensorGridScale) + 1.5f, 0.0f, 3.0f);
  const float gridY = constrain((y / kSensorGridScale) + 1.5f, 0.0f, 3.0f);
  constexpr float kSyntheticPeak = 800.0f;
  constexpr float kSyntheticSigma = 0.85f;
  constexpr float kSyntheticBackground = 800.0f;
  const float twoSigmaSquared = 2.0f * kSyntheticSigma * kSyntheticSigma;

  for (size_t row = 0; row < 4; ++row) {
    for (size_t col = 0; col < 4; ++col) {
      const float dx = static_cast<float>(col) - gridX;
      const float dy = static_cast<float>(row) - gridY;
      const float distanceSquared = dx * dx + dy * dy;
      const size_t index = row * 4 + col;
      irSignal_[index] = kSyntheticBackground - kSyntheticPeak * expf(-distanceSquared / twoSigmaSquared);
    }
  }
}

void IRSensorArray::injectSyntheticMuxChannel(size_t channel) {
  constexpr float kSyntheticPeak = 800.0f;
  const size_t active = min(channel, kSensorCount - 1);
  for (size_t i = 0; i < kSensorCount; ++i) {
    irSignal_[i] = (i == active) ? 0.0f : kSyntheticPeak;
  }
}

bool IRSensorArray::ballOnPlate(float threshold) const {
  return (maxSignal() - minSignal()) >= threshold;
}

void IRSensorArray::calculateWeightedCenter(float &x, float &y, float threshold) const {
  if (!ballOnPlate(threshold)) {
    x = 0.0f;
    y = 0.0f;
    return;
  }

  const float minV = minSignal();
  const float maxV = maxSignal();
  const float range = maxV - minV;

  if (range <= 0.0f) {
    x = 0.0f;
    y = 0.0f;
    return;
  }

  static constexpr int kMirrorIndex[6] = {0, 0, 1, 2, 3, 3};
  float wx = 0.0f;
  float wy = 0.0f;
  float sumW = 0.0f;

  for (int iy = -1; iy <= 4; ++iy) {
    const int mirroredY = kMirrorIndex[iy + 1];
    for (int ix = -1; ix <= 4; ++ix) {
      const int mirroredX = kMirrorIndex[ix + 1];
      float normalized = (maxV - irSignal_[mirroredY * 4 + mirroredX]) / range;
      if (normalized < 0.0f) {
        normalized = 0.0f;
      }

      const float weight = normalized * normalized * normalized;
      wx += static_cast<float>(ix) * weight;
      wy += static_cast<float>(iy) * weight;
      sumW += weight;
    }
  }

  if (sumW <= 0.0f) {
    x = 0.0f;
    y = 0.0f;
    return;
  }

  x = (wx / sumW - 1.5f) * kSensorGridScale;
  y = (wy / sumW - 1.5f) * kSensorGridScale;
}

void IRSensorArray::copySignals(float *destination, size_t count) const {
  if (destination == nullptr) {
    return;
  }

  const size_t limit = min(count, kSensorCount);
  for (size_t i = 0; i < limit; ++i) {
    destination[i] = irSignal_[i];
  }
  for (size_t i = limit; i < count; ++i) {
    destination[i] = 0.0f;
  }
}

void IRSensorArray::evaluateStartupHealth(bool &flat, bool &saturated, float &range, float &peakSignal) const {
  const float minV = minSignal();
  peakSignal = this->maxSignal();
  range = peakSignal - minV;
  flat = range <= kIrStartupFlatRangeThreshold;
  saturated = peakSignal >= kIrStartupSaturationThreshold;
}

void IRSensorArray::readMuxBank(CD74HC4067 &mux, int signalPin, int *destination) {
  for (size_t channel = 0; channel < kSensorsPerMux; ++channel) {
    mux.channel(static_cast<uint8_t>(channel));
    delayMicroseconds(kMuxSettleDelayUs);
    int sum = analogRead(signalPin);
    for (size_t s = 1; s < kMuxMultiSampleCount; ++s) {
      delayMicroseconds(kMuxMultiSampleDelayUs);
      sum += analogRead(signalPin);
    }
    destination[channel] = sum / static_cast<int>(kMuxMultiSampleCount);
  }
}

float IRSensorArray::minSignal() const {
  float minV = irSignal_[0];
  for (size_t i = 1; i < kSensorCount; ++i) {
    minV = min(minV, irSignal_[i]);
  }
  return minV;
}

float IRSensorArray::maxSignal() const {
  float maxV = irSignal_[0];
  for (size_t i = 1; i < kSensorCount; ++i) {
    maxV = max(maxV, irSignal_[i]);
  }
  return maxV;
}

}
