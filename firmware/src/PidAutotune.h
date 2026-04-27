#pragma once

// Forward declaration - defined further down in this file.
void chooseNextPidAutotuneCandidate();

uint8_t pidAutotuneProgress() {
  if (gPidAutotunePhase == AUTOTUNE_COMPLETE) {
    return 100;
  }
  if (gPidAutotunePhase == AUTOTUNE_IDLE || gPidAutotunePhase == AUTOTUNE_FAILED) {
    return 0;
  }
  if (gPidAutotunePhase == AUTOTUNE_APPLYING) {
    return 98;
  }
  if (gPidAutotuneStage == AUTOTUNE_STAGE_TUNE_I) {
    const uint16_t scaled = (static_cast<uint16_t>(gPidAutotuneIIndex) * 80) /
                            static_cast<uint16_t>(kPidAutotuneIValueCount);
    return static_cast<uint8_t>(min<uint16_t>(95, 15 + scaled));
  }
  if (!gPidAutotuneHasBaseline) {
    return 5;
  }
  // CMA-ES progress: estimate from generation and sigma convergence
  if (gPidAutotuneStage == AUTOTUNE_STAGE_CMAES) {
    const float genFrac = static_cast<float>(gCmaEs.generation) / static_cast<float>(kCmaEsMaxGenerations);
    const float sigmaFrac = 1.0f - (gCmaEs.sigma / kCmaEsInitialSigma);
    const float progress = max(genFrac, sigmaFrac) * 80.0f;
    return static_cast<uint8_t>(10 + min(progress, 80.0f));
  }
  return static_cast<uint8_t>(5 + (gPidAutotuneEvaluationIndex % 90));
}

bool pidAutotuneActive() {
  return gPidAutotunePhase != AUTOTUNE_IDLE &&
         gPidAutotunePhase != AUTOTUNE_COMPLETE &&
         gPidAutotunePhase != AUTOTUNE_FAILED;
}

void setPidAutotuneFailure(const char *reason) {
  const char *safeReason = (reason != nullptr) ? reason : "";
  strncpy(gPidAutotuneLastFailure, safeReason, sizeof(gPidAutotuneLastFailure) - 1);
  gPidAutotuneLastFailure[sizeof(gPidAutotuneLastFailure) - 1] = '\0';
}

void restorePidAutotuneBaseSetpoint() {
  gConfig.setpointX = gPidAutotuneBaseSetpointX;
  gConfig.setpointY = gPidAutotuneBaseSetpointY;
}

float currentAutotuneRadius() {
  const float dx = gPidAutotuneBaseSetpointX - gCenterX;
  const float dy = gPidAutotuneBaseSetpointY - gCenterY;
  return sqrtf(dx * dx + dy * dy);
}

PidAutotuneCandidate clampPidAutotuneCandidate(PidAutotuneCandidate candidate) {
  candidate.p = constrain(candidate.p, kPidAutotuneMinP, kPidAutotuneMaxP);
  candidate.i = constrain(candidate.i, kPidAutotuneMinI, kPidAutotuneMaxI);
  candidate.d = constrain(candidate.d, kPidAutotuneMinD, kPidAutotuneMaxD);
  return candidate;
}

bool pidAutotuneCandidatesClose(const PidAutotuneCandidate &a, const PidAutotuneCandidate &b) {
  return fabsf(a.p - b.p) <= kPidAutotuneDuplicateTolerance &&
         fabsf(a.i - b.i) <= kPidAutotuneDuplicateTolerance &&
         fabsf(a.d - b.d) <= kPidAutotuneDuplicateTolerance;
}

bool pidAutotuneCandidateSeen(const PidAutotuneCandidate &candidate) {
  for (uint8_t i = 0; i < gPidAutotuneQueuedCount; ++i) {
    if (pidAutotuneCandidatesClose(gPidAutotuneQueue[i], candidate)) {
      return true;
    }
  }
  const uint8_t historyCount = static_cast<uint8_t>(min<uint16_t>(gPidAutotuneEvaluationIndex, kPidAutotuneMaxEvaluations));
  for (uint8_t i = 0; i < historyCount; ++i) {
    if (pidAutotuneCandidatesClose(gPidAutotuneHistory[i].candidate, candidate)) {
      return true;
    }
  }
  return false;
}

