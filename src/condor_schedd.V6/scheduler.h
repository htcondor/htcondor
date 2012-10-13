/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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

#include <map>
#include <set>

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
#include "write_user_log.h"
#include "autocluster.h"
#include "shadow_mgr.h"
#include "enum_utils.h"
#include "self_draining_queue.h"
#include "schedd_cron_job_mgr.h"
#include "named_classad_list.h"
#include "env.h"
#include "tdman.h"
#include "condor_crontab.h"
#include "condor_timeslice.h"
#include "condor_claimid_parser.h"
#include "transfer_queue.h"
#include "timed_queue.h"
#include "schedd_stats.h"
#include "condor_holdcodes.h"

extern  int         STARTD_CONTACT_TIMEOUT;
const	int			NEGOTIATOR_CONTACT_TIMEOUT = 30;
const	int			SCHEDD_INTERVAL_DEFAULT = 300;
const	int			JOB_DEFERRAL_PREP_TIME_DEFAULT = 300; // seconds
const	int			JOB_DEFERRAL_WINDOW_DEFAULT = 0; // seconds

#define DEFAULT_MAX_JOB_QUEUE_LOG_ROTATIONS 1

extern	DLL_IMPORT_MAGIC char**		environ;

extern char const * const HOME_POOL_SUBMITTER_TAG;

//
// Given a ClassAd from the job queue, we check to see if it
// has the ATTR_SCHEDD_INTERVAL attribute defined. If it does, then
// then we will simply update it with the latest value
// This needs to be here because it causes problems in schedd_main.C 
// with new compilers (gcc 4.1+)
//
extern int updateSchedDInterval( ClassAd* );

class match_rec;

struct shadow_rec
{
    int             pid;
    PROC_ID         job_id;
	int				universe;
    match_rec*	    match;
    int             preempted;
    int             conn_fd;
	int				removed;
	bool			isZombie;	// added for Maui by stolley
	bool			is_reconnect;
		//
		// This flag will cause the schedd to keep certain claim
		// attributes for jobs with leases during a graceful shutdown
		// This ensures that the job can reconnect when we come back up
		//
	bool			keepClaimAttributes;

	PROC_ID			prev_job_id;
	Stream*			recycle_shadow_stream;
	bool			exit_already_handled;

	shadow_rec();
	~shadow_rec();
}; 

struct OwnerData {
  char* Name;
  char* Domain;
  int JobsRunning;
  int JobsIdle;
  int JobsHeld;
  int JobsFlocked;
  int FlockLevel;
  int OldFlockLevel;
		// Time of most recent change in flocking level or
		// successful negotiation at highest current flocking
		// level.
  time_t NegotiationTimestamp;
  std::set<int> PrioSet; // Set of job priorities, used for JobPrioArray attr
  OwnerData() { Name=NULL; Domain=NULL;
  NegotiationTimestamp=JobsRunning=JobsIdle=JobsHeld=JobsFlocked=FlockLevel=OldFlockLevel=0; }
};

class match_rec: public ClaimIdParser
{
 public:
    match_rec(char const*, char const*, PROC_ID*, const ClassAd*, char const*, char const* pool,bool is_dedicated);
	~match_rec();

    char*   		peer; //sinful address of startd
	MyString        m_description;

		// cluster of the job we used to obtain the match
	int				origcluster; 

		// if match is currently active, cluster and proc of the
		// running job associated with this match; otherwise,
		// cluster==origcluster and proc==-1
		// NOTE: use SetMrecJobID() to change cluster and proc,
		// because this updates the index of matches by job id.
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
	bool            is_dedicated; // true if this match belongs to ded. sched.
	bool			allocated;	// For use by the DedicatedScheduler
	bool			scheduled;	// For use by the DedicatedScheduler
	bool			needs_release_claim;
	classy_counted_ptr<DCMsgCallback> claim_requester;

		// if we created a dynamic hole in the DAEMON auth level
		// to support flocking, this will be set to the id of the
		// punched hole
	MyString*		auth_hole_id;

	bool m_startd_sends_alives;

	int keep_while_idle; // number of seconds to hold onto an idle claim
	int idle_timer_deadline; // if the above is nonzero, abstime to hold claim

		// Set the mrec status to the given value (also updates
		// entered_current_status)
	void	setStatus( int stat );

