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

#ifndef CONDOR_RENICE_SELF_H
#define CONDOR_RENICE_SELF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Note:  this file is now only referenced by the starter.V5 and 
   starter.jim.  Once they change to DaemonCore, this function 
   can go away.  I moved the functionality into DaemonCore->
   CreateProcess(), but the person who calls CreateProcess has
   to do the param'ing themself.  -MEY 4-16-1999 */

int renice_self(char*);

#ifdef __cplusplus
}
#endif

#endif /* CONDOR_RENICE_SELF_H */
