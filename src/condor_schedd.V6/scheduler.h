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
#include <unordered_set>
#include <unordered_map>
#include <queue>

// use a persistent JobQueueUserRec instead of ephemeral OwnerInfo class
#define USE_JOB_QUEUE_USERREC 1

#include "dc_collector.h"
#include "daemon.h"
#include "daemon_list.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "proc.h"
#include "prio_rec.h"
#include "HashTable.h"
#include "write_user_log.h"
#include "autocluster.h"
#include "enum_utils.h"
#include "self_draining_queue.h"
#include "schedd_cron_job_mgr.h"
#include "named_classad_list.h"
#include "env.h"
#include "condor_crontab.h"
#include "condor_timeslice.h"
#include "condor_claimid_parser.h"
#include "transfer_queue.h"
#include "timed_queue.h"
#include "schedd_stats.h"
#include "condor_holdcodes.h"
#include "job_transforms.h"
#include "history_queue.h"
#include "live_job_counters.h"

extern  int         STARTD_CONTACT_TIMEOUT;
const	int			NEGOTIATOR_CONTACT_TIMEOUT = 30;

#define DEFAULT_MAX_JOB_QUEUE_LOG_ROTATIONS 1

extern	DLL_IMPORT_MAGIC char**		environ;

extern char const * const HOME_POOL_SUBMITTER_TAG;

void AuditLogNewConnection( int cmd, Sock &sock, bool failure );
bool removeOtherJobs(int cluster_id, int proc_id);

//
// Given a ClassAd from the job queue, we check to see if it
// has the ATTR_SCHEDD_INTERVAL attribute defined. If it does, then
// then we will simply update it with the latest value
// This needs to be here because it causes problems in schedd_main.C 
// with new compilers (gcc 4.1+)
//
class JobQueueJob;
extern int updateSchedDInterval( JobQueueJob*, const JOB_ID_KEY&, void* );

class JobQueueCluster;
class JobQueueJobSet;
class JobQueueBase;
class JobQueueUserRec;
class TransactionWatcher;

//typedef std::set<JOB_ID_KEY> JOB_ID_SET;
class LocalJobRec {
  public:
	int			prio;
	JOB_ID_KEY 	job_id;

	LocalJobRec(int _prio, JOB_ID_KEY _job_id) :
		prio(_prio),
		job_id(_job_id)
	{}
	bool operator<(const LocalJobRec& rhs) const {
		// We want LocalJobRec objects to run in priority order, but when two
		// records have equal priority, they run in order of submission which we
		// infer from the cluster.proc pair (lower numbers go first)
		if(this->prio == rhs.prio) {
			if(this->job_id.cluster == rhs.job_id.cluster) {
				return this->job_id.proc < rhs.job_id.proc;
			}
			return this->job_id.cluster < rhs.job_id.cluster;
		}
		// When sorting in priority order, higher numbers go first
		return this->prio > rhs.prio;
	}
};

bool jobLeaseIsValid( ClassAd* job, int cluster, int proc );

class match_rec;

