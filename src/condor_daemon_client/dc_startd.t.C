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
