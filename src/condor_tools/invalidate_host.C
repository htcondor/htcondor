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


 

/***********************************************************************

 Invalidate the collector's ClassAds for the given host

 Originally written on 8/18/00 by Derek Wright <wright@cs.wisc.edu>

 TODO:
 1) Support for multiple hosts
 2) Support for a -pool option
 3) More general code cleanup/polish, etc. (e.g. splitting main up
     into some functions. *grin*  

***********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "get_full_hostname.h"
#include "daemon.h"
#include "condor_attributes.h"
#include "subsystem_info.h"

char	*MyName;
DECL_SUBSYSTEM( "TOOL", SUBSYSTEM_TYPE_TOOL );

void
usage( void )
{
	fprintf( stderr, "Usage: %s [host]\n", MyName );
	exit( 1 );
}

int
main( int argc, char** argv ) 
{
	char* host;
	ClassAd invalidate_ad;
	char line[256];
	Daemon Collector( DT_COLLECTOR);
	ReliSock* coll_sock;

	config();

	MyName = argv[0];
	if( argc < 2 ) {
		usage();
	}
	
	if( ! argv[1] ) {
		usage();
	}
	host = get_full_hostname( argv[1] );
	if( ! host ) {
		fprintf( stderr, "%s: Unknown host %s\n", MyName, argv[1] );
		exit( 1 );
	}

	printf( "Trying %s\n", host );

	if( ! Collector.locate() ) {
		fprintf( stderr, "%s: ERROR, can't locate Central Manager!\n", 
				 MyName );
		exit( 1 );
	}

		// Set the correct types
    invalidate_ad.SetMyTypeName( QUERY_ADTYPE );
    invalidate_ad.SetTargetTypeName( STARTD_ADTYPE );

        // We only want to invalidate this slot.
    sprintf( line, "%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_MACHINE,  
             host );
    invalidate_ad.Insert( line );


	coll_sock = (ReliSock*)Collector.startCommand( INVALIDATE_STARTD_ADS );
	if( ! coll_sock ) {
		fprintf( stderr, "%s: ERROR: can't create TCP socket to %s\n", 
				 MyName, Collector.fullHostname() );
		exit( 1 );
	}

	coll_sock->encode();

	if( ! invalidate_ad.put(*coll_sock) ) {
		fprintf( stderr, "%s: ERROR: can't send classad!\n", MyName ); 
		delete coll_sock;
		exit( 1 );
	}

	if( ! coll_sock->end_of_message() ) {
		fprintf( stderr, "%s: ERROR: can't send EOM!\n", MyName ); 
		delete coll_sock;
		exit( 1 );
	}

	delete coll_sock;
	printf( "%s: Sent invalidate ad for %s to %s\n", MyName, host,
			Collector.fullHostname() );
	exit( 0 );
}