	void makeDescription();
	char const *description() {
		return m_description.Value();
	}
};

class UserIdentity {
	public:
			// The default constructor is not recommended as it
			// has no real identity.  It only exists so
			// we can put UserIdentities in various templates.
		UserIdentity() : m_username(""), m_domain(""), m_auxid("") { }
		UserIdentity(const char * user, const char * domainname, const char * aux)
			: m_username(user), m_domain(domainname), m_auxid(aux) { }		
		UserIdentity(const UserIdentity & src) {
			m_username = src.m_username;
			m_domain = src.m_domain;
			m_auxid = src.m_auxid;			
		}
		UserIdentity(const char * user, const char * domainname, ClassAd * ad);
		const UserIdentity & operator=(const UserIdentity & src) {
			m_username = src.m_username;
			m_domain = src.m_domain;
			m_auxid = src.m_auxid;
			return *this;
		}
		bool operator==(const UserIdentity & rhs) {
			return m_username == rhs.m_username && 
				m_domain == rhs.m_domain && 
				m_auxid == rhs.m_auxid;
		}
		MyString username() const { return m_username; }
		MyString domain() const { return m_domain; }
		MyString auxid() const { return m_auxid; }

			// For use in HashTables
		static unsigned int HashFcn(const UserIdentity & index);
	
	private:
		MyString m_username;
		MyString m_domain;
		MyString m_auxid;
};


struct GridJobCounts {
	GridJobCounts() : GridJobs(0), UnmanagedGridJobs(0) { }
	unsigned int GridJobs;
	unsigned int UnmanagedGridJobs;
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
	NO_SHADOW_RECONNECT,
	NO_SHADOW_MPI,
	NO_SHADOW_VM,
} NoShadowFailure_t;

// These are the args to contactStartd that get stored in the queue.
class ContactStartdArgs
{
public:
	ContactStartdArgs( char const* the_claim_id, char* sinful, bool is_dedicated );
	~ContactStartdArgs();

	char*		claimId( void )		{ return csa_claim_id; };
	char*		sinful( void )		{ return csa_sinful; }
	bool		isDedicated()		{ return csa_is_dedicated; }

private:
	char *csa_claim_id;
	char *csa_sinful;
	bool csa_is_dedicated;
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
	void			update_local_ad_file(); // warning, may be removed
	
	// negotiation
	int				negotiatorSocketHandler(Stream *);
	int				negotiate(int, Stream *);
	int				reschedule_negotiator(int, Stream *);
	void			negotiationFinished( char const *owner, char const *remote_pool, bool satisfied );

	void				reschedule_negotiator_timer() { reschedule_negotiator(0, NULL); }
	void			release_claim(int, Stream *);
	AutoCluster		autocluster;
		// send a reschedule command to the negotiatior unless we
		// have recently sent one and not yet heard from the negotiator
	void			sendReschedule();
		// call this when state of job queue has changed in a way that
		// requires a new round of negotiation
	void            needReschedule();

	// [IPV6] These two functions are never called by others.
	// It is non-IPv6 compatible, though.
	void			send_all_jobs(ReliSock*, struct sockaddr_in*);
	void			send_all_jobs_prioritized(ReliSock*, struct sockaddr_in*);

	friend	int		NewProc(int cluster_id);
	friend	int		count(ClassAd *);
	friend	void	job_prio(ClassAd *);
	friend  int		find_idle_local_jobs(ClassAd *);
	friend	int		updateSchedDInterval( ClassAd* );
    friend  void    add_shadow_birthdate(int cluster, int proc, bool is_reconnect = false);
	void			display_shadow_recs();
	int				actOnJobs(int, Stream *);
	void            enqueueActOnJobMyself( PROC_ID job_id, JobAction action, bool notify );
	int             actOnJobMyselfHandler( ServiceData* data );
	int				updateGSICred(int, Stream* s);
	void            setNextJobDelay( ClassAd *job_ad, ClassAd *machine_ad );
	int				spoolJobFiles(int, Stream *);
	static int		spoolJobFilesWorkerThread(void *, Stream *);
	static int		transferJobFilesWorkerThread(void *, Stream *);
	static int		generalJobFilesWorkerThread(void *, Stream *);
	int				spoolJobFilesReaper(int,int);	
	int				transferJobFilesReaper(int,int);
	void			PeriodicExprHandler( void );
	void			addCronTabClassAd( ClassAd* );
	void			addCronTabClusterId( int );
	int				RecycleShadow(int cmd, Stream *stream);
	void			finishRecycleShadow(shadow_rec *srec);

