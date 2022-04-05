/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"

#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "basename.h"
#include "hibernator.h"
#include "simple_arg.h"
#include "condor_config.h"

#if defined ( WIN32 )
#  include "hibernator.WINDOWS.h"
#elif defined ( LINUX )
#  include "hibernator.linux.h"
#endif


/**	Preprocessor definitions */

#define DESCRIPTION "hibernate a machine"

/**	Constants */

/**	Error codes */

enum
{
	E_NONE	  =  0,  /* no error */
	E_NOMEM   = -1,  /* not enough memory */
	E_INIT    = -2,  /* Initialization error */
	E_STATES  = -3,  /* Error getting available states */
	E_OPTION  = -4,  /* unknown option */
	E_OPTARG  = -5,  /* missing option argument */
	E_ARGCNT  = -6,  /* too few/many arguments */
	E_NOREST  = -7,  /* failed to switch the machine's power state */
	E_CLASSAD = -8,  /* error in class-ad (errno = %d) */
	E_UNSUPPORTED = -9, /* OS not supported */
	E_UNKNOWN = -10   /* unknown error */
};

/**	Error messages */

static const char *errmsgs[] = {
  /* E_NONE		 0 */  "no error%s.\n",
  /* E_NOMEM    -1 */  "not enough memory for %s.\n",
  /* E_INIT     -2 */  "Initialization error.\n",
  /* E_STATES   -3 */  "error getting available states.\n",
  /* E_OPTION   -4 */  "unknown option -%c.\n",
  /* E_OPTARG   -5 */  "missing option argument to %s.\n",
  /* E_ARGCNT   -6 */  "wrong number of arguments.\n",
  /* E_NOREST   -7 */  "failed to switch the machine's power state.\n",
  /* E_CLASSAD  -8 */  "error in class-ad (errno = %d).\n",
  /* E_UNSUPPORTED = -9 */ "condor_power() not implemented for this OS.\n",
  /* E_UNKNOWN  -10 */  "unknown error.\n"
};

/**	Typedefs */

enum RunMode
{
	MODE_NONE = 0,
	MODE_AD,
	MODE_SET
};

typedef HibernatorBase::SLEEP_STATE state_t;

/**	Global Variables */

static const char		*name		= NULL; 	 // program name
static const char		*state		= NULL;		 // hibernation state
static const char		*method		= NULL;		 // Hibernation method
static RunMode			run_mode	= MODE_NONE; // Run mode
static HibernatorBase	*hibernator	= NULL; 	 // hibernation mechanism

/**	Functions */

static void
usage( bool error = true )
{
	FILE	*out = error ? stderr : stdout;
	fprintf( out, "usage: %s [OPTIONS] (ad|set <level>)\n", name );
	fprintf( out, "%s - %s\n", name, DESCRIPTION );
	fprintf( out, "-h|--help            This message\n" );
	fprintf( out, "-d|--debug           Enables debugging\n" );
	fprintf( out, "-m|--method <method> Specify Linux hibernation method\n" );
	fprintf( out, "                     /sys, /proc, pm-utils\n" );
	fprintf( out, "ad                   Output capabilities ad\n" );
	fprintf( out, "set <state>          Switch to <state>\n" );

	if ( ! error ) {
		exit ( 0 );
	}
}

static void
enable_debug( void )
{
	dprintf_set_tool_debug("TOOL", 0);
}

static void
cleanup( void )
{
	if ( hibernator ) {
		delete hibernator;
		hibernator = NULL;
	}
}

static void
error( int code, ... )
{
	va_list	args;
	const char *msg;

	assert ( name );

	if ( code < E_UNKNOWN ) {
		code = E_UNKNOWN;
	}

	if ( code < 0 ) {

		msg = errmsgs[-code];
		if ( !msg ) {
			msg = errmsgs[-E_UNKNOWN];
		}
		assert ( msg != NULL );

		fprintf ( stderr, "%s: ", name );
		va_start ( args, code );
		vfprintf ( stderr, msg, args );
		va_end ( args );

	}

	cleanup();

	exit ( code );
}

static void
parse_command_line( int argc, const char *argv[] )
{
	int		argno = 1;
	while ( argno < argc ) {
		SimpleArg	arg( argv, argc, argno );

		if ( arg.Error() ) {
			usage();
			error( E_OPTION, arg.Arg() );
		}

		if ( arg.Match('d', "debug") ) {
			enable_debug( );
		}
		else if ( arg.Match('h', "help") ) {
			usage( false );
		}
		else if ( arg.Match( 'm', "method" ) ) {
			if ( !arg.getOpt( method ) ) {
				usage();
				error( E_OPTARG, arg.Arg() );
			}
		}
		else if ( arg.fixedMatch("ad") ) {
			run_mode = MODE_AD;
		}
		else if ( arg.fixedMatch("set") ) {
			run_mode = MODE_SET;
			if ( !arg.getOpt( state ) ) {
				usage();
				error( E_OPTARG, arg.Arg() );
			}
		}
		else {
			usage();
			error( E_OPTION, arg.Arg() );
		}
		argno = arg.Index();
	}
}

static void
hibernate_machine(void)
{

	bool	ok		= false;
	state_t	desired	= HibernatorBase::stringToSleepState( state );
	state_t	actual;


	/**	Try to put this machine into the requested power state. */
	dprintf( D_FULLDEBUG,
			 "Switching to state %s\n",
			 HibernatorBase::sleepStateToString(desired) );
	ok = hibernator->switchToState ( desired, actual );

	if ( (!ok)  ||  (desired != actual) ) {
		error( E_NOREST );
	}

	fprintf ( stderr, "Power level change.\n" );

}

static void
dump_ad( void )
{
	unsigned	mask = hibernator->getStates();
	std::string	states;
	if ( !HibernatorBase::maskToString(mask, states) ) {
		error( E_STATES );
	}
	ClassAd	ad;
	ad.Assign( ATTR_HIBERNATION_METHOD, hibernator->getMethod() );
	ad.Assign( ATTR_HIBERNATION_SUPPORTED_STATES, states );
	ad.Assign( ATTR_HIBERNATION_RAW_MASK, mask );
	fPrintAd( stdout, ad );
}

int
main( int argc, const char *argv[] )
{
	/**	Retrieve the program's name */
	name = condor_basename ( argv[0] );
	if ( !name ) {
		name = argv[0];
	}

	/**	Parse the command line and populate the global state */
	parse_command_line ( argc, argv );


	/**	Create the hibernation mechanism. */
# if ( HIBERNATOR_TYPE_DEFINED )
	hibernator = new RealHibernator;
# else
	error( E_UNSUPPORTED );
# endif
	if ( !hibernator ) {
		error( E_NOMEM, "hibernator object." );
	}
	if ( method ) {
		hibernator->setMethod( method );
	}
	if ( !hibernator->initialize() ) {
		error( E_INIT );
	}

	if ( MODE_AD == run_mode ) {
		dump_ad( );
	}
	else if ( MODE_SET == run_mode ) {
		hibernate_machine( );
	}

	delete hibernator;

	return 0;

}
