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
// //////////////////////////////////////////////////////////////////////
//
// condor_sched.h
//
// Define class Scheduler. This class does local scheduling and then negotiates
// with the central manager to obtain resources for jobs.
//
// //////////////////////////////////////////////////////////////////////

#ifndef _CONDOR_SCHED_H_
#define _CONDOR_SCHED_H_

#include "dc_collector.h"
#include "daemon.h"
#include "daemon_list.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "proc.h"
#include "prio_rec.h"
#include "HashTable.h"
#include "string_list.h"
#include "list.h"
#include "classad_hashtable.h"	// for HashKey class
#include "Queue.h"
#include "user_log.c++.h"
#include "autocluster.h"
#include "shadow_mgr.h"

const 	int			MAX_REJECTED_CLUSTERS = 1024;
const   int         STARTD_CONTACT_TIMEOUT = 45;
const	int			NEGOTIATOR_CONTACT_TIMEOUT = 30;

extern	DLL_IMPORT_MAGIC char**		environ;

class match_rec;

struct shadow_rec
{
    int             pid;
    PROC_ID         job_id;
    match_rec*	    match;
    int             preempted;
    int             conn_fd;
	int				removed;
    char*           sinfulString;  // added for V6.1 Shadow by MEY
	bool			isZombie;	// added for Maui by stolley
}; 

struct OwnerData {
  char* Name;
  char* Domain;
  char* X509;
  int JobsRunning;
  int JobsIdle;
  int JobsHeld;
  int JobsFlocked;
  int FlockLevel;
  int OldFlockLevel;
  int GlobusJobs;
  int GlobusUnmanagedJobs;
  time_t NegotiationTimestamp;
  OwnerData() { Name=NULL; Domain=NULL; X509=NULL;
  JobsRunning=JobsIdle=JobsHeld=JobsFlocked=FlockLevel=OldFlockLevel=GlobusJobs=GlobusUnmanagedJobs=0; }
};

#define SIZE_OF_CAPABILITY_STRING 40 /* see also matchmater.C */
class match_rec
{
 public:
    match_rec(char*, char*, PROC_ID*, ClassAd*, char*, char* pool);
	~match_rec();
    char    		id[SIZE_OF_CAPABILITY_STRING];
    char    		peer[50];
	
		// cluster of the job we used to obtain the match
	int				origcluster; 

		// if match is currently active, cluster and proc of the
		// running job associated with this match; otherwise,
		// cluster==origcluster and proc==-1
    int     		cluster;
    int     		proc;

    int     		status;
	shadow_rec*		shadowRec;
	int				alive_countdown;
	int				num_exceptions;
	int				entered_current_status;
	ClassAd*		my_match_ad;
	char*			user;
	char*			pool;		// negotiator hostname if flocking; else NULL
	bool			sent_alive_interval;
	bool			allocated;	// For use by the DedicatedScheduler
	bool			scheduled;	// For use by the DedicatedScheduler

		// Set the mrec status to the given value (also updates
		// entered_current_status)
	void	setStatus( int stat );
};

enum MrecStatus {
    M_UNCLAIMED,
	M_STARTD_CONTACT_LIMBO,  // after contacting startd; before recv'ing reply
	M_CLAIMED,
    M_ACTIVE
};

	
typedef enum {
	NO_SHADOW_STD,
	NO_SHADOW_JAVA,
	NO_SHADOW_WIN32,
	NO_SHADOW_DC_VANILLA,
	NO_SHADOW_OLD_VANILLA,
} NoShadowFailure_t;


// These are the args to contactStartd that get stored in the queue.
class ContactStartdArgs
{
public:
	ContactStartdArgs( char* the_capab, char* the_owner, char*
					   the_sinful, PROC_ID the_id, ClassAd* match,
					   char* the_pool, bool is_dedicated );
	~ContactStartdArgs();

	char*		sinful( void )		{ return csa_sinful; };
	char*		capability( void )	{ return csa_capability; };
	char*		owner( void )		{ return csa_owner; };
	char*		pool( void )		{ return csa_pool; };
	ClassAd*	matchAd( void )		{ return csa_match_ad; };
	bool		isDedicated( void )	{ return csa_is_dedicated; };
	int			cluster( void ) 	{ return csa_id.cluster; };
	int			proc( void )		{ return csa_id.proc; };

private:
	char *csa_capability;
	char *csa_owner;
	char *csa_sinful;
	PROC_ID csa_id;
	ClassAd *csa_match_ad;
	char *csa_pool;	
	bool csa_is_dedicated;
};


// SC2000 This struct holds the state in ContactStartdArgs.  We register
// a pointer to this state so we can restore it after a non-blocking connect.
struct contactStartdState {
    match_rec* mrec;
    char* capability;
    char* server;
    ClassAd *jobAd;
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
	void			schedd_exit();
	void			invalidate_ads();
	
	// negotiation
	int				doNegotiate(int, Stream *);
	int				negotiatorSocketHandler(Stream *);
	int				delayedNegotiatorHandler(Stream *);
	int				negotiate(int, Stream *);
	void			reschedule_negotiator(int, Stream *);
	void			vacate_service(int, Stream *);
	AutoCluster		autocluster;
	void			sendReschedule( void );

