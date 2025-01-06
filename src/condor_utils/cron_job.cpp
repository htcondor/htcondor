/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include <algorithm>
#include <limits.h>
#include <string.h>
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_cron_job_mgr.h"
#include "condor_cron_job_params.h"

// Size of the buffer for reading from the child process
#define STDOUT_READBUF_SIZE	1024
#define STDERR_READBUF_SIZE	128

// CronJob constructor
CronJob::CronJob( CronJobParams *params, CronJobMgr &mgr )
		: m_params( params ),
		  m_mgr( mgr ),
		  m_state( CRON_INITIALIZING ),
		  m_in_shutdown( false ),
		  m_run_timer( -1 ),
		  m_pid( -1 ),
		  m_stdOut( -1 ),
		  m_stdErr( -1 ),
		  m_reaperId( -1 ),
		  m_stdOutBuf( NULL ),
		  m_stdErrBuf( NULL ),
		  m_killTimer( -1 ),
		  m_num_outputs( 0 ),				// No data produced yet
		  m_num_runs( 0 ),					// Hasn't run yet
		  m_num_fails( 0 ),
		  m_last_start_time( 0 ),
		  m_last_exit_time( 0 ),
		  m_run_load( 0.0 ),
		  m_marked( false ),
		  m_old_period( 0 )
{
	for( int i = 0; i < 3;  i++ ) {
		m_childFds[i] = -1;
	}

	// Build my output buffers
	m_stdOutBuf = new CronJobOut( *this );
	m_stdErrBuf = new CronJobErr( *this );

	// Register my reaper
	m_reaperId = daemonCore->Register_Reaper( 
		"Cron_Reaper",
		(ReaperHandlercpp) &CronJob::Reaper,
		"Cron Reaper",
		this );
}

// CronJob destructor
CronJob::~CronJob( )
{
	dprintf( D_ALWAYS, "CronJob: Deleting job '%s' (%s), timer %d\n",
			 GetName(), GetExecutable(), m_run_timer );

	// Delete the timer & reaper FIRST
	CancelRunTimer( );
	if ( m_reaperId >= 0 ) {
		daemonCore->Cancel_Reaper( m_reaperId );
	}

	// Kill job if it's still running
	KillJob( true );

	// Close FDs
	CleanAll( );

	// Delete the buffers
	delete m_stdOutBuf; m_stdOutBuf = nullptr;
	delete m_stdErrBuf; m_stdErrBuf = nullptr;

	delete m_params;
}

// Initialize
int
CronJob::Initialize( void )
{
	// If we're already initialized, do nothing...
	if ( IsInitialized() ) {
		return 0;
	}

	// Update our state to idle..
	SetState( CRON_IDLE );

	dprintf( D_ALWAYS, "CronJob: Initializing job '%s' (%s)\n", 
			 GetName(), GetExecutable() );
	return 0;
}

// Set job parameters
bool
CronJob::SetParams( CronJobParams *params )
{
	m_old_period = m_params->GetPeriod( );
	delete m_params;
	m_params = params;
	return true;
}

// Reconfigure a running job
int
CronJob::HandleReconfig( void )
{
	// Handle "one shot" jobs
	if (  Params().OptReconfigRerun()  &&  m_num_runs ) {
		SetState( CRON_READY );
		return 0;
	}

	// Only do this to running jobs with the reconfig option set
	if (  IsRunning()  &&  Params().OptReconfig()  ) {
		return SendHup( );
	}

	// For idle periodic jobs, adjust the timer if required
	if (  IsIdle() && ( IsPeriodic() || IsWaitForExit() )  ) {
		if ( Period() == m_old_period ) {
			return 0;
		}
		time_t now = time(nullptr);
		time_t last_time;
		unsigned period;
		if ( IsPeriodic() ) {
			last_time = m_last_start_time;
			period = Period();
		}
		else {
			last_time = m_last_exit_time;
			period = TIMER_NEVER;
		}

		// If the timer should have expired, reset it & set
		if ( last_time + Period() < now ) {
			CancelRunTimer( );
			SetState( CRON_READY );
			if ( IsPeriodic() )  {
				return SetTimer( Period(), period );
			}
			return 0;
		}
		else {
			return SetTimer( (unsigned)(last_time + Period() - now), period );
		}
	}

	return 0;
}

