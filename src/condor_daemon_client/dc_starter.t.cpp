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
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "daemon.h"
#include "dc_starter.h"

int sendCmd( const char* starter_addr );


int
main( int argc, char* argv[] )
{
	config();
	switch( argc ) {
	case 2:
		return sendCmd( argv[1] );
		break;

	default:
		fprintf( stderr, "Usage: %s starter_address\n",
				 argv[0] );
		break;
	}
	return 1;
}

int
sendCmd( const char* starter_addr )
{
	DCStarter d( starter_addr );

	ClassAd req;
	ClassAd reply;
	ReliSock rsock;
	
	if( ! d.reconnect(&req, &reply, &rsock) ) {
		fprintf( stderr, "command failed: (%s) %s\n",
				 getCAResultString(d.errorCode()), d.error() );
	}

	printf( "reply ad:\n" );
	reply.fPrint( stdout );

	return 0;
}
