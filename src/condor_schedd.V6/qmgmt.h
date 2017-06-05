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

// this header declares functions internal to the schedd, it should not be included by code external to the schedd
// and it should always be included before condor_qmgr.h so that it can disable external prototype declaraions in that file. 
#define SCHEDD_INTERNAL_DECLARATIONS
#ifdef SCHEDD_EXTERNAL_DECLARATIONS
#error This header must be included before condor_qmgr.h for code internal to the SCHEDD, and not at all for external code
#endif

void PrintQ();
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

		bool setEffectiveOwner(char const *o);
		bool setAllowProtectedAttrChanges(bool val);
		bool getAllowProtectedAttrChanges() { return allow_protected_attr_changes_by_superuser; }

		ReliSock *getReliSock() const { return sock; };
		const CondorVersionInfo *get_peer_version() const { return sock->get_peer_version(); };

		const char *endpoint_ip_str() const;
		const condor_sockaddr endpoint() const;
		const char* getOwner() const;
		const char* getRealOwner() const;
		const char* getFullyQualifiedUser() const;
		int isAuthenticated() const;

	protected:

		char *owner;  
		bool allow_protected_attr_changes_by_superuser;
		char *fquser;  // owner@domain
		char *myendpoint; 
		condor_sockaddr addr;
		ReliSock *sock; 

		Transaction *transaction;
		int next_proc_num, active_cluster_num;
		time_t xact_start_time;

	private:
		// we do not allow deep-copies via copy ctor or assignment op,
		// so disable there here by making them private.
		QmgmtPeer(QmgmtPeer&);	
		QmgmtPeer & operator=(const QmgmtPeer & rhs);
};


#define USE_JOB_QUEUE_JOB 1 // contents of the JobQueue is a class *derived* from ClassAd, (new for 8.3)

#define JQJ_CACHE_DIRTY_JOBOBJ        0x00001 // set when an attribute cached in the JobQueueJob that doesn't have it's own flag has changed
#define JQJ_CACHE_DIRTY_SUBMITTERDATA 0x00002 // set when an attribute that affects the submitter name is changed

class JobFactory;

// used to store a ClassAd + runtime information in a condor hashtable.
class JobQueueJob : public ClassAd {
public:
	JOB_ID_KEY jid;
protected:
	char entry_type;    // Job queue entry type: header, cluster or job (i.e. one of entry_type_xxx enum codes, 0 is unknown)
	char universe;      // this is in sync with ATTR_JOB_UNIVERSE
	char has_noop_attr; // 1 if job has ATTR_JOB_NOOP
	char status;        // this is in sync with committed job status and used when tracking job counts by state
public:
	int dirty_flags;	// one or more of JQJ_CHACHE_DIRTY_ flags indicating that the job ad differs from the JobQueueJob 
	int spare;
	int autocluster_id;
	// cached pointer into schedulers's SubmitterDataMap and OwnerInfoMap
	// it is set by count_jobs() or by scheduler::get_submitter_and_owner()
	// DO NOT FREE FROM HERE!
	struct SubmitterData * submitterdata;
	struct OwnerInfo * ownerinfo;
	JobFactory * factory; // this will be non-null only for cluster ads, and only when cluster is doing late materialization
protected:
	JobQueueJob * link; // FUTURE: jobs are linked to clusters.
	int future_num_procs_or_hosts; // FUTURE: num_procs if cluster, num hosts if job

public:
	JobQueueJob(int _etype=0)
		: jid(0,0)
		, entry_type(_etype)
		, universe(0)
		, has_noop_attr(2) // value of 2 forces PopulateFromAd() to check if it exists.
		, status(0) // JOB_STATUS_MIN
		, dirty_flags(0)
		, autocluster_id(0)
		, submitterdata(NULL)
		, ownerinfo(NULL)
		, factory(NULL)
		, link(NULL)
		, future_num_procs_or_hosts(0)
	{}
	virtual ~JobQueueJob();

	enum {
		entry_type_unknown=0,
		entry_type_header,
		entry_type_cluster,
		entry_type_job,
	};
	bool IsType(char _type) { if ( ! entry_type) this->PopulateFromAd(); return entry_type==_type; }
	bool IsJob() { return IsType(entry_type_job); }
	bool IsHeader() { return IsType(entry_type_header); }
	bool IsCluster() { return IsType(entry_type_cluster); }
	int  Universe() { return universe; }
	int  Status() { return status; }
	void SetUniverse(int uni) { universe = uni; }
	void SetStatus(int st) { status = st; }
	bool IsNoopJob();
	// FUTURE:
	int NumProcs() { if (entry_type == entry_type_cluster) return future_num_procs_or_hosts; return 0; }
	int SetNumProcs(int num_procs) { if (entry_type == entry_type_cluster) { return future_num_procs_or_hosts = num_procs; } return 0; }
	int NumHosts() { if (entry_type == entry_type_job) return future_num_procs_or_hosts; return 0; }

