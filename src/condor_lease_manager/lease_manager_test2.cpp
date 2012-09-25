/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "basename.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "subsystem_info.h"
#include "dc_lease_manager.h"
#include "simple_arg.h"

#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "conversion.h"
using namespace std;

#include <list>

static const char *	VERSION = "0.1.0";

enum Verbosity
{
	VERB_NONE = 0,
	VERB_ERROR,
	VERB_WARNING,
	VERB_INFO,
	VERB_ALL
};

class Tests
{
public:
	Tests( void );
	~Tests( void );

	int CmdLine( int argc, const char *argv[] );
	int Run( );
	void Shutdown( void );

private:
	Verbosity	 m_verbose;

	const char	*m_name;
	const char	*m_pool;

	classad::ClassAd	m_base_ad;

	const char	*m_requestor_name;
	const char	*m_requirements;
	int			 m_request_count;
	int			 m_request_duration;

	int			 m_loops;
	int			 m_max_leases;
	bool		 m_exit;
	int			 m_sleep;

	bool		 m_shutdown;
};

Tests::Tests( void )
{
	m_verbose = VERB_INFO;

	m_name = NULL;
	m_pool = NULL;

	m_requestor_name = "TEST";
	m_requirements = NULL;
	m_request_count = 1;
	m_request_duration = 90;

	m_loops = 1;
	m_max_leases = 1;
	m_exit = false;
	m_sleep = 30;

	m_shutdown = false;
}

Tests::~Tests( void )
{
}

