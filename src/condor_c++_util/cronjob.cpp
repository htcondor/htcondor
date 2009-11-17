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
#include <limits.h>
#include <string.h>
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_cronmgr.h"
#include "condor_cron.h"
#include "condor_string.h"

// Size of the buffer for reading from the child process
#define STDOUT_READBUF_SIZE	1024
#define STDOUT_LINEBUF_SIZE	8192
#define STDERR_READBUF_SIZE	128

// Cron's Line StdOut Buffer constructor
CronJobOut::CronJobOut( class CronJobBase *job_arg ) :
		LineBuffer( STDOUT_LINEBUF_SIZE )
{
	this->job = job_arg;
}

// Output function
int
CronJobOut::Output( const char *buf, int len )
{
	// Ignore empty lines
	if ( 0 == len ) {
		return 0;
	}

	// Check for record delimitter
	if ( '-' == buf[0] ) {
		return 1;
	}

	// Build up the string
	const char	*prefix = job->GetPrefix( );
	int		fulllen = len;
	if ( prefix ) {
		fulllen += strlen( prefix );
	}
	char	*line = (char *) malloc( fulllen + 1 );
	if ( NULL == line ) {
		dprintf( D_ALWAYS,
				 "cronjob: Unable to duplicate %d bytes\n",
				 fulllen );
		return -1;
	}
	if ( prefix ) {
		strcpy( line, prefix );
	} else {
		*line = '\0';
	}
	strcat( line, buf );

	// Queue it up, get out
	lineq.enqueue( line );

	// Done
	return 0;
}

// Get size of the queue
int
CronJobOut::GetQueueSize( void )
{
	return lineq.Length( );
}

// Flush the queue
int
CronJobOut::FlushQueue( void )
{
	int		size = lineq.Length( );
	char	*line;

	// Flush out the queue
	while( ! lineq.dequeue( line ) ) {
		free( line );
	}

	// Return the size
	return size;
}

// Get next queue element
char *
CronJobOut::GetLineFromQueue( void )
{
	char	*line;

	if ( ! lineq.dequeue( line ) ) {
		return line;
	} else {
		return NULL;
	}
}

// Cron's Line StdErr Buffer constructor
CronJobErr::CronJobErr( class CronJobBase *job_arg ) :
		LineBuffer( 128 )
{
	this->job = job_arg;
}

// StdErr Output function
int
CronJobErr::Output( const char *buf, int   /*len*/ )
{
	dprintf( D_FULLDEBUG, "%s: %s\n", job->GetName( ), buf );

	// Done
	return 0;
}

// CronJob constructor
CronJobBase::CronJobBase( const char *   /*mgrName*/, const char *jobName )
{
	// Reset *everything*
	period = UINT_MAX;
	state = CRON_NOINIT;
	mode = CRON_ILLEGAL;
	optKill = false;
	runTimer = -1;
	killTimer = -1;
	childFds[0] = childFds[1] = childFds[2] = -1;
	stdOut = stdErr = -1;
	numOutputs = 0;							// No data produced yet
	eventHandler = NULL;
	eventService = NULL;

	// Build my output buffers
	stdOutBuf = new CronJobOut( this );
	stdErrBuf = new CronJobErr( this );

# if CRONJOB_PIPEIO_DEBUG
	TodoBufSize = 20 * 1024;
	TodoWriteNum = TodoBufWrap = TodoBufOffset = 0;
	TodoBuffer = (char *) malloc( TodoBufSize );
# endif

	// Store the name, etc.
	SetName( jobName );

	// Register my reaper
	reaperId = daemonCore->Register_Reaper( 
		"Cron_Reaper",
		(ReaperHandlercpp) &CronJobBase::Reaper,
		"Cron Reaper",
		this );
}

