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
#include "basename.h"


bool validateHookPath( const char* hook_param, char*& hpath )
{
    hpath = NULL;
	char* tmp = param(hook_param);

    // an undefined hook parameter is not an error
    if (NULL == tmp) return true;

	struct stat si = {};
	if (stat(tmp, &si) != 0) {
		dprintf(D_ALWAYS, "ERROR: invalid path specified for %s (%s): "
				"stat() failed with errno %d (%s)\n",
				hook_param, tmp, errno, strerror(errno));
		free(tmp);
		return false;
	}

#if !defined(WIN32)
	mode_t mode = si.st_mode;
	if (mode & S_IWOTH) {
		dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
				"is world-writable! Refusing to use.\n",
				hook_param, tmp);
		free(tmp);
		return false;
	}

	if (!(si.st_mode & S_IEXEC)) {
		dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
				"is not executable.\n", hook_param, tmp);
		free(tmp);
		return false;
	}
#endif

	// TODO: forbid symlinks, too?
	
	// Now, make sure the parent directory isn't world-writable.
#if !defined(WIN32)
	std::string dir_path = condor_dirname(tmp);
	struct stat dir_si = {};
	stat(dir_path.c_str(), &dir_si);
	mode_t dir_mode = dir_si.st_mode;
	if (dir_mode & S_IWOTH) {
		dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
				"is a world-writable directory! Refusing to use.\n",
				hook_param, tmp);
		free(tmp);
		return false;
	}
#endif

    // If the hook parameter was defined and the path passes inspection,
    // return with success.
    hpath = tmp;
	return true;
}
