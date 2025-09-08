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

#if !defined(_QMGMT_H)
#define _QMGMT_H

#include "condor_classad.h"
#include "condor_io.h"
#include "log_transaction.h" // for Transaction
#include "prio_rec.h"
#include "condor_sockaddr.h"
#include "classad_log.h"
#include "live_job_counters.h"

// the pedantic idiots at gcc generate this warning whenever you use offsetof on a struct or class that has a constructor....
GCC_DIAG_OFF(invalid-offsetof)

// this header declares functions internal to the schedd, it should not be included by code external to the schedd
// and it should always be included before condor_qmgr.h so that it can disable external prototype declaraions in that file. 
#define SCHEDD_INTERNAL_DECLARATIONS
#ifdef SCHEDD_EXTERNAL_DECLARATIONS
#error This header must be included before condor_qmgr.h for code internal to the SCHEDD, and not at all for external code
#endif


// until we can remove targettype from the classad log entirely
// use the legacy "Machine" value as the target adtype
#define JOB_TARGET_ADTYPE "Machine"

class Service;

class QmgmtPeer {
	
	friend QmgmtPeer* getQmgmtConnectionInfo();
	friend bool setQmgmtConnectionInfo(QmgmtPeer*);
	friend void unsetQmgmtConnection();

	public:
		QmgmtPeer();
		~QmgmtPeer();

		bool set(ReliSock *sock);
		bool set(const condor_sockaddr& raddr, const char *fqOwnerAndDomain);
		void unset();

		bool initAuthOwner(bool read_only);
		bool setEffectiveOwner(const class JobQueueUserRec * urec, bool not_super_effective);
		// used during submit when a UserRec is created as a side effect of submit
		void attachNewUserRec(const class JobQueueUserRec * urec) { jquser = urec; }
		bool setAllowProtectedAttrChanges(bool val);
		bool getAllowProtectedAttrChanges() const { return allow_protected_attr_changes_by_superuser; }
		bool getReadOnly() const { return readonly; }
		bool getWriteAuth() const { return !readonly && write_ok; }

		ReliSock *getReliSock() const { return sock; };
		const CondorVersionInfo *get_peer_version() const { return sock->get_peer_version(); };

		const char *endpoint_ip_str() const;
		const condor_sockaddr endpoint() const;
		const char* getOwner() const { return owner ? owner : (sock ? sock->getOwner() : nullptr); }
		const char* getDomain() const { return sock ? sock->getDomain() : nullptr; }
		const char* getRealOwner() const { return sock ? sock->getOwner() : owner; }
		const char* getRealUser() const { return sock ? sock->getFullyQualifiedUser() : fquser; }
		const char* getUser() const  { return fquser ? fquser : (sock ? sock->getFullyQualifiedUser() : nullptr); }
		const class JobQueueUserRec * UserRec() const { return jquser; } // EffectiveUser as a JobQueueUserRec
		int isAuthenticated() const;
		bool isAuthorizationInBoundingSet(const char *authz) const {return sock->isAuthorizationInBoundingSet(authz);}

		friend inline const char * EffectiveUserName(QmgmtPeer * qsock);
		friend inline const class JobQueueUserRec * EffectiveUserRec(QmgmtPeer * qsock);

		CondorError& getErrStack() { return errstack; }

	protected:

		char *owner;  
		char *fquser;  // owner@domain
		const class JobQueueUserRec * jquser = nullptr; // same as 
		char *myendpoint; 
		condor_sockaddr addr;
		ReliSock *sock; 

		CondorError errstack;
		Transaction *transaction;
		int next_proc_num{}, active_cluster_num{};
		time_t xact_start_time{};

		bool readonly{false};
		bool write_ok = false;
		bool allow_protected_attr_changes_by_superuser{true};
		bool real_auth_is_super = false;	// real auth identifier is a super user
		bool not_super_effective = false;	// Ignore EffectiveOwner Superuser status


	private:
		// we do not allow deep-copies via copy ctor or assignment op,
		// so disable there here by making them private.
		QmgmtPeer(QmgmtPeer&);	
		QmgmtPeer & operator=(const QmgmtPeer & rhs);
};

extern bool user_is_the_new_owner; // set in schedd.cpp at startup
extern bool ignore_domain_mismatch_when_setting_owner;
inline const char * EffectiveUserName(QmgmtPeer * peer) {
	if (peer) {
		// with JobQueueUserRec we always want to know the full username
		// so regardless of the setting for user_is_the_new_owner, we want
		// return a fully qualified user here
		return peer->getUser();
	}
	return "";
}

#define JQJ_CACHE_DIRTY_JOBOBJ        0x00001 // set when an attribute cached in the JobQueueJob that doesn't have it's own flag has changed
#define JQJ_CACHE_DIRTY_SUBMITTERDATA 0x00002 // set when an attribute that affects the submitter name is changed
#define JQJ_CACHE_DIRTY_CLUSTERATTRS  0x00004 // set then ATTR_EDITED_CLUSTER_ATTRS changes, used only in the cluster ad.

class JobFactory;
class JobQueueCluster;

