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
#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "directory.h"
#include <sys/stat.h>

static char *_FileName_ = __FILE__;

time_t dev_idle_time( const char *path, time_t current_time );

time_t
tty_pty_idle_time()
{
	char	*f;
	static Directory *dev;
	char	pathname[ _POSIX_PATH_MAX ];
	time_t	idle_time;
	time_t	current_time;
	time_t	answer = INT_MAX;

	if( !dev ) {
		dev = new Directory( "/dev" );
	}
	current_time = time( 0 );

	for( dev->Rewind();  f = dev->Next(); ) {
		if( strncmp("tty",f,3) == MATCH || strncmp("pty",f,3) == MATCH ) {
			sprintf( pathname, "/dev/%s", f );
			idle_time = dev_idle_time( pathname, current_time );
			dprintf( D_FULLDEBUG, "%s: %d secs\n", pathname, idle_time );
			if( idle_time < answer ) {
				answer = idle_time;
			}
		}
	}
	return answer;
}


time_t
dev_idle_time( const char *path, time_t current_time )
{
	struct stat	buf;
	time_t	answer;

	if( stat(path,&buf) < 0 ) {
		EXCEPT( "stat" );
	}

	dprintf( D_FULLDEBUG,  "%s %s", path, ctime(&buf.st_atime) );
	answer =  current_time - buf.st_atime;
	if( answer < 0 ) {
		return 0;
	} else {
		return answer;
	}
}
