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

char * sysapi_opsys();
char * sysapi_condor_arch();

int main ( int argc, char *argv[] ) {

	FILE* fp;
	if ( (fp=fopen( "/tmp/dropping", "w" )) == NULL ) {
		exit(29);
	}

    char *buf = new char[1024];
    sprintf ( buf, "%s", argv[1] );
    for ( int i=2 ; i<argc ; i++ ) {
        strcat( buf, " " );
        strcat( buf, argv[i] );
    }

    char *shadow = NULL;
    shadow = getenv( "MPI_SHADOW_SINFUL" );

	if ( shadow ) {
		fprintf ( fp, "MPI_SHADOW_SINFUL = %s\n", shadow );
		fprintf ( fp, "args: %s\n", buf );
	}
	else {
		fprintf ( fp, "No MPI_Shadow_Sinful!  Aborting!\n" );
		fclose( fp );
		exit(30);
	}

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

	fprintf ( fp, "Sent args.\n" );
	fclose ( fp );

    return 0;
}