struct shadow_rec
{
    int             pid;
    PROC_ID         job_id;
	int				universe;
    match_rec*	    match;
    bool            preempted;
	bool            preempt_pending;
    int             conn_fd;
	int				removed;
	bool			isZombie;	// added for Maui by stolley
	bool			is_reconnect;
	bool			reconnect_done; // could be success or failure
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


struct SubmitterFlockCounters {
  int JobsRunning{0};
  int WeightedJobsRunning{0};
  int JobsIdle{0};
  int WeightedJobsIdle{0};
};

// counters within the SubmitterData struct that are cleared and re-computed by count_jobs.
struct SubmitterCounters {
  int JobsRunning;
  int JobsIdle;
  double WeightedJobsRunning;
  double WeightedJobsIdle;
  int JobsHeld;
  int JobsFlocked;
  int SchedulerJobsRunning; // Scheduler Universe (i.e dags)
  int SchedulerJobsIdle;    // Scheduler Universe (i.e dags)
  int LocalJobsRunning; // Local universe
  int LocalJobsIdle;    // Local universe
  int Hits;  // used in the mark/sweep algorithm of count_jobs to detect Owners that no longer have any jobs in the queue.
  int JobsCounted; // smaller than Hits by the number of match recs for this Owner.
//  int JobsRecentlyAdded; // zeroed on each sweep, incremented on submission.
  void clear_job_counters() { memset(this, 0, sizeof(*this)); }
  SubmitterCounters()
	: JobsRunning(0)
	, JobsIdle(0)
	, WeightedJobsRunning(0)
	, WeightedJobsIdle(0)
	, JobsHeld(0)
	, JobsFlocked(0)
	, SchedulerJobsRunning(0), SchedulerJobsIdle(0)
	, LocalJobsRunning(0), LocalJobsIdle(0)
	, Hits(0)
	, JobsCounted(0)
//	, JobsRecentlyAdded(0)
  {}
};

// The schedd will have one of these records for each SUBMITTER, the submitter name is the
// same as the owner name for jobs that have no NiceUser or AccountingGroup attribute.
// This record is used to construct submitter ads. 
struct SubmitterData {
  std::string name;
  const char * Name() const { return name.empty() ? "" : name.c_str(); }
  bool empty() const { return name.empty(); }
  SubmitterCounters num;
  std::unordered_map<std::string, SubmitterFlockCounters> flock; // Per-pool flock information
  std::unordered_set<std::string> owners; // Number of unique owners observed using this submitter.
  time_t LastHitTime; // records the last time we incremented num.Hit, use to expire Owners
  // Time of most recent change in flocking level or
  // successful negotiation at highest current flocking
  // level.
  int FlockLevel;
  int OldFlockLevel;
  time_t NegotiationTimestamp;
  time_t lastUpdateTime; // the last time we sent updates to the collector
  bool isOwnerName; // the name of this submitter record is the same as the name of an owner record.
  bool absentUpdateSent;
  std::set<int> PrioSet; // Set of job priorities, used for JobPrioArray attr
  SubmitterData() : LastHitTime(0), FlockLevel(0), OldFlockLevel(0), NegotiationTimestamp(0)
      , lastUpdateTime(0), isOwnerName(false), absentUpdateSent(false)  { }
};

typedef std::map<std::string, SubmitterData> SubmitterDataMap;

#ifdef USE_JOB_QUEUE_USERREC
class JobQueueUserRec;
typedef JobQueueUserRec OwnerInfo;
typedef std::map<std::string, JobQueueUserRec*> OwnerInfoMap;
// attribute of the JobQueueUserRec to use as the Name() and key value of the OwnerInfo struct
constexpr int  CONDOR_USERREC_ID = 1;
constexpr int  LAST_RESERVED_USERREC_ID = CONDOR_USERREC_ID;

#include "userrec.h"
#else

struct RealOwnerCounters {
  int Hits;                 // counts (possibly overcounts) references to this class, used for mark/sweep expiration code
  int JobsCounted;          // ALL jobs in the queue owned by this owner at the time count_jobs() was run
  int JobsRecentlyAdded;    // ALL jobs owned by this owner that were added since count_jobs() was run
  int JobsIdle;             // does not count Local or Scheduler universe jobs, or Grid jobs that are externally managed.
  int JobsRunning;
  int JobsHeld;
  int LocalJobsIdle;
  int LocalJobsRunning;
  int LocalJobsHeld;
  int SchedulerJobsIdle;
  int SchedulerJobsRunning;
  int SchedulerJobsHeld;
  void clear_counters() { memset(this, 0, sizeof(*this)); }
  RealOwnerCounters()
	: Hits(0)
	, JobsCounted(0)
	, JobsRecentlyAdded(0)
	, JobsIdle(0)
	, JobsRunning(0)
	, JobsHeld(0)
	, LocalJobsIdle(0)
	, LocalJobsRunning(0)
	, LocalJobsHeld(0)
	, SchedulerJobsIdle(0)
	, SchedulerJobsRunning(0)
	, SchedulerJobsHeld(0)
  {}
};

// The schedd will have one of these records for each unique owner, it counts jobs that
// have that Owner attribute even if the jobs also have an AccountingGroup or NiceUser
// attribute and thus have a different SUBMITTER name than their OWNER name
// The counts in this structure are used to enforce MAX_JOBS_PER_OWNER and other PER_OWNER
// limits, they are NOT sent to the collector and are never seen by the accountant - the SubmitterData is used for accounting
//
struct OwnerInfo {
  std::string name;
  const char * Name() const { return name.empty() ? "" : name.c_str(); }
  bool empty() const { return name.empty(); }
  RealOwnerCounters num; // job counts by OWNER rather than by submitter
  LiveJobCounters live; // job counts that are always up-to-date with the committed job state
  time_t LastHitTime; // records the last time we incremented num.Hit, use to expire OwnerInfo
  OwnerInfo() : LastHitTime(0) { }
};

typedef std::map<std::string, OwnerInfo> OwnerInfoMap;
#endif

class match_rec
{
 public:
    match_rec(char const*, char const*, PROC_ID*, const ClassAd*, char const*, char const* pool,bool is_dedicated);
	~match_rec();

    char*   		peer; //sinful address of startd
	std::string        m_description;

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
	int				num_exceptions;
	time_t			entered_current_status;
	ClassAd*		my_match_ad;
	ClassAd         m_added_attrs;
	char*			user;
	bool            is_dedicated; // true if this match belongs to ded. sched.
	bool			allocated;	// For use by the DedicatedScheduler
	bool			scheduled;	// For use by the DedicatedScheduler
	bool			needs_release_claim;
	bool use_sec_session;
	ClaimIdParser claim_id;
	classy_counted_ptr<DCMsgCallback> claim_requester;

		// if we created a dynamic hole in the DAEMON auth level
		// to support flocking, this will be set to the id of the
		// punched hole
	std::string*	auth_hole_id;

	bool m_startd_sends_alives;
	bool m_claim_pslot;

	int keep_while_idle; // number of seconds to hold onto an idle claim
	int idle_timer_deadline; // if the above is nonzero, abstime to hold claim

