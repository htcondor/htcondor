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


#ifndef _NETWORK_UTIL_H
#define _NETWORK_UTIL_H

#if !defined(WIN32)
typedef int SOCKET;
const SOCKET INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
#endif

void socket_error(char*);
void network_init();
void network_cleanup();
SOCKET get_bound_socket();
unsigned short get_socket_port(SOCKET);
void disable_socket_inheritance(SOCKET);

#endif
