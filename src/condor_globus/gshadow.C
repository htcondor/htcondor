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
#include "condor_debug.h"
#include "condor_string.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_qmgr.h"


/* gshadow is a wrapper around globusrun, meant to be a scheduler universe 
 * scheduler. It monitors job status and updates its ClassAd.
 */


#define QUERY_DELAY_SECS_DEFAULT 15 //can be overriden ClassAd
#define QMGMT_TIMEOUT 300 //this is copied per Todd's advice from shadow val.


	//these are global so the sig handlers can use them.
char *contactString = NULL;
char *globusrun = NULL;

extern "C" char * chomp(char*);

/*
  Wait up for one of those nice debuggers which attaches to a running
  process.  These days, most every debugger can do this with a notable
  exception being the ULTRIX version of "dbx".
*/
void wait_for_debugger( int do_wait )
{
   sigset_t sigset;

   // This is not strictly POSIX conforming becuase it uses SIGTRAP, but
   // since it is only used in those environments where is is defined, it
   // will probably pass...
#if defined(SIGTRAP)
      /* Make sure we don't block the signal used by the
      ** debugger to control the debugged process (us).
      */
   sigemptyset( &sigset );
   sigaddset( &sigset, SIGTRAP );
   sigprocmask( SIG_UNBLOCK, &sigset, 0 );
#endif

   while( do_wait )
      ;
}


void
remove_job( int signal ) {
	if ( contactString && globusrun && ( fork() == 0 ) ) {
			//calling globusrun -kill <contact string> is like condor_rm
			//I used exec here rather than popen because pclose blocks
			//until completion, and I figured we wanted a fast shutdown.
		execl( globusrun, globusrun, "-kill", contactString, NULL );

			//if we get here, execl failed...
		fprintf(stderr, "ERROR on execl %s %s -kill %s\n", globusrun, 
				globusrun, contactString );
//		dprintf(D_ALWAYS, "ERROR on execl %s %s -kill %s\n", globusrun, 
//				globusrun, contactString );
	}
	exit( signal );
}

void
my_exit( int signal ) {
//probably want to change this
	remove_job( signal );
}

