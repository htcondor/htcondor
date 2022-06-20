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
#include "udp_waker.h"

#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "basename.h"
#include "condor_config.h"

/**	Preprocessor definitions */

#define DESCRIPTION "wake or hibernate a machine"

/**	Constants */

/**	Error codes */

enum {
	E_NONE	  =  0,  /* no error */
	E_NOMEM   = -1,  /* not enough memory */
	E_FOPEN   = -2,  /* cannot open file */
	E_FREAD   = -3,  /* read error on file */
	E_OPTION  = -4,  /* unknown option */
	E_OPTARG  = -5,  /* missing option argument */
	E_ARGCNT  = -6,  /* too few/many arguments */
	E_NOWAKE  = -7,  /* failed to wake machine */
	E_NOREST  = -8,  /* failed to switch the machine's power state */
	E_CLASSAD = -9,  /* error in class-ad (errno = %d) */
	E_UNKNOWN = -10  /* unknown error */
};

/**	Error messages */

static const char *errmsgs[] = {
  /* E_NONE		 0 */  "no error%s.\n",
  /* E_NOMEM    -1 */  "not enough memory for %s.\n",
  /* E_FOPEN    -2 */  "cannot open file %s: %s (errno = %d).\n",
  /* E_FREAD    -3 */  "read error on file %s.\n",
  /* E_OPTION   -4 */  "unknown option -%c.\n",
  /* E_OPTARG   -5 */  "missing option argument.\n",
  /* E_ARGCNT   -6 */  "wrong number of arguments.\n",
  /* E_NOWAKE   -7 */  "failed to send wake packet.\n",
  /* E_NOREST   -8 */  "failed to switch the machine's power state.\n",
  /* E_CLASSAD  -9 */  "error in class-ad (errno = %d).\n",
  /* E_UNKNOWN -10 */  "unknown error.\n"
};

/**	Typedefs */

/**	Global Variables */

static FILE				*in		= NULL;  /* input file */
static const char				*fn_in	= NULL;  /* name of input file */
static char				*fn_out	= NULL;  /* name of output file */
static const char		*name	= NULL;  /* program name */
static const char				*mac	= NULL;  /* hardware address */
static const char				*mask	= "0.0.0.0"; /*subnet to broadcast on*/
static int				port	= 9;	 /* port number to use */
static bool				stdio	= false; /* if true, use stdin and stdout. */
static ClassAd			*ad		= NULL;  /* machine class-ad */
static WakerBase		*waker	= NULL;  /* waking mechanism */

/**	Functions */

static void
usage( void )
{

	fprintf ( stderr, "usage: %s [OPTIONS] [INPUT-CLASSAD-FILE] [OUTPUT]\n",
			  name );
	fprintf ( stderr, "%s - %s\n", name, DESCRIPTION );
	fprintf ( stderr, "\n" );
	fprintf ( stderr, "-h      This message\n" );
	fprintf ( stderr, "\n" );
	fprintf ( stderr, "-d      Enables debugging\n" );
	fprintf ( stderr, "\n" );
	fprintf ( stderr, "-i      Read a classad that is piped in through standard input" );
	fprintf ( stderr, "        This is achieved on the command line like so: condor_power -i < [INPUT-CLASSAD-FILE]\n" );
	fprintf ( stderr, "-m      Hardware address (MAC address)\n" );
	fprintf ( stderr, "-p      Port (default: %d)\n", port );
	fprintf ( stderr, "-s      Subnet mask (default: %s)\n", mask );
	fprintf ( stderr, "\n" );

	exit ( 0 );

}

static void
enable_debug( void )
{
	dprintf_set_tool_debug("TOOL", 0);
}

static void PREFAST_NORETURN
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
		ASSERT( msg != NULL );

		fprintf ( stderr, "%s: ", name );
		va_start ( args, code );
		vfprintf ( stderr, msg, args );
		va_end ( args );

	}

	if ( in  && ( in != stdin ) ) {
		fclose ( in );
	}

	if ( waker ) {
		delete waker;
		waker = NULL;
	}

	if ( ad ) {
		delete ad;
		ad = NULL;
	}

	exit ( code );

}

