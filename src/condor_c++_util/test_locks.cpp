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
#include "file_lock.h"

int open_file( const char *name );

int
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
			lock = new FileLock(fd, NULL, arg);
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

	return 0;
}

int
open_file( const char *name )
{
	int		fd;

	fd = safe_open_wrapper( name, O_RDWR | O_CREAT, 0664 );
	if( fd < 0 ) {
		perror( name );
		exit( 1 );
	}
	return fd;
}