// CronJob destructor
CronJobBase::~CronJobBase( )
{
	dprintf( D_ALWAYS, "Cron: Deleting job '%s' (%s)\n", GetName(),
			 GetPath() );
	dprintf( D_FULLDEBUG, "Cron: Deleting timer for '%s'; ID = %d\n",
			 GetName(), runTimer );

	// Delete the timer FIRST
	if ( runTimer >= 0 ) {
		daemonCore->Cancel_Timer( runTimer );
	}

	// Kill job if it's still running
	KillJob( true );

	// Close FDs
	CleanAll( );

	// Delete the buffers
	delete stdOutBuf;
	delete stdErrBuf;
}

// Initialize
int
CronJobBase::Initialize( )
{
	// If we're already initialized, do nothing...
	if ( state != CRON_NOINIT )	{
		return 0;
	}

	// Update our state to idle..
	state = CRON_IDLE;

	dprintf( D_ALWAYS, "Cron: Initializing job '%s' (%s)\n", 
			 GetName(), GetPath() );

	// Schedule & see if we should run...
	return Schedule( );
}

// Set job characteristics: Name
int
CronJobBase::SetName( const char *newName )
{
	if ( NULL == newName ) {
		return -1;
	}
	name = newName;
	return 0;
}

// Set job characteristics: Prefix
int
CronJobBase::SetPrefix( const char *newPrefix )
{
	if ( NULL == newPrefix ) {
		return -1;
	}
	prefix = newPrefix;
	return 0;
}

// Set job characteristics: Path
int
CronJobBase::SetPath( const char *newPath )
{
	if ( NULL == newPath ) {
		return -1;
	}
	path = newPath;
	return 0;
}

// Set job characteristics: Command line args
int
CronJobBase::SetArgs( ArgList const &new_args )
{
	args.Clear();
	return AddArgs(new_args);
}

// Set job characteristics: Path
int
CronJobBase::SetConfigVal( const char *newPath )
{
	if ( NULL == newPath ) {
		configValProg = "";
	} else {
		configValProg = newPath;
	}
	return 0;
}

// Set job characteristics: Environment
int
CronJobBase::SetEnv( char const * const *env_array )
{
	env.Clear();
	return !env.MergeFrom(env_array);
}

int
CronJobBase::AddEnv( char const * const *env_array )
{
	return !env.MergeFrom(env_array);
}

// Set job characteristics: CWD
int
CronJobBase::SetCwd( const char *newCwd )
{
	cwd = newCwd;
	return 0;
}

// Set job characteristics: Kill option
int
CronJobBase::SetKill( bool kill )
{
	optKill = kill;
	return 0;
}

// Set job characteristics: Reconfig option
int
CronJobBase::SetReconfig( bool reconfig )
{
	optReconfig = reconfig;
	return 0;
}

// Add to the job's path
int
CronJobBase::AddPath( const char *newPath )
{
	if ( path.Length() ) {
		path += ":";
	}
	path += newPath;
	return 0;
}

// Add to the job's arg list
int
CronJobBase::AddArgs( ArgList const &new_args )
{
	args.AppendArgsFromArgList(new_args);
	return 1;
}

// Set job died handler
int
CronJobBase::SetEventHandler(
	CronEventHandler	NewHandler,
	Service				*s )
{
	// No nests, etc.
	if( eventHandler ) {
		return -1;
	}

	// Just store 'em & go
	eventHandler = NewHandler;
	eventService = s;
	return 0;
}