// Schedule a run?
int
CronJob::Schedule( void )
{
	dprintf( D_FULLDEBUG,
			 "CronJob::Schedule '%s' "
			 "IR=%c IP=%c IWE=%c IOS=%c IOD=%c nr=%d nf=%d\n",
			 GetName(),
			 IsReady() ? 'T' : 'F',
			 IsPeriodic() ? 'T' : 'F',
			 IsWaitForExit() ? 'T' : 'F',
			 IsOneShot() ? 'T' : 'F',
			 IsOnDemand() ? 'T' : 'F',
			 m_num_runs,
			 m_num_fails );

	// If we're not initialized yet, do nothing...
	if ( ! IsInitialized() ) {
		return 0;
	}

	// Now, schedule the job to run..
	int	status = 0;

	// Are we in the ready state?  Just start it if so
	if ( IsReady() ) {
		status = StartJob( );
	}

	// Period job?  Schedule it to run
	else if ( IsPeriodic() ) {

		// Start the first run..
		if (  ( 0 == m_num_runs ) && ( 0 == m_num_fails )  ) {
			status = RunJob( );
		}
	}

	// "Wait for exit" job?  Start at init time
	else if ( IsWaitForExit() ) {
		if (  ( 0 == m_num_runs ) && ( 0 == m_num_fails )  ) {
			status = StartJob( );
		}
	}

	// One shot?  Only start it if it hasn't been already run
	else if ( IsOneShot() ) {
		if (  ( 0 == m_num_runs ) && ( 0 == m_num_fails )  ) {
			status = StartJob( );
		}
	}

	// On demand jobs
	else if ( IsOnDemand() ) {
		// Do nothing
	}

	// Nothing to do for now
	return status;
}

// Start an on-demand job
int
CronJob::StartOnDemand( void )
{
	if ( IsOnDemand() && IsIdle() ) {
		SetState( CRON_READY );
		return StartJob( );
	}
	return 0;
}

// Send HUP to job
int
CronJob::SendHup( void ) const
{

	// Don't send the HUP before it's first output block
	if ( ! m_num_outputs ) {
		dprintf( D_ALWAYS,
				 "Not HUPing '%s' pid %d before it's first output\n",
				 GetName(), m_pid );
		return 0;
	}

	// Verify that it's got a valid PID
	if ( m_pid <= 0 ) {
		return 0;
	}

	// we want this D_ALWAYS, since it's pretty rare anyone
	// actually wants a SIGHUP, and to aid in debugging, it's
	// best to always log it when we do so everyone sees it.
	dprintf( D_ALWAYS, "CronJob: Sending HUP to '%s' pid %d\n",
			 GetName(), m_pid );
	return daemonCore->Send_Signal( m_pid, SIGHUP );
}

// Schdedule the job to run
int
CronJob::RunJob( void )
{

	// Make sure that the job is idle before running it
	if ( IsAlive() ) {
		dprintf( D_ALWAYS,
				 "CronJob: Job '%s' is still running!\n", GetName() );

		// If we're not supposed to kill the process, just skip this timer
		if ( Params().OptKill() ) {
			return KillJob( false );
		} else {
			return -1;
		}
	}

	return StartJob( );
}

// Start a job
int
CronJob::StartJob( void )
{
	// Sanity check
	if ( !IsIdle() && !IsReady() ) {
		dprintf( D_ALWAYS, "CronJob: Job '%s' not idle!\n", GetName() );
		return 0;
	}

	// Too busy to run me?
	if ( ! m_mgr.ShouldStartJob( *this ) ) {
		SetState( CRON_READY );
		dprintf( D_FULLDEBUG,
				 "CronJob: Too busy to run job '%s'\n", GetName() );
		return 0;
	}

	dprintf( D_FULLDEBUG, "CronJob: Starting job '%s' (%s)\n",
			 GetName(), GetExecutable() );

	// Check output queue!
	if ( m_stdOutBuf->FlushQueue( ) ) {
		dprintf( D_ALWAYS,
				 "CronJob: Job '%s': Queue not empty!\n", GetName() );
	}

	// Run it
	return StartJobProcess( );
}