	// job managing
	int				abort_job(int, Stream *);
	void			send_all_jobs(ReliSock*, struct sockaddr_in*);
	void			send_all_jobs_prioritized(ReliSock*, struct sockaddr_in*);
	friend	int		count(ClassAd *);
	friend	void	job_prio(ClassAd *);
	friend  int		find_idle_sched_universe_jobs(ClassAd *);
	void			display_shadow_recs();
	int				actOnJobs(int, Stream *);
	int				spoolJobFiles(int, Stream *);
	static int		spoolJobFilesWorkerThread(void *, Stream*);
	int				spoolJobFilesReaper(int,int);
	void				PeriodicExprHandler( void );

	// match managing
    match_rec*      AddMrec(char*, char*, PROC_ID*, ClassAd*, char*, char*);
    int         	DelMrec(char*);
    int         	DelMrec(match_rec*);
	shadow_rec*		FindSrecByPid(int);
	shadow_rec*		FindSrecByProcID(PROC_ID);
	void			RemoveShadowRecFromMrec(shadow_rec*);
	int				AlreadyMatched(PROC_ID*);
	void			StartJobs();
	void			StartSchedUniverseJobs();
	void			sendAlives();
	void			StartJobHandler();
	UserLog*		InitializeUserLog( PROC_ID job_id );
	bool			WriteAbortToUserLog( PROC_ID job_id );
	bool			WriteHoldToUserLog( PROC_ID job_id );
	bool			WriteReleaseToUserLog( PROC_ID job_id );
	bool			WriteExecuteToUserLog( PROC_ID job_id, const char* sinful = NULL );
	bool			WriteEvictToUserLog( PROC_ID job_id, bool checkpointed = false );
	bool			WriteTerminateToUserLog( PROC_ID job_id, int status );
#ifdef WANT_NETMAN
	void			RequestBandwidth(int cluster, int proc, match_rec *rec);
#endif

		// Public startd socket management functions
	void			addRegContact( void ) { num_reg_contacts++; };
	void			delRegContact( void ) { num_reg_contacts--; };
	int				numRegContacts( void ) { return num_reg_contacts; };
	void            checkContactQueue();

		/** Used to enqueue another set of information we need to use
			to contact a startd.  This is called by both
			Scheduler::negotiate() and DedicatedScheduler::negotiate()
			once a match is made.  This function will also start a
			timer to check the contact queue, if needed.
			@param args A structure that holds all the information
			@return true on success, false on failure (to enqueue).
		*/
	bool			enqueueStartdContact( ContactStartdArgs* args );

		/** Registered in contactStartd, this function is called when
			the startd replies to our request.  If it replies in the 
			positive, the mrec->status is set to M_ACTIVE, else the 
			mrec gets deleted.  The 'sock' is de-registered and deleted
			before this function returns.
			@param sock The sock with the startd's reply.
			@return FALSE on denial/problems, TRUE on success
		*/
	int             startdContactSockHandler( Stream *sock );

		// Useful public info
	char*			shadowSockSinful( void ) { return MyShadowSockName; };
	char*			dcSockSinful( void ) { return MySockName; };
	int				aliveInterval( void ) { return alive_interval; };
	char*			uidDomain( void ) { return UidDomain; };

		// Used by the DedicatedScheduler class
	void 			swap_space_exhausted();
	void			delete_shadow_rec(int);
	shadow_rec*     add_shadow_rec(int, PROC_ID*, match_rec*, int);
	shadow_rec*		add_shadow_rec(shadow_rec*);
	void			HadException( match_rec* );


		// Used by both the Scheduler and DedicatedScheduler during
		// negotiation
	bool canSpawnShadow( int started_jobs, int total_jobs );  
	void addActiveShadows( int num ) { CurNumActiveShadows += num; };
	
	// info about our central manager
	DCCollector*	Collector;
	Daemon*			Negotiator;

		// object to manage our various shadows and their ClassAds
	ShadowMgr shadow_mgr;

		// hashtable used to hold matching ClassAds for Globus Universe
		// jobs which desire matchmaking.
	HashTable <PROC_ID, ClassAd *> *resourcesByProcID;
  
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
	int				SchedDMinInterval;
	int				QueueCleanInterval;
	int				PeriodicExprInterval;
	int				JobStartDelay;
	int				MaxJobsRunning;
	bool			NegotiateAllJobsInCluster;
	int				JobsStarted; // # of jobs started last negotiating session
	int				SwapSpace;	 // available at beginning of last session
	int				ShadowSizeEstimate;	// Estimate of swap needed to run a job
	int				SwapSpaceExhausted;	// True if job died due to lack of swap space
	int				ReservedSwap;		// for non-condor users
	int				MaxShadowsForSwap;
	int				CurNumActiveShadows;
	int				JobsIdle; 
	int				JobsRunning;
	int				JobsHeld;
	int				JobsTotalAds;
	int				JobsFlocked;
	int				JobsRemoved;
	int				SchedUniverseJobsIdle;
	int				SchedUniverseJobsRunning;
	int				BadCluster;
	int				BadProc;
	//int				RejectedClusters[MAX_REJECTED_CLUSTERS];
	ExtArray<int>   RejectedClusters;
	int				N_RejectedClusters;
	ExtArray<OwnerData> Owners;
	int				N_Owners;
	time_t			LastTimeout;
	int				ExitWhenDone;  // Flag set for graceful shutdown
	Queue<shadow_rec*> RunnableJobQueue;
	int				StartJobTimer;
	int				timeoutid;		// daemoncore timer id for timeout()
	int				startjobsid;	// daemoncore timer id for StartJobs()

