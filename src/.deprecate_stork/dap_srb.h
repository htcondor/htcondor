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

// Wrapper to include SRB client header file, with appropriate #defines, and C
// linkage.

// The following must be defined prior to including srbClient.h:
#if defined LINUX
#define PORTNAME_linux
#elif defined SOLARIS
#define PORTNAME_solaris
#else
#error Need to add new PORTNAME mapping here
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include "srbClient.h"

#if defined(__cplusplus)
}
#endif