// Handle the kill timer
void
CronJob::KillHandler( int /* timerID */ )
{

	// Log that we're here
	dprintf( D_FULLDEBUG, "CronJob: KillHandler for job '%s'\n", GetName() );

	// If we're idle, we shouldn't be here.
	if ( IsIdle() ) {
		dprintf( D_ALWAYS, "CronJob: Job '%s' already idle (%s)!\n", 
			GetName(), GetExecutable() );
		return;
	}

	// Kill it.
	KillJob( false );
}

// Child reaper
int
CronJob::Reaper( int exitPid, int exitStatus )
{
	bool failed = false;
	if( WIFSIGNALED(exitStatus) ) {
		failed = true;
		dprintf( D_ALWAYS, "CronJob: '%s' (pid %d) exit_signal=%d\n",
			 GetName(), exitPid, WTERMSIG(exitStatus) );
	} else {
		auto debugLevel = D_FULLDEBUG;
		auto es = WEXITSTATUS(exitStatus);

		std::string knob_name;
		formatstr( knob_name, "%s_CRON_LOG_NON_ZERO_EXIT", m_mgr.GetName() );
		if( es != 0 && param_boolean( knob_name.c_str(), false ) ) {
			failed = true;
			debugLevel = D_ALWAYS;
		}
		dprintf( debugLevel, "CronJob: '%s' (pid %d) exit_status=%d\n",
			 GetName(), exitPid, es );
	}

	// What if the PIDs don't match?!
	if ( exitPid != m_pid ) {
		dprintf( D_ALWAYS,
			"CronJob: WARNING: Child PID %d != Exit PID %d\n",
			m_pid, exitPid );
	}

	m_pid = 0;
	m_last_exit_time = time(NULL);
	m_run_load = 0.0;

	// Read the stderr & output
	if ( m_stdOut >= 0 ) {
		StdoutHandler( m_stdOut );
	}
	if ( m_stdErr >= 0 ) {
		StderrHandler( m_stdErr );
	}

	// Clean up it's file descriptors
	CleanAll( );

	// We *should* be in CRON_RUNNING state now; check this...
	switch ( GetState() )
	{
		// Normal death
	case CRON_RUNNING:
		SetState( CRON_IDLE );				// Note it's death
		if ( IsWaitForExit() ) {
			if ( 0 == Period() ) {			// ExitTime mode, no delay
				StartJob( );
			} else {						// ExitTime mode with delay
				SetTimer( Period(), TIMER_NEVER );
			}
		}
		break;

		// Huh?  Should never happen
	case CRON_IDLE:
	case CRON_DEAD:
		dprintf( D_ALWAYS, "CronJob::Reaper:: Job %s in state %s: Huh?\n",
				 GetName(), StateString() );
		break;							// Do nothing

		// died either directly by term sig or kill sig, or other means while in sig code path.
	case CRON_TERM_SENT:
	case CRON_KILL_SENT:
		m_in_shutdown = false;
		// Fall through...	
		//@fallthrough@
	default:
		SetState( CRON_IDLE );			// Note that it's dead

		// Cancel the kill timer if required
		KillTimer( TIMER_NEVER );

		// Re-start the job
		if ( IsWaitForExit() ) {
			if ( 0 == Period() ) {		// WaitForExit mode, no delay
				StartJob( );
			} else {					// WaitForExit mode with delay
				SetTimer( Period(), TIMER_NEVER );
			}
		}
		else if ( IsPeriodic() ) {		// Periodic
			RunJob( );
		}
		break;

	}


	if( failed ) {
		int linecount = m_stdOutBuf->GetQueueSize();
		if( (linecount == 0) && m_stdErrBuf->m_content.empty() ) {
			dprintf( D_ALWAYS, "CronJob: '%s' (pid %d) produced no output\n",
				GetName(), exitPid );
		} else if( linecount != 0 ) {
			dprintf( D_ALWAYS, "CronJob: '%s' (pid %d) produced %d lines of standard output, which follow.\n",
				GetName(), exitPid, linecount );
		}
	}

	// Process the output
	// Also logs stdout if `failed` is true.
	ProcessOutputQueue( failed, exitPid );

	if( failed ) {
		if (!m_stdErrBuf->m_content.empty()) {
			size_t errLines = std::count(m_stdErrBuf->m_content.begin(), m_stdErrBuf->m_content.end(), '\n');
			dprintf( D_ALWAYS, "CronJob: '%s' (pid %d) produced %zu lines of standard error, which follow.\n",
					GetName(), exitPid, errLines );
			dprintf(D_ALWAYS, "%s", m_stdErrBuf->m_content.c_str());
		}
	}
	if (m_stdErrBuf) {
		m_stdErrBuf->m_content.clear();
	}


	// Finally, notify my manager
	m_mgr.JobExited( *this );

	return 0;
}

