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

#ifndef _CONDOR_CRONJOB_H
#define _CONDOR_CRONJOB_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "linebuffer.h"
#include "Queue.h"
#include "env.h"

// Cron's StdOut Line Buffer
class CronJobOut : public LineBuffer
{
  public:
	CronJobOut( class CronJobBase *job );
	virtual ~CronJobOut( void ) {};
	virtual int Output( const char *buf, int len );
	int GetQueueSize( void );
	char *GetLineFromQueue( void );
	int FlushQueue( void );
  private:
	Queue<char *>		lineq;
	class CronJobBase	*job;
};

// Cron's StdErr Line Buffer
class CronJobErr : public LineBuffer
{
  public:
	CronJobErr( class CronJobBase *job );
	virtual ~CronJobErr( void ) {};
	virtual int Output( const char *buf, int len );
  private:
	class CronJobBase	*job;
};

// Job's state
typedef enum {
	CRON_NOINIT,			// Not initialized yet
	CRON_IDLE, 				// Job is idle / not running
	CRON_RUNNING,			// Job is running
	CRON_TERMSENT,			// SIGTERM sent to job, waiting for SIGCHLD
	CRON_KILLSENT,			// SIGKILL sent to job
	CRON_DEAD				// Job is dead
} CronJobState;

// Job's "run" (when to restart) mode
typedef enum 
{ 
	CRON_WAIT_FOR_EXIT,		// Timing from job's exit
	CRON_PERIODIC, 			// Run it periodically
	CRON_KILL,				// Job has been killed & don't restart it
	CRON_ILLEGAL			// Illegal mode
} CronJobMode;

// Notification events..
typedef enum {
	CONDOR_CRON_JOB_START,
	CONDOR_CRON_JOB_DIED,
} CondorCronEvent;

// Cronjob event
class CronJobBase;
typedef int     (Service::*CronEventHandler)(CronJobBase*,CondorCronEvent);

// Enable pipe I/O debugging
#define CRONJOB_PIPEIO_DEBUG	0

// Define a Condor 'Cron' job
class CronJobBase : public Service
{
  public:
	CronJobBase( const char *mgrName, const char *jobName );
	virtual ~CronJobBase( );

	// Finish initialization
	virtual int Initialize( void );

	// Manipulate the job
	const char *GetName( void ) { return name.Value(); };
	const char *GetPrefix( void ) { return prefix.Value(); };
	const char *GetPath( void ) { return path.Value(); };
	//const char *GetArgs( void ) { return args.Value(); };
	const char *GetCwd( void ) { return cwd.Value(); };
	unsigned GetPeriod( void ) { return period; };

	// State information
	CronJobState GetState( void ) { return state; };
	bool IsRunning( void ) 
		{ return ( CRON_RUNNING == state ? true : false ); };
	bool IsIdle( void )
		{ return ( CRON_IDLE == state ? true : false ); };
	bool IsDead( void ) 
		{ return ( CRON_DEAD == state ? true : false ); };
	bool IsAlive( void ) 
		{ return ( (CRON_IDLE == state)||(CRON_DEAD == state)
				   ? false : true ); };

	int Reconfig( void );

	int SetConfigVal( const char *path );
	int SetName( const char *name );
	int SetPrefix( const char *prefix );
	int SetPath( const char *path );	
	int SetArgs( ArgList const &new_args );
	int SetEnv( char const * const *env );
	int AddEnv( char const * const *env );
	int SetCwd( const char *cwd );
	int SetPeriod( CronJobMode mode, unsigned period );
	int SetKill( bool );
	int SetReconfig( bool );
	int AddPath( const char *path );	
	int AddArgs( ArgList const &new_args );

	virtual int KillJob( bool );

	void Mark( void ) { marked = true; };
	void ClearMark( void ) { marked = false; };
	bool IsMarked( void ) { return marked; };

	int ProcessOutputQueue( void );
	virtual int ProcessOutput( const char *line ) = 0;

	int SetEventHandler( CronEventHandler handler, Service *s );

  protected:
	MyString		configValProg;	// Path to _config_val

  private:
	MyString		name;			// Logical name of the job
	MyString		prefix;			// Publishing prefix
	MyString		path;			// Path to the executable
	ArgList         args;           // Arguments to pass it
	Env             env;			// Environment variables
	MyString		cwd;			// Process's initial CWD
	unsigned		period;			// The configured period
	int				runTimer;		// It's DaemonCore "run" timerID
	CronJobMode		mode;			// Is is a periodic
	CronJobState	state;			// Is is currently running?
	int				pid;			// The process's PID
	int				stdOut;			// Process's stdout file descriptor
	int				stdErr;			// Process's stderr file descriptor
	int				childFds[3];	// Child process FDs
	int				reaperId;		// ID Of the child reaper
	CronJobOut		*stdOutBuf;		// Buffer for stdout
	CronJobErr		*stdErrBuf;		// Buffer for stderr
	bool			marked;			// Is this one marked?
	int				killTimer;		// Make sure it dies
	int				numOutputs;		// How many output blocks have we processed?

	// Event handler stuff
	CronEventHandler	eventHandler;	// Handle cron events
	Service				*eventService;	// Associated service

	// Options
	bool			optKill;		// Kill the job if before next run?
	bool			optReconfig;	// Send the job a HUP for reconfig

	// Private methods; these can be replaced
	virtual int Schedule( void );
	virtual int RunJob( void );
	virtual int StartJob( void );
	virtual void KillHandler( void );
	virtual int StdoutHandler( int pipe );
	virtual int StderrHandler( int pipe );
	virtual int Reaper( int exitPid, int exitStatus );
	virtual int RunProcess( void );

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

	// Debugging
# if CRONJOB_PIPEIO_DEBUG
	char	*TodoBuffer;
	int		TodoBufSize;
	int		TodoBufWrap;
	int		TodoBufOffset;
	int		TodoWriteNum;
	public:
	void	TodoWrite( void );
# endif
};

#endif /* _CONDOR_CRONJOB_H */
