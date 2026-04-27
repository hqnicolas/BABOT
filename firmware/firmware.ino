#include <cstring>
#include <math.h>

#include "src/BoardConfig.h"
#include "src/DebugLogger.h"
#include "src/IRSensorArray.h"
#include "src/PlatformControl.h"
#include "src/RuntimeState.h"
#include "src/WebUiServer.h"
#include "src/X9C104Digipot.h"

namespace {

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial &kDebugPort = Serial0;
#else
decltype(Serial) &kDebugPort = Serial;
#endif

enum CalibServo {
  CALIB_A,
  CALIB_B,
  CALIB_C,
  CALIB_DONE
};

enum ButtonPress {
  NO_PRESS,
  SINGLE_PRESS,
  DOUBLE_PRESS,
  TRIPLE_PRESS,
  QUAD_PRESS,
  LONG_PRESS
};

enum StartupRiskStage : uint8_t {
  STARTUP_RISK_NONE = 0,
  STARTUP_RISK_PLATFORM_BEGIN = 1,
  STARTUP_RISK_DIGIPOT_RESTORE = 2
};

enum StartupSequenceStep : uint8_t {
  STARTUP_STEP_PLATFORM = 0,
  STARTUP_STEP_DIGIPOT = 1,
  STARTUP_STEP_SENSORS = 2,
  STARTUP_STEP_COMPLETE = 3
};

enum PidAutotunePhase : uint8_t {
  AUTOTUNE_IDLE,
  AUTOTUNE_WAITING_FOR_BALL_ENTRY,
  AUTOTUNE_TESTING_CANDIDATE,
  AUTOTUNE_SCORING_TRIAL,
  AUTOTUNE_CHOOSING_NEXT_CANDIDATE,
  AUTOTUNE_APPLYING,
  AUTOTUNE_COMPLETE,
  AUTOTUNE_FAILED
};

enum PidAutotuneStage : uint8_t {
  AUTOTUNE_STAGE_BASELINE,
  AUTOTUNE_STAGE_CMAES,
  AUTOTUNE_STAGE_TUNE_I
};

struct PidAutotuneCandidate {
  float p;
  float i;
  float d;
};

struct PidAutotuneMetrics {
  float score;
  float overshoot;
  float settlingTimeMs;
  float averageError;
  float entrySpeed;
  float captureTimeMs;
  float averageRadius;
  uint8_t captures;
  uint8_t misses;
  bool unsafe;
};

struct PidAutotuneHistory {
  PidAutotuneCandidate candidate;
  PidAutotuneMetrics metrics;
};


enum PidAutotuneRelayAxis : uint8_t {
  RELAY_AXIS_NONE,
  RELAY_AXIS_X,
  RELAY_AXIS_Y
};

struct PidAutotuneRelayMetrics {
  float ku;
  float puCycles;
  float observedAmplitude;
  uint8_t crossings;
  bool valid;
};

struct PidAutotuneStationarySample {
  float x;
  float y;
  float radius;
  float speed;
};

struct PidAutotuneStationaryMetrics {
  float meanCenterX;
  float meanCenterY;
  float meanRadius;
  float meanSpeed;
  float peakSpeed;
  float centerSpanX;
  float centerSpanY;
  float radiusSpread;
  float outwardDrift;
  uint8_t sampleCount;
  bool valid;
};

enum PidAutotuneAbortReason : uint8_t {
  AUTOTUNE_ABORT_NONE,
  AUTOTUNE_ABORT_BALL_LOST,
  AUTOTUNE_ABORT_RADIUS_LIMIT,
  AUTOTUNE_ABORT_SPEED_LIMIT,
  AUTOTUNE_ABORT_NOT_SETTLED,
  AUTOTUNE_ABORT_NO_CROSSINGS
};

babot::IRSensorArray gIrSensors(babot::kBoardPins);
babot::PlatformControl gPlatform(babot::kBoardPins);
babot::X9C104Digipot gDigipot(babot::kBoardPins.digipotCsPin,
                              babot::kBoardPins.digipotIncrementPin,
                              babot::kBoardPins.digipotUpDownPin);
babot::SharedState gSharedState;
babot::DebugLogger gDebugLogger;
babot::WebUiServer gWebUi(gSharedState, gDebugLogger);

babot::BaBotMode gMode = babot::ON;
babot::RuntimeConfig gConfig = babot::defaultRuntimeConfig();
babot::SystemStatus gSystemStatus = babot::defaultSystemStatus();
babot::TelemetryState gTelemetry{};

CalibServo gCalibServo = CALIB_A;
float gCenterX = 0.0f;
float gCenterY = 0.0f;
float gLastErrorX = 0.0f;
float gLastErrorY = 0.0f;
float gIntegralX = 0.0f;
float gIntegralY = 0.0f;
float gErrorX = 0.0f;
float gErrorY = 0.0f;
float gPlateRoll = 0.0f;
float gPlatePitch = 0.0f;
float gRawPlateRoll = 0.0f;
float gRawPlatePitch = 0.0f;
float gSmoothRoll = 0.0f;
float gSmoothPitch = 0.0f;
int gSkipCounter = 0;
unsigned long gTestStartTime = 0;
uint8_t gSelectedTestServo = 0;
int8_t gPositionTestMux = -1;
uint8_t gBallMissingCycles = 0;
bool gBallDetectionGraceActive = false;

bool gBallDetected = false;
bool gLastLoggedBallDetected = false;
bool gBallWasOnPlate = false;
bool gStartupComplete = false;
bool gWebTaskOwnsDebugFlush = false;
unsigned long gBallLostTime = 0;
unsigned long gButtonDownTime = 0;
unsigned long gLastPressTime = 0;
unsigned long gBootGuardStartTime = 0;
unsigned long gLastDebugSnapshotMs = 0;
int gPressCount = 0;
bool gButtonArmed = false;
ButtonPress gLatchedPress = NO_PRESS;
StartupSequenceStep gStartupStep = STARTUP_STEP_PLATFORM;
StartupRiskStage gRecoveredRiskStage = STARTUP_RISK_NONE;

int gCalibrationOffsetA = babot::kServoOffsetDefault;
int gCalibrationOffsetB = babot::kServoOffsetDefault;
int gCalibrationOffsetC = babot::kServoOffsetDefault;

constexpr uint8_t kPidAutotuneIValueCount = 4;
constexpr uint8_t kPidAutotuneTrialsPerCandidate = 3;
constexpr uint16_t kPidAutotuneTrialCycles = 120;
constexpr uint16_t kPidAutotuneIgnoreCycles = 5;
constexpr uint16_t kPidAutotuneCaptureHoldCycles = 12;
constexpr float kPidAutotuneSafeTiltDeg = 10.0f;
constexpr float kPidAutotuneCaptureRadius = 0.22f;
constexpr float kPidAutotuneNearCaptureRadius = 0.38f;
constexpr float kPidAutotuneMinP = 0.5f;
constexpr float kPidAutotuneMaxP = 12.0f;
constexpr float kPidAutotuneMinI = 0.0f;
constexpr float kPidAutotuneMaxI = 0.04f;
constexpr float kPidAutotuneMinD = 2.0f;
constexpr float kPidAutotuneMaxD = 60.0f;
constexpr float kPidAutotuneDuplicateTolerance = 0.03f;
constexpr float kPidAutotuneImprovementEpsilon = 0.01f;
constexpr uint16_t kPidAutotuneMaxEvaluations = 1024;

constexpr float kCaptureReleaseRadius = 0.55f;
constexpr float kCaptureReleaseSpeed = 0.08f;
constexpr float kCaptureReleaseInwardRate = -0.04f;

constexpr uint16_t kDefaultCaptureMaxCycles = 10;
constexpr uint16_t kCaptureMinCycles = 2;
constexpr float kDefaultCaptureTriggerRadius = 1.15f;
constexpr float kDefaultCaptureTriggerSpeed = 0.18f;
constexpr float kDefaultCapturePositionGain = 14.0f;
constexpr float kDefaultCaptureVelocityGain = 32.0f;
constexpr float kCaptureTriggerOutwardRate = 0.03f;

PidAutotunePhase gPidAutotunePhase = AUTOTUNE_IDLE;
PidAutotuneStage gPidAutotuneStage = AUTOTUNE_STAGE_BASELINE;
PidAutotuneCandidate gPidAutotuneQueue[kPidAutotuneMaxEvaluations] = {};
PidAutotuneHistory gPidAutotuneHistory[kPidAutotuneMaxEvaluations] = {};
PidAutotuneCandidate gPidAutotunePreviousGains = {0.0f, 0.0f, 0.0f};
PidAutotuneCandidate gPidAutotuneBestGains = {0.0f, 0.0f, 0.0f};
PidAutotuneCandidate gPidAutotuneNextGains = {0.0f, 0.0f, 0.0f};
PidAutotuneMetrics gPidAutotuneCurrentMetrics = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
PidAutotuneMetrics gPidAutotuneBestMetrics = {999999.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
PidAutotuneMetrics gPidAutotuneBaselineMetrics = {999999.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
PidAutotuneMetrics gPidAutotunePdReferenceMetrics = {999999.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
PidAutotuneCandidate gPidAutotunePdBestGains = {0.0f, 0.0f, 0.0f};
float gPidAutotuneNoiseFloor = 0.0f;
float gPidAutotuneNoiseSum = 0.0f;
uint8_t gPidAutotuneSafetyAborts = 0;

uint8_t gPidAutotuneIIndex = 0;
float gPidAutotuneBaseSetpointX = 0.0f;
float gPidAutotuneBaseSetpointY = 0.0f;
float gPidAutotuneErrorSum = 0.0f;
float gPidAutotuneRadiusSum = 0.0f;
float gPidAutotunePreviousCenterX = 0.0f;
float gPidAutotunePreviousCenterY = 0.0f;
float gPidAutotuneVelocitySum = 0.0f;
float gPidAutotunePeakVelocity = 0.0f;
uint16_t gPidAutotunePhaseCycles = 0;
uint16_t gPidAutotuneCaptureCycles = 0;
uint16_t gPidAutotuneSampleCycles = 0;
uint16_t gPidAutotuneEvaluationIndex = 0;
uint8_t gPidAutotuneQueuedCount = 0;
uint8_t gPidAutotuneTrial = 0;
uint8_t gPidAutotuneLossCycles = 0;
uint8_t gPidAutotuneSaturationCycles = 0;
uint16_t gPidAutotuneExplorationStep = 0;
bool gPidAutotuneHasBest = false;
bool gPidAutotuneHasBaseline = false;
bool gPidAutotuneHasPrevious = false;
bool gPidAutotuneTrialHadBall = false;
char gPidAutotuneLastFailure[babot::kStatusTextCapacity] = "";

// CMA-ES optimizer state
#include "src/CmaEsOptimizer.h"
CmaEsState gCmaEs = {};
float gCmaEsGenerationScores[kCmaEsLambda] = {};
uint8_t gCmaEsLambdaIndex = 0;  // How many of the current generation have been scored

bool isPinAssigned(int pin) {
  return babot::isPinAssigned(pin);
}

void resetClosedLoopState();
uint8_t pidAutotuneProgress();
bool pidAutotuneActive();
void failPidAutotune(const char *reason);
void cancelPidAutotune(const char *reason);

#include "src/ButtonInput.h"
#include "src/ControlLoop.h"
#include "src/PidAutotune.h"

void setMode(babot::BaBotMode mode);

#include "src/StartupManager.h"
#include "src/ModeHandlers.h"
#include "src/UiCommandHandler.h"

}

void setup() {

  kDebugPort.begin(babot::kDebugBaudRate);
  delay(50);
  bootPrint("BOOT 01: serial ready");

  if (isFirstBootAfterFlash()) {
    bootPrint("BOOT 01A: first boot detected - resetting all NVS to defaults");
    performFactoryReset();
  }

  if (isPinAssigned(babot::kBoardPins.buttonPin)) {
    pinMode(babot::kBoardPins.buttonPin, INPUT_PULLUP);
  }
  bootCheckpoint("BOOT 02: button pin configured", makeColor(16, 16, 16));

  bootCheckpoint("BOOT 03: RGB LED driver ready", makeColor(0, 0, 32));

  if (isPinAssigned(babot::kBoardPins.buttonPin)) {
    updateSubsystemStatus("button", gSystemStatus.button, babot::READY, "Boot button available.");
  } else {
    updateSubsystemStatus("button", gSystemStatus.button, babot::SUBSYSTEM_DISABLED, "Button pin not assigned.");
  }

  if (isPinAssigned(babot::kBoardPins.irLedPin)) {
    pinMode(babot::kBoardPins.irLedPin, INPUT_PULLUP);
  }

  if (babot::kWaitForDebugSerialOnBootMs > 0) {
    delay(babot::kWaitForDebugSerialOnBootMs);
  }
  bootCheckpoint("BOOT 04: post-serial delay complete", makeColor(0, 16, 16));

  gDebugLogger.begin(kDebugPort);
  gDebugLogger.tryLog("Boot start.");
  bootCheckpoint("BOOT 05: debug logger ready", makeColor(0, 32, 32));

  gSystemStatus = babot::defaultSystemStatus();
  updateSubsystemStatus("button",
                        gSystemStatus.button,
                        isPinAssigned(babot::kBoardPins.buttonPin) ? babot::READY : babot::SUBSYSTEM_DISABLED,
                        isPinAssigned(babot::kBoardPins.buttonPin) ? "Boot button available." : "Button pin not assigned.");

  gRecoveredRiskStage = readPendingRiskStage();
  clearPendingRiskStage();
  applyRecoveredBootPolicy();

  setStartupPhaseWithLog(babot::STARTUP_SAFE_BOOT, "Loading persisted runtime state");
  const bool storageAvailable = gSharedState.loadRuntimeConfig(gConfig);
  updateSubsystemStatus("storage",
                        gSystemStatus.storage,
                        storageAvailable ? babot::READY : babot::DEGRADED,
                        storageAvailable ? "Runtime configuration loaded from NVS."
                                         : "Runtime configuration unavailable; using defaults.",
                        !storageAvailable);

  bootPrintf("BOOT 06: runtime config loaded digipot=%u", static_cast<unsigned>(gConfig.digipotTap));
  gDebugLogger.tryLogf("Runtime config loaded. Digipot=%u",
                       static_cast<unsigned>(gConfig.digipotTap));
  bootCheckpoint("BOOT 07: runtime config applied", makeColor(16, 32, 0));

  bootPrint("BOOT 08A: shared state begin");
  if (!gSharedState.begin(gConfig)) {
    bootPrint("BOOT 08E: shared state init failed");
    gDebugLogger.tryLog("Shared state initialization failed.");
    setSystemAlertWithLog("Shared state initialization failed; Web UI commands may be unavailable.");
  }
  bootCheckpoint("BOOT 08B: shared state begin complete", makeColor(32, 0, 0));
  publishAllState();

  setStartupPhaseWithLog(babot::STARTUP_NETWORK, "Starting Wi-Fi AP and Web UI");
  bootPrint("BOOT 09A: web UI begin");
  gWebTaskOwnsDebugFlush = gWebUi.begin();
  if (gWebTaskOwnsDebugFlush) {
    updateSubsystemStatus("wifiAp", gSystemStatus.wifiAp, babot::READY, "Wi-Fi AP started.");
    updateSubsystemStatus("webUi", gSystemStatus.webUi, babot::READY, "Async Web UI running.");
    bootCheckpoint("BOOT 09B: web UI begin complete", makeColor(0, 32, 8));
    gDebugLogger.tryLog("Async Web UI started; serial debug flush moved to Wi-Fi core.");
  } else {
    updateSubsystemStatus("wifiAp", gSystemStatus.wifiAp, babot::FAILED, "Wi-Fi AP or web task failed to start.", true);
    updateSubsystemStatus("webUi", gSystemStatus.webUi, babot::FAILED, "Web UI unavailable; staying in serial-only mode.");
    bootPrint("BOOT 09E: web UI begin failed");
    gDebugLogger.tryLog("Web UI task failed; falling back to direct-loop debug flushing.");
    gDebugLogger.flushDirect();
  }

  gBootGuardStartTime = millis();
  bootCheckpoint("BOOT 10: safe boot complete, deferred startup queued", makeColor(0, 48, 0));
  publishAllState();
}

void loop() {
  const ButtonPress press = checkButton(gMode == babot::CALIBRATION);
  if (press != NO_PRESS) {
    gLatchedPress = press;
    gDebugLogger.tryLogf("Button event: %s", buttonPressToString(press));
  }

  applyModeSwitching(press);
  drainUiCommands();

  if (!gStartupComplete) {
    advanceStartupSequence();
    if (!gWebTaskOwnsDebugFlush) {
      gDebugLogger.flushDirect();
    }
    gLatchedPress = NO_PRESS;
    delay(1);
    return;
  }

  if (xSemaphoreTake(gIrSensors.measurementReady, 0) != pdTRUE) {
    return;
  }

  gDigipot.setTap(gConfig.digipotTap);

  if (gMode != babot::POSITION_TEST) {
    const bool previousBallDetected = gBallDetected;
    const bool rawBallDetected = gIrSensors.ballOnPlate(gConfig.ballThreshold);
    float rawX = 0.0f;
    float rawY = 0.0f;
    if (rawBallDetected) {
      gBallDetected = true;
      gBallDetectionGraceActive = false;
      gBallMissingCycles = 0;
      gIrSensors.calculateWeightedCenter(rawX, rawY, gConfig.ballThreshold);
      if (!previousBallDetected) {
        gCenterX = rawX;
        gCenterY = rawY;
      } else {
        gCenterX = babot::kBallPositionAlpha * rawX + (1.0f - babot::kBallPositionAlpha) * gCenterX;
        gCenterY = babot::kBallPositionAlpha * rawY + (1.0f - babot::kBallPositionAlpha) * gCenterY;
      }
    } else if (previousBallDetected && gBallMissingCycles < babot::kBallMissingDebounceCycles) {
      ++gBallMissingCycles;
      gBallDetected = true;
      gBallDetectionGraceActive = true;
    } else {
      gBallDetected = false;
      gBallDetectionGraceActive = false;
      gBallMissingCycles = 0;
    }

    if (previousBallDetected != gBallDetected || gLastLoggedBallDetected != gBallDetected) {
      gDebugLogger.tryLogf("Ball state changed: %s center=(%.2f,%.2f)",
                           gBallDetected ? "detected" : "missing",
                           gCenterX,
                           gCenterY);
      gLastLoggedBallDetected = gBallDetected;
    }
  }
  maybeQueueDebugSnapshot();

  if (pidAutotuneActive()) {
    handlePidAutotune();
    publishAllState();
    if (!gWebTaskOwnsDebugFlush) {
      gDebugLogger.flushDirect();
    }
    gLatchedPress = NO_PRESS;
    return;
  }

  switch (gMode) {
    case babot::ON:
      handleOnMode();
      break;

    case babot::OFF:
      resetClosedLoopState();
      movePlatformWithMotionGrace(0.0f, 0.0f, babot::kPlateHeightMm);
      break;

    case babot::ASSEMBLY:
      handleAssemblyMode();
      break;

    case babot::CALIBRATION:
      handleCalibrationMode(gLatchedPress);
      break;

    case babot::TEST:
      handleTestMode();
      break;

    case babot::INDIVIDUAL_TEST:
      handleIndividualTestMode();
      break;

    case babot::POSITION_TEST:
      handlePositionTestMode();
      break;
  }

  publishAllState();
  if (!gWebTaskOwnsDebugFlush) {
    gDebugLogger.flushDirect();
  }
  gLatchedPress = NO_PRESS;
}
