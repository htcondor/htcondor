/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
	char *version;
	char *platform;
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

    return 0;
}

END_C_DECLS

