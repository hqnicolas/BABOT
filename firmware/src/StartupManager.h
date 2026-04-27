#pragma once

#include <Preferences.h>

bool isFirstBootAfterFlash() {
  Preferences preferences;
  if (!preferences.begin("babot_boot", true)) {
    return true;  // Can't read NVS → treat as first boot
  }
  
  String savedVersion = preferences.getString("fw_version", "");
  preferences.end();
  
  // Automatically grab compiler timestamp
  String currentVersion = String(__DATE__) + " " + String(__TIME__);
  
  // If timestamps differ, we have flashed a new build!
  return savedVersion != currentVersion;
}

void performFactoryReset() {
  // Wipe all memory sections
  const char *namespaces[] = {"babot_boot", "babot_ui", "babot_mux", "babot"};
  for (const char *ns : namespaces) {
    Preferences preferences;
    if (preferences.begin(ns, false)) {
      preferences.clear();
      preferences.end();
    }
  }

  // Save the newest timestamp
  Preferences preferences;
  if (preferences.begin("babot_boot", false)) {
    String currentVersion = String(__DATE__) + " " + String(__TIME__);
    preferences.putString("fw_version", currentVersion);
    preferences.end();
  }
}

void bootPrint(const char *message) {
  kDebugPort.println(message);
  kDebugPort.flush();
}

void bootPrintf(const char *format, ...) {
  char buffer[160];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  bootPrint(buffer);
}

void bootPulseColor(uint32_t color, unsigned long onMs = 80, unsigned long offMs = 40) {
}

void bootCheckpoint(const char *label, uint32_t color) {
  bootPrint(label);
  bootPulseColor(color);
}

const char *startupRiskStageToString(StartupRiskStage stage) {
  switch (stage) {
    case STARTUP_RISK_NONE:
      return "NONE";
    case STARTUP_RISK_PLATFORM_BEGIN:
      return "PLATFORM_BEGIN";
    case STARTUP_RISK_DIGIPOT_RESTORE:
      return "DIGIPOT_RESTORE";
    default:
      return "UNKNOWN";
  }
}

bool subsystemAffectsDegradedMode(const babot::SubsystemStatus &status) {
  return status.state == babot::DEGRADED ||
         status.state == babot::FAILED ||
         status.state == babot::UNVERIFIED;
}

void refreshDegradedModeFlag() {
  gSystemStatus.degradedModeActive =
      gSystemStatus.policy != babot::AUTO ||
      subsystemAffectsDegradedMode(gSystemStatus.storage) ||
      subsystemAffectsDegradedMode(gSystemStatus.wifiAp) ||
      subsystemAffectsDegradedMode(gSystemStatus.webUi) ||
      subsystemAffectsDegradedMode(gSystemStatus.actuator) ||
      subsystemAffectsDegradedMode(gSystemStatus.digipot) ||
      subsystemAffectsDegradedMode(gSystemStatus.irSensors);
}

void publishSystemStatusNow() {
  refreshDegradedModeFlag();
  gSharedState.publishSystemStatus(gSystemStatus);
}

void publishAllState() {
  publishSharedState();
  publishSystemStatusNow();
}

void setSystemAlertWithLog(const char *alert) {
  const char *safeAlert = (alert != nullptr) ? alert : "";
  if (strcmp(gSystemStatus.latestAlert, safeAlert) == 0) {
    return;
  }

  babot::setSystemAlert(gSystemStatus, safeAlert);
  if (safeAlert[0] != '\0') {
    bootPrintf("ALERT: %s", safeAlert);
    gDebugLogger.tryLogf("Alert: %s", safeAlert);
  }
  publishSystemStatusNow();
}

void setStartupPhaseWithLog(babot::StartupPhase phase, const char *detail) {
  gSystemStatus.phase = phase;
  publishSystemStatusNow();
  if (detail != nullptr && detail[0] != '\0') {
    bootPrintf("STARTUP %s: %s", babot::startupPhaseToString(phase), detail);
    gDebugLogger.tryLogf("Startup %s: %s",
                         babot::startupPhaseToString(phase),
                         detail);
  }
}

void updateSubsystemStatus(const char *label,
                           babot::SubsystemStatus &status,
                           babot::SubsystemState state,
                           const char *detail,
                           bool raiseAlert = false) {
  const char *safeDetail = (detail != nullptr) ? detail : "";
  const bool changed = status.state != state || strcmp(status.detail, safeDetail) != 0;
  babot::setSubsystemStatus(status, state, safeDetail);
  publishSystemStatusNow();

  if (changed) {
    bootPrintf("HEALTH %s: %s - %s",
               label,
               babot::subsystemStateToString(state),
               safeDetail);
    gDebugLogger.tryLogf("Health %s: %s - %s",
                         label,
                         babot::subsystemStateToString(state),
                         safeDetail);
  }

  if (raiseAlert && safeDetail[0] != '\0') {
    setSystemAlertWithLog(safeDetail);
  }
}

