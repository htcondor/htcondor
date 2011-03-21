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


#ifndef _NAMED_PIPE_UTIL_H
#define _NAMED_PIPE_UTIL_H

// make a pathname for a named pipe that includes the
// given prefix, PID, and serial number
//
char* named_pipe_make_client_addr(const char*, pid_t, int);

// make a pathname for a "watchdog server" named pipe
//
char* named_pipe_make_watchdog_addr(const char*);

// a helper method to create a named pipe node in
// the file system, and return a read and a write FDs
// for the pipe
//
bool named_pipe_create(const char*, int&, int&);

#endif
