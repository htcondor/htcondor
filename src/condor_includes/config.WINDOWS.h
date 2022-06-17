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

#ifndef _WINDOWS_CONFIG_H_
#define _WINDOWS_CONFIG_H_

/* For all features supported under Windows explicitly set the inclusion
   macro to 1 (true) */
#define HAVE_HIBERNATION 1
#define HAVE_BACKFILL 1
#define HAVE_BOINC 1
#define HAVE_JOB_HOOKS 1
#define WITH_OPENSSL 1
#define HAVE_EXT_KRB5 1
#define HAVE_SNPRINTF 1
#define HAVE_SCHED_SETAFFINITY 1
#define HAVE_SHARED_PORT 1

#endif /* _WINDOWS_CONFIG_H_ */
