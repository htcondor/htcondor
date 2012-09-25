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
//#include "conversion.h"
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

	enum Operation {
		OP_NONE = 0,
		OP_DUMP,
		OP_GET,
		OP_RENEW,
		OP_RELEASE,
		OP_DELETE,
		OP_EXPIRE
	};

	int cmdLine( int argc, const char *argv[] );
	Operation getOp( void ) const { return m_op; };

	int init( void );
	int readLeaseFile( const char *f = NULL );
	int writeLeaseFile( const char *f = NULL ) const;

	int runTest( void );
	int doDump( void );
	int doGet( void );
	int doRenew( void );
	int doRelease( void );
	int doDelete( void );
	int doExpire( void );

	list<DCLeaseManagerLease *> & getList (
		bool selected_only = false );
	list<const DCLeaseManagerLease *> & getListConst (
		bool selected_only = false ) const;

private:
	void displayLeases( const char *label = NULL, bool selected_only = false );
	const char *boolStr( bool value ) const;

	Verbosity			 m_verbose;
	Operation			 m_op;
	const char			*m_op_str;

	const char			*m_lease_file;
	bool				 m_read_required;
	bool				 m_write_file;

	const char			*m_name;
	const char			*m_pool;

	// For get operations
	const char			*m_requestor_name;
	const char			*m_requirements;
	classad::ClassAd	 m_request_ad;
	int			 		 m_request_count;

	// For get / renew operations
	int					 m_request_duration;

	// For renew / release operations
	list<const char *>	 m_lease_ids;

	// For release
	bool				 m_release_expired;
	bool				 m_release_delete;

	// Lists of leases:
	//  selected list will *always* be a subset of the main list
	list<DCLeaseManagerLease *> m_leases;
	list<DCLeaseManagerLease *> m_selected_leases;

	// The lease manager
	DCLeaseManager		*m_lm;

};

Tests::Tests( void )
{
	m_verbose = VERB_INFO;
	m_op = OP_NONE;
	m_op_str = NULL;

	m_lease_file = NULL;
	m_read_required = false;
	m_write_file = false;

	m_name = NULL;
	m_pool = NULL;
	m_lm = NULL;

	// For get operations
	m_requestor_name = "TEST";
	m_requirements = NULL;

	m_request_count = 0;
	m_request_duration = 0;

	m_release_expired = false;
	m_release_delete = false;
}

Tests::~Tests( void )
{
	if ( m_lm ) {
		delete m_lm;
	}
	DCLeaseManagerLease_freeList( m_leases );
}

