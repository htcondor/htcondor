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

int get_credmon_pid();
bool credmon_poll_setup(const char* user, bool force_fresh, bool send_signal);
bool credmon_poll_continue(const char* user, int retry);
bool credmon_poll(const char* user, bool force_fresh, bool send_signal);
bool credmon_mark_creds_for_sweeping(const char* user);
void credmon_sweep_creds();
bool credmon_clear_mark(const char* user);

#endif // CREDMON_INTERFACE_H

