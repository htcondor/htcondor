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

// This module contains fdpass_send() and fdpass_recv(), a pair of utility
// functions for passing file descriptors over UNIX domain sockets.

#ifndef _FDPASS_H
#define _FDPASS_H

#if defined(__cplusplus)
extern "C" {
#endif

int fdpass_send(int uds_fd, int fd);
int fdpass_recv(int uds_fd);

#if defined(__cplusplus)
}
#endif

#endif
