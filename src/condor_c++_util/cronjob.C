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

#include <limits.h>
#include <string.h>
#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_cronmgr.h"
#include "condor_cron.h"

// Size of the buffer for reading from the child process
#define READBUF_SIZE	1024

// Cron's Line StdOut Buffer constructor
CronJobOut::
CronJobOut( const char *prefix) :
		LineBuffer( 128 )
{
	this->prefix = NULL;
	SetPrefix( prefix );
}

// Set output name prefix
int
CronJobOut::
SetPrefix( const char *prefix )
{
	if ( this->prefix ) { 
		free( (void*)this->prefix ); 
	}
	if ( prefix ) {
		this->prefix = strdup( prefix );
		if ( NULL == prefix ) {
			return -1;
		}
	} else {
		this->prefix = NULL;
	}
	return 0;
}

// Output function
int
CronJobOut::
Output( char *buf, int len )
{
	// Check for record delimitter
	if ( '-' == buf[0] ) {
		return 1;
	}

	// Clean up EOL
	if ( '\n' ==  buf[len] ) {
		buf[len] = '\0';
	}

	// Build up the string
	if ( stringBuf.Length() ) {
		stringBuf += ",";
	}
	stringBuf += prefix;
	stringBuf += buf;

	// Done
	return 0;
}

// Cron's Line StdErr Buffer constructor
CronJobErr::
CronJobErr( const char *prefix) :
		LineBuffer( 128 )
{
	this->prefix = NULL;
	SetPrefix( prefix );
}

// Set stderr name prefix
int
CronJobErr::
SetPrefix( const char *prefix )
{
	if ( this->prefix ) { 
		free( (void*)this->prefix ); 
	}
	if ( prefix ) {
		this->prefix = strdup( prefix );
		if ( NULL == prefix ) {
			return -1;
		}
	} else {
		this->prefix = NULL;
	}
	return 0;
}

// StdErr Output function
int
CronJobErr::
Output( char *buf, int len )
{
	// Clean up EOL
	if ( '\n' ==  buf[len] ) {
		buf[len] = '\0';
	}

	dprintf( D_ALWAYS, "%s: %s\n", prefix, buf );

	// Done
	return 0;
}

// Output function
MyString *
CronJobOut::
GetString( void )
{
	MyString	*retString = new MyString( stringBuf );

	// Zero out the old string
	stringBuf = "";

	// Return the copy
	return retString;
}

// CronJob constructor
CondorCronJob::
CondorCronJob( const char *jobName )
{
	// Reset *everything*
	path = NULL;
	prefix = NULL;
	name = NULL;
	args = NULL;
	env = NULL;
	cwd = NULL;
	period = UINT_MAX;
	state = CRON_IDLE;
	mode = CRON_ILLEGAL;
	optKill = false;
	runTimer = -1;
	killTimer = -1;
	todoReadTimer = -1;
	childFds[0] = childFds[1] = childFds[2] = -1;
	stdOut = stdErr = -1;

	// Build my output buffers
	stdOutBuf = new CronJobOut( );
	stdErrBuf = new CronJobErr( );

	// Store the name, etc.
	SetName( jobName );

	// Register my reaper
	reaperId = daemonCore->Register_Reaper( 
		"Cron_Reaper",
		(ReaperHandlercpp) &CondorCronJob::Reaper,
		"Cron Reaper",
		this );
}

// CronJob destructor
CondorCronJob::
~CondorCronJob( )
{
	dprintf( D_FULLDEBUG, "Cron: Deleting timer '%s'; ID = %d, path = '%s'\n",
			 name, runTimer, path );

	// Delete the timer FIRST
	if ( runTimer >= 0 ) {
		daemonCore->Cancel_Timer( runTimer );
	}

	// Kill job if it's still running
	KillJob( true );

	// Free up the name and path buffers
	if ( name ) free( name );
	if ( prefix ) free( prefix );
	if ( path ) free( path );
	if ( args ) free( args );
	if ( env ) free( env );
	if ( cwd ) free( cwd );

	// Close FDs
	CleanAll( );
}

