/*
 * mgl_test.h
 * MGL
 *
 * Minimal test framework. Tests register themselves with a constructor, so
 * adding a file to the build is all it takes to add tests.
 */

#ifndef mgl_test_h
#define mgl_test_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MGLTestFn)(void);

typedef struct MGLTestCase {
    const char *suite;
    const char *name;
    MGLTestFn   fn;
    int         needs_gpu;
    struct MGLTestCase *next;
} MGLTestCase;

void mgl_test_register(MGLTestCase *tc);

// per-test state, owned by the runner
extern int  mgl_test_failures;
extern int  mgl_test_checks;
extern int  mgl_test_skipped;
extern const char *mgl_test_current;

void mgl_test_fail(const char *file, int line, const char *fmt, ...);
void mgl_test_skip(const char *reason);
int  mgl_test_is_skipped(void);

#define MGL_TEST_IMPL(suite_, name_, gpu_)                                    \
    static void mgl_test_##suite_##_##name_(void);                            \
    static MGLTestCase mgl_tc_##suite_##_##name_ = {                          \
        #suite_, #name_, mgl_test_##suite_##_##name_, gpu_, NULL };           \
    __attribute__((constructor))                                              \
    static void mgl_reg_##suite_##_##name_(void) {                            \
        mgl_test_register(&mgl_tc_##suite_##_##name_);                        \
    }                                                                         \
    static void mgl_test_##suite_##_##name_(void)

// TEST runs on CPU only; GPU_TEST needs a live Metal context
#define TEST(suite_, name_)     MGL_TEST_IMPL(suite_, name_, 0)
#define GPU_TEST(suite_, name_) MGL_TEST_IMPL(suite_, name_, 1)

#define CHECK(cond)                                                           \
    do { mgl_test_checks++;                                                   \
         if (!(cond)) mgl_test_fail(__FILE__, __LINE__, "CHECK(%s)", #cond);  \
    } while (0)

#define CHECK_MSG(cond, ...)                                                  \
    do { mgl_test_checks++;                                                   \
         if (!(cond)) mgl_test_fail(__FILE__, __LINE__, __VA_ARGS__);         \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                    \
    do { mgl_test_checks++;                                                   \
         long long _a = (long long)(a), _b = (long long)(b);                  \
         if (_a != _b) mgl_test_fail(__FILE__, __LINE__,                      \
            "%s == %s : got %lld, want %lld", #a, #b, _a, _b);                \
    } while (0)

#define CHECK_EQ_UINT(a, b)                                                   \
    do { mgl_test_checks++;                                                   \
         unsigned long long _a = (unsigned long long)(a);                     \
         unsigned long long _b = (unsigned long long)(b);                     \
         if (_a != _b) mgl_test_fail(__FILE__, __LINE__,                      \
            "%s == %s : got %llu (0x%llx), want %llu (0x%llx)",               \
            #a, #b, _a, _a, _b, _b);                                          \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                 \
    do { mgl_test_checks++;                                                   \
         double _a = (double)(a), _b = (double)(b), _t = (double)(tol);       \
         if (!(fabs(_a - _b) <= _t)) mgl_test_fail(__FILE__, __LINE__,        \
            "%s ~= %s : got %g, want %g (tol %g)", #a, #b, _a, _b, _t);       \
    } while (0)

#define CHECK_STR_EQ(a, b)                                                    \
    do { mgl_test_checks++;                                                   \
         const char *_a = (a), *_b = (b);                                     \
         if (!_a || !_b || strcmp(_a, _b) != 0) mgl_test_fail(__FILE__,       \
            __LINE__, "%s == %s : got \"%s\", want \"%s\"", #a, #b,           \
            _a ? _a : "(null)", _b ? _b : "(null)");                          \
    } while (0)

#define SKIP(reason) do { mgl_test_skip(reason); return; } while (0)

#ifdef __cplusplus
}
#endif

#endif /* mgl_test_h */
