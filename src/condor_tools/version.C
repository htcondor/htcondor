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
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "condor_distribution.h"

void
usage(char name[])
{
	fprintf(stderr, "Usage: %s [-syscall] [-arch] -[opsys]\n", name);
	exit(1);
}

int
main(int argc, char *argv[])
{

	myDistro->Init( argc, argv );
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
		if (argv[i][0] == '-' && argv[i][1] == 'a') {
			printf("%s\n", version->getArchVer() );
		}

		if (argv[i][0] == '-' && argv[i][1] == 'o') {
			printf("%s\n", version->getOpSysVer() );
		}
	}
	return 0;
}