// Set job characteristics
int
CronJobBase::SetPeriod( CronJobMode newMode, unsigned newPeriod )
{
	// Verify that the mode seleted is valid
	if (  ( CRON_WAIT_FOR_EXIT != newMode ) && ( CRON_PERIODIC != newMode )  ) {
		dprintf( D_ALWAYS, "Cron: illegal mode selected for job '%s'\n",
				 GetName() );
		return -1;
	} else if (  ( CRON_PERIODIC == newMode ) && ( 0 == newPeriod )  ) {
		dprintf( D_ALWAYS, 
				 "Cron: Job '%s'; Periodic requires non-zero period\n",
				 GetName() );
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

	// Schedule a run..
	return Schedule( );
}

// Reconfigure a running job
int
CronJobBase::Reconfig( void )
{
	// Only do this to running jobs with the reconfig option set
	if (  ( ! optReconfig ) || ( CRON_RUNNING != state )  ) {
		return 0;
	}

	// Don't send the HUP before it's first output block
	if ( ! numOutputs ) {
		dprintf( D_ALWAYS,
				 "Not HUPing '%s' pid %d before it's first output\n",
				 GetName(), pid );
		return 0;
	}

	// HUP it; if it dies it'll get the new config when it restarts
	if ( pid >= 0 )
	{
			// we want this D_ALWAYS, since it's pretty rare anyone
			// actually wants a SIGHUP, and to aid in debugging, it's
			// best to always log it when we do so everyone sees it.
		dprintf( D_ALWAYS, "Cron: Sending HUP to '%s' pid %d\n",
				 GetName(), pid );
		return daemonCore->Send_Signal( pid, SIGHUP );
	}

	// Otherwise, all ok
	return 0;
}

// Schedule a run?
int
CronJobBase::Schedule( void )
{
	// If we're not initialized yet, do nothing...
	if ( CRON_NOINIT == state ) {
		return 0;
	}

	// Now, schedule the job to run..
	int	status = 0;

	// It's not a periodic -- just start it
	if ( CRON_WAIT_FOR_EXIT == mode ) {
		status = StartJob( );

	} else {				// Periodic
		// Set the job's timer
		status = SetTimer( period, period );

		// Start the first run..
		if (  ( 0 == status ) && ( CRON_IDLE == state )  ) {
			status = RunJob( );
		}
	}

	// Nothing to do for now
	return status;
}

// Schdedule the job to run
int
CronJobBase::RunJob( void )
{

	// Make sure that the job is idle!
	if ( ( state != CRON_IDLE ) && ( state != CRON_DEAD ) ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' is still running!\n", GetName() );

		// If we're not supposed to kill the process, just skip this timer
		if ( optKill ) {
			return KillJob( false );
		} else {
			return -1;
		}
	}

	// Check output queue!
	if ( stdOutBuf->FlushQueue( ) ) {
		dprintf( D_ALWAYS, "Cron: Job '%s': Queue not empty!\n", GetName() );
	}

	// Job not running, just start it
	dprintf( D_JOB, "Cron: Running job '%s' (%s)\n",
			 GetName(), GetPath() );

	// Start it up
	return RunProcess( );
}

// Handle the kill timer
void
CronJobBase::KillHandler( void )
{

	// Log that we're here
	dprintf( D_FULLDEBUG, "Cron: KillHandler for job '%s'\n", GetName() );

	// If we're idle, we shouldn't be here.
	if ( CRON_IDLE == state ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' already idle (%s)!\n", 
			GetName(), GetPath() );
		return;
	}

	// Kill it.
	KillJob( false );
}

// Start a job
int
CronJobBase::StartJob( void )
{
	if ( CRON_IDLE != state ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' not idle!\n", GetName() );
		return 0;
	}
	dprintf( D_JOB, "Cron: Starting job '%s' (%s)\n",
			 GetName(), GetPath() );

	// Check output queue!
	if ( stdOutBuf->FlushQueue( ) ) {
		dprintf( D_ALWAYS, "Cron: Job '%s': Queue not empty!\n", GetName() );
	}

	// Run it
	return RunProcess( );
}

