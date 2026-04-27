#pragma once
#include <math.h>

// CMA-ES (Covariance Matrix Adaptation Evolution Strategy) for 3D PID tuning.
// Self-contained, zero-dependency implementation.
// Fits entirely in ~472 bytes of RAM.

static constexpr uint8_t kCmaEsDim = 3;          // P, I, D
static constexpr uint8_t kCmaEsLambda = 6;       // Population size (candidates per generation)
static constexpr uint8_t kCmaEsMu = 3;           // Elite survivors used for update
static constexpr float kCmaEsInitialSigma = 1.5f; // Initial step size (in normalized units)
static constexpr float kCmaEsMinSigma = 0.03f;    // Convergence threshold
static constexpr uint8_t kCmaEsMaxGenerations = 12;

// Weights for the top-mu candidates (rank-based, sum to 1)
static const float kCmaEsWeights[kCmaEsMu] = {0.5f, 0.3f, 0.2f};

struct CmaEsState {
  float mean[kCmaEsDim];      // Current distribution mean [P, I, D]
  float C[kCmaEsDim][kCmaEsDim]; // Covariance matrix
  float ps[kCmaEsDim];        // Evolution path for step size control
  float pc[kCmaEsDim];        // Evolution path for covariance
  float sigma;                // Current step size
  uint8_t generation;         // Generation counter

  // Sampled population for current generation (filled by cmaesSample)
  float population[kCmaEsLambda][kCmaEsDim];
};

// --- Internal math helpers ---

// Cholesky decomposition of a symmetric positive-definite 3x3 matrix.
// Returns lower-triangular L such that L*L^T = A. Returns true on success.
static bool cmaesCholesky3(const float A[kCmaEsDim][kCmaEsDim], float L[kCmaEsDim][kCmaEsDim]) {
  for (uint8_t i = 0; i < kCmaEsDim; ++i) {
    for (uint8_t j = 0; j <= i; ++j) {
      float sum = A[i][j];
      for (uint8_t k = 0; k < j; ++k) {
        sum -= L[i][k] * L[j][k];
      }
      if (i == j) {
        if (sum <= 0.0f) return false;
        L[i][j] = sqrtf(sum);
      } else {
        L[i][j] = (L[j][j] > 1e-12f) ? sum / L[j][j] : 0.0f;
      }
    }
    for (uint8_t j = i + 1; j < kCmaEsDim; ++j) {
      L[i][j] = 0.0f;
    }
  }
  return true;
}

// Simple Lehmer-style pseudo-random number generator (seeded from millis).
static uint32_t gCmaEsRandState = 0;

static void cmaesRandSeed(uint32_t seed) {
  gCmaEsRandState = seed ^ 0x12345678u;
  if (gCmaEsRandState == 0) gCmaEsRandState = 1;
}

static float cmaesRandN01() {
  // Box-Muller transform for standard normal samples
  gCmaEsRandState ^= gCmaEsRandState << 13;
  gCmaEsRandState ^= gCmaEsRandState >> 17;
  gCmaEsRandState ^= gCmaEsRandState << 5;
  const float u1 = (static_cast<float>(gCmaEsRandState & 0x7FFFFFFFu) + 1.0f) / 2147483649.0f;
  gCmaEsRandState ^= gCmaEsRandState << 13;
  gCmaEsRandState ^= gCmaEsRandState >> 17;
  gCmaEsRandState ^= gCmaEsRandState << 5;
  const float u2 = (static_cast<float>(gCmaEsRandState & 0x7FFFFFFFu) + 1.0f) / 2147483649.0f;
  return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
}

// --- Public API ---

void cmaesInit(CmaEsState &s, float pInit, float iInit, float dInit) {
  // Normalized coordinates: [0,1] per axis mapped to gain range.
  s.mean[0] = pInit;
  s.mean[1] = iInit;
  s.mean[2] = dInit;
  s.sigma = kCmaEsInitialSigma;
  s.generation = 0;

  // Start with identity covariance (isotropic distribution)
  for (uint8_t i = 0; i < kCmaEsDim; ++i) {
    for (uint8_t j = 0; j < kCmaEsDim; ++j) {
      s.C[i][j] = (i == j) ? 1.0f : 0.0f;
    }
    s.ps[i] = 0.0f;
    s.pc[i] = 0.0f;
  }

  cmaesRandSeed(static_cast<uint32_t>(millis()) ^ 0xDEADBEEFu);
}

// Fill s.population with kCmaEsLambda candidates sampled from N(mean, sigma^2 * C).
// Returns candidates in the original gain space (not normalized).
void cmaesSample(CmaEsState &s) {
  float L[kCmaEsDim][kCmaEsDim] = {};
  bool ok = cmaesCholesky3(s.C, L);
  if (!ok) {
    // Reset to identity if Cholesky fails (numerical issue)
    for (uint8_t i = 0; i < kCmaEsDim; ++i) {
      for (uint8_t j = 0; j < kCmaEsDim; ++j) {
        L[i][j] = (i == j) ? 1.0f : 0.0f;
      }
    }
  }

  for (uint8_t k = 0; k < kCmaEsLambda; ++k) {
    float z[kCmaEsDim];
    for (uint8_t i = 0; i < kCmaEsDim; ++i) {
      z[i] = cmaesRandN01();
    }
    // compute y = L * z
    for (uint8_t i = 0; i < kCmaEsDim; ++i) {
      float y = 0.0f;
      for (uint8_t j = 0; j <= i; ++j) {
        y += L[i][j] * z[j];
      }
      s.population[k][i] = s.mean[i] + s.sigma * y;
    }
  }
}

