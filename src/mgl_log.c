/*
 * mgl_log.c
 * MGL
 */

#include <stdlib.h>

#include "mgl_log.h"

int mgl_log_level = -1;

int mglLogLevel(void)
{
    if (mgl_log_level < 0)
    {
        const char *env = getenv("MGL_LOG_LEVEL");

        mgl_log_level = env ? atoi(env) : MGL_LOG_ERROR;

        if (mgl_log_level < 0)
            mgl_log_level = 0;
    }

    return mgl_log_level;
}
