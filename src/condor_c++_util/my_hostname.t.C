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
#include "my_hostname.h"


void
usage( char* me ) {
	fprintf( stderr, "Usage: %s -ip | -host | -fullhost | -sin\n", me );
	exit( 1 );
}


int
main( int argc, char* argv[] )
{
	if( argc != 2 ) {
		usage( argv[0] );
	}
	switch( argv[1][1] ) {
	case 'i':
		printf( "ip: %ul\n", my_ip_addr() );
		break;
	case 'h':
		printf( "host: %s\n", my_hostname() );
		break;
	case 'f':
		printf( "fullhost: %s\n", my_full_hostname() );
		break;
	case 's':
		printf( "sin_addr: %ul\n", (my_sin_addr())->s_addr );
		printf( "ntohl sin_addr: %ul\n", ntohl((my_sin_addr())->s_addr) );
		printf( "ntoa sin_addr: %s\n", inet_ntoa(*(my_sin_addr())) );
		break;
	default:
		usage( argv[0] );
		break;
	}
	return 0;
}
