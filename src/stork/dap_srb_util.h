#ifndef _SRB_UTIL_H_
#define _SRB_UTIL_H_

#if defined(__cplusplus)
extern "C" {
#endif

#if 0
// old unused code
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include "srbClient.h"

char * get_from_result_struct(mdasC_sql_result_struct *result,char *att_name);

extern void write_log_basic(char *request_id, char *targetfilename, char *logfilename, char *status);
#endif

const char * srbResource(void);

#if defined(__cplusplus)
}
#endif

#endif









