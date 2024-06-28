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
#include "env.h"

enum AllGoneT { MASTER_RESTART, MASTER_EXIT, MASTER_RESET };
enum ReaperT { DEFAULT_R, ALL_R, NO_R };
enum StopStateT { PEACEFUL, GRACEFUL, FAST, KILL, NONE };

const char * StopStateToString(StopStateT state);
StopStateT StringToStopState(const char * psz);
bool advertise_shutdown_program(ClassAd & ad);

constexpr const char	*default_dc_daemon_array[] = {
"MASTER", "STARTD", "SCHEDD", "KBDD", "COLLECTOR", "NEGOTIATOR", "EVENTD",
"VIEW_SERVER", "CONDOR_VIEW", "VIEW_COLLECTOR", "CREDD", "HAD",
"REPLICATION", "JOB_ROUTER", "ROOSTER", "SHARED_PORT",
"DEFRAG", "GANGLIAD", "ANNEXD", "PLACEMENTD"
};

// used to keep track of a query command for which we want to deferr the reply
class DeferredQuery
{
public:
	Stream * stream;
	ExprTree * requirements;
	time_t   expire_time;
	DeferredQuery(int cmd, Stream * stm, ExprTree * req, time_t expires);
	~DeferredQuery();
};

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
	char*	ready_state;
	int		runs_here;		// This only indicates whether it's the master itself
	int		pid;
	int 	restarts;
	int		newExec; 
	time_t	timeStamp;		// Timestamp of this daemon's binary.
	time_t	startTime;		// Time this daemon was started
	bool	isDC;
	bool	use_collector_port; // true for daemon's that are SHARED_PORT in front of a COLLECTOR

	Env     env;			// Environment of daemon.

	time_t		GetNextRestart() const;
	int		NextStart() const;
	int		Start( bool never_forward = false );
	int		RealStart();
	int		Restart(bool by_command=false);
	int		Drain(std::string & request_id, int how_fast, int on_completion, const char * reason, ExprTree* check, ExprTree * Start);
	void    Hold( bool on_hold, bool never_forward = false );
	int		OnHold( void ) const { return on_hold; };
	bool	WaitingforStartup(bool & for_file) { for_file = ! m_after_startup_wait_for_file.empty(); return m_waiting_for_startup; }
	void	Stop( bool never_forward = false );
	void	StopFast( bool never_forward = false );
	void	StopFastTimer( int timerID = -1 );
	void	StopPeaceful();
	void	HardKill( int timerID = -1 );
	bool	Exited(int status);
	void	Obituary( int );
	void	CancelAllTimers();
	void	CancelRestartTimers();
	void	Kill( int ) const;
	void	KillFamily( void ) const;
	void	Reconfig();
	void	InitParams();
	void	SetReadyState(const char * state);
	const char * Stopping(); // return non-null if the daemon is in the process of stopping

	int		SetupController( void );
	int		DetachController( void );
	int		RegisterControllee( class daemon * );
	void		DeregisterControllee( class daemon * );

	bool	IsHA( void ) const { return is_ha; };

	bool WaitBeforeStartingOtherDaemons(bool first_time);

		// true if this daemon needs to run right up until just before
		// the master shuts down (e.g. shared port server)
	bool OnlyStopWhenMasterStops() const { return m_only_stop_when_master_stops; }