// Get job Characteristics
const char *
CondorCronJob::
GetName( void )
{
	return name;
}

// Get job Characteristics
const char *
CondorCronJob::
GetPath( void )
{
	return path;
}

// Get job Characteristics
const char *
CondorCronJob::
GetArgs( void )
{
	return args;
}

// Get job Characteristics
const char *
CondorCronJob::
GetEnv( void )
{
	return env;
}

// Get job Characteristics
const char *
CondorCronJob::
GetCwd( void )
{
	return cwd;
}

// Get job Characteristics
unsigned
CondorCronJob::
GetPeriod( void )
{
	return period;
}

// Set job characteristics
int
CondorCronJob::
SetName( const char *newName )
{
	return SetVar( &name, newName, false );
}

// Set job characteristics
int
CondorCronJob::
SetPrefix( const char *newPrefix )
{
	stdOutBuf->SetPrefix( newPrefix );
	stdErrBuf->SetPrefix( newPrefix );
	return SetVar( &prefix, newPrefix, false );
}

// Set job characteristics
int
CondorCronJob::
SetPath( const char *newPath )
{
	return SetVar( &path, newPath, false );
}

// Set job characteristics
int
CondorCronJob::
SetArgs( const char *newArgs )
{
	return SetVar( &args, newArgs, true );
}

// Set job characteristics
int
CondorCronJob::
SetEnv( const char *newEnv )
{
	return SetVar( &env, newEnv, true );
}

// Set job characteristics
int
CondorCronJob::
SetCwd( const char *newCwd )
{
	return SetVar( &cwd, newCwd, true );
}

// Set job characteristics
int
CondorCronJob::
SetKill( bool kill )
{
	optKill = kill;
	return 0;
}

// Set job characteristics
int
CondorCronJob::
SetVar( char **var, const char *value, bool allowNull )
{
	// If path is already set, and hasn't changed, do nothing!
	if (  ( NULL != *var ) && ( NULL != value ) &&
		  ( 0 == strcmp( value, *var ) )  ) {
		return 0;
	} 

	// Free up the existing value
	if ( *var ) {
		free( *var );
	}

	// NULL Value to set to?
	if ( NULL == value ) {
		if ( ! allowNull ) {
			return -1;
		}
		*var = NULL;
		return 0;
	}

	// Allocate & copy in the new path
	*var = strdup ( value );
	return ( NULL == *var ) ? -1 : 0;
}

// Set job characteristics
int
CondorCronJob::
SetPeriod( CronJobMode newMode, unsigned newPeriod )
{
	// Verify that the mode seleted is valid
	if (  ( CRON_CONTINUOUS != newMode ) && ( CRON_PERIODIC != newMode )  ) {
		dprintf( D_ALWAYS, "Cron: illegal mode selected for job '%s'\n",
				 name );
		return -1;
	} else if (  ( CRON_PERIODIC == newMode ) && ( 0 == newPeriod )  ) {
		dprintf( D_ALWAYS, 
				 "Cron: Job '%s'; Periodic requires non-zero period\n",
				 name );
		return -1;
	}

	// Any work to do?
	if (  ( mode == newMode ) && ( period == newPeriod )  ) {
		return 0;
	}

	// Mode change; cancel the existing timer
	if (  ( mode != newMode ) && ( runTimer >= 0 )  ) {
		daemonCore->Cancel_Timer( runTimer );
		runTimer = -1;
	}

	// Store the period; is it a periodic?
	mode = newMode;
	period = newPeriod;

	// Now, schedule the job to run..
	int	status = 0;

	// It's not a periodic -- just start it
	if ( CRON_CONTINUOUS == mode ) {
		status = StartJob( );

	} else {				// Periodic
		// Set the job's timer
		status = SetTimer( period, period );

		// Start the first run..
		if (  ( 0 == status ) && ( CRON_IDLE == state )  ) {
			status = RunJob( );
		}
	}

	// Done
	return status;
}

