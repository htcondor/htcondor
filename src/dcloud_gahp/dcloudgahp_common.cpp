#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>
#include "dcloudgahp_common.h"

static pthread_mutex_t dcloudprintf_mutex = PTHREAD_MUTEX_INITIALIZER;

void dcloudprintf_internal(const char *function, const char *fmt, ...)
{
    va_list va_args;

    pthread_mutex_lock(&dcloudprintf_mutex);
    fprintf(logfp, "%s: ", function);
    va_start(va_args, fmt);
    vfprintf(logfp, fmt, va_args);
    va_end(va_args);
    fflush(logfp);
    pthread_mutex_unlock(&dcloudprintf_mutex);
}

MyString create_failure(int req_id, const char *err_msg)
{
    MyString buffer;

    buffer += req_id;
    buffer += ' ';

    buffer += err_msg;
    buffer += '\n';

    return buffer;
}

