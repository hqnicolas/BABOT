# BaBot - Kinematics Theory and System Geometry

## 1. Introduction to the Stewart mechanism
The BaBot platform utilizes a 3-DOF (Degrees of Freedom) parallel manipulator based on the Stewart-Gough platform principle. This architecture provides high mechanical rigidity and precise control over the platform's posture, specifically its height ($Z$), roll ($\alpha$), and pitch ($\beta$).

Unlike serial manipulators where errors accumulate along the linkage chain, the parallel arrangement of the BaBot distributes the load and precision across three independent actuator branches (Servos A, B, and C).

## 2. Geometric Constants
The system is defined by five critical physical lengths, as extracted from the legacy robot design:

| Constant | Symbol | Value | Description |
|---|---|---|---|
| `kBaseRadiusMm` | $L$ | $85.0$ mm | Radial distance from the origin to each servo anchor point. |
| `kPlatformRadiusMm` | $R_p$ | $103.92$ mm | Radial distance from the platform center to each linkage anchor. |
| `kServoArmLengthMm` | $r$ | $70.0$ mm | Length of the primary servo horn arm. |
| `kPassiveLinkLengthMm` | $l$ | $85.0$ mm | Length of the passive linkage rod. |
| `kPlateHeightMm` | $d$ | $115.0$ mm | Nominal vertical height of the platform center at rest. |

## 3. Inverse Kinematics (IK)
The Inverse Kinematics problem for the BaBot is: *Given a desired platform height $Z$ and tilt angles $\alpha$ (roll) and $\beta$ (pitch), calculate the required angles $\theta_i$ for each servo motor.*

### 3.1. Reference Frame and Coordinate Mapping
The origin $(0,0,0)$ is located at the geometric center of the base triangle. The three servo anchors are distributed at $120^\circ$ intervals:
- **Servo A**: $210^\circ$ (or $-150^\circ$)
- **Servo B**: $90^\circ$
- **Servo C**: $-30^\circ$ (or $330^\circ$)

### 3.2. Platform Anchor Positions
For a given roll ($\alpha$) and pitch ($\beta$), we define a rotation matrix or use vector rotation to find the position of the three anchor points on the moving plate. The firmware uses a simplified geometric projection since the BaBot typically operates at small tilt angles ($<20^\circ$), but it maintains precision by calculating the absolute 3D position of each anchor relative to its respective base servo anchor.

### 3.3. The Individual Actuator Equation
For each leg, the distance between the servo rotation axis and the platform anchor must be bridged by the combination of the servo arm ($r$) and the passive link ($l$). 

We solve for the leg distance $d_{leg}$ in the vertical action plane of the servo:
$$d_{leg}^2 = \Delta X^2 + \Delta Y^2 + \Delta Z^2$$

Applying the Law of Cosines to the triangle formed by the servo arm $r$, the link $l$, and the distance $d_{leg}$:
$$\cos(\phi) = \frac{r^2 + d_{leg}^2 - l^2}{2 \cdot r \cdot d_{leg}}$$
The final servo angle $\theta$ is then derived from the combination of the geometric elevation of the leg and the internal angle $\phi$:
$$\theta = \text{atan2}(\Delta Z, \Delta X_{planar}) - \phi$$

The ESP32 processes these equations in real-time, allowing for fluid motion without the need for pre-calculated lookup tables.

## 4. Operational Limits and Clamping
Due to the physical lengths of the rods, there is a maximum theoretical height the platform can reach. For the current hardware dimensions:
- **Horizontal Reach Needed**: $\approx 19$ mm (difference between platform and base radii).
- **Max Theoretical Height**: $\approx 154$ mm (at full vertical extension).
- **Practical Default**: $115$ mm, providing a neutral servo angle of $\approx 34.3^\circ$, optimized for maximum symmetrical tilt authority.

The firmware enforces a software clamp on the tilt angles to prevent the servos from reaching "singularity" points where the linkage could flip or lock.
