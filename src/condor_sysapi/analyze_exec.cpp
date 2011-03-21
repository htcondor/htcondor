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
#include "condor_constants.h"
#include "condor_ver_info.h"

BEGIN_C_DECLS

/* 
	Check to see if the file is there, a regular file, and executable.
	Should be good enough as it replaces all of this OS specific junk
	we used to do here instead that opened up the binary and checked
	actual magic numbers. I can see a case where it might allow running
	of a wrong binary(suppose a linux binary on a solaris machine) but
	we can deal with that when/if it ever crops up.
*/
int
sysapi_magic_check( char *executable )
{
	struct stat buf;

	/* This should prolly do more, but for now this is good enough */

	if (stat(executable, &buf) < 0) {
		return -1;
	}
	if (!(buf.st_mode & S_IFREG)) {
		return -1;
	}

#ifndef WINDOWS
	if (!(buf.st_mode & S_IXUSR)) {
		dprintf(D_ALWAYS, "Magic check warning. Executable '%s' not "
			"executable\n", executable);
	}
#endif
	return 0;
}

/*
	Use the version object to peek into the executable and see if it has a
	valid version string. If so, chances are near 99.99% that it is an
	executable linked with the condor libraries.
*/
int
sysapi_symbol_main_check( char *executable )
{
	char *version = NULL;
	char *platform = NULL;
	CondorVersionInfo vinfo;

	version = vinfo.get_version_from_file(executable);
    if (version == NULL)
    {
        dprintf(D_ALWAYS, 
			"File '%s' is not a valid standard universe executable\n", 
			executable);
        return -1;
    }

	platform = vinfo.get_platform_from_file(executable);
    if (platform == NULL)
    {
        dprintf(D_ALWAYS, 
			"File '%s' is not a valid standard universe executable\n", 
			executable);
		free(version);
        return -1;
    }

	dprintf( D_ALWAYS,  "Executable '%s' is linked with \"%s\" on a \"%s\"\n",
		executable, version, platform?platform:"(NULL)");

	free(version);
	free(platform);

    return 0;
}

END_C_DECLS

