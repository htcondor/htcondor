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


#ifndef CREDMON_INTERFACE_H
#define CREDMON_INTERFACE_H

const int credmon_type_PWD = 0;
const int credmon_type_KRB = 1;
const int credmon_type_OAUTH = 2;
void credmon_sweep_creds(const char * cred_dir, int cred_type);
void credmon_clear_completion(int cred_type, const char * cred_dir);
bool credmon_poll_for_completion(int cred_type, const char * cred_dir, int timeout);
bool credmon_kick_and_poll_for_ccfile(int cred_type, const char * ccfile, int timeout);
bool credmon_kick(int cred_type);

// make a .mark file for the given user in the given cred_dir.  the user may end in @domain
// in which case only the part before the @ will be used to construct the .mark file
bool credmon_clear_mark(const char * cred_dir, const char* user);
bool credmon_mark_creds_for_sweeping(const char * cred_dir, const char* user, int cred_type);

#endif // CREDMON_INTERFACE_H

