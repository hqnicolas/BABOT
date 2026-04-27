#pragma once

void pidControl(float input, float target, float gainP, float gainI, float gainD, float &lastErr, float &integ, float &out) {
  const float error = target - input;
  integ += gainI * error;
  const float velocity = error - lastErr;
  const float absoluteVelocity = fabsf(velocity);
  const float scale = absoluteVelocity / (absoluteVelocity + babot::kVelocityScale);
  out = gainP * error + integ + gainD * scale * velocity;
  lastErr = error;
}

void movePlatformWithMotionGrace(float roll, float pitch, float height) {
  gPlatform.movePlatform(roll, pitch, height);
}

void publishSharedState() {
  gTelemetry.mode = gMode;
  gTelemetry.ballDetected = gBallDetected;
  gTelemetry.centerX = gCenterX;
  gTelemetry.centerY = gCenterY;
  gTelemetry.errorX = gErrorX;
  gTelemetry.errorY = gErrorY;
  gTelemetry.plateRoll = gPlateRoll;
  gTelemetry.platePitch = gPlatePitch;
  gTelemetry.rawPlateRoll = gRawPlateRoll;
  gTelemetry.rawPlatePitch = gRawPlatePitch;
  gTelemetry.smoothRoll = gSmoothRoll;
  gTelemetry.smoothPitch = gSmoothPitch;
  gTelemetry.digipotTap = gDigipot.currentTap();
  gTelemetry.muxCompensationReady = gIrSensors.muxCompensationReady();
  gTelemetry.selectedTestServo = gSelectedTestServo;
  gTelemetry.positionTestMux = gPositionTestMux;

  babot::PidAutotuneStatus status = static_cast<babot::PidAutotuneStatus>(gPidAutotunePhase);
  if (gPidAutotunePhase == AUTOTUNE_TESTING_CANDIDATE ||
      gPidAutotunePhase == AUTOTUNE_SCORING_TRIAL ||
      gPidAutotunePhase == AUTOTUNE_WAITING_FOR_BALL_ENTRY ||
      gPidAutotunePhase == AUTOTUNE_CHOOSING_NEXT_CANDIDATE) {
    if (gPidAutotuneStage == AUTOTUNE_STAGE_CMAES) {
      status = babot::PID_AUTOTUNE_EVOLVING;
    } else if (gPidAutotuneStage == AUTOTUNE_STAGE_TUNE_I) {
      status = babot::PID_AUTOTUNE_TUNING_I;
    }
  }
  gTelemetry.pidAutotuneStatus = status;

  gTelemetry.pidAutotuneProgress = pidAutotuneProgress();
  gTelemetry.pidAutotuneBestP = gPidAutotuneBestGains.p;
  gTelemetry.pidAutotuneBestI = gPidAutotuneBestGains.i;
  gTelemetry.pidAutotuneBestD = gPidAutotuneBestGains.d;
  gTelemetry.pidAutotuneScore = gPidAutotuneHasBest ? gPidAutotuneBestMetrics.score : 0.0f;
  gTelemetry.pidAutotuneOvershoot = gPidAutotuneHasBest ? gPidAutotuneBestMetrics.overshoot : 0.0f;
  gTelemetry.pidAutotuneSettlingTimeMs = gPidAutotuneHasBest ? gPidAutotuneBestMetrics.settlingTimeMs : 0.0f;
  gTelemetry.pidAutotuneAverageError = gPidAutotuneHasBest ? gPidAutotuneBestMetrics.averageError : 0.0f;
  gTelemetry.pidAutotuneBaselineScore = gPidAutotuneHasBaseline ? gPidAutotuneBaselineMetrics.score : 0.0f;
  gTelemetry.pidAutotuneHasBest = gPidAutotuneHasBest;
  gTelemetry.pidAutotuneCandidate = static_cast<uint8_t>(min<uint8_t>(gPidAutotuneEvaluationIndex + 1,
                                                                      kPidAutotuneMaxEvaluations));
  gTelemetry.pidAutotuneTrial = gPidAutotuneTrial;
  gTelemetry.pidAutotuneCaptures = gPidAutotuneCurrentMetrics.captures;
  gTelemetry.pidAutotuneMisses = gPidAutotuneCurrentMetrics.misses;
  gTelemetry.pidAutotuneEntrySpeed = gPidAutotuneCurrentMetrics.entrySpeed;
  gTelemetry.pidAutotuneCaptureTimeMs = gPidAutotuneHasBest ? gPidAutotuneBestMetrics.captureTimeMs
                                                            : gPidAutotuneCurrentMetrics.captureTimeMs;
  gTelemetry.pidAutotuneAverageRadius = gPidAutotuneHasBest ? gPidAutotuneBestMetrics.averageRadius
                                                            : gPidAutotuneCurrentMetrics.averageRadius;
  gTelemetry.pidAutotuneNextP = gPidAutotuneNextGains.p;
  gTelemetry.pidAutotuneNextI = gPidAutotuneNextGains.i;
  gTelemetry.pidAutotuneNextD = gPidAutotuneNextGains.d;
  gTelemetry.pidAutotuneSigma = gCmaEs.sigma;
  gTelemetry.pidAutotuneGeneration = gCmaEs.generation;

  strncpy(gTelemetry.pidAutotuneLastFailure,
          gPidAutotuneLastFailure,
          sizeof(gTelemetry.pidAutotuneLastFailure) - 1);
  gTelemetry.pidAutotuneLastFailure[sizeof(gTelemetry.pidAutotuneLastFailure) - 1] = '\0';
  gIrSensors.copySignals(gTelemetry.sensorSignals, babot::kSensorCount);
  gSharedState.publishTelemetry(gTelemetry, gConfig);
}