int
Tests::CmdLine( int argc, const char *argv[] )
{
	const char *	usage =
		"Usage: test_log_reader [options]\n"
		"  --name <name>: set 'daemon' name\n"
		"  --pool <name>: set pool to query\n"
		"\n"
		"  --requestor|-r <name>: set requestor name\n"
		"  --requirements <string>: set requirements\n"
		"  --leases|-n <number>: set number of leases / loop\n"
		"  --duration <number>: set requested lease duration\n"
		"  --set <attr> <value>: set attribute in request least\n"
		"\n"
		"  --loops <number>: set # of loops to run\n"
		"  --max-leases|-m <number>: set total max # of leases\n"
		"  --sleep <number>: seconds to sleep between loops\n"
		"  --exit|-x: Exit when no leases available\n"
		"\n"
		"  --debug|-d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number|name>: set verbosity level (default is ERROR)\n"
		"    names: NONE=0 ERROR WARNING INFO ERROR\n"
		"  --version: print the version number and compile date\n";

	int	status = 0;

	int		argno = 1;
	while ( (argno < argc) & (status == 0) ) {
		SimpleArg	arg( argv, argc, argno );

		if ( arg.Error() ) {
			printf("%s", usage);
			status = -1;
		}

		if ( arg.Match('d', "debug") ) {
			if ( arg.hasOpt() ) {
				set_debug_flags( const_cast<char *>(arg.getOpt()), 0 );
				argno = arg.ConsumeOpt( );
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match("name") ) {
			if ( !arg.getOpt( m_name ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match("pool") ) {
			if ( !arg.getOpt( m_pool ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match('r', "requestor") ) {
			if ( !arg.getOpt( m_requestor_name ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match("requirements") ) {
			if ( !arg.getOpt( m_requirements ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match('n', "leases") ) {
			if ( !arg.getOpt( m_request_count ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match("duration") ) {
			if ( !arg.getOpt( m_request_duration ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match("loops") ) {
			if ( !arg.getOpt( m_loops ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match('m', "max-leases") ) {
			if ( !arg.getOpt( m_max_leases ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match( "set") ) {
			const char	*attr, *value;
			if ( !arg.getOpt( attr ) ) {
				fprintf(stderr, "Attribute needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else if ( !arg.getOpt( value ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else {
				m_base_ad.InsertAttr( attr, value );
			}

		} else if ( arg.Match( 'x', "exit") ) {
			m_exit = true;

		} else if ( arg.Match("sleep") ) {
			if ( !arg.getOpt( m_sleep ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( ( arg.Match("usage") )		||
					( arg.Match('h') )			||
					( arg.Match("help") )  )	{
			printf("%s", usage);
			status = 1;

		} else if ( arg.Match('v') ) {
			int		v = (int) m_verbose;
			m_verbose = (Verbosity) (v + 1);

		} else if ( arg.Match("verbosity") ) {
			if ( arg.isOptInt() ) {
				int		verb;
				arg.getOpt(verb);
				m_verbose = (Verbosity) verb;
			}
			else if ( arg.hasOpt() ) {
				const char	*s;
				arg.getOpt( s );
				if ( !strcasecmp(s, "NONE" ) ) {
					m_verbose = VERB_NONE;
				}
				else if ( !strcasecmp(s, "ERROR" ) ) {
					m_verbose = VERB_ERROR;
				}
				else if ( !strcasecmp(s, "WARNING" ) ) {
					m_verbose = VERB_WARNING;
				}
				else if ( !strcasecmp(s, "INFO" ) ) {
					m_verbose = VERB_INFO;
				}
				else if ( !strcasecmp(s, "ALL" ) ) {
					m_verbose = VERB_ALL;
				}
				else {
					fprintf(stderr, "Unknown %s '%s'\n", arg.Arg(), s );
					printf("%s", usage);
					status = -1;
				}
			}
			else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match("version") ) {
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
			status = 1;

		} else {
			fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
			printf("%s", usage);
			status = 1;
		}

		argno = arg.Index();
	}

	return status;

}

void
Tests::Shutdown( void )
{
	m_shutdown = true;
}

int
Tests::Run( void )
{
	// Build the ad
	m_base_ad.InsertAttr( "Name", m_requestor_name );
	m_base_ad.InsertAttr( "LeaseDuration", m_request_duration );

	if ( m_requirements ) {
		classad::ClassAdParser	parser;
		classad::ExprTree	*expr = parser.ParseExpression( m_requirements );
		m_base_ad.Insert( "Requirements", expr );
	}

	// Instantiate the list of leases
	list< DCLeaseManagerLease *> leases;

	// Instantiate the lease manager
	DCLeaseManager	lm( m_name, m_pool );

	classad::ClassAd	ad;

	printf( "name: %s\n", m_name );
	printf( "count: %d\n", m_request_count );
	printf( "duration: %d\n", m_request_duration );
	printf( "leases: %d\n", m_max_leases );
	printf( "req: %s\n", m_requirements );

	// Loop for a while...
	time_t	now = time( NULL );
	time_t	starttime = now;
	for( int loop = 0;  (!m_shutdown) && (loop < m_loops);  loop++ ) {

		// Go through the list of leases, release RLWD ones, renew the others
		list<DCLeaseManagerLease *> renew_list;
		list<const DCLeaseManagerLease *> release_list;
		for( list< DCLeaseManagerLease *>::iterator iter = leases.begin( );
			 iter != leases.end( );
			 iter++ ) {
			DCLeaseManagerLease	*lease = *iter;
			if ( lease->ReleaseLeaseWhenDone() ) {
				release_list.push_back( lease );
			} else if ( lease->LeaseRemaining( now ) < 60 ) {
				if ( random() % 5 ) {
					renew_list.push_back( lease );
				} else {
					release_list.push_back( lease );
				}
			}
		}

		// Release 'em
		if ( release_list.size() ) {
			printf( "Releasing %d leases\n", release_list.size() );
			double	t1 = dtime();
			if ( !lm->releaseLeases( release_list ) ) {
				printf( "Release failed!!!\n" );
			} else {
				dump( "release", release_list.size(), t1 );
			}
			DCLeaseManagerLease_RemoveLeases( leases, release_list );
		}

		// Renew leases
		if ( renew_list.size() ) {
			printf( "Renew %d leases\n", renew_list.size() );
			list<DCLeaseManagerLease *> renewed_list;

			// Mark all of the leases to renew as false, all others as true
			DCLeaseManagerLease_MarkLeases( leases, true );
			DCLeaseManagerLease_MarkLeases( renew_list, false );

			// Now, do the renew
			double	t1 = dtime();
			if ( !lm->renewLeases(*DCLeaseManagerLease_GetConstList(&renew_list),
								  renewed_list ) ) {
				printf( "Renew failed!!!\n" );
			} else {
				dump( "renew", renew_list.size(), t1 );

				// Mark all renewed leass as true
				DCLeaseManagerLease_MarkLeases( renewed_list, true );

				// Update the renewed leases
				DCLeaseManagerLease_UpdateLeases(
					leases, *DCLeaseManagerLease_GetConstList(&renewed_list) );

				// Remove the leases that are still marked as false
				list<const DCLeaseManagerLease *> remove_list;
				int count = DCLeaseManagerLease_GetMarkedLeases(
					*DCLeaseManagerLease_GetConstList( &leases ),
					false, remove_list );
				if ( count ) {
					show_leases( "non-renewed", remove_list );
					printf( "Removing the %d marked leases\n", count );
					DCLeaseManagerLease_RemoveLeases( leases, remove_list );
				}
			}

			// Finally, remove the renewed leases themselves
			DCLeaseManagerLease_FreeList( renewed_list );
		}

		// Get more leases
		if ( leases.size() < max_leases ) {
			int		num = max_leases - leases.size();
			if ( num > lease_req_count ) {
				num = lease_req_count;
			}
			printf( "Requesting %d leases\n", num );
			ad.InsertAttr( "RequestCount", num );

			// Get more leases
			int		oldsize = leases.size();
			double	t1 = dtime();
			if ( !lm->getLeases( ad, leases ) ) {
				fprintf( stderr, "Error getting matches:\n" );
				lm->display( stderr );
			}
			dump( "request", leases.size() - oldsize, t1 );
		}
		show_leases( "current", leases );
	}

	if ( leases.size() ) {
		printf( "Releasing leases\n" );
		double	t1 = dtime();
		if ( !lm->releaseLeases( *DCLeaseManagerLease_GetConstList(&leases))) {
			fprintf( stderr, "release failed\n" );
		}
		dump( "release", leases.size(), t1 );
	}

	// This
	DCLeaseManagerLease	a;
	leases.push_back( &a );
	leases.remove( &a );

	return 0;


}


// Simple term signal handler
static Tests	tests;
void handle_sig(int /*sig*/ )
{
	printf( "Got signal; shutting down\n" );
	tests.Shutdown( );
}

int
main(int argc, const char **argv)
{
	set_debug_flags(NULL, D_ALWAYS);

	set_mySubSystem( "TEST_LEASE_MANAGER", SUBSYSTEM_TYPE_TOOL );

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	dprintf_set_tool_debug("TEST_LEASE_MANAGER", 0);

	int		status;
	status = tests.CmdLine( argc, argv );
	if ( status < 0 ) {
		fprintf(stderr, "Error processing command line\n" );
		exit( 1 );
	}
	else if ( status > 0 ) {
		exit( 0 );
	}

	// Install handlers
	signal( SIGINT, handle_sig );
	signal( SIGTERM, handle_sig );

	status = Tests.Run( );

	exit( 0 );
}