// Child reaper
int
CronJobBase::Reaper( int exitPid, int exitStatus )
{
	if( WIFSIGNALED(exitStatus) ) {
		dprintf( D_FULLDEBUG, "Cron: '%s' (pid %d) exit_signal=%d\n",
				 GetName(), exitPid, WTERMSIG(exitStatus) );
	} else {
		dprintf( D_FULLDEBUG, "Cron: '%s' (pid %d) exit_status=%d\n",
				 GetName(), exitPid, WEXITSTATUS(exitStatus) );
	}

	// What if the PIDs don't match?!
	if ( exitPid != pid ) {
		dprintf( D_ALWAYS, "Cron: WARNING: Child PID %d != Exit PID %d\n",
				 pid, exitPid );
	}
	pid = 0;

	// Read the stderr & output
	if ( stdOut >= 0 ) {
		StdoutHandler( stdOut );
	}
	if ( stdErr >= 0 ) {
		StderrHandler( stdErr );
	}

	// Clean up it's file descriptors
	CleanAll( );

	// We *should* be in CRON_RUNNING state now; check this...
	switch ( state )
	{
		// Normal death
	case CRON_RUNNING:
		state = CRON_IDLE;				// Note it's death
		if ( CRON_WAIT_FOR_EXIT == mode ) {
			if ( 0 == period ) {			// ExitTime mode, no delay
				StartJob( );
			} else {						// ExitTime mode with delay
				SetTimer( period, TIMER_NEVER );
			}
		}
		break;

		// Huh?  Should never happen
	case CRON_IDLE:
	case CRON_DEAD:
		dprintf( D_ALWAYS, "CronJob::Reaper:: Job %s in state %s: Huh?\n",
				 GetName(), StateString() );
		break;							// Do nothing

		// Waiting for it to die...
	case CRON_TERMSENT:
	case CRON_KILLSENT:
		break;		// Do nothing at all

		// We've sent the process a signal, waiting for it to die
	default:
		state = CRON_IDLE;				// Note that it's dead

		// Cancel the kill timer if required
		KillTimer( TIMER_NEVER );

		// Re-start the job
		if ( CRON_PERIODIC == mode ) {		// Periodic
			RunJob( );
		} else if ( 0 == period ) {			// ExitTime mode, no delay
			StartJob( );
		} else {							// ExitTime mode with delay
			SetTimer( period, TIMER_NEVER );
		}
		break;

	}

	// Note that we're dead
	if ( CRON_KILL == mode ) {
		state = CRON_DEAD;
	}

	// Process the output
	ProcessOutputQueue( );

	// Finally, notify my manager
	if( eventHandler ) {
		(eventService->*eventHandler)( this, CONDOR_CRON_JOB_DIED );
	}

	return 0;
}

// Publisher
int
CronJobBase::ProcessOutputQueue( void )
{
	int		status = 0;
	int		linecount = stdOutBuf->GetQueueSize( );

	// If there's data, process it...
	if ( linecount != 0 ) {
		dprintf( D_FULLDEBUG, "%s: %d lines in Queue\n",
				 GetName(), linecount );

		// Read all of the data from the queue
		char	*linebuf;
		while( ( linebuf = stdOutBuf->GetLineFromQueue( ) ) != NULL ) {
			int		tmpstatus = ProcessOutput( linebuf );
			if ( tmpstatus ) {
				status = tmpstatus;
			}
			free( linebuf );
			linecount--;
		}

		// Sanity checks
		int		tmp = stdOutBuf->GetQueueSize( );
		if ( 0 != linecount ) {
			dprintf( D_ALWAYS, "%s: %d lines remain!!\n",
					 GetName(), linecount );
		} else if ( 0 != tmp ) {
			dprintf( D_ALWAYS, "%s: Queue reports %d lines remain!\n",
					 GetName(), tmp );
		} else {
			// The NULL output means "end of block", so go publish
			ProcessOutput( NULL );
			numOutputs++;				// Increment # of valid output blocks
		}
	}
	return 0;
}

