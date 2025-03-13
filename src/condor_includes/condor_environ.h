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

#ifndef _CONDOR_ENVIRON_H
#define _CONDOR_ENVIRON_H

#define ENV_CONDOR_INHERIT      "CONDOR_INHERIT"
#define ENV_CONDOR_PRIVATE      "CONDOR_PRIVATE_INHERIT"
#define ENV_CONDOR_STACKSIZE    "CONDOR_STACK_SIZE"
#define ENV_CONDOR_CORESIZE     "CONDOR_CORESIZE"
#define ENV_CONDOR_UG_IDS       "CONDOR_IDS"
#define ENV_CONDOR_ID           "CONDOR_ID"
#define ENV_CONDOR_PARENT_ID    "CONDOR_PARENT_ID"
#define ENV_CONDOR_CONFIG       "CONDOR_CONFIG"
#define ENV_CONDOR_CONFIG_ROOT  "CONDOR_CONFIG_ROOT"
#define ENV_CONDOR_SECRET       "CONDOR_DAEMON_PROVIDED_SECRET"

#define ENV_GZIP                "GZIP"
#define ENV_PATH                "PATH"
#define ENV_DAEMON_DEATHTIME    "DAEMON_DEATHTIME"
#define ENV_PARALLEL_SHADOW_SINFUL "PARALLEL_SHADOW_SINFUL"
#define ENV_RENDEZVOUS_DIRECTORY   "RENDEZVOUS_DIRECTORY"

#endif /* _CONDOR_ENVIRON_H */
