/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
usage( char name[], int rval )
{
	fprintf( stderr, "Usage: %s [options]\n", name );
    fprintf( stderr, "   If no options are specified, print the "
			 "version and platform strings\n   where the tool was built." );
    fprintf( stderr, "  Valid options are:\n" );
    fprintf( stderr, "   -help\t(this message)\n" );
    fprintf( stderr, 
			 "   -arch\t(print the ARCH string)\n" );
    fprintf( stderr, 
			 "   -opsys\t(print the OPSYS string)\n" );
    fprintf( stderr, "   -syscall\t(get info from the libcondorsyscall.a "
			 "this Condor pool is\n        \t configured to use, not the "
			 "values where the tool was built)\n" );

	exit( rval );
}


int
main(int argc, char *argv[])
{

	myDistro->Init( argc, argv );
	CondorVersionInfo *version;

	bool use_syscall_lib = false;
	bool print_arch = false;
	bool print_opsys = false;
	bool opsys_first = false;		// stupid flag for maintaining the
									// order we saw the args in...

	for(int i = 1; i < argc; i++ ) {
		if( argv[i][0] != '-' ) {
			fprintf( stderr, "ERROR: invalid argument: '%s'\n", argv[i] );
			usage( argv[0], 1 );
		}
		switch( argv[i][1] ) {
		case 's':
			use_syscall_lib = true;
			break;
		case 'a':
			print_arch = true;
			break;
		case 'o':
			print_opsys = true;
			if( ! print_arch ) {
				opsys_first = true;
			}
			break;
		case 'h':
			usage( argv[0], 0 );
			break;
		default:
			fprintf( stderr, "ERROR: unrecognized argument: '%s'\n", argv[i] );
			usage( argv[0], 1 );
		}
	}

	char *path, *fullpath, *vername, *platform;
	if( use_syscall_lib ) {
		config();
		path = param( "LIB" );
		if( path == NULL ) {
			fprintf( stderr, "ERROR: -syscall_lib specified but 'LIB' not "
					 "defined in configuration!\n" );
			usage( argv[0], 1 );
		}
		fullpath = (char *)malloc(strlen(path) + 24);
		strcpy(fullpath, path);
		strcat(fullpath, "/libcondorsyscall.a");
				
		vername = NULL;
		vername = version->get_version_from_file(fullpath, vername);
		platform = NULL;
		platform = version->get_platform_from_file(fullpath, platform);

		version = new CondorVersionInfo(vername, NULL, platform);
		free(path);
		free(fullpath);
	} else {
		version = new CondorVersionInfo;
	}

	if( opsys_first ) {
		assert( print_opsys );
		printf("%s\n", version->getOpSysVer() );
	}
	if( print_arch ) {
		printf("%s\n", version->getArchVer() );
	}
	if( ! opsys_first && print_opsys ) {
		printf("%s\n", version->getOpSysVer() );
	}

	if( print_arch || print_opsys ) {
		return 0;
	}

	if( use_syscall_lib ) {
		printf( "%s\n%s\n", vername, platform );
	} else { 
		printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	}
	return 0;
}
