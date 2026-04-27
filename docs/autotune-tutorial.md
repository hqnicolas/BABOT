# Tutorial: Testing the BABOT Multi-Stage PID Autotuner

This guide explains how to verify the performance and stability of the BABOT Hill Climbing autotuner.

## Prerequisites
1. **Physical Setup**: Ensure the platform is level and the servos are energized.
2. **Web UI Access**: Navigate to the BABOT Web UI in your browser.
3. **Ball on Standby**: Have the balancing ball ready to place on the plate.

---

## Step 1: Baseline Verification
The autotuner starts by measuring your "current" gains to establish a reference point.

1. Open the **PID Autotune** drawer in the Web UI.
2. Click **Start Autotune**.
3. **Action**: Place the ball on the plate when the status shows "Waiting for ball entry".
4. **Observation**: The system will run 3 trials. After completion, notice the **Baseline Score**. This is your target to beat.

## Step 2: Monitoring the Evolution Phase
The system will now enter **Evolving (CMA-ES)** mode. This is the machine learning phase where BABOT learns the physics of your plate.

1. **Trial Flow**: 
   - The system samples a "population" of 6 candidates per generation.
   - You will see the **Generation** counter (0, 1, 2...).
   - Watch the **Step Size (σ)**. As the system gains confidence in a specific region of PID space, σ will decrease.
2. **Telemetry Check**: 
   - The system automatically shifts its search toward gain combinations that produce lower scores.
   - If a generation fails to find a better set, it will expand its search (increasing σ) to find new stable regions.

## Step 3: Integral Tuning
Once a solid P/D foundation is found, the system switches to **Tuning I**.

1. **Observation**: You will see the status change to "Tuning I".
2. **Logic**: The system is now testing small increments of Integral gain (e.g., 0.005, 0.010).
3. **Criteria**: It looks for the I-gain that reduces steady-state error without making the ball "orbit" the center slowly.

## Step 4: Applying and Saving
1. **Completion**: Once the queue is empty, the status will show **Complete**.
2. **Verification**: The best gains found are automatically applied to the active controller.
3. **Persistence**: If you are happy with the performance, click **Save Runtime** in the Config drawer to write these gains to the ESP32's permanent memory (NVS).

> [!TIP]
> **Don't wait for completion?** If you are satisfied with how the ball is balancing during a trial, you can click **Cancel** at any time. The system will automatically apply the **Best Found Gains** it has recorded so far, allowing you to stop the evolution early once you see a result you like!

---

## Troubleshooting the Test
- **"Waiting for ball entry" persists**: Check your `Ball Threshold` and `Digipot Tap`. The ball must be clearly detected for a trial to start.
- **Tuner stays on one candidate**: Ensure you are not removing the ball too early. The tuner needs 3 consistent trials to move on.
- **Ball pinned against the fence**: If a candidate just pushes the ball to the edge and holds it there, it will be heavily penalized by the **Edge Penalty** (300 points). This ensures the autotuner only considers center-balancing behavior as "Better".
- **Instability**: If the ball is shaking violently, the "Best" gains might be too high for your specific linkage. You can always click **Apply Previous Gains** to revert.

## How to "Stress Test" the Result
After autotuning, switch the mode to **POSITION_TEST**. This will inject a circular synthetic ball position. If the autotune was successful, the plate should follow the simulated circular path smoothly with minimal overshoot.