		// Set the mrec status to the given value (also updates
		// entered_current_status)
	void	setStatus( int stat );

	void makeDescription();
	char const *description() const {
		return m_description.c_str();
	}

	const std::string & getPool() const {return m_pool;}

	PROC_ID m_now_job;

	std::string m_pool; // negotiator hostname if flocking; else empty
};

class UserIdentity {
	public:
			// The default constructor is not recommended as it
			// has no real identity.  It only exists so
			// we can put UserIdentities in various templates.
		UserIdentity() : m_username(""), m_osname(""), m_auxid("") { }
		UserIdentity(const char * user, const char * osname, const char * aux)
			: m_username(user), m_osname(osname), m_auxid(aux) { }
		UserIdentity(const UserIdentity & src) {
			m_username = src.m_username;
			m_osname = src.m_osname;
			m_auxid = src.m_auxid;			
		}
		UserIdentity(JobQueueJob& job_ad);
		const UserIdentity & operator=(const UserIdentity & src) {
			m_username = src.m_username;
			m_osname = src.m_osname;
			m_auxid = src.m_auxid;
			return *this;
		}
		bool operator==(const UserIdentity & rhs) {
			return m_username == rhs.m_username && 
				m_auxid == rhs.m_auxid;
		}
		const std::string& username() const { return m_username; }
		const std::string& osname() const { return m_osname; }
		const std::string& auxid() const { return m_auxid; }

			// For use in HashTables
		static size_t HashFcn(const UserIdentity & index);
	
	private:
		std::string m_username;
		std::string m_osname;
		std::string m_auxid;
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


namespace std
{
	template<> struct hash<std::pair<std::string, std::string>>
	{
		typedef std::pair<std::string, std::string> argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& input) const noexcept
		{
			result_type const h1 ( std::hash<std::string>{}(input.first) );
			result_type const h2 ( std::hash<std::string>{}(input.second) );
			return h1 ^ (h2 << 1);
		}
	};
}

class PoolSubmitterMap
{
public:
	void AddSubmitter(const std::string &pool, const std::string &submitter, time_t now) {
		m_map[std::make_pair(pool, submitter)] = now + 1200;
	}

	bool IsSubmitterValid(const std::string &pool, const std::string &submitter, time_t now) const {
		auto iter = m_map.find({pool, submitter});
		if (iter == m_map.end()) {
			return false;
		}
		return iter->second > now;
	}

	void Cleanup(time_t now) {
		for (auto it = m_map.begin(); it != m_map.end();) {
			if (it->second <= now) {
				it = m_map.erase(it);
			} else {
				++it;
			}
		}
	}

private:
	std::unordered_map<std::pair<std::string, std::string>, time_t> m_map;
};

// These are the args to contactStartd that get stored in the queue.
class ContactStartdArgs
{
public:
	ContactStartdArgs( char const* the_claim_id, char const *extra_claims, const char* sinful, bool is_dedicated );
	~ContactStartdArgs();

	char*		claimId( void )		{ return csa_claim_id; };
	char*		extraClaims( void )	{ return csa_extra_claims; };
	char*		sinful( void )		{ return csa_sinful; }
	bool		isDedicated() const		{ return csa_is_dedicated; }

private:
	char *csa_claim_id;
	char *csa_extra_claims;
	char *csa_sinful;
	bool csa_is_dedicated;
};


#define USE_VANILLA_START 1

class VanillaMatchAd : public ClassAd
{
	public:
		/// Default constructor
		VanillaMatchAd() {};
		virtual ~VanillaMatchAd() { Reset(); };

	bool Insert(const std::string &attr, ClassAd*ad);
	bool EvalAsBool(ExprTree *expr, bool def_value);
	bool EvalExpr(ExprTree *expr, classad::Value &val);
	void Init(ClassAd* slot_ad, const OwnerInfo* powni, JobQueueJob * job);
	void Reset();
private:
	ClassAd owner_ad;
};

class JobSets; // forward reference - declared in jobsets.h

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
	void			timeout( int timerID = -1 );
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

	void			reschedule_negotiator_timer( int /* timerID */ ) { reschedule_negotiator(0, NULL); }
	void			release_claim(int, Stream *);
	// I think this is actually a serious bug...
	int				release_claim_command_handler(int i, Stream * s) { release_claim(i, s); return 0; }

	AutoCluster		autocluster;
		// send a reschedule command to the negotiatior unless we
		// have recently sent one and not yet heard from the negotiator
	void			sendReschedule( int timerID = -1 );
		// call this when state of job queue has changed in a way that
		// requires a new round of negotiation
	void            needReschedule();

	// [IPV6] These two functions are never called by others.
	// It is non-IPv6 compatible, though.
	void			send_all_jobs(ReliSock*, struct sockaddr_in*);
	void			send_all_jobs_prioritized(ReliSock*, struct sockaddr_in*);

