#ifndef __CONDOR_MASTER
#define __CONDOR_MASTER

// An exponential ceilinged back-off scheme is implemented to control the
// restart elapse time of a daemon.
// Weiru

#include "condor_timer_manager.h"

class daemon : public Service
{
public:
	daemon(char *name);
	char*	name_in_config_file;
	char*	log_filename_in_config_file;
	char*	flag_in_config_file;
	char*	process_name;
	char*	log_name;
	int		flag;  					// whether the process runs here or not
	int		pid;
	int 	restarts;
	time_t	timeStamp;
	time_t	tartRunning;
	int		recoverTimer;			// id of the recover timer

	int		Restart();
	int		StartDaemon();

private:
	int		runs_on_this_host();
	void	Recover();
};


// to add a new process as a condor daemon, just add one line in 
// the structure daemon_ptr defined in daemon.C . The first elemnt 
// is the string that is 
// looked for in the config file for the executable, and the
// second element is the parameter looked for in the confit
// file for the name of the corresponding log file.If no log
// file need be there, then put a zero in the second column.
// The third parameter is the name of a condor_config variable
// that is ckecked before the process is created. If it is zero, 
// then the process is created always. If it is a valid name,
// this name shud be set to true in the condor_config file for the
// process to be created.



class Daemons : public Service
{
private:
	daemon **daemon_ptr;
	int	no_daemons;
	int daemon_list_size;
public:
	Daemons();
	void	RegisterDaemon(daemon *);
	void 	InitParams();
	int	GetIndex(const char* process_name);
	void 	StartAllDaemons();
	int		StartDaemon(char* name);      // retuns pid
	void	Restart(int pid);              // process pid got killed somehow
	void    RestartMaster();      	// PVMD??
	void    Vacate();				// send vacate to startd
	void	SignalAll(int signal);	       // send signal to all :PVMD??
	char*   DaemonName(int pid);	       // full process path name
	char*	DaemonLog(int pid);	       // full log file path name
	char*	SymbolicName(int pid);	       // name as in config file
	int	NumberRestarts(int pid);        // number of restarts
	int	IsDaemon(int pid);	       // returns 1 if pid is a daemon
	void	CheckForNewExecutable();
};


#if 0
#define BADSIG          ((void (*)(int))(-1))
#define SIG_IGN         ((void (*)(int))( 1))
#define SIG_DFL         ((void (*)(int))( 0))
#endif

#define MAXIMUM(a,b) ((a)>(b)?(a):(b))

#endif /* __CONDOR_MASTER */