// structures for a doubly linked list with append-to-tail and remove-from-head semantics (a.k.a a queue)
// so that any element can act as a queue head, an empty queue will have next == prev == this
// and elements not currently attached to any queue should also have next == prev == this.
class qelm {
public:
	qelm() : nxt(this), prv(this) {}
	qelm *next() const { return nxt; }
	qelm *prev() const { return prv; }
	bool empty() const { return nxt==this && prv==this; }
	// append item to tail of this list
	void append_tail(qelm &qe) { qe.nxt = this; qe.prv = prv; qe.prv->nxt = &qe; prv = &qe; }
	// remove and return item from head of this list
	qelm * remove_head() { if (empty()) { return NULL; } else { return nxt->detach(); } }
	// detach from list.
	qelm * detach() { prv->nxt = nxt; nxt->prv = prv; nxt = prv = this; return this; }

	// return a pointer to the class that this qelm is part of.
	template <typename T> T* as() {
		char * p = (char*)this - offsetof(T,qe);
		return reinterpret_cast<T*>(p);
	}
private:
	qelm *nxt;
	qelm *prv;
};

const int USERRECID_qkey1 = 0;
inline int USERRECID_to_qkey2(unsigned int userrec_id) {
	if ((int)userrec_id <= 0) dprintf(D_ALWAYS | D_BACKTRACE, "USERRECID_to_qkey2 called with id=%d", userrec_id);
	ASSERT(userrec_id > 0);
	return (int)(userrec_id);
}

const int JOBSETID_qkey2 = -100;
const int CLUSTERID_qkey2 = -1;
const int CLUSTERPRIVATE_qkey2 = -2;
// jobset ids are id.-100
inline int JOBSETID_to_qkey1(unsigned int jobset_id) {
    if (jobset_id <= 0) dprintf(D_ALWAYS | D_BACKTRACE, "JOBSETID_to_qkey1 called with id=%ud", jobset_id);
	ASSERT(jobset_id > 0);
	return (int)(jobset_id);
}

// map jobqueue key2 to jobset id
inline unsigned int qkey1_to_JOBSETID(int cluster) {
	if (cluster <= 0) dprintf(D_ALWAYS | D_BACKTRACE, "qkey1_to_JOBSETID called with key1=%d", cluster);
	ASSERT(cluster > 0);
	return (unsigned int)(cluster);
}

// used to store a ClassAd + basic information in a condor hashtable.
class JobQueueBase : public ClassAd {
public:
	JOB_ID_KEY jid;
protected:
	char entry_type;    // Job queue entry type: header, cluster or job (i.e. one of entry_type_xxx enum codes, 0 is unknown)	
public:
	JobQueueBase(int _etype=entry_type_unknown)  : jid(), entry_type(_etype) {};
	JobQueueBase(const JOB_ID_KEY & key, int _etype)  : jid(key), entry_type(_etype) {};
	virtual ~JobQueueBase() {};

	virtual void PopulateFromAd(); // populate this structure from contained ClassAd state

	enum {
		entry_type_unknown = 0,
		entry_type_header,
		entry_type_userrec,
		entry_type_project,
		entry_type_jobset,
		entry_type_cluster_private,
		entry_type_cluster,
		entry_type_job,
	};
	static int TypeOfJid(const JOB_ID_KEY &key) {
		if (key.cluster > 0) {
			if (key.proc >= 0) { return entry_type_job; }
			if (key.proc == JOBSETID_qkey2) { return entry_type_jobset; }
			if (key.proc == CLUSTERID_qkey2) { return entry_type_cluster; }
			if (key.proc == CLUSTERPRIVATE_qkey2) { return entry_type_cluster_private; }
		} else if ( ! key.cluster) {
			if ( ! key.proc) { return entry_type_header; }
			else if (key.proc > 0) { return entry_type_userrec; } // note: could be project rec can't tell just from the jid.
		}
		return entry_type_unknown;
	}
	static bool IsJobId(const JOB_ID_KEY &key) { return TypeOfJid(key) == entry_type_job; }
	static bool IsClusterId(const JOB_ID_KEY &key) { return TypeOfJid(key) == entry_type_cluster; }
	static bool IsJobSetId(const JOB_ID_KEY &key) { return TypeOfJid(key) == entry_type_jobset; }
	static bool IsUserRecId(const JOB_ID_KEY &key) { return TypeOfJid(key) == entry_type_userrec; }
	static bool IsClusterPrivateId(const JOB_ID_KEY &key) { return TypeOfJid(key) == entry_type_cluster_private; }

	void CheckJidAndType(const JOB_ID_KEY &key); // called when reloading the job queue
	bool IsType(char _type) const { return entry_type == _type; }
	bool IsJob() const { return IsType(entry_type_job); }
	bool IsHeader() const { return IsType(entry_type_header); }
	bool IsUserRec() const { return IsType(entry_type_userrec); }
	bool IsProject() const { return IsType(entry_type_project); }
	bool IsJobSet() const { return IsType(entry_type_jobset); }
	bool IsCluster() const { return IsType(entry_type_cluster); }
	bool IsClusterPrivate() const { return IsType(entry_type_cluster_private); }

	// write a value from the current record into the job queue log
	// note that this evaluates expressions, so it is only suitable for working with literals
	bool UpdateSecureAttribute(const char * attr);
};

