#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "BoardConfig.h"

namespace babot {

enum BaBotMode : uint8_t {
  ON,
  OFF,
  ASSEMBLY,
  CALIBRATION,
  TEST,
  INDIVIDUAL_TEST,
  POSITION_TEST
};

constexpr size_t kStatusTextCapacity = 96;

enum StartupPolicy : uint8_t {
  AUTO,
  BENCH,
  STRICT
};

enum SubsystemState : uint8_t {
  BOOTING,
  READY,
  DEGRADED,
  SUBSYSTEM_DISABLED,
  FAILED,
  UNVERIFIED
};

enum StartupPhase : uint8_t {
  STARTUP_SAFE_BOOT,
  STARTUP_NETWORK,
  STARTUP_ACTUATORS,
  STARTUP_DIGIPOT,
  STARTUP_SENSORS,
  STARTUP_READY
};

enum PidAutotuneStatus : uint8_t {
  PID_AUTOTUNE_IDLE,
  PID_AUTOTUNE_WAITING_FOR_BALL_ENTRY,
  PID_AUTOTUNE_TESTING_CANDIDATE,
  PID_AUTOTUNE_SCORING_TRIAL,
  PID_AUTOTUNE_CHOOSING_NEXT_CANDIDATE,
  PID_AUTOTUNE_EVOLVING,
  PID_AUTOTUNE_TUNING_I,
  PID_AUTOTUNE_APPLYING_BEST_GAINS,
  PID_AUTOTUNE_COMPLETE,
  PID_AUTOTUNE_FAILED
};

struct SubsystemStatus {
  SubsystemState state;
  char detail[kStatusTextCapacity];
};

struct SystemStatus {
  StartupPolicy policy;
  StartupPhase phase;
  bool degradedModeActive;
  char latestAlert[kStatusTextCapacity];
  SubsystemStatus storage;
  SubsystemStatus button;
  SubsystemStatus wifiAp;
  SubsystemStatus webUi;
  SubsystemStatus actuator;
  SubsystemStatus digipot;
  SubsystemStatus irSensors;
};

enum RuntimeFieldMask : uint32_t {
  kFieldNone = 0,
  kFieldPGain = 1u << 0,
  kFieldIGain = 1u << 1,
  kFieldDGain = 1u << 2,
  kFieldSetpointX = 1u << 3,
  kFieldSetpointY = 1u << 4,
  kFieldBallThreshold = 1u << 5,
  kFieldDigipotTap = 1u << 6,
};

struct RuntimeConfig {
  float pGain;
  float iGain;
  float dGain;
  float setpointX;
  float setpointY;
  float ballThreshold;
  uint8_t digipotTap;
};

struct TelemetryState {
  BaBotMode mode;
  bool ballDetected;
  float centerX;
  float centerY;
  float errorX;
  float errorY;
  float plateRoll;
  float platePitch;
  float rawPlateRoll;
  float rawPlatePitch;
  float smoothRoll;
  float smoothPitch;
  uint8_t digipotTap;
  bool muxCompensationReady;
  uint8_t selectedTestServo;
  int8_t positionTestMux;
  PidAutotuneStatus pidAutotuneStatus;
  uint8_t pidAutotuneProgress;
  float pidAutotuneBestP;
  float pidAutotuneBestI;
  float pidAutotuneBestD;
  float pidAutotuneScore;
  float pidAutotuneOvershoot;
  float pidAutotuneSettlingTimeMs;
  float pidAutotuneAverageError;
  float pidAutotuneBaselineScore;
  bool pidAutotuneHasBest;
  uint16_t pidAutotuneCandidate;
  uint8_t pidAutotuneTrial;
  uint8_t pidAutotuneCaptures;
  uint8_t pidAutotuneMisses;
  float pidAutotuneEntrySpeed;
  float pidAutotuneCaptureTimeMs;
  float pidAutotuneAverageRadius;
  float pidAutotuneNextP;
  float pidAutotuneNextI;
  float pidAutotuneNextD;
  float pidAutotuneSigma;
  uint8_t pidAutotuneGeneration;
  bool captureActive;
  float captureTimeMs;
  float captureVelocity;
  float captureRadiusTrend;
  char pidAutotuneLastFailure[kStatusTextCapacity];
  float sensorSignals[kSensorCount];
};

enum UiCommandType : uint8_t {
  UI_PATCH_CONFIG,
  UI_SET_MODE,
  UI_SAVE_RUNTIME,
  UI_LOAD_RUNTIME,
  UI_SAVE_CALIBRATION,
  UI_RESET_DEFAULTS,
  UI_COMPENSATE_MUX,
  UI_SET_TEST_SERVO,
  UI_START_PID_AUTOTUNE,
  UI_CANCEL_PID_AUTOTUNE,
  UI_RESTORE_PREVIOUS_PID
};

struct UiCommand {
  UiCommandType type;
  uint32_t fieldsMask;
  RuntimeConfig config;
  BaBotMode mode;
  uint8_t testServo;
};

RuntimeConfig defaultRuntimeConfig();
SystemStatus defaultSystemStatus();
const char *modeToString(BaBotMode mode);
bool modeFromString(const String &modeText, BaBotMode &mode);
const char *startupPolicyToString(StartupPolicy policy);
const char *subsystemStateToString(SubsystemState state);
const char *startupPhaseToString(StartupPhase phase);
const char *pidAutotuneStatusToString(PidAutotuneStatus status);
void setSubsystemStatus(SubsystemStatus &status, SubsystemState state, const char *detail);
void setSystemAlert(SystemStatus &status, const char *alert);

class SharedState {
 public:
  SharedState();
  ~SharedState();

  bool begin(const RuntimeConfig &initialConfig);
  bool enqueueCommand(const UiCommand &command);
  bool dequeueCommand(UiCommand &command);

  void publishTelemetry(const TelemetryState &telemetry, const RuntimeConfig &config);
  void publishSystemStatus(const SystemStatus &status);
  void snapshot(TelemetryState &telemetry, RuntimeConfig &config) const;
  void snapshotSystem(SystemStatus &status) const;

  RuntimeConfig loadRuntimeConfig() const;
  bool loadRuntimeConfig(RuntimeConfig &config) const;
  bool saveRuntimeConfig(const RuntimeConfig &config) const;
  RuntimeConfig resetRuntimeConfig() const;

 private:
  RuntimeConfig readPersistedRuntimeConfig(Preferences &preferences) const;
  void lock() const;
  void unlock() const;

  QueueHandle_t queue_;
  SemaphoreHandle_t mutex_;
  RuntimeConfig runtimeConfig_;
  TelemetryState telemetry_;
  SystemStatus systemStatus_;
};

}