	JobTransforms	jobTransforms;
	friend	int		NewProc(int cluster_id);
	friend	int		count_a_job(JobQueueBase*, const JOB_ID_KEY&, void* );
//	friend	void	job_prio(ClassAd *);
	void			AddRunnableLocalJobs();
	bool			IsLocalJobEligibleToRun(JobQueueJob* job);
	friend	int		updateSchedDInterval(JobQueueJob*, const JOB_ID_KEY&, void* );
    friend  void    add_shadow_birthdate(int cluster, int proc, bool is_reconnect);
	void			display_shadow_recs();
	int				actOnJobs(int, Stream *);
	void            enqueueActOnJobMyself( PROC_ID job_id, JobAction action, bool log );
	int             actOnJobMyselfHandler( ServiceData* data );
	int				updateGSICred(int, Stream* s);
	void            setNextJobDelay( ClassAd *job_ad, ClassAd *machine_ad );
	int				spoolJobFiles(int, Stream *);
	static int		spoolJobFilesWorkerThread(void *, Stream *);
	static int		transferJobFilesWorkerThread(void *, Stream *);
	static int		generalJobFilesWorkerThread(void *, Stream *);
	int				spoolJobFilesReaper(int,int);	
	int				transferJobFilesReaper(int,int);
	void			PeriodicExprHandler( int timerID = -1 );
	void			addCronTabClassAd( JobQueueJob* );
	void			addCronTabClusterId( int );
	void			indexAJob(JobQueueJob* job, bool loading_job_queue=false);
	void			removeJobFromIndexes(const JOB_ID_KEY& job_id, int job_prio=0);
	int				RecycleShadow(int cmd, Stream *stream);
	void			finishRecycleShadow(shadow_rec *srec);
	int				CmdDirectAttach(int cmd, Stream* stream);

	int			FindGManagerPid(PROC_ID job_id);

	// match managing
	int 			publish( ClassAd *ad );
	void			OptimizeMachineAdForMatchmaking(ClassAd *ad);
    match_rec*      AddMrec(char const*, char const*, PROC_ID*, const ClassAd*, char const*, char const*, match_rec **pre_existing=NULL);
	// support for START_VANILLA_UNIVERSE
	ExprTree *      flattenVanillaStartExpr(JobQueueJob * job, const OwnerInfo* powni);
	bool            jobCanUseMatch(JobQueueJob * job, ClassAd * slot_ad, const std::string &pool, const char *&because); // returns true when START_VANILLA allows this job to run on this match
	bool            jobCanNegotiate(JobQueueJob * job, const char *&because); // returns true when START_VANILLA allows this job to negotiate
	bool            vanillaStartExprIsConst(VanillaMatchAd &vad, bool &bval);
	bool            evalVanillaStartExpr(VanillaMatchAd &vad);

	// All deletions of match records _MUST_ go through DelMrec() to ensure
	// proper cleanup.
    int         	DelMrec(char const*);
    int         	DelMrec(match_rec*);
    int         	unlinkMrec(match_rec*);
	match_rec*      FindMrecByJobID(PROC_ID);
	match_rec*      FindMrecByClaimID(char const *claim_id);
	void            SetMrecJobID(match_rec *rec, int cluster, int proc);
	void            SetMrecJobID(match_rec *match, PROC_ID job_id);
	shadow_rec*		FindSrecByPid(int);
	shadow_rec*		FindSrecByProcID(PROC_ID);
	void			RemoveShadowRecFromMrec(shadow_rec*);
	void            sendSignalToShadow(pid_t pid,int sig,PROC_ID proc);
	int				AlreadyMatched(PROC_ID*);
	int				AlreadyMatched(JobQueueJob * job, int universe);
	void			ExpediteStartJobs() const;
	void			StartJobs( int timerID = -1 );
	void			StartJob(match_rec *rec);
	void			sendAlives( int timerID = -1 );
	void			RecomputeAliveInterval(int cluster, int proc);
	void			StartJobHandler( int timerID = -1 );
	void			addRunnableJob( shadow_rec* );
	void			spawnShadow( shadow_rec* );
	void			spawnLocalStarter( shadow_rec* );
	bool			claimLocalStartd();
	bool			isStillRunnable( int cluster, int proc, int &status ); 
	WriteUserLog*	InitializeUserLog( PROC_ID job_id );
	bool			WriteSubmitToUserLog( JobQueueJob* job, bool do_fsync, const char * warning );
	bool			WriteAbortToUserLog( PROC_ID job_id );
	bool			WriteHoldToUserLog( PROC_ID job_id );
	bool			WriteReleaseToUserLog( PROC_ID job_id );
	bool			WriteExecuteToUserLog( PROC_ID job_id, const char* sinful = NULL );
	bool			WriteEvictToUserLog( PROC_ID job_id, bool checkpointed = false );
	bool			WriteTerminateToUserLog( PROC_ID job_id, int status );
	bool			WriteRequeueToUserLog( PROC_ID job_id, int status, const char * reason );
	bool			WriteAttrChangeToUserLog( const char* job_id_str, const char* attr, const char* attr_value, const char* old_value);
	bool			WriteClusterSubmitToUserLog( JobQueueCluster* cluster, bool do_fsync );
	bool			WriteClusterRemoveToUserLog( JobQueueCluster* cluster, bool do_fsync );
	bool			WriteFactoryPauseToUserLog( JobQueueCluster* cluster, int hold_code, const char * reason, bool do_fsync=false ); // write pause or resume event.
	int				receive_startd_alive(int cmd, Stream *s) const;
	void			InsertMachineAttrs( int cluster, int proc, ClassAd *machine, bool do_rotation );
		// Public startd socket management functions
	void            checkContactQueue( int timerID = -1 );

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
	void			checkReconnectQueue( int timerID = -1 );
	void			makeReconnectRecords( PROC_ID* job, const ClassAd* match_ad );

