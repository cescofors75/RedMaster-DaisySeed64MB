/* =====================================================================
 *  dsp_common.h  --  Shared DSP utilities for the procedural drum engines
 * ---------------------------------------------------------------------
 *  Clamp / FastTanh / Rng (Xoshiro32**) / SVF were byte-identical copies
 *  hand-maintained separately in tr505.h, tr808.h and tr909.h. Factored
 *  out here so a future fix/tweak only has to happen once.
 *
 *  NOT included from sh101.h / tb303.h / fm2op.h: those engines' local
 *  Clamp/FastTanh look similar but are not identical (no input clamp
 *  before the Pade tanh approximation), and are left untouched rather
 *  than silently normalized to a different behavior.
 *
 *  Deliberately NOT shared: VelCurve. TR505/TR909 use a smoothstep
 *  curve (v*v*(3-2v)); TR808 uses a logarithmic curve instead
 *  (log(1+9v)/log(10)) for its characteristic soft-hit response — that
 *  is a real per-engine difference, not duplication, so each engine
 *  keeps its own VelCurve.
 *
 *  Global scope (not namespaced): included only from within each
 *  engine's own `namespace TR5xx/TR8xx/TR9xx { ... }` block, so
 *  unqualified calls (e.g. `Clamp(...)` from inside SVF::SetCoefs)
 *  resolve here via normal enclosing-scope lookup. sh101.h/tb303.h/
 *  fm2op.h each keep their own namespaced Clamp/FastTanh, so there is
 *  no collision with this header's global-scope versions.
 * ===================================================================== */
#pragma once
#include <math.h>
#include <stdint.h>

#ifndef RED_DSP_TWOPI
#define RED_DSP_TWOPI 6.283185307179586f
#endif

static inline float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* tanh(x) aproximacion racional Pade 3/3
 * Error < 0.4% en [-4,4]  ~3x mas rapido que tanhf() en Cortex-M */
static inline float FastTanh(float x) {
    x = Clamp(x, -3.0f, 3.0f);          // evita distorsion por overflow Pade
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/* ---------------------------------------------------------------------
 *  Xoshiro32** PRNG
 *  Mejor distribucion espectral que el Xorshift simple de v1.0
 *  Evita artefactos tonales en el metallic noise del hihat/cymbal
 * --------------------------------------------------------------------- */
struct Rng {
    uint32_t s[4];

    void Seed(uint32_t seed) {
        for (int i = 0; i < 4; i++) {
            seed += 0x9e3779b9u;
            uint32_t z = seed;
            z = (z ^ (z >> 16)) * 0x85ebca6bu;
            z = (z ^ (z >> 13)) * 0xc2b2ae35u;
            s[i] = z ^ (z >> 16);
        }
    }

    uint32_t Next() {
        const uint32_t result = s[0] + s[3];
        const uint32_t t = s[1] << 9;
        s[2] ^= s[0]; s[3] ^= s[1];
        s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t;
        s[3] = (s[3] << 11) | (s[3] >> 21);
        return result;
    }

    float White() {
        return ((float)(int32_t)Next()) * (1.0f / 2147483648.0f);
    }
};

/* ---------------------------------------------------------------------
 *  SVF -- State Variable Filter 2 polos
 *  Topologia Andy Simper (Cytomic): numericamente estable
 *
 *  REGLA DE ORO: llamar SetCoefs() en Trigger() o Init()
 *  NUNCA en Process() -- eso era el principal problema de v1.0
 * --------------------------------------------------------------------- */
struct SVF {
    float g  = 0.0f;
    float k  = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float ic1 = 0.0f;
    float ic2 = 0.0f;

    void SetCoefs(float sr, float fc, float Q) {
        g  = tanf(RED_DSP_TWOPI * Clamp(fc, 10.0f, sr * 0.49f) / (2.0f * sr));
        k  = 1.0f / Clamp(Q, 0.5f, 40.0f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void Reset() { ic1 = ic2 = 0.0f; }

    float ProcessLP(float v0) {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    float ProcessBP(float v0) {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v1;
    }

    float ProcessHP(float v0) {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v0 - k * v1 - v2;
    }
};