// flag values for JobQueueUserRec
#define JQU_F_DIRTY    0x01   // PopulateFromAd needed 
#define JQU_F_PENDING  0x80   // JobQueueUserRec is in the pendingOwners collection

class JobQueueUserRec : public JobQueueBase {
public:
	struct CountJobsCounters {
		int Hits=0;                 // counts (possibly overcounts) references to this class, used for mark/sweep expiration code
		int JobsCounted=0;          // ALL jobs in the queue owned by this owner at the time count_jobs() was run
		int JobsRecentlyAdded=0;    // ALL jobs owned by this owner that were added since count_jobs() was run
		int JobsIdle=0;             // does not count Local or Scheduler universe jobs, or Grid jobs that are externally managed.
		int JobsRunning=0;
		int JobsHeld=0;
		int LocalJobsIdle=0;
		int LocalJobsRunning=0;
		int LocalJobsHeld=0;
		int SchedulerJobsIdle=0;
		int SchedulerJobsRunning=0;
		int SchedulerJobsHeld=0;
		int OCUJobsRunning = 0;
		void clear_counters() { memset(this, 0, sizeof(*this)); }
	};

	//JOB_ID_KEY jid    - declared in JobQueueBase
	//char entry_type   - declared in JobQueueBase
	unsigned char flags=JQU_F_DIRTY;   // dirty on creation
protected:
	bool        enabled=true;
	bool        local=false;
	bool        os_user_differs=false;
	unsigned char super=JQU_F_PENDING; // config stale on creation
	std::string name;   // the name used in the schedd's map of OwnerInfo records
	std::string os_user; // the os account to use for this user
	// used by the JobQueueProjectRec constructor
	JobQueueUserRec(int userrec_id, char _etype, const char *_name=nullptr)
		: JobQueueBase(JOB_ID_KEY(USERRECID_qkey1,userrec_id), (_etype == entry_type_project) ? entry_type_project : entry_type_userrec)
		, super(false), name(_name?_name:"")
	{}
public:
	CountJobsCounters num; // job counts by OWNER rather than by submitter
	LiveJobCounters live; // job counts that are always up-to-date with the committed job state
	time_t LastHitTime=0; // records the last time we incremented num.Hit, use to expire OwnerInfo

	JobQueueUserRec(int userrec_id, const char* _name=nullptr, const char * _os_user=nullptr, unsigned char is_super=0)
		: JobQueueBase(JOB_ID_KEY(USERRECID_qkey1,userrec_id), entry_type_userrec)
		, super(is_super), name(_name?_name:""), os_user(_os_user?_os_user:"")
	{}
	virtual ~JobQueueUserRec() {};

	virtual void PopulateFromAd(); // populate this structure from contained ClassAd state

	bool empty() const { return name.empty(); }
	const char * Name() const { return name.empty() ? "" : name.c_str(); }
	const char * OsUser() const { return os_user.empty() ? nullptr : os_user.c_str(); }
	bool isDirty() const { return (flags & JQU_F_DIRTY) != 0; }
	void setDirty() { flags |= JQU_F_DIRTY; }
	bool isPending() const { return (flags & JQU_F_PENDING) != 0; }
	void clearPending() { flags &= ~JQU_F_PENDING; }
	void setPending() { flags |= JQU_F_PENDING; }
	bool IsEnabled() const { return enabled; }
	bool IsLocalUser() const { return local; }
	bool OsUserDiffers() const { return os_user_differs; }

	// The super member has 4 possible values
	// super == 0 is not super
	// super == 1 is intrinsic super ( UserRec has SuperUser=true or UserRec is a built-in )
	// super == 2 is cached config super ( name matched QUEUE_SUPER_USERS when we checked )
	// super == JQU_F_PENDING is stale config super ( we need to check name against QUEUE_SUPER_USERS )
	bool IsSuperUser() const { return super == 1 || super == 2; }
	bool setConfigSuper(bool value) {
		// clear stale state and set config super state if not inherently super
		if (super != 1) super = (value?2:0);
		return IsSuperUser();
	}
	bool isStaleConfigSuper() const { return super == JQU_F_PENDING; }
	void setStaleConfigSuper() { if (super != 1) super = JQU_F_PENDING; } // intrinsic super cant be stale
	bool IsInherentlySuper() const { return super == 1; }
	bool IsConfigSuper() const { return super == 2; }

	// add a numeric value to a numeric value in the user record, and optionally
	// write the result to the job queue log
	template <typename T> bool AccumToRecordAndLog(const char * attr, T addval, bool log_set_attr=true);
};

// used to store a Project ClassAd in a condor hashtable.
class JobQueueProjectRec : public JobQueueUserRec {
public:
	JobQueueProjectRec(int userrec_id, const char * _name=nullptr)
		: JobQueueUserRec(userrec_id, entry_type_project, _name)
	{}
	virtual ~JobQueueProjectRec() {};
};

typedef JobQueueUserRec OwnerInfo;
typedef JOB_ID_KEY JobQueueKey;

