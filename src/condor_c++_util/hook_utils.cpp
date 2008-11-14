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
#include "hook_utils.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "directory.h"


char*
validateHookPath( const char* hook_param )
{
	char* tmp = param(hook_param);
	if (tmp) {
		StatInfo si(tmp);
		if (si.Error() != SIGood) {
			int si_errno = si.Errno();
			dprintf(D_ALWAYS, "ERROR: invalid path specified for %s (%s): "
					"stat() failed with errno %d (%s)\n",
					hook_param, tmp, si_errno, strerror(si_errno));
			free(tmp);
			return NULL;
		}
#if !defined(WIN32)
		mode_t mode = si.GetMode();
		if (mode & S_IWOTH) {
			dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
					"is world-writable! Refusing to use.\n",
					hook_param, tmp);
			free(tmp);
			return NULL;
		}
#endif
		if (!si.IsExecutable()) {
			dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
					"is not executable.\n", hook_param, tmp);
			free(tmp);
			return NULL;
		}
			// TODO: forbid symlinks, too?
		
			// Now, make sure the parent directory isn't world-writable.
#if !defined(WIN32)
		StatInfo dir_si(si.DirPath());
		mode_t dir_mode = dir_si.GetMode();
		if (dir_mode & S_IWOTH) {
			dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
					"is a world-writable directory (%s)! Refusing to use.\n",
					hook_param, tmp, si.DirPath());
			free(tmp);
			return NULL;
		}
#endif
	}

		// If we got this far, we've either got a valid hook or it
		// wasn't defined. Either way, we can just return that directly.
	return tmp;
}