	int				requestSandboxLocation(int mode, Stream* s);
	int			FindGManagerPid(PROC_ID job_id);

	// match managing
	int 			publish( ClassAd *ad );
	void			OptimizeMachineAdForMatchmaking(ClassAd *ad);
    match_rec*      AddMrec(char const*, char const*, PROC_ID*, const ClassAd*, char const*, char const*, match_rec **pre_existing=NULL);
	// All deletions of match records _MUST_ go through DelMrec() to ensure
	// proper cleanup.
    int         	DelMrec(char const*);
    int         	DelMrec(match_rec*);
	match_rec*      FindMrecByJobID(PROC_ID);
	match_rec*      FindMrecByClaimID(char const *claim_id);
	void            SetMrecJobID(match_rec *rec, int cluster, int proc);
	void            SetMrecJobID(match_rec *match, PROC_ID job_id);
	shadow_rec*		FindSrecByPid(int);
	shadow_rec*		FindSrecByProcID(PROC_ID);
	void			RemoveShadowRecFromMrec(shadow_rec*);
	void            sendSignalToShadow(pid_t pid,int sig,PROC_ID proc);
	int				AlreadyMatched(PROC_ID*);
	void			ExpediteStartJobs();
	void			StartJobs();
	void			StartJob(match_rec *rec);
	void			StartLocalJobs();
	void			sendAlives();
	void			RecomputeAliveInterval(int cluster, int proc);
	void			StartJobHandler();
	void			addRunnableJob( shadow_rec* );
	void			spawnShadow( shadow_rec* );
	void			spawnLocalStarter( shadow_rec* );
	bool			claimLocalStartd();
	bool			isStillRunnable( int cluster, int proc, int &status ); 
	bool			jobNeedsTransferd( int cluster, int proc, int univ ); 
	bool			availableTransferd( int cluster, int proc ); 
	bool			availableTransferd( int cluster, int proc, 
						TransferDaemon *&td_ref ); 
	bool			startTransferd( int cluster, int proc ); 
	WriteUserLog*	InitializeUserLog( PROC_ID job_id );
	bool			WriteSubmitToUserLog( PROC_ID job_id, bool do_fsync );
	bool			WriteAbortToUserLog( PROC_ID job_id );
	bool			WriteHoldToUserLog( PROC_ID job_id );
	bool			WriteReleaseToUserLog( PROC_ID job_id );
	bool			WriteExecuteToUserLog( PROC_ID job_id, const char* sinful = NULL );
	bool			WriteEvictToUserLog( PROC_ID job_id, bool checkpointed = false );
	bool			WriteTerminateToUserLog( PROC_ID job_id, int status );
	bool			WriteRequeueToUserLog( PROC_ID job_id, int status, const char * reason );
	bool			WriteAttrChangeToUserLog( const char* job_id_str, const char* attr, const char* attr_value, const char* old_value);
	int				receive_startd_alive(int cmd, Stream *s);
	void			InsertMachineAttrs( int cluster, int proc, ClassAd *machine );
		// Public startd socket management functions
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

		/** Set a timer to call checkContactQueue() in the near future.
			This is called when we get new matches and when existing
			attempts to contact startds finish.
		*/
	void			rescheduleContactQueue();

		/** Registered in contactStartdConnected, this function is called when
			the startd replies to our request.  If it replies in the 
			positive, the mrec->status is set to M_ACTIVE, else the 
			mrec gets deleted.  The 'sock' is de-registered and deleted
			before this function returns.
			@param sock The sock with the startd's reply.
			@return FALSE on denial/problems, TRUE on success
		*/
	int             startdContactSockHandler( Stream *sock );

		/** Registered in contactStartd, this function is called when
			the non-blocking connection is opened.
			@param sock The sock with the connection to the startd.
			@return FALSE on denial/problems, KEEP_STREAM on success
		*/
	int startdContactConnectHandler( Stream *sock );

		/** Used to enqueue another disconnected job that we need to
			spawn a shadow to attempt to reconnect to.
		*/
	bool			enqueueReconnectJob( PROC_ID job );
	void			checkReconnectQueue( void );
	void			makeReconnectRecords( PROC_ID* job, const ClassAd* match_ad );