// Publisher
int
CronJob::ProcessOutputQueue( bool failed, int exitPid )
{
	int		status = 0;
	int		linecount = m_stdOutBuf->GetQueueSize( );

	// If there's data, process it...
	if ( linecount != 0 ) {
		dprintf( D_FULLDEBUG, "%s: %d lines in Queue\n",
				 GetName(), linecount );

		status = ProcessOutputSep( m_stdOutBuf->GetQueueSep() );

		// Read all of the data from the queue
		char	*linebuf;
		while( ( linebuf = m_stdOutBuf->GetLineFromQueue( ) ) != NULL ) {
			if( failed ) {
				dprintf( D_ALWAYS, "['%s' (%d)] %s\n", GetName(), exitPid, linebuf );
			}

			int		tmpstatus = ProcessOutput( linebuf );
			if ( tmpstatus ) {
				status = tmpstatus;
			}
			free( linebuf );
			linecount--;
		}

		// Sanity checks
		int		tmp = m_stdOutBuf->GetQueueSize( );
		if ( 0 != linecount ) {
			dprintf( D_ALWAYS, "%s: %d lines remain!!\n",
					 GetName(), linecount );
		} else if ( 0 != tmp ) {
			dprintf( D_ALWAYS, "%s: Queue reports %d lines remain!\n",
					 GetName(), tmp );
		} else {
			// The NULL output means "end of block", so go publish
			ProcessOutput( NULL );
			m_num_outputs++;			// Increment # of valid output blocks
		}
	}
	return status;
}

// Start a job
int
CronJob::StartJobProcess( void )
{
	ArgList final_args;

	// Create file descriptors
	if ( OpenFds( ) < 0 ) {
		dprintf( D_ALWAYS, "CronJob: Error creating FDs for '%s'\n",
				 GetName() );
		return -1;
	}

	// Add the name to the argument list, then any specified in the config
	final_args.AppendArg( GetName() );
	if( Params().GetArgs().Count() ) {
		final_args.AppendArgsFromArgList( Params().GetArgs() );
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
		dprintf( D_ALWAYS, "CronJob: Invalid UID -1\n" );
		return -1;
	}
	gid_t gid = get_condor_gid( );
	if ( gid == (uid_t) -1 )
	{
		dprintf( D_ALWAYS, "CronJob: Invalid GID -1\n" );
		return -1;
	}
	set_user_ids( uid, gid );
# endif

	// Create the process, finally..
	m_pid = daemonCore->Create_Process(
		GetExecutable(),	// Path to executable
		final_args,			// argv
		priv,				// Priviledge level
		m_reaperId,			// ID Of reaper
		FALSE,				// Command port?  No
		FALSE,				// Command port?  No
		&Params().GetEnv(), // Env to give to child
		Params().GetCwd(),	// Starting CWD
		NULL,				// Process family info
		NULL,				// Socket list
		m_childFds,			// Stdin/stdout/stderr
		0 );				// Nice increment

	// Restore my priv state.
	uninit_user_ids( );

	// Close the child FDs
	CleanFd( &m_childFds[0] );
	CleanFd( &m_childFds[1] );
	CleanFd( &m_childFds[2] );

	// Did it work?
	if ( m_pid <= 0 ) {
		dprintf( D_ALWAYS, "CronJob: Error running job '%s'\n", GetName() );
		CleanAll();
		SetState( CRON_IDLE );
		m_num_fails++;

		// Finally, notify my manager to transition the startd
		// from benchmarking back to idle activity if there are no
		// more benchmarks to be scheduled.
		m_mgr.JobExited( *this );
		return -1;
	}

	// All ok here
	SetState( CRON_RUNNING );
	m_last_start_time = time(NULL);
	m_run_load = GetJobLoad();
	m_num_runs++;

	// Finally, notify my manager
	m_mgr.JobStarted( *this );

	// All ok!
	return 0;
}

