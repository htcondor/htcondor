#ifndef DCLOUDGAHP_COMMON_H
#define DCLOUDGAHP_COMMON_H

#include "MyString.h"

#define dcloudprintf(fmt, ...) dcloudprintf_internal(__FUNCTION__, fmt, ## __VA_ARGS__)
void dcloudprintf_internal(const char *function, const char *fmt, ...);

#define STRCASEEQ(a,b) (strcasecmp(a,b) == 0)

#define NULLSTRING "NULL"

extern FILE *logfp;

MyString create_failure(int req_id, const char *err_msg);

#endif
