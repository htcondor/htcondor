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
#include "condor_config.h"
#include "condor_privsep.h"
#include "condor_daemon_core.h"

// PrivSep on Windows: check back in a little while...

bool
privsep_enabled()
{
	if (param_boolean("PRIVSEP_ENABLED", false)) {
		EXCEPT("PRIVSEP_ENABLED not supported on this platform");
	}
	return false;
}

int
privsep_spawn_procd(const char*, ArgList&, int[3], int)
{
	return FALSE;
}

bool
privsep_remove_dir(const char* /*pathname*/)
{
	EXCEPT("privsep_remove_dir() not supported on this platform");
	return false;
}
