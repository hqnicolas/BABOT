#include "RuntimeState.h"

#include <cstring>

namespace babot {

namespace {

constexpr char kRuntimeNamespace[] = "babot_ui";
constexpr char kPGainKey[] = "p_gain";
constexpr char kIGainKey[] = "i_gain";
constexpr char kDGainKey[] = "d_gain";
constexpr char kSetpointXKey[] = "set_x";
constexpr char kSetpointYKey[] = "set_y";
constexpr char kBallThresholdKey[] = "ball_thr";
constexpr char kDigipotTapKey[] = "digipot";

void copyStatusText(char *destination, size_t capacity, const char *source) {
  if (destination == nullptr || capacity == 0) {
    return;
  }

  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }

  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

}

RuntimeConfig defaultRuntimeConfig() {
  return RuntimeConfig{
      kProportionalGain,
      kIntegralGain,
      kDerivativeGain,
      0.0f,
      0.0f,
      kBallThreshold,
      kDefaultDigipotTap,
  };
}

SystemStatus defaultSystemStatus() {
  SystemStatus status{};
  status.policy = AUTO;
  status.phase = STARTUP_SAFE_BOOT;
  status.degradedModeActive = false;
  setSystemAlert(status, "");
  setSubsystemStatus(status.storage, BOOTING, "Storage not checked yet");
  setSubsystemStatus(status.button, BOOTING, "Button not checked yet");
  setSubsystemStatus(status.wifiAp, BOOTING, "Wi-Fi AP not started yet");
  setSubsystemStatus(status.webUi, BOOTING, "Web UI not started yet");
  setSubsystemStatus(status.actuator, BOOTING, "Actuator startup pending");
  setSubsystemStatus(status.digipot, BOOTING, "Digipot startup pending");
  setSubsystemStatus(status.irSensors, BOOTING, "IR sensor startup pending");
  return status;
}

const char *modeToString(BaBotMode mode) {
  switch (mode) {
    case ON:
      return "ON";
    case OFF:
      return "OFF";
    case ASSEMBLY:
      return "ASSEMBLY";
    case CALIBRATION:
      return "CALIBRATION";
    case TEST:
      return "TEST";
    case INDIVIDUAL_TEST:
      return "INDIVIDUAL_TEST";
    case POSITION_TEST:
      return "POSITION_TEST";
    default:
      return "UNKNOWN";
  }
}

bool modeFromString(const String &modeText, BaBotMode &mode) {
  String normalized = modeText;
  normalized.trim();
  normalized.toUpperCase();

  if (normalized == "ON") {
    mode = ON;
    return true;
  }
  if (normalized == "OFF") {
    mode = OFF;
    return true;
  }
  if (normalized == "ASSEMBLY") {
    mode = ASSEMBLY;
    return true;
  }
  if (normalized == "CALIBRATION") {
    mode = CALIBRATION;
    return true;
  }
  if (normalized == "TEST") {
    mode = TEST;
    return true;
  }
  if (normalized == "INDIVIDUAL_TEST") {
    mode = INDIVIDUAL_TEST;
    return true;
  }
  if (normalized == "POSITION_TEST") {
    mode = POSITION_TEST;
    return true;
  }

  return false;
}

const char *startupPolicyToString(StartupPolicy policy) {
  switch (policy) {
    case AUTO:
      return "AUTO";
    case BENCH:
      return "BENCH";
    case STRICT:
      return "STRICT";
    default:
      return "UNKNOWN";
  }
}

const char *subsystemStateToString(SubsystemState state) {
  switch (state) {
    case BOOTING:
      return "BOOTING";
    case READY:
      return "READY";
    case DEGRADED:
      return "DEGRADED";
    case SUBSYSTEM_DISABLED:
      return "DISABLED";
    case FAILED:
      return "FAILED";
    case UNVERIFIED:
      return "UNVERIFIED";
    default:
      return "UNKNOWN";
  }
}

const char *startupPhaseToString(StartupPhase phase) {
  switch (phase) {
    case STARTUP_SAFE_BOOT:
      return "SAFE_BOOT";
    case STARTUP_NETWORK:
      return "NETWORK";
    case STARTUP_ACTUATORS:
      return "ACTUATORS";
    case STARTUP_DIGIPOT:
      return "DIGIPOT";
    case STARTUP_SENSORS:
      return "SENSORS";
    case STARTUP_READY:
      return "READY";
    default:
      return "UNKNOWN";
  }
}

const char *pidAutotuneStatusToString(PidAutotuneStatus status) {
  switch (status) {
    case PID_AUTOTUNE_IDLE:
      return "Idle";
    case PID_AUTOTUNE_WAITING_FOR_BALL_ENTRY:
      return "Waiting for ball entry";
    case PID_AUTOTUNE_TESTING_CANDIDATE:
      return "Testing candidate";
    case PID_AUTOTUNE_SCORING_TRIAL:
      return "Scoring trial";
    case PID_AUTOTUNE_CHOOSING_NEXT_CANDIDATE:
      return "Choosing next candidate";
    case PID_AUTOTUNE_EVOLVING:
      return "Evolving (CMA-ES)";
    case PID_AUTOTUNE_TUNING_I:
      return "Tuning I";
    case PID_AUTOTUNE_APPLYING_BEST_GAINS:
      return "Applying best gains";
    case PID_AUTOTUNE_COMPLETE:
      return "Complete";
    case PID_AUTOTUNE_FAILED:
      return "Failed";
    default:
      return "Unknown";
  }
}

void setSubsystemStatus(SubsystemStatus &status, SubsystemState state, const char *detail) {
  status.state = state;
  copyStatusText(status.detail, sizeof(status.detail), detail);
}

void setSystemAlert(SystemStatus &status, const char *alert) {
  copyStatusText(status.latestAlert, sizeof(status.latestAlert), alert);
}

SharedState::SharedState()
    : queue_(nullptr),
      mutex_(nullptr),
      runtimeConfig_(defaultRuntimeConfig()),
      telemetry_{},
      systemStatus_(defaultSystemStatus()) {}

SharedState::~SharedState() {
  if (queue_ != nullptr) {
    vQueueDelete(queue_);
  }
  if (mutex_ != nullptr) {
    vSemaphoreDelete(mutex_);
  }
}

bool SharedState::begin(const RuntimeConfig &initialConfig) {
  if (queue_ == nullptr) {
    queue_ = xQueueCreate(kUiCommandQueueLength, sizeof(UiCommand));
  }
  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateMutex();
  }