// Start a job
int
CronJobBase::RunProcess( void )
{
	ArgList final_args;

	// Create file descriptors
	if ( OpenFds( ) < 0 ) {
		dprintf( D_ALWAYS, "Cron: Error creating FDs for '%s'\n",
				 GetName() );
		return -1;
	}

	// Add the name to the argument list, then any specified in the config
	final_args.AppendArg( name.Value() );
	if(args.Count()) {
		final_args.AppendArgsFromArgList(args);
	}

	// Create the priv state for the process
	priv_state priv;
# ifdef WIN32
	// WINDOWS
	priv = PRIV_CONDOR;
# else
	// UNIX
	priv = PRIV_USER_FINAL;
	uid_t uid = get_condor_uid( );
	if ( uid == (uid_t) -1 )
	{
		dprintf( D_ALWAYS, "Cron: Invalid UID -1\n" );
		return -1;
	}
	gid_t gid = get_condor_gid( );
	if ( gid == (uid_t) -1 )
	{
		dprintf( D_ALWAYS, "Cron: Invalid GID -1\n" );
		return -1;
	}
	set_user_ids( uid, gid );
# endif

	// Create the process, finally..
	pid = daemonCore->Create_Process(
		path.Value(),		// Path to executable
		final_args,			// argv
		priv,				// Priviledge level
		reaperId,			// ID Of reaper
		FALSE,				// Command port?  No
		&env, 		 		// Env to give to child
		cwd.Value(),		// Starting CWD
		NULL,				// Process family info
		NULL,				// Socket list
		childFds,			// Stdin/stdout/stderr
		0 );				// Nice increment

	// Restore my priv state.
	uninit_user_ids( );

	// Close the child FDs
	CleanFd( &childFds[0] );
	CleanFd( &childFds[1] );
	CleanFd( &childFds[2] );

	// Did it work?
	if ( pid <= 0 ) {
		dprintf( D_ALWAYS, "Cron: Error running job '%s'\n", GetName() );
		CleanAll( );
		return -1;
	}

	// All ok here
	state = CRON_RUNNING;

	// Finally, notify my manager
	if( eventHandler ) {
		(eventService->*eventHandler)( this, CONDOR_CRON_JOB_START );
	}

	// All ok!
	return 0;
}

// Debugging
# if CRONJOB_PIPEIO_DEBUG
void
CronJobBase::TodoWrite( void )
{
	char	fname[1024];
	FILE	*fp;
	snprintf( fname, 1024, "todo.%s.%06d.%02d", name, getpid(), TodoWriteNum++ );
	dprintf( D_ALWAYS, "%s: Writing input log '%s'\n", GetName(), fname );

	if ( ( fp = safe_fopen_wrapper( fname, "w" ) ) != NULL ) {
		if ( TodoBufWrap ) {
			fwrite( TodoBuffer + TodoBufOffset,
					TodoBufSize - TodoBufOffset,
					1,
					fp );
		}
		fwrite( TodoBuffer, TodoBufOffset, 1, fp );
		fclose( fp );
	}
	TodoBufOffset = 0;
	TodoBufWrap = 0;
}
# endif

// Data is available on Standard Out.  Read it!
//  Note that we set the pipe to be non-blocking when we created it
int
CronJobBase::StdoutHandler ( int   /*pipe*/ )
{
	char			buf[STDOUT_READBUF_SIZE];
	int				bytes;
	int				reads = 0;

	// Read 'til we suck up all the data (or loop too many times..)
	while ( ( ++reads < 10 ) && ( stdOut >= 0 ) ) {

		// Read a block from it
		bytes = daemonCore->Read_Pipe( stdOut, buf, STDOUT_READBUF_SIZE );

		// Zero means it closed
		if ( bytes == 0 ) {
			dprintf(D_FULLDEBUG, "Cron: STDOUT closed for '%s'\n", GetName());
			daemonCore->Close_Pipe( stdOut );
			stdOut = -1;
		}

		// Positve value is byte count
		else if ( bytes > 0 ) {
			const char	*bptr = buf;

			// TODO
# 		  if CRONJOB_PIPEIO_DEBUG
			if ( TodoBuffer ) {
				char	*OutPtr = TodoBuffer + TodoBufOffset;
				int		Count = bytes;
				char	*InPtr = buf;
				while( Count-- ) {
					*OutPtr++ = *InPtr++;
					if ( ++TodoBufOffset >= TodoBufSize ) {
						TodoBufOffset = 0;
						TodoBufWrap++;
						OutPtr = TodoBuffer;
					}
				}
			}
#		  endif
			// End TODO

			// stdOutBuf->Output() returns 1 if it finds '-', otherwise 0,
			// so that's what Buffer returns, too...
			while ( stdOutBuf->Buffer( &bptr, &bytes ) > 0 ) {
				ProcessOutputQueue( );
			}
		}

		// Negative is an error; check for EWOULDBLOCK
		else if (  ( EWOULDBLOCK == errno ) || ( EAGAIN == errno )  ) {
			break;			// No more data; break out; we're done
		}

		// Something bad
		else {
			dprintf( D_ALWAYS,
					 "Cron: read STDOUT failed for '%s' %d: '%s'\n",
					 GetName(), errno, strerror( errno ) );
			return -1;
		}
	}
	return 0;
}

