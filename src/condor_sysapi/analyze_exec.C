/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 
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
	if (!(buf.st_mode & S_IXUSR)) {
		dprintf(D_ALWAYS, "Magic check warning. Executable '%s' not "
			"executable\n", executable);
	}

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
        return -1;
    }

	dprintf( D_ALWAYS,  "Executable '%s' is linked with \"%s\" on a \"%s\"\n",
		executable, version, platform?platform:"(NULL)");

	free(platform);

    return 0;
}

END_C_DECLS

