/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "hibernator.WINDOWS.h"

#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "basename.h"

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
  /* E_NOWAKE   -7 */  "failed to sent wake packet.\n",
  /* E_NOREST   -8 */  "failed to switch the machine's power state.\n",
  /* E_CLASSAD  -9 */  "error in class-ad (errno = %d).\n",
  /* E_UNKNOWN -10 */  "unknown error.\n"
};

/**	Typedefs */

typedef HibernatorBase::SLEEP_STATE state_t;

/**	Global Variables */

static FILE				*in				= NULL;  /* input file */
static FILE				*out			= NULL;  /* output file */
static char				*fn_in			= NULL;  /* name of input file */
static char				*fn_out			= NULL;  /* name of output file */
static const char		*name			= NULL;  /* program name */
static char				*mac			= NULL;  /* hardware address */
static char				*mask			= "255.255.255.255"; /* subnet to broadcast on */
static int				port			= 9;	 /* port number to use */
static int				level			= 0;	 /* hibernation level (S0--running) */
static bool				stdio			= false; /* if true, use stdin as input and stdout for output. */
static bool				wake			= true;  /* are we waking the machine up, or putting it to sleep? */
static bool				capabilities	= false; /* just print the things we need, as a machine, to be woken up. */
static ClassAd			*ad				= NULL;  /* machine class-ad */
static WakerBase		*waker			= NULL;  /* waking mechanism */
static HibernatorBase	*hibernator		= NULL;  /* hibernation mechanism */

/**	Functions */

static void
usage () {

	fprintf ( stderr, "usage: %s [OPTIONS] [INPUT-CLASS-AD-FILE] [OUTPUT]\n", name );
	fprintf ( stderr, "%s - %s\n", name, DESCRIPTION );
	fprintf ( stderr, "\n" );
	fprintf ( stderr, "-h	   this message\n" );
	fprintf ( stderr, "\n" );
	fprintf ( stderr, "-d	   enables debugging\n" );
	fprintf ( stderr, "\n" );
	fprintf ( stderr, "-c	   output capabilities\n" );
	fprintf ( stderr, "-i	   read and write to standard input and output\n" );
	fprintf ( stderr, "-m	   hardware address (MAC address)\n" );
	fprintf ( stderr, "-p	   port (default: %d)\n", port );
	fprintf ( stderr, "-s	   subnet mask (default: %s)\n", mask );
	fprintf ( stderr, "-H	   hibernate the current machine (default: S%d)\n", level );
	fprintf ( stderr, "-W	   wake a remote machine (default action)\n" );
	fprintf ( stderr, "\n" );
	fprintf ( stderr, "With -i, read standard input.\n" );
	
	exit ( 0 );

}

static void
enable_debug () {

	Termlog = 1;
	dprintf_config ( "TOOL" );

}

static void
error ( int code, ... ) {

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
parse_command_line ( int argc, char *argv[] ) {

	int		i, j = 0;
	char	*s;					/* to traverse the options */
	char	**argument = NULL;	/* option argument */	

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
					case 'c': capabilities = true;		break;
					case 'd': enable_debug ();			break;
					case 'h': usage ();					break;
					case 'i': stdio	= true;				break;
					case 'm': argument = &mac;			break;
					case 'p': port = (int) strtol ( s, &s, port ); break;
					case 's': argument = &mask;			break;
					case 'H': level = (int) strtol ( s, &s, level ); break;
					case 'W': level = 0;				break;
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
serialize_input () {

	char sinful[16 + 10];
	int  found_eof		= 0,
		 found_error	= 0,
		 empty			= 0;

	/**	Determine if we are using the command-line options, a file,
		or standard input as input */
	if ( !stdio ) {

		/**	Contrive a sinful string based on our IP address */
		sprintf ( sinful, "<%s:1234>", my_ip_string () );

		/**	We were give all the raw data, so we're going to create
			a fake machine ad that we will use when invoking the waking
			mechanism */
		ad = new ClassAd ();
		ad->SetMyTypeName ( STARTD_ADTYPE );
		ad->SetTargetTypeName ( JOB_ADTYPE );
		ad->Assign ( ATTR_HARDWARE_ADDRESS, mac );
		ad->Assign ( ATTR_SUBNET_MASK, mask );
		ad->Assign ( ATTR_PUBLIC_NETWORK_IP_ADDR, sinful );
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
			error (
				E_FOPEN,
				fn_in,
				strerror ( errno ),
				errno );
		}

		/**	Serialize the machine ad from a file */
		ad = new ClassAd (
			in,
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
serialize_output () {

	/**	Determine if we are using a file or standard output */
	if ( !stdio ) {

	} else {

		/**	Open the machine ad file or write to stdoutn */
		if ( fn_out && *fn_out ) {
			out = safe_fopen_wrapper ( fn_out, "w" );
		} else {
			out = stdout;
			fn_out = "<stdout>";
		}

	}

}

static void
wake_machine () {

	/**	Create the waking mechanism. */
	waker = WakerBase::createWaker ( ad );

	if ( !waker ) {
		error (
			E_NOMEM,
			"waker object." );
	}

	/**	Attempt to wake the remote machine */
	if ( !waker->doWake () ) {
		error (
			E_NOWAKE );
	}

	fprintf (
		stderr,
		"Packet sent.\n" );

	delete waker;

}

static void
hibernate_machine () {

	bool	ok		= false;
	state_t	desired	= HibernatorBase::intToSleepState ( level ),
			actual;

	/**	Create the hibernation mechanism. */
	hibernator = HibernatorBase::createHibernator ();

	if ( !hibernator ) {
		error (
			E_NOMEM,
			"hibernator object." );
	}

	/**	Try to put this machine into the requested power 
		state. */
	ok = hibernator->switchToState ( desired, actual );

	if ( !ok || desired != actual ) {
		error (
			E_NOREST );
	}

	fprintf (
		stderr,
		"Power level change.\n" );

	delete hibernator;

}

int
main ( int argc, char *argv[] ) {

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

		if ( level > 0 ) { 
			
			hibernate_machine ();

		} else {

			wake_machine ();

		}

	}

	return 0;

}