// Internal set attribute functions for only the schedd to use.
// these functions add attributes to a transaction and also set a transaction trigger
int SetUserAttribute(JobQueueUserRec & urec, const char * attr_name, const char * expr_string);
int SetUserAttributeInt(JobQueueUserRec & urec, const char * attr_name, long long attr_value);
int SetUserAttributeValue(JobQueueUserRec & urec, const char * attr_name, const classad::Value & attr_value);
inline int SetUserAttributeString(JobQueueUserRec & urec, const char * attr_name, const char * attr_value) {
	classad::Value tmp;
	if (attr_value) tmp.SetStringValue(attr_value);
	return SetUserAttributeValue(urec, attr_name, tmp);
}
int DeleteUserAttribute(JobQueueUserRec & urec, const char * attr_name);
// these can be used on UserRec or ProjectRec ads
struct UpdateUserRecAttributesInfo { int valid{0}; int invalid{0}; int special{0}; };
int UpdateUserRecAttributes(JobQueueKey & key, bool is_project, const ClassAd & cmdAd, bool enabled, struct UpdateUserRecAttributesInfo& info);

// get the Effect User record from the peer
// returns NULL if no peer or the peer has not yet had an userrec set.
inline const class JobQueueUserRec * EffectiveUserRec(QmgmtPeer * peer)
{
	if (peer) {
		return peer->UserRec();
	}
	// TODO: return CondorUserRec here?
	return nullptr;
}

class JobQueueClusterPrivate : public JobQueueBase
{
public:

	//JOB_ID_KEY jid    - declared in JobQueueBase
	//char entry_type   - declared in JobQueueBase
	unsigned char flags=JQU_F_DIRTY;   // dirty on creation
public:
	JobQueueClusterPrivate(const JOB_ID_KEY & key) : JobQueueBase(key, entry_type_cluster_private) {}
	virtual ~JobQueueClusterPrivate() {};
	virtual void PopulateFromAd() { flags &= ~JQU_F_DIRTY; }

	bool isDirty() const { return (flags & JQU_F_DIRTY) != 0; }
	void setDirty() { flags |= JQU_F_DIRTY; }
};


// state of JobQueueJob run variable, used along with the prio-rec array to track which jobs
// need matches and which have already been given matches.
// Also used to cache ATTR_JOB_NOOP since that is also checked in the prio-rec
enum class JobRunnableState : char {
	Unset       = 0,
	Runnable    = 1,
	NotRunnable = 2,
	Matched     = 3,
	Cooldown    = 4,
	DirtyNoop   = 5,  // when ATTR_JOB_NOOP has been set, but not yet checked to see if it is true
	Noop        = 6,  // when ATTR_JOB_NOOP is true for the job
};

class JobQueueJob : public JobQueueBase {
public:
	//JOB_ID_KEY jid (defined in JobQueueBase)
protected:
	//char entry_type (defined in JobQueueBase)
	char universe{0};      // this is in sync with ATTR_JOB_UNIVERSE
	char status{0};        // this is in sync with committed job status and used when tracking job counts by state
public:
	JobRunnableState run{JobRunnableState::Unset};
	int dirty_flags{0};	// one or more of JQJ_CHACHE_DIRTY_ flags indicating that the job ad differs from the JobQueueJob 
	int set_id{0};
	int autocluster_id{0};
	// cached pointer into schedulers's SubmitterDataMap and OwnerInfoMap and ProjectInfoMap
	// it is set by count_jobs() or by scheduler::get_submitter_and_owner()
	// DO NOT FREE FROM HERE!
	OwnerInfo * ownerinfo{nullptr};
	JobQueueProjectRec * project{nullptr};
	struct SubmitterData * submitterdata{nullptr};
protected:
	JobQueueCluster * parent{nullptr}; // job pointer back to the cluster ad
	qelm qe;

public:
	JobQueueJob(const JOB_ID_KEY & key)
		: JobQueueBase(key, (key.proc < 0) ? entry_type_cluster : entry_type_job)
		//base class initializes jid to 0,0
	{}
	virtual ~JobQueueJob() {};

	virtual void PopulateFromAd(); // populate this structure from contained ClassAd state

	int  Universe() const { return universe; }
	int  Status() const { return status; }
	unsigned int  Jobset() const { return set_id < 0 ? 0 : set_id; }
	void SetUniverse(int uni) { universe = uni; }
	void SetStatus(int st) { status = st; }
	bool IsNoopJob();
	bool IsOCUClaimer() const; // True if this job holds the ocu claim. It is not a real job
	void DirtyNoopAttr() { run = JobRunnableState::DirtyNoop; }
#if 0
	// FUTURE:
	int NumProcs() { if (entry_type == entry_type_cluster) return future_num_procs_or_hosts; return 0; }
	int SetNumProcs(int num_procs) { if (entry_type == entry_type_cluster) { return future_num_procs_or_hosts = num_procs; } return 0; }
	int NumHosts() { if (entry_type == entry_type_job) return future_num_procs_or_hosts; return 0; }
#endif

	//JobQueueJob( const ClassAd &ad );
	//JobQueueJob( const classad::ClassAd &ad );
	friend class JobQueueCluster;
	friend class qelm;

	// if this object is a job object, it should be linked to its cluster object
	JobQueueCluster * Cluster() { if (entry_type == entry_type_job) return parent; return NULL; }
};

