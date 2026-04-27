#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "third_party/CD74HC4067/src/CD74HC4067.h"

#include "BoardConfig.h"

namespace babot {

class IRSensorArray {
 public:
  explicit IRSensorArray(const BoardPins &pins);

  void begin();
  // Blocking single measurement — used during startup health checks and calibration.
  void measure();
  // Start the FreeRTOS sensor task. Must be called after begin().
  // The task runs measure() in a loop and signals measurementReady after each cycle.
  void startTask();

  // Binary semaphore: given once per completed measurement cycle by the sensor task.
  // The control loop takes this semaphore (non-blocking) before running PID.
  SemaphoreHandle_t measurementReady = nullptr;

  bool loadMuxCompensation();
  bool saveMuxCompensation();
  void resetMuxCompensation();
  bool calibrateMuxCompensation(size_t samples);
  bool muxCompensationReady() const;
  void injectSyntheticBall(float x, float y);
  void injectSyntheticMuxChannel(size_t channel);
  bool ballOnPlate(float threshold) const;
  void calculateWeightedCenter(float &x, float &y, float threshold) const;
  void copySignals(float *destination, size_t count) const;
  void evaluateStartupHealth(bool &flat, bool &saturated, float &range, float &maxSignal) const;

 private:
  static void taskEntryPoint(void *parameter);
  void taskLoop();
  void readMuxBank(CD74HC4067 &mux, int signalPin, int *destination);
  void processDeltas();
  float minSignal() const;
  float maxSignal() const;

  const BoardPins &pins_;
  CD74HC4067 mux_;
  TaskHandle_t taskHandle_ = nullptr;
  int ambientLight_[kSensorCount];
  int irLight_[kSensorCount];
  float irSignal_[kSensorCount];
  float muxCompensation_[kSensorCount];
  bool muxCompensationReady_;
};

}