	bool	spawnJobHandler( int cluster, int proc, shadow_rec* srec );
	bool 	enqueueFinishedJob( int cluster, int proc );


		// Useful public info
	char*			shadowSockSinful( void ) { return MyShadowSockName; };
	int				aliveInterval( void ) const { return alive_interval; };
	char*			uidDomain( void ) { return UidDomain; };
	const std::string & genericCEDomain() { return GenericCEDomain; }
	std::string 		accountingDomain() const { return AccountingDomain; };
	int				getMaxMaterializedJobsPerCluster() const { return MaxMaterializedJobsPerCluster; }
	bool			getAllowLateMaterialize() const { return AllowLateMaterialize; }
	bool			getNonDurableLateMaterialize() const { return NonDurableLateMaterialize; }
	const ClassAd & getUserRecDefaultsAd() const { return m_userRecDefaultsAd; }
	const ClassAd * getExtendedSubmitCommands() const { return &m_extendedSubmitCommands; }
	const std::string & getExtendedSubmitHelpFile() const { return m_extendedSubmitHelpFile; }
	bool			getEnableJobQueueTimestamps() const { return EnableJobQueueTimestamps; }
	int				getMaxJobsRunning() const { return MaxJobsRunning; }
	int				getJobsTotalAds() const { return JobsTotalAds; };
	int				getMaxJobsSubmitted() const { return MaxJobsSubmitted; };
	int				getMaxJobsPerOwner() const { return MaxJobsPerOwner; }
	int				getMaxJobsPerSubmission() const { return MaxJobsPerSubmission; }

		// Used by the UserIdentity class and some others
	const ExprTree*	getGridParsedSelectionExpr() const 
					{ return m_parsed_gridman_selection_expr; };
	const char*		getGridUnparsedSelectionExpr() const
					{ return m_unparsed_gridman_selection_expr; };


		// Used by the DedicatedScheduler class
	void 			swap_space_exhausted();
	void			delete_shadow_rec(int);
	void			delete_shadow_rec(shadow_rec *rec);
	shadow_rec*     add_shadow_rec( int pid, PROC_ID*, int univ, match_rec*,
									int fd );
	shadow_rec*		add_shadow_rec(shadow_rec*);
	void			add_shadow_rec_pid(shadow_rec*);
	void			HadException( match_rec* );

		// Used to manipulate the "extra ads" (read:Hawkeye)
	int adlist_register( const char *name );
	int adlist_replace( const char *name, ClassAd *newAd );
	int adlist_delete( const char *name );
	int adlist_publish( ClassAd *resAd );

		// Used by both the Scheduler and DedicatedScheduler during
		// negotiation
	bool canSpawnShadow();
	int shadowsSpawnLimit();

	void WriteRestartReport( int timerID = -1 );

	int				shadow_prio_recs_consistent();
	void			mail_problem_message();
	bool            FindRunnableJobForClaim(match_rec* mrec);

	bool usesLocalStartd() const { return m_use_startd_for_local;}

	//
	// Verifies that the new clusters created in the current transaction
	// each pass each submit requirement.
	//
	bool	shouldCheckSubmitRequirements();
	int		checkSubmitRequirements( ClassAd * procAd, CondorError * errorStack );

	// generic statistics pool for scheduler, in schedd_stats.h
	ScheddStatistics stats;
	ScheddOtherStatsMgr OtherPoolStats;

	// live counters for running/held/idle jobs
	LiveJobCounters liveJobCounts; // job counts that are always up-to-date with the committed job state

	// the significant attributes that the schedd belives are absolutely required.
	// This is NOT the effective set of sig attrs we get after we talk to negotiators
	// it is the basic set needed for correct operation of the Schedd: Requirements,Rank,
	classad::References MinimalSigAttrs;

#ifdef USE_JOB_QUEUE_USERREC
	int		nextUnusedUserRecId();
	JobQueueUserRec * jobqueue_newUserRec(int userrec_id);
	void jobqueue_deleteUserRec(JobQueueUserRec * uad);
	void mapPendingOwners();
	// these are used during startup to handle the case where jobs have Owner/User attributes but
	// there is no persistnt JobQueueUserRec in the job_queue
	const std::map<int, OwnerInfo*> & queryPendingOwners() { return pendingOwners; }
	void clearPendingOwners();
	bool HasPersistentOwnerInfo() const { return EnablePersistentOwnerInfo; }
#endif
	void deleteZombieOwners(); // delete all zombies (called on shutdown)
	void purgeZombieOwners();  // delete unreferenced zombies (called in count_jobs)
	const OwnerInfo * insert_owner_const(const char*);
	const OwnerInfo * lookup_owner_const(const char*);
	OwnerInfo * incrementRecentlyAdded(OwnerInfo * ownerinfo, const char * owner);