void resetClosedLoopState() {
  gBallWasOnPlate = false;
  gIntegralX = 0.0f;
  gIntegralY = 0.0f;
  gLastErrorX = 0.0f;
  gLastErrorY = 0.0f;
  gErrorX = 0.0f;
  gErrorY = 0.0f;
  gPlateRoll = 0.0f;
  gPlatePitch = 0.0f;
  gRawPlateRoll = 0.0f;
  gRawPlatePitch = 0.0f;
  gSmoothRoll = 0.0f;
  gSmoothPitch = 0.0f;
  gSkipCounter = 0;
  gBallMissingCycles = 0;
  gBallDetectionGraceActive = false;
}

void applyCaptureOrPidControl(float targetX,
                              float targetY,
                              float gainP,
                              float gainI,
                              float gainD,
                              float tiltLimitDeg,
                              bool newlyDetected) {
  if (newlyDetected) {
    gLastErrorX = targetX - gCenterX;
    gLastErrorY = targetY - gCenterY;
    gIntegralX = 0.0f;
    gIntegralY = 0.0f;
  }

  gErrorX = targetX - gCenterX;
  gErrorY = targetY - gCenterY;
  pidControl(gCenterX, targetX, gainP, gainI, gainD, gLastErrorX, gIntegralX, gPlateRoll);
  pidControl(gCenterY, targetY, gainP, gainI, gainD, gLastErrorY, gIntegralY, gPlatePitch);

  gRawPlateRoll = gPlateRoll;
  gRawPlatePitch = gPlatePitch;
  gPlateRoll = constrain(gPlateRoll, -tiltLimitDeg, tiltLimitDeg);
  gPlatePitch = constrain(gPlatePitch, -tiltLimitDeg, tiltLimitDeg);
  gSmoothRoll = babot::kOutputSmoothingAlpha * gPlateRoll +
                (1.0f - babot::kOutputSmoothingAlpha) * gSmoothRoll;
  gSmoothPitch = babot::kOutputSmoothingAlpha * gPlatePitch +
                 (1.0f - babot::kOutputSmoothingAlpha) * gSmoothPitch;
  movePlatformWithMotionGrace(gSmoothRoll, gSmoothPitch, babot::kPlateHeightMm);
}