static void
parse_command_line( int argc, char *argv[] )
{

	int		i, j = 0;
	char	*s;					/* to traverse the options */
	const char	**argument = NULL;	/* option argument */

	for ( i = 1; i < argc; i++ ) {

		s = argv[i];

		if ( argument ) {
			*argument = s;
			argument = NULL;
			continue;
		}

		if ( ( '-' == *s ) && *++s ) {

			/* we're looking at an option */
			while ( *s ) {

				/* determine which one it is */
				switch ( *s++ ) {
					case 'd': enable_debug ();			break;
					case 'h': usage ();					break;
					case 'i': stdio	= true;				break;
					case 'm': argument = &mac;			break;
					case 'p': port = (int) strtol ( s, NULL, port ); break;
					case 's': argument = &mask;			break;
					default : error ( E_OPTION, *--s );	break;
				}

				/* if there is an argument to this option, stash it */
				if ( argument && *s ) {
					*argument = s;
					argument = NULL;
					break;
				}

			}

		} else {

			/* we're looking at a file name */
			switch ( j++ ) {
				case  0: fn_in  = s;		 break;
				case  1: fn_out = s;		 break;
				default: error ( E_ARGCNT ); break;
			}

		}

	}

}

static void
serialize_input( void )
{

	char sinful[16 + 10];
	int  found_eof		= 0,
		 found_error	= 0,
		 empty			= 0;

	/**	Determine if we are using the command-line options, a file,
		or standard input as input */
	if ( !stdio ) {

		/**	Contrive a sinful string based on our IP address */
		sprintf ( sinful, "<255.255.255.255:1234>" );

		/**	We were give all the raw data, so we're going to create
			a fake machine ad that we will use when invoking the waking
			mechanism */
		ad = new ClassAd ();
		SetMyTypeName ( *ad, STARTD_ADTYPE );
		SetTargetTypeName ( *ad, JOB_ADTYPE );
		ad->Assign ( ATTR_HARDWARE_ADDRESS, mac );
		ad->Assign ( ATTR_SUBNET_MASK, mask );
		ad->Assign ( ATTR_MY_ADDRESS, sinful );
		ad->Assign ( ATTR_WOL_PORT, port );

	} else {

		/**	Open the machine ad file or read it from stdin */
		if ( fn_in && *fn_in ) {
			in = safe_fopen_wrapper ( fn_in, "r" );
		} else {
			in = stdin;
			fn_in = "<stdin>";
		}

		if ( !in ) {
			error ( E_FOPEN, fn_in, strerror ( errno ), errno );
		}

		/**	Serialize the machine ad from a file */
		ad = new ClassAd();
		InsertFromFile(
			in,
			*ad,
			"?$#%^&*@", /* sufficiently random garbage? */
			found_eof,
			found_error,
			empty );

		if ( found_error ) {
			error (
				E_CLASSAD,
				found_error );
		}

	}

}

static void
wake_machine( void )
{

	/**	Create the waking mechanism. */
	waker = WakerBase::createWaker ( ad );

	if ( !waker ) {
		error( E_NOMEM, "waker object." );
	}

	/**	Attempt to wake the remote machine */
	if ( !waker->doWake () ) {
		error ( E_NOWAKE );
	}

	fprintf ( stderr, "Packet sent.\n" );

	delete waker;

}

int
main( int argc, char *argv[] )
{

	/**	Retrieve the program's name */
	name = condor_basename ( argv[0] );
	if ( !name ) {
		name = argv[0];
	}

	/**	Parse the command line and populate the global state */
	parse_command_line ( argc, argv );

	/**	Grab the user's input */
	serialize_input ();

	if ( ad ) {
		wake_machine ();
	}

	return 0;

}