	std::set<LocalJobRec> LocalJobsPrioQueue;

	// Class to manage sets of Job 
	JobSets *jobSets;

	bool ExportJobs(ClassAd & result, std::set<int> & clusters, const char *output_dir, const OwnerInfo *user, const char * new_spool_dir="##");
	bool ImportExportedJobResults(ClassAd & result, const char * import_dir, const OwnerInfo *user);
	bool UnexportJobs(ClassAd & result, std::set<int> & clusters, const OwnerInfo *user);

	bool forwardMatchToSidecarCM(const char *claim_id, const char *claim_ids, ClassAd &match_ad, const char *slot_name);

	void doCheckpointCleanUp( int cluster, int proc, ClassAd * jobAd );

	// Return a pointer to the protected URL map for late materilization factories
	MapFile* getProtectedUrlMap() { return &m_protected_url_map; }

	ClassAd *getLocalStarterAd() {
		static bool firstTime = true;
		if (firstTime) {
			firstTime = false;
			m_local_starter_ad.Assign(ATTR_CONDOR_SCRATCH_DIR, "#CoNdOrScRaTcHdIr#");
		}
		return &m_local_starter_ad;
	}
private:

	bool JobCanFlock(classad::ClassAd &job_ad, const std::string &pool);

	// Setup a new security session for a remote negotiator.
	// Returns a capability that can be included in an ad sent to the collector.
	bool SetupNegotiatorSession(unsigned duration, const std::string &remote_pool,
		std::string &capability);

	// Setup a new security session for a trusted collector.
	// As with SetupNegotiator session, it returns a capability for embedding in the schedd ad.
	bool SetupCollectorSession(unsigned duration, std::string &capability);

	// Negotiator & collector sessions have claim IDs, which includes a sequence number
	uint64_t m_negotiator_seq{0};
	time_t m_scheduler_startup{0};

	// We have to evaluate requirements in the listed order to maintain
	// user sanity, so the submit requirements data structure must ordered.
	struct SubmitRequirementsEntry {
		std::string name;
		std::unique_ptr<classad::ExprTree> requirement;
		std::unique_ptr<classad::ExprTree> reason;
		bool isWarning;

		SubmitRequirementsEntry( const std::string& n, classad::ExprTree * r, classad::ExprTree * rr, bool iw ) : name(n), requirement(r), reason(rr), isWarning(iw) {}
	};

	typedef std::vector< SubmitRequirementsEntry > SubmitRequirements;

	// information about this scheduler
	SubmitRequirements	m_submitRequirements;
	ClassAd*			m_adSchedd;
	ClassAd*        	m_adBase;
	ClassAd             m_userRecDefaultsAd;
	ClassAd             m_extendedSubmitCommands;
	std::string         m_extendedSubmitHelpFile;
	ClassAd             m_local_starter_ad;

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
	bool			AllowLateMaterialize;
	bool			EnablePersistentOwnerInfo;
	bool			NonDurableLateMaterialize;	// for testing, use non-durable transactions when materializing new jobs
	bool			EnableJobQueueTimestamps;	// for testing
	int				MaxMaterializedJobsPerCluster;
	char*			StartLocalUniverse; // expression for local jobs
	char*			StartSchedulerUniverse; // expression for scheduler jobs
	int				MaxRunningSchedulerJobsPerOwner;
	int				MaxJobsSubmitted;
	int				MaxJobsPerOwner;
	int				MaxJobsPerSubmission;
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

	char*			LocalUnivExecuteDir;
	int				BadCluster;
	int				BadProc;
	int				NumSubmitters; // number of non-zero entries in Submitters map, set by count_jobs()
	SubmitterDataMap Submitters;   // map of job counters by submitter, used to make SUBMITTER ads
	int				NumUniqueOwners;
	int				NextOwnerId;
	OwnerInfoMap    OwnersInfo;    // map of job counters by owner, used to enforce MAX_*_PER_OWNER limits
	std::map<int, OwnerInfo*> pendingOwners; // OwnerInfo records that have been created but not yet committed
	std::vector<OwnerInfo*> zombieOwners; // OwnerInfo records that have been removed from the job_queue, but not yet deleted

	HashTable<UserIdentity, GridJobCounts> GridJobOwners;
	time_t			NegotiationRequestTime;
	int				ExitWhenDone;  // Flag set for graceful shutdown
	std::queue<shadow_rec*> RunnableJobQueue;
	int				StartJobTimer;
	int				timeoutid;		// daemoncore timer id for timeout()
	int				startjobsid;	// daemoncore timer id for StartJobs()
	int				jobThrottleNextJobDelay;	// used by jobThrottle()

	int				shadowReaperId; // daemoncore reaper id for shadows
//	int 				dirtyNoticeId;
//	int 				dirtyNoticeInterval;

