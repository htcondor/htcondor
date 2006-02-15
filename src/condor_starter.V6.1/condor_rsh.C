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
#include "condor_commands.h"
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



