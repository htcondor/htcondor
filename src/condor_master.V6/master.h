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
#ifndef _CONDOR_MASTER_H
#define _CONDOR_MASTER_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "killfamily.h"

enum AllGoneT { MASTER_RESTART, MASTER_EXIT, MASTER_RESET };
enum ReaperT { DEFAULT_R, ALL_R, NO_R };
enum StopStateT { GRACEFUL, FAST, KILL, NONE };

class daemon : public Service
{
public:
	daemon(char *name, bool is_daemon_core = true);
	daemon_t type;
	char*	name_in_config_file;
	char*	daemon_name; 
	char*	log_filename_in_config_file;
	char*	flag_in_config_file;
	char*	process_name;
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
	int		Restart();
	void	Stop();
	void	StopFast();
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
	void	RegisterDaemon(daemon *);
	void 	InitParams();
	int		GetIndex(const char* process_name);

	void	CheckForNewExecutable();
	void	DaemonsOn();
	void	DaemonsOff( int fast = 0 );
	void 	StartAllDaemons();
	void	StopAllDaemons();
	void	StopFastAllDaemons();
	void	ReconfigAllDaemons();

	void	InitMaster();
	void    RestartMaster();
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

	daemon*	FindDaemon( daemon_t dt );

private:
	daemon **daemon_ptr;
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