// structure of job_queue hashtable entries for clusters.
class JobQueueCluster : public JobQueueJob {
public:
	JobFactory * factory{nullptr}; // this will be non-null only for cluster ads, and only when the cluster is doing late materialization
protected:
	int cluster_size{0}; // number of materialized jobs in this cluster that the schedd is currently tracking.
	int num_attached{0}; // number of procs attached to this cluster.
	int num_idle{0};
	int num_running{0};
	int num_held{0};

public:
	JobQueueCluster(JOB_ID_KEY & job_id)
		: JobQueueJob(job_id)
		{
		}
	virtual ~JobQueueCluster();

	// NumProcs is a copy of the count of procs in the ClusterSizeHashTable.
	// it will differ from the number of attached jobs when a createproc or destroyproc
	// has been started but the transaction has not yet been committed.
	int ClusterSize() const { return cluster_size; }
	int SetClusterSize(int _cluster_size) { cluster_size = _cluster_size; return cluster_size; }
	int getNumNotRunning() const { return num_idle + num_held; }

	bool HasAttachedJobs() { return ! qe.empty(); }
	void AttachJob(JobQueueJob * job);
	void DetachJob(JobQueueJob * job);
	void DetachAllJobs(); // When you absolutely positively need to free this class...
	JobQueueJob * FirstJob();
	JobQueueJob * NextJob(JobQueueJob * job);
	void JobStatusChanged(int old_status, int new_status);  // update cluster counters by job status.

	void PopulateInfoAd(ClassAd & iad, int num_pending, bool include_factory_info); // fill out an info ad from fields in this structure and from the factory
};

class JobQueueJobSet : public JobQueueBase {
public:
	//inherited from JobQueueBase JOB_ID_KEY jid;
	//inherited from JobQueueBase char entry_type;
	enum class garbagePolicyEnum { immediateAfterEmpty, delayedAferEmpty };

protected:
	// 3 bytes needed to align the next int
	char spareA{0};
	char spareB{0};
	bool dirty{false};
public:
	garbagePolicyEnum garbagePolicy{garbagePolicyEnum::immediateAfterEmpty};
	unsigned int member_count{0};
	unsigned int pending_remove_count{0};
	OwnerInfo * ownerinfo{nullptr};
	LiveJobCounters jobStatusAggregates;
	unsigned int Jobset() const { return (unsigned int)jid.cluster; }

public:
	JobQueueJobSet(unsigned int jobset_id)
		: JobQueueBase(JOB_ID_KEY(jobset_id,JOBSETID_qkey2), entry_type_jobset)
	{
	}
	virtual ~JobQueueJobSet() = default;
	virtual void PopulateFromAd(); // populate this structure from contained ClassAd state

	// add a numeric value to a numeric value in the user record, and optionally
	// write the result to the job queue log
	template <typename T> bool AccumToRecordAndLog(const char * attr, T addval, bool log_set_attr=true);
};

// until we can remove targettype from the classad log entirely
// use the legacy "Machine" value as the target adtype
#define JOB_TARGET_ADTYPE "Machine"

class TransactionWatcher;


// from qmgmt_factory.cpp
// make an empty job factory
class JobFactory * NewJobFactory(int cluster_id, const classad::ClassAd * extended_cmds, MapFile* urlMap);
// make a job factory from submit digest text, used on submit, optional user_ident is who to inpersonate when reading item data file (if any)
bool LoadJobFactoryDigest(JobFactory *factory, const char * submit_digest_text, ClassAd * user_ident, std::string & errmsg);
// attach submitted itemdata to a job factory that is pending submit.
//bool TakeJobFactoryItemdata(JobFactory *factory, void * itemdata, int itemdata_size);
int AppendRowsToJobFactory(JobFactory *factory, char * buf, size_t cbbuf, std::string & remainder);
int JobFactoryRowCount(JobFactory * factory);
// make a job factory from an on-disk submit digest - used on schedd restart
class JobFactory * MakeJobFactory(
	JobQueueCluster * job,
	const classad::ClassAd * extended_cmds,
	const char * submit_file,
	bool spooled_submit_file,
	MapFile* urlMap,
	std::string & errmsg);
void DestroyJobFactory(JobFactory * factory);

void AttachJobFactoryToCluster(JobFactory * factory, JobQueueCluster * cluster);

// returns 1 if a job was materialized, 0 if factory was paused or complete or itemdata is not yet available
// returns < 0 on error.  if return is 0, retry_delay is set to non-zero to indicate the retrying later might yield success
int MaterializeNextFactoryJob(JobFactory * factory, JobQueueCluster * cluster, TransactionWatcher & trans, int & retry_delay);

// returns true if there is no materialize policy expression, or if the expression evalues to true
// returns false if there is an expression and it evaluates to false. When false is returned, retry_delay is set
// a value of > 0 for retry_delay indicates that trying again later might give a different answer.
// num_pending should be the number of jobs that have been materialized but not yet committed
bool CheckMaterializePolicyExpression(JobQueueCluster * cluster, int num_pending, int & retry_delay);

int PostCommitJobFactoryProc(JobQueueCluster * cluster, JobQueueJob * job);
bool CanMaterializeJobs(JobQueueCluster * cluster); // reutrns true if cluster has a non-paused, non-complete factory
bool JobFactoryIsComplete(JobQueueCluster * cluster);
bool JobFactoryIsRunning(JobQueueCluster * cluster);
bool JobFactoryAllowsClusterRemoval(JobQueueCluster * cluster);
int UnMaterializedJobCount(JobQueueCluster * cluster, bool include_paused=false);

