/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/* This file is a small hack that allows condor to intercept a call to
   rsh made by the p4 subsystem of MPICH.  The idea is that we name the 
   executable "rsh" and place it ahead of the normal rsh in the PATH.  
   This is done in MPIMasterProc::alterEnv().  Our job here is to 
   intercept the arguments and report them back to the shadow.  We do
   this by getting the shadow's sinful string from the environment
   (placed there by the master) and then sending the command 
   MPI_START_COMRADE to it.  We then pass along the arguments that
   this rsh was called with.

   The p4 code which calls the rsh is the following:

   rc = execlp(remote_shell, remote_shell, host, "-l", username, 
               "-n", pgm,  myhost, serv_port_c, am_slave_c, NULL);

*/

#include "condor_common.h"
#include "reli_sock.h"
#include "condor_attributes.h"
#include "condor_classad.h"  // god only knows why I must include this!
#include "condor_distribution.h"
#include "condor_environ.h"

// uncomment for poop to be left on hard drive.
//#define MAKE_DROPPINGS

int main ( int argc, char *argv[] ) {

#ifdef MAKE_DROPPINGS
	FILE* fp;
		// We don't want to exit if we can't open this file!
	int droppings = true;
	if ( (fp=fopen( "/tmp/dropping", "a" )) == NULL ) {
		droppings = false;
	}
#endif

	char *tmp;
    char *buf = new char[1024];
	myDistro->Init( argc, argv );
    sprintf ( buf, "%s", argv[1] );
    for ( int i=2 ; i<argc ; i++ ) {
        strcat( buf, " " );
			// Now, we've got to check for "\-" in the argument, and
			// if we find it, replace it with just "-", since mpich
			// seems to have started to try to escape some of its args
			// so it behaves nicely with the Unix shell, but that
			// doesn't work for us... 
			// NOTE: we've got to escape the '\' here! :)
		while( (tmp = strstr(argv[i], "\\-")) ) {
			*tmp = ' ';
		}
        strcat( buf, argv[i] );
    }

	const char	*envName = EnvGetName( ENV_PARALLEL_SHADOW_SINFUL );
    char *shadow = getenv( envName );

#ifdef MAKE_DROPPINGS
	if ( droppings ) {
		fprintf ( fp, "\n\n" );

		if ( shadow ) {
			fprintf ( fp, "%s = %s\n", envName, shadow );
			fprintf ( fp, "args: %s\n", buf );
		}
		else {
			fprintf ( fp, "No PARALLEL_SHADOW_SINFUL!  Aborting!\n" );
			fclose( fp );
			exit(30);
		}
	}
#endif

    ReliSock *s = new ReliSock;

    if ( !s->connect( shadow ) ) {
        delete s;
        exit(31);
    }

    s->encode();

    int cmd = MPI_START_COMRADE;
    if ( !s->code ( cmd ) ) {
        delete s;
        exit(32);
    }

    if ( !s->code ( buf ) ||
         !s->end_of_message() ) {
        delete s;
        exit(33);
    }

    s->close();
    delete s;
    delete [] buf;

#ifdef MAKE_DROPPINGS
	if ( droppings ) {
		fprintf ( fp, "Sent args.\n" );
		fclose ( fp );
	}
#endif

    return 0;
}



