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
#ifndef _CONDOR_CRON_JOB_H
#define _CONDOR_CRON_JOB_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "env.h"

#include "condor_cron_job_params.h"
#include "condor_cron_job_io.h"

// Job's state
typedef enum
{
	CRON_INITIALIZING,		// Not initialized yet
	CRON_IDLE, 				// Job is idle / not running
	CRON_RUNNING,			// Job is running
	CRON_READY,				// Ready to run
	CRON_TERM_SENT,			// SIGTERM sent to job, waiting for SIGCHLD
	CRON_KILL_SENT,			// SIGKILL sent to job
	CRON_DEAD				// Job is dead
} CronJobState;

// Define a Condor 'Cron' job
class CronJobMgr;
class CronJob : public Service
{
  public:
	CronJob( CronJobParams *params, CronJobMgr &mgr );
	virtual ~CronJob( void );

	virtual int KillJob( bool );
	int Schedule( void );
	int StartOnDemand( void );
	virtual int Initialize( void );
	int HandleReconfig( void );

	bool SetParams( CronJobParams *params );

	int ProcessOutputQueue( bool dump = false, int exitPid = -1 );
	virtual int ProcessOutput( const char * /*line*/ ) { return 0; };
	virtual int ProcessOutputSep( const char * /*args*/ ) { return 0; };

	// State information
	CronJobState GetState( void ) const { return m_state; };
	bool IsInitialized( void ) const {
		return ( CRON_INITIALIZING != m_state );
	};
	bool IsRunning( void ) const {
		return ( (CRON_RUNNING == m_state) && (m_pid > 0) );
	};
	int  GetPid( void ) const { return m_pid; }
	bool IsIdle( void ) const {
		return ( CRON_IDLE == m_state );
	};
	bool IsReady( void ) const {
		return ( CRON_READY == m_state );
	};
	bool IsDead( void ) const {
		return ( CRON_DEAD == m_state );
	};
	bool IsTermSent( void ) const {
		return ( CRON_TERM_SENT == m_state );
	};
	bool IsKillSent( void ) const {
		return ( CRON_KILL_SENT == m_state );
	};
	bool IsSigSent( void ) const {
		return ( IsTermSent() || IsKillSent() );
	};
	bool IsAlive( void ) const {
		return ( IsRunning() || IsTermSent() || IsKillSent() );
	};
	bool IsActive( void ) const {
		return ( IsRunning() || IsReady() );
	};
	bool IsInShutdown( void ) const {
		return m_in_shutdown;
	};

	// Params accessors
	virtual const CronJobParams &Params( void ) const { return *m_params; };
	const char *GetName( void ) const { return m_params->GetName(); };
	const char *GetPrefix( void ) const { return m_params->GetPrefix(); };
	const char *GetExecutable( void ) const {
		return m_params->GetExecutable();
	};
	const char *GetCwd( void ) const { return m_params->GetCwd(); };
	unsigned GetPeriod( void ) const { return m_params->GetPeriod(); };
	double GetJobLoad( void ) const { return m_params->GetJobLoad(); };
	double GetRunLoad( void ) const { return m_run_load; };

	bool IsPeriodic( void ) const { return Params().IsPeriodic(); };
	bool IsWaitForExit( void ) const { return Params().IsWaitForExit(); };
	bool IsOneShot( void ) const { return Params().IsOneShot(); };
	bool IsOnDemand( void ) const { return Params().IsOnDemand(); };
	CronJobMode GetJobMode( void ) const { return m_params->GetJobMode(); };

	// Marking operations
	void Mark( void ) { m_marked = true; };
	void ClearMark( void ) { m_marked = false; };
	bool IsMarked( void ) const { return m_marked; };

  private:
	void SetState( CronJobState state ) {
		m_state = state;
	};
	int SendHup( void ) const;
	void CancelRunTimer( void );
	unsigned Period( void ) const { return m_params->GetPeriod(); };

	// Private methods; these can be overloaded
	virtual int RunJob( void );
	virtual void RunJobFromTimer( int /* timerID */ ) { RunJob(); }
	virtual int StartJob( void );
	virtual void StartJobFromTimer( int /* timerID */ ) { StartJob(); }
	virtual void KillHandler( int timerID = -1 );
	virtual int StdoutHandler( int pipe );
	virtual int StderrHandler( int pipe );
	virtual int Reaper( int exitPid, int exitStatus );
	virtual int StartJobProcess( void );

	// No reason to replace these
	int OpenFds( void );
	int TodoRead( int, int );
	void CleanAll( void );
	void CleanFile( FILE **file );
	void CleanFd( int *fd );
	const char *StateString( void );
	const char *StateString( CronJobState state );

	// Timer maintainence
	int SetTimer( unsigned first, unsigned seconds );
	int KillTimer( unsigned seconds );

  protected:
	virtual const CronJobMgr & Mgr( void ) {
		return m_params->GetMgr();
	};

	// Protected data
  protected:
	CronJobParams	*m_params;			// Job parameters
	CronJobMgr		&m_mgr;				// Job manager

	// Private data
	CronJobState	 m_state;			// Is is currently running?
	bool			 m_in_shutdown;		// Shutting down
	int				 m_run_timer;		// It's DaemonCore "run" timerID
	int				 m_pid;				// The process's PID
	int				 m_stdOut;			// Process's stdout file descriptor
	int				 m_stdErr;			// Process's stderr file descriptor
	int				 m_childFds[3];		// Child process FDs
	int				 m_reaperId;		// ID Of the child reaper
	CronJobOut		*m_stdOutBuf;		// Buffer for stdout
	CronJobErr		*m_stdErrBuf;		// Buffer for stderr
	int				 m_killTimer;		// Make sure it dies
	int				 m_num_outputs;		// # output blocks have we processed?
	int				 m_num_runs;		// # of times the job has run
	int				 m_num_fails;		// # of times we failed to exec it
	time_t			 m_last_start_time;	// Last run time
	time_t			 m_last_exit_time;	// Last exit time
	double			 m_run_load;		// Run load of the job
	bool			 m_marked;			// Is this one marked?
	unsigned		 m_old_period;		// Period before reconfig
};

#endif /* _CONDOR_CRON_JOB_H */