// Reconfigure a running job
int
CondorCronJob::
Reconfig( void )
{
	// Only do this to running continuous jobs
	if (  ( CRON_CONTINUOUS != mode ) || ( CRON_RUNNING != state )  ) {
		return 0;
	}

	// HUP it; if it dies it'll get the new config when it restarts
	dprintf( D_ALWAYS, "Cron: Sending HUP to '%s' pid %d\n", name, pid );
	return daemonCore->Send_Signal( pid, DC_SIGHUP );
}

// Schdedule the job to run
int
CondorCronJob::
RunJob( void )
{

	// Make sure that the job is idle!
	if ( state != CRON_IDLE ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' is still running!\n", name );

		// If we're not supposed to kill the process, just skip this timer
		if ( optKill ) {
			return KillJob( false );
		} else {
			return -1;
		}
	}

	// Job not running, just start it
	dprintf( D_JOB, "Cron: Running job '%s', path '%s'\n", name, path );

	// TODO: Create the "Read" timer
	if ( todoReadTimer < 0 ) {
		dprintf( D_FULLDEBUG, "Cron: Creating Read timer for '%s'\n", 
				 name );
		todoReadTimer = daemonCore->Register_Timer(
			1,
			1,
			(TimerHandlercpp)& CondorCronJob::TodoReadHandler,
			"TodoReadTimer",
			this );
		if ( todoReadTimer < 0 ) {
			dprintf( D_ALWAYS, "Cron: Failed to create read timer\n" );
			return -1;
		}
		dprintf( D_FULLDEBUG, "Cron: new read timer ID = %d set to 1s/1s\n",
				 todoReadTimer );
	}

	// Start it up
	return RunProcess( );
}

// Handle the kill timer
int
CondorCronJob::
KillHandler( void )
{

	// Log that we're here
	dprintf( D_ALWAYS, "Cron: KillHandler for job '%s'\n", name );

	// If we're idle, we shouldn't be here.
	if ( CRON_IDLE == state ) {
		dprintf( D_ALWAYS, "Cron: Job '%'s already idle '%s'!\n", name );
		return -1;
	}

	// Kill it.
	return KillJob( false );
}

// Handle the "Read" timer -- TODO --
int
CondorCronJob::
TodoReadHandler( void )
{
	// Read a chunk
	return TodoRead( 0, 50 );

}

// Start a job
int
CondorCronJob::
StartJob( void )
{
	if ( CRON_IDLE != state ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' already running!\n", name );
		return 0;
	}
	dprintf( D_JOB, "Cron: Starting job '%s', path '%s'\n", name, path );

	// TODO: Create the "Read" timer
	if ( todoReadTimer < 0 ) {
		dprintf( D_FULLDEBUG, "Cron: Creating Read timer for '%s'\n", 
				 name );
		todoReadTimer = daemonCore->Register_Timer(
			1,
			1,
			(TimerHandlercpp)& CondorCronJob::TodoReadHandler,
			"TodoReadTimer",
			this );
		if ( todoReadTimer < 0 ) {
			dprintf( D_ALWAYS, "Cron: Failed to create read timer\n" );
			return -1;
		}
		dprintf( D_FULLDEBUG, "Cron: new read timer ID = %d set to 1s/1s\n",
				 todoReadTimer );
	} else {
		daemonCore->Reset_Timer( todoReadTimer, 1, 1 );
	}

	// Run it
	return RunProcess( );
}