// Data is available on Standard Out.  Read it!
//  Note that we set the pipe to be non-blocking when we created it
int
CronJob::StdoutHandler ( int   /*pipe*/ )
{
	char			buf[STDOUT_READBUF_SIZE];
	int				bytes;
	int				reads = 0;

	// Read 'til we suck up all the data (or loop too many times..)
	while ( ( ++reads < 10 ) && ( m_stdOut >= 0 ) ) {

		// Read a block from it
		bytes = daemonCore->Read_Pipe( m_stdOut, buf, STDOUT_READBUF_SIZE );

		// Zero means it closed
		if ( bytes == 0 ) {
			dprintf(D_FULLDEBUG,
					"CronJob: STDOUT closed for '%s'\n", GetName());
			daemonCore->Close_Pipe( m_stdOut );
			m_stdOut = -1;
		}

		// Positve value is byte count
		else if ( bytes > 0 ) {
			const char	*bptr = buf;

			// stdOutBuf->Output() returns 1 if it finds '-', otherwise 0,
			// so that's what Buffer returns, too...
			while ( m_stdOutBuf->Buffer( &bptr, &bytes ) > 0 ) {
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
					 "CronJob: read STDOUT failed for '%s' %d: '%s'\n",
					 GetName(), errno, strerror( errno ) );
			return -1;
		}
	}
	return 0;
}

// Data is available on Standard Error.  Read it!
//  Note that we set the pipe to be non-blocking when we created it
int
CronJob::StderrHandler ( int   /*pipe*/ )
{
	char			buf[STDERR_READBUF_SIZE];
	int				bytes;

	// In case we are called after we closed the pipe, do nothing or we might abort. HTCONDOR-1343
	if (m_stdErr < 0) {
		if (m_stdErrBuf) m_stdErrBuf->Flush();
		return 0;
	}

	// Read a block from it
	bytes = daemonCore->Read_Pipe( m_stdErr, buf, STDERR_READBUF_SIZE );

	// Zero means it closed
	if ( bytes == 0 )
	{
		dprintf( D_FULLDEBUG,
				 "CronJob: STDERR closed for '%s'\n", GetName() );
		daemonCore->Close_Pipe( m_stdErr );
		m_stdErr = -1;
	}

	// Positive value is byte count
	else if ( bytes > 0 )
	{
		std::string line{buf, (std::string::size_type)bytes};
		m_stdErrBuf->m_content += line;
	}

	// Negative is an error; check for EWOULDBLOCK
	else if (  ( errno != EWOULDBLOCK ) && ( errno != EAGAIN )  )
	{
		dprintf( D_ALWAYS,
				 "CronJob: read STDERR failed for '%s' %d: '%s'\n",
				 GetName(), errno, strerror( errno ) );
		return -1;
	}

	return 0;
}

