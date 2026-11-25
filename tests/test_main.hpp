// Minimal test harness: no dependencies. CHECK_NEAR(a, b, tol) records failures; main returns nonzero if any.
#pragma once
#include <cmath>
#include <cstdio>

inline int g_failures = 0;
inline int g_checks = 0;

#define CHECK_NEAR(a, b, tol)                                                                   \
    do {                                                                                        \
        double _a = (a), _b = (b);                                                              \
        ++g_checks;                                                                             \
        if (!(std::fabs(_a - _b) <= (tol))) {                                                   \
            ++g_failures;                                                                       \
            std::printf("FAIL %s:%d  %s = %g, expected %g ± %g\n", __FILE__, __LINE__, #a, _a,  \
                        _b, (double)(tol));                                                     \
        }                                                                                       \
    } while (0)