// Child reaper
int
CondorCronJob::
Reaper( int exitPid, int exitStatus )
{
	dprintf( D_ALWAYS, "Cron: Job '%s' (pid %d) exit status=%d, state=%s\n",
			 name, exitPid, exitStatus, StateString( ) );

	// What if the PIDs don't match?!
	if ( exitPid != pid ) {
		dprintf( D_ALWAYS, "Cron: WARNING: Child PID %d != Exit PID %d\n",
				 pid, exitPid );
	}

	// TODO: Suspend the read timer
	if ( todoReadTimer > 0 )
	{
		daemonCore->Reset_Timer( todoReadTimer, TIMER_NEVER, TIMER_NEVER );
	}

	// TODO: Do the final read
	TodoRead( 0, 100 );

	// Read it's final stdout for building a ClassAd
	MyString	*string = stdOutBuf->GetString( );

	// Clean up it's file descriptors
	CleanAll( );

	// We *should* be in CRON_RUNNING state now; check this...
	switch ( state )
	{
		// Normal death
	case CRON_RUNNING:
		state = CRON_IDLE;				// Note it's death
		if ( CRON_CONTINUOUS == mode ) {
			if ( 0 == period ) {			// Continuous, no delay
				StartJob( );
			} else {						// Continuous with delay
				SetTimer( period, TIMER_NEVER );
			}
		}
		break;

		// Huh?  Should never happen
	case CRON_IDLE:
		break;							// Do nothing

		// We've sent the process a signal, waiting for it to die
	default:
		state = CRON_IDLE;				// Note that it's dead

		// Cancel the kill timer if required
		KillTimer( TIMER_NEVER );

		// Re-start the job
		if ( CRON_PERIODIC == mode ) {		// Periodic
			RunJob( );
		} else if ( 0 == period ) {			// Continuous, no delay
			StartJob( );
		} else {							// Continuous with delay
			SetTimer( period, TIMER_NEVER );
		}
		break;

	}

	// Process the output
	if ( string->Length() == 0 ) {
		return 0;
	} else {
		return ProcessOutput( string );
	}
}

// Publisher
int
CondorCronJob::
ProcessOutput( MyString *string )
{
	(void) string;

	// Do nothing
	return 0;
}

// Start a job
int
CondorCronJob::
RunProcess( void )
{
	dprintf( D_ALWAYS, "Running '%s'\n", name );

	// Create file descriptors
	if ( OpenFds( ) < 0 ) {
		dprintf( D_ALWAYS, "Cron: Error creating FDs for '%s'\n", name );
		return -1;
	}

	// Are there arguments to pass it?
	char *argBuf = NULL;
	if ( NULL != args ) {
		int		len = strlen( args ) + strlen( name ) + 1;
		if ( NULL == ( argBuf = (char *) malloc( len ) )  ) {
			dprintf( D_ALWAYS, 
					 "Cron: Couldn't allocate arg buffer %d bytes\n",
					 len );
			return -1;
		}
		strcpy( argBuf, name );
		strcat( argBuf, args );
	}

	// Create the process, finally..
	pid = daemonCore->Create_Process(
		path,				// Path to executable
		argBuf,				// argv
		PRIV_UNKNOWN,		// Priviledge level
		reaperId,			// ID Of reaper
		FALSE,				// Command port?  No
		env,				// Env to give to child
		cwd,				// Starting CWD
		FALSE,				// New process group?
		NULL,				// Socket list
		childFds,			// Stdin/stdout/stderr
		0 );				// Nice increment

	// Close the child FDs
	CleanFd( &childFds[0] );
	CleanFd( &childFds[1] );
	CleanFd( &childFds[2] );

	// Did it work?
	if ( pid < 0 ) {
		dprintf( D_ALWAYS, "Cron: Error running job '%s'\n", name );
		if ( NULL != argBuf ) {
			free( argBuf );
		}
		return -1;
	}

	// All ok here
	state = CRON_RUNNING;

	// HACK: TODO
	TodoRead( 2, 0 );
	return 0;
}

