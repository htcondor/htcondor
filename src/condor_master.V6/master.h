#ifndef __CONDOR_MASTER
#define __CONDOR_MASTER

// An exponential ceilinged back-off scheme is implemented to control the
// restart elapse time of a daemon.
// Weiru

#include "../condor_daemon_core.V6/condor_daemon_core.h"

enum AllGoneT { MASTER_RESTART, MASTER_EXIT, MASTER_RESET };
enum ReaperT { DEFAULT_R, ALL_R, NO_R };
enum StopStateT { GRACEFUL, FAST, KILL, NONE };

class daemon : public Service
{
public:
	daemon(char *name);
	char*	name_in_config_file;
	char*	daemon_name; 
	char*	log_filename_in_config_file;
	char*	flag_in_config_file;
	char*	process_name;
	char*	log_name;
	int		runs_here;
	int		pid;
	int 	restarts;
	int		newExec; 
	time_t	timeStamp;

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
	void	Kill( int );
	void	Killpg( int );
	void	Reconfig();

private:

	int		runs_on_this_host();
	void	Recover();
	void	DoStart();

	int		start_tid;
	int		recover_tid;
	int		stop_tid;
	int		stop_fast_tid;
	int 	hard_kill_tid;

	StopStateT stop_state;

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
	void	DaemonsOff();
	void 	StartAllDaemons();
	void	StopAllDaemons();
	void	StopFastAllDaemons();
	void	CancelAllStopTimers();
	void	ReconfigAllDaemons();

	void	InitMaster();
	void    RestartMaster();
	void	FinishRestartMaster();

	char*	DaemonLog(int pid);			// full log file path name
#if 0
	void	SignalAll(int signal);		// send signal to all children
#endif
	int		NumberOfChildren();

	int		AllReaper(Service*, int, int);
	int		DefaultReaper(Service*, int, int);
	void	SetAllReaper();
	void	SetDefaultReaper();

	void	AllDaemonsGone();
	void	SetAllGoneAction( AllGoneT a ) {all_daemons_gone_action=a;};
	void	StartTimers();

private:
	daemon **daemon_ptr;
	int	no_daemons;
	int daemon_list_size;
	int check_new_exec_tid;
	int update_tid;
	AllGoneT all_daemons_gone_action;
	ReaperT reaper;
};

#endif /* __CONDOR_MASTER */