int 
main( int argc, char *argv[] ) {
	FILE *run = NULL;
	char Gstatus[64] = "";
	char buffer[2048] = "";
	char args[ATTRLIST_MAX_EXPRESSION] = "";
	int delay = QUERY_DELAY_SECS_DEFAULT;

		//install sig handlers
	signal( SIGUSR1, remove_job );
	signal( SIGQUIT, my_exit );
	signal( SIGTERM, my_exit );
	signal( SIGPIPE, SIG_IGN );
//wait_for_debugger( 1 );
		//atoi returns zero on error, so --help, etc. will print usage...
		//(there shouldn't be a cluster # 0....)
	int cluster; 
	if ( !( cluster = atoi( argv[1] ) ) )
	{
//		dprintf( D_FULLDEBUG, "%s invalid command invocation\n" );
		fprintf( stderr, "usage: %s <cluster>.<pid> <sinful schedd addr> \\"
				"<globusrunpath> <args>\n", argv[0] );
		exit( 1 );
	}

	int proc = 0;
	char *decimal = strchr( argv[1], '.' );
	proc = atoi( ++decimal ); //move past decimal


		//THIS MUST be a fatal error if we cannot connect, since we 
		//start the globus job without getting GlobusArgs...
	Qmgr_connection *schedd = ConnectQ( argv[2], QMGMT_TIMEOUT );
	if ( !schedd ) {
		//wait a bit and try once more...
		sleep( 30 );
		if ( !( schedd = ConnectQ( argv[2], QMGMT_TIMEOUT ) ) ) {
//			dprintf( D_ALWAYS, "%s ERROR, can't connect to schedd (%s)\n",argv[2]);
			fprintf( stderr, "%s ERROR, cannot connect to schedd (%s)\n", argv[2] );
			exit( 2 );
		}
	}

		//these two attributes should always exist in the ClassAd
	if ( GetAttributeString( cluster, proc, "GlobusStatus", Gstatus )
		|| GetAttributeString( cluster, proc, "GlobusArgs", args ) )
	{
//		dprintf(D_ALWAYS,"ERROR, cannot find ClassAd for %d.%d\n", cluster, proc);
		fprintf(stderr,"ERROR, cannot find ClassAd for %d.%d\n", cluster, proc);
		DisconnectQ( schedd );
		exit( 3 );
	}

		//these values *might* be in ClassAd
	GetAttributeString( cluster, proc, "GlobusContactString", buffer );
	if ( buffer[0] ) {
		contactString = strdup( buffer );
	}
	GetAttributeInt( cluster, proc, "GlobusQueryDelay", &delay );
	DisconnectQ( schedd ); //close because popen for globusrun takes long time
	schedd = NULL;

	globusrun = strdup( argv[3] );
	struct stat statbuf;
	if ( ( stat( globusrun, &statbuf ) < 0 ) 
//	|| (((statbuf.st_mode)&0xF000) != 0x0001)
	)
	{
//		dprintf( D_ALWAYS, "ERROR stat'ing globusrun (%s)\n", globusrun );
		fprintf( stderr, "ERROR stat'ing globusrun (%s)\n", globusrun );
		exit( 4 );
	}

		//if there was no contactString in the ad, it hasn't 
		//been submitted to globusrun yet
	if ( !strcmp( contactString, "X" ) ) 
	{
		sprintf( buffer, "%s %s", globusrun, args );

		if ( !(run = popen( buffer, "r" ) ) ) {
//			dprintf( D_ALWAYS, "unable to popen \"%s\"", buffer );
			fprintf( stderr, "unable to popen \"%s\"", buffer );
			exit( 5 );
		}

		while ( !feof( run ) ) {
			if ( !fgets( buffer, 80, run ) ) {
				fprintf(stderr, "error reading output of globus job\n" );
			}
			if ( !strncasecmp( buffer, "http", 4 ) ) {
				contactString = strdup( chomp( buffer ) );
				break;
			}
		}
		if ( contactString ) {
			if ( schedd = ConnectQ( argv[2], QMGMT_TIMEOUT ) ) {
				SetAttributeString( cluster, proc, "GlobusContactString", 
						contactString );
				DisconnectQ( schedd );
			}
			else {
				//FATAL error if we can't set GlobusContactString!
//				dprintf( D_ALWAYS, "Error contacting schedd %s\n", argv[2] );
				fprintf( stderr, "Error contacting schedd %s\n", argv[2] );
				exit( 6 );
			}
	
		}
		else {
//			dprintf( D_ALWAYS, "Error reading contactString from globusrun\n" );
			fprintf( stderr, "Error reading contactString from globusrun\n" );
			exit( 7 );
		}
	}
	schedd = NULL;

	FILE *statusfp = NULL;
	char status[80] = "";
	sprintf( buffer, "%s -status %s", globusrun, contactString );

		//loop until we're done or killed with a signal
	while ( strcasecmp( Gstatus, "DONE" ) ) {

		if ( !(statusfp = popen( buffer, "r" ) ) ) {
//			dprintf( D_ALWAYS, "cannot popen( %s )\n", buffer );
			fprintf( stderr, "cannot popen( %s )\n", buffer );
			exit( 8 );
		}
		if ( !fgets( status, 80, statusfp ) ) {
//			dprintf( D_ALWAYS, "pipe read errno %d\n", errno );
			fprintf( stderr, "pipe read errno %d\n", errno );
		}
		chomp( status );

		rc = pclose( statusfp );
//globusrun returns 79 when the jobmanager can't be contacted
//   (i.e. when a job has exitted)
/*
		if ( rc ) {
			fprintf( stderr, "globusrun exited with status %d on job status check\n", rc );
			exit( 10 );
		}
*/
		//In Globus 1.1.1 and earlier, on certain errors, globusrun will
		//exit with a false status of 0 (success). In these cases, nothing
		//is printed to stdout. To work with these versions of Globus, we
		//should treat empty stdout as an error (otherwise, we may end up
		//with an infinite loop..

		if ( !strncasecmp( status, "ERROR", 5 ) ) {
			strcpy( status, "DONE" ); 
		}

		pclose( statusfp );
		if ( strcasecmp( Gstatus, status ) ) {
			strcpy( Gstatus, status );	

				//this update is NOT fatal, just try again later
			if ( schedd = ConnectQ( argv[2], QMGMT_TIMEOUT ) ) {
				SetAttributeString( cluster, proc, "GlobusStatus", Gstatus );
				DisconnectQ( schedd );
			}
			else {
//				dprintf( D_ALWAYS, "unable to update classAd for %d.%d\n",
//					cluster, proc );
				fprintf( stderr, "unable to update classAd for %d.%d\n",
					cluster, proc );
			}

		}
		sleep( delay );
	}
}
