/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef _CONDOR_MASTER_H
#define _CONDOR_MASTER_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "../condor_daemon_core.V6/condor_lock.h"
#include "dc_collector.h"
#include "killfamily.h"

enum AllGoneT { MASTER_RESTART, MASTER_EXIT, MASTER_RESET };
enum ReaperT { DEFAULT_R, ALL_R, NO_R };
enum StopStateT { PEACEFUL, GRACEFUL, FAST, KILL, NONE };

class daemon : public Service
{
public:
	daemon(char *name, bool is_daemon_core = true, bool is_ha = false );
	daemon_t type;
	char*	name_in_config_file;
	char*	daemon_name; 
	char*	log_filename_in_config_file;
	char*	flag_in_config_file;
	char*	process_name;
	char*	watch_name;
	char*	log_name;
	int		runs_here;
	int		on_hold;
	int		pid;
	int 	restarts;
	int		newExec; 
	time_t	timeStamp;		// Timestamp of this daemon's binary.
	time_t	startTime;		// Time this daemon was started
	bool	isDC;

	char*   env;			// Environment of daemon, e.g.,
							// "FOO=bar;CONDOR_CONFIG=/some/path/config"
							// null if there is none

#if 0
	char*	port;				 	// for config server
	char*	config_info_file;				// for config server
#endif

	int		NextStart();
	int		Start();
	int		RealStart();
	int		Restart();
	void	Stop();
	void	StopFast();
	void	StopPeaceful();
	void	HardKill();
	void	Exited( int );
	void	Obituary( int );
	void	CancelAllTimers();
	void	CancelRestartTimers();
	void	Kill( int );
	void	KillFamily( void );
	void	Reconfig();
	void	InitProcFam( int pid );
	void	DeleteProcFam( void );

private:

	int		runs_on_this_host();
	void	Recover();
	void	DoStart();
	void	DoConfig( bool init );
	int		SetupHighAvailability( void );
	int		HaLockAcquired( LockEventSrc src );
	int		HaLockLost( LockEventSrc src );

	int		start_tid;
	int		recover_tid;
	int		stop_tid;
	int		stop_fast_tid;
	int 	hard_kill_tid;

	int		needs_update;
	StopStateT stop_state;

	int		was_not_responding;

	ProcFamily*	procfam;
	int		procfam_tid;

	CondorLock	*ha_lock;
	bool	is_ha;
};


// to add a new process as a condor daemon, just add one line in the
// structure daemon_ptr defined in daemon.C. The first element is the
// string that is looked for in the config file for the executable,
// and the second element is the parameter looked for in the config
// file for the name of the corresponding log file.  If no log file
// need be there, then put a zero in the second column.  The third
// parameter is the name of a condor_config variable that is checked
// before the process is created. If it is zero, then the process is
// created always. If it is a valid name, this name should be set to
// true in the condor_config file for the process to be created.

class Daemons : public Service
{
public:
	Daemons();
	void	RegisterDaemon(class daemon *);
	void 	InitParams();
	int		GetIndex(const char* process_name);

	void	CheckForNewExecutable();
	void	DaemonsOn();
	void	DaemonsOff( int fast = 0 );
	void	DaemonsOffPeaceful();
	void 	StartAllDaemons();
	void	StopAllDaemons();
	void	StopFastAllDaemons();
	void	StopPeacefulAllDaemons();
	void	ReconfigAllDaemons();

	void	InitMaster();
	void    RestartMaster();
	void    RestartMasterPeaceful();
	void	FinishRestartMaster();
	void	FinalRestartMaster();

	char*	DaemonLog(int pid);			// full log file path name
#if 0
	void	SignalAll(int signal);		// send signal to all children
#endif
	int		NumberOfChildren();

	int		AllReaper(int, int);
	int		DefaultReaper(int, int);
	void	SetAllReaper();
	void	SetDefaultReaper();

	void	AllDaemonsGone();
	void	SetAllGoneAction( AllGoneT a ) {all_daemons_gone_action=a;};
	void	StartTimers();
	void	CancelRestartTimers();
	void	StartNewExecTimer();
	void	CancelNewExecTimer();

	int		immediate_restart;
	int		immediate_restart_master;

	void	Update( ClassAd* );
	void	UpdateCollector();

	class daemon*	FindDaemon( daemon_t dt );

private:
	class daemon **daemon_ptr;
	int	no_daemons;
	int daemon_list_size;
	int check_new_exec_tid;
	int update_tid;
	int preen_tid;
	int master;  		// index of the master in our daemon table
	AllGoneT all_daemons_gone_action;
	ReaperT reaper;

};

#endif /* _CONDOR_MASTER_H */