StartupRiskStage readPendingRiskStage() {
  Preferences preferences;
  if (!preferences.begin("babot_boot", true)) {
    return STARTUP_RISK_NONE;
  }

  const uint8_t stored = preferences.getUChar("risk", static_cast<uint8_t>(STARTUP_RISK_NONE));
  preferences.end();
  if (stored > static_cast<uint8_t>(STARTUP_RISK_DIGIPOT_RESTORE)) {
    return STARTUP_RISK_NONE;
  }
  return static_cast<StartupRiskStage>(stored);
}

void writePendingRiskStage(StartupRiskStage stage) {
  Preferences preferences;
  if (!preferences.begin("babot_boot", false)) {
    return;
  }

  preferences.putUChar("risk", static_cast<uint8_t>(stage));
  preferences.end();
}

void clearPendingRiskStage() {
  writePendingRiskStage(STARTUP_RISK_NONE);
}

bool modeRequiresActuator(babot::BaBotMode mode) {
  return mode == babot::ASSEMBLY || mode == babot::CALIBRATION || mode == babot::TEST ||
         mode == babot::INDIVIDUAL_TEST || mode == babot::POSITION_TEST;
}

bool ensureModeHardwareAvailable(babot::BaBotMode mode) {
  if (!modeRequiresActuator(mode) || gPlatform.motionAvailable()) {
    return true;
  }

  char detail[128];
  snprintf(detail,
           sizeof(detail),
           "%s mode requires servo output; dummy backend is active.",
           babot::modeToString(mode));
  setSystemAlertWithLog(detail);
  gDebugLogger.tryLogf("Rejected mode transition to %s because actuator output is unavailable.",
                       babot::modeToString(mode));
  return false;
}

void applyRecoveredBootPolicy() {
  if (gRecoveredRiskStage == STARTUP_RISK_PLATFORM_BEGIN) {
    gSystemStatus.policy = babot::BENCH;
    setSystemAlertWithLog("Recovered from a reset during actuator startup; dummy backend will be used this boot.");
    gDebugLogger.tryLogf("Recovered pending startup stage: %s",
                         startupRiskStageToString(gRecoveredRiskStage));
  } else if (gRecoveredRiskStage == STARTUP_RISK_DIGIPOT_RESTORE) {
    setSystemAlertWithLog("Recovered from a reset during digipot restore; digipot output will stay disabled this boot.");
    gDebugLogger.tryLogf("Recovered pending startup stage: %s",
                         startupRiskStageToString(gRecoveredRiskStage));
  }
}

void runPlatformStartupStep() {
  setStartupPhaseWithLog(babot::STARTUP_ACTUATORS, "Initializing actuator backend");

  const bool recoveredReset = gRecoveredRiskStage == STARTUP_RISK_PLATFORM_BEGIN;
  const bool forceDummyBackend = recoveredReset || gSystemStatus.policy == babot::BENCH;
  babot::PlatformInitResult result{};

  if (forceDummyBackend) {
    result = gPlatform.begin(true);
  } else {
    writePendingRiskStage(STARTUP_RISK_PLATFORM_BEGIN);
    result = gPlatform.begin(false);
    clearPendingRiskStage();
  }

  gCalibrationOffsetA = gPlatform.servoOffsetA();
  gCalibrationOffsetB = gPlatform.servoOffsetB();
  gCalibrationOffsetC = gPlatform.servoOffsetC();

  char detail[160];
  if (recoveredReset) {
    snprintf(detail,
             sizeof(detail),
             "Skipped after previous reset during actuator startup; dummy backend active.");
    updateSubsystemStatus("actuator", gSystemStatus.actuator, babot::DEGRADED, detail, true);
  } else if (forceDummyBackend) {
    snprintf(detail,
             sizeof(detail),
             "Bench policy active; servo output disabled and dummy backend active.");
    updateSubsystemStatus("actuator", gSystemStatus.actuator, babot::SUBSYSTEM_DISABLED, detail, true);
  } else if (result.backend == babot::PLATFORM_BACKEND_REAL) {
    snprintf(detail,
             sizeof(detail),
             "Servo output ready on channels %d/%d/%d%s",
             result.channelA,
             result.channelB,
             result.channelC,
             result.preferencesAvailable ? "" : " with default offsets");
    updateSubsystemStatus("actuator", gSystemStatus.actuator, babot::READY, detail);
  } else {
    gSystemStatus.policy = babot::BENCH;
    snprintf(detail,
             sizeof(detail),
             "Servo attach incomplete A=%d B=%d C=%d; dummy backend active.",
             result.channelA,
             result.channelB,
             result.channelC);
    updateSubsystemStatus("actuator", gSystemStatus.actuator, babot::DEGRADED, detail, true);
  }

  gStartupStep = STARTUP_STEP_DIGIPOT;
  publishAllState();
}