// Update CMA-ES state after a generation completes.
// scores[k] is the score for population[k] (lower = better).
void cmaesUpdate(CmaEsState &s, const float scores[kCmaEsLambda]) {
  // Sort indices by score (ascending = best first), simple insertion sort
  uint8_t sortedIdx[kCmaEsLambda];
  for (uint8_t i = 0; i < kCmaEsLambda; ++i) sortedIdx[i] = i;
  for (uint8_t i = 1; i < kCmaEsLambda; ++i) {
    const uint8_t key = sortedIdx[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0 && scores[sortedIdx[j]] > scores[key]) {
      sortedIdx[j + 1] = sortedIdx[j];
      --j;
    }
    sortedIdx[j + 1] = key;
  }

  // Compute weighted mean update (xmean = weighted sum of top-mu)
  float xmean[kCmaEsDim] = {};
  for (uint8_t muIdx = 0; muIdx < kCmaEsMu; ++muIdx) {
    const uint8_t k = sortedIdx[muIdx];
    for (uint8_t i = 0; i < kCmaEsDim; ++i) {
      xmean[i] += kCmaEsWeights[muIdx] * s.population[k][i];
    }
  }

  // CMA-ES step-size control constants (standard formulas for n=3, lambda=6)
  const float muW = 1.0f / (0.5f * 0.5f + 0.3f * 0.3f + 0.2f * 0.2f); // ~3.03
  const float cs = (muW + 2.0f) / (static_cast<float>(kCmaEsDim) + muW + 5.0f);
  const float ds = 1.0f + 2.0f * max(0.0f, sqrtf((muW - 1.0f) / (static_cast<float>(kCmaEsDim) + 1.0f)) - 1.0f) + cs;
  const float chiN = sqrtf(static_cast<float>(kCmaEsDim)) * (1.0f - 1.0f / (4.0f * static_cast<float>(kCmaEsDim)));
  const float cc = (4.0f + muW / static_cast<float>(kCmaEsDim)) / (static_cast<float>(kCmaEsDim) + 4.0f + 2.0f * muW / static_cast<float>(kCmaEsDim));
  const float c1 = 2.0f / ((static_cast<float>(kCmaEsDim) + 1.3f) * (static_cast<float>(kCmaEsDim) + 1.3f) + muW);
  const float cmu = min(1.0f - c1, 2.0f * (muW - 2.0f + 1.0f / muW) / ((static_cast<float>(kCmaEsDim) + 2.0f) * (static_cast<float>(kCmaEsDim) + 2.0f) + muW));

  // Step-size evolution path ps = (1-cs)*ps + sqrt(cs*(2-cs)*muW) * (xmean - mean) / sigma
  const float psCoeff = sqrtf(cs * (2.0f - cs) * muW);
  float stepDiff[kCmaEsDim];
  for (uint8_t i = 0; i < kCmaEsDim; ++i) {
    stepDiff[i] = (xmean[i] - s.mean[i]) / s.sigma;
    s.ps[i] = (1.0f - cs) * s.ps[i] + psCoeff * stepDiff[i];
  }

  // Cumulation path pc
  const float hsig = (sqrtf(s.ps[0]*s.ps[0] + s.ps[1]*s.ps[1] + s.ps[2]*s.ps[2]) /
                      sqrtf(1.0f - powf(1.0f - cs, 2.0f * (static_cast<float>(s.generation) + 1.0f))) / chiN) < 1.4f + 2.0f / (static_cast<float>(kCmaEsDim) + 1.0f) ? 1.0f : 0.0f;
  const float pcCoeff = sqrtf(cc * (2.0f - cc) * muW);
  for (uint8_t i = 0; i < kCmaEsDim; ++i) {
    s.pc[i] = (1.0f - cc) * s.pc[i] + hsig * pcCoeff * stepDiff[i];
  }

  // Covariance matrix update: C = (1-c1-cmu)*C + c1*(pc*pc^T) + cmu*(Σ w_i * Δx_i*Δx_i^T)
  for (uint8_t i = 0; i < kCmaEsDim; ++i) {
    for (uint8_t j = 0; j <= i; ++j) {
      float rankMuUpdate = 0.0f;
      for (uint8_t muIdx = 0; muIdx < kCmaEsMu; ++muIdx) {
        const uint8_t k = sortedIdx[muIdx];
        const float di = (s.population[k][i] - s.mean[i]) / s.sigma;
        const float dj = (s.population[k][j] - s.mean[j]) / s.sigma;
        rankMuUpdate += kCmaEsWeights[muIdx] * di * dj;
      }
      const float newCij = (1.0f - c1 - cmu) * s.C[i][j] + c1 * s.pc[i] * s.pc[j] + cmu * rankMuUpdate;
      s.C[i][j] = newCij;
      s.C[j][i] = newCij;
    }
  }

  // Step-size update: sigma = sigma * exp((cs/ds) * (|ps|/chiN - 1))
  const float psNorm = sqrtf(s.ps[0]*s.ps[0] + s.ps[1]*s.ps[1] + s.ps[2]*s.ps[2]);
  s.sigma *= expf((cs / ds) * (psNorm / chiN - 1.0f));
  s.sigma = constrain(s.sigma, kCmaEsMinSigma, kCmaEsInitialSigma * 3.0f);

  // Update mean
  for (uint8_t i = 0; i < kCmaEsDim; ++i) {
    s.mean[i] = xmean[i];
  }

  ++s.generation;
}

bool cmaesConverged(const CmaEsState &s) {
  return s.sigma < kCmaEsMinSigma || s.generation >= kCmaEsMaxGenerations;
}
