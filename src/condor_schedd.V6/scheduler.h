////////////////////////////////////////////////////////////////////////////////
//
// condor_sched.h
//
// Define class Scheduler. This class does local scheduling and then negotiates
// with the central manager to obtain resources for jobs.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _CONDOR_SCHED_H_
#define _CONDOR_SCHED_H_

#include "_condor_fix_types.h"
#include "dgram_io_handle.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "proc.h"
#include "condor_daemon_core.h"
#include "sched.h"
#include "prio_rec.h"

const	int			MAX_SHADOW_RECS	= 1024; 
const 	int			MAX_NUM_OWNERS = 512;
const 	int			MAX_REJECTED_CLUSTERS = 1024;
const 	int			MAX_SCHED_UNIVERSE_RECS = 32;


extern	char**		environ;

struct shadow_rec
{
    int             pid;
    PROC_ID         job_id;
    struct Mrec*    match;
    int             preempted;
    int             conn_fd;
};

struct OwnerData {
  char* Name;
  int JobsRunning;
  int JobsIdle;
  OwnerData() { Name=NULL; JobsRunning=JobsIdle=0; }
};

struct Mrec
{
    Mrec(char*, char*, PROC_ID*);
   
    char    		id[SIZE_OF_CAPABILITY_STRING];
    char    		peer[50];
    int     		cluster;
    int     		proc;
    int     		status;
    int     		agentPid;
	shadow_rec*		shadowRec;
	int				alive_countdown;
};

enum
{
    M_ACTIVE,
    M_INACTIVE,
    M_DELETED
};

#define MAXMATCHES      32

class Scheduler : public Service
{
  public:
	
	Scheduler();
	~Scheduler();
	
	// initialization
	void			Init();
	void			Register(DaemonCore*);

	// accessing
   	int				Port() { return port; }
 
	// maintainence
	void			timeout(); 
	void			sighup_handler();
	void			sigterm_handler();
	void			sigquit_handler();
	void			sigint_handler();
	void			SetClassAd(ClassAd*);
	void			SetSockName(int);
	
	// negotiation
	void			negotiate(ReliSock*, struct sockaddr_in*);
	void			reschedule_negotiator(ReliSock*, struct sockaddr_in*);
	void			vacate_service(ReliSock *, struct sockaddr_in*);

	// job managing
	void			abort_job(ReliSock*, struct sockaddr_in*);
	void			send_all_jobs(ReliSock*, struct sockaddr_in*);
	void			send_all_jobs_prioritized(ReliSock*, struct sockaddr_in*);
	friend	int		count(int, int);
	friend	void	job_prio(int, int);
	void			display_shadow_recs();

	// match managing
    Mrec*       	AddMrec(char*, char*, PROC_ID*);
    int         	DelMrec(char*);
    int         	DelMrec(Mrec*);
    int         	MarkDel(char*);
    Mrec*       	FindMrecByPid(int);
	shadow_rec*		FindSrecByPid(int);
	void			RemoveShadowRecFromMrec(shadow_rec*);
	int				AlreadyMatched(PROC_ID*);
	void			Agent(char*, char*, char*, int, PROC_ID*);
	void			StartJobs();
	void			StartSchedUniverseJobs();
	void			send_alive();
	
  private:
	
	// information about this scheduler
	ClassAd*		ad;
	char*			MySockName;		// dhaval
	int				UdpSock;					// for talking to collector
	Scheduler*		myself;
	u_short			port;
	
	// parameters controling the scheduling and starting shadow
	int				SchedDInterval;
	int				MaxJobStarts;
	int				MaxJobsRunning;
	int				JobsStarted; // # of jobs started last negotiating session
	int				SwapSpace;	 // available at beginning of last session
	int				ShadowSizeEstimate;	// Estimate of swap needed to run a job
	int				SwapSpaceExhausted;	// True if a job had died due to lack of swap space
	int				ReservedSwap;		// for non-condor users
	int				JobsIdle; 
	int				JobsRunning;
	int				SchedUniverseJobsIdle;
	int				SchedUniverseJobsRunning;
	int				BadCluster;
	int				BadProc;
	int				RejectedClusters[MAX_REJECTED_CLUSTERS];
	int				N_RejectedClusters;
        OwnerData			Owners[MAX_NUM_OWNERS];
	int				N_Owners;
	time_t			LastTimeout;
	int				ExitWhenDone;  // Flag set for graceful shutdown
	
	// parameters controling updown algorithm
	int				ScheddScalingFactor;
	int				ScheddHeavyUserTime;
	int				Step;
	int				ScheddHeavyUserPriority; 

	// useful names
	char*			CollectorHost;
	char*			NegotiatorHost;
	char*			Shadow;
	char*			CondorAdministrator;
	char*			Mail;
	char*			filename;					// save UpDown object
	char*			AccountantName;
        char*                   UidDomain;

	// connection variables
	struct sockaddr_in	From;
	int					Len; 

	// utility functions
	int				shadow_prio_recs_consistent();
	void			mail_problem_message();
	int				cluster_rejected(int);
	void   			mark_cluster_rejected(int); 
	int				count_jobs();
	void			update_central_mgr();
	int			insert_owner(char*);
	void			reaper(int, int, struct sigcontext*);
	void			clean_shadow_recs();
	void			preempt(int);
	int				permission(char*, char*, PROC_ID*);
	shadow_rec*		StartJob(Mrec*, PROC_ID*);
	shadow_rec*		start_std(Mrec*, PROC_ID*);
	shadow_rec*		start_pvm(Mrec*, PROC_ID*);
	shadow_rec*		start_sched_universe_job(PROC_ID*);
	void			Relinquish(Mrec*);
	void 			swap_space_exhausted();
	void			delete_shadow_rec(int);
	void			mark_job_running(PROC_ID*);
	void			mark_job_stopped(PROC_ID*);
	int				is_alive(int);
	void			check_zombie(int, PROC_ID*);
	void			kill_zombie(int, PROC_ID*);
	void			cleanup_ckpt_files(int, PROC_ID*);

    Mrec**      	rec;
    int         	nMrec;                     // # of match records
    int         	aliveFrequency;            // how often to broadcast alive
    int         	aliveInterval;             // how often to broadcast alive
};
	
#endif