		// Here we enqueue calls to 'contactStartd' when we can't just 
		// call it any more.  See contactStartd and the call to it...
	std::queue<ContactStartdArgs*> startdContactQueue;
	int				checkContactQueue_tid;	// DC Timer ID to check queue
	int num_pending_startd_contacts;
	int max_pending_startd_contacts;

		// If we we need to reconnect to disconnected starters, we
		// stash the proc IDs in here while we read through the job
		// queue.  Then, we can spawn all the shadows after the fact. 
	std::vector<PROC_ID> jobsToReconnect;
	int				checkReconnectQueue_tid;

		// queue for sending hold/remove signals to shadows
	SelfDrainingQueue stop_job_queue;
		// queue for doing other "act_on_job_myself" calls
		// such as releasing jobs
	SelfDrainingQueue act_on_job_myself_queue;

	SelfDrainingQueue job_is_finished_queue;
	int jobIsFinishedHandler( ServiceData* job_id );

	int checkpointCleanUpReaper(int, int);

		// variables to implement SCHEDD_VANILLA_START expression
	ConstraintHolder vanilla_start_expr;

		// Get the associated GridJobCounts object for a given
		// user identity.  If necessary, will create a new one for you.
		// You can read or write the values, but don't go
		// deleting the pointer!
	GridJobCounts * GetGridJobCounts(UserIdentity user_identity);

		// (un)parsed expressions from condor_config GRIDMANAGER_SELECTION_EXPR
	ExprTree* m_parsed_gridman_selection_expr;
	char* m_unparsed_gridman_selection_expr;

	ExprTree* m_jobCoolDownExpr {nullptr};

	// The object which manages the transfer queue
	TransferQueueManager m_xfer_queue_mgr;

	// Information to pass to shadows for contacting file transfer queue
	// manager.
	bool m_have_xfer_queue_contact;
	std::string m_xfer_queue_contact;

	// useful names
	char*			CondorAdministrator;
	char*			AccountantName;
    char*			UidDomain;
	// when QmgmtSetEffectiveOwner domain matches this domain, use UidDomain as the effective domain
	std::string		GenericCEDomain{"users.htcondor.org"};
	std::string		AccountingDomain;

	// connection variables
	struct sockaddr_in	From;

	ExprTree* slotWeightOfJob;
	ClassAd * slotWeightGuessAd;
	bool			m_use_slot_weights;

	// utility functions
	void		sumAllSubmitterData(SubmitterData &all);
	void		updateSubmitterAd(SubmitterData &submitterData, ClassAd &pAd, DCCollector *collector,  int flock_level, time_t time_now);
	int			count_jobs();
	bool		fill_submitter_ad(ClassAd & pAd, const SubmitterData & Owner, const std::string &pool_name, int flock_level);
	int			make_ad_list(ClassAdList & ads, ClassAd * pQueryAd=NULL);
	int			handleMachineAdsQuery( Stream * stream, ClassAd & queryAd );
	int			command_query_ads(int, Stream* stream);
	int			command_query_job_ads(int, Stream* stream);
	int			command_query_job_aggregates(ClassAd & query, Stream* stream);
	int			command_query_user_ads(int, Stream* stream);
	int			command_act_on_user_ads(int, Stream* stream);
	void   			check_claim_request_timeouts( void );
	OwnerInfo     * find_ownerinfo(const char*);
	OwnerInfo     * insert_ownerinfo(const char*);
	SubmitterData * insert_submitter(const char*);
	SubmitterData * find_submitter(const char*);
	OwnerInfo * get_submitter_and_owner(JobQueueJob * job, SubmitterData * & submitterinfo);
	OwnerInfo * get_ownerinfo(JobQueueJob * job);
	int			act_on_user(int cmd, const std::string & username, const ClassAd& cmdAd,
					TransactionWatcher & txn, CondorError & errstack, struct UpdateUserAttributesInfo & info);
	void		remove_unused_owners();
	void			child_exit(int, int);
	// AFAICT, reapers should be be registered void to begin with.
	int				child_exit_from_reaper(int a, int b) { child_exit(a, b); return 0; }
	void			scheduler_univ_job_exit(int pid, int status, shadow_rec * srec);
	void			scheduler_univ_job_leave_queue(PROC_ID job_id, int status, ClassAd *ad);
	void			clean_shadow_recs();
	void			preempt( int n, bool force_sched_jobs = false );
	void			attempt_shutdown( int timerID = -1 );
	static void		refuse( Stream* s );
	void			tryNextJob();
	int				jobThrottle( void );
	void			initLocalStarterDir( void );
	bool			jobExitCode( PROC_ID job_id, int exit_code );
	double			calcSlotWeight(match_rec *mrec) const;
	double			guessJobSlotWeight(JobQueueJob * job);
	
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
	std::set<int> cronTabClusterIds;
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
	void claimStartdForUs(DCMsgCallback *cb);

	shadow_rec*		StartJob(match_rec*, PROC_ID*);

	shadow_rec*		start_std(match_rec*, PROC_ID*, int univ);
	shadow_rec*		start_sched_universe_job(PROC_ID*);
	bool			spawnJobHandlerRaw( shadow_rec* srec, const char* path,
										ArgList const &args,
										Env const *env, 
										const char* name, bool want_udp );
	void			check_zombie(int, PROC_ID*);
	void			kill_zombie(int, PROC_ID*);
	int				is_alive(shadow_rec* srec);
	
