#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "directory.h"
#include <sys/stat.h>

static char *_FileName_ = __FILE__;

time_t dev_idle_time( const char *path, time_t current_time );
extern "C" time_t tty_pty_idle_time();


extern "C" {
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
