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
#include "file_lock.h"

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
