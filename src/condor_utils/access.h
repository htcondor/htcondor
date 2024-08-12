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

#ifndef __ACCESS_H__
#define __ACCESS_H__

#include "condor_io.h"
#include "condor_daemon_core.h"

const int ACCESS_READ = 0;
const int ACCESS_WRITE = 1;

int attempt_access_handler(int i, Stream *s);
int  attempt_access(const char *filename, int  mode, int uid, int gid, const char *scheddAddress = NULL );

#endif
	
