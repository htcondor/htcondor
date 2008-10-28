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
#include "udp_waker.h"

#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "condor_classad.h"
#include "condor_attributes.h"

/*
Preprocessor Definitions
*/

#define DESCRIPTION "wake a remote machine"

/*
Constants
*/

/* error codes */
enum {
    E_NONE    =  0,  /* no error */
    E_NOMEM   = -1,  /* not enough memory */
    E_FOPEN   = -2,  /* cannot open file */
    E_FREAD   = -3,  /* read error on file */
    E_OPTION  = -4,  /* unknown option */
    E_OPTARG  = -5,  /* missing option argument */
    E_ARGCNT  = -6,  /* too few/many arguments */
    E_NOWAKE  = -7,  /* failed to wake machine */
    E_CLASSAD = -8,  /* error in class-ad (errno = %d)*/
    E_UNKNOWN = -9   /* unknown error */
};

/* error messages */
static const char *errmsgs[] = {
  /* E_NONE     0 */  "no error%s.\n",
  /* E_NOMEM   -1 */  "not enough memory for %s.\n",
  /* E_FOPEN   -2 */  "cannot open file %s: %s (errno = %d).\n",
  /* E_FREAD   -3 */  "read error on file %s.\n",
  /* E_OPTION  -4 */  "unknown option -%c.\n",
  /* E_OPTARG  -5 */  "missing option argument.\n",
  /* E_ARGCNT  -6 */  "wrong number of arguments.\n",
  /* E_NOWAKE  -7 */  "failed to sent wake packet.\n",
  /* E_CLASSAD -8 */  "error in class-ad %s.\n",
  /* E_UNKNOWN -9 */  "unknown error.\n"
};

/*
Global Variables
*/

static char      *program_name   = NULL; /* program name for error messages */
static FILE      *in             = NULL; /* input file */
static char      *fn_in          = NULL; /* name of input file */
static char      *mac            = NULL; /* hardware address */
static char      *subnet         = "255.255.255.255"; /* subnet to broadcast on */
static int       port            = 9;    /* port number to use */
static ClassAd   *ad             = NULL; /* machine class-ad */
static WakerBase *waker          = NULL; /* waking mechanism */

/*
Functions
*/

static void
usage () {

    fprintf ( stderr, "usage: %s [OPTIONS] [CLASS-AD-FILE]\n",
        program_name );
    fprintf ( stderr, "%s - %s\n", program_name, DESCRIPTION );
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "-h       this message\n" );
    fprintf ( stderr, "-m       hardware address (MAC address)\n" );
    fprintf ( stderr, "-s       subnet (default: %s)\n", subnet );
    /* fprintf ( stderr, "-p#      port (default: %d)\n", port ); */
    fprintf ( stderr, "-d       turns debugging on\n" );
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "With no CLASS-AD-FILE, read standard input.\n" );

    exit ( 0 );

}

static void
enable_debug () {

    Termlog = 1;
	dprintf_config ( "TOOL" );

}

static void
error ( int code, ... ) {

    va_list    args;
    const char *msg;

    assert ( program_name );

    if ( code < E_UNKNOWN ) {
        code = E_UNKNOWN;
    }

    if ( code < 0 ) {

        msg = errmsgs[-code];

        if ( !msg ) {
            msg = errmsgs[-E_UNKNOWN];
        }

        fprintf ( stderr, "%s: ", program_name );
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

    int     i, j = 0;
    char    *s;                 /* to traverse the options */
    char    **argument = NULL;  /* option argument */	

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
                    case 'd': enable_debug ();          break;
                    case 'h': usage ();                 break;
                    case 'm': argument = &mac;          break;
                    case 's': argument = &subnet;       break;
                    case 'p': port = (int) strtol ( s, &s, port ); break;
                    default : error ( E_OPTION, *--s ); break;
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
                case  0: fn_in = s;          break;
                default: error ( E_ARGCNT ); break;
            }

        }

    }

 }

int
main ( int argc, char *argv[] ) {

    char    *name       = NULL,
            sinful[16 + 10];
    int     found_eof   = 0,
            found_error = 0,
            empty       = 0;
    bool    sent_wake   = false;

    my_ip_string ();

    /* retrieve the program's name */
    name = strrchr ( argv[0], DIR_DELIM_CHAR );
    program_name = !name ? argv[0] : name + 1;

    /* parse the command line and populate the global state */
    parse_command_line ( argc, argv );

    /* determine if we are using the command-line options, a file,
    or standard input as input */
    if ( mac && *mac ) {

        /* contrive a sinful string based on our IP address */
        sprintf ( sinful, "<%s:1234>", my_ip_string () );

        /* we were give all the raw data, so we're going to create
        a fake machine ad that we will use when invoking the waking
        mechanism */
        ad = new ClassAd ();
        ad->SetMyTypeName ( STARTD_ADTYPE );
        ad->SetTargetTypeName ( JOB_ADTYPE );
        ad->Assign ( ATTR_HARDWARE_ADDRESS, mac );
        ad->Assign ( ATTR_SUBNET, subnet );
        ad->Assign ( ATTR_PUBLIC_NETWORK_IP_ADDR, sinful );

    } else {

        /* open the machine ad file or read it from stdin */
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

        /* serialize the machine ad from a file */
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

    if ( ad ) {

        /* create the waking mechanism, and wake the machine */
        waker = WakerBase::createWaker ( ad );

        if ( !waker ) {
            error (
                E_NOMEM,
                "waker object." );
        }

        sent_wake = waker->doWake ();

    }

    if ( !sent_wake ) {
        error (
            E_NOWAKE );
    }

    fprintf (
        stderr,
        "Packet sent.\n" );

    return 0;

}
