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

  gTelemetry.captureActive = gCaptureState.active;
  gTelemetry.captureTimeMs = static_cast<float>(gCaptureState.cycles) *
                             static_cast<float>(babot::kControlIntervalUs) / 1000.0f;
  gTelemetry.captureVelocity = gCaptureState.speed;
  gTelemetry.captureRadiusTrend = gCaptureState.radiusTrend;

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
  gCaptureState = {false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}

void updateCaptureKinematics(float targetX, float targetY) {
  const float dx = targetX - gCenterX;
  const float dy = targetY - gCenterY;
  const float radius = sqrtf(dx * dx + dy * dy);
  float velocityX = 0.0f;
  float velocityY = 0.0f;
  float radiusTrend = 0.0f;

  if (gCaptureState.hasPrevious) {
    velocityX = gCenterX - gCaptureState.previousX;
    velocityY = gCenterY - gCaptureState.previousY;
    radiusTrend = radius - gCaptureState.previousRadius;
  }

  gCaptureState.velocityX = velocityX;
  gCaptureState.velocityY = velocityY;
  gCaptureState.speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
  gCaptureState.radius = radius;
  gCaptureState.radiusTrend = radiusTrend;
  gCaptureState.previousX = gCenterX;
  gCaptureState.previousY = gCenterY;
  gCaptureState.previousRadius = radius;
  gCaptureState.hasPrevious = true;
}

bool captureShouldStart(bool newlyDetected) {
  return newlyDetected ||
         gCaptureState.radius >= gCaptureTuning.radiusThreshold ||
         gCaptureState.speed >= gCaptureTuning.speedThreshold ||
         gCaptureState.radiusTrend >= kCaptureTriggerOutwardRate;
}

bool captureShouldRelease() {
  if (gCaptureState.cycles < kCaptureMinCycles) {
    return false;
  }
  if (gCaptureState.cycles >= gCaptureTuning.maxCycles) {
    return true;
  }
  if (gCaptureState.radius <= kCaptureReleaseRadius && gCaptureState.speed <= kCaptureReleaseSpeed) {
    return true;
  }
  return gCaptureState.radiusTrend <= kCaptureReleaseInwardRate;
}

void resetPidDerivativeForCurrentTarget(float targetX, float targetY) {
  gLastErrorX = targetX - gCenterX;
  gLastErrorY = targetY - gCenterY;
  gIntegralX = 0.0f;
  gIntegralY = 0.0f;
}

void applyCaptureOrPidControl(float targetX,
                              float targetY,
                              float gainP,
                              float gainI,
                              float gainD,
                              float tiltLimitDeg,
                              bool newlyDetected) {
  updateCaptureKinematics(targetX, targetY);

  if (!gCaptureState.active && captureShouldStart(newlyDetected)) {
    gCaptureState.active = true;
    gCaptureState.cycles = 0;
    gIntegralX = 0.0f;
    gIntegralY = 0.0f;
  }

  if (gCaptureState.active) {
    const float dx = targetX - gCenterX;
    const float dy = targetY - gCenterY;
    gPlateRoll = gCaptureTuning.positionGain * dx - gCaptureTuning.velocityGain * gCaptureState.velocityX;
    gPlatePitch = gCaptureTuning.positionGain * dy - gCaptureTuning.velocityGain * gCaptureState.velocityY;
    gRawPlateRoll = gPlateRoll;
    gRawPlatePitch = gPlatePitch;
    gPlateRoll = constrain(gPlateRoll, -tiltLimitDeg, tiltLimitDeg);
    gPlatePitch = constrain(gPlatePitch, -tiltLimitDeg, tiltLimitDeg);
    gSmoothRoll = babot::kOutputSmoothingAlpha * gPlateRoll +
                  (1.0f - babot::kOutputSmoothingAlpha) * gSmoothRoll;
    gSmoothPitch = babot::kOutputSmoothingAlpha * gPlatePitch +
                   (1.0f - babot::kOutputSmoothingAlpha) * gSmoothPitch;
    movePlatformWithMotionGrace(gSmoothRoll, gSmoothPitch, babot::kPlateHeightMm);
    ++gCaptureState.cycles;
    if (captureShouldRelease()) {
      gCaptureState.active = false;
      resetPidDerivativeForCurrentTarget(targetX, targetY);
    }
    return;
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