void runDigipotStartupStep() {
  setStartupPhaseWithLog(babot::STARTUP_DIGIPOT, "Initializing digipot output");

  const bool recoveredReset = gRecoveredRiskStage == STARTUP_RISK_DIGIPOT_RESTORE;
  const bool enableOutput = !recoveredReset && gSystemStatus.policy != babot::BENCH;
  gDigipot.begin(enableOutput);

  char detail[144];
  if (recoveredReset) {
    snprintf(detail,
             sizeof(detail),
             "Skipped after previous reset during digipot restore; dummy tap tracking active.");
    updateSubsystemStatus("digipot", gSystemStatus.digipot, babot::DEGRADED, detail, true);
  } else if (!enableOutput) {
    snprintf(detail,
             sizeof(detail),
             "Bench policy active; digipot output disabled, keeping software tap value only.");
    updateSubsystemStatus("digipot", gSystemStatus.digipot, babot::SUBSYSTEM_DISABLED, detail);
  } else {
    writePendingRiskStage(STARTUP_RISK_DIGIPOT_RESTORE);
    gDigipot.setTap(gConfig.digipotTap);
    clearPendingRiskStage();
    snprintf(detail,
             sizeof(detail),
             "Digipot output ready at tap %u.",
             static_cast<unsigned>(gDigipot.currentTap()));
    updateSubsystemStatus("digipot", gSystemStatus.digipot, babot::READY, detail);
  }

  gStartupStep = STARTUP_STEP_SENSORS;
  publishAllState();
}

void runSensorStartupStep() {
  setStartupPhaseWithLog(babot::STARTUP_SENSORS, "Initializing IR sensor array");
  gIrSensors.begin();
  const bool compensationLoaded = gIrSensors.loadMuxCompensation();
  gIrSensors.measure();

  bool flat = false;
  bool saturated = false;
  float range = 0.0f;
  float peakSignal = 0.0f;
  gIrSensors.evaluateStartupHealth(flat, saturated, range, peakSignal);

  char detail[160];
  if (flat && saturated) {
    snprintf(detail,
             sizeof(detail),
             "IR readings look flat and saturated (range=%.1f peak=%.1f).",
             range,
             peakSignal);
    updateSubsystemStatus("irSensors", gSystemStatus.irSensors, babot::DEGRADED, detail, true);
  } else if (saturated) {
    snprintf(detail,
             sizeof(detail),
             "IR readings look saturated (range=%.1f peak=%.1f).",
             range,
             peakSignal);
    updateSubsystemStatus("irSensors", gSystemStatus.irSensors, babot::DEGRADED, detail, true);
  } else if (flat) {
    snprintf(detail,
             sizeof(detail),
             "IR readings are flat or floating (range=%.1f peak=%.1f).",
             range,
             peakSignal);
    updateSubsystemStatus("irSensors", gSystemStatus.irSensors, babot::UNVERIFIED, detail, true);
  } else {
    snprintf(detail,
             sizeof(detail),
             compensationLoaded ? "IR sensor array ready with MUX compensation (range=%.1f peak=%.1f)."
                                : "IR sensor array ready without MUX compensation (range=%.1f peak=%.1f).",
             range,
             peakSignal);
    updateSubsystemStatus("irSensors", gSystemStatus.irSensors, babot::READY, detail);
  }

  gBallDetected = gIrSensors.ballOnPlate(gConfig.ballThreshold);
  gLastLoggedBallDetected = gBallDetected;
  gIrSensors.calculateWeightedCenter(gCenterX, gCenterY, gConfig.ballThreshold);
  gStartupStep = STARTUP_STEP_COMPLETE;
  publishAllState();
}

void advanceStartupSequence() {
  if (gStartupComplete) {
    return;
  }

  switch (gStartupStep) {
    case STARTUP_STEP_PLATFORM:
      runPlatformStartupStep();
      break;
    case STARTUP_STEP_DIGIPOT:
      runDigipotStartupStep();
      break;
    case STARTUP_STEP_SENSORS:
      runSensorStartupStep();
      break;
    case STARTUP_STEP_COMPLETE:
      gStartupComplete = true;
      gSystemStatus.phase = babot::STARTUP_READY;
      if (!gSystemStatus.degradedModeActive) {
        babot::setSystemAlert(gSystemStatus, "");
      }
      publishAllState();
      bootPrint("STARTUP READY: bench-safe initialization finished");
      gDebugLogger.tryLog("Startup ready.");
      gIrSensors.startTask();  // Begin the continuous sensor measurement task.
      break;
  }
}

void maybeQueueDebugSnapshot() {
  if (!babot::kEnableDebugSnapshots) {
    return;
  }

  const unsigned long now = millis();
  if (now - gLastDebugSnapshotMs < babot::kDebugSnapshotIntervalMs) {
    return;
  }

  gLastDebugSnapshotMs = now;
  gDebugLogger.tryLogf("snapshot mode=%s ball=%d center=(%.2f,%.2f) target=(%.2f,%.2f) roll=%.2f pitch=%.2f",
                       babot::modeToString(gMode),
                       gBallDetected ? 1 : 0,
                       gCenterX,
                       gCenterY,
                       gConfig.setpointX,
                       gConfig.setpointY,
                       gSmoothRoll,
                       gSmoothPitch);
}