// if pause_code < 0, pause is permanent, if >= 3, cluster was removed
typedef enum {
	mmInvalid = -1, // some fatal error occurred, such as failing to load the submit digest.
	mmRunning = 0,
	mmHold = 1,
	mmNoMoreItems = 2,
	mmClusterRemoved = 3
} MaterializeMode;
int PauseJobFactory(JobFactory * factory, MaterializeMode pause_code);
int ResumeJobFactory(JobFactory * factory, MaterializeMode pause_code);
bool CheckJobFactoryPause(JobFactory * factory, int want_pause); // Make sure factory mode matches the persist mode
bool GetJobFactoryMaterializeMode(JobQueueCluster * cluster, int & pause_code);
void PopulateFactoryInfoAd(JobFactory * factory, ClassAd & iad);
bool JobFactoryIsSubmitOnHold(JobFactory * factory, int & hold_code);
void ScheduleClusterForDeferredCleanup(int cluster_id);
void ScheduleClusterForJobMaterializeNow(int cluster_id);

// called by qmgmt_recievers to handle the SetJobFactory RPC call
int QmgmtHandleSetJobFactory(int cluster_id, const char* filename, const char * digest_text);
int QmgmtHandleSendMaterializeData(int cluster_id, ReliSock * sock, std::string & filename, int& row_count, int &terrno);

// called by qmgmt_receivers to handle CONDOR_SendJobQueueAd RPC call
int QmgmtHandleSendJobsetAd(int cluster_id, ClassAd & ad, int flags, int & terrno);

void SetMaxHistoricalLogs(int max_historical_logs);
time_t GetOriginalJobQueueBirthdate();
void DestroyJobQueue( void );
int handle_q(int, Stream *sock);
void dirtyJobQueue( void );
bool SendDirtyJobAdNotification(const PROC_ID& job_id);
bool JobQueueInitDone();

bool isQueueSuperUser(const JobQueueUserRec * user);

// Verify that the user issuing a command (test_owner) is authorized
// to modify the given queue object (job, jobset, userrec, etc).
// In addition to everything UserCheck2()
// does, this also calls IPVerify to check for WRITE authorization.
// This call assumes Q_SOCK is set to a valid QmgmtPeer object.
bool UserCheck(const JobQueueBase *ad, const JobQueueUserRec * test_owner);

// Verify that the user issuing a command (test_owner) is authorized
// to modify the given queue object (job, jobset, userrec, etc).
// when not_super is true, behave as if test_owner is not a superuser even if it is one.
bool UserCheck2(const JobQueueBase *ad, const JobQueueUserRec * test_owner, bool not_super=false);

bool BuildPrioRecArray(bool no_match_found=false);
void DirtyPrioRecArray(int tid=-1);
extern ClassAd *dollarDollarExpand(int cid, int pid, ClassAd *job, ClassAd *res, bool persist_expansions);
bool rewriteSpooledJobAd(ClassAd *job_ad, int cluster, int proc, bool modify_ad);

// The returned expanded ad is a copy of the job classad, and must be deleted
ClassAd* GetExpandedJobAd(const PROC_ID& jid, bool persist_expansions);

#ifdef SCHEDD_INTERNAL_DECLARATIONS
JobQueueJob* GetJobAd(const PROC_ID& jid);
JobQueueJob* GetJobAndInfo(const PROC_ID& jid, int &universe, const OwnerInfo* &powni); // Used by schedd.cpp since JobQueueJob is not a public structure
int GetJobInfo(JobQueueJob *job, const OwnerInfo* &powni); // returns universe and OwnerInfo pointer for job
JobQueueJob* GetJobAd(int cluster, int proc);
JobQueueCluster* GetClusterAd(const PROC_ID& jid);
JobQueueCluster* GetClusterAd(int cluster);
JobQueueJobSet* GetJobSetAd(unsigned int jobset_id);
ClassAd * GetJobAd_as_ClassAd(int cluster_id, int proc_id, bool expStardAttrs = false, bool persist_expansions = true );
ClassAd *GetJobByConstraint_as_ClassAd(const char *constraint);
ClassAd *GetNextJobByConstraint_as_ClassAd(const char *constraint, int initScan);
#define FreeJobAd(ad) ad = NULL

// Inside the sched SetAttribute call takes a 32 bit integer as the flags field
// but only 8 bits are used by the wire protocol.  so anything bigger than 1<<7
// is effectively a SetAttribute flag private to the schedd. we start here at 1<<16
// to leave room to expand the public flags someday (maybe)
typedef unsigned int SetAttributeFlags_t;
const SetAttributeFlags_t SetAttribute_SubmitTransform     = (1 << 16);
const SetAttributeFlags_t SetAttribute_LateMaterialization = (1 << 17);
const SetAttributeFlags_t SetAttribute_Delete              = (1 << 18);

