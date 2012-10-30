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

#ifndef _CONDOR_MASTER_H
#define _CONDOR_MASTER_H

#include "condor_daemon_core.h"
#include "condor_lock.h"
#include "dc_collector.h"
#include "condor_pidenvid.h"
#include "env.h"

enum AllGoneT { MASTER_RESTART, MASTER_EXIT, MASTER_RESET };
enum ReaperT { DEFAULT_R, ALL_R, NO_R };
enum StopStateT { PEACEFUL, GRACEFUL, FAST, KILL, NONE };

// Max # of controllee's a controller can support
const int MAX_CONTROLLEES = 5;

class daemon : public Service
{
public:
	daemon(const char *name, bool is_daemon_core = true, bool is_ha = false );
	~daemon();
	daemon_t type;
	char*	name_in_config_file;
	char*	daemon_name; 
	char*	log_filename_in_config_file;
	char*	flag_in_config_file;
	char*	process_name;
	char*	watch_name;
	char*	log_name;
	int		runs_here;
	int		pid;
	int 	restarts;
	int		newExec; 
	time_t	timeStamp;		// Timestamp of this daemon's binary.
	time_t	startTime;		// Time this daemon was started
	bool	isDC;

	Env     env;			// Environment of daemon.

#if 0
	char*	port;				 	// for config server
	char*	config_info_file;				// for config server
#endif

	int		NextStart();
	int		Start( bool never_forward = false );
	int		RealStart();
	int		Restart();
	void    Hold( bool on_hold, bool never_forward = false );
	bool	OnHold( void ) { return on_hold; };
	void	Stop( bool never_forward = false );
	void	StopFast( bool never_forward = false );
	void	StopFastTimer();
	void	StopPeaceful();
	void	HardKill();
	void	Exited( int );
	void	Obituary( int );
	void	CancelAllTimers();
	void	CancelRestartTimers();
	void	Kill( int );
	void	KillFamily( void );
	void	Reconfig();
	void	InitParams();

	int		SetupController( void );
	int		DetachController( void );
	int		RegisterControllee( class daemon * );
	void		DeregisterControllee( class daemon * );

	bool	IsHA( void ) { return is_ha; };

	bool WaitBeforeStartingOtherDaemons(bool first_time);

		// true if this daemon needs to run right up until just before
		// the master shuts down (e.g. shared port server)
	bool OnlyStopWhenMasterStops() { return m_only_stop_when_master_stops; }

private:

	int		runs_on_this_host();
	void	Recover();
	void	DoStart();
	void	DoConfig( bool init );
	int		SetupHighAvailability( void );
	int		HaLockAcquired( LockEventSrc src );
	int		HaLockLost( LockEventSrc src );
	void	DoActionAfterStartup();

	int		start_tid;
	int		recover_tid;
	int		stop_tid;
	int		stop_fast_tid;
	int 	hard_kill_tid;

	int		on_hold;

	int		needs_update;
	StopStateT stop_state;

	int		was_not_responding;

	CondorLock	*ha_lock;
	bool	is_ha;

	int		m_backoff_constant;
	float	m_backoff_factor;
	int		m_backoff_ceiling;
	int		m_recover_time;

	char	*controller_name;
	class daemon  *controller;

	int		num_controllees;
	class daemon  *controllees[MAX_CONTROLLEES];

	MyString m_after_startup_wait_for_file;
	bool m_reload_shared_port_addr_after_startup;
	bool m_never_use_shared_port;
	bool m_waiting_for_startup;
	bool m_only_stop_when_master_stops;
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

	void	CheckForNewExecutable();
	void	DaemonsOn();
	void	DaemonsOff( int fast = 0 );
	void	DaemonsOffPeaceful();
	void 	StartAllDaemons();
	int 	StartDaemonHere(class daemon *);
	void	StopAllDaemons();
	void	StopFastAllDaemons();
	void	StopDaemon( char* name );
	void	HardKillAllDaemons();
	void	StopPeacefulAllDaemons();
	int		SetPeacefulShutdown(int timeout);
	void	ReconfigAllDaemons();

	void	InitMaster();
	void    RestartMaster();
	void    RestartMasterPeaceful();
	void	FinishRestartMaster();
	void	FinalRestartMaster();

	void	CleanupBeforeRestart();
	void	ExecMaster();

	const char*	DaemonLog(int pid);			// full log file path name
#if 0
	void	SignalAll(int signal);		// send signal to all children
#endif
	int		NumberOfChildren();
	int		NumberOfChildrenOfType(daemon_t type);

	int		AllReaper(int, int);
	int		DefaultReaper(int, int);
	void	SetAllReaper(bool fStartdsFirst=false);
	void	SetDefaultReaper();

	void	AllDaemonsGone();
	void	SetAllGoneAction( AllGoneT a ) {all_daemons_gone_action=a;};
	void	AllStartdsGone();
	void	StartTimers();
	void	CancelRestartTimers();
	void	StartNewExecTimer();
	void	CancelNewExecTimer();

	int		SetupControllers( );

	int		immediate_restart;
	int		immediate_restart_master;
	StopStateT	stop_other_daemons_when_startds_gone;

	StringList	ordered_daemon_names;

	void	Update( ClassAd* );
	void	UpdateCollector();

	class daemon*	FindDaemon( daemon_t dt );
	class daemon*	FindDaemon( const char * );

private:
	std::map<std::string, class daemon*> daemon_ptr;
	std::map<int, class daemon*> exit_allowed;
	int check_new_exec_tid;
	int update_tid;
	int preen_tid;
	class daemon* master;  	// the master in our daemon table
	AllGoneT all_daemons_gone_action;
	ReaperT reaper;
	int prevLHF;
	int m_retry_start_all_daemons_tid;

	void ScheduleRetryStartAllDaemons();
	void CancelRetryStartAllDaemons();
	void RetryStartAllDaemons();
	int  SendSetPeacefulShutdown(class daemon*, int timeout);
	void DoPeacefulShutdown(int timeout, void (Daemons::*pfn)(void), const char * lbl);

		// returns true if there are no remaining daemons
	bool StopDaemonsBeforeMasterStops();
};

#endif /* _CONDOR_MASTER_H */
