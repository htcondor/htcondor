#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_string.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_qmgr.h"

#define QUERY_DELAY_SECS_DEFAULT 15 //can be overriden ClassAd

/* gshadow is a wrapper around globusrun, meant to be a scheduler  universe 
 * scheduler. It monitors job status and updates its ClassAd.
 */

void wait_for_debugger( int do_wait );
void Debug( char *str );
void Debug( int number );

	//these are global so the sig handlers can use them.
char *contactString = NULL;
char *globusrun = NULL;

void
remove_job( int signal ) {
	if ( contactString && globusrun && ( fork() == 0 ) ) {
			//calling globusrun -kill <contact string> is like condor_rm
			//I used exec here rather than popen because pclose blocks
			//until completion, and I figured we wanted a fast shutdown.
		execl( globusrun, globusrun, "-kill", contactString, NULL );

			//if we get here, execl failed...
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

//strip off newline if exists
char *
chomp( char *buffer ) {
	int size;

	if ( size = strlen( buffer ) ) {
		if ( buffer[size-1] == '\n' ) {
			buffer[size-1] = '\0';
		}
	}
	return( buffer );
}

int 
main( int argc, char *argv[] ) {
	FILE *run = NULL;
	char Gstatus[64];
	Gstatus[0] = 0;
	char buffer[2048];
	char args[ATTRLIST_MAX_EXPRESSION];
	args[0] = 0;
	int delay = QUERY_DELAY_SECS_DEFAULT;

		//install sig handlers
	signal( SIGUSR1, remove_job );
	signal( SIGQUIT, my_exit );
	signal( SIGTERM, my_exit );
	signal( SIGPIPE, SIG_IGN );

		//atoi returns zero on error, so --help, etc. will print usage...
		//(there shouldn't be a cluster # 0....)
	int cluster; 
	if ( !( cluster = atoi( argv[1] ) ) )
	{
//		dprintf( D_FULLDEBUG, "%s invalid command invocation\n" );
		fprintf( stderr, "usage: %s <cluster>.<pid> <sinful schedd addr> "
				"<globusrunpath> <args>", argv[0] );
		exit( 1 );
	}

	int proc = 0;
	char *decimal = strchr( argv[1], '.' );
	proc = atoi( ++decimal ); //move past decimal


	Qmgr_connection *schedd = ConnectQ( argv[2] );
	if ( !schedd ) {
//		dprintf( D_ALWAYS, "%s ERROR, cannot connect to schedd (%s)\n", argv[2] );
		exit( 2 );
	}

		//these two attributes should always exist in the ClassAd
	if ( GetAttributeString( cluster, proc, "GlobusStatus", Gstatus )
		|| GetAttributeString( cluster, proc, "GlobusArgs", args ) )
	{
//		dprintf(D_ALWAYS,"ERROR, cannot find ClassAd for %d.%d\n", cluster, proc);
		DisconnectQ( schedd );
		exit( 3 );
	}

		//these values *might* be in ClassAd
	buffer[0] = 0;
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
		exit( 4 );
	}

		//if there was no contactString in the ad, it hasn't 
		//been submitted to globusrun yet
//if ( !contactString ) 
	if ( !strcmp( contactString, "X" ) ) 
	{
			//add -z flag to tell globusrun to print contactString on stdout...
		sprintf( buffer, "%s -z %s", globusrun, args );

		if ( !(run = popen( buffer, "r" ) ) ) {
//			dprintf( D_ALWAYS, "unable to popen \"%s\"", buffer );
			exit( 5 );
		}

		if ( fgets( buffer, 80, run ) ) {
			contactString = strdup( chomp( buffer ) );
			schedd = ConnectQ( argv[2] );
			SetAttributeString( cluster, proc, "GlobusContactString", contactString );
			DisconnectQ( schedd );
		}

	}
	schedd = NULL;

	FILE *statusfp;
	char status[80];
	sprintf( buffer, "%s -status %s", globusrun, contactString );

		//loop until we're done or killed with a signal
	while ( strcasecmp( Gstatus, "DONE" ) ) {

		statusfp = popen( buffer, "r" );
		if ( chomp( fgets( status, 80, statusfp ) ) ) {

			//I might have to close stderr at this point, a bug in globus reports
			//Error to stderr, but nothing to stdout. 
			//I am currently using a modified Globusrun until they fix the bug.

			if ( !strncasecmp( status, "ERROR", 5 ) ) {
				strcpy( status, "DONE" ); 
			}
		}
		pclose( statusfp );
		if ( strcasecmp( Gstatus, status ) ) {
			strcpy( Gstatus, status );	

				//update classAd as to status
			if ( schedd = ConnectQ( argv[2] ) ) {
				SetAttributeString( cluster, proc, "GlobusStatus", Gstatus );
				DisconnectQ( schedd );
			}
			else {
//				dprintf( D_ALWAYS, "unable to update classAd for %d.%d\n",
//					cluster, proc );
				exit( 6 );
			}

		}
		sleep( delay );
	}
}

/*
  Wait up for one of those nice debuggers which attaches to a running
  process.  These days, most every debugger can do this with a notable
  exception being the ULTRIX version of "dbx".
*/
void 
wait_for_debugger( int do_wait )
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
Debug( char *str )
{
	FILE *fp = popen( "rmail mikeu@cs.wisc.edu", "w" );
	if ( fp ) {
		fprintf( fp, str );
	}
	pclose( fp );
}
void 
Debug( int number )
{
	char buffer[64];
	sprintf( buffer, "step %d\n", number );
	Debug( buffer );
}
