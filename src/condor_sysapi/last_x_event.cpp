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


#include "condor_common.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

void
sysapi_last_xevent(int delta /*=0*/)
{
	sysapi_internal_reconfig();

	time_t now = time(NULL);
	_sysapi_last_x_event = now + delta;
	if (IsDebugCategory(D_IDLE)) {
		dprintf( D_IDLE , "last_x_event set to : %lld (now=%lld)\n", (long long)_sysapi_last_x_event, (long long)now);
	}
}