	//JobQueueJob( const ClassAd &ad );
	//JobQueueJob( const classad::ClassAd &ad );

	void PopulateFromAd(); // populate this structure from contained ClassAd state
	// if this object is a job object, it can be linked to it's cluster object
	JobQueueJob * Cluster() { if (entry_type == entry_type_job) return link; return NULL; }
	void SetCluster(JobQueueJob* _link) {
		if (entry_type == entry_type_unknown) entry_type = entry_type_job;
		if (entry_type == entry_type_job) link = _link;
	}
};


// from qmgmt_factory.cpp
class JobFactory * MakeJobFactory(JobQueueJob * job, const char * submit_file);
void DestroyJobFactory(JobFactory * factory);
int MaterializeNextFactoryJob(JobFactory * factory, JobQueueJob * cluster);
int PostCommitJobFactoryProc(JobQueueJob * cluster, JobQueueJob * job);
bool JobFactoryAllowsClusterRemoval(JobQueueJob * cluster);
// if pause_code < 0, pause is permanent, if >= 3, cluster was removed
int PauseJobFactory(JobFactory * factory, int pause_code);
int ResumeJobFactory(JobFactory * factory, int pause_code);
bool CheckJobFactoryPause(JobFactory * factory, int pause_code); // 0 for resume, 1 for pause, returns true if state changed
void ScheduleClusterForDeferredCleanup(int cluster_id);

void SetMaxHistoricalLogs(int max_historical_logs);
time_t GetOriginalJobQueueBirthdate();
void DestroyJobQueue( void );
int handle_q(Service *, int, Stream *sock);
void dirtyJobQueue( void );
bool SendDirtyJobAdNotification(const PROC_ID& job_id);

bool isQueueSuperUser( const char* user );

// Verify that the user issuing a command (test_owner) is authorized
// to modify the given job.  In addition to everything OwnerCheck2()
// does, this also calls IPVerify to check for WRITE authorization.
bool OwnerCheck( ClassAd *ad, const char *test_owner );

// Verify that the user issuing a command (test_owner) is authorized
// to modify the given job.  Either ad or job_owner should be given
// but not both.  If job_owner is NULL, the owner is looked up in the ad.
bool OwnerCheck2( ClassAd *ad, const char *test_owner, char const *job_owner=NULL );

bool BuildPrioRecArray(bool no_match_found=false);
void DirtyPrioRecArray();
extern ClassAd *dollarDollarExpand(int cid, int pid, ClassAd *job, ClassAd *res, bool persist_expansions);
bool rewriteSpooledJobAd(ClassAd *job_ad, int cluster, int proc, bool modify_ad);

// The returned expanded ad is a copy of the job classad, and must be deleted
ClassAd* GetExpandedJobAd(const PROC_ID& jid, bool persist_expansions);

#ifdef SCHEDD_INTERNAL_DECLARATIONS
JobQueueJob* GetJobAd(const PROC_ID& jid);
JobQueueJob* GetJobAd(int cluster, int proc);
ClassAd * GetJobAd_as_ClassAd(int cluster_id, int proc_id, bool expStardAttrs = false, bool persist_expansions = true );
ClassAd *GetJobByConstraint_as_ClassAd(const char *constraint);
ClassAd *GetNextJobByConstraint_as_ClassAd(const char *constraint, int initScan);
#define FreeJobAd(ad) ad = NULL

JobQueueJob* GetNextJob(int initScan);
JobQueueJob* GetNextJobByCluster( int, int );
JobQueueJob* GetNextJobByConstraint(const char *constraint, int initScan);
JobQueueJob* GetNextJobOrClusterByConstraint(const char *constraint, int initScan);
JobQueueJob* GetNextDirtyJobByConstraint(const char *constraint, int initScan);

// does the parts of NewProc that are common between external submit and schedd late materialization
int NewProcInternal(int cluster_id, int proc_id);
// call NewProcInternal, and then SetAttribute on all of the attributes in job that are not the same as ClusterAd
typedef unsigned char SetAttributeFlags_t;
int NewProcFromAd (const classad::ClassAd * job, int ProcId, JobQueueJob * ClusterAd, SetAttributeFlags_t flags);
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
	void sprint(MyString & s) { s = this->c_str(); }
	JOB_ID_KEY_BUF(const char * jid)          : JOB_ID_KEY(0,0) { set(jid); }
	JOB_ID_KEY_BUF(int c, int p)              : JOB_ID_KEY(c,p) { job_id_str[0] = 0; }
	JOB_ID_KEY_BUF(const JOB_ID_KEY_BUF& rhs) : JOB_ID_KEY(rhs.cluster, rhs.proc) { job_id_str[0] = 0; }
	JOB_ID_KEY_BUF(const JOB_ID_KEY& rhs)     : JOB_ID_KEY(rhs.cluster, rhs.proc) { job_id_str[0] = 0; }
};


