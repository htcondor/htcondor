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


#ifndef __UTIL_LIB_PROTO_H
#define __UTIL_LIB_PROTO_H

#include "condor_config.h"
#include "condor_getmnt.h"
#include "condor_header_features.h"

#if defined(__cplusplus)
extern "C" {
#endif

char* getExecPath( void );

int rotate_file(const char *old_filename, const char *new_filename);
int rotate_file_dprintf(const char *old_filename, const char *new_filename, int calledByDprintf);

/// If new_filename exists, overwrite it.
int copy_file(const char *old_filename, const char *new_filename);
int hardlink_or_copy_file(const char *old_filename, const char *new_filename);

//Only daemon_core_main need this
void detach ( void );

#if defined(__cplusplus)
}		/* End of extern "C" declaration */
#endif

#endif