	void			expand_mpi_procs(const std::vector<std::string> &, std::vector<std::string> &);

	static void		token_request_callback(bool success, void *miscdata);

	HashTable <std::string, match_rec *> *matches;
	HashTable <PROC_ID, match_rec *> *matchesByJobID;
	std::map<int, shadow_rec *> shadowsByPid;
	std::map<PROC_ID, shadow_rec *> shadowsByProcID;
	std::map<int, std::vector<PROC_ID> *> spoolJobFileWorkers;
	int				numMatches;
	int				numShadows;
	std::vector<DCCollector> FlockCollectors;
	std::vector<Daemon> FlockNegotiators;
	std::unordered_set<std::string> FlockPools; // Names of all "default" flocked collectors.
	std::unordered_map<std::string, std::unique_ptr<DCCollector>> FlockExtra; // User-provided flock targets.

	PoolSubmitterMap		SubmitterMap;  // Map between remote pools and advertised submitters

	int				MinFlockLevel;
	int				MaxFlockLevel;
	int				FlockLevel;
    int         	alive_interval;  // how often to broadcast alive
		// leaseAliveInterval is the minimum interval we need to send
		// keepalives based upon ATTR_JOB_LEASE_DURATION...
	int				leaseAliveInterval;  
	int				aliveid;	// timer id for sending keepalives to startd
	int				MaxExceptions;	 // Max shadow excep. before we relinquish

		// get connection info for creating sec session to a running job
		// (e.g. condor_ssh_to_job)
	int get_job_connect_info_handler(int, Stream* s);
	int get_job_connect_info_handler_implementation(int, Stream* s);

		// Given two job IDs, start the first job on the second job's resource.
	int reassign_slot_handler( int cmd, Stream * s );

		// Mark a job as clean
	int clear_dirty_job_attrs_handler(int, Stream *stream);

		// command handlers for Lumberjack export and import
	int export_jobs_handler(int, Stream *stream);
	int import_exported_job_results_handler(int, Stream *stream);
	int unexport_jobs_handler(int, Stream *stream);

		// Command handlers for direct startd
   int receive_startd_update(int, Stream *s);
   int receive_startd_invalidate(int, Stream *s);

   int local_startd_reaper(int pid, int status);
   int launch_local_startd();

		// Command handler for collector token requests.
	int handle_collector_token_request(int, Stream *s);

		// A bit that says wether or not we've sent email to the admin
		// about a shadow not starting.
	int sent_shadow_failure_email;

	bool m_need_reschedule;
	int m_send_reschedule_timer;
	Timeslice m_negotiate_timeslice;

	std::vector<std::string> m_job_machine_attrs;
	int m_job_machine_attrs_history_length;

	bool m_use_startd_for_local;
	int m_local_startd_pid;
	std::map<std::string, ClassAd *> m_unclaimedLocalStartds;
	std::map<std::string, ClassAd *> m_claimedLocalStartds;

    int m_userlog_file_cache_max;
    time_t m_userlog_file_cache_clear_last;
    int m_userlog_file_cache_clear_interval;
    WriteUserLog::log_file_cache_map_t m_userlog_file_cache;
    void userlog_file_cache_clear(bool force = false);
    void userlog_file_cache_erase(const int& cluster, const int& proc);

	// State for the history helper queue.
	// object to manage history queries in flight
	HistoryHelperQueue HistoryQue;

	bool m_matchPasswordEnabled;

	bool m_include_default_flock_param{true};
	DCTokenRequester m_token_requester;

	MapFile m_protected_url_map;

	friend class DedicatedScheduler;
};


// Other prototypes
class JobQueueJob;
struct JOB_ID_KEY;
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
extern void incrementJobAdAttr(int cluster, int proc, const char* attrName, const char *nestedAdAttrName = nullptr);
extern bool holdJob( int cluster, int proc, const char* reason = NULL, 
					 int reason_code=0, int reason_subcode=0,
					 bool use_transaction = false, 
					 bool email_user = false, bool email_admin = false,
					 bool system_hold = true,
					 bool write_to_user_log = true);
extern bool releaseJob( int cluster, int proc, const char* reason = NULL, 
					 bool use_transaction = false, 
					 bool email_user = false, bool email_admin = false,
					 bool write_to_user_log = true);
extern bool setJobFactoryPauseAndLog(JobQueueCluster * cluster, int pause_mode, int hold_code, const std::string& reason);


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

/* Returns true if an external manager (e.g. gridmanager, job router) has
 * indicated it is handling this job.
 */
bool jobExternallyManaged(ClassAd * ad);

/* Returns true if an external manager (e.g. gridmanager, job router) has
 * finished handling this job, the job is now in a terminal state
 * (COMPELTED, REMOVED), and the manager doesn't need to see the job again.
 */
bool jobManagedDone(ClassAd * ad);


#endif /* _CONDOR_SCHED_H_ */