// Data is available on Standard Error.  Read it!
//  Note that we set the pipe to be non-blocking when we created it
int
CronJobBase::StderrHandler ( int   /*pipe*/ )
{
	char			buf[STDERR_READBUF_SIZE];
	int				bytes;

	// Read a block from it
	bytes = daemonCore->Read_Pipe( stdErr, buf, STDERR_READBUF_SIZE );

	// Zero means it closed
	if ( bytes == 0 )
	{
		dprintf( D_FULLDEBUG, "Cron: STDERR closed for '%s'\n", GetName() );
		daemonCore->Close_Pipe( stdErr );
		stdErr = -1;
	}

	// Positve value is byte count
	else if ( bytes > 0 )
	{
		const char	*bptr = buf;

		while( stdErrBuf->Buffer( &bptr, &bytes ) > 0 ) {
			// Do nothing for now
		}
	}

	// Negative is an error; check for EWOULDBLOCK
	else if (  ( errno != EWOULDBLOCK ) && ( errno != EAGAIN )  )
	{
		dprintf( D_ALWAYS,
				 "Cron: read STDERR failed for '%s' %d: '%s'\n",
				 GetName(), errno, strerror( errno ) );
		return -1;
	}


	// Flush the buffers
	stdErrBuf->Flush();
	return 0;
}

