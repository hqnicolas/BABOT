#pragma once

void enterCalibrationMode() {
  gPlatform.resetServoOffsets();
  gCalibrationOffsetA = babot::kServoOffsetDefault;
  gCalibrationOffsetB = babot::kServoOffsetDefault;
  gCalibrationOffsetC = babot::kServoOffsetDefault;
  gCalibServo = CALIB_A;
  gMode = babot::CALIBRATION;
  gPlatform.moveServos(babot::kCalibrationNeutralAngleDeg,
                       babot::kCalibrationNeutralAngleDeg,
                       babot::kCalibrationNeutralAngleDeg);
  delay(2000);
}

void setMode(babot::BaBotMode mode) {
  const babot::BaBotMode previousMode = gMode;
  if (pidAutotuneActive()) {
    cancelPidAutotune("Canceled by mode change");
  }
  if (!ensureModeHardwareAvailable(mode)) {
    return;
  }

  if (mode == babot::CALIBRATION) {
    enterCalibrationMode();
    gDebugLogger.tryLogf("Mode transition: %s -> %s",
                         babot::modeToString(previousMode),
                         babot::modeToString(gMode));
    return;
  }

  gMode = mode;
  gTestStartTime = 0;
  gPositionTestMux = -1;
  if (mode == babot::OFF) {
    resetClosedLoopState();
  } else if (mode == babot::ASSEMBLY) {
    gPlatform.moveServos(babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg);
  } else if (mode == babot::TEST) {
    resetClosedLoopState();
  } else if (mode == babot::INDIVIDUAL_TEST) {
    resetClosedLoopState();
    gPlatform.moveServos(babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg);
  } else if (mode == babot::POSITION_TEST) {
    resetClosedLoopState();
  }

  if (previousMode != gMode) {
    gDebugLogger.tryLogf("Mode transition: %s -> %s",
                         babot::modeToString(previousMode),
                         babot::modeToString(gMode));
  }
}

void handleCalibrationMode(ButtonPress press) {
  if (press == SINGLE_PRESS) {
    if (gCalibServo == CALIB_A) {
      ++gCalibrationOffsetA;
    } else if (gCalibServo == CALIB_B) {
      ++gCalibrationOffsetB;
    } else if (gCalibServo == CALIB_C) {
      ++gCalibrationOffsetC;
    }

    gPlatform.setServoOffsets(gCalibrationOffsetA, gCalibrationOffsetB, gCalibrationOffsetC);
  }

  if (press == LONG_PRESS) {
    if (gCalibServo == CALIB_A) {
      gCalibServo = CALIB_B;
    } else if (gCalibServo == CALIB_B) {
      gCalibServo = CALIB_C;
    } else if (gCalibServo == CALIB_C) {
      gCalibServo = CALIB_DONE;
    }
  }

  if (gCalibServo == CALIB_DONE) {
  gPlatform.setServoOffsets(gCalibrationOffsetA - babot::kCalibrationFinalOffsetDelta,
                              gCalibrationOffsetB - babot::kCalibrationFinalOffsetDelta,
                              gCalibrationOffsetC - babot::kCalibrationFinalOffsetDelta);
    gPlatform.saveServoOffsets();
    resetClosedLoopState();
    gMode = babot::ON;
    return;
  }

  if (gCalibServo == CALIB_A) {
    gPlatform.moveServos(babot::kCalibrationActiveAngleDeg,
                         babot::kCalibrationNeutralAngleDeg,
                         babot::kCalibrationNeutralAngleDeg);
  } else if (gCalibServo == CALIB_B) {
    gPlatform.moveServos(babot::kCalibrationNeutralAngleDeg,
                         babot::kCalibrationActiveAngleDeg,
                         babot::kCalibrationNeutralAngleDeg);
  } else {
    gPlatform.moveServos(babot::kCalibrationNeutralAngleDeg,
                         babot::kCalibrationNeutralAngleDeg,
                         babot::kCalibrationActiveAngleDeg);
  }
}

void handleOnMode() {
  if (gBallDetected) {
    setStatusColor(makeColor(0, 255, 0));
    const bool newlyDetected = !gBallWasOnPlate;

    if (newlyDetected) {
      gSkipCounter = 2;
      gLastErrorX = gConfig.setpointX - gCenterX;
      gLastErrorY = gConfig.setpointY - gCenterY;
    }

    gBallWasOnPlate = true;
    gBallLostTime = millis();

    applyCaptureOrPidControl(gConfig.setpointX,
                             gConfig.setpointY,
                             gConfig.pGain,
                             gConfig.iGain,
                             gConfig.dGain,
                             babot::kMaxPlateTiltDeg,
                             newlyDetected);
    return;
  }

  blinkStatusColor(300, makeColor(255, 96, 0));

  if (gBallWasOnPlate && millis() - gBallLostTime < babot::kBallHoldTimeoutMs) {
    movePlatformWithMotionGrace(gPlateRoll, gPlatePitch, babot::kPlateHeightMm);
    return;
  }

  resetClosedLoopState();
  movePlatformWithMotionGrace(0.0f, 0.0f, babot::kPlateHeightMm);
}