	bool	spawnJobHandler( int cluster, int proc, shadow_rec* srec );
	bool 	enqueueFinishedJob( int cluster, int proc );


		// Useful public info
	char*			shadowSockSinful( void ) { return MyShadowSockName; };
	int				aliveInterval( void ) { return alive_interval; };
	char*			uidDomain( void ) { return UidDomain; };
	int				getJobsTotalAds() { return JobsTotalAds; };
	int				getMaxJobsSubmitted() { return MaxJobsSubmitted; };

		// Used by the UserIdentity class and some others
	const ExprTree*	getGridParsedSelectionExpr() const 
					{ return m_parsed_gridman_selection_expr; };
	const char*		getGridUnparsedSelectionExpr() const
					{ return m_unparsed_gridman_selection_expr; };


		// Used by the DedicatedScheduler class
	void 			swap_space_exhausted();
	void			delete_shadow_rec(int);
	void			delete_shadow_rec(shadow_rec*);
	shadow_rec*     add_shadow_rec( int pid, PROC_ID*, int univ, match_rec*,
									int fd );
	shadow_rec*		add_shadow_rec(shadow_rec*);
	void			add_shadow_rec_pid(shadow_rec*);
	void			HadException( match_rec* );

	// Callbacks which are notifications from the TDMan object about
	// registrations and reaping of transfer daemons
	// These functions DO NOT own the memory passed to them.
	TdAction td_register_callback(TransferDaemon *td);
	TdAction td_reaper_callback(long pid, int status, TransferDaemon *td);

	// Callbacks to handle transfer requests for clients uploading files into
	// Condor's control.
	// These functions DO NOT own the memory passed to them.
	TreqAction treq_upload_pre_push_callback(TransferRequest *treq,
		TransferDaemon *td);
	TreqAction treq_upload_post_push_callback(TransferRequest *treq, 
		TransferDaemon *td);
	TreqAction treq_upload_update_callback(TransferRequest *treq, 
		TransferDaemon *td, ClassAd *update);
	TreqAction treq_upload_reaper_callback(TransferRequest *treq);

	// Callbacks to handle transfer requests for clients downloading files
	// out of Condor's control.
	// These functions DO NOT own the memory passed to them.
	TreqAction treq_download_pre_push_callback(TransferRequest *treq, 
		TransferDaemon *td);
	TreqAction treq_download_post_push_callback(TransferRequest *treq, 
		TransferDaemon *td);
	TreqAction treq_download_update_callback(TransferRequest *treq, 
		TransferDaemon *td, ClassAd *update);
	TreqAction treq_download_reaper_callback(TransferRequest *treq);

		// Used to manipulate the "extra ads" (read:Hawkeye)
	int adlist_register( const char *name );
	int adlist_replace( const char *name, ClassAd *newAd );
	int adlist_delete( const char *name );
	int adlist_publish( ClassAd *resAd );

		// Used by both the Scheduler and DedicatedScheduler during
		// negotiation
	bool canSpawnShadow();

	int				shadow_prio_recs_consistent();
	void			mail_problem_message();
	bool            FindRunnableJobForClaim(match_rec* mrec,bool accept_std_univ=true);
	
		// object to manage our various shadows and their ClassAds
	ShadowMgr shadow_mgr;

		// hashtable used to hold matching ClassAds for Globus Universe
		// jobs which desire matchmaking.
	HashTable <PROC_ID, ClassAd *> *resourcesByProcID;
  
	bool usesLocalStartd() const { return m_use_startd_for_local;}
	
private:
	
	// information about this scheduler
	ClassAd*		m_adSchedd;
    ClassAd*        m_adBase;
	Scheduler*		myself;

	// information about the command port which Shadows use
	char*			MyShadowSockName;
	ReliSock*		shadowCommandrsock;
	SafeSock*		shadowCommandssock;

	// The "Cron" manager (Hawkeye) & it's classads
	ScheddCronJobMgr	*CronJobMgr;
	NamedClassAdList	 extra_ads;

