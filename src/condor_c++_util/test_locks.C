#define _POSIX_SOURCE
#include <iostream.h>
#include <stdio.h>

#if 0
#include "types.h"
#include "file_lock.h"
#else
#include "condor_common.h"
#include "condor_constants.h"
#include "file_lock.h"
#endif

int open_file( const char *name );

void
main( int argc, char *argv[] )
{
	char	cmd [512];
	char	arg [512];
	FileLock	*lock;
	int		fd;


	for(;;) {

		if( lock ) {
			lock->display();
		}

		cout << "Cmd: ";
		cin >> cmd;
		if( strcmp( cmd, "quit" ) == 0 ) {
			exit( 0 );
		}
		// cout << "Cmd = '" << cmd << "' Arg = '" << arg << "'" << endl;

		if( strcmp(cmd,"file") == 0 ) {
			cin >> arg;
			fd = open_file( arg );
			delete lock;
			lock = new FileLock(fd);
			continue;
		}

		if( strcmp(cmd,"read") == 0 ) {
			if( lock->obtain(READ_LOCK) ) {
				cout << "OK\n";
			} else {
				cout << "Can't\n";
			}
			continue;
		}

		if( strcmp(cmd,"write") == 0 ) {
			if( lock->obtain(WRITE_LOCK) ) {
				cout << "OK\n";
			} else {
				cout << "Can't\n";
			}
			continue;
		}

		if( strcmp(cmd,"unlock") == 0 ) {
			if( lock->obtain(UN_LOCK) ) {
				cout << "OK\n";
			} else {
				cout << "Can't\n";
			}
			continue;
		}

		if( strcmp(cmd,"release") == 0 ) {
			if( lock->release() ) {
				cout << "OK\n";
			} else {
				cout << "Can't\n";
			}
			continue;
		}



		cout << "Unknown command\n";
	}
}

int
open_file( const char *name )
{
	int		fd;

	fd = open( name, O_RDWR | O_CREAT, 0664 );
	if( fd < 0 ) {
		perror( name );
		exit( 1 );
	}
	return fd;
}
