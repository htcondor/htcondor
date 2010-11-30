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

#ifndef CONDOR_GET_DAEMON_NAME_H
#define CONDOR_GET_DAEMON_NAME_H

#include "daemon_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char* get_daemon_name(const char* name);
extern const char* get_host_part(const char* name);
extern char* build_valid_daemon_name(const char* name);
extern char* default_daemon_name( void );

#ifdef __cplusplus
}
#endif

#endif /* CONDOR_GET_DAEMON_NAME_H */