	// parameters controling the scheduling and starting shadow
	Timeslice       SchedDInterval;
	Timeslice       PeriodicExprInterval;
	int             periodicid;
	int				QueueCleanInterval;
	int             RequestClaimTimeout;
	int				JobStartDelay;
	int				JobStartCount;
	int				JobStopDelay;
	int				JobStopCount;
	int             MaxNextJobDelay;
	int				JobsThisBurst;
	int				MaxJobsRunning;
	char*			StartLocalUniverse; // expression for local jobs
	char*			StartSchedulerUniverse; // expression for scheduler jobs
	int				MaxJobsSubmitted;
	bool			NegotiateAllJobsInCluster;
	int				JobsStarted; // # of jobs started last negotiating session
	int				SwapSpace;	 // available at beginning of last session
	int				ShadowSizeEstimate;	// Estimate of swap needed to run a job
	int				SwapSpaceExhausted;	// True if job died due to lack of swap space
	int				ReservedSwap;		// for non-condor users
	int				MaxShadowsForSwap;
	bool			RecentlyWarnedMaxJobsRunning;
	int				JobsIdle; 
	int				JobsRunning;
	int				JobsHeld;
	int				JobsTotalAds;
	int				JobsFlocked;
	int				JobsRemoved;
	int				SchedUniverseJobsIdle;
	int				SchedUniverseJobsRunning;
	int				LocalUniverseJobsIdle;
	int				LocalUniverseJobsRunning;

    // generic statistics pool for scheduler, in schedd_stats.h
    ScheddStatistics stats;
	ScheddOtherStatsMgr OtherPoolStats;

	char*			LocalUnivExecuteDir;
	int				BadCluster;
	int				BadProc;
	ExtArray<OwnerData> Owners; // May be tracking AccountingGroup instead of owner username/domain
	HashTable<UserIdentity, GridJobCounts> GridJobOwners;
	int				N_Owners;
	time_t			NegotiationRequestTime;
	int				ExitWhenDone;  // Flag set for graceful shutdown
	Queue<shadow_rec*> RunnableJobQueue;
	int				StartJobTimer;
	int				timeoutid;		// daemoncore timer id for timeout()
	int				startjobsid;	// daemoncore timer id for StartJobs()
	int				jobThrottleNextJobDelay;	// used by jobThrottle()

	int				shadowReaperId; // daemoncore reaper id for shadows
//	int 				dirtyNoticeId;
//	int 				dirtyNoticeInterval;

		// Here we enqueue calls to 'contactStartd' when we can't just 
		// call it any more.  See contactStartd and the call to it...
	Queue<ContactStartdArgs*> startdContactQueue;
	int				checkContactQueue_tid;	// DC Timer ID to check queue
	int num_pending_startd_contacts;
	int max_pending_startd_contacts;

		// If we we need to reconnect to disconnected starters, we
		// stash the proc IDs in here while we read through the job
		// queue.  Then, we can spawn all the shadows after the fact. 
	SimpleList<PROC_ID> jobsToReconnect;
	int				checkReconnectQueue_tid;

		// queue for sending hold/remove signals to shadows
	SelfDrainingQueue stop_job_queue;
		// queue for doing other "act_on_job_myself" calls
		// such as releasing jobs
	SelfDrainingQueue act_on_job_myself_queue;

	SelfDrainingQueue job_is_finished_queue;
	int jobIsFinishedHandler( ServiceData* job_id );

		// Get the associated GridJobCounts object for a given
		// user identity.  If necessary, will create a new one for you.
		// You can read or write the values, but don't go
		// deleting the pointer!
	GridJobCounts * GetGridJobCounts(UserIdentity user_identity);

		// (un)parsed expressions from condor_config GRIDMANAGER_SELECTION_EXPR
	ExprTree* m_parsed_gridman_selection_expr;
	char* m_unparsed_gridman_selection_expr;

	// The object which manages the various transferds.
	TDMan m_tdman;

	// The object which manages the transfer queue
	TransferQueueManager m_xfer_queue_mgr;

	// Information to pass to shadows for contacting file transfer queue
	// manager.
	bool m_have_xfer_queue_contact;
	std::string m_xfer_queue_contact;

	// useful names
	char*			CondorAdministrator;
	char*			Mail;
	char*			AccountantName;
    char*			UidDomain;

	// connection variables
	struct sockaddr_in	From;
	int					Len; 