void handleAssemblyMode() {
  static uint8_t assemblyStep = 0;
  const bool pressed = (gLatchedPress == SINGLE_PRESS);

  if (pressed) {
    gLatchedPress = NO_PRESS;
  }

  if (assemblyStep == 0 && pressed) {
    assemblyStep = 1;

    gPlatform.moveServos(babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg);
    gPlatform.moveServos(80.0f, babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg);
    delay(250);
    gPlatform.moveServos(babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg);

    gPlatform.moveServos(babot::kAssemblyAngleDeg, 80.0f, babot::kAssemblyAngleDeg);
    delay(250);
    gPlatform.moveServos(babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg);

    gPlatform.moveServos(babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg, 80.0f);
    delay(250);
    gPlatform.moveServos(babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg, babot::kAssemblyAngleDeg);

    assemblyStep = 0;
  }
}

void handleTestMode() {
  setStatusColor(makeColor(0, 96, 255));

  if (gTestStartTime == 0) {
    gTestStartTime = millis();
  }

  const unsigned long elapsedMs = millis() - gTestStartTime;
  const float phase = elapsedMs * babot::kTestAngularRateRadPerMs * 2.0f * PI;
  const float roll = babot::kTestRollAmplitudeDeg * sinf(phase);
  const float pitch = babot::kTestPitchAmplitudeDeg * sinf(phase + PI / 2.0f);

  gPlateRoll = roll;
  gPlatePitch = pitch;
  gRawPlateRoll = roll;
  gRawPlatePitch = pitch;
  gSmoothRoll = roll;
  gSmoothPitch = pitch;
  movePlatformWithMotionGrace(gSmoothRoll, gSmoothPitch, babot::kPlateHeightMm);
}

void handleIndividualTestMode() {
  setStatusColor(makeColor(255, 160, 0));

  if (gTestStartTime == 0) {
    gTestStartTime = millis();
  }

  const unsigned long elapsedMs = millis() - gTestStartTime;
  const float phase = elapsedMs * babot::kTestAngularRateRadPerMs * 2.0f * PI;
  const float offset = babot::kTestRollAmplitudeDeg * sinf(phase);
  float servoAngles[3] = {
      babot::kAssemblyAngleDeg,
      babot::kAssemblyAngleDeg,
      babot::kAssemblyAngleDeg,
  };

  const uint8_t selected = gSelectedTestServo % 3;
  servoAngles[selected] = babot::kAssemblyAngleDeg + offset;
  gPlateRoll = 0.0f;
  gPlatePitch = 0.0f;
  gRawPlateRoll = 0.0f;
  gRawPlatePitch = 0.0f;
  gSmoothRoll = 0.0f;
  gSmoothPitch = 0.0f;
  gPlatform.moveServos(servoAngles[0], servoAngles[1], servoAngles[2]);
}

void handlePositionTestMode() {
  setStatusColor(makeColor(0, 180, 255));

  if (gTestStartTime == 0) {
    gTestStartTime = millis();
  }

  const unsigned long elapsedMs = millis() - gTestStartTime;
  const float phase = elapsedMs * babot::kPositionTestAngularRateRadPerMs;
  const float radius = min(babot::kPositionTestRadius,
                           min(babot::kCenterXMax - gConfig.setpointX,
                               min(gConfig.setpointX - babot::kCenterXMin,
                                   min(babot::kCenterYMax - gConfig.setpointY,
                                       gConfig.setpointY - babot::kCenterYMin))));

  const float testX = constrain(gConfig.setpointX + radius * cosf(phase),
                                babot::kCenterXMin,
                                babot::kCenterXMax);
  const float testY = constrain(gConfig.setpointY + radius * sinf(phase),
                                babot::kCenterYMin,
                                babot::kCenterYMax);

  gPositionTestMux = -1;
  gIrSensors.injectSyntheticBall(testX, testY);

  gBallDetected = gIrSensors.ballOnPlate(gConfig.ballThreshold);
  gIrSensors.calculateWeightedCenter(gCenterX, gCenterY, gConfig.ballThreshold);
  gBallMissingCycles = 0;
  gBallDetectionGraceActive = false;
  handleOnMode();
}