// Create the job's file descriptors
int
CronJobBase::OpenFds ( void )
{
	int	tmpfds[2];

	// stdin goes to the bit bucket
	childFds[0] = -1;

	// Pipe to stdout
	if ( !daemonCore->Create_Pipe( tmpfds,	// pipe ends
				       true,	// read end registerable
				       false,	// write end not registerable
				       true	// read end nonblocking
				     ) )
	{
		dprintf( D_ALWAYS, "Cron: Can't create pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	stdOut = tmpfds[0];
	childFds[1] = tmpfds[1];
	daemonCore->Register_Pipe( stdOut,
							   "Standard Out",
							   (PipeHandlercpp) & CronJobBase::StdoutHandler,
							   "Standard Out Handler",
							   this );

	// Pipe to stderr
	if ( !daemonCore->Create_Pipe( tmpfds,	// pipe ends	
				       true,	// read end registerable
				       false,	// write end not registerable
				       true	// read end nonblocking
				    ) )
	{
		dprintf( D_ALWAYS, "Cron: Can't create STDERR pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	stdErr = tmpfds[0];
	childFds[2] = tmpfds[1];
	daemonCore->Register_Pipe( stdErr,
							   "Standard Error",
							   (PipeHandlercpp) & CronJobBase::StderrHandler,
							   "Standard Error Handler",
							   this );

	// Done; all ok
	return 0;
}

// Clean up all file descriptors & FILE pointers
void
CronJobBase::CleanAll ( void )
{
	CleanFd( &stdOut );
	CleanFd( &stdErr );
	CleanFd( &childFds[0] );
	CleanFd( &childFds[1] );
	CleanFd( &childFds[2] );
}

// Clean up a FILE *
void
CronJobBase::CleanFile ( FILE **file )
{
	if ( NULL != *file ) {
		fclose( *file );
		*file = NULL;
	}
}

// Clean up a file descriptro
void
CronJobBase::CleanFd ( int *fd )
{
	if ( *fd >= 0 ) {
		daemonCore->Close_Pipe( *fd );
		*fd = -1;
	}
}

// Kill a job
int
CronJobBase::KillJob( bool force )
{
	// Change our mode
	mode = CRON_KILL;

	// Idle?
	if ( ( CRON_IDLE == state ) || ( CRON_DEAD == state ) ) {
		return 0;
	}

	// Not running?
	if ( pid <= 0 ) {
		dprintf( D_ALWAYS, "Cron: '%s': Trying to kill illegal PID %d\n",
				 GetName(), pid );
		return -1;
	}

	// Kill the process *hard*?
	if ( ( force ) || ( CRON_TERMSENT == state )  ) {
		dprintf( D_JOB, "Cron: Killing job '%s' with SIGKILL, pid = %d\n", 
				 GetName(), pid );
		if ( daemonCore->Send_Signal( pid, SIGKILL ) == 0 ) {
			dprintf( D_ALWAYS,
					 "Cron: job '%s': Failed to send SIGKILL to %d\n",
					 GetName(), pid );
		}
		state = CRON_KILLSENT;
		KillTimer( TIMER_NEVER );	// Cancel the timer
		return 0;
	} else if ( CRON_RUNNING == state ) {
		dprintf( D_JOB, "Cron: Killing job '%s' with SIGTERM, pid = %d\n", 
				 GetName(), pid );
		if ( daemonCore->Send_Signal( pid, SIGTERM ) == 0 ) {
			dprintf( D_ALWAYS,
					 "Cron: job '%s': Failed to send SIGTERM to %d\n",
					 GetName(), pid );
		}
		state = CRON_TERMSENT;
		KillTimer( 1 );				// Schedule hard kill in 1 sec
		return 1;
	} else {
		return -1;					// Nothing else to do!
	}
}

// Set the job timer
int
CronJobBase::SetTimer( unsigned first, unsigned period_arg )
{
	// Reset the timer
	if ( runTimer >= 0 )
	{
		daemonCore->Reset_Timer( runTimer, first, period_arg );
		if( period_arg == TIMER_NEVER ) {
			dprintf( D_FULLDEBUG, "Cron: timer ID %d reset to first: %u, "
					 "period: NEVER\n", runTimer, first );
		} else {
			dprintf( D_FULLDEBUG, "Cron: timer ID %d reset to first: %u, "
					 "period: %u\n", runTimer, first, period );
		}
	}

	// Create a periodic timer
	else
	{
		// Debug
		dprintf( D_FULLDEBUG, 
				 "Cron: Creating timer for job '%s'\n", GetName() );
		TimerHandlercpp handler =
			(  ( CRON_WAIT_FOR_EXIT == mode ) ? 
			   (TimerHandlercpp)& CronJobBase::StartJob :
			   (TimerHandlercpp)& CronJobBase::RunJob );
		runTimer = daemonCore->Register_Timer(
			first,
			period_arg,
			handler,
			"RunJob",
			this );
		if ( runTimer < 0 ) {
			dprintf( D_ALWAYS, "Cron: Failed to create timer\n" );
			return -1;
		}
		if( period_arg == TIMER_NEVER ) {
			dprintf( D_FULLDEBUG, "Cron: new timer ID %d set to first: %u, "
					 "period: NEVER\n", runTimer, first );
		} else {
			dprintf( D_FULLDEBUG, "Cron: new timer ID %d set to first: %u, "
					 "period: %u\n", runTimer, first, period );
		}
	} 

	return 0;
}

// Start the kill timer
int
CronJobBase::KillTimer( unsigned seconds )
{
	// Cancel request?
	if ( TIMER_NEVER == seconds ) {
		dprintf( D_FULLDEBUG, "Cron: Canceling kill timer for '%s'\n",
				 GetName() );
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
				 GetName() );
		killTimer = daemonCore->Register_Timer(
			seconds,
			0,
			(TimerHandlercpp)& CronJobBase::KillHandler,
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
CronJobBase::StateString( CronJobState state_arg )
{
	switch( state_arg )
	{
	case CRON_IDLE:
		return "Idle";
	case CRON_RUNNING:
		return "Running";
	case CRON_TERMSENT:
		return "TermSent";
	case CRON_KILLSENT:
		return "KillSent";
	case CRON_DEAD:
		return "Dead";
	default:
		return "Unknown";
	}
}

// Same, but uses the job's current state
const char *
CronJobBase::StateString( void )
{
	return StateString( state );
}
