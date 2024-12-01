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



#ifndef __CONDOR_RW_H__
#define __CONDOR_RW_H__

#include "sock.h"

 // Returns < 0 upon an error.
int condor_read(char const *peer_description,SOCKET fd, char *buf, int sz, time_t timeout, int flags=0, bool non_blocking=false);


int condor_write(char const *peer_description,SOCKET fd, const char *buf, int sz, time_t timeout, int flags=0, bool non_blocking=false);

#endif