	// utility functions
	int				count_jobs();
	bool			fill_submitter_ad(ClassAd & pAd, int owner_num, int flock_level=-1); 
    int             make_ad_list(ClassAdList & ads, ClassAd * pQueryAd=NULL);
    int             command_query_ads(int, Stream* stream);
	void   			check_claim_request_timeouts( void );
	int				insert_owner(char const*);
	void			child_exit(int, int);
	void			scheduler_univ_job_exit(int pid, int status, shadow_rec * srec);
	void			scheduler_univ_job_leave_queue(PROC_ID job_id, int status, ClassAd *ad);
	void			clean_shadow_recs();
	void			preempt( int n, bool force_sched_jobs = false );
	void			attempt_shutdown();
	static void		refuse( Stream* s );
	void			tryNextJob();
	int				jobThrottle( void );
	void			initLocalStarterDir( void );
	void	noShadowForJob( shadow_rec* srec, NoShadowFailure_t why );
	bool			jobExitCode( PROC_ID job_id, int exit_code );
	
	// -----------------------------------------------
	// CronTab Jobs
	// -----------------------------------------------
		//
		// Check our list of cluster_ids for new CronTab jobs
		//
	void processCronTabClusterIds();
		//
		// Calculate new runtimes for all our CronTab jobs
		//
	void calculateCronTabSchedules();
		//
		// Calculate the next runtime for a CronTab job
		//
	bool calculateCronTabSchedule( ClassAd*, bool force = false );
		//
		// This table is used for scheduling cron jobs
		// If a ClassAd has an entry in this table then we need
		// to query the CronTab object to ask it what the next
		// runtime is for job is
		//
	HashTable<PROC_ID, CronTab*> *cronTabs;	
		//
		// A list of cluster_ids that may have jobs that require
		// CronTab execution scheduling
		// This is used by processCronTabClusterIds()
		//
	Queue<int> cronTabClusterIds;
	// -----------------------------------------------

		/** We begin the process of opening a non-blocking ReliSock
			connection to the startd.

			We push the ClaimId and the jobAd, then register
			the Socket with startdContactSockHandler and put the new mrec
			into the daemonCore data pointer.  
			@param args An object that holds all the info we care about 
			@return false on failure and true on success
		 */
	void	contactStartd( ContactStartdArgs* args );
	void claimedStartd( DCMsgCallback *cb );

	shadow_rec*		StartJob(match_rec*, PROC_ID*);

	shadow_rec*		start_std(match_rec*, PROC_ID*, int univ);
	shadow_rec*		start_sched_universe_job(PROC_ID*);
	shadow_rec*		start_local_universe_job(PROC_ID*);
	bool			spawnJobHandlerRaw( shadow_rec* srec, const char* path,
										ArgList const &args,
										Env const *env, 
										const char* name, bool is_dc,
										bool wants_pipe, bool want_udp );
	void			check_zombie(int, PROC_ID*);
	void			kill_zombie(int, PROC_ID*);
	int				is_alive(shadow_rec* srec);
	shadow_rec*     find_shadow_rec(PROC_ID*);
	
#ifdef CARMI_OPS
	shadow_rec*		find_shadow_by_cluster( PROC_ID * );
#endif

	void			expand_mpi_procs(StringList *, StringList *);

	HashTable <HashKey, match_rec *> *matches;
	HashTable <PROC_ID, match_rec *> *matchesByJobID;
	HashTable <int, shadow_rec *> *shadowsByPid;
	HashTable <PROC_ID, shadow_rec *> *shadowsByProcID;
	HashTable <int, ExtArray<PROC_ID> *> *spoolJobFileWorkers;
	int				numMatches;
	int				numShadows;
	DaemonList		*FlockCollectors, *FlockNegotiators;
	int				MaxFlockLevel;
	int				FlockLevel;
    int         	alive_interval;  // how often to broadcast alive
		// leaseAliveInterval is the minimum interval we need to send
		// keepalives based upon ATTR_JOB_LEASE_DURATION...
	int				leaseAliveInterval;  
	int				aliveid;	// timer id for sending keepalives to startd
	int				MaxExceptions;	 // Max shadow excep. before we relinquish

		// put state into ClassAd return it.  Used for condor_squawk
	int	dumpState(int, Stream *);

		// get connection info for creating sec session to a running job
		// (e.g. condor_ssh_to_job)
	int get_job_connect_info_handler(int, Stream* s);
	int get_job_connect_info_handler_implementation(int, Stream* s);

		// Mark a job as clean
	int clear_dirty_job_attrs_handler(int, Stream *stream);