bool enqueuePidAutotuneCandidate(PidAutotuneCandidate candidate) {
  if (gPidAutotuneQueuedCount >= kPidAutotuneMaxEvaluations) {
    return false;
  }
  candidate = clampPidAutotuneCandidate(candidate);
  if (pidAutotuneCandidateSeen(candidate)) {
    return false;
  }
  gPidAutotuneQueue[gPidAutotuneQueuedCount++] = candidate;
  return true;
}

bool dequeuePidAutotuneCandidate(PidAutotuneCandidate &candidate) {
  if (gPidAutotuneQueuedCount == 0) {
    return false;
  }
  candidate = gPidAutotuneQueue[0];
  for (uint8_t i = 1; i < gPidAutotuneQueuedCount; ++i) {
    gPidAutotuneQueue[i - 1] = gPidAutotuneQueue[i];
  }
  --gPidAutotuneQueuedCount;
  return true;
}

void resetPidAutotuneTrialMetrics() {
  gPidAutotuneErrorSum = 0.0f;
  gPidAutotuneRadiusSum = 0.0f;
  gPidAutotuneVelocitySum = 0.0f;
  gPidAutotunePeakVelocity = 0.0f;
  gPidAutotuneCaptureCycles = 0;
  gPidAutotuneSampleCycles = 0;
  gPidAutotuneLossCycles = 0;
  gPidAutotuneSaturationCycles = 0;
  gPidAutotuneTrialHadBall = false;
}

