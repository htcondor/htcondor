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
#ifndef _CONDOR_CRONJOB_H
#define _CONDOR_CRONJOB_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "linebuffer.h"

// Cron's StdOut Line Buffer
class CronJobOut : public LineBuffer
{
  public:
	CronJobOut( const char *prefix = NULL );
	virtual ~CronJobOut( void ) {};
	int SetPrefix( const char *prefix );
	virtual int Output( char *buf, int len );
	MyString *GetString( void );
  private:
	const char	*prefix;
	MyString	stringBuf;
};

// Cron's StdErr Line Buffer
class CronJobErr : public LineBuffer
{
  public:
	CronJobErr( const char *prefix = NULL );
	virtual ~CronJobErr( void ) {};
	int SetPrefix( const char *prefix );
	virtual int Output( char *buf, int len );
  private:
	const char	*prefix;
};

// Job's state
typedef enum {
	CRON_IDLE, 
	CRON_RUNNING,
	CRON_TERMSENT,
	CRON_KILLSENT
} CronJobState;

// Job's "run" mode
typedef enum 
{ 
	CRON_CONTINUOUS,		// Keep it running forever (possible delay)
	CRON_PERIODIC, 			// Run it periodically
	CRON_ILLEGAL
} CronJobMode;

// Define a Condor 'Cron' job
class CondorCronJob : public Service
{
  public:
	CondorCronJob( const char *name );
	virtual ~CondorCronJob( );

	// Manipulate the job
	const char *GetName( void ) { return name; };
	const char *GetPrefix( void ) { return prefix; };
	const char *GetPath( void ) { return path; };
	const char *GetArgs( void ) { return args; };
	const char *GetEnv( void ) { return env; };
	const char *GetCwd( void ) { return cwd; };
	unsigned GetPeriod( void ) { return period; };
	int Reconfig( void );

	int SetName( const char *name );
	int SetPrefix( const char *prefix );
	int SetPath( const char *path );	
	int SetArgs( const char *args );
	int SetEnv( const char *env );
	int SetCwd( const char *cwd );
	int SetPeriod( CronJobMode mode, unsigned period );
	int SetKill( bool );

	void Mark( void ) { marked = true; };
	void ClearMark( void ) { marked = false; };
	bool IsMarked( void ) { return marked; };

	virtual int ProcessOutput( MyString *string );

  private:
	char			*name;			// Logical name of the job
	char			*prefix;		// Publishing prefix
	char			*path;			// Path to the executable
	char			*args;			// Arguments to pass it
	char			*env;			// Environment variables
	char			*cwd;			// Process's initial CWD
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

	// Options
	bool			optKill;		// Kill the job if before next run?

	// Private methods; these can be replaced
	virtual int RunJob( void );
	virtual int StartJob( void );
	virtual int KillHandler( void );
	virtual int StdoutHandler( int pipe );
	virtual int StderrHandler( int pipe );
	virtual int KillJob( bool );
	virtual int Reaper( int exitPid, int exitStatus );
	virtual int RunProcess( void );

	// No reason to replace these
	int SetVar( char **var, const char *value, bool allowNull = false );
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

};

#endif /* _CONDOR_CRONJOB_H */
