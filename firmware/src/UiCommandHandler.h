#pragma once


void applyModeSwitching(ButtonPress press) {
  if (gMode != babot::CALIBRATION) {
    if (press == SINGLE_PRESS && gMode != babot::ASSEMBLY) {
      setMode(gMode == babot::ON ? babot::OFF : babot::ON);
    } else if (press == QUAD_PRESS) {
      setMode(babot::ASSEMBLY);
    } else if (press == TRIPLE_PRESS) {
      setMode(babot::CALIBRATION);
    }
  }
}

void compensateMuxIndividual() {
  const babot::BaBotMode previousMode = gMode;
  gMode = babot::OFF;
  resetClosedLoopState();
  movePlatformWithMotionGrace(0.0f, 0.0f, babot::kPlateHeightMm);
  updateSubsystemStatus("irSensors",
                        gSystemStatus.irSensors,
                        babot::BOOTING,
                        "Calibrating individual MUX channel compensation. Remove the ball.");

  const bool saved = gIrSensors.calibrateMuxCompensation(babot::kMuxCompensationSamples);
  updateSubsystemStatus("irSensors",
                        gSystemStatus.irSensors,
                        saved ? babot::READY : babot::DEGRADED,
                        saved ? "Individual MUX compensation saved to NVS."
                              : "Failed to save individual MUX compensation.",
                        !saved);
  gDebugLogger.tryLog(saved ? "Individual MUX compensation saved."
                            : "Individual MUX compensation failed.");
  gMode = previousMode == babot::ON ? babot::OFF : previousMode;
}

void applyRuntimePatch(const babot::UiCommand &command) {
  if ((command.fieldsMask & babot::kFieldPGain) != 0u) {
    gConfig.pGain = constrain(command.config.pGain, 0.0f, 100.0f);
  }
  if ((command.fieldsMask & babot::kFieldIGain) != 0u) {
    gConfig.iGain = constrain(command.config.iGain, 0.0f, 100.0f);
  }
  if ((command.fieldsMask & babot::kFieldDGain) != 0u) {
    gConfig.dGain = constrain(command.config.dGain, 0.0f, 200.0f);
  }
  if ((command.fieldsMask & babot::kFieldSetpointX) != 0u) {
    gConfig.setpointX = constrain(command.config.setpointX, babot::kCenterXMin, babot::kCenterXMax);
  }
  if ((command.fieldsMask & babot::kFieldSetpointY) != 0u) {
    gConfig.setpointY = constrain(command.config.setpointY, babot::kCenterYMin, babot::kCenterYMax);
  }
  if ((command.fieldsMask & babot::kFieldBallThreshold) != 0u) {
    gConfig.ballThreshold = constrain(command.config.ballThreshold, 0.0f, 4095.0f);
  }
  if ((command.fieldsMask & babot::kFieldDigipotTap) != 0u) {
    gConfig.digipotTap = constrain(static_cast<int>(command.config.digipotTap), 0, babot::kDigitalPotMaxTap);
  }
}

void applyUiCommand(const babot::UiCommand &command) {
  switch (command.type) {
    case babot::UI_PATCH_CONFIG:
      applyRuntimePatch(command);
      break;

    case babot::UI_SET_MODE:
      setMode(command.mode);
      break;

    case babot::UI_SAVE_RUNTIME:
      if (gSharedState.saveRuntimeConfig(gConfig)) {
        updateSubsystemStatus("storage", gSystemStatus.storage, babot::READY, "Runtime configuration saved to NVS.");
      } else {
        updateSubsystemStatus("storage",
                              gSystemStatus.storage,
                              babot::DEGRADED,
                              "Failed to save runtime configuration to NVS.",
                              true);
      }
      break;

    case babot::UI_LOAD_RUNTIME: {
      babot::RuntimeConfig stored = babot::defaultRuntimeConfig();
      const bool loaded = gSharedState.loadRuntimeConfig(stored);
      gConfig.pGain = stored.pGain;
      gConfig.iGain = stored.iGain;
      gConfig.dGain = stored.dGain;
      gConfig.setpointX = stored.setpointX;
      gConfig.setpointY = stored.setpointY;
      gConfig.ballThreshold = stored.ballThreshold;
      gConfig.digipotTap = stored.digipotTap;
      updateSubsystemStatus("storage",
                            gSystemStatus.storage,
                            loaded ? babot::READY : babot::DEGRADED,
                            loaded ? "Runtime configuration reloaded from NVS."
                                   : "Runtime configuration unavailable; using defaults.",
                            !loaded);
      break;
    }

    case babot::UI_SAVE_CALIBRATION:
      if (!gPlatform.motionAvailable()) {
        setSystemAlertWithLog("Calibration save is unavailable while actuator output is disabled.");
      } else if (gMode == babot::OFF || gMode == babot::CALIBRATION) {
        gPlatform.saveServoOffsets();
      }
      break;

    case babot::UI_RESET_DEFAULTS: {
      const babot::RuntimeConfig defaults = gSharedState.resetRuntimeConfig();
      gConfig.pGain = defaults.pGain;
      gConfig.iGain = defaults.iGain;
      gConfig.dGain = defaults.dGain;
      gConfig.setpointX = defaults.setpointX;
      gConfig.setpointY = defaults.setpointY;
      gConfig.ballThreshold = defaults.ballThreshold;
      gConfig.digipotTap = defaults.digipotTap;
      updateSubsystemStatus("storage", gSystemStatus.storage, babot::READY, "Runtime configuration reset to defaults.");
      break;
    }

    case babot::UI_COMPENSATE_MUX:
      compensateMuxIndividual();
      break;

    case babot::UI_SET_TEST_SERVO:
      gSelectedTestServo = command.testServo % 3;
      break;

    case babot::UI_START_PID_AUTOTUNE:
      startPidAutotune();
      break;

    case babot::UI_CANCEL_PID_AUTOTUNE:
      cancelPidAutotune("Canceled");
      break;

    case babot::UI_RESTORE_PREVIOUS_PID:
      restorePreviousPidGains();
      break;
  }
}

void drainUiCommands() {
  babot::UiCommand command{};
  while (gSharedState.dequeueCommand(command)) {
    applyUiCommand(command);
  }
}