	int             startJobsDelayBit;  // for delay when starting jobs.

		// used so that we don't register too many Sockets at once & fd panic
	int             num_reg_contacts;  
		// Here we enqueue calls to 'contactStartd' when we can't just 
		// call it any more.  See contactStartd and the call to it...
	Queue<ContactStartdArgs*> startdContactQueue;
	int             MAX_STARTD_CONTACTS;
	int				checkContactQueue_tid;	// DC Timer ID to check queue
	
	// useful names
	char*			CondorAdministrator;
	char*			Mail;
	char*			filename;					// save UpDown object
	char*			AccountantName;
    char*			UidDomain;

	bool reschedule_request_pending;

	// connection variables
	struct sockaddr_in	From;
	int					Len; 

	// utility functions
	int				shadow_prio_recs_consistent();
	void			mail_problem_message();
	int				cluster_rejected(int);
	void   			mark_cluster_rejected(int); 
	int				count_jobs();
	int				insert_owner(char*, char*);
	void			child_exit(int, int);
	void			clean_shadow_recs();
	void			preempt(int);
	void			preempt_one_job();
	static void		refuse( Stream* s );
	void			tryNextJob( void );
	void	noShadowForJob( shadow_rec* srec, NoShadowFailure_t why );


		/** We add a match record (AddMrec), then open a ReliSock to the
			startd.  We push the capability and the jobAd, then register
			the Socket with startdContactSockHandler and put the new mrec
			into the daemonCore data pointer.  
			@param args An object that holds all the info we care about 
			@return false on failure and true on success
		 */
	bool	contactStartd( ContactStartdArgs* args );

	shadow_rec*		StartJob(match_rec*, PROC_ID*);
	shadow_rec*		start_std(match_rec*, PROC_ID*);
	shadow_rec*		start_pvm(match_rec*, PROC_ID*);
	shadow_rec*		start_sched_universe_job(PROC_ID*);
	void			Relinquish(match_rec*);
	void			check_zombie(int, PROC_ID*);
	void			kill_zombie(int, PROC_ID*);
	int				is_alive(shadow_rec* srec);
	shadow_rec*     find_shadow_rec(PROC_ID*);
	void			NotifyUser(shadow_rec*, char*, int, int);
	
#ifdef CARMI_OPS
	shadow_rec*		find_shadow_by_cluster( PROC_ID * );
#endif

	HashTable <HashKey, match_rec *> *matches;
	HashTable <int, shadow_rec *> *shadowsByPid;
	HashTable <PROC_ID, shadow_rec *> *shadowsByProcID;
	HashTable <int, ExtArray<PROC_ID> *> *spoolJobFileWorkers;
	int				numMatches;
	int				numShadows;
	List <PROC_ID>	*IdleSchedUniverseJobIDs;
	DaemonList		*FlockCollectors, *FlockNegotiators;
	int				MaxFlockLevel;
	int				FlockLevel;
    int         	alive_interval;  // how often to broadcast alive
	int				MaxExceptions;	 // Max shadow excep. before we relinquish
	bool			ManageBandwidth;

		// put state into ClassAd return it.  Used for condor_squawk
	int	dumpState(int, Stream *);
	int intoAd ( ClassAd *ad, char *lhs, char *rhs );
	int intoAd ( ClassAd *ad, char *lhs, int rhs );

		// A bit that says wether or not we've sent email to the admin
		// about a shadow not starting.
	int sent_shadow_failure_email;

};


// Other prototypes
extern void set_job_status(int cluster, int proc, int status);
extern bool claimStartd( match_rec* mrec, ClassAd* job_ad, bool is_dedicated );
extern bool sendAlive( match_rec* mrec );
extern void fixReasonAttrs( PROC_ID job_id, int action );
extern bool moveStrAttr( PROC_ID job_id, const char* old_attr,  
						 const char* new_attr, bool verbose );
extern bool moveIntAttr( PROC_ID job_id, const char* old_attr,  
						 const char* new_attr, bool verbose );
extern bool abortJob( int cluster, int proc, const char *reason, bool use_transaction );
extern bool holdJob( int cluster, int proc, const char* reason = NULL, 
					 bool use_transaction = false, 
					 bool notify_shadow = true,  
					 bool email_user = false, bool email_admin = false,
					 bool system_hold = true);
extern bool releaseJob( int cluster, int proc, const char* reason = NULL, 
					 bool use_transaction = false, 
					 bool email_user = false, bool email_admin = false,
					 bool write_to_user_log = true);
#endif /* _CONDOR_SCHED_H_ */