// specialize the helper class for create/destroy of hashtable entries for the ClassAdLog class
template <>
class ConstructClassAdLogTableEntry<JobQueueJob*> : public ConstructLogEntry
{
public:
	virtual ClassAd* New(const char * /*key*/, const char * /*mytype*/) const { return new JobQueueJob(); }
	virtual void Delete(ClassAd* &val) const;
};

// specialize the helper class for used by ClassAdLog transactional insert/remove functions
template <>
class ClassAdLogTable<JOB_ID_KEY,JobQueueJob*> : public LoggableClassAdTable {
public:
	ClassAdLogTable(HashTable<JOB_ID_KEY,JobQueueJob*> & _table) : table(_table) {}
	virtual ~ClassAdLogTable() {};
	virtual bool lookup(const char * key, ClassAd*& ad) {
		JobQueueJob* Ad=NULL;
		JOB_ID_KEY k(key);
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
		JobQueueJob* Ad = new JobQueueJob();  // TODO: find out of we can count on ad already being a JobQueueJob*
		Ad->Update(*ad); delete ad; // TODO: transfer ownership of attributes from ad to Ad? I think this ad is always nearly empty.
		Ad->SetDirtyTracking(true);
		int iret = table.insert(k, Ad);
		return iret >= 0;
	}
	virtual void startIterations() { table.startIterations(); } // begin iterations
	virtual bool nextIteration(const char*& key, ClassAd*&ad) {
		JobQueueJob* Ad=NULL;
		current_key.set(NULL); // make sure to clear out the string value for the current key
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
	HashTable<JOB_ID_KEY,JobQueueJob*> & table;
	JOB_ID_KEY_BUF current_key; // used during iteration, so we can return a const char *
};

// new for 8.3, use a non-string type as the key for the JobQueue
// and a type derived from ClassAd for the payload.
typedef JOB_ID_KEY JobQueueKey;
typedef JobQueueJob* JobQueuePayload;
typedef ClassAdLog<JOB_ID_KEY, const char*,JobQueueJob*> JobQueueLogType;

#define JOB_QUEUE_ITERATOR_OPT_INCLUDE_CLUSTERS     0x0001
JobQueueLogType::filter_iterator GetJobQueueIterator(const classad::ExprTree &requirements, int timeslice_ms);
JobQueueLogType::filter_iterator GetJobQueueIteratorEnd();


class schedd_runtime_probe;
typedef int (*queue_classad_scan_func)(ClassAd *ad, void* user);
void WalkJobQueue3(queue_classad_scan_func fn, void* pv, schedd_runtime_probe & ftm);
typedef int (*queue_job_scan_func)(JobQueueJob *ad, const JobQueueKey& key, void* user);
void WalkJobQueue3(queue_job_scan_func fn, void* pv, schedd_runtime_probe & ftm);
#define WalkJobQueue(fn) WalkJobQueue3( (fn), NULL, WalkJobQ_ ## fn ## _runtime )
#define WalkJobQueue2(fn,pv) WalkJobQueue3( (fn), (pv), WalkJobQ_ ## fn ## _runtime )

bool InWalkJobQueue();

void InitQmgmt();
void InitJobQueue(const char *job_queue_name,int max_historical_logs);
void PostInitJobQueue();
void CleanJobQueue();
bool setQSock( ReliSock* rsock );
void unsetQSock();
void MarkJobClean(PROC_ID job_id);
void MarkJobClean(int cluster_id, int proc_id);
void MarkJobClean(const char* job_id_str);

bool Reschedule();

int get_myproxy_password_handler(Service *, int, Stream *sock);

QmgmtPeer* getQmgmtConnectionInfo();
bool OwnerCheck(int,int);


// priority records
extern prio_rec *PrioRec;
extern int N_PrioRecs;
extern HashTable<int,int> *PrioRecAutoClusterRejected;
extern int grow_prio_recs(int);

extern void	FindRunnableJob(PROC_ID & jobid, ClassAd* my_match_ad, char const * user);
extern int Runnable(PROC_ID*);
extern int Runnable(ClassAd*);

extern class ForkWork schedd_forker;

#endif /* _QMGMT_H */
