/*
 * mgl_log.h
 * MGL
 *
 * Set MGL_LOG_LEVEL to 0 silent, 1 errors, 2 info, 3 debug. Default is 1.
 */

#ifndef mgl_log_h
#define mgl_log_h

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MGL_LOG_SILENT = 0,
    MGL_LOG_ERROR  = 1,
    MGL_LOG_INFO   = 2,
    MGL_LOG_DEBUG  = 3
};

extern int mgl_log_level;

int mglLogLevel(void);

#define MGL_LOG_ENABLED(_lvl_) (mglLogLevel() >= (_lvl_))

#define MGL_ERR(...)    do { if (MGL_LOG_ENABLED(MGL_LOG_ERROR)) fprintf(stderr, __VA_ARGS__); } while(0)
#define MGL_INFO(...)   do { if (MGL_LOG_ENABLED(MGL_LOG_INFO))  fprintf(stderr, __VA_ARGS__); } while(0)
#define MGL_DEBUG(...)  do { if (MGL_LOG_ENABLED(MGL_LOG_DEBUG)) fprintf(stderr, __VA_ARGS__); } while(0)

#ifdef __OBJC__
#define MGL_NSERR(...)   do { if (MGL_LOG_ENABLED(MGL_LOG_ERROR)) NSLog(__VA_ARGS__); } while(0)
#define MGL_NSINFO(...)  do { if (MGL_LOG_ENABLED(MGL_LOG_INFO))  NSLog(__VA_ARGS__); } while(0)
#define MGL_NSDEBUG(...) do { if (MGL_LOG_ENABLED(MGL_LOG_DEBUG)) NSLog(__VA_ARGS__); } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* mgl_log_h */