  if (queue_ == nullptr || mutex_ == nullptr) {
    return false;
  }

  runtimeConfig_ = initialConfig;
  return true;
}

bool SharedState::enqueueCommand(const UiCommand &command) {
  if (queue_ == nullptr) {
    return false;
  }
  return xQueueSend(queue_, &command, 0) == pdTRUE;
}

bool SharedState::dequeueCommand(UiCommand &command) {
  if (queue_ == nullptr) {
    return false;
  }
  return xQueueReceive(queue_, &command, 0) == pdTRUE;
}

void SharedState::publishTelemetry(const TelemetryState &telemetry, const RuntimeConfig &config) {
  if (mutex_ == nullptr) {
    telemetry_ = telemetry;
    runtimeConfig_ = config;
    return;
  }

  lock();
  telemetry_ = telemetry;
  runtimeConfig_ = config;
  unlock();
}

void SharedState::publishSystemStatus(const SystemStatus &status) {
  if (mutex_ == nullptr) {
    systemStatus_ = status;
    return;
  }

  lock();
  systemStatus_ = status;
  unlock();
}

void SharedState::snapshot(TelemetryState &telemetry, RuntimeConfig &config) const {
  if (mutex_ == nullptr) {
    telemetry = telemetry_;
    config = runtimeConfig_;
    return;
  }

  lock();
  telemetry = telemetry_;
  config = runtimeConfig_;
  unlock();
}

void SharedState::snapshotSystem(SystemStatus &status) const {
  if (mutex_ == nullptr) {
    status = systemStatus_;
    return;
  }

  lock();
  status = systemStatus_;
  unlock();
}

RuntimeConfig SharedState::loadRuntimeConfig() const {
  RuntimeConfig config{};
  loadRuntimeConfig(config);
  return config;
}

bool SharedState::loadRuntimeConfig(RuntimeConfig &config) const {
  Preferences preferences;
  if (!preferences.begin(kRuntimeNamespace, true)) {
    config = defaultRuntimeConfig();
    return false;
  }

  config = readPersistedRuntimeConfig(preferences);
  preferences.end();
  return true;
}

bool SharedState::saveRuntimeConfig(const RuntimeConfig &config) const {
  Preferences preferences;
  if (!preferences.begin(kRuntimeNamespace, false)) {
    return false;
  }

  preferences.putFloat(kPGainKey, config.pGain);
  preferences.putFloat(kIGainKey, config.iGain);
  preferences.putFloat(kDGainKey, config.dGain);
  preferences.putFloat(kSetpointXKey, config.setpointX);
  preferences.putFloat(kSetpointYKey, config.setpointY);
  preferences.putFloat(kBallThresholdKey, config.ballThreshold);
  preferences.putUChar(kDigipotTapKey, config.digipotTap);
  preferences.end();
  return true;
}

RuntimeConfig SharedState::resetRuntimeConfig() const {
  Preferences preferences;
  if (preferences.begin(kRuntimeNamespace, false)) {
    preferences.clear();
    preferences.end();
  }
  return defaultRuntimeConfig();
}

RuntimeConfig SharedState::readPersistedRuntimeConfig(Preferences &preferences) const {
  RuntimeConfig config = defaultRuntimeConfig();
  config.pGain = preferences.getFloat(kPGainKey, config.pGain);
  config.iGain = preferences.getFloat(kIGainKey, config.iGain);
  config.dGain = preferences.getFloat(kDGainKey, config.dGain);
  config.setpointX = preferences.getFloat(kSetpointXKey, config.setpointX);
  config.setpointY = preferences.getFloat(kSetpointYKey, config.setpointY);
  config.ballThreshold = preferences.getFloat(kBallThresholdKey, config.ballThreshold);
  config.digipotTap = preferences.getUChar(kDigipotTapKey, config.digipotTap);
  return config;
}

void SharedState::lock() const {
  xSemaphoreTake(mutex_, portMAX_DELAY);
}

void SharedState::unlock() const {
  xSemaphoreGive(mutex_);
}

}