// Hack to read from the pipes 'til Todd get's the pipe checking in
// daemonCore
int
CondorCronJob::
TodoRead ( int sec, int usec )
{
	fd_set			fd_read;
	struct timeval	timeout;
	int				maxfd = MAX( stdOut, stdErr ) + 1;
	char			buf[READBUF_SIZE];
	int				bytes;

	timeout.tv_sec = sec;
	timeout.tv_usec = usec;

	// Loop 'til select times out or both fd's close
	while( ( stdOut >= 0 ) || ( stdErr >= 0 ) )
	{

		// Build the FD set, call select
		FD_ZERO( &fd_read );
		if ( stdOut >= 0 ) FD_SET( stdOut, &fd_read );
		if ( stdErr >= 0 ) FD_SET( stdErr, &fd_read );
		int numfds = select( maxfd, &fd_read, NULL, NULL, &timeout );

		// Check the results of select
		if ( 0 == numfds )
		{
			return 0;
		}
		else if ( numfds < 0 )
		{
			dprintf( D_ALWAYS, "Cron: select failed %d : '%s'\n",
					 errno, strerror( errno ) );
			return -1;
		}

		// Check for data on StdOut
		if ( ( stdOut >= 0 ) && FD_ISSET( stdOut, &fd_read ) )
		{
			bytes = read( stdOut, buf, READBUF_SIZE );
			if ( bytes == 0 )
			{
				dprintf( D_ALWAYS, "Cron: STDOUT closed for '%s'\n", name );
				close( stdOut );
				stdOut = -1;
			}
			else if ( bytes > 0 )
			{
				const char	*bptr = buf;

				// stdOutBuf->Output() returns 1 if it finds '-', otherwise 0,
				// so that's what Buffer returns, too...
				while ( stdOutBuf->Buffer( &bptr, &bytes ) > 0 ) {
					MyString	*string = stdOutBuf->GetString( );
					ProcessOutput( string );
				}
			}
			else if ( errno != EWOULDBLOCK )
			{
				dprintf( D_ALWAYS,
						 "Cron: read STDOUT failed for '%s' %d: '%s'\n",
						 name, errno, strerror( errno ) );
				return -1;
			}
		}

		// Check for data on StdErr
		if ( ( stdErr >= 0 ) && FD_ISSET( stdErr, &fd_read ) )
		{
			bytes = read( stdErr, buf, READBUF_SIZE );
			if ( bytes == 0 )
			{
				dprintf( D_ALWAYS, "Cron: STDERR closed for '%s'\n", name );
				close( stdErr );
				stdErr = -1;
			}
			else if ( bytes > 0 )
			{
				const char	*bptr = buf;
				while( stdErrBuf->Buffer( &bptr, &bytes ) > 0 ) {
					// Do nothing for now
				}
			}
			else if ( errno != EWOULDBLOCK )
			{
				dprintf( D_ALWAYS, 
						 "Cron: read STDERR failed for '%s' %d : '%s'\n",
						 name, errno, strerror( errno ) );
				return -1;
			}
		}

	}

	// Flush the Std err/out buffers
	stdOutBuf->Flush();
	stdErrBuf->Flush();
	return 0;
}