		// Command handlers for direct startd
   int receive_startd_update(int, Stream *s);
   int receive_startd_invalidate(int, Stream *s);

   int local_startd_reaper(int pid, int status);
   int launch_local_startd();

		// A bit that says wether or not we've sent email to the admin
		// about a shadow not starting.
	int sent_shadow_failure_email;

	bool m_need_reschedule;
	int m_send_reschedule_timer;
	Timeslice m_negotiate_timeslice;

	// some stuff about Quill that should go into the ad
#ifdef HAVE_EXT_POSTGRESQL
	int quill_enabled;
	int quill_is_remotely_queryable;
	char *quill_name;
	char *quill_db_name;
	char *quill_db_ip_addr;
	char *quill_db_query_password;
	int prevLHF;
#endif

	StringList m_job_machine_attrs;
	int m_job_machine_attrs_history_length;

	bool m_use_startd_for_local;
	int m_local_startd_pid;
	std::map<std::string, ClassAd *> m_unclaimedLocalStartds;
	std::map<std::string, ClassAd *> m_claimedLocalStartds;
};


// Other prototypes
int		get_job_prio(ClassAd *ad);
extern void set_job_status(int cluster, int proc, int status);
extern bool claimStartd( match_rec* mrec );
extern bool claimStartdConnected( Sock *sock, match_rec* mrec, ClassAd *job_ad);
extern bool sendAlive( match_rec* mrec );
extern void fixReasonAttrs( PROC_ID job_id, JobAction action );
extern bool moveStrAttr( PROC_ID job_id, const char* old_attr,  
						 const char* new_attr, bool verbose );
extern bool moveIntAttr( PROC_ID job_id, const char* old_attr,  
						 const char* new_attr, bool verbose );
extern bool abortJob( int cluster, int proc, const char *reason, bool use_transaction );
extern bool abortJobsByConstraint( const char *constraint, const char *reason, bool use_transaction );
extern bool holdJob( int cluster, int proc, const char* reason = NULL, 
					 int reason_code=0, int reason_subcode=0,
					 bool use_transaction = false, 
					 bool notify_shadow = true,  
					 bool email_user = false, bool email_admin = false,
					 bool system_hold = true);
extern bool releaseJob( int cluster, int proc, const char* reason = NULL, 
					 bool use_transaction = false, 
					 bool email_user = false, bool email_admin = false,
					 bool write_to_user_log = true);


/** Hook to call whenever we're going to give a job to a "job
	handler", be that a shadow, starter (local univ), or gridmanager.
	it takes the optional shadow record as a void* so this can be
	seamlessly used for Create_Thread_With_Data().  In fact, we don't
	need the srec at all for the function itself, but we'll need it as
	our data pointer in the reaper (so we can actually spawn the right
	handler), so we ignore it here, but will need it later...
*/
int aboutToSpawnJobHandler( int cluster, int proc, void* srec=NULL );


/** For use as a reaper with Create_Thread_With_Data(), or to call
	directly after aboutToSpawnJobHandler() if there's no thread. 
*/
int aboutToSpawnJobHandlerDone( int cluster, int proc, void* srec=NULL,
								int exit_status = 0 );


/** A helper function that wraps the call to jobPrepNeedsThread() and
	invokes aboutToSpawnJobHandler() as appropriate, either in its own
	thread using Create_Thread_Qith_Wata(), or calling it and then
	aboutToSpawnJobHandlerDone() directly.
*/
void callAboutToSpawnJobHandler( int cluster, int proc, shadow_rec* srec );


/** Hook to call whenever a job enters a "finished" state, something
	it can never get out of (namely, COMPLETED or REMOVED).  Like the
	aboutToSpawnJobHandler() hook above, this might be expensive, so
	if jobPrepNeedsThread() returns TRUE for this job, we should call
	this hook within a seperate thread.  Therefore, it takes the
	optional void* so it can be used for Create_Thread_With_Data().
*/
int jobIsFinished( int cluster, int proc, void* vptr = NULL );


/** For use as a reaper with Create_Thread_With_Data(), or to call
	directly after jobIsFinished() if there's no thread. 
*/
int jobIsFinishedDone( int cluster, int proc, void* vptr = NULL,
					   int exit_status = 0 );


#endif /* _CONDOR_SCHED_H_ */
