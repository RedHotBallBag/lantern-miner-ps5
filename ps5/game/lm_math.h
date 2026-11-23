/* Small libm replacements (the console app has no libm in libc.a): floor, sin, cos, exp, log, pow, atan2-free. */
#pragma once

#include <stdint.h>
#include <string.h>

#define LM_PI 3.14159265358979323846
#define LM_TAU 6.28318530717958647692

static inline double lm_floor(double x)
{
    if (x >= 4503599627370496.0 || x <= -4503599627370496.0)
        return x;
    long long i = (long long)x;
    return (double)((double)i > x ? i - 1 : i);
}

static inline double lm_ceil(double x) { return -lm_floor(-x); }
static inline double lm_fabs(double x) { return x < 0 ? -x : x; }
static inline double lm_min(double a, double b) { return a < b ? a : b; }
static inline double lm_max(double a, double b) { return a > b ? a : b; }
static inline double lm_clamp(double x, double lo, double hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline double lm_sqrt(double x) { return __builtin_sqrt(x); }
static inline double lm_hypot(double a, double b) { return __builtin_sqrt(a * a + b * b); }
/* JS Math.round */
static inline double lm_round(double x) { return lm_floor(x + 0.5); }
/* JS-style remainder (sign of the dividend) */
static inline double lm_fmod(double a, double b)
{
    double q = a / b;
    q = q < 0 ? lm_ceil(q) : lm_floor(q);
    return a - q * b;
}

static inline double lm_sin(double x)
{
    /* reduce to [-pi, pi] */
    x = x - LM_TAU * lm_floor(x / LM_TAU + 0.5);
    /* fold to [-pi/2, pi/2] */
    if (x > LM_PI / 2) x = LM_PI - x;
    else if (x < -LM_PI / 2) x = -LM_PI - x;
    double x2 = x * x;
    return x * (1.0 + x2 * (-1.0 / 6 + x2 * (1.0 / 120 + x2 * (-1.0 / 5040 + x2 * (1.0 / 362880 + x2 * (-1.0 / 39916800 + x2 * (1.0 / 6227020800.0)))))));
}

static inline double lm_cos(double x) { return lm_sin(x + LM_PI / 2); }

static inline double lm_exp(double x)
{
    if (x > 700) x = 700;
    if (x < -700) return 0.0;
    double k = lm_floor(x * 1.4426950408889634 + 0.5);
    double f = x - k * 0.6931471805599453; /* |f| <= ln2/2 */
    double p = 1.0 + f * (1.0 + f * (1.0 / 2 + f * (1.0 / 6 + f * (1.0 / 24 + f * (1.0 / 120 + f * (1.0 / 720 + f * (1.0 / 5040 + f * (1.0 / 40320))))))));
    int64_t bits = (int64_t)((int)k + 1023) << 52;
    double s;
    memcpy(&s, &bits, 8);
    return p * s;
}

static inline double lm_log(double x)
{
    if (x <= 0) return -745.0;
    int64_t bits;
    memcpy(&bits, &x, 8);
    int e = (int)((bits >> 52) & 0x7FF) - 1023;
    bits = (bits & 0x000FFFFFFFFFFFFFLL) | 0x3FF0000000000000LL;
    double m;
    memcpy(&m, &bits, 8);
    if (m > 1.4142135623730951) { m *= 0.5; e++; }
    double s = (m - 1.0) / (m + 1.0), s2 = s * s;
    double l = 2.0 * s * (1.0 + s2 * (1.0 / 3 + s2 * (1.0 / 5 + s2 * (1.0 / 7 + s2 * (1.0 / 9 + s2 * (1.0 / 11))))));
    return l + e * 0.6931471805599453;
}

static inline double lm_pow(double a, double b) { return lm_exp(b * lm_log(a)); }