// Create the job's file descriptors
int
CondorCronJob::
OpenFds ( void )
{
	int	tmpfds[2];

	// stdin goes to the bit bucket
	childFds[0] = -1;

	// Pipe to stdout
	if ( pipe( tmpfds ) < 0 ) {
		dprintf( D_ALWAYS, "Cron: Can't create pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	stdOut = tmpfds[0];
	childFds[1] = tmpfds[1];

	// Make it non-blocking -- TODO
# ifdef UNIX
	fcntl( stdOut, F_SETFL, FNDELAY );
# endif

	// Pipe to stderr
	if ( pipe( tmpfds ) < 0 ) {
		dprintf( D_ALWAYS, "Cron: Can't create STDERR pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	stdErr = tmpfds[0];
	childFds[2] = tmpfds[1];

	// Make it non-blocking -- TODO
# ifdef UNIX
	fcntl( stdErr, F_SETFL, FNDELAY );
# endif

	// Done; all ok
	return 0;
}

// Clean up all file descriptors & FILE pointers
void
CondorCronJob::
CleanAll ( void )
{
	CleanFd( &stdOut );
	CleanFd( &stdErr );
	CleanFd( &childFds[0] );
	CleanFd( &childFds[1] );
	CleanFd( &childFds[2] );
}

// Clean up a FILE *
void
CondorCronJob::
CleanFile ( FILE **file )
{
	if ( NULL != *file ) {
		fclose( *file );
		*file = NULL;
	}
}

// Clean up a file descriptro
void
CondorCronJob::
CleanFd ( int *fd )
{
	if ( *fd >= 0 ) {
		close( *fd );
		*fd = -1;
	}
}

// Kill a job
int
CondorCronJob::
KillJob( bool force )
{
	// Idle?
	if ( CRON_IDLE == state ) {
		return 0;
	}

	// Kill the process *hard*?
	if ( ( force ) || ( CRON_TERMSENT == state )  ) {
		dprintf( D_JOB, "Cron: Killing job '%s' with SIGKILL, pid = %d\n", 
				 name, pid );
		daemonCore->Send_Signal( 0 - pid, DC_SIGKILL );
		state = CRON_KILLSENT;
		KillTimer( TIMER_NEVER );	// Cancel the timer
		return 0;
	} else if ( CRON_RUNNING == state ) {
		dprintf( D_JOB, "Cron: Killing job '%s' with SIGTERM, pid = %d\n", 
				 name, pid );
		daemonCore->Send_Signal( 0 - pid, DC_SIGTERM );
		state = CRON_TERMSENT;
		KillTimer( 1 );				// Schedule hard kill in 1 sec
		return 1;
	} else {
		return -1;					// Nothing else to do!
	}
}

// Set the job timer
int
CondorCronJob::
SetTimer( unsigned first, unsigned period )
{
	// Reset the timer
	if ( runTimer >= 0 )
	{
		daemonCore->Reset_Timer( runTimer, first, period );
		dprintf( D_FULLDEBUG, "Cron: timer ID %d reset to %u/%u\n", 
				 runTimer, first, period );
	}

	// Create a periodic timer
	else
	{
		// Debug
		dprintf( D_FULLDEBUG, 
				 "Cron: Creating timer for job '%s'\n", name );
		TimerHandlercpp handler =
			(  ( CRON_CONTINUOUS == mode ) ? 
			   (TimerHandlercpp)& CondorCronJob::StartJob :
			   (TimerHandlercpp)& CondorCronJob::RunJob );
		runTimer = daemonCore->Register_Timer(
			first,
			period,
			handler,
			"RunJob",
			this );
		if ( runTimer < 0 ) {
			dprintf( D_ALWAYS, "Cron: Failed to create timer\n" );
			return -1;
		}
		dprintf( D_FULLDEBUG, "Cron: new timer ID = %d set to %u/%u\n", 
				 runTimer, first, period );
	} 

	return 0;
}

// Start the kill timer
int
CondorCronJob::
KillTimer( unsigned seconds )
{
	// Cancel request?
	if ( TIMER_NEVER == seconds ) {
		dprintf( D_FULLDEBUG, "Cron: Canceling kill timer for '%s'\n", name );
		if ( killTimer >= 0 ) {
			return daemonCore->Reset_Timer( 
				killTimer, TIMER_NEVER, TIMER_NEVER );
		}
		return 0;
	}

	// Reset the timer
	if ( killTimer >= 0 )
	{
		daemonCore->Reset_Timer( killTimer, seconds, 0 );
		dprintf( D_FULLDEBUG, "Cron: Kill timer ID %d reset to %us\n", 
				 killTimer, seconds );
	}

	// Create the Kill timer
	else
	{
		// Debug
		dprintf( D_FULLDEBUG, "Cron: Creating kill timer for '%s'\n", 
				 name );
		killTimer = daemonCore->Register_Timer(
			seconds,
			0,
			(TimerHandlercpp)& CondorCronJob::KillHandler,
			"KillJob",
			this );
		if ( killTimer < 0 ) {
			dprintf( D_ALWAYS, "Cron: Failed to create kill timer\n" );
			return -1;
		}
		dprintf( D_FULLDEBUG, "Cron: new kill timer ID = %d set to %us\n",
				 killTimer, seconds );
	}

	return 0;
}

// Convert state value into string (for printing)
const char *
CondorCronJob::
StateString( CronJobState state )
{
	switch( state )
	{
	case CRON_IDLE:
		return "Idle";
	case CRON_RUNNING:
		return "Running";
	case CRON_TERMSENT:
		return "TermSent";
	case CRON_KILLSENT:
		return "KillSent";
	default:
		return "Unknown";
	}
}

// Same, but uses the job's current state
const char *
CondorCronJob::
StateString( void )
{
	return StateString( state );
}