JobQueueJob* GetNextJob(int initScan);
JobQueueJob* GetNextJobByConstraint(const char *constraint, int initScan);
JobQueueJob* GetNextJobOrClusterByConstraint(const char *constraint, int initScan);
JobQueueJob* GetNextDirtyJobByConstraint(const char *constraint, int initScan);

// does the parts of NewProc that are common between external submit and schedd late materialization
int NewProcInternal(int cluster_id, int proc_id);
// call NewProcInternal, and then SetAttribute on all of the attributes in job that are not the same as ClusterAd
int NewProcFromAd (const classad::ClassAd * job, int ProcId, JobQueueCluster * ClusterAd, SetAttributeFlags_t flags);
#endif

void * BeginJobAggregation(const char * projection, bool create_if_not, const char * constraint);
ClassAd *GetNextJobAggregate(void * aggregation, bool first);
void ReleaseAggregationAd(void *aggregation, ClassAd * ad);
void ReleaseAggregation(void *aggregation);


// this class combines a JOB_ID_KEY with a cached string representation of the key
class JOB_ID_KEY_BUF : public JOB_ID_KEY {
public:
	char job_id_str[PROC_ID_STR_BUFLEN];
	JOB_ID_KEY_BUF() { cluster = proc = 0; job_id_str[0] = 0; }
	//operator const char*() { return this->c_str(); }
	const char * c_str() {
		if ( ! job_id_str[0])
			ProcIdToStr(cluster, proc, job_id_str);
		return job_id_str;
	}
	const PROC_ID& id() {
		return *reinterpret_cast<PROC_ID*>(&(this->cluster));
	}
	void set(const char * jid) {
		if ( ! jid || ! jid[0]) {
			cluster =  proc = 0;
			job_id_str[0] = 0;
			return;
		}
		strncpy(job_id_str, jid, sizeof(job_id_str));
		job_id_str[sizeof(job_id_str)-1] = 0;
		StrIsProcId(job_id_str, cluster, proc, NULL);
	}
	void set(int c, int p) {
		cluster = c; proc = p;
		job_id_str[0] = 0; // this will force re-rendering of the string
	}
	void sprint(std::string & s) { s = this->c_str(); }
	JOB_ID_KEY_BUF(const char * jid)          : JOB_ID_KEY(0,0) { set(jid); }
	JOB_ID_KEY_BUF(int c, int p)              : JOB_ID_KEY(c,p) { job_id_str[0] = 0; }
	JOB_ID_KEY_BUF(const JOB_ID_KEY_BUF& rhs) : JOB_ID_KEY(rhs.cluster, rhs.proc) { job_id_str[0] = 0; }
	JOB_ID_KEY_BUF(const JOB_ID_KEY& rhs)     : JOB_ID_KEY(rhs.cluster, rhs.proc) { job_id_str[0] = 0; }
};

typedef JobQueueBase* JobQueuePayload;
// new for 8.3, use a non-string type as the key for the JobQueue
// and a type derived from ClassAd for the payload.
typedef ClassAdLog<JOB_ID_KEY, JobQueuePayload> JobQueueLogType;

// specialize the helper class for create/destroy of hashtable entries for the ClassAdLog class
template <>
class ConstructClassAdLogTableEntry<JobQueuePayload> : public ConstructLogEntry
{
public:
	ConstructClassAdLogTableEntry(class Scheduler * _schedd) : schedd(_schedd) {}
	virtual ClassAd* New(const char * /*key*/, const char * /*mytype*/) const;
	virtual void Delete(ClassAd* &val) const;
	class Scheduler* schedd = nullptr;
};

// specialize the helper class for used by ClassAdLog transactional insert/remove functions
template <>
class ClassAdLogTable<JOB_ID_KEY,JobQueuePayload> : public LoggableClassAdTable {
public:
	ClassAdLogTable(HashTable<JOB_ID_KEY,JobQueuePayload> & _table) : table(_table) {}
	virtual ~ClassAdLogTable() {};
	virtual bool lookup(const char * key, ClassAd*& ad) {
		JOB_ID_KEY k(key);
		JobQueuePayload Ad=nullptr;
		int iret = table.lookup(k, Ad);
		ad=Ad;
		return iret >= 0;
	}
	virtual bool remove(const char * key) {
		JOB_ID_KEY k(key);
		return table.remove(k) >= 0;
	}
	virtual bool insert(const char * key, ClassAd * ad) {
		JOB_ID_KEY k(key);
		JobQueuePayload payload = dynamic_cast<JobQueuePayload>(ad);
		ASSERT(payload);
		return table.insert(k, payload) >= 0;
	}
	virtual void startIterations() { table.startIterations(); } // begin iterations
	virtual bool nextIteration(const char*& key, ClassAd*&ad) {
		current_key.set(NULL); // make sure to clear out the string value for the current key
		JobQueuePayload Ad=NULL;
		int iret = table.iterate(current_key, Ad);
		if (iret != 1) {
			key = NULL;
			ad = NULL;
			return false;
		}
		key = current_key.c_str();
		ad = Ad;
		return true;
	}
protected:
	HashTable<JOB_ID_KEY,JobQueuePayload> & table;
	JOB_ID_KEY_BUF current_key; // used during iteration, so we can return a const char *
};

class TransactionWatcher {
public:
	TransactionWatcher() : firstid(-1), lastid(-1), started(false), completed(false) {}
	~TransactionWatcher() { AbortIfAny(); }

