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