private:

	void	Recover( int timerID = -1 );
	void	DoStart( int timerID = -1 );
	void	DoConfig( bool init );
	int		SetupHighAvailability( void );
	int		HaLockAcquired( LockEventSrc src );
	int		HaLockLost( LockEventSrc src );
	void	DoActionAfterStartup() const;

	int		start_tid;
	int		recover_tid;
	int		stop_tid;
	int		stop_fast_tid;
	int 	hard_kill_tid;

	int		on_hold;

	int		needs_update;
	StopStateT stop_state;
	int		draining;

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

	std::string m_after_startup_wait_for_file;
	bool m_reload_shared_port_addr_after_startup;
	bool m_never_use_shared_port;
	bool m_waiting_for_startup;
	bool m_only_stop_when_master_stops;

	std::string localName;
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
	void 	UnRegisterAllDaemons() { for(auto &kvpair: daemon_ptr) { delete kvpair.second;} ; daemon_ptr.clear();}
	void 	InitParams();

	void	CheckForNewExecutable( int timerID = -1 );
	void	DaemonsOn();
	void	DaemonsOff( int fast = 0 );
	void	DaemonsOffPeaceful();
	void 	StartAllDaemons();
	int 	StartDaemonHere(class daemon *);
	void	StopAllDaemons();
	void	StopFastAllDaemons();
	void	RemoveDaemon(const char* name );
	void	HardKillAllDaemons();
	void	StopPeacefulAllDaemons();
	int		SetPeacefulShutdown(int timeout);
	void	ReconfigAllDaemons();

	void	InitMaster();
	void    RestartMaster();
	void    RestartMasterPeaceful( int timerID = -1 );
	void	FinishRestartMaster();
	void	FinalRestartMaster( int timerID = -1 );

	void	CleanupBeforeRestart();
	void	ExecMaster();

	const char*	DaemonLog(int pid);			// full log file path name
	int		ChildrenOfType(daemon_t type, std::vector<std::string> *names=nullptr);

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
	int		QueryReady(ClassAd & cmdAd, Stream* stm);

	int		immediate_restart = FALSE;
	int		immediate_restart_master = FALSE;
	StopStateT	stop_other_daemons_when_startds_gone = NONE;
	ClassAd * cmd_after_drain = nullptr; // when draining startds in order to shutdown/restart/etc this will be non-null

	std::vector<std::string>	ordered_daemon_names;

	void	Update( ClassAd* );
	void	UpdateCollector( int timerID = -1 );

	class daemon*	FindDaemon( daemon_t dt );
	class daemon*	FindDaemon( const char * );
	class daemon*	FindDaemonByPID( int pid );

	friend int do_flexible_daemons_command(int cmd, ClassAd & cmdAd, ClassAd * replyAd);

private:
	std::map<std::string, class daemon*> daemon_ptr;
	std::map<int, class daemon*> removed_daemons; // never or no longer in the daemon list, but we still want to reap them
	int check_new_exec_tid = -1;
	int update_tid = -1;
	int preen_tid = -1;
	class daemon* master = nullptr;  	// the master in our daemon table
	AllGoneT all_daemons_gone_action = MASTER_RESET;
	ReaperT reaper = NO_R;
	int prevLHF = 0;
	int m_retry_start_all_daemons_tid = -1;
	int m_deferred_query_ready_tid = -1;
	std::list<DeferredQuery*> deferred_queries;
	DCTokenRequester m_token_requester;

	void ScheduleRetryStartAllDaemons();
	void CancelRetryStartAllDaemons();
	void RetryStartAllDaemons( int timerID = -1 );
	void DeferredQueryReadyReply( int timerID = -1 );
	bool InitDaemonReadyAd(ClassAd & readyAd, bool include_addrs);
	//bool GetDaemonReadyStates(std::string & ready);
	int  SendSetPeacefulShutdown(class daemon*, int timeout);
	void DoPeacefulShutdown(int timeout, void (Daemons::*pfn)(int), const char * lbl);
	int  ReapPreen(int pid, int status);

		// returns true if there are no remaining daemons
	bool StopDaemonsBeforeMasterStops();

	static void ProcdStopped(void*, int pid, int status);

	static void token_request_callback(bool success, void *miscdata);
};

int do_basic_admin_command(int cmd);
int do_flexible_daemons_command(int cmd, ClassAd & cmdAd, ClassAd * replyAd=nullptr);


#endif /* _CONDOR_MASTER_H */
