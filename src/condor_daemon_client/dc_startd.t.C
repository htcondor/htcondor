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
#include "dc_startd.h"

int sendCmd( const char* job_id, const char* name, const char* pool );

int
main( int argc, char* argv[] )
{
	config();
	switch( argc ) {
	case 2:
		return sendCmd( argv[1], NULL, NULL );
		break;

	case 3:
		return sendCmd( argv[1], argv[2], NULL );
		break;

	case 4:
		return sendCmd( argv[1], argv[2], argv[3] );
		break;

	default:
		fprintf( stderr, "Usage: %s global_job_id [startd_name] [pool]\n",
				 argv[0] );
		break;
	}
	return 1;
}

int
sendCmd( const char* job_id, const char* name, const char* pool )
{
	DCStartd d( name, pool );
	if( ! d.locate() ) {
		fprintf( stderr, "Can't locate startd: %s\n", d.error() );
		return 1;
	}

	ClassAd reply;
	if( ! d.locateStarter(job_id, &reply) ) {
		fprintf( stderr, "locateStarter() failed: %s\n", d.error() );
		fprintf( stderr, "reply ad:\n" );
		reply.fPrint( stderr );
		return 1;
	}

	printf( "locateStarter() worked!\n" );
	printf( "reply ad:\n" );
	reply.fPrint( stdout );
	return 0;
}
