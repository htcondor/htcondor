/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_ver_info.h"

void
usage(char name[])
{
	fprintf(stderr, "Usage: %s [-syscall] [-arch] -[opsys] [-libc]\n", name);
	exit(1);
}

int
main(int argc, char *argv[])
{

	CondorVersionInfo *version;
	if (argc < 2) {
		printf("%s\n%s\n", CondorVersion(), CondorPlatform());
		exit(0);
	}

	version = new CondorVersionInfo;
	for(int i = 1; i < argc; i++ ) {

		if (argv[i][0] == '-' && argv[i][1] == 's') {
			char *path, *fullpath, *vername, *platform;
			config();
			path = param("LIB");
			if(path != NULL) {
				fullpath = (char *)malloc(strlen(path) + 24);
				strcpy(fullpath, path);
				strcat(fullpath, "/libcondorsyscall.a");
				
				vername = NULL;
				vername = version->get_version_from_file(fullpath, vername);
				platform = NULL;
				platform = version->get_platform_from_file(fullpath, platform);

				delete version;
				version = new CondorVersionInfo(vername, NULL, platform);
				free(path);
				free(fullpath);
			}

		}
		if (argv[i][0] == '-' && argv[i][1] == 'l') {
			char *libc;
			libc = version->getLibcVer();

			if(libc){
				printf("%s\n", libc );
			} else {
				printf("LIBC is not defined on this platform\n"); 
			}
		}

		if (argv[i][0] == '-' && argv[i][1] == 'a') {
			printf("%s\n", version->getArchVer() );
		}

		if (argv[i][0] == '-' && argv[i][1] == 'o') {
			printf("%s\n", version->getOpSysVer() );
		}
	}
	return 0;
}