int
Tests::cmdLine( int argc, const char *argv[] )
{
	const char *	usage =
		"Usage: tester [options] <lease-file> operation [parameters]\n"
		"  --name <name>: set 'daemon' name\n"
		"  --pool <name>: set pool to query\n"
		"  --debug|-d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number|name>: set verbosity level (default is ERROR)\n"
		"    names: NONE=0 ERROR WARNING INFO ERROR\n"
		"  --version: print the version number and compile date\n"
		"\n"
		"  operations: DUMP GET RENEW RELEASE RELEASE DELETE EXPIRE\n"
		"    DUMP\n"
		"    GET [options] <duration> <count>:\n"
		"      --requestor|-r <name>: set requestor name\n"
		"      --requirements <string>: set requirements\n"
		"      --set <attr> <value>: set attribute in request least\n"
		"    RENEW <duration> <lease-id> *|[lease-id ..]\n"
		"    DELETE *|<lease-id> [lease-id ..]\n"
		"    RELEASE [options] *|<lease-id> [lease-id ..]\n"
		"      -d|--delete: delete leases after releasing them\n"
		"    EXPIRE\n"
		"\n";

	int			 status = 0;
	int			 argno = 1;

	while ( (argno < argc) & (status == 0) ) {
		SimpleArg	arg( argv, argc, argno );

		if ( arg.Error() ) {
			printf("%s", usage);
			status = -1;
		}

		if (  (m_op == OP_NONE)  &&  arg.Match('d', "debug")  ) {
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
			if ( m_op != OP_GET ) {
				fprintf(stderr,
						"%s only valid for GET operation\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else if ( !arg.getOpt( m_requestor_name ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}

		} else if ( arg.Match("requirements") ) {
			const char	*req = NULL;
			if ( m_op != OP_GET ) {
				fprintf(stderr,
						"%s only valid for GET operation\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else if ( !arg.getOpt( req ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else {
				classad::ClassAdParser	parser;
				classad::ExprTree	*expr = parser.ParseExpression( req );
				if ( NULL == expr ) {
					fprintf(stderr,
							"Error parsing requirements '%s'\n", req );
					printf("%s", usage);
					status = -1;
				}
				else {
					m_request_ad.Insert( "Requirements", expr );
					m_requirements = req;
				}
			}

		} else if ( arg.Match( "set") ) {
			const char	*attr, *value;
			if ( m_op != OP_GET ) {
				fprintf(stderr,
						"%s only valid for GET operation\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else if ( !arg.getOpt( attr ) ) {
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
				m_request_ad.InsertAttr( attr, value );
			}

		} else if ( arg.Match("expired") ) {
			if ( m_op != OP_RELEASE ) {
				fprintf(stderr,
						"%s only valid for RELEASE operation\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else {
				m_release_expired = true;
			}

		} else if (  (m_op != OP_NONE)  &&  arg.Match( 'd', "delete") ) {
			if ( m_op != OP_RELEASE ) {
				fprintf(stderr,
						"%s only valid for RELEASE operation\n", arg.Arg() );
				printf("%s", usage);
				status = -1;
			}
			else {
				m_release_delete = true;
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

		} else if ( (!arg.ArgIsOpt() ) && (NULL == m_lease_file) ) {
			arg.getOpt( m_lease_file, true );

		} else if ( ! arg.ArgIsOpt() ) {
			switch( m_op ) {
			case OP_NONE:
			{
				const char	*s;
				arg.getOpt( s, true );
				if ( !strcasecmp(s, "GET" ) ) {
					m_op = OP_GET;
					m_op_str = "GET";
					m_read_required = false;
					m_write_file = true;
				}
				else if ( !strcasecmp(s, "RENEW" ) ) {
					m_op = OP_RENEW;
					m_op_str = "RENEW";
					m_read_required = true;
					m_write_file = true;
				}
				else if ( !strcasecmp(s, "RELEASE" ) ) {
					m_op = OP_RELEASE;
					m_op_str = "RELEASE";
					m_release_expired = false;
					m_release_delete = false;
					m_read_required = true;
					m_write_file = true;
				}
				else if ( !strcasecmp(s, "DUMP" ) ) {
					m_op = OP_DUMP;
					m_op_str = "DUMP";
					m_read_required = true;
					m_write_file = false;
				}
				else if ( !strcasecmp(s, "DELETE" ) ) {
					m_op = OP_DUMP;
					m_op_str = "DELETE";
					m_read_required = true;
					m_write_file = true;
				}
				else if ( !strcasecmp(s, "EXPIRE" ) ) {
					m_op = OP_EXPIRE;
					m_op_str = "EXPIRE";
					m_read_required = true;
					m_write_file = true;
				}
				else {
					fprintf(stderr, "Unknown operation '%s'\n", s );
					printf("%s", usage);
					status = -1;
				}
				break;
			}

			case OP_DUMP:
			case OP_EXPIRE:
				fprintf(stderr, "%s: no arguments allowed for %s\n",
						arg.Arg(), m_op_str );
				printf("%s", usage);
				status = -1;
				break;

			case OP_GET:
				if (  !arg.getOpt(m_request_duration, true) ) {
					fprintf(stderr, "%s: invalid/missing duration\n",
							arg.Arg() );
					printf("%s", usage);
					status = -1;
				}
				else if (  !arg.getOpt(m_request_count, true) ) {
					fprintf(stderr, "%s: invalid/missing count\n",
							arg.Arg() );
					printf("%s", usage);
					status = -1;
				}
				break;

			case OP_RENEW:
			case OP_DELETE:
				m_lease_ids.push_back( arg.getOpt() );
				break;

			case OP_RELEASE:
				if ( m_release_expired ) {
					fprintf(stderr, "%s: lease IDs not valid with %s\n",
							arg.Arg(), m_op_str );
					printf("%s", usage);
					status = -1;
				}
				else {
					m_lease_ids.push_back( arg.getOpt() );
				}
				break;

			}

		} else {
			fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
			printf("%s", usage);
			status = 1;
		}

		argno = arg.Index();
	}

	if ( OP_NONE == m_op ) {
		fprintf(stderr, "No command specified\n" );
		printf("%s", usage);
		status = 1;
	}

	return status;

}

int
Tests::init( void )
{
	int		errors = 0;

	m_lm = new DCLeaseManager( m_name, m_pool );

	int status = readLeaseFile( );
	if ( (status < 0) && m_read_required ) {
		fprintf(stderr, "Error reading lease file '%s'\n", m_lease_file );
		return -1;
	}

	// If our lease ID list is just a single '*', select them all
	if ( ( m_lease_ids.size() == 1 )  &&
		 ( !strcmp(m_lease_ids.front(),"*") )  ) {
		list<DCLeaseManagerLease *>::iterator lease_iter;
		for( lease_iter = m_leases.begin( );
			 lease_iter != m_leases.end( );
			 lease_iter++ ) {
			DCLeaseManagerLease	*lease = *lease_iter;
			m_selected_leases.push_back( lease );
		}
	}

	// Otherwise, walk through them all, find matches
	else {
		list<const char *>::iterator id_iter;
		for( id_iter  = m_lease_ids.begin( );
			 id_iter != m_lease_ids.end( );
			 id_iter++ ) {
			const char	*id = *id_iter;

			list<DCLeaseManagerLease *>::iterator lease_iter;
			bool	found = false;
			for( lease_iter = m_leases.begin( );
				 lease_iter != m_leases.end( );
				 lease_iter++ ) {
				DCLeaseManagerLease	*lease = *lease_iter;
				if ( lease->idMatch(id) ) {
					m_selected_leases.push_back( lease );
					found = true;
					break;
				}
			}
			if ( !found ) {
				fprintf( stderr, "No match for lease ID '%s' found\n", id );
				errors++;
			}
		}
	}

	return errors;
}

int
Tests::readLeaseFile( const char *f )
{
	if ( f == NULL ) {
		f = m_lease_file;
	}
	if ( f == NULL ) {
		fprintf( stderr, "No lease file\n" );
		return -1;
	}
	FILE	*fp = fopen( f, "r+b" );
	if ( NULL == fp ) {
		if ( OP_GET != m_op ) {
			fprintf( stderr, "Failed to open lease file %s\n", f );
		}
		return -1;
	}
	int count = DCLeaseManagerLease_freadList( getList(false), fp );
	fclose( fp );

	if ( count < 0 ) {
		fprintf( stderr, "Error reading lease file %s\n", f );
		return -1;
	}
	return 0;
}

int
Tests::writeLeaseFile( const char *f ) const
{
	if ( f == NULL ) {
		f = m_lease_file;
	}
	if ( f == NULL ) {
		fprintf( stderr, "No lease file\n" );
		return -1;
	}
	FILE	*fp = fopen( f, "w+b" );
	if ( NULL == fp ) {
		fprintf( stderr, "Failed to open lease file for writing %s\n", f );
		return -1;
	}
	int count = DCLeaseManagerLease_fwriteList( getListConst(false), fp );
	fclose( fp );

	if ( count < 0 ) {
		fprintf( stderr, "Error writing lease file %s\n", f );
		return -1;
	}
	return 0;
}

int
Tests::runTest( void )
{
	int		status = 0;
	switch( m_op )
	{
	case OP_DUMP:
		status = doDump( );
		break;
	case OP_GET:
		status = doGet( );
		displayLeases( );
		break;
	case OP_RENEW:
		status = doRenew( );
		displayLeases( );
		break;
	case OP_RELEASE:
		status = doRelease( );
		displayLeases( );
		break;
	case OP_DELETE:
		status = doDelete( );
		displayLeases( );
		break;
	case OP_EXPIRE:
		status = doExpire( );
		displayLeases( );
		break;
	default:
		fprintf( stderr, "OPERATION IS NONE\n" );
		return -1;
	}

	if ( ( status == 0 ) && ( m_write_file ) ) {
		status = writeLeaseFile( );
		if ( status ) {
			fprintf( stderr, "Failed to write to lease file\n" );
		}
	}

	return status;
}

int
Tests::doDump( void )
{
	displayLeases(  );
	return 0;
}

int
Tests::doGet( void )
{
	// Build the ad
	if ( m_requestor_name ) {
		m_request_ad.InsertAttr( "Name", m_requestor_name );
	}
	m_request_ad.InsertAttr( "RequestCount", m_request_count );
	m_request_ad.InsertAttr( "LeaseDuration", m_request_duration );
	if ( m_requirements ) {
		m_request_ad.InsertAttr( "Requirements", m_requirements );
	}

	if ( m_verbose >= VERB_ALL ) {
		printf( "request name: %s\n", m_name ? m_name : "<NONE>" );
		printf( "request count: %d\n", m_request_count );
		printf( "request duration: %d\n", m_request_duration );
		printf( "request requirements: %s\n",
				m_requirements ? m_requirements : "<NONE>" );
	}

	
	bool status = m_lm->getLeases( m_request_ad, getList(false) );
	if ( !status ) {
		fprintf( stderr, "Error getting leases\n" );
		return -1;
	}

	return 0;
}

int
Tests::doRenew( void )
{
	list<DCLeaseManagerLease *> renewed_list;
	bool status = m_lm->renewLeases( getListConst(true),
									 renewed_list );
	if ( !status ) {
		fprintf( stderr, "Error renewing leases\n" );
		return -1;
	}

	// Mark all renewed leass as true
	DCLeaseManagerLease_markLeases( renewed_list, true );

	// Update the renewed leases
	DCLeaseManagerLease_updateLeases(
		getList(false),
		DCLeaseManagerLease_getConstList(renewed_list) );

	return 0;
}

int
Tests::doRelease( void )
{
	list<DCLeaseManagerLease *> release_list;

	if ( m_release_expired ) {
		list<DCLeaseManagerLease *>::iterator iter;
		for( iter = m_leases.begin( );  iter != m_leases.end( );  iter++ ) {
			DCLeaseManagerLease	*lease = *iter;
			if (lease->isExpired() ) {
				release_list.push_back( lease );
			}
		}		
	}
	else {
		DCLeaseManagerLease_copyList( getList(true), release_list );
	}
	if ( !m_lm->releaseLeases( release_list ) ) {
		fprintf( stderr, "release failed\n" );
		return -1;
	}
	if ( m_release_delete ) {
		return doDelete( );
	}
	return 0;
}

int
Tests::doDelete( void )
{
	int errors;
	errors = DCLeaseManagerLease_removeLeases( getList(false),
											   getListConst(true) );
	if ( errors ) {
		fprintf( stderr, "delete failed\n" );
		return -1;
	}
	return errors ? -1 : 0;
}

int
Tests::doExpire( void )
{
	int errors;

	list<DCLeaseManagerLease *>::iterator iter;
	for( iter = m_leases.begin( );  iter != m_leases.end( );  iter++ ) {
		DCLeaseManagerLease	*lease = *iter;
		if ( lease->isDead()  ||  lease->isExpired() ) {
			m_selected_leases.push_back( lease );
		}
	}
	
	errors = DCLeaseManagerLease_removeLeases( getList(false),
											   getListConst(true) );
	if ( errors ) {
		fprintf( stderr, "expire failed\n" );
		return -1;
	}
	return errors ? -1 : 0;
}

// Show leases -- const list version
void
Tests::displayLeases( const char *label, bool selected_only )
{
	list<const DCLeaseManagerLease *> &leases = getListConst(selected_only);

	if ( label ) {
		printf( "%s: %ld leases:\n", label, leases.size() );
	}
	else {
		printf( "%ld leases:\n", leases.size() );
	}

	int		n = 0;
	list< const DCLeaseManagerLease *>::iterator iter;
	for( iter = leases.begin( );  iter != leases.end( );  iter++ ) {
		const DCLeaseManagerLease	*lease = *iter;
		classad::ClassAd			*ad = lease->leaseAd( );
		string	name;
		ad->EvaluateAttrString( "ResourceName", name );
		printf( "  LEASE %d {\n"
				"    Resource=%s\n"
				"    LeaseID=%s\n"
				"    Duration=%d\n"
				"    Remaining=%d\n"
				"    Expired=%s\n"
				"    Dead=%s\n"
				"    RLWD=%s\n"
				"  }\n",
				n++,
				name.c_str(),
				lease->leaseId().c_str(),
				lease->leaseDuration(),
				lease->secondsRemaining(),
				boolStr(lease->isExpired() ),
				boolStr(lease->isDead() ),
				boolStr(lease->releaseLeaseWhenDone() )
				);
	}
}

const char *
Tests::boolStr( bool value ) const
{
	return value ? "TRUE" : "FALSE";
}

list<DCLeaseManagerLease *> &
Tests::getList( bool selected_only )
{
	if ( selected_only ) {
		return m_selected_leases;
	}
	else {
		return m_leases;
	}
}

list<const DCLeaseManagerLease *> &
Tests::getListConst( bool selected_only ) const
{
	if ( selected_only ) {
		return DCLeaseManagerLease_getConstList( m_selected_leases );
	}
	else {
		return DCLeaseManagerLease_getConstList( m_leases );
	}
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

	Tests	tests;
	int		status;
	status = tests.cmdLine( argc, argv );
	if ( status < 0 ) {
		fprintf(stderr, "Error processing command line\n" );
		exit( 1 );
	}
	else if ( status > 0 ) {
		exit( 0 );
	}

	status = tests.init( );
	if ( status < 0 ) {
		fprintf(stderr, "Test initialization failed\n" );
		exit( 1 );
	}

	status = tests.runTest( );
	if ( status < 0 ) {
		fprintf(stderr, "Test failed\n" );
		exit( 1 );
	}

	exit( 0 );
}
