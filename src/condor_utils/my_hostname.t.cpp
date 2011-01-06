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