	// don't allow copy or assigment of this class
	TransactionWatcher(const TransactionWatcher&) = delete;
	TransactionWatcher& operator=(const TransactionWatcher) = delete;

	bool InTransaction() const { return started && ! completed; }

	// start a transaction, or continue one if we already started it
	int BeginOrContinue(int id);

	// commit if we started a transaction
	int CommitIfAny(SetAttributeFlags_t flags, CondorError * errorStack);

	// cancel if we started a transaction
	int AbortIfAny();

	// return the range of ids passed to BeginOrContinue
	// returns true if ids were set.
	bool GetIdRange(int & first, int & last) const {
		if (! started) return false;
		first = firstid;
		last = lastid;
		return true;
	}

private:
	int firstid;
	int lastid;
	bool started;
	bool completed;
};


#define JOB_QUEUE_ITERATOR_OPT_INCLUDE_CLUSTERS     0x0001
#define JOB_QUEUE_ITERATOR_OPT_INCLUDE_JOBSETS      0x0002
#define JOB_QUEUE_ITERATOR_OPT_NO_PROC_ADS          0x0004
JobQueueLogType::filter_iterator GetJobQueueIterator(const classad::ExprTree &requirements, int timeslice_ms);
JobQueueLogType::filter_iterator GetJobQueueIteratorEnd();


class schedd_runtime_probe;
#define WJQ_WITH_CLUSTERS 0x01  // include cluster ads when walking the job queue
#define WJQ_WITH_JOBSETS  0x02  // include jobset ads when walking the job queue
#define WJQ_WITH_NO_JOBS  0x04  // do not include job (proc) ads when walking the job queue
#define WJQ_WITH_USERS    0x10  // include user records
#define WJQ_WITH_PROJECTS 0x20  // include project records
typedef int (*queue_scan_func)(JobQueuePayload ad, const JobQueueKey& key, void* user);
void WalkJobQueueEntries(int with, queue_scan_func fn, void* pv, schedd_runtime_probe & ftm);
#define WalkJobQueueWith(with,fn,pv) WalkJobQueueEntries(with, (fn), pv, WalkJobQ_ ## fn ## _runtime )
typedef int (*queue_job_scan_func)(JobQueueJob *ad, const JobQueueKey& key, void* user);
void WalkJobQueue3(queue_job_scan_func fn, void* pv, schedd_runtime_probe & ftm);
#define WalkJobQueue(fn) WalkJobQueue3((fn), NULL, WalkJobQ_ ## fn ## _runtime )
#define WalkJobQueue2(fn,pv) WalkJobQueue3((fn), (pv), WalkJobQ_ ## fn ## _runtime )

bool InWalkJobQueue();

// A negative JobsSeenOnQueueWalk means the last job queue walk terminated
// early, so no reliable count is available.
extern int TotalJobsCount;
extern int JobsSeenOnQueueWalk;

void InitQmgmt();
void InitJobQueue(const char *job_queue_name,int max_historical_logs);
void PostInitJobQueue();
void CleanJobQueue(int tid = -1);
bool setQSock( ReliSock* rsock );
void unsetQSock();
void MarkJobClean(PROC_ID job_id);
void MarkJobClean(int cluster_id, int proc_id);
void MarkJobClean(const char* job_id_str);

bool Reschedule();

bool UniverseUsesVanillaStartExpr(int universe);

QmgmtPeer* getQmgmtConnectionInfo();

// JobSet qmgmt support functions
bool JobSetDestroy(int setid);
bool JobSetCreate(int setId, const char * setName, const char * ownerinfoName);

bool UserRecDestroy(int userrec_id);
bool UserRecCreate(int userrec_id, bool is_project, const char * name, const ClassAd & cmdAd, bool enabled);
void UserRecFixupDefaultsAd(ClassAd & defaultsAd);

// priority records
//#define PRIO_REC_IS_VECTOR 1
#ifdef PRIO_REC_IS_VECTOR
  extern std::vector<prio_rec> PrioRec;
#else
  extern std::deque<prio_rec> PrioRec;
#endif

extern void	FindRunnableJob(PROC_ID & jobid, ClassAd* my_match_ad, char const * user);
//extern bool Runnable(PROC_ID*);
enum class runnable_reason_code : int {
	IsRunnable=0,
	NotFound,
	IsNoopJob,
	NotIdle,
	UniverseNotInService,
	IsOCUClaimer, // job is an OCU claimer, not a real job
	InLongCooldown,  // cooldown > 5min
	InShortCooldown, // cooldown <= 5min
	AlreadyMatched,
};
//extern bool Runnable(JobQueueJob *job, const char *& reason);
extern bool Runnable(JobQueueJob *job, runnable_reason_code & code);
extern const char * getRunnableReason(runnable_reason_code code);


extern class ForkWork schedd_forker;

int SetPrivateAttributeString(int cluster_id, int proc_id, const char *attr_name, const char *attr_value);
int GetPrivateAttributeString(int cluster_id, int proc_id, const char *attr_name, std::string &attr_value);
int DeletePrivateAttribute(int cluster_id, int proc_id, const char *attr_name);

#endif /* _QMGMT_H */
