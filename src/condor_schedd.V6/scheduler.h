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
// ///////////////////////////////////////////////////////////////////////
//
// condor_sched.h
//
// Define class Scheduler. This class does local scheduling and then 
// negotiates with the central manager to obtain resources for jobs.
//
// ///////////////////////////////////////////////////////////////////////

#ifndef _CONDOR_SCHED_H_
#define _CONDOR_SCHED_H_

#include "condor_classad.h"
#include "condor_io.h"
#include "proc.h"
#include "sched.h"
#include "prio_rec.h"
#include "HashTable.h"
#include "string_list.h"
#include "list.h"
#include "classad_hashtable.h"	// for HashKey class
#include "Queue.h"

const 	int			MAX_NUM_OWNERS = 512;
const 	int			MAX_REJECTED_CLUSTERS = 1024;


extern	char**		environ;

struct shadow_rec
{
    int             pid;
    PROC_ID         job_id;
    struct match_rec*    match;
    int             preempted;
    int             conn_fd;
	int				removed;
};

struct OwnerData {
  char* Name;
  int JobsRunning;
  int JobsIdle;
  OwnerData() { Name=NULL; JobsRunning=JobsIdle=0; }
};

enum MrecStatus {
    M_ACTIVE,
    M_INACTIVE,
    M_DELETED,
	M_STARTD_CONTACT_LIMBO,  // after contacting startd; before recv'ing reply
	M_DELETE_PENDING         // is set if we should delete, but in above state
};

struct match_rec
{
    match_rec(char*, char*, PROC_ID*);
    char    		 id[SIZE_OF_CAPABILITY_STRING];
    char    		 peer[50];
    int     		 cluster;
    int     		 proc;
    enum MrecStatus  status;
	shadow_rec*		 shadowRec;
	int				 alive_countdown;
	int				 num_exceptions;
};

class Scheduler : public Service
{
  public:
	
	Scheduler();
	~Scheduler();
	
	// initialization
	void			Init();
	void			Register();
	void			RegisterTimers();

	// maintainence
	void			timeout(); 
	void			reconfig();
	void			shutdown_fast();
	void			shutdown_graceful();
	
	// negotiation
	int				doNegotiate(int, Stream *);
	int				negotiatorSocketHandler(Stream *);
	int				delayedNegotiatorHandler(Stream *);
	int				negotiate(int, Stream *);
	void			reschedule_negotiator(int, Stream *);
	void			vacate_service(int, Stream *);

	// job managing
	int				abort_job(int, Stream *);
	void			send_all_jobs(ReliSock*, struct sockaddr_in*);
	void			send_all_jobs_prioritized(ReliSock*, struct sockaddr_in*);
	friend	int		count(ClassAd *);
	friend	void	job_prio(ClassAd *);
	friend  int		find_idle_sched_universe_jobs(ClassAd *);
	void			display_shadow_recs();

	// match managing
    match_rec*       	AddMrec(char*, char*, PROC_ID*);
    int         	DelMrec(char*);
    int         	DelMrec(match_rec*);
    int         	MarkDel(char*);
	shadow_rec*		FindSrecByPid(int);
	shadow_rec*		FindSrecByProcID(PROC_ID);
	void			RemoveShadowRecFromMrec(shadow_rec*);
	int				AlreadyMatched(PROC_ID*);
	int				Agent(char*, char*, char*, ClassAd*, match_rec*);
	void			StartJobs();
	void			StartSchedUniverseJobs();
	void			send_alive();
	void			StartJobHandler();
	bool			WriteAbortToUserLog(PROC_ID job_id);
	
  private:
	
	// information about this scheduler
	ClassAd*		ad;
	char*			MySockName;		// dhaval
	Scheduler*		myself;

	// information about the command port which Shadows use
	char*			MyShadowSockName;
	ReliSock*		shadowCommandrsock;
	SafeSock*		shadowCommandssock;
	
	// parameters controling the scheduling and starting shadow
	int				SchedDInterval;
	int				QueueCleanInterval;
	int				JobStartDelay;
	int				MaxJobsRunning;
	int				JobsStarted; // # of jobs started last negotiating session
	int				SwapSpace;	 // available at beginning of last session
	int				ShadowSizeEstimate;	// Estimate of swap needed to run a job
	int				SwapSpaceExhausted;	// True if job died due to lack of swap space
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
	Queue<shadow_rec*> RunnableJobQueue;
	int				StartJobTimer;
	int				timeoutid;		// daemoncore timer id for timeout()
	int				startjobsid;	// daemoncore timer id for StartJobs()
	int             startJobsDelayBit;  // for delay when starting jobs.
	
	// useful names
	char*			CondorViewHost;
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
	void			update_central_mgr(int command, char *host, int port);
	int				insert_owner(char*);
#ifndef WANT_DC_PM
	void			reaper(int);
#endif
	void			child_exit(int, int);
	void			clean_shadow_recs();
	void			preempt(int);
		/** We add a match record (AddMrec), then open a ReliSock to the
			startd.  We push the capability and the jobAd, then register
			the Socket with startdContactSockHandler and put the new mrec
			into the daemonCore data pointer.  
			@param capability AKA id, this identifies the mrec
			@param user The submitting "owner@uiddomain"
			@param server The startd to contact
			@param jobId Put in the mrec and used to get the jobAd
			@return 0 on failure and 1 on success
		 */
	int		 		contactStartd(char *capability , char *user, 
								  char *server, PROC_ID *jobId   );

		/** Registered in contactStartd, this function is called when
			the startd replies to our request.  If it replies in the 
			positive, the mrec->status is set to M_ACTIVE, else the 
			mrec gets deleted.  The 'sock' is de-registered and deleted
			before this function returns.
			@param sock The sock with the startd's reply.
			@return FALSE on denial/problems, TRUE on success
		*/
	int             startdContactSockHandler( Stream *sock );

	shadow_rec*		StartJob(match_rec*, PROC_ID*);
	shadow_rec*		start_std(match_rec*, PROC_ID*);
	shadow_rec*		start_pvm(match_rec*, PROC_ID*);
	shadow_rec*     start_mpi(match_rec*, PROC_ID*);
	shadow_rec*		start_sched_universe_job(PROC_ID*);
	void			Relinquish(match_rec*);
	void 			swap_space_exhausted();
	void			delete_shadow_rec(int);
	void			mark_job_running(PROC_ID*);
	int				is_alive(shadow_rec* srec);
	void			check_zombie(int, PROC_ID*);
	void			kill_zombie(int, PROC_ID*);
	shadow_rec*     find_shadow_rec(PROC_ID*);
	shadow_rec*     add_shadow_rec(int, PROC_ID*, match_rec*, int);
	shadow_rec*		add_shadow_rec(shadow_rec*);
	void			NotifyUser(shadow_rec*, char*, int, int);
	void			HadException( match_rec* );
	
#ifdef CARMI_OPS
	shadow_rec*		find_shadow_by_cluster( PROC_ID * );
#endif

	HashTable <HashKey, match_rec *> *matches;
	HashTable <int, shadow_rec *> *shadowsByPid;
	HashTable <PROC_ID, shadow_rec *> *shadowsByProcID;
	int				numMatches;
	int				numShadows;
	List <PROC_ID>	*IdleSchedUniverseJobIDs;
	StringList		*FlockHosts;
	int				FlockLevel;
	int				MaxFlockLevel;
    int         	aliveInterval;             // how often to broadcast alive
	int				MaxExceptions;		// Max shadow exceptions before we relinquish
};
	
#endif
