/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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