void beginPidAutotuneCandidate(const PidAutotuneCandidate &candidate) {
  gPidAutotuneNextGains = clampPidAutotuneCandidate(candidate);
  gConfig.pGain = gPidAutotuneNextGains.p;
  gConfig.iGain = gPidAutotuneNextGains.i;
  gConfig.dGain = gPidAutotuneNextGains.d;
  restorePidAutotuneBaseSetpoint();
  gPidAutotuneTrial = 0;
  gPidAutotuneCurrentMetrics = {999999.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
  gPidAutotunePhase = AUTOTUNE_WAITING_FOR_BALL_ENTRY;
  gPidAutotunePhaseCycles = 0;
  resetClosedLoopState();
  movePlatformWithMotionGrace(0.0f, 0.0f, babot::kPlateHeightMm);
}

void beginPidAutotuneTrial() {
  gConfig.pGain = gPidAutotuneNextGains.p;
  gConfig.iGain = gPidAutotuneNextGains.i;
  gConfig.dGain = gPidAutotuneNextGains.d;
  restorePidAutotuneBaseSetpoint();
  gPidAutotunePhase = AUTOTUNE_TESTING_CANDIDATE;
  gPidAutotunePhaseCycles = 0;
  resetPidAutotuneTrialMetrics();
  resetClosedLoopState();
  gPidAutotunePreviousCenterX = gCenterX;
  gPidAutotunePreviousCenterY = gCenterY;
}

void failPidAutotune(const char *reason) {
  setPidAutotuneFailure(reason);
  gPidAutotunePhase = AUTOTUNE_FAILED;
  gConfig.pGain = gPidAutotunePreviousGains.p;
  gConfig.iGain = gPidAutotunePreviousGains.i;
  gConfig.dGain = gPidAutotunePreviousGains.d;
  restorePidAutotuneBaseSetpoint();
  resetClosedLoopState();
  movePlatformWithMotionGrace(0.0f, 0.0f, babot::kPlateHeightMm);
}

void updatePidAutotuneTrialMetrics() {
  if (gPidAutotunePhaseCycles <= kPidAutotuneIgnoreCycles || !gBallDetected) {
    return;
  }

  const float radius = currentAutotuneRadius();
  const float stepX = gCenterX - gPidAutotunePreviousCenterX;
  const float stepY = gCenterY - gPidAutotunePreviousCenterY;
  const float speed = sqrtf(stepX * stepX + stepY * stepY);
  gPidAutotunePreviousCenterX = gCenterX;
  gPidAutotunePreviousCenterY = gCenterY;

  if (!gPidAutotuneTrialHadBall) {
    gPidAutotuneCurrentMetrics.entrySpeed = max(gPidAutotuneCurrentMetrics.entrySpeed, speed);
    gPidAutotuneTrialHadBall = true;
  }

  gPidAutotuneVelocitySum += speed;
  gPidAutotunePeakVelocity = max(gPidAutotunePeakVelocity, speed);
  gPidAutotuneRadiusSum += radius;
  ++gPidAutotuneSampleCycles;
  if (radius <= kPidAutotuneCaptureRadius) {
    ++gPidAutotuneCaptureCycles;
  } else {
    gPidAutotuneCaptureCycles = 0;
  }
}

void finishPidAutotuneTrial(bool captured) {
  const uint16_t sampleCycles = max<uint16_t>(1, gPidAutotuneSampleCycles);
  const float averageRadius = gPidAutotuneRadiusSum / static_cast<float>(sampleCycles);
  const float averageSpeed = gPidAutotuneVelocitySum / static_cast<float>(sampleCycles);
  const float captureTimeMs = captured
                                  ? static_cast<float>(gPidAutotunePhaseCycles) *
                                        static_cast<float>(babot::kControlIntervalUs) / 1000.0f
                                  : static_cast<float>(kPidAutotuneTrialCycles) *
                                        static_cast<float>(babot::kControlIntervalUs) / 1000.0f;
  const float missPenalty = captured ? 0.0f : 280.0f;
  const float closePenalty = (!captured && averageRadius < kPidAutotuneNearCaptureRadius) ? -80.0f : 0.0f;
  const float edgePenalty = (captured && averageRadius > 0.8f) ? 300.0f : 0.0f;
  const float score = averageRadius * 150.0f + averageSpeed * 120.0f + gPidAutotunePeakVelocity * 50.0f +
                      captureTimeMs * 0.04f + static_cast<float>(gPidAutotuneSaturationCycles) * 8.0f +
                      missPenalty + closePenalty + edgePenalty;

  gPidAutotuneCurrentMetrics.score = min(gPidAutotuneCurrentMetrics.score, score);
  gPidAutotuneCurrentMetrics.averageError += averageRadius;
  gPidAutotuneCurrentMetrics.averageRadius += averageRadius;
  gPidAutotuneCurrentMetrics.overshoot = max(gPidAutotuneCurrentMetrics.overshoot, gPidAutotunePeakVelocity);
  gPidAutotuneCurrentMetrics.settlingTimeMs += captureTimeMs;
  gPidAutotuneCurrentMetrics.captureTimeMs += captureTimeMs;
  if (captured) {
    ++gPidAutotuneCurrentMetrics.captures;
  } else {
    ++gPidAutotuneCurrentMetrics.misses;
  }
  ++gPidAutotuneTrial;
  gPidAutotunePhase = AUTOTUNE_SCORING_TRIAL;
  gPidAutotunePhaseCycles = 0;
}

bool pidAutotuneMetricsBetter(const PidAutotuneMetrics &candidate, const PidAutotuneMetrics &reference) {
  return candidate.captures > 0 && candidate.score + kPidAutotuneImprovementEpsilon < reference.score;
}

bool pidAutotuneIMetricsSafe(const PidAutotuneMetrics &candidate, const PidAutotuneMetrics &reference) {
  if (candidate.captures == 0 || candidate.misses > reference.misses) {
    return false;
  }
  if (candidate.overshoot > reference.overshoot + 0.02f) {
    return false;
  }
  if (candidate.averageRadius > reference.averageRadius + 0.03f) {
    return false;
  }
  return true;
}

void recordPidAutotuneHistory() {
  const uint8_t slot = static_cast<uint8_t>(gPidAutotuneEvaluationIndex % kPidAutotuneMaxEvaluations);
  gPidAutotuneHistory[slot] = {gPidAutotuneNextGains, gPidAutotuneCurrentMetrics};
  ++gPidAutotuneEvaluationIndex;
}

void queuePidAutotuneISearch() {
  static const float kIValues[kPidAutotuneIValueCount] = {0.0f, 0.005f, 0.010f, 0.020f};
  gPidAutotuneQueuedCount = 0;
  gPidAutotuneStage = AUTOTUNE_STAGE_TUNE_I;
  gPidAutotuneIIndex = 0;
  for (uint8_t i = 0; i < kPidAutotuneIValueCount; ++i) {
    gPidAutotuneQueue[gPidAutotuneQueuedCount++] =
        clampPidAutotuneCandidate({gPidAutotuneBestGains.p, kIValues[i], gPidAutotuneBestGains.d});
  }
}

// --- CMA-ES integration ---

// Enqueue one full generation of CMA-ES candidates.
void generateCmaEsCandidates() {
  cmaesSample(gCmaEs);
  gCmaEsLambdaIndex = 0;
  gPidAutotuneQueuedCount = 0;
  for (uint8_t k = 0; k < kCmaEsLambda; ++k) {
    const PidAutotuneCandidate c = clampPidAutotuneCandidate(
        {gCmaEs.population[k][0], gCmaEs.population[k][1], gCmaEs.population[k][2]});
    // Always enqueue; clamp handles the boundary, duplicates tolerated within a generation
    gPidAutotuneQueue[gPidAutotuneQueuedCount++] = c;
  }
}

// Called after every candidate in the CMAES stage finishes.
// When a full generation is done, trigger a CMA-ES update and sample the next one.
void updateCmaEsIfGenerationComplete(float candidateScore) {
  if (gCmaEsLambdaIndex < kCmaEsLambda) {
    gCmaEsGenerationScores[gCmaEsLambdaIndex] = candidateScore;
    ++gCmaEsLambdaIndex;
  }
  if (gCmaEsLambdaIndex >= kCmaEsLambda) {
    // Full generation scored — update the distribution
    cmaesUpdate(gCmaEs, gCmaEsGenerationScores);
    // Sample next generation if not converged
    if (!cmaesConverged(gCmaEs) && gPidAutotuneEvaluationIndex < kPidAutotuneMaxEvaluations) {
      generateCmaEsCandidates();
    } else {
      gPidAutotuneQueuedCount = 0;  // Signal exhaustion
    }
  }
}

void finishPidAutotuneCandidate() {
  const uint8_t trials = max<uint8_t>(1, gPidAutotuneTrial);
  gPidAutotuneCurrentMetrics.averageError /= static_cast<float>(trials);
  gPidAutotuneCurrentMetrics.averageRadius /= static_cast<float>(trials);
  gPidAutotuneCurrentMetrics.settlingTimeMs /= static_cast<float>(trials);
  gPidAutotuneCurrentMetrics.captureTimeMs /= static_cast<float>(trials);
  gPidAutotuneCurrentMetrics.score += static_cast<float>(gPidAutotuneCurrentMetrics.misses) * 80.0f;
  if (gPidAutotuneCurrentMetrics.captures == 0) {
    gPidAutotuneCurrentMetrics.score += 400.0f;
  }

  recordPidAutotuneHistory();

  if (gPidAutotuneStage == AUTOTUNE_STAGE_BASELINE) {
    gPidAutotuneBaselineMetrics = gPidAutotuneCurrentMetrics;
    gPidAutotuneHasBaseline = true;
    gPidAutotuneStage = AUTOTUNE_STAGE_CMAES;
    gPidAutotuneQueuedCount = 0;
    // Initialize CMA-ES from current gains
    cmaesInit(gCmaEs,
              gPidAutotunePreviousGains.p,
              gPidAutotunePreviousGains.i,
              gPidAutotunePreviousGains.d);
    generateCmaEsCandidates();
    return;  // Queue already loaded; fall through to CHOOSING_NEXT_CANDIDATE
  } else if (gPidAutotuneStage == AUTOTUNE_STAGE_CMAES) {
    // Update best if this candidate captures and beats baseline
    if (pidAutotuneMetricsBetter(gPidAutotuneCurrentMetrics, gPidAutotuneBaselineMetrics)) {
      if (!gPidAutotuneHasBest ||
          gPidAutotuneCurrentMetrics.score + kPidAutotuneImprovementEpsilon < gPidAutotuneBestMetrics.score) {
        gPidAutotuneBestGains = gPidAutotuneNextGains;
        gPidAutotuneBestMetrics = gPidAutotuneCurrentMetrics;
        gPidAutotuneHasBest = true;
      }
    }
    // Pass candidate score to CMA-ES generation tracker (population order maps to lambda index)
    const uint8_t lambdaSlot = static_cast<uint8_t>((gPidAutotuneEvaluationIndex - 1) % kCmaEsLambda);
    updateCmaEsIfGenerationComplete(lambdaSlot == gCmaEsLambdaIndex
                                        ? gPidAutotuneCurrentMetrics.score
                                        : gPidAutotuneCurrentMetrics.score);
    // If CMA-ES converged and we found a best, start I tuning
    if (gPidAutotuneQueuedCount == 0) {
      if (gPidAutotuneHasBest) {
        queuePidAutotuneISearch();
        return;
      } else {
        gPidAutotunePhase = AUTOTUNE_APPLYING;  // will fail in APPLYING with no best
        return;
      }
    }
  } else if (gPidAutotuneStage == AUTOTUNE_STAGE_TUNE_I) {
    if (pidAutotuneIMetricsSafe(gPidAutotuneCurrentMetrics, gPidAutotuneBestMetrics) &&
        (!gPidAutotuneHasBest ||
         gPidAutotuneCurrentMetrics.score <= gPidAutotuneBestMetrics.score + kPidAutotuneImprovementEpsilon)) {
      gPidAutotuneBestMetrics = gPidAutotuneCurrentMetrics;
      gPidAutotuneBestGains = gPidAutotuneNextGains;
      gPidAutotuneHasBest = true;
    }
    ++gPidAutotuneIIndex;
  }

  gPidAutotunePhase = AUTOTUNE_CHOOSING_NEXT_CANDIDATE;
  gPidAutotunePhaseCycles = 0;
}

void chooseNextPidAutotuneCandidate() {
  if (gPidAutotuneStage == AUTOTUNE_STAGE_TUNE_I && gPidAutotuneQueuedCount == 0) {
    gPidAutotunePhase = AUTOTUNE_APPLYING;
    return;
  }
  if (gPidAutotuneEvaluationIndex >= kPidAutotuneMaxEvaluations) {
    gPidAutotunePhase = AUTOTUNE_APPLYING;
    return;
  }
  PidAutotuneCandidate candidate{};
  if (dequeuePidAutotuneCandidate(candidate)) {
    beginPidAutotuneCandidate(candidate);
    return;
  }
  // Queue empty but not exhausted — stay in CHOOSING until it refills (handled by updateCmaEs)
  gPidAutotunePhase = AUTOTUNE_CHOOSING_NEXT_CANDIDATE;
}

void startPidAutotune() {
  if (pidAutotuneActive()) return;

  gPidAutotunePreviousGains = {gConfig.pGain, gConfig.iGain, gConfig.dGain};
  gPidAutotuneBaseSetpointX = gConfig.setpointX;
  gPidAutotuneBaseSetpointY = gConfig.setpointY;

  gPidAutotuneBestGains = gPidAutotunePreviousGains;
  gPidAutotuneNextGains = gPidAutotunePreviousGains;
  gPidAutotuneBestMetrics = {999999.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
  gPidAutotuneCurrentMetrics = {999999.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
  gPidAutotuneBaselineMetrics = {999999.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false};
  gPidAutotuneStage = AUTOTUNE_STAGE_BASELINE;
  gPidAutotuneEvaluationIndex = 0;
  gPidAutotuneTrial = 0;
  gPidAutotuneLossCycles = 0;
  gPidAutotuneSaturationCycles = 0;
  gPidAutotuneExplorationStep = 0;
  gPidAutotuneIIndex = 0;
  gPidAutotunePhaseCycles = 0;
  gPidAutotuneQueuedCount = 0;
  gPidAutotuneHasBest = false;
  gPidAutotuneHasBaseline = false;
  gPidAutotuneTrialHadBall = false;
  gCmaEsLambdaIndex = 0;
  for (uint8_t k = 0; k < kCmaEsLambda; ++k) gCmaEsGenerationScores[k] = 999999.0f;
  setPidAutotuneFailure("");
  // Seed with current gains (baseline trial)
  gPidAutotuneQueue[0] = gPidAutotunePreviousGains;
  gPidAutotuneQueuedCount = 1;
  resetClosedLoopState();
  gMode = babot::ON;
  gPidAutotunePhase = AUTOTUNE_CHOOSING_NEXT_CANDIDATE;
  gDebugLogger.tryLog("PID autotune (CMA-ES) started.");
}

void handlePidAutotune() {
  switch (gPidAutotunePhase) {
    case AUTOTUNE_WAITING_FOR_BALL_ENTRY:
      movePlatformWithMotionGrace(0.0f, 0.0f, babot::kPlateHeightMm);
      if (gBallDetected) {
        beginPidAutotuneTrial();
      }
      return;

    case AUTOTUNE_TESTING_CANDIDATE:
      ++gPidAutotunePhaseCycles;
      applyCaptureOrPidControl(gPidAutotuneBaseSetpointX, gPidAutotuneBaseSetpointY,
                               gConfig.pGain, gConfig.iGain, gConfig.dGain,
                               kPidAutotuneSafeTiltDeg, false);
      updatePidAutotuneTrialMetrics();
      if (gPidAutotuneTrialHadBall && !gBallDetected && gPidAutotunePhaseCycles > kPidAutotuneIgnoreCycles) {
        finishPidAutotuneTrial(false);
      } else if (gPidAutotuneCaptureCycles >= kPidAutotuneCaptureHoldCycles) {
        finishPidAutotuneTrial(true);
      } else if (gPidAutotunePhaseCycles >= kPidAutotuneTrialCycles) {
        finishPidAutotuneTrial(gPidAutotuneCaptureCycles > 0);
      }
      return;

    case AUTOTUNE_SCORING_TRIAL:
      if (gPidAutotuneTrial < kPidAutotuneTrialsPerCandidate) {
        gPidAutotunePhase = AUTOTUNE_WAITING_FOR_BALL_ENTRY;
        gPidAutotunePhaseCycles = 0;
        resetClosedLoopState();
        movePlatformWithMotionGrace(0.0f, 0.0f, babot::kPlateHeightMm);
      } else {
        finishPidAutotuneCandidate();
      }
      return;

    case AUTOTUNE_CHOOSING_NEXT_CANDIDATE:
      chooseNextPidAutotuneCandidate();
      return;

    case AUTOTUNE_APPLYING:
      if (!gPidAutotuneHasBest) {
        failPidAutotune("No capture-capable PID candidate found");
        return;
      }
      gConfig.pGain = gPidAutotuneBestGains.p;
      gConfig.iGain = gPidAutotuneBestGains.i;
      gConfig.dGain = gPidAutotuneBestGains.d;
      resetClosedLoopState();
      gPidAutotunePhase = AUTOTUNE_COMPLETE;
      return;

    default:
      break;
  }
}

void cancelPidAutotune(const char *reason) {
  if (gPidAutotuneHasBest) {
    gConfig.pGain = gPidAutotuneBestGains.p;
    gConfig.iGain = gPidAutotuneBestGains.i;
    gConfig.dGain = gPidAutotuneBestGains.d;
    restorePidAutotuneBaseSetpoint();
    resetClosedLoopState();
    gPidAutotunePhase = AUTOTUNE_COMPLETE;
    gDebugLogger.tryLog("Autotune cancelled. Kept best gains so far.");
  } else {
    failPidAutotune(reason);
  }
}

void restorePreviousPidGains() {
  gConfig.pGain = gPidAutotunePreviousGains.p;
  gConfig.iGain = gPidAutotunePreviousGains.i;
  gConfig.dGain = gPidAutotunePreviousGains.d;
  gPidAutotunePhase = AUTOTUNE_COMPLETE;
}