// Create the job's file descriptors
int
CronJob::OpenFds ( void )
{
	int	tmpfds[2];

	// stdin goes to the bit bucket
	m_childFds[0] = -1;

	// Pipe to stdout
	if ( !daemonCore->Create_Pipe( tmpfds,	// pipe ends
								   true,	// read end registerable
								   false,	// write end not registerable
								   true		// read end nonblocking
								   ) ) {
		dprintf( D_ALWAYS, "CronJob: Can't create pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	m_stdOut = tmpfds[0];
	m_childFds[1] = tmpfds[1];
	daemonCore->Register_Pipe( m_stdOut,
							   "Standard Out",
							   static_cast<PipeHandlercpp>(&CronJob::StdoutHandler),
							   "Standard Out Handler",
							   this );

	// Pipe to stderr
	if ( !daemonCore->Create_Pipe( tmpfds,	// pipe ends	
								   true,	// read end registerable
								   false,	// write end not registerable
								   true		// read end nonblocking
				    ) ) {
		dprintf( D_ALWAYS,
				 "CronJob: Can't create STDERR pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	m_stdErr = tmpfds[0];
	m_childFds[2] = tmpfds[1];
	daemonCore->Register_Pipe( m_stdErr,
							   "Standard Error",
							   static_cast<PipeHandlercpp>(&CronJob::StderrHandler),
							   "Standard Error Handler",
							   this );

	// Done; all ok
	return 0;
}

// Clean up all file descriptors & FILE pointers
void
CronJob::CleanAll ( void )
{
	CleanFd( &m_stdOut );
	CleanFd( &m_stdErr );
	CleanFd( &m_childFds[0] );
	CleanFd( &m_childFds[1] );
	CleanFd( &m_childFds[2] );
}

// Clean up a FILE *
void
CronJob::CleanFile ( FILE **file )
{
	if ( NULL != *file ) {
		fclose( *file );
		*file = NULL;
	}
}

// Clean up a file descriptro
void
CronJob::CleanFd ( int *fd )
{
	if ( *fd >= 0 ) {
		daemonCore->Close_Pipe( *fd );
		*fd = -1;
	}
}

// Kill a job
int
CronJob::KillJob( bool force )
{
	// Change our mode
	m_in_shutdown = true;

	// Idle?
	if (IsIdle() || IsDead() || IsReady()) {
		return 0;
	}

	// Not running?
	if ( m_pid <= 0 ) {
		dprintf( D_ALWAYS, "CronJob: '%s': Trying to kill illegal PID %d\n",
				 GetName(), m_pid );
		return -1;
	}

	// Kill the process *hard*?
	if ( IsReady() ) {
		SetState( CRON_IDLE );
		return 0;
	}
	else if ( ( force ) || IsTermSent() ) {
		dprintf( D_FULLDEBUG,
				 "CronJob: Killing job '%s' with SIGKILL, pid = %d\n", 
				 GetName(), m_pid );
		if ( daemonCore->Send_Signal( m_pid, SIGKILL ) == 0 ) {
			dprintf( D_ALWAYS,
					 "CronJob: job '%s': Failed to send SIGKILL to %d\n",
					 GetName(), m_pid );
		}
		SetState( CRON_KILL_SENT );
		KillTimer( TIMER_NEVER );	// Cancel the timer
		return 0;
	}
	else if ( IsRunning() ) {
		dprintf( D_FULLDEBUG,
				 "CronJob: Killing job '%s' with SIGTERM, pid = %d\n",
				 GetName(), m_pid );
		if ( daemonCore->Send_Signal( m_pid, SIGTERM ) == 0 ) {
			dprintf( D_ALWAYS,
					 "CronJob: job '%s': Failed to send SIGTERM to %d\n",
					 GetName(), m_pid );
		}
		SetState( CRON_TERM_SENT );
		KillTimer( 1 );				// Schedule hard kill in 1 sec
		return 1;
	}
	else {
		return -1;					// Nothing else to do!
	}
}

// Cancel the run timer
void
CronJob::CancelRunTimer( void )
{
	if ( m_run_timer >= 0 ) {
		daemonCore->Cancel_Timer( m_run_timer );
	}
	m_run_timer = -1;
}

// Set the job timer
int
CronJob::SetTimer( unsigned first, unsigned period_arg )
{
	ASSERT( IsPeriodic() || IsWaitForExit() );

	// Reset the timer
	if ( m_run_timer >= 0 )
	{
		daemonCore->Reset_Timer( m_run_timer, first, period_arg );
		if( period_arg == TIMER_NEVER ) {
			dprintf( D_FULLDEBUG,
					 "CronJob: timer ID %d reset first=%u, period=NEVER\n",
					 m_run_timer, first );
		} else {
			dprintf( D_FULLDEBUG,
					 "CronJob: timer ID %d reset first=%u, period=%u\n",
					 m_run_timer, first, Period() );
		}
	}

	// Create a periodic timer
	else
	{
		// Debug
		dprintf( D_FULLDEBUG, 
				 "CronJob: Creating timer for job '%s'\n", GetName() );
		TimerHandlercpp handler =
			(  ( IsWaitForExit() ) ? 
			   (TimerHandlercpp)& CronJob::StartJobFromTimer :
			   (TimerHandlercpp)& CronJob::RunJobFromTimer );
		m_run_timer = daemonCore->Register_Timer(
			first,
			period_arg,
			handler,
			"RunJob",
			this );
		if ( m_run_timer < 0 ) {
			dprintf( D_ALWAYS, "CronJob: Failed to create timer\n" );
			return -1;
		}
		if( period_arg == TIMER_NEVER ) {
			dprintf( D_FULLDEBUG, "CronJob: new timer ID %d set first=%u, "
					 "period: NEVER\n", m_run_timer, first );
		} else {
			dprintf( D_FULLDEBUG,
					 "CronJob: new timer ID %d set first=%u, period: %u\n",
					 m_run_timer, first, Period() );
		}
	} 

	return 0;
}

// Start the kill timer
int
CronJob::KillTimer( unsigned seconds )
{
	// Cancel request?
	if ( TIMER_NEVER == seconds ) {
		dprintf( D_FULLDEBUG, "CronJob: Canceling kill timer for '%s'\n",
				 GetName() );
		if ( m_killTimer >= 0 ) {
			return daemonCore->Reset_Timer( 
				m_killTimer, TIMER_NEVER, TIMER_NEVER );
		}
		return 0;
	}

	// Reset the timer
	if ( m_killTimer >= 0 )
	{
		daemonCore->Reset_Timer( m_killTimer, seconds, 0 );
		dprintf( D_FULLDEBUG, "CronJob: Kill timer ID %d reset to %us\n", 
				 m_killTimer, seconds );
	}

	// Create the Kill timer
	else
	{
		// Debug
		dprintf( D_FULLDEBUG, "CronJob: Creating kill timer for '%s'\n", 
				 GetName() );
		m_killTimer = daemonCore->Register_Timer(
			seconds,
			0,
			(TimerHandlercpp)& CronJob::KillHandler,
			"KillJob",
			this );
		if ( m_killTimer < 0 ) {
			dprintf( D_ALWAYS, "CronJob: Failed to create kill timer\n" );
			return -1;
		}
		dprintf( D_FULLDEBUG, "CronJob: new kill timer ID=%d set to %us\n",
				 m_killTimer, seconds );
	}

	return 0;
}

// Convert state value into string (for printing)
const char *
CronJob::StateString( CronJobState state_arg )
{
	switch( state_arg )
	{
	case CRON_IDLE:
		return "Idle";
	case CRON_RUNNING:
		return "Running";
	case CRON_TERM_SENT:
		return "TermSent";
	case CRON_KILL_SENT:
		return "KillSent";
	case CRON_DEAD:
		return "Dead";
	default:
		return "Unknown";
	}
}

// Same, but uses the job's current state
const char *
CronJob::StateString( void )
{
	return StateString( GetState() );
}
