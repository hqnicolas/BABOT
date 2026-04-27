#pragma once

#include <Arduino.h>

namespace babot {

constexpr int kUnassignedPin = -1;
constexpr size_t kSensorsPerMux = 16;
constexpr size_t kMuxCount = 1;
constexpr size_t kSensorCount = kSensorsPerMux * kMuxCount;

struct MuxPins {
  int s0;
  int s1;
  int s2;
  int s3;
  int sig;
};

struct BoardPins {
  int servoPinA;
  int servoPinB;
  int servoPinC;
  int irLedPin;
  int buttonPin;
  int digipotCsPin;
  int digipotIncrementPin;
  int digipotUpDownPin;
  MuxPins mux;
};

constexpr bool isPinAssigned(int pin) {
  return pin >= 0;
}

constexpr BoardPins kBoardPins = {
    41,
    2,
    1,
    45,
    0,
    14,
    12,
    13,
    {40, 39, 48, 47, 9},
};

constexpr unsigned long kDebugBaudRate = 115200;  // Serial baud rate used for debug logging and startup diagnostics.
constexpr unsigned long kLongPressTimeMs = 1000;  // Button hold duration required to classify a press as a long press.
constexpr unsigned long kDoublePressGapMs = 500;  // Maximum time gap used to group short button presses into multi-press gestures.
constexpr unsigned long kBallHoldTimeoutMs = 1000;  // Time to keep the last plate command after a previously detected ball disappears.
constexpr unsigned long kBootReleaseGuardMs = 250;  // Delay window that ignores a held boot button immediately after startup.
constexpr unsigned long kControlIntervalUs = 33333;  // Nominal control-loop period in microseconds (~30 Hz). Used as a timing reference for cycle-to-ms conversions; the actual loop is driven by the sensor state machine.
constexpr unsigned long kTelemetryBroadcastMs = 50;  // WebSocket state broadcast interval for the Web UI.
constexpr size_t kUiCommandQueueLength = 12;  // FreeRTOS queue depth for pending Web UI commands.
constexpr uint16_t kWebTaskStackSize = 8192;  // Stack size allocated to the async Web UI task.
constexpr UBaseType_t kWebTaskPriority = 1;  // FreeRTOS priority assigned to the Web UI task.
constexpr BaseType_t kWebTaskCore = 0;  // ESP32 core used to run the Web UI task.
constexpr uint16_t kSensorTaskStackSize = 4096;  // Stack size for the IR sensor measurement task.
constexpr UBaseType_t kSensorTaskPriority = 2;  // FreeRTOS priority for the sensor task (higher than web, lower than idle would starve it).
constexpr BaseType_t kSensorTaskCore = 1;  // ESP32 core for the sensor task (same as control loop for safe shared state access).
constexpr char kWifiApSsid[] = "12345678";  // SSID broadcast by the robot access point.
constexpr char kWifiApPassword[] = "12345678";  // Password required to connect to the robot access point.

constexpr float kProportionalGain = 1.5f;  // Default PID proportional gain applied to both ball-position axes.
constexpr float kIntegralGain = 0.0f;  // Default PID integral gain used to reduce steady-state position error.
constexpr float kDerivativeGain = 5.0f;  // Default PID derivative gain used to damp fast ball motion.
constexpr float kVelocityScale = 0.001f;  // Small scaling term that prevents derivative velocity normalization from dividing by zero.
constexpr float kIrAlpha = 0.8f;  // Exponential smoothing factor for per-channel IR signal readings.
constexpr float kBallPositionAlpha = 0.7f;  // Exponential smoothing factor for the calculated ball center position.
constexpr float kOutputSmoothingAlpha = 1.0f;  // Exponential smoothing factor for roll and pitch commands.
constexpr float kBallThreshold = 80.0f;  // Minimum IR signal range required to treat the ball as detected.
constexpr float kSensorGridScale = 1.0f;  // Reverted to legacy BaBot coordinate scale (-1.5 to 1.5).

constexpr float kDegToRad = PI / 180.0f;  // Conversion multiplier from degrees to radians for trigonometry.
constexpr float kRadToDeg = 180.0f / PI;  // Conversion multiplier from radians to degrees for servo-angle output.
constexpr float kSqrt3 = 1.73205080757f;  // Cached square root of 3 used in triangular platform geometry.
constexpr float kServoArmLengthMm = 70.0f;  // Length of each servo horn arm used by inverse kinematics.
constexpr float kPassiveLinkLengthMm = 85.0f;  // Length of each passive linkage between servo arm and moving plate.
constexpr float kBaseRadiusMm = 85.0f;  // Base triangle radius from center to each servo linkage anchor.
constexpr float kPlatformRadiusMm = 180.0f / kSqrt3;  // Moving plate triangle radius from center to each linkage anchor.
constexpr float kPlateHeightMm = 140.0f;  // Nominal vertical height of the moving plate used by inverse kinematics.
constexpr float kPlatformPhaseOffsetDeg = 0.0f;  // Angular offset between base anchors and platform anchors.
constexpr float kAssemblyAngleDeg = 34.3f;  // Neutral servo-link angle used by assembly and individual servo tests.
constexpr float kMaxPlateTiltDeg = 8.0f;  // Safety clamp for commanded plate roll and pitch angles.
constexpr float kTestRollAmplitudeDeg = 6.0f;  // Roll amplitude used by the mechanical platform test mode.
constexpr float kTestPitchAmplitudeDeg = 6.0f;  // Pitch amplitude used by the mechanical platform test mode.
constexpr float kTestAngularRateRadPerMs = 0.0005f;  // Angular speed for sine-wave test movement.
constexpr float kPositionTestRadius = 1.2f;  // Radius of the synthetic ball path used by POSITION_TEST.
constexpr float kPositionTestAngularRateRadPerMs = 0.0015f;  // Angular speed of the synthetic ball path in POSITION_TEST.
constexpr uint8_t kBallMissingDebounceCycles = 4;  // Consecutive missing-ball cycles required before declaring the ball absent.

constexpr float kServoMinAngleDeg = 25.0f;  // Lower software clamp for inverse-kinematics servo link angles.
constexpr float kServoMaxAngleDeg = 100.0f;  // Upper software clamp for inverse-kinematics servo link angles.
constexpr int kServoPulseMinUs = 500;  // Minimum PWM pulse width accepted by the servo driver.
constexpr int kServoPulseMaxUs = 2400;  // Maximum PWM pulse width accepted by the servo driver.
constexpr int kServoFrameHz = 50;  // Servo PWM refresh frequency.
constexpr int kServoOffsetDefault = 100;  // Default calibrated servo center offset.
constexpr int kServoOffsetMin = 50;  // Minimum allowed saved servo offset.
constexpr int kServoOffsetMax = 150;  // Maximum allowed saved servo offset.
constexpr int kCalibrationNeutralAngleDeg = 70;  // Servo angle used for inactive servos during calibration.
constexpr int kCalibrationActiveAngleDeg = 25;  // Servo angle used to expose the active servo during calibration.
constexpr int kCalibrationFinalOffsetDelta = 25;  // Offset correction applied when finishing calibration.

constexpr uint8_t kDigitalPotMaxTap = 99;  // Highest supported tap index for the X9C104 digital potentiometer.
constexpr uint8_t kDefaultDigipotTap = 60;  // Default digital potentiometer tap applied at startup.

constexpr unsigned long kAmbientSettleDelayMs = 10;  // Settling delay before reading ambient IR values.
constexpr unsigned long kIrSettleDelayMs = 10;  // Settling delay before reading IR-lit values.
constexpr unsigned long kMuxSettleDelayUs = 600;  // MUX channel settling delay before analog sampling.
constexpr size_t kMuxMultiSampleCount = 1;  // Number of ADC readings to average per MUX channel.
constexpr unsigned long kMuxMultiSampleDelayUs = 25;  // Delay in microseconds between consecutive ADC readings on the same MUX channel.
constexpr size_t kMuxCompensationSamples = 24;  // Number of samples collected per channel during MUX compensation calibration.
constexpr float kMuxCompensationMaxOffset = 2048.0f;  // Maximum per-channel compensation offset applied to IR readings.
constexpr bool kEnableDebugLogging = false;  // Enables queued debug logging through the serial interface.
constexpr bool kEnableDebugSnapshots = false;  // Enables periodic telemetry snapshots in debug logs when true.
constexpr unsigned long kDebugSnapshotIntervalMs = 200;  // Period between optional debug telemetry snapshots.
constexpr unsigned long kWaitForDebugSerialOnBootMs = 250;  // Startup wait time for a debug serial connection.
constexpr size_t kDebugQueueLength = 32;  // Number of debug messages that can be queued before dropping new logs.
constexpr size_t kDebugFlushBudgetPerLoop = 4;  // Maximum queued debug messages flushed per main-loop pass.
constexpr size_t kDebugMessageCapacity = 160;  // Maximum character capacity of one debug log message.
constexpr float kIrStartupFlatRangeThreshold = 5.0f;  // IR range below this value is considered flat during startup health checks.
constexpr float kIrStartupSaturationThreshold = 3500.0f;  // IR peak above this value is considered saturated during startup health checks.
constexpr float kGridAxisMin = -2.0f * kSensorGridScale;  // Minimum logical coordinate for both sensor-grid axes.
constexpr float kGridAxisMax = 2.0f * kSensorGridScale;  // Maximum logical coordinate for both sensor-grid axes.
constexpr float kCenterXMin = kGridAxisMin;  // Minimum allowed X setpoint and measured center value.
constexpr float kCenterXMax = kGridAxisMax;  // Maximum allowed X setpoint and measured center value.
constexpr float kCenterYMin = kGridAxisMin;  // Minimum allowed Y setpoint and measured center value.
constexpr float kCenterYMax = kGridAxisMax;  // Maximum allowed Y setpoint and measured center value.

}
