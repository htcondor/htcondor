/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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


#include "condor_common.h"
#include "condor_io.h"
#include "string_list.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"

#include "basename.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "classad_collection.h"
#include "prio_rec.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "spooled_job_files.h"
#include "scheduler.h"	// for shadow_rec definition
#include "dedicated_scheduler.h"
#include "condor_email.h"
#include "condor_universe.h"
#include "globus_utils.h"
#include "env.h"
#include "condor_classad.h"
#include "condor_ver_info.h"
#include "forkwork.h"
#include "condor_open.h"
#include "ickpt_share.h"
#include "classadHistory.h"
#include "directory.h"
#include "filename_tools.h"
#include "spool_version.h"
#include "condor_holdcodes.h"
#include "nullfile.h"
#include "condor_url.h"
#include "classad_helpers.h"
#include "iso_dates.h"
#include "jobsets.h"
#include "exit.h"
#include "credmon_interface.h"
#include <algorithm>
#include <math.h>
#include <param_info.h>
#include <shortfile.h>

#include "ScheddPlugin.h"

#ifdef UNIX
#include <sys/types.h>
#include <grp.h>
#endif

#define USE_MATERIALIZE_POLICY

// Do filtered iteration in a way that is specific to the job queue
//
template <typename K, typename AD>
typename ClassAdLog<K,AD>::filter_iterator
ClassAdLog<K,AD>::filter_iterator::operator++(int)
{
	m_found_ad = false;
	typename ClassAdLog<K,AD>::filter_iterator cur = *this;
	if (m_done) {
		return cur;
	}

	HashIterator<K, AD> end = m_table->end();
	bool boolVal = false;
	int miss_count = 0;
	Stopwatch sw;
	while (!(m_cur == end))
	{
		cur = *this;
		//const K & tmp_key = (*m_cur).first;
		AD tmp_ad = (*m_cur++).second;
		if (!tmp_ad) continue;

		//dprintf(D_COMMAND | D_VERBOSE, "ClassAdLog::filter_iterator++ 0x%x key=%d.%d (%d.%d)\n", 
		//	m_options, tmp_key.cluster, tmp_key.proc, tmp_ad->jid.cluster, tmp_ad->jid.proc);

		// we want to ignore all but job ads, unless the options flag indicates we should
		// also iterate this type of ad, in any case we always want to skip the header ad.
		if (tmp_ad->IsJob()) {
			if (m_options & JOB_QUEUE_ITERATOR_OPT_NO_PROC_ADS) {
				continue; // don't iterate this one
			}
		} else {
			if (tmp_ad->IsCluster() && (m_options & JOB_QUEUE_ITERATOR_OPT_INCLUDE_CLUSTERS)) {
				// iterate this one
			} else if (tmp_ad->IsJobSet() && (m_options & JOB_QUEUE_ITERATOR_OPT_INCLUDE_JOBSETS)) {
				// iterate this one
			} else {
				continue; // not this one
			}
		}

		if (m_requirements) {
			//IsDebugCatAndVerbosity(D_COMMAND | D_VERBOSE) {
			//  dprintf(D_COMMAND | D_VERBOSE, "ClassAdLog::filter_iterator++ checking requirements: %s\n", ExprTreeToString(&requirements));
			//}
			classad::Value result;
		#if 1
			bool eval_ok = classad::ClassAd::EvaluateExpr(tmp_ad, m_requirements, result, classad::Value::ValueType::NUMBER_VALUES);
		#else
			classad::ExprTree &requirements = *const_cast<classad::ExprTree*>(m_requirements);
			const classad::ClassAd *old_scope = requirements.GetParentScope();
			requirements.SetParentScope( tmp_ad );
			bool eval_ok = tmp_ad->EvaluateExpr(requirements, result, classad::Value::ValueType::NUMBER_VALUES);
			requirements.SetParentScope(old_scope);
		#endif
			eval_ok = eval_ok && result.IsBooleanValueEquiv(boolVal);
			if ( ! eval_ok || ! boolVal) {

				if ( ! eval_ok && IsDebugCatAndVerbosity(D_COMMAND | D_VERBOSE)) {
					dprintf(D_COMMAND | D_VERBOSE, "requirements did not evaluate to bool equivalent: %s\n", ClassAdValueToString(result));
				}

				// 500 (ish) was chosen here based on a queue of 1M jobs and
				// estimated 30ns per clock_gettime call - resulting in
				// an overhead of 0.06 ms from the timing calls to iterate
				// through a whole queue.  Compared to the cost of doing
				// the rest of the iteration (6ms per 10k ads, or 600ms)
				// for the whole queue, I consider this overhead
				// acceptable.  BB, 09/2014.
				if ( ! miss_count) { sw.start(); }
				++miss_count;
				if ((miss_count & 0x1FF) == 0 && (sw.get_ms() > m_timeslice_ms)) {break;}

				// keep looking
				continue;
			}
		}
		cur.m_found_ad = true;
		m_found_ad = true;
		break;
	}
	if ((m_cur == end) && (!m_found_ad)) {
		m_done = true;
	}
	return cur;
}

extern Scheduler scheduler;

// force instantiation of the template types needed by the JobQueue
#ifdef JOB_QUEUE_PAYLOAD_IS_BASE
template <typename K, typename AD>
class JobQueueCollection : public GenericClassAdCollection<K, AD>
{
public:
	JobQueueCollection()
		: GenericClassAdCollection<K, AD>(new ConstructClassAdLogTableEntry<JobQueuePayload>(&scheduler))
		{}

	bool Lookup(const JobQueueKey & k, JobQueuePayload & ad) {
		return GenericClassAdCollection<K, AD>::Lookup(k, ad);
	}

	bool Lookup(const JobQueueKey & k, JobQueueJob* & job) {
		job = nullptr;
		AD b = nullptr;
		if (Lookup(k, b) && k.cluster > 0 && k.proc >= -1) {
			job = static_cast<JobQueueJob*>(b);
			return true;
		}
		return false;
	}
};
typedef JobQueueCollection<JobQueueKey, JobQueuePayload> JobQueueType;
#else
typedef GenericClassAdCollection<JobQueueKey, JobQueuePayload> JobQueueType;
#endif
template class ClassAdLog<JobQueueKey,JobQueuePayload>;


extern char *Spool;
extern char *Name;
extern Scheduler scheduler;
extern DedicatedScheduler dedicated_scheduler;

extern  void    cleanup_ckpt_files(int, int, const char*);
extern	bool	service_this_universe(int, ClassAd *);
static QmgmtPeer *Q_SOCK = nullptr;
extern const std::string & attr_JobUser; // the attribute name we use for the "owner" of the job, historically ATTR_OWNER 
extern JobQueueUserRec * get_condor_userrec();
extern JobQueueUserRec * real_owner_is_condor(const Sock * sock);
JobQueueUserRec * real_owner_is_condor(QmgmtPeer * qsock) {
	return real_owner_is_condor(qsock->getReliSock());
}

extern bool ignore_domain_for_OwnerCheck;
extern bool warn_domain_for_OwnerCheck;
extern bool job_owner_must_be_UidDomain; // only users who are @$(UID_DOMAIN) may submit.
extern bool allow_submit_from_known_users_only; // if false, create UseRec for new users when they submit

// Hash table with an entry for every job owner that
// has existed in the queue since this schedd has been
// running.  Used by SuperUserAllowedToSetOwnerTo().
static HashTable<std::string,int> owner_history(hashFunction);

int		do_Q_request(QmgmtPeer &);
#if 0 // not used?
void	FindPrioJob(PROC_ID &);
#endif
void	DoSetAttributeCallbacks(const std::set<std::string> &jobids, int triggers);
int		MaterializeJobs(JobQueueCluster * clusterAd, TransactionWatcher & txn, int & retry_delay);

static bool qmgmt_was_initialized = false;
static JobQueueType *JobQueue = nullptr;
static std::set<JOB_ID_KEY> DirtyJobIDs;
static std::set<JOB_ID_KEY>::iterator DirtyJobIDsItr = DirtyJobIDs.begin();
static std::set<int> DirtyPidsSignaled;
static int next_cluster_num = -1;
// If we ever allow more than one concurrent transaction, and next_proc_num
// being a global, we'll need to make the same change to
// jobs_added_this_transaction.
static int next_proc_num = 0;
static int jobs_added_this_transaction = 0;
int active_cluster_num = -1;	// client is restricted to only insert jobs to the active cluster
static bool JobQueueDirty = false;
static bool in_DestroyJobQueue = false;
static int in_walk_job_queue = 0;
static time_t xact_start_time = 0;	// time at which the current transaction was started
static int cluster_initial_val = 1;		// first cluster number to use
static int cluster_increment_val = 1;	// increment for cluster numbers of successive submissions 
static int cluster_maximum_val = 0;     // maximum cluster id (default is 0, or 'no max')
static int job_queued_count = 0;
static Regex *queue_super_user_may_impersonate_regex = nullptr;

static void AddOwnerHistory(const std::string &user);

typedef _condor_auto_accum_runtime< stats_entry_probe<double> > condor_auto_runtime;

schedd_runtime_probe WalkJobQ_runtime;
schedd_runtime_probe WalkJobQ_mark_idle_runtime;
schedd_runtime_probe WalkJobQ_get_job_prio_runtime;

class Service;

bool        PrioRecArrayIsDirty = true;
// spend at most this fraction of the time rebuilding the PrioRecArray
const double PrioRecRebuildMaxTimeSlice = 0.05;
const double PrioRecRebuildMaxTimeSliceWhenNoMatchFound = 0.1;
const double PrioRecRebuildMaxInterval = 20 * 60;
Timeslice   PrioRecArrayTimeslice;
prio_rec	PrioRecArray[INITIAL_MAX_PRIO_REC];
prio_rec	* PrioRec = &PrioRecArray[0];
int			N_PrioRecs = 0;
HashTable<int,int> *PrioRecAutoClusterRejected = nullptr;
int BuildPrioRecArrayTid = -1;

static int 	MAX_PRIO_REC=INITIAL_MAX_PRIO_REC ;	// INITIAL_MAX_* in prio_rec.h

JOB_ID_KEY_BUF HeaderKey(0,0);

ForkWork schedd_forker;

// A negative JobsSeenOnQueueWalk means the last job queue walk terminated
// early, so no reliable count is available.
int TotalJobsCount = 0;
int JobsSeenOnQueueWalk = -1;

// Create a hash table which, given a cluster id, tells how
// many procs are in the cluster
typedef HashTable<int, int> ClusterSizeHashTable_t;
static ClusterSizeHashTable_t *ClusterSizeHashTable = nullptr;
static std::set<int> ClustersNeedingCleanup;
static int defer_cleanup_timer_id = -1;
static std::set<int> ClustersNeedingMaterialize;
static std::map<int,JobFactory*> JobFactoriesSubmitPending; // job factories that have been submitted, but not yet committed.
static int job_materialize_timer_id = -1;

// if false, we version check and fail attempts by newer clients to set secure attrs via the SetAttribute function
static bool Ignore_Secure_SetAttr_Attempts = true;

static classad::References immutable_attrs, protected_attrs, secure_attrs;
static int flush_job_queue_log_timer_id = -1;
static int dirty_notice_timer_id = -1;
static int flush_job_queue_log_delay = 0;
static void HandleFlushJobQueueLogTimer(int tid);
static int dirty_notice_interval = 0;
static void PeriodicDirtyAttributeNotification(int tid);
static void ScheduleJobQueueLogFlush();

bool qmgmt_all_users_trusted = false;
static std::vector<std::string> super_users;
static const char *default_super_user =
#if defined(WIN32)
	"Administrator";
#else
	"root";
#endif

std::map<JobQueueKey, std::map<std::string, std::string>> PrivateAttrs;


// a struct with no data and a functor for std::sort
// This maximizes the likelyhood the compiler will inline
// the comparison
struct prio_compar {
bool operator()(const prio_rec& a, const prio_rec& b) const 
{
	// First sort by owner name.  This doesn't need to be alphabetical,
	// just unique.  Sort first by length, as that's faster,
	if (a.submitter.length() < b.submitter.length()) {
		return false;
	}

	if (a.submitter.length() > b.submitter.length()) {
		return true;
	}

	if (a.submitter < b.submitter) {
		return false;
	}

	if (a.submitter > b.submitter) {
		return true;
	}

	// If we get here, the submitters are the same, so sort by priorities
	
	 /* compare submitted job preprio's: higher values have more priority */
	 /* Typically used to prioritize entire DAG jobs over other DAG jobs */
	if( a.pre_job_prio1 < b.pre_job_prio1 ) {
		return false;
	}
	if( a.pre_job_prio1 > b.pre_job_prio1 ) {
		return true;
	}

	if( a.pre_job_prio2 < b.pre_job_prio2 ) {
		return false;
	}
	if( a.pre_job_prio2 > b.pre_job_prio2 ) {
		return true;
	}
	 
	 /* compare job priorities: higher values have more priority */
	 if( a.job_prio < b.job_prio ) {
		  return false;
	 }
	 if( a.job_prio > b.job_prio ) {
		  return true;
	 }
	 
	 /* compare submitted job postprio's: higher values have more priority */
	 /* Typically used to prioritize entire DAG jobs over other DAG jobs */
	if( a.post_job_prio1 < b.post_job_prio1 ) {
		return false;
	}
	if( a.post_job_prio1 > b.post_job_prio1 ) {
		return true;
	}

	if( a.post_job_prio2 < b.post_job_prio2 ) {
		return false;
	}
	if( a.post_job_prio2 > b.post_job_prio2 ) {
		return true;
	}

	 /* here, all job_prios are both equal */

	 /* go in order of cluster id */
	if ( a.id.cluster < b.id.cluster )
		return true;
	if ( a.id.cluster > b.id.cluster )
		return false;

	/* finally, go in order of the proc id */
	if ( a.id.proc < b.id.proc )
		return true;
	if ( a.id.proc > b.id.proc )
		return false;

	/* give up! very unlikely we'd ever get here */
	return false;
}
};

// Return the same comparison as above, but just comparing the submitter
// field.  This allows us to binary search the PrioRecArray by submitter
// Note the comparison up to the submitter must match the above
struct prio_rec_submitter_lb {
bool operator()(const prio_rec& a, const std::string &user) const {
	if (a.submitter.length() < user.length()) {
		return false;
	}

	if (a.submitter.length() > user.length()) {
		return true;
	}

	if (a.submitter > user) {
		return true;
	}

	return false;
}
};

// The corresponding upper bound function, the negation of the above
struct prio_rec_submitter_ub {
bool operator()(const std::string &user, const prio_rec &a) const {
	if (a.submitter.length() < user.length()) {
		return true;
	}

	if (a.submitter.length() > user.length()) {
		return false;
	}

	if (a.submitter < user) {
		return true;
	}

	return false;
}
};

int
SetPrivateAttributeString(int cluster_id, int proc_id, const char *attr_name, const char *attr_value)
{
	if (attr_name == nullptr || attr_value == nullptr) {return -1;}

	ClassAd *job_ad = GetJobAd(cluster_id, proc_id);
	if (job_ad == nullptr) {return -1;}

	Transaction *xact = JobQueue->getActiveTransaction();
	std::string quoted_value;
	QuoteAdStringValue(attr_value, quoted_value);
	JobQueueKey job_id(cluster_id, proc_id);
	JobQueue->SetAttribute(job_id, attr_name, quoted_value.c_str(), SetAttribute_SetDirty);
	job_ad->Delete(attr_name);
	PrivateAttrs[job_id][attr_name] = attr_value;
	JobQueue->setActiveTransaction(xact);
	return 0;
}

int
GetPrivateAttributeString(int cluster_id, int proc_id, const char *attr_name, std::string &attr_value)
{
	if (attr_name == nullptr) {return -1;}
	JobQueueKey job_id(cluster_id, proc_id);
	auto job_itr = PrivateAttrs.find(job_id);
	if (job_itr == PrivateAttrs.end()) {return -1;}
	auto attr_itr = job_itr->second.find(attr_name);
	if (attr_itr == job_itr->second.end()) {return -1;}
	attr_value = attr_itr->second;
	return 0;
}

int
DeletePrivateAttribute(int cluster_id, int proc_id, const char *attr_name)
{
	if (attr_name == nullptr) {return -1;}
	ClassAd *job_ad = GetJobAd(cluster_id, proc_id);
	if (job_ad == nullptr) {return -1;}
	job_ad->InsertAttr(attr_name, "");
	DeleteAttribute(cluster_id, proc_id, attr_name);
	JobQueueKey job_id(cluster_id, proc_id);
	PrivateAttrs[job_id].erase(attr_name);
	if (PrivateAttrs[job_id].empty()) {
		PrivateAttrs.erase(job_id);
	}
	return 0;
}


//static int allow_remote_submit = FALSE;
JobQueueLogType::filter_iterator
GetJobQueueIterator(const classad::ExprTree &requirements, int timeslice_ms)
{
	return JobQueue->GetFilteredIterator(requirements, timeslice_ms);
}

JobQueueLogType::filter_iterator
GetJobQueueIteratorEnd()
{
	return JobQueue->GetIteratorEnd();
}

typedef JOB_ID_KEY_BUF JobQueueKeyBuf;
static inline JobQueueKey& IdToKey(int cluster, int proc, JobQueueKeyBuf& key)
{
	key.set(cluster, proc);
	return key;
}
/*
typedef char JobQueueKeyStrBuf[PROC_ID_STR_BUFLEN];
static inline const char * KeyToStr(const JobQueueKey& key, JobQueueKeyStrBuf& buf) {
	ProcIdToStr(key.cluster, key.proc, buf);
	return buf;
}
static inline const JobQueueKey& StrToKey(const char * job_id_str, JobQueueKey& key) {
	StrToProcId(job_id_str, key.cluster, key.proc);
	return key;
}
*/

// Note, the only use of the following  is in the ifdef'd out section of DestroyProc
#if 0
static
void
KeyToId(JobQueueKey &key,int & cluster,int & proc)
{
	cluster = key.cluster;
	proc = key.proc;
}
#endif

static inline bool IsSpecialJobId( int cluster_id, int proc_id )
{
	// Return true if job id is special and should NOT be edited by
	// the end user.  Currently this means job 0.0 (header ad) 
	// and jobset ads <positive-integers>.-100
	//
	if (cluster_id <= 0 || proc_id < -1) {
		return true;
	}

	return false;
}

ClassAd* ConstructClassAdLogTableEntry<JobQueuePayload>::New(const char * key, const char * /* mytype */) const
{
	JOB_ID_KEY jid(key);
	if (jid.cluster > 0) {
		if (jid.proc >= 0) {
			return new JobQueueJob(jid);
		} else if (jid.proc == CLUSTERID_qkey2) {
			return new JobQueueCluster(jid);
		} else if (jid.proc == JOBSETID_qkey2) {
			return new JobQueueJobSet(qkey1_to_JOBSETID(jid.cluster));
		}
#ifdef USE_JOB_QUEUE_USERREC
	} else if (jid.cluster == USERRECID_qkey1 && jid.proc > 0) {
		if (schedd) { return schedd->jobqueue_newUserRec(jid.proc); }
		return new JobQueueUserRec(jid.proc);
#endif
	}
	return new JobQueueBase(jid, JobQueueBase::TypeOfJid(jid));
}


// PRAGMA_REMIND("TJ: fix these for JOB_STATUS_BLOCKED")
void JobQueueCluster::AttachJob(JobQueueJob * job)
{
	if ( ! job) return;
	++num_attached;
	qe.append_tail(job->qe);
	job->parent = this;
	switch (job->Status()) {
	case HELD: ++num_held; break;
	case IDLE: ++num_idle; break;
	case RUNNING: case TRANSFERRING_OUTPUT: case SUSPENDED: ++num_running; break;
	}
}
void JobQueueCluster::DetachJob(JobQueueJob * job)
{
	--num_attached;
	job->qe.detach();
	job->parent = nullptr;
	switch (job->Status()) {
	case HELD: --num_held; break;
	case IDLE: --num_idle; break;
	case RUNNING: case TRANSFERRING_OUTPUT: case SUSPENDED: --num_running; break;
	}
}
void JobQueueCluster::JobStatusChanged(int old_status, int new_status)
{
	switch (old_status) {
	case HELD: --num_held; break;
	case IDLE: --num_idle; break;
	case RUNNING: case TRANSFERRING_OUTPUT: case SUSPENDED: --num_running; break;
	}
	switch (new_status) {
	case HELD: ++num_held; break;
	case IDLE: ++num_idle; break;
	case RUNNING: case TRANSFERRING_OUTPUT: case SUSPENDED: ++num_running; break;
	}
}

void JobQueueCluster::PopulateInfoAd(ClassAd & iad, int num_pending, bool include_factory_info)
{
	int num_pending_held = 0, num_pending_idle = 0;
	iad.Assign("JobsPresent", num_attached + num_pending);
	if (num_pending) {
		int code = -1;
		if (this->factory && JobFactoryIsSubmitOnHold(this->factory, code)) {
			num_pending_held = num_pending;
		} else {
			num_pending_idle = num_pending;
		}
	}
	iad.Assign("JobsIdle", num_idle + num_pending_idle);
	iad.Assign("JobsRunning", num_running);
	iad.Assign("JobsHeld", num_held + num_pending_held);
	iad.Assign("ClusterSize", cluster_size + num_pending);
	if (this->factory && include_factory_info) {
		PopulateFactoryInfoAd(this->factory, iad);
	}
	iad.ChainToAd(this);
}


// detach all of the procs from a cluster, we really hope that the list of attached proc
// is empty before we get to the ::Delete call below, but just in case it isn't do the detach here.
void JobQueueCluster::DetachAllJobs() {
	while (qelm *q = qe.remove_head()) {
		auto * job = q->as<JobQueueJob>();
		job->Unchain();
		job->parent = nullptr;
		--num_attached;
		if (num_attached < -100) break; // something is seriously wrong here...
	}
	if (num_attached != 0) {
		dprintf(D_ALWAYS, "ERROR - Cluster %d has num_attached=%d after detaching all jobs\n", jid.cluster, num_attached);
	}
}

JobQueueJob * JobQueueCluster::FirstJob() {
	if (qe.empty()) return nullptr;
	return qe.next()->as<JobQueueJob>();
}

JobQueueJob * JobQueueCluster::NextJob(JobQueueJob * job)
{
	if (! job || job->qe.empty()) return nullptr;
	if (job->Cluster() != this) {
		// TODO: assert here?
		return nullptr;
	}
	job = job->qe.next()->as<JobQueueJob>();
	// when we get to the end of the linked list, we hit the JobQueueCluster record again
	if (job && job->IsCluster()) job = nullptr;
	return job;
}


// This is where we can clean up any data structures that refer to the job object
void
ConstructClassAdLogTableEntry<JobQueuePayload>::Delete(ClassAd* &ad) const
{
	if ( ! ad) return;
	auto bad = dynamic_cast<JobQueuePayload>(ad);
	if ( ! bad) {
		delete ad;
		return;
	}
#ifdef USE_JOB_QUEUE_USERREC
	if (schedd && bad->IsUserRec()) {
		// let schedd decide to delete (or not)
		schedd->jobqueue_deleteUserRec(dynamic_cast<JobQueueUserRec*>(bad));
		return;
	}
#endif
	if (in_DestroyJobQueue) {
		// skip the IsCluster() and IsJob() record keeping below when we are shutting down
	} else if (bad->IsCluster()) {
		// this is a cluster, detach all jobs
		auto * clusterad = dynamic_cast<JobQueueCluster*>(bad);
		if (clusterad->HasAttachedJobs()) {
			if ( ! in_DestroyJobQueue) {
				dprintf(D_ALWAYS, "WARNING - Cluster %d was deleted with proc ads still attached to it. This should only happen during schedd shutdown.\n", clusterad->jid.cluster);
			}
			clusterad->DetachAllJobs();
		}
	} else if (bad->IsJob()) {
		auto * job = dynamic_cast<JobQueueJob*>(bad);
		// this is a job
		//PRAGMA_REMIND("tj: decrement autocluster use count here??")

		// we do this here because DestroyProc could happen while we are in the middle of a transaction
		// in which case the actual destruction would be delayed until the transaction commit. i.e. here...
		IncrementLiveJobCounter(scheduler.liveJobCounts, job->Universe(), job->Status(), -1);
		if (job->ownerinfo) { IncrementLiveJobCounter(job->ownerinfo->live, job->Universe(), job->Status(), -1); }
		if (job->Cluster()) {
			job->Cluster()->DetachJob(job);
		}
	}
	delete bad;
}


static
void
ClusterCleanup(int cluster_id)
{
	JobQueueKeyBuf key;
	IdToKey(cluster_id,-1,key);

	// If this cluster has a job factory, write a ClusterRemove log event
	// TODO: ClusterRemove events should be logged for all clusters with more
	// than one proc, regardless of whether a factory or not.
	JobQueueCluster * clusterad = GetClusterAd(cluster_id);
	if( clusterad ) {
		if( clusterad->factory || clusterad->Lookup(ATTR_JOB_MATERIALIZE_DIGEST_FILE) ) {
			scheduler.WriteClusterRemoveToUserLog( clusterad, false );
		}

		if (scheduler.jobSets) {
			scheduler.jobSets->removeJobFromSet(*clusterad);
		}
	}
	else {
		dprintf(D_ALWAYS, "ERROR: ClusterCleanup() could not find ad for"
				" cluster ID %d\n", cluster_id);
	}

	// pull out the owner and hash used for ickpt sharing
	std::string hash, owner, digest;
	GetAttributeString(cluster_id, -1, ATTR_JOB_CMD_HASH, hash);
	if ( ! hash.empty()) {
		// TODO: fix for USERREC_NAME_IS_FULLY_QUALIFIED ?
		GetAttributeString(cluster_id, -1, ATTR_OWNER, owner);
	}
	const char * submit_digest = nullptr;
	if (GetAttributeString(cluster_id, -1, ATTR_JOB_MATERIALIZE_DIGEST_FILE, digest) >= 0 && ! digest.empty()) {
		submit_digest = digest.c_str();
	}

	// remove entry in ClusterSizeHashTable 
	ClusterSizeHashTable->remove(cluster_id);

	// delete the cluster classad
	JobQueue->DestroyClassAd( key );

	SpooledJobFiles::removeClusterSpooledFiles(cluster_id, submit_digest);

	// garbage collect the shared ickpt file if necessary
	// As of 9.0 new jobs will be unable to submit shared executables
	// but there may still be some jobs in the queue that have that.
	if (!hash.empty()) {
		ickpt_share_try_removal(owner.c_str(), hash.c_str());
	}
}

int GetSchedulerCapabilities(int mask, ClassAd & reply)
{
	reply.Assign( "LateMaterialize", scheduler.getAllowLateMaterialize() );
	reply.Assign("LateMaterializeVersion", 2);
	bool use_jobsets = scheduler.jobSets ? true : false;
	reply.Assign("UseJobsets", use_jobsets);
	const ClassAd * cmds = scheduler.getExtendedSubmitCommands();
	if (cmds && (cmds->size() > 0)) {
		reply.Insert("ExtendedSubmitCommands", cmds->Copy());
	}
	auto helpfile = scheduler.getExtendedSubmitHelpFile();
	if ( ! helpfile.empty()) {
		// if EXTENDED_SUBMIT_HELPFILE is not a URL, assume it is a small local file and return the content
		if ((mask & GetsScheddCapabilities_F_HELPTEXT) && ! IsUrl(helpfile.c_str())) {
			std::string contents;
			htcondor::readShortFile(helpfile, contents);
			reply.Assign("ExtendedSubmitHelp", contents);
		} else {
			reply.Assign("ExtendedSubmitHelpFile", helpfile);
		}
	}
	if (IsDebugVerbose(D_COMMAND)) {
		std::string buf;
		dprintf(D_COMMAND | D_VERBOSE, "GetSchedulerCapabilities(%d) returning:\n%s", mask, formatAd(buf, reply, "\t"));
	}
	return 0;
}

#ifdef USE_MATERIALIZE_POLICY
inline bool HasMaterializePolicy(JobQueueCluster * cad)
{
	if ( ! cad || ! cad->factory) return false;
	if (cad->Lookup(ATTR_JOB_MATERIALIZE_MAX_IDLE)) {
		return true;
	}
#if 0 // future
	if (cad->Lookup(ATTR_JOB_MATERIALIZE_CONSTRAINT)) {
		return true;
	}
#endif
	return false;
}
#else
inline bool HasMaterializePolicy(JobQueueCluster * /*cad*/)
{
	return false;
}
#endif

#ifdef USE_MATERIALIZE_POLICY
// num_pending should be the number of jobs that have been materialized, but not yet committed
// returns true if materialization of a single job is allowed by policy, if false retry_delay
// will be set to a suggested delay before trying again.  a retry_delay > 10 means "wait for a state change before retrying"
bool CheckMaterializePolicyExpression(JobQueueCluster * cad, int num_pending, int & retry_delay)
{
	long long max_idle = -1;
	if (cad->LookupInteger(ATTR_JOB_MATERIALIZE_MAX_IDLE, max_idle) && max_idle >= 0) {
		if ((cad->getNumNotRunning() + num_pending) < max_idle) {
			return true;
		} else {
			retry_delay = 20; // don't bother to retry by polling, wait for a job to change state instead.
			return false;
		}
	}

#if 0 // future
	if (cad->Lookup(ATTR_JOB_MATERIALIZE_CONSTRAINT)) {

		ClassAd iad;
		cad->PopulateInfoAd(iad, num_pending, submit_on_hold, false);

		classad::ExprTree * expr = NULL;
		ParseClassAdRvalExpr("JobsIdle < 4", expr);
		if (expr) {
			bool rv = EvalExprBool(&iad, expr);
			delete expr;
			if (rv) return true;
		}
	}

	retry_delay = 20; // don't bother to retry by polling, wait for a job to change state instead.
	return false;
#endif
	return true;
}
#else
bool CheckMaterializePolicyExpression(JobQueueCluster * /*cad*/, int & /*retry_delay*/)
{
	return true;
}
#endif

// This timer is after when we create a job factory, change the pause state of one or more job factories, or
// remove a job that is part of a cluster with a job factory. What we want to do here
// is either materialize a new job in that cluster, queue up a read of itemdata for the job factory
// or schedule the cluster for removal.
// 
void
JobMaterializeTimerCallback(int /* tid */)
{
	dprintf(D_MATERIALIZE | D_VERBOSE, "in JobMaterializeTimerCallback\n");

	bool allow_materialize = scheduler.getAllowLateMaterialize();
	if ( ! allow_materialize || ! JobQueue) {
		// materialization is disabled, so just clear the list and quit.
		ClustersNeedingMaterialize.clear();
	} else {

		int system_limit = MIN(scheduler.getMaxMaterializedJobsPerCluster(), scheduler.getMaxJobsPerSubmission());
		int owner_limit = scheduler.getMaxJobsPerOwner();
		//system_limit = MIN(system_limit, scheduler.getMaxJobsRunning());

		int total_new_jobs = 0;
		// iterate the list of clusters needing work, removing them from the work list when they
		// no longer need future materialization.
		for (auto it = ClustersNeedingMaterialize.begin(); it != ClustersNeedingMaterialize.end(); /*next handled in loop*/) {
			int cluster_id = *it;
			auto prev = it++;

			bool remove_entry = true;
			JobQueueCluster * cad = GetClusterAd(cluster_id);
			if ( ! cad) {
				// if no clusterad in the job queue, there is nothing to cleanup or materialize, so just skip it.
				dprintf(D_ALWAYS, "\tWARNING: JobMaterializeTimerCallback could not find cluster ad for cluster %d\n", cluster_id);

			} else if (cad->factory) {

				int factory_owner_limit = owner_limit;
				if (cad->ownerinfo) {
					factory_owner_limit = owner_limit - (cad->ownerinfo->num.JobsCounted + cad->ownerinfo->num.JobsRecentlyAdded);
				}
				if (factory_owner_limit < 1) {
					dprintf(D_MATERIALIZE | D_VERBOSE, "\tcluster %d cannot materialize because of MAX_JOBS_PER_OWNER\n", cluster_id);
				}

				if (JobFactoryAllowsClusterRemoval(cad)) {
					dprintf(D_MATERIALIZE | D_VERBOSE, "\tcluster %d has completed job factory\n", cluster_id);
					// if job factory is complete and there are no jobs attached to the cluster, schedule the cluster for removal
					if ( ! cad->HasAttachedJobs()) {
						ScheduleClusterForDeferredCleanup(cluster_id);
					}
				} else if (CanMaterializeJobs(cad)) {
					dprintf(D_MATERIALIZE | D_VERBOSE, "\tcluster %d has running job factory, invoking it.\n", cluster_id);

					int proc_limit = INT_MAX;
					if ( ! cad->LookupInteger(ATTR_JOB_MATERIALIZE_LIMIT, proc_limit) || proc_limit <= 0) {
						proc_limit = INT_MAX;
					}
					int effective_limit = MIN(proc_limit, system_limit);
					effective_limit = MIN(effective_limit, factory_owner_limit);
					//uncomment this code to poll quickly, but only if ownerinfo->num counters are also updated quickly (currently they are not)
					//if (factory_owner_limit < 1) { remove_entry = false; }

					dprintf(D_MATERIALIZE | D_VERBOSE, "in JobMaterializeTimerCallback, proc_limit=%d, sys_limit=%d MIN(%d,%d,%d), ClusterSize=%d\n",
						proc_limit, system_limit,
						scheduler.getMaxMaterializedJobsPerCluster(), scheduler.getMaxJobsPerSubmission(), factory_owner_limit,
						cad->ClusterSize());

					TransactionWatcher txn;
					int num_materialized = 0;
					int cluster_size = cad->ClusterSize();
					while ((cluster_size + num_materialized) < effective_limit) {
						int retry_delay = 0; // will be set to non-zero when we should try again later.
						int rv = 0;
						if (CheckMaterializePolicyExpression(cad, num_materialized, retry_delay)) {
							rv = MaterializeNextFactoryJob(cad->factory, cad, txn, retry_delay);
						}
						if (rv == 1) {
							num_materialized += 1;
						} else {
							// either failure, or 'not now' use retry_delay to tell the difference. 0 means stop materializing
							// for small non-zero values of retry, just leave this entry in the timer list so we end up polling.
							// larger retry values indicate that the factory is in a resumable pause state
							// which we will handle using a catMaterializeState transaction trigger rather than
							// by polling with this timer callback.
							if (retry_delay > 0 && retry_delay < 10) { remove_entry = false; }
							break;
						}
					}

					if (num_materialized > 0) {
						// If we materialized any jobs, we may need to commit the transaction now.
						if (txn.InTransaction()) {
							CondorError errorStack;
							SetAttributeFlags_t flags = scheduler.getNonDurableLateMaterialize() ? NONDURABLE : 0;
							int rv = txn.CommitIfAny(flags, &errorStack);
							if (rv < 0) {
								dprintf(D_ALWAYS, "CommitTransaction() failed for cluster %d rval=%d (%s)\n",
									cad->jid.cluster, rv, errorStack.empty() ? "no message" : errorStack.message());
								num_materialized = 0;
							}
						}
						total_new_jobs += num_materialized;
					} else {
						int next_row = 0;
						if (GetAttributeInt(cluster_id, -1, ATTR_JOB_MATERIALIZE_NEXT_ROW, &next_row) < 0) {
							next_row = 0;
						}
						dprintf(D_MATERIALIZE | D_VERBOSE, "cluster %d has_jobs=%d next_row=%d\n", cluster_id, cad->HasAttachedJobs(), next_row);

						// if we didn't materialize any jobs, the cluster has no attached jobs, and
						// the next row to materialize is -1 (which means no-more-rows)
						// we need to schedule the cluster for cleanup since it isn't likely to happen otherwise.
						if ( ! num_materialized && ! cad->HasAttachedJobs() && next_row == -1) {
							ScheduleClusterForDeferredCleanup(cluster_id);
						}
					}
					dprintf(D_MATERIALIZE, "cluster %d job factory invoked, %d jobs materialized\n", cluster_id, num_materialized);
				}

			}

			// factory is paused or completed in some way, so take it out of the list of clusters to service.
			if (remove_entry) {
				ClustersNeedingMaterialize.erase(prev);
			}
		}
		if (total_new_jobs > 0) {
			scheduler.needReschedule();
		}
	}

	if( ClustersNeedingMaterialize.empty() && job_materialize_timer_id > 0 ) {
		dprintf(D_FULLDEBUG, "Cancelling job materialization timer\n");
		daemonCore->Cancel_Timer(job_materialize_timer_id);
		job_materialize_timer_id = -1;
	}
}

void
ScheduleClusterForJobMaterializeNow(int cluster_id)
{
	ClustersNeedingMaterialize.insert(cluster_id);
	// Start timer to do job materialize if it is not already running.
	if( job_materialize_timer_id <= 0 ) {
		dprintf(D_FULLDEBUG, "Starting job materialize timer\n");
		job_materialize_timer_id = daemonCore->Register_Timer(0, 5, JobMaterializeTimerCallback, "JobMaterializeTimerCallback");
	}
}



// This timer is called when we scheduled deferred cluster cleanup, which we do when for clusters that
// have job factories
// sees num_procs for a cluster go to 0, but there is a job factory on that cluster refusing to let it die.
// our job here is to either materialize a new job, or actually do the cluster cleanup.
// 
void
DeferredClusterCleanupTimerCallback(int /* tid */)
{
	dprintf(D_MATERIALIZE | D_VERBOSE, "in DeferredClusterCleanupTimerCallback\n");

	int total_new_jobs = 0;

	// iterate the list of clusters needing work, removing them from the work list before we
	// service them. 
	for (auto it = ClustersNeedingCleanup.begin(); it != ClustersNeedingCleanup.end(); /*next handled in loop*/) {
		int cluster_id = *it;
		auto prev = it++;
		ClustersNeedingCleanup.erase(prev);

		dprintf(D_MATERIALIZE | D_VERBOSE, "\tcluster %d needs cleanup\n", cluster_id);

		// If no JobQueue, then we must be exiting. so skip all of the cleanup
		if ( ! JobQueue) continue;

		bool do_cleanup = true;

		// first get the proc count for this cluster, if it is zero then we *might* want to do cleanup
		int *numOfProcs = nullptr;
		if ( ClusterSizeHashTable->lookup(cluster_id,numOfProcs) != -1 ) {
			do_cleanup = *numOfProcs <= 0;
			dprintf(D_MATERIALIZE | D_VERBOSE, "\tcluster %d has %d procs, setting do_cleanup=%d\n", cluster_id, *numOfProcs, do_cleanup);
		} else {
			dprintf(D_MATERIALIZE | D_VERBOSE, "\tcluster %d has no entry in ClusterSizeHashTable, setting do_cleanup=%d\n", cluster_id, do_cleanup);
		}

		// get the clusterad, and have it materialize factory jobs if any are desired
		// if we materalized any jobs, then cleanup is cancelled.
		if (scheduler.getAllowLateMaterialize()) {
			JobQueueCluster * clusterad = GetClusterAd(cluster_id);
			if ( ! clusterad) {
				// if no clusterad in the job queue, there is nothing to cleanup or materialize, so just skip it.
				dprintf(D_ALWAYS, "\tWARNING: DeferredClusterCleanupTimerCallback could not find cluster ad for cluster %d\n", cluster_id);
				continue;
			}
			ASSERT(clusterad->IsCluster());
			if (clusterad->factory) {
				dprintf(D_MATERIALIZE | D_VERBOSE, "\tcluster %d has job factory, invoking it.\n", cluster_id);
				int retry_delay = 0;
				TransactionWatcher txn;
				int num_materialized = MaterializeJobs(clusterad, txn, retry_delay);
				if (num_materialized > 0) {
					if (txn.InTransaction()) {
						CondorError errorStack;
						SetAttributeFlags_t flags = scheduler.getNonDurableLateMaterialize() ? NONDURABLE : 0;
						int rv = txn.CommitIfAny(flags, &errorStack);
						if (rv < 0) {
							dprintf(D_ALWAYS, "CommitTransaction() failed for cluster %d rval=%d (%s)\n",
								cluster_id, rv, errorStack.empty() ? "no message" : errorStack.message());
							num_materialized = 0;
						}
					}
					total_new_jobs += num_materialized;
					// PRAGMA_REMIND("TJ: should we do_cleanup here if the transaction failed to commit?");
					do_cleanup = false;
				} else {
					// if no jobs materialized, we have to decide if the factory is done, or in a recoverable pause state
					if (retry_delay > 0) {
						do_cleanup = false;
					}
				}
				dprintf(D_MATERIALIZE, "cluster %d job factory invoked, %d jobs materialized%s\n", cluster_id, num_materialized, do_cleanup ? ", doing cleanup" : "");
			}
		}

		// if cleanup is still desired, do it now.
		if (do_cleanup) {
			dprintf(D_MATERIALIZE, "DeferredClusterCleanupTimerCallback: Removing cluster %d\n", cluster_id);
			ClusterCleanup(cluster_id);
		}
	}
	if (total_new_jobs > 0) {
		scheduler.needReschedule();
	}

	if( ClustersNeedingCleanup.empty() && defer_cleanup_timer_id > 0 ) {
		dprintf(D_FULLDEBUG, "Cancelling deferred cluster cleanup/job materialization timer\n");
		daemonCore->Cancel_Timer(defer_cleanup_timer_id);
		defer_cleanup_timer_id = -1;
	}
}

// this is called when there is a job factory for the cluster and a proc for that cluster is destroyed.
// it may be called even when the cluster still has materialized jobs
//  (i.e. is not really ready for cleanup yet)
void
ScheduleClusterForDeferredCleanup(int cluster_id)
{
	ClustersNeedingCleanup.insert(cluster_id);
	// Start timer to do deferred materialize if it is not already running.
	if( defer_cleanup_timer_id <= 0 ) {
		dprintf(D_FULLDEBUG, "Starting deferred cluster cleanup/materialize timer\n");
		defer_cleanup_timer_id = daemonCore->Register_Timer(0, 5, DeferredClusterCleanupTimerCallback, "DeferredClusterCleanupTimerCallback");
	}
}

static
int
IncrementClusterSize(int cluster_num)
{
	int 	*numOfProcs = nullptr;

	if ( ClusterSizeHashTable->lookup(cluster_num,numOfProcs) == -1 ) {
		// First proc we've seen in this cluster; set size to 1
		ClusterSizeHashTable->insert(cluster_num,1);
	} else {
		// We've seen this cluster_num go by before; increment proc count
		(*numOfProcs)++;
	}

		// return the number of procs in this cluster
	if ( numOfProcs ) {
		return *numOfProcs;
	} else {
		return 1;
	}
}

static
int
DecrementClusterSize(int cluster_id, JobQueueCluster * clusterAd)
{
	int 	*numOfProcs = nullptr;

	if ( ClusterSizeHashTable->lookup(cluster_id,numOfProcs) != -1 ) {
		// We've seen this cluster_num go by before; increment proc count
		// NOTICE that numOfProcs is a _reference_ to an int which we
		// fetched out of the hash table via the call to lookup() above.
		(*numOfProcs)--;

		bool cleanup_now = (*numOfProcs <= 0);
		if (clusterAd) {
			clusterAd->SetClusterSize(*numOfProcs);
			if (scheduler.getAllowLateMaterialize()) {
				// if there is a job factory, and it doesn't want us to do cleanup
				// then schedule deferred cleanup even if the cluster is not yet empty
				// we do this because deferred cleanup is where we do late materialization
				if ( ! JobFactoryAllowsClusterRemoval(clusterAd)) {
					ScheduleClusterForDeferredCleanup(cluster_id);
					cleanup_now = false;
				}
			}
		}

		// If this is the last job in the cluster, remove the initial
		//    checkpoint file and the entry in the ClusterSizeHashTable.
		if ( cleanup_now ) {
			ClusterCleanup(cluster_id);
			numOfProcs = nullptr;
		}
	}
	TotalJobsCount--;
	
		// return the number of procs in this cluster
	if ( numOfProcs ) {
		return *numOfProcs;
	} else {
		// if it isn't in our hashtable, there are no procs, so return 0
		return 0;
	}
}

// CRUFT: Everything in this function is cruft, but not necessarily all
//    the same cruft. Individual sections should say if/when they should
//    be removed.
// Look for attributes that have changed name or syntax in a previous
// version of Condor and convert the old format to the current format.
// *This function is not transaction-safe!*
// If startup is true, then this job was read from the job queue log on
//   schedd startup. Otherwise, it's a new job submission. Some of these
//   conversion checks only need to be done for existing jobs at startup.
// Returns true if the ad was modified, false otherwise.
void
ConvertOldJobAdAttrs( ClassAd *job_ad, bool startup )
{
	int universe = 0, cluster = 0, proc = 0;

	if (!job_ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
		dprintf(D_ALWAYS,
				"Job has no %s attribute. Skipping conversion.\n",
				ATTR_CLUSTER_ID);
		return;
	}

	if (!job_ad->LookupInteger(ATTR_PROC_ID, proc)) {
		dprintf(D_ALWAYS,
				"Job has no %s attribute. Skipping conversion.\n",
				ATTR_PROC_ID);
		return;
	}

	if( !job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) ) {
		dprintf( D_ALWAYS,
				 "Job %d.%d has no %s attribute. Skipping conversion.\n",
				 cluster, proc, ATTR_JOB_UNIVERSE );
		return;
	}

		// CRUFT
		// Before 7.6.0, Condor-C didn't set ATTR_HOLD_REASON_CODE
		// properly when submitting jobs to a remote schedd. Since then,
		// we look at the code instead of the hold reason string to
		// see if a job is expecting input files to be spooled.
	int job_status = -1;
	job_ad->LookupInteger( ATTR_JOB_STATUS, job_status );
	if ( job_status == HELD && job_ad->LookupExpr( ATTR_HOLD_REASON_CODE ) == nullptr ) {
		std::string hold_reason;
		job_ad->LookupString( ATTR_HOLD_REASON, hold_reason );
		if ( hold_reason == "Spooling input data files" ) {
			job_ad->Assign( ATTR_HOLD_REASON_CODE,
							CONDOR_HOLD_CODE::SpoolingInput );
		}
	}

		// CRUFT
		// Starting in 6.7.11, ATTR_JOB_MANAGED changed from a boolean
		// to a string.
	bool ntmp = false;
	if( startup && job_ad->LookupBool(ATTR_JOB_MANAGED, ntmp) ) {
		if(ntmp) {
			job_ad->Assign(ATTR_JOB_MANAGED, MANAGED_EXTERNAL);
		} else {
			job_ad->Assign(ATTR_JOB_MANAGED, MANAGED_SCHEDD);
		}
	}

		// CRUFT
		// Starting in 7.9.1, in ATTR_GRID_JOB_ID, the grid-types
		// pbs, sge, lsf, nqs, and naregi were made sub-types of
		// 'batch'.
	std::string orig_value;
	if ( job_ad->LookupString( ATTR_GRID_JOB_ID, orig_value ) ) {
		const char *orig_str = orig_value.c_str();
		if ( strncasecmp( "pbs", orig_str, 3 ) == 0 ||
			 strncasecmp( "sge", orig_str, 3 ) == 0 ||
			 strncasecmp( "lsf", orig_str, 3 ) == 0 ||
			 strncasecmp( "nqs", orig_str, 3 ) == 0 ||
			 strncasecmp( "naregi", orig_str, 6 ) == 0 ) {
			std::string new_value = "batch " + orig_value;
			job_ad->Assign( ATTR_GRID_JOB_ID, new_value );
		}
	}
}

QmgmtPeer::QmgmtPeer() : owner(NULL), fquser(NULL), myendpoint(NULL), sock(NULL), transaction(NULL)
{
	unset();
}

QmgmtPeer::~QmgmtPeer()
{
	unset();
}

bool
QmgmtPeer::set(ReliSock *input)
{
	if ( !input || sock || myendpoint ) {
		// already set, or no input
		return false;
	}
	
	sock = input;
	return true;
}

bool
QmgmtPeer::set(const condor_sockaddr& raddr, const char *o)
{
	if ( !raddr.is_valid() || sock || myendpoint ) {
		// already set, or no input
		return false;
	}
	ASSERT(owner == nullptr);

	if ( o ) {
		fquser = strdup(o);
			// owner is just fquser that stops at the first '@' 
		owner = strdup(o);
		char *atsign = strchr(owner,'@');
		if (atsign) {
			*atsign = '\0';
		}
	}

	addr = raddr;
	myendpoint = strdup(addr.to_ip_string().c_str());

	return true;
}

bool
QmgmtPeer::setAllowProtectedAttrChanges(bool val)
{
	bool old_val = allow_protected_attr_changes_by_superuser;

	allow_protected_attr_changes_by_superuser = val;

	return old_val;
}

#ifdef USE_JOB_QUEUE_USERREC
bool QmgmtPeer::setEffectiveOwner(const JobQueueUserRec * urec, bool ignore_effective_super)
{
	dprintf(D_FULLDEBUG, "QmgmtPeer::setEffectiveOwner(%p,%d) %s was %s\n ",
		urec, ignore_effective_super,
		urec ? urec->Name() : "(null)",
		this->jquser ? this->jquser->Name() : "(null)");

	not_super_effective = ignore_effective_super;
	if (urec == this->jquser) {
		// nothing to do
		return true;
	}
	free(owner); owner = nullptr;
	free(fquser); fquser = nullptr;

	jquser = urec;
	if ( ! jquser) {
		dprintf(D_ALWAYS, "QmgmtPeer::setEffectiveOwner(%p,%d) result is to clear effective\n",
			urec, ignore_effective_super);
		return true;
	}

	const char * o = jquser->Name();
	owner = strdup(o);
	char * at = strrchr(owner,'@');
	if (at) {
		fquser = owner;
		*at = 0;
		owner = strdup(fquser);
		*at = '@';
	} else {
		std::string user = std::string(o) + "@" + scheduler.uidDomain();
		fquser = strdup(user.c_str());
	}

	dprintf(D_ALWAYS, "QmgmtPeer::setEffectiveOwner(%p,%d) result is user=%s owner=%s\n",
		urec, ignore_effective_super,
		fquser ? fquser : "(null)",
		owner ? owner : "(null)");

	return true;
}
#else
// pass either unqualified owner "bob" or fully qualified "bob@chtc.wisc.edu"
// this will set both EffectiveOwner and EffectiveUser appropriately
bool
QmgmtPeer::setEffectiveOwner(char const *o)
{
	if (o && ((owner  && MATCH == strcmp(owner , o)) ||
		      (fquser && MATCH == strcmp(fquser, o)))
		) {
		// nothing to do.
		return true;
	}

	free(owner);
	owner = NULL;
	free(fquser);
	fquser = NULL;

	if ( o ) {
		owner = strdup(o);
		char * at = strrchr(owner,'@');
		if (at) {
			fquser = owner;
			*at = 0;
			owner = strdup(fquser);
			*at = '@';
		} else {
			std::string user = std::string(o) + "@" + scheduler.uidDomain();
			fquser = strdup(user.c_str());
		}
	}
	return true;
}
#endif

bool
QmgmtPeer::initAuthOwner(bool read_only)
{
	readonly = read_only;
	if (sock) {
		const char * user = sock->getFullyQualifiedUser();
		if (user) {
			jquser = scheduler.lookup_owner_const(user);
			if ( ! jquser) {
				jquser = real_owner_is_condor(sock);
			}
		}
		real_auth_is_super = isQueueSuperUser(jquser);
	}
	return jquser != nullptr;
}

void
QmgmtPeer::unset()
{
	if (owner) {
		free(owner);
		owner = nullptr;
	}
	if (fquser) {
		free(fquser);
		fquser = nullptr;
	}
	if (myendpoint) {
		free(myendpoint);
		myendpoint = nullptr;
	}
	if (sock) sock=nullptr;	// note: do NOT delete sock!!!!!
	if (transaction) {
		delete transaction;
		transaction = nullptr;
	}

	next_proc_num = 0;
	active_cluster_num = -1;	
	xact_start_time = 0;	// time at which the current transaction was started
	readonly = false;
	real_auth_is_super = false;
	jquser = nullptr;
}

const char*
QmgmtPeer::endpoint_ip_str() const
{
	if ( sock ) {
		return sock->peer_ip_str();
	} else {
		return myendpoint;
	}
}

const condor_sockaddr
QmgmtPeer::endpoint() const
{
	if ( sock ) {
		return sock->peer_addr();
	} else {
		return addr;
	}
}

int
QmgmtPeer::isAuthenticated() const
{
	if ( sock ) {
		return sock->isMappedFQU();
	} else {
		if ( qmgmt_all_users_trusted ) {
			return TRUE;
		} else {
			return owner ? TRUE : FALSE;
		}
	}
}


// Read out any parameters from the config file that we need and
// initialize our internal data structures.  This is also called
// on reconfig.
void
InitQmgmt()
{
	std::vector<std::string> s_users;

	std::string uid_domain;
	param(uid_domain, "UID_DOMAIN");

	qmgmt_was_initialized = true;

	auto_free_ptr super(param("QUEUE_SUPER_USERS"));
	if (super) {
		s_users = split(super);
	} else {
		s_users = split(default_super_user);
	}
	dprintf(D_ALWAYS, "config super users : %s\n", super ? super : default_super_user);
#ifdef WIN32
	const char * process_dom_and_user = get_condor_username();
	const char * process_user = strchr(process_dom_and_user, '/');
	if (process_user) { ++process_user; } else { process_user = process_dom_and_user; }
	if ( ! contains(s_users, process_user) ) { s_users.emplace_back(process_user); }
	if ( ! contains(s_users, "condor")) { s_users.emplace_back("condor"); } // because of family security sessions
#else
	if( ! contains(s_users, get_condor_username()) ) {
		s_users.emplace_back( get_condor_username() );
	}
#endif
	super_users.clear();
	super_users.reserve(s_users.size() + 3);
	for (auto& tmp: s_users) {
		if (user_is_the_new_owner) {
			// Backward compatibility hack:
			// QUEUE_SUPER_USERS historically has referred to owners (actual OS usernames)
			// as opposed to a HTCondor user; the defaults are `root` and `condor`.  We now
			// get the 'user' from the authenticated identity (which is of the form 'user@domain').
			// Hence, we try a few alternate identities; particularly, if 'foo' is a superuser,
			// then we also consider 'foo@UID_DOMAIN' a superuser.  Additionally, if 'condor'
			// is a superuser, then 'condor@child', 'condor@parent', and 'condor@family'
			// are considered superusers.
			std::string super_user(tmp);
			if (super_user.find('@') == std::string::npos) {
				std::string alt_super_user = tmp + "@" + uid_domain;
				super_users.emplace_back(alt_super_user);
			} else {
				super_users.emplace_back(super_user);
			}
#ifdef WIN32
			if (!strcasecmp(tmp.c_str(), "condor"))
#else
			if (!strcmp(tmp.c_str(), "condor"))
#endif
			{
				super_users.emplace_back("condor@child");
				super_users.emplace_back("condor@parent");
				super_users.emplace_back("condor@family");
			}
		} else {
			super_users.emplace_back(tmp);
		}
	}

	if( IsFulldebug(D_FULLDEBUG) ) {
		dprintf( D_FULLDEBUG, "Queue Management Super Users:\n" );
		for (const auto &username : super_users) {
			dprintf( D_FULLDEBUG, "\t%s\n", username.c_str() );
		}
	}

	qmgmt_all_users_trusted = param_boolean("QUEUE_ALL_USERS_TRUSTED",false);
	if (qmgmt_all_users_trusted) {
		dprintf(D_ALWAYS,
			"NOTE: QUEUE_ALL_USERS_TRUSTED=TRUE - "
			"all queue access checks disabled!\n"
			);
	}

	delete queue_super_user_may_impersonate_regex;
	queue_super_user_may_impersonate_regex = nullptr;
	std::string queue_super_user_may_impersonate;
	if( param(queue_super_user_may_impersonate,"QUEUE_SUPER_USER_MAY_IMPERSONATE") ) {
		queue_super_user_may_impersonate_regex = new Regex;
		int errnumber = 0;
		int erroffset=0;
		if( !queue_super_user_may_impersonate_regex->compile(queue_super_user_may_impersonate.c_str(),&errnumber,&erroffset) ) {
			EXCEPT("QUEUE_SUPER_USER_MAY_IMPERSONATE is an invalid regular expression: %s",queue_super_user_may_impersonate.c_str());
		}
	}

	immutable_attrs.clear();
	param_and_insert_attrs("IMMUTABLE_JOB_ATTRS", immutable_attrs);
	param_and_insert_attrs("SYSTEM_IMMUTABLE_JOB_ATTRS", immutable_attrs);
	protected_attrs.clear();
	param_and_insert_attrs("PROTECTED_JOB_ATTRS", protected_attrs);
	param_and_insert_attrs("SYSTEM_PROTECTED_JOB_ATTRS", protected_attrs);
	secure_attrs.clear();
	param_and_insert_attrs("SECURE_JOB_ATTRS", secure_attrs);
	param_and_insert_attrs("SYSTEM_SECURE_JOB_ATTRS", secure_attrs);

	Ignore_Secure_SetAttr_Attempts = param_boolean("IGNORE_ATTEMPTS_TO_SET_SECURE_JOB_ATTRS", true);

	schedd_forker.Initialize();
	int max_schedd_forkers = param_integer ("SCHEDD_QUERY_WORKERS",8,0);
	schedd_forker.setMaxWorkers( max_schedd_forkers );

	cluster_initial_val = param_integer("SCHEDD_CLUSTER_INITIAL_VALUE",1,1);
	cluster_increment_val = param_integer("SCHEDD_CLUSTER_INCREMENT_VALUE",1,1);
    cluster_maximum_val = param_integer("SCHEDD_CLUSTER_MAXIMUM_VALUE",0,0);

	flush_job_queue_log_delay = param_integer("SCHEDD_JOB_QUEUE_LOG_FLUSH_DELAY",5,0);
	dirty_notice_interval = param_integer("SCHEDD_JOB_QUEUE_NOTIFY_UPDATES",30,0);
}

void
SetMaxHistoricalLogs(int max_historical_logs)
{
	JobQueue->SetMaxHistoricalLogs(max_historical_logs);
}

time_t
GetOriginalJobQueueBirthdate()
{
	return JobQueue->GetOrigLogBirthdate();
}

static void
RenamePre_7_5_5_SpoolPathsInJob( ClassAd *job_ad, char const *spool, int cluster, int proc )
{
	std::string old_path;
	formatstr(old_path,"%s%ccluster%d.proc%d.subproc%d", spool, DIR_DELIM_CHAR, cluster, proc, 0);
	std::string new_path;
	SpooledJobFiles::getJobSpoolPath(job_ad, new_path);
	ASSERT( !new_path.empty() );

	static const int ATTR_ARRAY_SIZE = 6;
	static const char *AttrsToModify[ATTR_ARRAY_SIZE] = { 
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY,
		ATTR_JOB_IWD};
	static const bool AttrIsList[ATTR_ARRAY_SIZE] = {
		false,
		false,
		true,
		false,
		false,
		false};

	int a = 0;
	for(a=0;a<ATTR_ARRAY_SIZE;a++) {
		char const *attr = AttrsToModify[a];
		std::string v;
		char const *o = old_path.c_str();
		char const *n = new_path.c_str();

		if( !job_ad->LookupString(attr,v) ) {
			continue;
		}
		if( !AttrIsList[a] ) {
			if( !strncmp(v.c_str(),o,strlen(o)) ) {
				std::string np = n;
				np += v.c_str() + strlen(o);
				dprintf(D_ALWAYS,"Changing job %d.%d %s from %s to %s\n",
						cluster, proc, attr, o, np.c_str());
				job_ad->Assign(attr,np);
			}
			continue;
		}

			// The value we are changing is a list of files
		std::vector<std::string> new_paths;
		bool changed = false;

		char const *op = nullptr;
		for (auto& item: StringTokenIterator(v, ",")) {
			op = item.c_str();
			if( !strncmp(op,o,strlen(o)) ) {
				std::string np = n;
				np += op + strlen(o);
				new_paths.emplace_back(np);
				changed = true;
			}
			else {
				new_paths.emplace_back(op);
			}
		}

		if( changed ) {
			std::string nv = join(new_paths, ",");
			dprintf(D_ALWAYS,"Changing job %d.%d %s from %s to %s\n",
					cluster, proc, attr, v.c_str(), nv.c_str());
			job_ad->Assign(attr, nv);
		}
	}
}


static void
SpoolHierarchyChangePass1(char const *spool,std::list< PROC_ID > &spool_rename_list)
{
	int cluster = 0, proc = 0, subproc = 0;

	Directory spool_dir(spool);
	const char *f = nullptr;
	while( (f=spool_dir.Next()) ) {
		int len = 0;
		cluster = proc = subproc = -1;
		len = 0;

		if( sscanf(f,"cluster%d.proc%d.subproc%d%n",&cluster,&proc,&subproc,&len)==3 && f[len] == '\0' )
		{
			dprintf(D_ALWAYS,"Found pre-7.5.5 spool directory %s\n",f);
			if( !GetJobAd( cluster, proc ) ) {
				dprintf(D_ALWAYS,"No job %d.%d exists, so ignoring old spool directory %s.\n",cluster,proc,f);
			}
			else {
				PROC_ID job_id;
				job_id.cluster = cluster;
				job_id.proc = proc;
				spool_rename_list.push_back(job_id);
			}
		}

		cluster = proc = subproc = -1;
		len = 0;
		if( sscanf(f,"cluster%d.ickpt.subproc%d%n",&cluster,&subproc,&len)==2 && f[len] == '\0')
		{
			dprintf(D_ALWAYS,"Found pre-7.5.5 spooled executable %s\n",f);
			if( !GetJobAd( cluster, ICKPT ) ) {
				dprintf(D_ALWAYS,"No job %d.%d exists, so ignoring old spooled executable %s.\n",cluster,proc,f);
			}
			else {
				PROC_ID job_id;
				job_id.cluster = cluster;
				job_id.proc = ICKPT;
				spool_rename_list.push_back(job_id);
			}
		}
	}

	std::list< PROC_ID >::iterator spool_rename_it;
	for( spool_rename_it = spool_rename_list.begin();
		 spool_rename_it != spool_rename_list.end();
		 spool_rename_it++ )
	{
		PROC_ID job_id = *spool_rename_it;
		cluster = job_id.cluster;
		proc = job_id.proc;

		ClassAd *job_ad = GetJobAd( cluster, proc );
		ASSERT( job_ad ); // we already checked that this job exists

		RenamePre_7_5_5_SpoolPathsInJob( job_ad, spool, cluster, proc );

		JobQueueDirty = true;
	}
}

static void
SpoolHierarchyChangePass2(char const *spool,std::list< PROC_ID > &spool_rename_list)
{
	int cluster = 0, proc = 0;

		// now rename the job spool directories and executables
	std::list< PROC_ID >::iterator spool_rename_it;
	for( spool_rename_it = spool_rename_list.begin();
		 spool_rename_it != spool_rename_list.end();
		 spool_rename_it++ )
	{
		PROC_ID job_id = *spool_rename_it;
		cluster = job_id.cluster;
		proc = job_id.proc;

		ClassAd *job_ad = GetJobAd( cluster, proc );
		ASSERT( job_ad ); // we already checked that this job exists

		std::string old_path;
		std::string new_path;

		if( proc == ICKPT ) {
			formatstr(old_path,"%s%ccluster%d.ickpt.subproc%d",spool,DIR_DELIM_CHAR,cluster,0);
		}
		else {
			formatstr(old_path,"%s%ccluster%d.proc%d.subproc%d",spool,DIR_DELIM_CHAR,cluster,proc,0);
		}
		SpooledJobFiles::getJobSpoolPath(job_ad, new_path);

		if( !SpooledJobFiles::createParentSpoolDirectories(job_ad) ) {
			EXCEPT("Failed to create parent spool directories for "
				   "%d.%d: %s: %s",
				   cluster,proc,new_path.c_str(),strerror(errno));
		}

		priv_state saved_priv;
		if( proc != ICKPT ) {
			std::string old_tmp_path = old_path + ".tmp";
			std::string new_tmp_path = new_path + ".tmp";

				// We move the tmp directory first, because it is the presence
				// of the non-tmp directory that is checked for if we crash
				// and restart.
			StatInfo si(old_tmp_path.c_str());
			if( si.Error() != SINoFile ) {
				saved_priv = set_priv(PRIV_ROOT);

				if( rename(old_tmp_path.c_str(),new_tmp_path.c_str())!= 0 ) {
					EXCEPT("Failed to move %s to %s: %s",
						   old_tmp_path.c_str(),
						   new_tmp_path.c_str(),
						   strerror(errno));
				}

				set_priv(saved_priv);

				dprintf(D_ALWAYS,"Moved %s to %s.\n",
						old_tmp_path.c_str(), new_tmp_path.c_str() );
			}
		}

		saved_priv = set_priv(PRIV_ROOT);

		if( rename(old_path.c_str(),new_path.c_str())!= 0 ) {
			EXCEPT("Failed to move %s to %s: %s",
				   old_path.c_str(),
				   new_path.c_str(),
				   strerror(errno));
		}

		set_priv(saved_priv);

		dprintf(D_ALWAYS,"Moved %s to %s.\n",
				old_path.c_str(), new_path.c_str() );
	}
}

/* handled in header for now.
JobQueueJob::~JobQueueJob()
{
}
*/

JobQueueCluster::~JobQueueCluster()
{
	if (this->factory) {
		DestroyJobFactory(this->factory);
		this->factory = nullptr;
	}
}

bool JobQueueJob::IsNoopJob()
{
	if ( ! has_noop_attr) return false;
	bool noop = false;
	if ( ! this->LookupBool(ATTR_JOB_NOOP, noop)) { has_noop_attr = false; }
	else { has_noop_attr = true; }
	return has_noop_attr && noop;
}

void JobQueueBase::CheckJidAndType(const JOB_ID_KEY &key)
{
	if (key.cluster != jid.cluster || key.proc != jid.proc) {
		dprintf(D_ERROR, "ERROR! - JobQueueBase jid is not initialized for %d.%d", key.cluster, key.proc);
	}
	if (TypeOfJid(key) != entry_type) {
		dprintf(D_ERROR, "ERROR! - JobQueueBase entry_type (%d) is wrong type for %d.%d", entry_type, key.cluster, key.proc);
	}
}

// After the job queue is loaded from disk, or a new job is submitted
// the fields in the job object have to be initialized to match the ad
//
void JobQueueBase::PopulateFromAd()
{
	// TODO: TJ remove this? it should be dead as of 9.10.0
	if (!entry_type) {
		if (!jid.cluster) {
			this->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
			this->LookupInteger(ATTR_PROC_ID, jid.proc);
		}
		entry_type = TypeOfJid(jid);
		if (entry_type) {
			dprintf(D_ERROR, "WARNING - JobQueueBase had no entry_type set %d.%d\n", jid.cluster, jid.proc);
		}
	}
}

#ifdef USE_JOB_QUEUE_USERREC

static bool MakeUserRec(const OwnerInfo * owni, bool enabled, const ClassAd * defaults);

void JobQueueUserRec::PopulateFromAd()
{
	if (this->name.empty()) {
		this->LookupString(ATTR_USERREC_NAME, this->name);
	}
	if (this->flags & JQU_F_DIRTY) {
		this->LookupBool(ATTR_ENABLED, this->enabled);
		if ( ! this->LookupString(ATTR_NT_DOMAIN, this->domain)) { this->domain.clear(); }
		this->flags &= ~JQU_F_DIRTY;
	}
}

// forward declaration
bool CreateNeededUserRecs(const std::map<int, OwnerInfo*> &needed_owners);
#endif

void JobQueueJob::PopulateFromAd()
{
	// First have our base class fill in whatever it can
	JobQueueBase::PopulateFromAd();

	if ( ! this->Lookup(ATTR_TARGET_TYPE)) {
		// For backward compat matchmaking with pre 23.0 HTCondor Execute nodes and Negotiators
		// TODO: move this into the the code that creates resource requests?
		this->Assign(ATTR_TARGET_TYPE, JOB_TARGET_ADTYPE);
	}

	if ( ! universe) {
		int uni = 0;
		if (this->LookupInteger(ATTR_JOB_UNIVERSE, uni)) {
			this->universe = uni;
		}
	}

	if ( ! set_id) {
		this->LookupInteger(ATTR_JOB_SET_ID, set_id);
	}

	// NOTE: we do *NOT* want to populate job state here because we 
	// want to count state transitions.  JobQueueJob objects are always created in IDLE state
	// and transitioned to the submitted state (if different) at the end of CommitTransaction
	// or InitJobQueue

#if 0	// we don't do this anymore, since we update both the ad and the job object
		// when we calculate the autocluster - the on-disk value is never useful.
	if (autocluster_id < 0) {
		this->LookupInteger(ATTR_AUTO_CLUSTER_ID, this->autocluster_id);
	}
#endif
}

// stuff we will need if we have to update the UID domain of the USER attribute as we load
struct ownerinfo_init_state {
	const char * prior_uid_domain;
	const char * uid_domain;
	bool update_uid_domain;
};

// helper function for InitJobQueue
// sets the ownerinfo field in the JobQueueJob header
// valid for use on JobQueueJob, JobQueueCluster and JobQueueJobSet ads
static bool
InitOwnerinfo(
	JobQueueBase * bad,
	std::string & owner,
	const struct ownerinfo_init_state & is)
{
#ifdef USE_JOB_QUEUE_USERREC

	// fetch the actual value of Owner or User from the ad
	// and if the update_uid_domain flag is passed, fixup the domain of the User as requested
	if (bad->LookupString(ATTR_USER, owner)) {
		YourStringNoCase domain(domain_of_user(owner.c_str(),nullptr));
		if (is.update_uid_domain && (domain == is.prior_uid_domain)) {
			size_t at_sign = owner.find_last_of('@');
			if (at_sign != std::string::npos) {
				owner.erase(at_sign+1);
				owner += is.uid_domain;
				bad->Assign(ATTR_USER, owner);
				JobQueueDirty = true;
			}
		}
		if ( ! USERREC_NAME_IS_FULLY_QUALIFIED) {
			// TODO: romove this once we go fully qualified.
			if ( ! bad->LookupString(ATTR_USERREC_NAME, owner)) {
				owner.clear();
			}
		}
	} else {
		owner.clear();
	}
#else
	// owner_history tracks the OS usernames that have
	// been used to run jobs on this schedd; it's part of a
	// security mechanism to prevent the schedd from executing
	// as an OS user who has never submitted jobs.  Hence, we
	// actually want to use ATTR_OWNER here and not ATTR_USER.
	if (bad->LookupString(ATTR_OWNER, owner)) {
		AddOwnerHistory(owner);
	} else {
		owner.clear();
	}

#endif

	if (owner.empty())
		return false;

	auto* ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(owner.c_str()));
	if (bad->IsCluster() || bad->IsJob()) {
		dynamic_cast<JobQueueJob*>(bad)->ownerinfo = ownerinfo;
	} else if (bad->IsJobSet()) {
		dynamic_cast<JobQueueJobSet*>(bad)->ownerinfo = ownerinfo;
	}
#ifdef USE_JOB_QUEUE_USERREC
	// owner_history tracks the OS usernames that have
	// been used to run jobs on this schedd; it's part of a
	// security mechanism to prevent the schedd from executing
	// as an OS user who has never submitted jobs.  Hence, we
	// want to use the bare username and not the fully qualified one
	// TODO: git rid of OwnerHistory and use JobQueueUserRec instead
	if (ownerinfo) {
		YourStringNoCase domain(domain_of_user(ownerinfo->Name(), scheduler.uidDomain()));
		if (domain == scheduler.uidDomain()) {
			AddOwnerHistory(name_of_user(ownerinfo->Name(), owner));
		}
	#ifdef WIN32
		// if this ownerinfo does not yet have an NTDomain value, copy that from the job
		// TODO: when USERREC_NAME_IS_FULLY_QUALIFIED check domain against User attribute
		// we expect this code to trigger only when loading a job queue for the first time
		// after upgrading to 10.5.x
		std::string ntdomain;
		if ( ! ownerinfo->LookupString(ATTR_NT_DOMAIN, ntdomain) &&
			bad->LookupString(ATTR_NT_DOMAIN, ntdomain)) {
			ownerinfo->Assign (ATTR_NT_DOMAIN, ntdomain);
			ownerinfo->flags |= JQU_F_DIRTY;
			ownerinfo->PopulateFromAd();
			JobQueueDirty = true;
		}
	#endif
	}
#endif
	return true;
}

// helper function for InitJobQueue
// called during the first loop iteration, posibly ahead of iteration order
// do NOT call if jobset->ownerinfo has already been set
static bool
InitJobsetAd(
	JobQueueJobSet * jobset,
	std::string & owner,
	const struct ownerinfo_init_state & is)
{
	if ( ! InitOwnerinfo(jobset, owner, is))
		return false;

	if (scheduler.jobSets) {
		scheduler.jobSets->restoreJobSet(jobset);
	}
	return true;
}


// helper function for InitJobQueue
// called during the first loop iteration, posibly ahead of iteration order
// do NOT call if cad->ownerinfo has already been set
static bool
InitClusterAd (
	JobQueueCluster * cad,
	std::string & owner,
	const struct ownerinfo_init_state & is,
	std::vector<unsigned int> & jobset_ids,
	std::unordered_map<std::string, unsigned int> & needed_sets)
{
	std::string name1, name2;

	if ( ! InitOwnerinfo(cad, owner, is))
		return false;

	cad->PopulateFromAd();

	if ( ! scheduler.jobSets) {
		cad->set_id = 0;
		return true;
	}

	// do jobset sanity checking and build a map of jobsets that we need to create
	//
	if ( ! cad->LookupString(ATTR_JOB_SET_NAME, name1)) {
		if (cad->set_id > 0) cad->Delete(ATTR_JOB_SET_ID);
		cad->set_id = -1; // not in a jobset. remember that.
	} else {
		// verify the setid, and determine if we need to create a new jobset object
		JobQueueJobSet * jobset = GetJobSetAd(cad->set_id);
		if (jobset) {
			// we might end up looking at the jobset ad before we iterate it, so make sure the ownerinfo is set
			if ( ! jobset->ownerinfo) {
				std::string owner2;
				InitJobsetAd(jobset, owner2, is);
				jobset_ids.push_back(jobset->Jobset());
			}

			// verify that the name of the jobset matches the name in the ad
			jobset->LookupString(ATTR_JOB_SET_NAME, name2);
			if (MATCH != strcasecmp(name1.c_str(), name2.c_str()) || jobset->ownerinfo != cad->ownerinfo) {
				jobset = nullptr;
			}
		}

		// we want a jobset, but don't know the id, so we add it to the map
		// while also keeping track of the lowest cluster id that wants that setname/owner combo
		if ( ! jobset) {
			name2 = JobSets::makeAlias(name1, *cad->ownerinfo);
			unsigned int & setId = needed_sets[name2];
			if (!setId || (setId > (unsigned) cad->jid.cluster)) { setId = cad->jid.cluster; }

			// set set_id header field to 0 to mean 'figure out the id later'
			// and if it was already set to non-zero,  that means the ad has a bogus attribute value
			if (cad->set_id > 0) cad->Delete(ATTR_JOB_SET_ID);
			cad->set_id = 0;
		}
	}
	return true;
}


bool
CreateNeededJobsets(
	std::unordered_map<std::string, unsigned int> & needed_sets,  // map of jobset alias to lowest clusterid that needs that jobset
	std::vector<unsigned int> & jobset_ids)
{
	int  fail_count = 0;
	bool already_in_transaction = InTransaction();
	if (!already_in_transaction) {
		BeginTransaction();
	}

	// create the needed sets
	for (const auto& it : needed_sets) {
		const std::string & alias = it.first;
		int cluster_id = it.second;
		JobQueueCluster * cad = GetClusterAd(cluster_id);
		if (! cad) {
			// TODO: this is unexpected
			dprintf(D_ERROR, "UNEXPECTED - no cluster ad for needed jobset id %d\n", cluster_id);
			continue;
		}
		int jobset_id = cluster_id; // assume we will use the cluster id as the jobset id

		// we don't expect there to be a jobset ad (yet), but if there is, we should check to see that the name is what we want.
		JobQueueJobSet * sad = GetJobSetAd(jobset_id);
		if (sad) {
			std::string name2;
			sad->LookupString(ATTR_JOB_SET_NAME, name2);
			if (alias == JobSets::makeAlias(name2, *sad->ownerinfo)) {
				dprintf(D_FULLDEBUG, "need jobset id %d already has a jobset ad\n", jobset_id);
				continue;
			} else {
				// allocate a new jobset id here
				jobset_id = NewCluster(nullptr);
				dprintf(D_STATUS, "new jobset id %d allocated for %s\n", jobset_id, alias.c_str());
				sad = nullptr;
			}
		}

		std::string setname;
		cad->LookupString(ATTR_JOB_SET_NAME, setname);
		if ( ! JobSetCreate(jobset_id, setname.c_str(), cad->ownerinfo->Name())) {
			++fail_count;
			continue;
		} else {
			dprintf(D_STATUS, "Created new jobset %s for user %s.  jobset id = %d\n", setname.c_str(), cad->ownerinfo->Name(), jobset_id);
			jobset_ids.push_back(jobset_id);
			cad->set_id = jobset_id;
			JobQueueDirty = true;
		}
	}

	if ( ! already_in_transaction) {
		if (fail_count) {
			AbortTransaction();
		} else {
			CommitNonDurableTransactionOrDieTrying();
		}
	}

	return fail_count == 0;
}

void
InitJobQueue(const char *job_queue_name,int max_historical_logs)
{
	ASSERT(qmgmt_was_initialized);	// make certain our parameters are setup
	ASSERT(!JobQueue);

	std::string spool;
	if( !param(spool,"SPOOL") ) {
		EXCEPT("SPOOL must be defined.");
	}

#ifndef WIN32
	{
		// The master usually does this, but if we are a secondary schedd,
		// it can't know what our per-schedd SPOOL is set to.
		uid_t condor_uid = get_condor_uid();
		uid_t condor_gid = get_condor_gid();
		TemporaryPrivSentry sentry(PRIV_ROOT);
		std::ignore = mkdir(spool.c_str(), 0755); // just in case
		std::ignore = chown(spool.c_str(), condor_uid, condor_gid);
	}
#endif

	int spool_min_version = 0;
	int spool_cur_version = 0;
	CheckSpoolVersion(spool.c_str(),SPOOL_MIN_VERSION_SCHEDD_SUPPORTS,SPOOL_CUR_VERSION_SCHEDD_SUPPORTS,spool_min_version,spool_cur_version);

#ifdef JOB_QUEUE_PAYLOAD_IS_BASE
	JobQueue = new JobQueueType();
#else
	JobQueue = new JobQueueType(new ConstructClassAdLogTableEntry<JobQueuePayload>());
#endif
	if( !JobQueue->InitLogFile(job_queue_name,max_historical_logs) ) {
		EXCEPT("Failed to initialize job queue log!");
	}
	ClusterSizeHashTable = new ClusterSizeHashTable_t(hashFuncInt);
	TotalJobsCount = 0;
	jobs_added_this_transaction = 0;


	/* We read/initialize the header ad in the job queue here.  Currently,
	   this header ad just stores the next available cluster number. */
	JobQueueBase *bad = nullptr;
	JobQueueCluster *clusterad = nullptr;
	JobQueueKey key;
	std::vector<unsigned int> jobset_ids;
	std::unordered_map<std::string, unsigned int> needed_sets;
	int 	cluster_num = 0, cluster = 0, proc = 0, universe = 0;
	int		stored_cluster_num = 0;
	bool	CreatedAd = false;
	JobQueueKey cluster_key;
	std::string	owner;
	std::string	user;
	std::string correct_user;
	std::string	attr_scheduler;
	std::string correct_scheduler;
	std::string buffer;
	std::string name1;

	// setup stuff we will need if we have to update the UID domain of the USER attribute as we load
	auto_free_ptr prior_uid_domain(param("PRIOR_UID_DOMAIN"));
	struct ownerinfo_init_state ownerinfo_is{};
	ownerinfo_is.prior_uid_domain = prior_uid_domain;
	ownerinfo_is.uid_domain = scheduler.uidDomain();
	ownerinfo_is.update_uid_domain = false;
	if (prior_uid_domain && ownerinfo_is.uid_domain && MATCH != strcasecmp(ownerinfo_is.uid_domain, prior_uid_domain)) {
		ownerinfo_is.update_uid_domain = true;
	}

	if (!JobQueue->Lookup(HeaderKey, bad)) {
		// we failed to find header ad, so create one
		JobQueue->NewClassAd(HeaderKey, JOB_ADTYPE);
		CreatedAd = true;
	} else if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		std::string oldUidDomain;
		bad->LookupString(ATTR_UID_DOMAIN, oldUidDomain);
		if (oldUidDomain != scheduler.uidDomain()) {
			JobQueue->SetAttribute(HeaderKey, ATTR_UID_DOMAIN, QuoteAdStringValue(scheduler.uidDomain(), buffer));
			// when upgrading Schedds to 23.x oldUidDomain will be empty here.
			// TODO: set effective PRIOR_UID_DOMAIN value here?
		}
	}

	if (CreatedAd ||
		bad->LookupInteger(ATTR_NEXT_CLUSTER_NUM, stored_cluster_num) != 1) {
		// cluster_num is not already set, so we set a flag to set it from a
		// computed value 
		stored_cluster_num = 0;
	}

    // If a stored cluster id exceeds a configured maximum, tag it for re-computation
    if ((cluster_maximum_val > 0) && (stored_cluster_num > cluster_maximum_val)) {
        dprintf(D_ALWAYS, "Stored cluster id %d exceeds configured max %d.  Flagging for reset.\n", stored_cluster_num, cluster_maximum_val);
        stored_cluster_num = 0;
    }

		// Figure out what the correct ATTR_SCHEDULER is for any
		// dedicated jobs in this queue.  Since it'll be the same for
		// all jobs, we only have to figure it out once.  We use '%'
		// as the delimiter, since ATTR_NAME might already have '@' in
		// it, and we don't want to confuse things any further.
	formatstr( correct_scheduler, "DedicatedScheduler@%s", Name );

#ifdef USE_JOB_QUEUE_USERREC
	// add OwnerInfo to the scheduler map for JobQueueUserRec records that we just loaded
	// this will clear the pending owners collection
	if (scheduler.HasPersistentOwnerInfo()) {
		scheduler.mapPendingOwners();
	} else {
		// if we loaded any JobQueueUserRec ads, destroy them now
		auto pending_owners = scheduler.queryPendingOwners();
		if ( ! pending_owners.empty()) {
			JobQueue->BeginTransaction();
			for (auto it : pending_owners) {
				JobQueue->DestroyClassAd(it.second->jid);
			}
			JobQueue->CommitNondurableTransaction();
			scheduler.deleteZombieOwners();
		}
	}
#endif

	next_cluster_num = cluster_initial_val;
	JobQueue->StartIterateAllClassAds();
	while (JobQueue->Iterate(key,bad)) {
		bad->CheckJidAndType(key); // make sure that QueueBase object has correct jid and type fields
		if (bad->IsHeader()) { continue; }

		if (bad->IsUserRec()) {
			#ifdef USE_JOB_QUEUE_USERREC
			auto * urec = dynamic_cast<JobQueueUserRec*>(bad);
			if (urec) {
				// TODO: fix it so that job queue does not set TargetType 
				urec->Delete(ATTR_TARGET_TYPE);
				// TODO: validate ?
			}
			#endif
			continue;
		}

		// populate the ownerinfo for cluster ads and jobset ads,
		// we can't do proc ads yet because we have not yet chained them to the clusters
		if (bad->IsJobSet()) {
			auto * sad = dynamic_cast<JobQueueJobSet*>(bad);
			// we init jobset ads the first time we need them, which might be *before* we iterate them
			// so we use a null ownerinfo pointer as a signal that we havent handled this ad yet
			if ( ! sad->ownerinfo) {
				InitJobsetAd(sad, owner, ownerinfo_is);
				jobset_ids.push_back(sad->Jobset());
			}
			continue; // done with this jobset ad for the first pass
		}

		if (bad->IsCluster()) {
			auto * cad = dynamic_cast<JobQueueCluster*>(bad);
			if ( ! cad->ownerinfo) {
				InitClusterAd(cad, owner, ownerinfo_is, jobset_ids, needed_sets);
			}
			continue;  // done with this cluster ad for the first pass
		}
		if ( ! bad->IsJob()) continue;

		cluster_num = key.cluster;

		auto *ad = dynamic_cast<JobQueueJob*>(bad);

		// this brace isn't needed anymore, it's here to avoid re-indenting all of the code below.
		{
			JOB_ID_KEY_BUF job_id(key);

			// find highest cluster, set next_cluster_num to one increment higher
			if (cluster_num >= next_cluster_num) {
				next_cluster_num = cluster_num + cluster_increment_val;
			}

				// make sure that the cluster ad's jobset id and name are visible in the job ad
			ad->set_id = 0;
			ad->Delete(ATTR_JOB_SET_ID);
			ad->Delete(ATTR_JOB_SET_NAME);

				// Update fields in the newly created JobObject
			ad->autocluster_id = -1;
			ad->Delete(ATTR_AUTO_CLUSTER_ID);

			// link all proc ads to their cluster ad, if there is one
			clusterad = GetClusterAd(cluster_num);
			if (clusterad) {
				if ( ! clusterad->ownerinfo) {
					InitClusterAd(clusterad, owner, ownerinfo_is, jobset_ids, needed_sets);

					// backward compat hack.  Older versions of grid universe and job router don't populate the cluster ad
					// so if we failed to get ownerinfo, copy attributes from the proc ad and try again
					if ( ! clusterad->ownerinfo && owner.empty()) {
						if ( ! clusterad->LookupString(ATTR_USER, buffer) && ad->LookupString(ATTR_USER, buffer)) {
							clusterad->Assign(ATTR_USER, buffer);
						}
						if ( ! clusterad->LookupString(ATTR_OWNER, buffer) && ad->LookupString(ATTR_OWNER, buffer)) {
							clusterad->Assign(ATTR_OWNER, buffer);
						}
						InitClusterAd(clusterad, owner, ownerinfo_is, jobset_ids, needed_sets);
					}
				}
				clusterad->AttachJob(ad);
				clusterad->autocluster_id = -1;
				ad->ChainToAd(clusterad);
				ad->ownerinfo = clusterad->ownerinfo;
				ad->SetUniverse(clusterad->Universe());
				ad->set_id = clusterad->set_id; // cluster ad may not have the right id yet, but it's still ok to copy it
			}
			ad->PopulateFromAd();

			// sanity check some immutable attributes
			if (!ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						job_id.c_str(), ATTR_CLUSTER_ID);
				JobQueue->DestroyClassAd(job_id);
				continue;
			}

			if ((cluster != cluster_num) || ! clusterad) {
				dprintf(D_ALWAYS,
						"Job %s has invalid cluster number %d.  Removing...\n",
						job_id.c_str(), cluster);
				JobQueue->DestroyClassAd(job_id);
				continue;
			}


			// TODO: fix for USERREC_NAME_IS_FULLY_QUALIFIED
			user.clear();
			if (!ad->LookupString(ATTR_OWNER, owner) || ( !ad->LookupString(ATTR_USER, user) && user_is_the_new_owner)) {
				dprintf(D_ALWAYS,
						"Job %s has no " ATTR_OWNER " or no " ATTR_USER " attribute.  Removing....\n",
						job_id.c_str());
				JobQueue->DestroyClassAd(job_id);
				continue;
			}

			// TODO: shouldn't this be happening to the cluster ad?
			if ( ! user_is_the_new_owner) {

				// Figure out what ATTR_USER *should* be for this job
				#ifdef NO_DEPRECATE_NICE_USER
				int nice_user = 0;
				ad->LookupInteger( ATTR_NICE_USER, nice_user );
				formatstr( correct_user, "%s%s@%s",
						 (nice_user) ? "nice-user." : "", owner.c_str(),
						 scheduler.uidDomain() );
				#else
				correct_user = owner + "@" + scheduler.uidDomain();
				#endif

				if (user.empty()) {
					dprintf( D_FULLDEBUG,
							"Job %s has no %s attribute.  Inserting one now...\n",
							job_id.c_str(), ATTR_USER);
					ad->Assign( ATTR_USER, correct_user );
					JobQueueDirty = true;
				} else {
						// ATTR_USER exists, make sure it's correct, and
						// if not, insert the new value now.
					if( user != correct_user ) {
							// They're different, so insert the right value
						dprintf( D_FULLDEBUG,
								 "Job %s has stale %s attribute.  "
								 "Inserting correct value now...\n",
								 job_id.c_str(), ATTR_USER );
						ad->Assign( ATTR_USER, correct_user );
						JobQueueDirty = true;
					}
				}
			}

			if (!ad->LookupInteger(ATTR_PROC_ID, proc)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						job_id.c_str(), ATTR_PROC_ID);
				JobQueue->DestroyClassAd(job_id);
				continue;
			}

			if( !ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) ) {
				dprintf( D_ALWAYS,
						 "Job %s has no %s attribute.  Removing....\n",
						 job_id.c_str(), ATTR_JOB_UNIVERSE );
				JobQueue->DestroyClassAd( job_id );
				continue;
			}

			if( universe <= CONDOR_UNIVERSE_MIN ||
				universe >= CONDOR_UNIVERSE_MAX ) {
				dprintf( D_ALWAYS,
						 "Job %s has invalid %s = %d.  Removing....\n",
						 job_id.c_str(), ATTR_JOB_UNIVERSE, universe );
				JobQueue->DestroyClassAd( job_id );
				continue;
			}

			int job_status = 0;
			if (ad->LookupInteger(ATTR_JOB_STATUS, job_status)) {
				if (ad->Status() != job_status) {
					if (clusterad) {
						clusterad->JobStatusChanged(ad->Status(), job_status);
					}
					ad->SetStatus(job_status);
				}
				IncrementLiveJobCounter(scheduler.liveJobCounts, ad->Universe(), ad->Status(), 1);
				if (ad->ownerinfo) { IncrementLiveJobCounter(ad->ownerinfo->live, ad->Universe(), ad->Status(), 1); }
			}

				// Make sure ATTR_SCHEDULER is correct.
				// XXX TODO: Need a better way than hard-coded
				// universe check to decide if a job is "dedicated"
			if( universe == CONDOR_UNIVERSE_MPI ||
				universe == CONDOR_UNIVERSE_PARALLEL ) {
				if( !ad->LookupString(ATTR_SCHEDULER, attr_scheduler) ) { 
					dprintf( D_FULLDEBUG, "Job %s has no %s attribute.  "
							 "Inserting one now...\n", job_id.c_str(),
							 ATTR_SCHEDULER );
					ad->Assign( ATTR_SCHEDULER, correct_scheduler );
					JobQueueDirty = true;
				} else {

						// ATTR_SCHEDULER exists, make sure it's correct,
						// and if not, insert the new value now.
					if( attr_scheduler != correct_scheduler ) {
							// They're different, so insert the right
							// value 
						dprintf( D_FULLDEBUG,
								 "Job %s has stale %s attribute.  "
								 "Inserting correct value now...\n",
								 job_id.c_str(), ATTR_SCHEDULER );
						ad->Assign( ATTR_SCHEDULER, correct_scheduler );
						JobQueueDirty = true;
					}
				}
			}
			
				//
				// CronTab Special Handling Code
				// If this ad contains any of the attributes used 
				// by the crontab feature, then we will tell the 
				// schedd that this job needs to have runtimes calculated
				// 
			if ( ad->LookupString( ATTR_CRON_MINUTES, buffer ) ||
				 ad->LookupString( ATTR_CRON_HOURS, buffer ) ||
				 ad->LookupString( ATTR_CRON_DAYS_OF_MONTH, buffer ) ||
				 ad->LookupString( ATTR_CRON_MONTHS, buffer ) ||
				 ad->LookupString( ATTR_CRON_DAYS_OF_WEEK, buffer ) ) {
				scheduler.addCronTabClassAd( ad );
			}

			ConvertOldJobAdAttrs( ad, true );

				// Add the job to various runtime indexes for quick lookups
				//
			scheduler.indexAJob(ad, true);

				// If input files are going to be spooled, rewrite
				// the paths in the job ad to point at our spool area.
				// If the schedd crashes between committing a new job
				// submission and rewriting the job ad for spooling,
				// we need to redo the rewriting here.
			int hold_code = -1;
			ad->LookupInteger(ATTR_HOLD_REASON_CODE, hold_code);
			if ( job_status == HELD && hold_code == CONDOR_HOLD_CODE::SpoolingInput ) {
				if ( rewriteSpooledJobAd( ad, cluster, proc, true ) ) {
					JobQueueDirty = true;
				}
			}

				// make file transfer status attributes sane in case
				// we died while in the middle of transferring
			int transferring_input = false;
			int transferring_output = false;
			int transfer_queued = false;
			if( ad->LookupInteger(ATTR_TRANSFERRING_INPUT,transferring_input) ) {
				if( job_status == RUNNING ) {
					if( transferring_input ) {
						ad->Assign(ATTR_TRANSFERRING_INPUT,false);
						JobQueueDirty = true;
					}
				}
				else {
					ad->Delete(ATTR_TRANSFERRING_INPUT);
					JobQueueDirty = true;
				}
			}
			if( ad->LookupInteger(ATTR_TRANSFERRING_OUTPUT,transferring_output) ) {
				if( job_status == RUNNING ) {
					if( transferring_output ) {
						ad->Assign(ATTR_TRANSFERRING_OUTPUT,false);
						JobQueueDirty = true;
					}
				}
				else {
					ad->Delete(ATTR_TRANSFERRING_OUTPUT);
					JobQueueDirty = true;
				}
			}
			if( ad->LookupInteger(ATTR_TRANSFER_QUEUED,transfer_queued) ) {
				if( job_status == RUNNING ) {
					if( transfer_queued ) {
						ad->Assign(ATTR_TRANSFER_QUEUED,false);
						JobQueueDirty = true;
					}
				}
				else {
					ad->Delete(ATTR_TRANSFER_QUEUED);
					JobQueueDirty = true;
				}
			}
			if( ad->LookupString(ATTR_CLAIM_ID, buffer) ) {
				ad->Delete(ATTR_CLAIM_ID);
				PrivateAttrs[key][ATTR_CLAIM_ID] = buffer;
			}
			if( ad->LookupString(ATTR_CLAIM_IDS, buffer) ) {
				ad->Delete(ATTR_CLAIM_IDS);
				PrivateAttrs[key][ATTR_CLAIM_IDS] = buffer;
			}

			// count up number of procs in cluster, update ClusterSizeHashTable
			int num_procs = IncrementClusterSize(cluster_num);
			if (clusterad) clusterad->SetClusterSize(num_procs);
			TotalJobsCount++;
		}
	} // WHILE

#ifdef USE_JOB_QUEUE_USERREC
	// if we get to here we need to turn any pending owners into actual
	//  UserRec records in the job queue.  
	auto pending_owners = scheduler.queryPendingOwners();
	if ( ! pending_owners.empty()) {
		CreateNeededUserRecs(pending_owners);
	}
	scheduler.clearPendingOwners();
#endif

	// If JobSets enabled, scan again to create needed jobsets and add jobs into jobSets runtime structures
	if (scheduler.jobSets) {
		dprintf(D_FULLDEBUG, "Restoring JobSet state\n");

		if ( ! needed_sets.empty()) {
			// make sure that sets we "need" to create weren't just sets we had not initialized yet
			auto it = needed_sets.begin();
			for ( ; it != needed_sets.end(); ) {
				if (scheduler.jobSets->find(it->first)) {
					it = needed_sets.erase(it);
				} else {
					++it;
				}
			}
			if ( ! needed_sets.empty()) {
				// create jobsets we detected were needed during the first pass 
				dprintf(D_STATUS, "Creating %d JobSets that are needed but did not already exist\n", (int)needed_sets.size());
				CreateNeededJobsets(needed_sets, jobset_ids);
			}
		}

		unsigned int updates = 0;

		// walk the job queue again, initializing the jobset counters
		// the set_id field of the job may or may not have been set correctly
		// and the jobset objects have not yet counted anything, we do that now.
		JobQueue->StartIterateAllClassAds();
		while (JobQueue->Iterate(bad)) {
			if (!bad->IsJob() && !bad->IsCluster()) continue;

			auto * job = dynamic_cast<JobQueueJob*>(bad);
			if (job->IsJob()) {
				// in most cases, we can init the set_id for the job from the cluster header
				// because we iterate in hashtable order, this is not 100% but it's faster when it works
				job->set_id = job->Cluster()->set_id;
			}
			if (job->set_id < 0) continue;

			if (scheduler.jobSets->addToSet(*job)) {
				if (job->IsCluster()) {
					// for cluster ads, write the set_id into the ad
					job->Assign(ATTR_JOB_SET_ID, job->set_id);
				} else {
					// we only count job updates to avoid confusing things below
					++updates;
				}
			}
		}

		// now that the sets are populated, do a garbage collection pass
		int num_removed = 0;
		for (auto set_id : jobset_ids) {
			JobQueueJobSet * jobset = GetJobSetAd(set_id);
			if (scheduler.jobSets->garbageCollectSet(jobset)) {
				++num_removed;
			}
		}

		dprintf(D_STATUS, "Finished restoring JobSet state, %d new sets, %d removed sets, mapped %u jobs into %zu sets\n",
			(int)needed_sets.size(), num_removed, updates, scheduler.jobSets->count());

	} else {
		// jobsets disabled 
		if ( ! jobset_ids.empty()) {
			if (param_boolean("PURGE_JOBSETS", false)) {
				for (auto set_id : jobset_ids) {
					JobQueueJobSet * jobset = GetJobSetAd(set_id);
					if (jobset) { JobQueue->DestroyClassAd(jobset->jid); }
				}
				dprintf(D_STATUS, "JobSets disabled, removed JobSet state for %d jobsets\n", (int)jobset_ids.size());
			} else {
				dprintf(D_STATUS, "JobSets disabled, but %d exist. They will be ignored.\n", (int)jobset_ids.size());
			}
		}
	}


    // We defined a candidate next_cluster_num above, as (current-max-clust) + (increment).
    // If the candidate exceeds the configured max, then wrap it.  Default maximum is zero,
    // which signals 'no maximum'
    if ((cluster_maximum_val > 0) && (next_cluster_num > cluster_maximum_val)) {
        dprintf(D_ALWAYS, "Next cluster id exceeded configured max %d: wrapping to %d\n", cluster_maximum_val, cluster_initial_val);
        next_cluster_num = cluster_initial_val;
    }

	if ( stored_cluster_num == 0 ) {
		char tmp[PROC_ID_STR_BUFLEN];
		snprintf(tmp, sizeof(tmp), "%d", next_cluster_num);
		JobQueue->SetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, tmp);
	} else {
        // This sanity check is not applicable if a maximum cluster value was set,  
        // since in that case wrapped cluster-ids are a valid condition.
		if ((next_cluster_num > stored_cluster_num) && (cluster_maximum_val <= 0)) {
			// Oh no!  Somehow the header ad in the queue says to reuse cluster nums!
			EXCEPT("JOB QUEUE DAMAGED; header ad NEXT_CLUSTER_NUM invalid");
		}
		next_cluster_num = stored_cluster_num;
	}

		// Now check for pre-7.5.5 spool directories/files that need
		// to be modernized.  This happens in two passes.  First we
		// update the job ads to point to the new paths.  Then we
		// write out the modified job queue to save these and any
		// other pending changes.  Then we rename the
		// directories/files in the spool.  This order of doing things
		// guarantees that if we crash in the middle we don't skip
		// over some unfinished work on restart.
	std::list< PROC_ID > spool_rename_list;

	if( spool_cur_version < 1 ) {
		SpoolHierarchyChangePass1(spool.c_str(),spool_rename_list);
	}


		// Some of the conversions done in ConvertOldJobAdAttrs need to be
		// persisted to disk. Particularly, GlobusContactString/RemoteJobId.
		// The spool renaming also needs to be saved here.  This is not
		// optional, so we cannot just call CleanJobQueue() here, because
		// that does not abort on failure.
#if 0
	if( JobQueueDirty ) {
		if( !JobQueue->TruncLog() ) {
			EXCEPT("Failed to write the modified job queue log to disk, so cannot continue.");
		}
		JobQueueDirty = false;
	}
#endif

	if( spool_cur_version < 1 ) {
		SpoolHierarchyChangePass2(spool.c_str(),spool_rename_list);
	}

	if( spool_cur_version != SPOOL_CUR_VERSION_SCHEDD_SUPPORTS ) {
		WriteSpoolVersion(spool.c_str(),SPOOL_MIN_VERSION_SCHEDD_WRITES,SPOOL_CUR_VERSION_SCHEDD_SUPPORTS);
	}
}


void
CleanJobQueue(int /* tid */)
{
	if (JobQueueDirty) {
		dprintf(D_ALWAYS, "Cleaning job queue...\n");
		JobQueue->TruncLog();

		auto job_itr = PrivateAttrs.begin();
		while (job_itr != PrivateAttrs.end()) {
			ClassAd *job_ad = GetJobAd(job_itr->first);
			if (job_ad == nullptr) {
				job_itr = PrivateAttrs.erase(job_itr);
			} else {
				for (auto &attr : job_itr->second) {
					if (SetAttributeString(job_itr->first.cluster, job_itr->first.proc, attr.first.c_str(), attr.second.c_str()) == 0) {
						job_ad->Delete(attr.first.c_str());
					}
				}
				job_itr++;
			}
		}

		JobQueueDirty = false;
	}
}


void
DestroyJobQueue( )
{
	in_DestroyJobQueue = true;

	// Clean up any children that have exited but haven't been reaped
	// yet.  This can occur if the schedd receives a query followed
	// immediately by a shutdown command.  The child will exit but
	// not be reaped because the SIGTERM from the shutdown command will
	// be processed before the SIGCHLD from the child process exit.
	// Allowing the stack to clean up child processes is problematic
	// because the schedd will be shutdown and the daemonCore
	// object deleted by the time the child cleanup is attempted.
	schedd_forker.DeleteAll( );

	if (JobQueueDirty) {
			// We can't destroy it until it's clean.
		CleanJobQueue();
	}
	ASSERT( JobQueueDirty == false );
	delete JobQueue;
	JobQueue = nullptr;

	scheduler.deleteZombieOwners();
	DirtyJobIDs.clear();

		// There's also our hashtable of the size of each cluster
	delete ClusterSizeHashTable;
	ClusterSizeHashTable = nullptr;
	TotalJobsCount = 0;

	delete queue_super_user_may_impersonate_regex;
	queue_super_user_may_impersonate_regex = nullptr;
	in_DestroyJobQueue = false;
}


int MaterializeJobs(JobQueueCluster * clusterad, TransactionWatcher &txn, int & retry_delay)
{
	retry_delay = 0;
	if ( ! clusterad->factory)
		return 0;

	int proc_limit = INT_MAX;
	if ( ! clusterad->LookupInteger(ATTR_JOB_MATERIALIZE_LIMIT, proc_limit) || proc_limit <= 0) {
		proc_limit = INT_MAX;
	}
	int system_limit = MIN(scheduler.getMaxMaterializedJobsPerCluster(), scheduler.getMaxJobsPerSubmission());
	// the MAX_JOBS_PER_OWNER limit straddles clusters, so we have to look at the number of jobs in the queue
	// for this owner to know the actual limit
	int owner_limit = scheduler.getMaxJobsPerOwner();
	if (clusterad->ownerinfo) {
		owner_limit -= (clusterad->ownerinfo->num.JobsCounted + clusterad->ownerinfo->num.JobsRecentlyAdded);
	}
	// when the effective owner limit is 0, we can't count on a state change in this factory to trigger new materialization
	// so we return a retry_delay of 5 to keep this cluster in the 'needs materialization' queue
	if (owner_limit < 1) {
		dprintf(D_MATERIALIZE, "in MaterializeJobs cannot materialize jobs for cluster %d because of MAX_JOBS_PER_OWNER\n",
			clusterad->jid.cluster);
		//uncomment this line to poll quickly, but only if ownerinfo->num counters are also updated quickly (currently they are not)
		//retry_delay = 5;
	}
	//system_limit = MIN(system_limit, scheduler.getMaxJobsRunning());

	int effective_limit = MIN(proc_limit, system_limit);
	effective_limit = MIN(effective_limit, owner_limit);

	dprintf(D_MATERIALIZE | D_VERBOSE, "in MaterializeJobs, proc_limit=%d, sys_limit=%d MIN(%d,%d,%d), ClusterSize=%d\n",
		proc_limit, system_limit,
		scheduler.getMaxMaterializedJobsPerCluster(), scheduler.getMaxJobsPerSubmission(), owner_limit,
		clusterad->ClusterSize());

	int num_materialized = 0;
	int cluster_size = clusterad->ClusterSize();
	while ((cluster_size + num_materialized) < effective_limit) {
		int retry = 0;
		if ( ! CheckMaterializePolicyExpression(clusterad, num_materialized, retry)) {
			retry_delay = retry;
			break;
		}
		if (MaterializeNextFactoryJob(clusterad->factory, clusterad, txn, retry) == 1) {
			num_materialized += 1;
		} else {
			retry_delay = MAX(retry_delay, retry);
			break;
		}
	}

	return num_materialized;
}

static int OpenSpoolFactoryFile(int cluster_id, const char * filename, int &fd, const char * tag, int &terrno)
{
	fd = -1;

	terrno = 0;
	std::string parent,junk;
	if (filename_split(filename,parent,junk)) {
			// Create directory hierarchy within spool directory.
			// All sub-dirs in hierarchy are owned by condor.
		if( !mkdir_and_parents_if_needed(parent.c_str(),0755,PRIV_CONDOR) ) {
			terrno = errno;
			dprintf(D_ALWAYS,
				"Failed %s %d because create parent spool directory of '%s' failed, errno = %d (%s)\n",
					tag, cluster_id, parent.c_str(), errno, strerror(errno));
			return -1;
		}
	}

	int flags = O_WRONLY | _O_BINARY | _O_SEQUENTIAL | O_LARGEFILE | O_CREAT | O_TRUNC;

	fd = safe_open_wrapper_follow( filename, flags, 0644 );
	if (fd < 0) {
		terrno = errno;
		dprintf(D_ALWAYS, "Failing %s %d because creat of '%s' failed, errno = %d (%s)\n",
			tag, cluster_id, filename, errno, strerror(errno));
		return -1;
	}

	return 0;
}

// handle the CONDOR_SendMaterializeData RPC
// this function reads materialize item data from the sock,  writes it to SPOOL with into a file
// whose name is based on the cluster id, it then returns the filename and the number of lines that it recieved
// the materialize itemdata is *also* stored in the submit pending JobFactory so we don't have to read it from the
// spool file on initial submit.
// 
int QmgmtHandleSendMaterializeData(int cluster_id, ReliSock * sock, std::string & spooled_filename, int &row_count, int &terrno)
{
	int rval = -1;
	SetAttributeFlags_t flags = 0;
	spooled_filename.clear();
	row_count = 0;
	terrno = 0;

	// check to see if the schedd is permitting late materialization
	if ( ! scheduler.getAllowLateMaterialize()) {
		terrno = EPERM;
		dprintf(D_MATERIALIZE, "Failing remote SetMaterializeData %d because SCHEDD_ALLOW_LATE_MATERIALIZE is false\n", cluster_id);
		return -1;
	}

	if ( ! sock) {
		terrno = EINVAL;
		dprintf(D_MATERIALIZE, "Failing remote SetMaterializeData %d because no socket was supplied\n", cluster_id);
		return -1;
	}

	JobFactory * factory = nullptr;
	JobFactory*& pending = JobFactoriesSubmitPending[cluster_id];
	if (pending) {
		factory = pending;
	} else {
		// parse the submit digest and (possibly) open the itemdata file.
		factory = NewJobFactory(cluster_id, scheduler.getExtendedSubmitCommands(), scheduler.getProtectedUrlMap());
		pending = factory;
	}

	if ( ! factory) {
		terrno = ENOMEM;
		dprintf(D_MATERIALIZE, "Failing remote SendMaterializeData %d because NewFactory failed\n", cluster_id);
		return -1;
	}

	// If the submit digest parsed correctly, we need to write it to spool so we can re-load if the schedd is restarted
	//
	GetSpooledMaterializeDataPath(spooled_filename, cluster_id, Spool);
	const char * filename = spooled_filename.c_str();

	terrno = 0;
	int fd = 0;
	rval = OpenSpoolFactoryFile(cluster_id, filename, fd, "SendMaterializeData", terrno);
	if (rval == 0) {
		std::string remainder; // holds the fragment of a line that straddles buffers
		const size_t cbbuf = 0x10000; // 64kbuffer
		int cbread = 0;
		char buf[cbbuf];
		while ((cbread = sock->get_bytes(buf, cbbuf)) > 0) {
			int cbwrote = write(fd, buf, cbread);
			if (cbwrote != cbread) {
				terrno = errno;
				dprintf(D_ALWAYS, "Failing remote SendMaterializeData %d because write to '%s' failed, errno = %d (%s)\n",
					cluster_id, filename, errno, strerror(errno));
				rval = -1;
				break;
			}
			rval = AppendRowsToJobFactory(factory, buf, cbread, remainder);
			if (rval < 0) {
				terrno = EINVAL;
				break;
			}
		}
		if (close(fd)!=0) {
			if ( ! terrno) { terrno = errno; }
			dprintf(D_ALWAYS, "SendMaterializeData %d close failed, errno = %d (%s)\n", cluster_id, errno, strerror(errno));
			rval = -1;
		}
		if (rval < 0) {
			if (unlink(filename) < 0) {
				dprintf(D_FULLDEBUG, "SendMaterializeData %d failed to unlink file '%s' errno = %d (%s)\n", cluster_id, filename, errno, strerror(errno));
			}
		}
	}

	if (rval < 0) {
		dprintf(D_MATERIALIZE, "Failing remote SendMaterializeData %d because spooling of data failed : %d - %s\n", cluster_id, terrno, strerror(terrno));
	} else {
		rval = SetAttributeString( cluster_id, -1, ATTR_JOB_MATERIALIZE_ITEMS_FILE, filename, flags );
	}

	// if we failed, destroy the pending job factory and remove it from the pending list.
	if (rval < 0) {
		DestroyJobFactory(factory); factory = nullptr;
		auto found = JobFactoriesSubmitPending.find(cluster_id);
		if (found != JobFactoriesSubmitPending.end()) JobFactoriesSubmitPending.erase(found);
	} else {
		row_count = JobFactoryRowCount(factory);
	}
	return rval;
}

// handle the CONDOR_SetJobFactory RPC
// This function writes the submit digest_text into SPOOL in a file whose name is based on the cluster id
// the filename argument is not used (it was used during 8.7 development, but abandoned in favor of spooling the digest)
int QmgmtHandleSetJobFactory(int cluster_id, const char* filename, const char * digest_text)
{
	int rval = -1;
	SetAttributeFlags_t flags = 0;
	std::string errmsg;

	// check to see if the schedd is permitting late materialization 
	if ( ! scheduler.getAllowLateMaterialize()) {
		dprintf(D_MATERIALIZE, "Failing remote SetJobFactory %d because SCHEDD_ALLOW_LATE_MATERIALIZE is false\n", cluster_id);
		return -1;
	}

	if (digest_text && digest_text[0]) {

		ClassAd * user_ident = nullptr;
		JobFactory * factory = nullptr;
		JobFactory*& pending = JobFactoriesSubmitPending[cluster_id];
		if (pending) {
			factory = pending;
		} else {
			// parse the submit digest and (possibly) open the itemdata file.
			factory = NewJobFactory(cluster_id, scheduler.getExtendedSubmitCommands(), scheduler.getProtectedUrlMap());
			pending = factory;
		}

		if (factory) {
			rval = 0;
			if ( ! LoadJobFactoryDigest(factory, digest_text, user_ident, errmsg)) {
				rval = -1;
			}
		}

		if (rval < 0) {
			dprintf(D_MATERIALIZE, "Failing remote SetJobFactory %d because MakeJobFactory failed : %s\n", cluster_id, errmsg.c_str());
		} else {

			// If the submit digest parsed correctly, we need to write it to spool so we can re-load if the schedd is restarted
			//
			std::string spooled_filename;
			GetSpooledSubmitDigestPath(spooled_filename, cluster_id, Spool);
			const char * filename = spooled_filename.c_str();

			int terrno = 0;
			int fd = 0;
			rval = OpenSpoolFactoryFile(cluster_id, filename, fd, "SetJobFactory", terrno);
			if (0 == rval) {
				size_t cbtext = strlen(digest_text);
				size_t cbwrote = write(fd, digest_text, (unsigned int)cbtext);
				if (cbwrote != cbtext) {
					terrno = errno;
					dprintf(D_ALWAYS, "Failing remote SetJobFactory %d because write to '%s' failed, errno = %d (%s)\n",
						cluster_id, filename, errno, strerror(errno));
					rval = -1;
				}
				if (close(fd)!=0) {
					if ( ! terrno) { terrno = errno; }
					dprintf(D_ALWAYS, "SetJobFactory %d close failed, errno = %d (%s)\n", cluster_id, errno, strerror(errno));
					rval = -1;
				}
				if (rval < 0) {
					if (unlink(filename) < 0) {
						dprintf(D_FULLDEBUG, "SetJobFactory %d failed to unlink file '%s' errno = %d (%s)\n", cluster_id, filename, errno, strerror(errno));
					}
				}
			}
			if (rval < 0) {
				errno = terrno;
			} else {
				rval = SetAttributeString( cluster_id, -1, ATTR_JOB_MATERIALIZE_DIGEST_FILE, filename, flags );
			}
		}

		// if we failed, destroy the pending job factory and remove it from the pending list.
		if (rval < 0) {
			if (factory) { DestroyJobFactory(factory); factory = nullptr; }
			auto found = JobFactoriesSubmitPending.find(cluster_id);
			if (found != JobFactoriesSubmitPending.end()) JobFactoriesSubmitPending.erase(found);
		}
	} else if (filename && filename[0]) {
		// we no longer support this form of factory submit.
		errno = EINVAL;
		return -1;
	}

	return rval;
}


int
grow_prio_recs( int newsize )
{
	int i = 0;
	prio_rec *tmp = nullptr;

	// just return if PrioRec already equal/larger than the size requested
	if ( MAX_PRIO_REC >= newsize ) {
		return 0;
	}

	dprintf(D_FULLDEBUG,"Dynamically growing PrioRec to %d\n",newsize);

	tmp = new prio_rec[(unsigned int)newsize];
	if ( tmp == nullptr ) {
		EXCEPT( "grow_prio_recs: out of memory" );
	}

	/* copy old PrioRecs over */
	for (i=0;i<N_PrioRecs;i++)
		tmp[i] = PrioRec[i];

	/* delete old too-small space, but only if we new-ed it; the
	 * first space came from the heap, so check and don't try to 
	 * delete that */
	if ( &PrioRec[0] != &PrioRecArray[0] )
		delete [] PrioRec;

	/* replace with spanky new big one */
	PrioRec = tmp;
	MAX_PRIO_REC = newsize;
	return 0;
}


// test an unqualified username "bob" or a fully qualfied username "bob@domain" to see
// if it is a queue superuser
static bool isQueueSuperUserName( const char* user )
{
	if( !user || super_users.empty() ) {
		return false;
	}

#ifdef UNIX
	// split username into bare username and domain part
	std::string tmp;
	const char * owner = name_of_user(user, tmp);
	// by comparing the bare owner name to the original user, we determine
	// if the actual or implicit domain is equal to uidDomain.  If it is we have a local user.
	bool is_local_user = is_same_user(user, owner, COMPARE_DOMAIN_FULL, scheduler.uidDomain());
#endif

	for (const auto &superuser : super_users) {
#ifdef UNIX
        if (superuser[0] == '%' && is_local_user) {
            // this is a user group, so check user against the group membership
            struct group* gr = getgrnam(&superuser[1]);
            if (gr) {
                for (char** gmem=gr->gr_mem;  *gmem != nullptr;  ++gmem) {
                    if (strcmp(owner, *gmem) == 0) return true;
                }
            } else {
                dprintf(D_SECURITY, "Group name \"%s\" was not found in defined user groups\n", &superuser[1]);
            }
            continue;
        }
		auto opt = (CompareUsersOpt)(COMPARE_DOMAIN_PREFIX | ASSUME_UID_DOMAIN);
#else
		CompareUsersOpt opt = (CompareUsersOpt)(COMPARE_DOMAIN_PREFIX | ASSUME_UID_DOMAIN | CASELESS_USER);
#endif
		if (is_same_user(user, superuser.c_str(), opt, scheduler.uidDomain())) {
			return true;
		}
	}
	return false;
}

bool isQueueSuperUser(const JobQueueUserRec * user)
{
	if ( ! user) return false;
	if (user->IsInherentlySuper()) return true;
	if (user->isStaleConfigSuper()) {
		// TODO: fix for USERREC_NAME_IS_FULLY_QUALIFIED ?
		bool super = isQueueSuperUserName(user->Name());
		const_cast<JobQueueUserRec *>(user)->setConfigSuper(super);
	}
	return user->IsSuperUser();
}



static void
AddOwnerHistory(const std::string &user) {
	owner_history.insert(user,1);
}

static bool
SuperUserAllowedToSetOwnerTo(const std::string &user) {
		// To avoid giving the queue super user (e.g. condor)
		// the ability to run as innocent people who have never
		// even run a job, only allow them to set the owner
		// attribute of a job to a value we have seen before.
		// The JobRouter depends on this when it is running as
		// root/condor.

	if( queue_super_user_may_impersonate_regex ) {
		if( queue_super_user_may_impersonate_regex->match(user) ) {
			return true;
		}
		dprintf(D_FULLDEBUG,"Queue super user not allowed to set owner to %s, because this does not match the QUEUE_SUPER_USER_MAY_IMPERSONATE regular expression.\n",user.c_str());
		return false;
	}

	int junk = 0;
	if( owner_history.lookup(user,junk) != -1 ) {
		return true;
	}
	dprintf(D_FULLDEBUG,"Queue super user not allowed to set owner to %s, because this instance of the schedd has never seen that user submit any jobs.\n",user.c_str());
	return false;
}


int 
QmgmtSetAllowProtectedAttrChanges(int val)
{
	if( !Q_SOCK ) {
		errno = ENOENT;
		return -1;
	}

	bool old_value = 
		Q_SOCK->setAllowProtectedAttrChanges(val ? true : false);

	return old_value ? TRUE : FALSE;
}


int
QmgmtSetEffectiveOwner(char const *o)
{
	if( !Q_SOCK ) {
		errno = ENOENT;
		return -1;
	}

	char const *real_owner = Q_SOCK->getRealOwner();

#ifdef USE_JOB_QUEUE_USERREC
	std::string expanded_owner; // in case we need to re-write the effective owner name
	if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		real_owner = Q_SOCK->getRealUser();
		if ( ! real_owner || ! *real_owner) {
			real_owner = Q_SOCK->getRealOwner();
			dprintf(D_ALWAYS, "Q_SOCK has no RealUser for SetEffectiveOwner RealOwner=%s",
				real_owner ? real_owner : "(null)");
		}
	}
	const JobQueueUserRec * real_urec = scheduler.lookup_owner_const(real_owner);
	bool clear_effective = !o || !o[0]; // caller wants to clear effective
	if ( ! real_urec) {
		real_urec = real_owner_is_condor(Q_SOCK);
		if ( ! real_urec && (real_owner && *real_owner)) {
			dprintf(D_ALWAYS | D_BACKTRACE, "SetEffectiveOwner: UserRec lookup for owner %s found no match\n",
				real_owner ? real_owner : "(null)");
		}
	}

	// if an effective owner arrives with a magic domain name
	// rewrite the effective user name to use the scheduler's uid domain.
	// We do this for the benefit of the JobRouter which has no simple way
	// to know the UID_DOMAIN of the destination schedd and the default
	// UID_DOMAIN of the CE is the magic value "users.htcondor.org"
	if (o && scheduler.genericCEDomain() == domain_of_user(o, ".")) { // "." is not expected to match the CE domain
		expanded_owner = o;
		size_t at_sign = expanded_owner.find_last_of('@');
		if (at_sign != std::string::npos) {
			expanded_owner.erase(at_sign+1);
			expanded_owner += scheduler.uidDomain();
			o = expanded_owner.c_str();
		}
	}

#endif

	if( o && real_owner && is_same_user(o,real_owner,COMPARE_DOMAIN_DEFAULT,scheduler.uidDomain()) ) {
		if ( ! is_same_user(o,real_owner,COMPARE_DOMAIN_FULL,scheduler.uidDomain())) {
			dprintf(D_SECURITY, "SetEffectiveOwner security warning: "
					"assuming \"%s\" is the same as active owner \"%s\"\n",
					o, real_owner);
		}
		// change effective owner --> real owner
		o = nullptr;
	}

	if( o && !*o ) {
		// treat empty string equivalently to NULL
		o = nullptr;
	}

#ifdef USE_JOB_QUEUE_USERREC
	const JobQueueUserRec * urec = nullptr;
	if (o) {
		urec = scheduler.lookup_owner_const(o);
		if ( ! urec) {
			if (allow_submit_from_known_users_only || Q_SOCK->getReadOnly()) {
				dprintf(D_ALWAYS, "SetEffectiveOwner(): fail because no User record for %s\n", o);
				errno = EACCES;
				return -1;
			} else {
				// create user a user record for a new submitter
				// the insert_owner_const will make a pending user record
				// which we then add to the current transaction by calling MakeUserRec
				urec = scheduler.insert_owner_const(o);
				if ( ! MakeUserRec(urec, true, &scheduler.getUserRecDefaultsAd())) {
					dprintf(D_ALWAYS, "SetEffectiveOwner(): failed to create new User record for %s\n", o);
					errno = EACCES;
					return -1;
				}
			}
		}
		if (urec == real_urec) {
			// myself, but without superuser perms. I'll allow it.
		} else {
			std::string buf;
			bool is_super = real_urec && real_urec->IsSuperUser();
			bool is_allowed_owner = SuperUserAllowedToSetOwnerTo(name_of_user(o, buf));

			dprintf(D_SECURITY, "QmgmtSetEffectiveOwner real=%s%s is%s allowed to set effective to %s\n",
				real_urec ? real_urec->Name() : "(null)",
				is_super ? " (super)" : "",
				is_allowed_owner ? "" : " not",
				urec ? urec->Name() : "(null)"
				);

			if( !is_super || !is_allowed_owner)
			{
				if ( ! is_allowed_owner) {
					dprintf(D_ALWAYS, "SetEffectiveOwner security violation: "
						"attempting to set owner to dis-allowed value %s\n", o);
				} else {
					dprintf(D_ALWAYS, "SetEffectiveOwner security violation: "
						"setting owner to %s when active owner is non-superuser \"%s\"\n",
						o, real_owner ? real_owner : "(null)");
				}
				errno = EACCES;
				return -1;
			}
		}
	} else if ( ! clear_effective) {
		urec = real_urec;
	}

	if( !Q_SOCK->setEffectiveOwner(urec, ! clear_effective)) {
		errno = EINVAL;
		return -1;
	}
#else
	// always allow request to set effective owner to NULL,
	// because this means set effective owner --> real owner
	if( o && !qmgmt_all_users_trusted ) {
		bool is_super = isQueueSuperUserName(real_owner);
		bool is_allowed_owner = SuperUserAllowedToSetOwnerTo( o );
		if( !is_super || !is_allowed_owner)
		{
			if ( ! is_allowed_owner) {
				dprintf(D_ALWAYS, "SetEffectiveOwner security violation: "
						"attempting to set owner to dis-allowed value %s\n", o);
			} else {
				dprintf(D_ALWAYS, "SetEffectiveOwner security violation: "
						"setting owner to %s when active owner is non-superuser \"%s\"\n",
						o, real_owner ? real_owner : "(null)");
			}
			errno = EACCES;
			return -1;
		}
	}

	if( !Q_SOCK->setEffectiveOwner( o ) ) {
		errno = EINVAL;
		return -1;
	}
#endif
	return 0;
}


#ifdef USE_JOB_QUEUE_USERREC

// Test if this owner matches my owner, so they're allowed to update me.
bool
UserCheck(const JobQueueBase *ad, const JobQueueUserRec *test_owner)
{
	return UserCheck2(ad, test_owner);
}

bool
UserCheck2(const JobQueueBase *ad, const JobQueueUserRec * test_user, bool not_super /*=false*/)
{
	if (Q_SOCK && Q_SOCK->getReadOnly()) {
		dprintf( D_FULLDEBUG, "OwnerCheck: reject read-only client\n" );
		errno = EACCES;
		return false;
	}

	// in the very rare event that the admin told us all users 
	// can be trusted, let it pass
	if ( qmgmt_all_users_trusted ) {
		return true;
	}

	// If test_owner is NULL, then we have no idea who the user is.  We
	// do not allow anonymous folks to mess around with the queue, so 
	// have UserCheck fail.  Note we only call UserCheck in the first place
	// if Q_SOCK is not null; if Q_SOCK is null, then the schedd is calling
	// a QMGMT command internally which is allowed.
	if ( ! test_user) {
		dprintf(D_ALWAYS,
			"QMGT command failed: anonymous user not permitted\n" );
		return false;
	}

	// If we don't have an Owner/User attribute (or classad) and we've
	// gotten this far, how can we deny service?
	if( !ad ) {
		dprintf(D_FULLDEBUG,"OwnerCheck success, '%s' no ad\n", test_user->Name());
		return true;
	}
	const JobQueueUserRec * job_user = nullptr;
	if (ad->IsCluster() || ad->IsJob()) {
		job_user = dynamic_cast<const JobQueueJob*>(ad)->ownerinfo;
	} else if (ad->IsJobSet()) {
		job_user = dynamic_cast<const JobQueueJobSet*>(ad)->ownerinfo;
	} else if (ad->IsUserRec()) {
		// when modifying UserRecs,  we allow self-modify when the not_super flag is passed
		// but only super-users can modify otherwise
		if (not_super) {
			job_user = dynamic_cast<const JobQueueUserRec*>(ad);
		} else if (isQueueSuperUser(test_user)) {
			dprintf(D_FULLDEBUG, "OwnerCheck success, '%s' is super_user UID_DOMAIN=%s\n",
				test_user->Name(), scheduler.uidDomain());
			return true;
		} else {
			dprintf(D_FULLDEBUG, "OwnerCheck reject, '%s' is NOT super_user UID_DOMAIN=%s\n",
				test_user->Name(), scheduler.uidDomain());
			errno = EACCES;
			return false;
		}
	}

	if ( ! job_user) {
		dprintf(D_FULLDEBUG,"OwnerCheck success, '%s' no ad owner\n", test_user->Name());
		return true;
	}

	if (job_user == test_user) {
		//dprintf(D_FULLDEBUG,"OwnerCheck success, '%s' matches ad owner '%s'\n", test_user->Name(), job_user->Name());
		return true;
	}

	if ( ! not_super && isQueueSuperUser(test_user)) {
		dprintf(D_FULLDEBUG, "OwnerCheck success, '%s' is super_user UID_DOMAIN=%s\n",
			test_user->Name(), scheduler.uidDomain());
		return true;
	}

	dprintf(D_FULLDEBUG, "OwnerCheck reject, '%s' not ad owner: '%s' UID_DOMAIN=%s\n",
		test_user->Name(), job_user->Name(), scheduler.uidDomain());
	errno = EACCES;
	return false;
}


#else

// Test if this owner matches my owner, so they're allowed to update me.
bool
UserCheck(const ClassAd *ad, const char *test_owner)
{
	if ( Q_SOCK->getReadOnly() ) {
		errno = EACCES;
		dprintf( D_FULLDEBUG, "OwnerCheck: reject read-only client\n" );
		return false;
	}

	// check if the IP address of the peer has daemon core write permission
	// to the schedd.  we have to explicitly check here because all queue
	// management commands come in via one sole daemon core command which
	// has just READ permission.
	condor_sockaddr addr = Q_SOCK->endpoint();
	if ( !Q_SOCK->isAuthorizationInBoundingSet("WRITE") ||
		daemonCore->Verify("queue management", WRITE, addr, Q_SOCK->getRealUser()) == FALSE )
	{
		// this machine does not have write permission; return failure
		return false;
	}

	return UserCheck2(ad, test_owner);
}


bool
UserCheck2(const ClassAd *ad, const char *test_user, const char *job_user)
{
	// in the very rare event that the admin told us all users 
	// can be trusted, let it pass
	if ( qmgmt_all_users_trusted ) {
		return true;
	}

	// If test_owner is NULL, then we have no idea who the user is.  We
	// do not allow anonymous folks to mess around with the queue, so 
	// have UserCheck fail.  Note we only call UserCheck in the first place
	// if Q_SOCK is not null; if Q_SOCK is null, then the schedd is calling
	// a QMGMT command internally which is allowed.
	if ( ! test_user || ! test_user[0]) {
		dprintf(D_ALWAYS,
				"QMGT command failed: anonymous user not permitted\n" );
		return false;
	}

	std::string owner_buf;

#if !defined(WIN32) 
		// If we're not root or condor, only allow qmgmt writes from
		// the UID we're running as.
	uid_t 	my_uid = get_my_uid();
	if( my_uid != 0 && my_uid != get_real_condor_uid() ) {
		// if a fully-qualified username was passed, extract the name
		const char * test_owner = name_of_user(test_user, owner_buf);
		if( strcmp(get_real_username(), test_owner) == MATCH ) {
			dprintf(D_FULLDEBUG, "OwnerCheck success, '%s' matches my username\n", test_owner );
			return true;
		} else if (isQueueSuperUser(test_user)) {
			dprintf(D_FULLDEBUG, "OwnerCheck success, '%s' is super_user\n", test_user);
			return true;
		} else {
			errno = EACCES;
			dprintf( D_FULLDEBUG, "OwnerCheck reject, '%s' not '%s' or super_user\n",
					 test_owner, get_real_username());
			return false;
		}
	}
#endif

		// If we don't have an Owner/User attribute (or classad) and we've
		// gotten this far, how can we deny service?
	if( !job_user ) {
		if( !ad ) {
			dprintf(D_FULLDEBUG,"OwnerCheck success, '%s' no ad\n", test_user);
			return true;
		}
		else if ( ! ad->LookupString(ATTR_USER, owner_buf) &&
			      ! ad->LookupString(ATTR_OWNER, owner_buf)) {
			dprintf(D_FULLDEBUG,"OwnerCheck success, '%s' no ad owner\n", test_user);
			return true;
		}
		job_user = owner_buf.c_str();
	}

		// If the job user is "nice-user.foo@bar", then we pass the permission
		// check for test user "foo@bar".
	if (MATCH == strncmp(job_user, "nice-user.", 10)) {
		job_user += 10;
		return true;
	}

		// Finally, compare the owner of the ad with the entity trying
		// to connect to the queue.
#ifdef WIN32
	CompareUsersOpt opt = (CompareUsersOpt)(COMPARE_DOMAIN_PREFIX | ASSUME_UID_DOMAIN | CASELESS_USER);
#else
	CompareUsersOpt opt = COMPARE_DOMAIN_DEFAULT;
#endif
	if (is_same_user(test_user, job_user, opt, scheduler.uidDomain())) {
		return true;
	}
	if (ignore_domain_for_OwnerCheck) {
		// if it doesn't match fully qualified, try again ignoring the domain.
		opt = (CompareUsersOpt)(COMPARE_IGNORE_DOMAIN | (opt & CASELESS_USER));
		if (is_same_user(test_user, job_user, opt, scheduler.uidDomain())) {
			if (warn_domain_for_OwnerCheck) {
				dprintf(D_FULLDEBUG, "OwnerCheck success, but '%s' is not ad owner: '%s' UID_DOMAIN=%s. Future HTCondor versions will not ignore domain.\n",
					test_user, job_user, scheduler.uidDomain());
			}
			return true;
		}
	}

	if (isQueueSuperUser(test_user)) {
		dprintf(D_FULLDEBUG, "OwnerCheck success, '%s' is super_user UID_DOMAIN=%s\n",
		        test_user, scheduler.uidDomain());
		return true;
	}

	errno = EACCES;
	dprintf(D_FULLDEBUG, "OwnerCheck reject, '%s' not ad owner: '%s' UID_DOMAIN=%s\n",
	        test_user, job_user, scheduler.uidDomain());
	return false;
}

#endif

QmgmtPeer*
getQmgmtConnectionInfo()
{
	QmgmtPeer* ret_value = nullptr;

	// put all qmgmt state back into QmgmtPeer object for safe keeping
	if ( Q_SOCK ) {
		Q_SOCK->next_proc_num = next_proc_num;
		Q_SOCK->active_cluster_num = active_cluster_num;	
		Q_SOCK->xact_start_time = xact_start_time;
			// our call to getActiveTransaction will clear it out
			// from the lower layers after returning the handle to us
		Q_SOCK->transaction  = JobQueue->getActiveTransaction();

		ret_value = Q_SOCK;
		Q_SOCK = nullptr;
		unsetQmgmtConnection();
	}

	return ret_value;
}

bool
setQmgmtConnectionInfo(QmgmtPeer *peer)
{
	bool ret_value = false;

	if (Q_SOCK) {
		return false;
	}

	// reset all state about our past connection to match
	// what was stored in the QmgmtPeer object
	Q_SOCK = peer;
	next_proc_num = peer->next_proc_num;
	active_cluster_num = peer->active_cluster_num;	
	xact_start_time = peer->xact_start_time;
		// Note: if setActiveTransaction succeeds, then peer->transaction
		// will be set to NULL.   The JobQueue does this to prevent anyone
		// from messing with the transaction cache while it is active.
	ret_value = JobQueue->setActiveTransaction( peer->transaction );

	return ret_value;
}

void
unsetQmgmtConnection()
{
	static QmgmtPeer reset;	// just used for reset/inital values

		// clear any in-process transaction via a call to AbortTransaction.
		// Note that this is effectively a no-op if getQmgmtConnectionInfo()
		// was called previously, since getQmgmtConnectionInfo() clears 
		// out the transaction after returning the handle.
	AbortTransaction();

	ASSERT(Q_SOCK == nullptr);

	next_proc_num = reset.next_proc_num;
	active_cluster_num = reset.active_cluster_num;	
	xact_start_time = reset.xact_start_time;
}



/* We want to be able to call these things even from code linked in which
   is written in C, so we make them C declarations
*/
extern "C++" {

bool
setQSock( ReliSock* rsock)
{
	// initialize per-connection variables.  back in the day this
	// was essentially InvalidateConnection().  of particular 
	// importance is setting Q_SOCK... this tells the rest of the QMGMT
	// code the request is from an external user instead of originating
	// from within the schedd itself.
	// If rsock is NULL, that means the request is internal to the schedd
	// itself, and thus anything goes!

	bool ret_val = false;

	if (Q_SOCK) {
		delete Q_SOCK;
		Q_SOCK = nullptr;
	}
	unsetQmgmtConnection();

		// setQSock(NULL) == unsetQSock() == unsetQmgmtConnection(), so...
	if ( rsock == nullptr ) {
		return true;
	}

	auto* p = new QmgmtPeer;
	ASSERT(p);

	if ( p->set(rsock) ) {
		// success
		ret_val =  setQmgmtConnectionInfo(p);
	} 

	if ( ret_val ) {
		return true;
	} else {
		delete p;
		return false;
	}
}


void
unsetQSock()
{
	// reset the per-connection variables.  of particular 
	// importance is setting Q_SOCK back to NULL. this tells the rest of 
	// the QMGMT code the request originated internally, and it should
	// be permitted (i.e. we only call OwnerCheck if Q_SOCK is not NULL).
	if ( Q_SOCK ) {
		delete Q_SOCK;
		Q_SOCK = nullptr;
	}
	unsetQmgmtConnection();  
}


int
handle_q(int cmd, Stream *sock)
{
	int	rval = 0;
	bool all_good = false;

	all_good = setQSock(dynamic_cast<ReliSock*>(sock));

		// if setQSock failed, unset it to purge any old/stale
		// connection that was never cleaned up, and try again.
	if ( !all_good ) {
		unsetQSock();
		all_good = setQSock(dynamic_cast<ReliSock*>(sock));
	}
	if (!all_good && sock) {
		// should never happen
		EXCEPT("handle_q: Unable to setQSock!!");
	}
	ASSERT(Q_SOCK);

	Q_SOCK->initAuthOwner(cmd == QMGMT_READ_CMD);

	BeginTransaction();

	ForkStatus fork_status = FORK_FAILED;

	if (cmd == QMGMT_READ_CMD) {
		fork_status = schedd_forker.NewJob();
	}

	if (fork_status != FORK_PARENT) {
		do {
			/* Probably should wrap a timer around this */
			rval = do_Q_request(*Q_SOCK);
		} while(rval >= 0);
	}

	unsetQSock();

	if( fork_status == FORK_CHILD ) {
		dprintf(D_FULLDEBUG, "QMGR forked query done\n");
		schedd_forker.WorkerDone(); // never returns
		EXCEPT("ForkWork::WorkDone() returned!");
	}
	else if( fork_status == FORK_PARENT ) {
		dprintf(D_FULLDEBUG, "QMGR forked query\n");
	}
	else {
		dprintf(D_FULLDEBUG, "QMGR Connection closed\n");
	}

	// Abort any uncompleted transaction.  The transaction should
	// be committed in CloseConnection().
	AbortTransactionAndRecomputeClusters();

	return 0;
}

int
NewCluster(CondorError* errstack)
{
#ifdef USE_JOB_QUEUE_USERREC
	// Nothing to do here
#else
	if( Q_SOCK && !UserCheck(NULL, EffectiveUser(Q_SOCK) ) ) {
		dprintf( D_FULLDEBUG, "NewCluser(): UserCheck failed\n" );
		errno = EACCES;
		return -1;
	}
#endif


	int total_jobs = TotalJobsCount;

		//
		// I have seen a weird bug where the getMaxJobsSubmitted() returns
		// zero. I added extra information to the dprintf statement 
		// to help me try to track down when this happens
		// Andy - 06.12.2006
		//
	int maxJobsSubmitted = scheduler.getMaxJobsSubmitted();
	if ( total_jobs >= maxJobsSubmitted ) {
		dprintf(D_ALWAYS,
			"NewCluster(): MAX_JOBS_SUBMITTED exceeded, submit failed. "
			"Current total is %d. Limit is %d\n",
			total_jobs, maxJobsSubmitted );
		if (errstack) errstack->pushf("SCHEDD",EINVAL,"MAX_JOBS_SUBMITTED exceeded. Current=%d, Limit=%d", total_jobs, maxJobsSubmitted);
		errno = EINVAL;
		return NEWJOB_ERR_MAX_JOBS_SUBMITTED;
	}

#ifdef USE_JOB_QUEUE_USERREC
	// if we have not seen this user before, add a JobQueueUserRec for them
	// if we *have* seen the user before, check to see if the user is enabled
	// note that the sock may have an EffectiveUserRec, but at this point it might
	// be set to the *real* UserRec, so we want to use the EffectiveUserName here.
	if (Q_SOCK) {
		const char * user = EffectiveUserName(Q_SOCK);
		if (user && user[0]) {
			// lookup JobQueueUserRec, possibly adding a new UserRec to the current transaction
			// if we create one here, it will be pending until CommitTransactionInternal
			auto urec = scheduler.lookup_owner_const(user);
			if ( ! urec) {
				if (allow_submit_from_known_users_only) {
					dprintf(D_ALWAYS, "NewCluster(): fail because no User record for %s\n", user);
					if (errstack) {
						errstack->pushf("SCHEDD",EACCES,"Unknown User %s. Use condor_qusers to add user before submitting jobs.", user);
					}
					errno = EACCES;
					return NEWJOB_ERR_DISABLED_USER;
				} else {
					// create user a user record for a new submitter
					// the insert_owner_const will make a pending user record
					// which we then add to the current transaction by calling MakeUserRec
					urec = scheduler.insert_owner_const(user);
					if ( ! MakeUserRec(urec, true, &scheduler.getUserRecDefaultsAd())) {
						dprintf(D_ALWAYS, "NewCluster(): failed to create new User record for %s\n", user);
						errno = EACCES;
						return -1;
					}
					// attach urec to Q_SOCK so we can use it for permission and various limit checks later
					if ( ! Q_SOCK->UserRec()) { Q_SOCK->attachNewUserRec(urec); }
				}
			} else if ( ! urec->IsEnabled()) {
				// We only check to see if the User is disabled if we are not creating it automatically with the submit
				std::string reason;
				urec->LookupString(ATTR_DISABLE_REASON, reason);

				dprintf(D_ALWAYS, "NewCluster(): rejecting attempt by disabled user %s to submit a job. %s\n", urec->Name(), reason.c_str());
				if (errstack) {
					if (!reason.empty()) {
						errstack->pushf("SCHEDD",EACCES,"User %s is disabled : %s", urec->Name(), reason.c_str());
					} else {
						errstack->pushf("SCHEDD",EACCES,"User %s is disabled", urec->Name());
					}
				}
				errno = EACCES;
				return NEWJOB_ERR_DISABLED_USER;
			}
			ASSERT(urec);
		}
	}
#endif

	next_proc_num = 0;
	active_cluster_num = next_cluster_num;
	next_cluster_num += cluster_increment_val;

    // check for wrapping if a maximum cluster id is set
    if ((cluster_maximum_val > 0) && (next_cluster_num > cluster_maximum_val)) {
        dprintf(D_ALWAYS, "NewCluster(): Next cluster id %d exceeded configured max %d.  Wrapping to %d.\n", next_cluster_num, cluster_maximum_val, cluster_initial_val);
        next_cluster_num = cluster_initial_val;
    }

    // check for collision with an existing cluster id
    JobQueueKeyBuf test_cluster_key;
    ClassAd* test_cluster_ad = nullptr;
    IdToKey(active_cluster_num,-1,test_cluster_key);
    if (JobQueue->LookupClassAd(test_cluster_key, test_cluster_ad)) {
        dprintf(D_ALWAYS, "NewCluster(): collision with existing cluster id %d\n", active_cluster_num);
        errno = EINVAL;
        return NEWJOB_ERR_INTERNAL;
    }

	char cluster_str[PROC_ID_STR_BUFLEN];
	snprintf(cluster_str, sizeof(cluster_str), "%d", next_cluster_num);
	JobQueue->SetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);

	// put a new classad in the transaction log to serve as the cluster ad
	JobQueueKeyBuf cluster_key;
	IdToKey(active_cluster_num,-1,cluster_key);
	JobQueue->NewClassAd(cluster_key, JOB_ADTYPE);

	return active_cluster_num;
}

// this function is called by external submitters.
// it enforces procs can only be created in the current cluster
// and establishes the owner attribute of the cluster
//
int
NewProc(int cluster_id)
{
	int				proc_id = 0;

	if( Q_SOCK && !UserCheck(nullptr, EffectiveUserRec(Q_SOCK) ) ) {
		errno = EACCES;
		return -1;
	}

	if ((cluster_id != active_cluster_num) || (cluster_id < 1)) {
#if !defined(WIN32)
		errno = EACCES;
#endif
		return -1;
	}

	if ( (TotalJobsCount + jobs_added_this_transaction) >= scheduler.getMaxJobsSubmitted() ) {
		dprintf(D_ALWAYS,
			"NewProc(): MAX_JOBS_SUBMITTED exceeded, submit failed\n");
		errno = EINVAL;
		return NEWJOB_ERR_MAX_JOBS_SUBMITTED;
	}


	// It is a security violation to change ATTR_USER or ATTR_OWNER to
	// something other than Q_SOCK->getFullyQualifiedUser() /
	// Q_SOCK->getOwner(), so as long condor_submit and
	// Q_SOCK->getFullyQualifiedUser() / Q_SOCK->getOwner() agree on who
	// the user / owner is (or until we change the schedd so that /it/
	// sets the Owner string), it's safe to do this rather than complicate
	// things by using the owner attribute from the job ad we don't have yet.
	//
	// The queue super-user(s) can pretend to be someone, in which case it's
	// valid and sensible to apply that someone's quota; or they could change
	// the owner attribute after the submission.  The latter would allow them
	// to bypass the job quota, but that's not necessarily a bad thing.
	if (Q_SOCK) {
		const char * owner = EffectiveUserName(Q_SOCK);
		if( owner == nullptr || ! owner[0] ) {
			// This should only happen for job submission via SOAP, but
			// it's unclear how we can verify that.  Regardless, if we
			// don't know who the owner of the job is, we can't enfore
			// MAX_JOBS_PER_OWNER.
			dprintf( D_FULLDEBUG, "Not enforcing MAX_JOBS_PER_OWNER for submit without owner of cluster %d.\n", cluster_id );
		} else {
			// NOTE: when user_is_the_new_owner is true, MAX_JOBS_PER_OWNER is keyed on ATTR_USER
			const OwnerInfo * ownerInfo = scheduler.insert_owner_const( owner );
			ASSERT( ownerInfo != nullptr );
			int ownerJobCount = ownerInfo->num.JobsCounted
								+ ownerInfo->num.JobsRecentlyAdded
								+ jobs_added_this_transaction;

			int maxJobsPerOwner = scheduler.getMaxJobsPerOwner();
			if( ownerJobCount >= maxJobsPerOwner ) {
				dprintf( D_ALWAYS,
					"NewProc(): MAX_JOBS_PER_OWNER exceeded for %s, submit failed.  "
					"Current total is %d.  Limit is %d.\n",
					owner, ownerJobCount, maxJobsPerOwner );
				errno = EINVAL;
				return NEWJOB_ERR_MAX_JOBS_PER_OWNER;
			}
		}
	}

	int maxJobsPerSubmission = scheduler.getMaxJobsPerSubmission();
	if( jobs_added_this_transaction >= maxJobsPerSubmission ) {
		dprintf( D_ALWAYS,
			"NewProc(): MAX_JOBS_PER_SUBMISSION exceeded, submit failed.  "
			"Current total is %d.  Limit is %d.\n",
			jobs_added_this_transaction, maxJobsPerSubmission );
		errno = EINVAL;
		return NEWJOB_ERR_MAX_JOBS_PER_SUBMISSION;
	}

	proc_id = next_proc_num++;
	IncrementClusterSize(cluster_id);
	return NewProcInternal(cluster_id, proc_id);
}

// this function is called by external submitters and by the schedd during late materialization
// it assumes that the procid and owner have already been validated, it sets the global job id
// adtype and other things common to both external submission and interal materialization.
//
int NewProcInternal(int cluster_id, int proc_id)
{
	// We can't increase ownerData->num.JobsRecentlyAdded here because we
	// don't know, at this point, that the overall transaction will succeed.
	// Instead, track how many jobs we're adding this transaction.
	++jobs_added_this_transaction;


	JobQueueKeyBuf key;
	IdToKey(cluster_id,proc_id,key);
	JobQueue->NewClassAd(key, JOB_ADTYPE);

	job_queued_count += 1;

	// can't increment the JobsSubmitted count for other pools yet
	scheduler.OtherPoolStats.DeferJobsSubmitted(cluster_id, proc_id);

		// now that we have a real job ad with a valid proc id, then
		// also insert the appropriate GlobalJobId while we're at it.
	std::string gjid = "\"";
	gjid += Name;             // schedd's name
	gjid += "#";
	gjid += std::to_string( cluster_id );
	gjid += ".";
	gjid += std::to_string( proc_id );
	if (param_boolean("GLOBAL_JOB_ID_WITH_TIME", true)) {
		int now = (int)time(nullptr);
		gjid += "#";
		gjid += std::to_string( now );
	}
	gjid += "\"";
	JobQueue->SetAttribute(key, ATTR_GLOBAL_JOB_ID, gjid.c_str());

	return proc_id;
}

// struct for a table mapping attribute names to a flag indicating that the attribute
// must only be in cluster ad or only in proc ad, or can be either.
//
typedef struct attr_force_pair {
	const char * key;
	int          forced; // -1=forced cluster, 0=not forced, 1=forced proc
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const struct attr_force_pair& rhs) const {
		return strcasecmp(this->key, rhs.key) < 0;
	}
} ATTR_FORCE_PAIR;

// table defineing attributes that must be in either cluster ad or proc ad
// force value of 1 is proc ad, -1 is cluster ad.
// NOTE: this table MUST be sorted by case-insensitive attribute name
#define FILL(attr,force) { attr, force }
static const ATTR_FORCE_PAIR aForcedSetAttrs[] = {
	FILL(ATTR_CLUSTER_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_NAME,       -1), // forced into cluster ad
	FILL(ATTR_JOB_STATUS,         1),  // forced into proc ad
	FILL(ATTR_JOB_UNIVERSE,       -1), // forced into cluster ad
	FILL(ATTR_NT_DOMAIN,          -1), // forced into cluster ad
	FILL(ATTR_OWNER,              -1), // forced into cluster ad
	FILL(ATTR_PROC_ID,            1),  // forced into proc ad
	FILL(ATTR_USER,              -1), // forced into cluster ad
};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
static int IsForcedProcAttribute(const char *attr)
{
	const ATTR_FORCE_PAIR* found = nullptr;
	found = BinaryLookup<ATTR_FORCE_PAIR>(
		aForcedSetAttrs,
		COUNTOF(aForcedSetAttrs),
		attr, strcasecmp);
	if (found) {
		return found->forced;
	}
	return 0;
}

#if 1 // this version works with a const classad::ClassAd, like what we get off the wire.

//PRAGMA_REMIND("TODO: fix submit_utils to use the classad cache and/or fast parsing?")

// Call NewProcInternal, then SetAttribute for each of the attributes in job that are not the
// same as the attributre in the given ClusterAd
int NewProcFromAd (const classad::ClassAd * ad, int ProcId, JobQueueCluster * ClusterAd, SetAttributeFlags_t flags)
{
	int ClusterId = ClusterAd->jid.cluster;

	int num_procs = IncrementClusterSize(ClusterId);
	ClusterAd->SetClusterSize(num_procs);
	NewProcInternal(ClusterId, ProcId);

	int rval = SetAttributeInt(ClusterId, ProcId, ATTR_PROC_ID, ProcId, flags | SetAttribute_LateMaterialization);
	if (rval < 0) {
		dprintf(D_ALWAYS, "ERROR: Failed to set ProcId=%d for job %d.%d (%d)\n", ProcId, ClusterId, ProcId, errno );
		return rval;
	}

	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );
	std::string buffer;

	// the EditedClusterAttrs attribute of the cluster ad is the list of attibutes where the value in the cluster ad
	// should win over the value in the proc ad because the cluster ad was edited after submit time.
	// we do this so that "condor_qedit <clusterid>" will affect ALL jobs in that cluster, not just those that have
	// already been materialized.
	classad::References clusterAttrs;
	if (ClusterAd->LookupString(ATTR_EDITED_CLUSTER_ATTRS, buffer)) {
		add_attrs_from_string_tokens(clusterAttrs, buffer);
	}

	for (const auto & it : *ad) {
		const std::string & attr = it.first;
		const ExprTree * tree = it.second;
		if (attr.empty() || ! tree) {
			dprintf(D_ALWAYS, "ERROR: Null attribute name or value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		}

		// check if attribute is forced to either cluster ad (-1) or proc ad (1)
		bool send_it = false;
		int forced = IsForcedProcAttribute(attr.c_str());
		if (forced) {
			send_it = forced > 0;
		} else if (clusterAttrs.find(attr) != clusterAttrs.end()) {
			send_it = false; // this is a forced cluster attribute
		} else {
			// If we aren't going to unconditionally send it, when we check if the value
			// matches the value in the cluster ad.
			// If the values match, don't add the attribute to this proc ad
			ExprTree *cluster_tree = ClusterAd->Lookup(attr);
			send_it = ! cluster_tree || ! (*tree == *cluster_tree || *tree == *SkipExprEnvelope(cluster_tree));
		}
		if ( ! send_it)
			continue;

		buffer.clear();
		unparser.Unparse(buffer, tree);
		if (buffer.empty()) {
			dprintf(D_ALWAYS, "ERROR: Null value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		} else {
			rval = SetAttribute(ClusterId, ProcId, attr.c_str(), buffer.c_str(), flags | SetAttribute_LateMaterialization);
			if (rval < 0) {
				dprintf(D_ALWAYS, "ERROR: Failed to set %s=%s for job %d.%d (%d)\n", attr.c_str(), buffer.c_str(), ClusterId, ProcId, errno );
				return rval;
			}
		}
	}

	return 0;
}

#else

// Call NewProcInternal, then SetAttribute for each of the attributes in job that are not the
// same as the attributre in the given ClusterAd
int NewProcFromAd (ClassAd * job, int ProcId, JobQueueJob * ClusterAd, SetAttributeFlags_t flags)
{
	int ClusterId = ClusterAd->jid.cluster;

	NewProcInternal(ClusterId, ProcId);

	int rval = SetAttributeInt(ClusterId, ProcId, ATTR_PROC_ID, ProcId, flags | SetAttribute_LateMaterialization);
	if (rval < 0) {
		dprintf(D_ALWAYS, "ERROR: Failed to set ProcId=%d for job %d.%d (%d)\n", ProcId, ClusterId, ProcId, errno );
		return rval;
	}

	ExprTree *tree = NULL;
	const char *attr;
	std::string buffer;

	for ( auto itr = job->begin(); itr != job->end(); itr++ ) {
		attr = itr->first.c_str();
		tree = itr->second;
		if ( ! attr || ! tree) {
			dprintf(D_ALWAYS, "ERROR: Null attribute name or value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		}

		bool send_it = false;
		int forced = IsForcedProcAttribute(attr);
		if (forced) {
			send_it = forced > 0;
		} else {
			// If we aren't going to unconditionally send it, when we check if the value
			// matches the value in the cluster ad.
			// If the values match, don't add the attribute to this proc ad
			ExprTree *cluster_tree = ClusterAd->LookupExpr(attr);
			send_it = ! cluster_tree || ! (*tree == *cluster_tree);
		}
		if ( ! send_it)
			continue;

		buffer.clear();
		const char * rhstr = ExprTreeToString(tree, buffer);
		if ( ! rhstr ) { 
			dprintf(D_ALWAYS, "ERROR: Null value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		} else {
			rval = SetAttribute(ClusterId, ProcId, attr, rhstr, flags | SetAttribute_LateMaterialization);
			if (rval < 0) {
				dprintf(D_ALWAYS, "ERROR: Failed to set %s=%s for job %d.%d (%d)\n", attr, rhstr, ClusterId, ProcId, errno );
				return rval;
			}
		}
	}

	return 0;
}

#endif // 0

int DestroyProc(int cluster_id, int proc_id)
{
	JobQueueKeyBuf		key;
	JobQueueJob			*ad = nullptr;

	// cannot destroy any SpecialJobIds like the header ad 0.0
	if ( IsSpecialJobId(cluster_id,proc_id) ) {
		errno = EINVAL;
		return DESTROYPROC_ERROR;
	}

	IdToKey(cluster_id,proc_id,key);
	if (!JobQueue->Lookup(key, ad)) {
		errno = ENOENT;
		return DESTROYPROC_ENOENT;
	}

	// Only the owner can delete a proc.
	if ( Q_SOCK && !UserCheck(ad, EffectiveUserRec(Q_SOCK) )) {
		errno = EACCES;
		return DESTROYPROC_EACCES;
	}

	// Take care of ATTR_COMPLETION_DATE
	int job_status = -1;
	ad->LookupInteger(ATTR_JOB_STATUS, job_status);
	if ( job_status == COMPLETED ) {
			// if job completed, insert completion time if not already there
		int completion_time = 0;
		ad->LookupInteger(ATTR_COMPLETION_DATE,completion_time);
		if ( !completion_time ) {
			SetAttributeInt(cluster_id,proc_id,ATTR_COMPLETION_DATE,
			                time(nullptr), true /*nondurable*/);
		}
	} else if ( job_status != REMOVED ) {
		// Jobs must be in COMPLETED or REMOVED status to leave the queue
		errno = EINVAL;
		return DESTROYPROC_ERROR;
	}

	int job_finished_hook_done = -1;
	ad->LookupInteger( ATTR_JOB_FINISHED_HOOK_DONE, job_finished_hook_done );
	if( job_finished_hook_done == -1 ) {
			// our cleanup hook hasn't finished yet for this job.  so,
			// we just want to add it to our queue of job ids to call
			// the hook on and return immediately
		scheduler.enqueueFinishedJob( cluster_id, proc_id );
		return DESTROYPROC_SUCCESS_DELAY;
	}

		// should we leave the job in the queue?
	bool leave_job_in_q = false;
	{
		ClassAd completeAd(*ad);
		JobQueue->AddAttrsFromTransaction(key,completeAd);
		completeAd.LookupBool(ATTR_JOB_LEAVE_IN_QUEUE,leave_job_in_q);
		if ( leave_job_in_q ) {
			return DESTROYPROC_SUCCESS_DELAY;
		}
	}

 
	// Remove checkpoint files
	if ( !Q_SOCK ) {
		//if socket is dead, have cleanup lookup ad owner
		cleanup_ckpt_files(cluster_id,proc_id,nullptr );
	}
	else {
		cleanup_ckpt_files(cluster_id,proc_id,Q_SOCK->getOwner() );
	}

	// Remove the job from its autocluster
	scheduler.autocluster.removeFromAutocluster(*ad);

	// Append to history file
	AppendHistory(ad);

	// Write a per-job history file (if PER_JOB_HISTORY_DIR param is set)
	WritePerJobHistoryFile(ad, false);

  ScheddPluginManager::Archive(ad);

  // save job ad to the log
	bool already_in_transaction = InTransaction();
	if( !already_in_transaction ) {
			// Start a transaction - this is particularly important because we 
			// want the remote the job and persist the jobSet attributes in one
			// transaction so we do not have off-by-one errors in our set aggregates
			// if machine crashed at the wrong moment.
		BeginTransaction();
	}

	// the job ad should have a pointer to the cluster ad, so use that if it has been set.
	// otherwise lookup the cluster ad.
	JobQueueCluster * clusterad = ad->Cluster();
	if ( ! clusterad) {
		clusterad = GetClusterAd(ad->jid);
	}

	JobQueue->DestroyClassAd(key);

#ifdef USE_JOB_QUEUE_USERREC
	/* update JobQueueUserRec counts of completed/removed jobs
	 */
	if (ad->ownerinfo) {
		if (ad->Status() >= REMOVED && ad->Status() <= COMPLETED) {
			static const char * const attrs[]{
				ATTR_TOTAL_REMOVED_JOBS, "Scheduler" ATTR_TOTAL_REMOVED_JOBS,
				ATTR_TOTAL_COMPLETED_JOBS, "Scheduler" ATTR_TOTAL_COMPLETED_JOBS,
			};
			int ix = 2 * (ad->Status() - REMOVED) + (ad->Universe() == CONDOR_UNIVERSE_SCHEDULER);
			const char * attr = attrs[ix];
				int val = 0;
				ad->ownerinfo->LookupInteger(attr, val);
				val++;
				SetUserAttributeInt(*(ad->ownerinfo), attr, val);
		}
	}

#endif

	/* If job is a member of the set, remove the job from the set
	   and also at the same time save persistent set aggregates
	   now before the job leaves the queue, so that we dont lose info
	   about this job if the schedd crashes.
	   Note we must do this in the same transaction as the call to
	   DestroyClassAd() above.
	*/
	if (scheduler.jobSets) {
		scheduler.jobSets->removeJobFromSet(*ad);
	}

	DecrementClusterSize(cluster_id, clusterad);

	// We'll need the JobPrio value later after the ad has been destroyed
	int job_prio = 0;
	ad->LookupInteger(ATTR_JOB_PRIO, job_prio);

	int universe = CONDOR_UNIVERSE_VANILLA;
	ad->LookupInteger(ATTR_JOB_UNIVERSE, universe);

	if( (universe == CONDOR_UNIVERSE_MPI) ||
		(universe == CONDOR_UNIVERSE_PARALLEL) ) {
			// Parallel jobs take up a whole cluster.  If we've been ask to
			// destroy any of the procs in a parallel job cluster, we
			// should destroy the entire cluster.  This hack lets the
			// schedd just destroy the proc associated with the shadow
			// when a multi-class parallel job exits without leaving other
			// procs in the cluster around.  It also ensures that the
			// user doesn't delete only some of the procs in the parallel
			// job cluster, since that's going to really confuse the
			// shadow.
		JobQueueJob *otherAd = nullptr;
		JobQueueKeyBuf otherKey;
		int otherProc = -1;

		bool stillLooking = true;
		while (stillLooking) {
			otherProc++;
			if (otherProc == proc_id) continue; // skip this proc

			IdToKey(cluster_id,otherProc,otherKey);
			if (!JobQueue->Lookup(otherKey, otherAd)) {
				stillLooking = false;
			} else {
				JobQueue->DestroyClassAd(otherKey);
				DecrementClusterSize(cluster_id, clusterad);
			}
		}
	}

	if( !already_in_transaction ) {
			// For performance, use a NONDURABLE transaction.  If we crash
			// before this makes it to the disk, on restart we will
			// attempt removing it again.  Yes, this means we may
			// generate a duplicate history entry and redo other
			// cleanup tasks, but that is possible even if this
			// transaction is marked DURABLE, since all of those tasks
			// are not atomic with respect to this transaction anyway.

		CommitNonDurableTransactionOrDieTrying();
	}

		// remove jobid from any indexes
	scheduler.removeJobFromIndexes(key, job_prio);

	JobQueueDirty = true;

	return DESTROYPROC_SUCCESS;
}


int DestroyCluster(int /*cluster_id*/, const char* /*reason*/)
{
#if 1 // TJ: 2021 neutered DestroyCluster because it was only used by submit on abort 
	  // The schedd should so this cleanup on it own and in any case,
	  // what the code below does is incorrect and/or incomplete
	return -1;
#endif
}

int
SetAttributeByConstraint(const char *constraint_str, const char *attr_name,
						 const char *attr_value,
						 SetAttributeFlags_t flags)
{
	ClassAd	*ad = nullptr;
	int cluster_match_count = 0;
	int job_match_count = 0;
	int had_error = 0;
	int rval = -1;
	int terrno = 0;
	JobQueueKey key;

	if ( Q_SOCK && !(Q_SOCK->isAuthenticated()) ) {
		errno = EACCES;
		return -1;
	}

	// if the only-my-jobs knob is off, strip the onlymyjobs flag and
	// just let the transaction fail if they try and change something that they shouldn't
	if ( ! param_boolean("CONDOR_Q_ONLY_MY_JOBS", true)) {
		flags &= ~SetAttribute_OnlyMyJobs;
	}

	// setup an owner_expr expression if the OnlyMyJobs flag is set.
	// and fixup and replace the constraint_str pointer if we have 
	// both an input constraint and OnlyMyJobs is set.
	// TODO: re-write this to take advantage of UserRec pointers
	YourString owner;
	YourString user;
	std::string owner_expr;
	if (flags & SetAttribute_OnlyMyJobs) {
		user = EffectiveUserName(Q_SOCK); // user is "" if no Q_SOCK
		owner = Q_SOCK ? Q_SOCK->getOwner() : "unauthenticated";
		if (owner == "unauthenticated") {
			// no job will be owned by "unauthenticated" so just quit now.
			errno = EACCES;
			return -1;
		}
		bool is_super = isQueueSuperUserName(user.c_str());
		dprintf(D_COMMAND | D_VERBOSE, "SetAttributeByConstraint w/ OnlyMyJobs owner = \"%s\" (isQueueSuperUser = %d)\n", owner.c_str(), is_super);
		if (is_super) {
			// for queue superusers, disable the OnlyMyJobs flag - they get to act on all jobs.
			flags &= ~SetAttribute_OnlyMyJobs;
		} else {
		#ifdef USE_JOB_QUEUE_USERREC
			const OwnerInfo * owni = scheduler.lookup_owner_const(user.c_str());
			if (owni) {
				formatstr(owner_expr, "(%s == \"%s\")", ATTR_USERREC_NAME, owni->Name());
			} else {
				formatstr(owner_expr, "(%s == \"%s\")", ATTR_OWNER, owner.c_str());
			}
		#else
			formatstr(owner_expr, "(%s == \"%s\")", attr_JobUser.c_str(), user.c_str());
		#endif
			if (constraint_str) {
				owner_expr += " && ";
				owner_expr += constraint_str;
			}
			constraint_str = owner_expr.c_str();
		}
		flags &= ~SetAttribute_OnlyMyJobs; // don't pass this flag to the inner SetAttribute call
	}

	// parse the constraint into an expression, ConstraintHolder will free it in it's destructor
	ConstraintHolder constraint;
	if (constraint_str ) {
		ExprTree * tree = nullptr;
		if (0 != ParseClassAdRvalExpr(constraint_str, tree)) {
			dprintf( D_ALWAYS, "can't parse constraint: %s\n", constraint_str );
			errno = EINVAL;
			return -1;
		}
		constraint.set(tree);
	}

	// TODO: Fix loop to implement OnlyMyjobs by comparing UserRec pointers

	// loop through the job queue, setting attribute on jobs that match
	JobQueue->StartIterateAllClassAds();
	while(JobQueue->IterateAllClassAds(ad,key)) {
		if (IsSpecialJobId(key.cluster,key.proc)) continue; // skip non-editable ads
		auto * job = dynamic_cast<JobQueueJob*>(ad);

		// ignore cluster ads unless the PostSubmitClusterChange flag is set.
		if (job->IsCluster() && ! (flags & SetAttribute_PostSubmitClusterChange))
			continue;

		if (EvalExprBool(ad, constraint.Expr())) {
			if (job->IsCluster())
				cluster_match_count += 1;
			else
				job_match_count += 1;
			if (SetAttribute(key.cluster, key.proc, attr_name, attr_value, flags) < 0) {
				had_error = 1;
				terrno = errno;
			}
		}
	}

		// If we couldn't find any jobs that matched the constraint,
		// or we could set the attribute on any of the ones we did
		// find, return error (-1).
	// failure (-1)
	if (had_error) {
		errno = terrno;
		return -1;
	}

	// At this point we need to determine the return value.
	// If 1 or more jobs matched, we wanted to return the number of job matches.
	// If 0 jobs matched, but clusters matched, return the number of cluster matches.
	// If no jobs or clusters matched, return value is -1 (error)
	if (job_match_count > 0) {
		rval = job_match_count;
	}
	else if (cluster_match_count > 0) {
		rval = cluster_match_count;
	}
	else {
		errno = ENOENT;
		rval = -1;
	}

	return rval;
}

// Set up an efficient lookup table for attributes that need special processing in SetAttribute
//
enum {
	idATTR_none=0,
	idATTR_CLUSTER_ID,
	idATTR_PROC_ID,
	idATTR_CONCURRENCY_LIMITS,
	idATTR_CRON_DAYS_OF_MONTH,
	idATTR_CRON_DAYS_OF_WEEK,
	idATTR_CRON_HOURS,
	idATTR_CRON_MINUTES,
	idATTR_CRON_MONTHS,
	idATTR_JOB_PRIO,
	idATTR_JOB_STATUS,
	idATTR_JOB_UNIVERSE,
	idATTR_NICE_USER,
	idATTR_ACCOUNTING_GROUP,
	idATTR_OWNER,
	idATTR_RANK,
	idATTR_REQUIREMENTS,
	idATTR_USER,
	idATTR_NUM_JOB_RECONNECTS,
	idATTR_JOB_NOOP,
	idATTR_JOB_MATERIALIZE_CONSTRAINT,
	idATTR_JOB_MATERIALIZE_DIGEST_FILE,
	idATTR_JOB_MATERIALIZE_ITEMS_FILE,
	idATTR_JOB_MATERIALIZE_LIMIT,
	idATTR_JOB_MATERIALIZE_MAX_IDLE,
	idATTR_JOB_MATERIALIZE_PAUSED,
	idATTR_HOLD_REASON,
	idATTR_HOLD_REASON_CODE,
	idATTR_JOB_SET_ID,
	idATTR_JOB_SET_NAME,
	idATTR_NT_DOMAIN,
	idATTR_ENABLED,
	idATTR_DISABLE_REASON,
	idATTR_USERREC_OPT_CREATE_DEPRECATED,
	idATTR_USERREC_LIVE,
};

enum {
	catNone         = 0,
	catJobObj       = 0x0001, // attributes that are cached in the job object
	catJobId        = 0x0002, // cluster & proc id
	catCron         = 0x0004, // attributes that tell us this is a crondor job
	catStatus       = 0x0008, // job status changed, need to adjust the counts of running/idle/held/etc jobs.
	catDirtyPrioRec = 0x0010,
	catTargetScope  = 0x0020,
	catSubmitterIdent = 0x0040,
	catNewMaterialize = 0x0080,  // attributes that control the job factory
	catMaterializeState = 0x0100, // change in state of job factory
	catSpoolingHold = 0x0200,    // hold reason was set to CONDOR_HOLD_CODE::SpoolingInput
	catPostSubmitClusterChange = 0x400, // a cluster ad was changed after submit time which calls for special processing in commit transaction
	catJobset       = 0x800,     // job membership in a jobset changed or a new jobset should be created
	catSetUserRec   = 0x1000,    // a UserRec was edited
	catNewUser      = 0x2000,    // a new job "owner" or "user" was added
	catSetOwner     = 0x4000,    // the ATTR_OWNER or ATTR_USER of a job or jobset was set/changed
	catCallbackTrigger = 0x10000, // indicates that a callback should happen on commit of this attribute
	catCallbackNow = 0x20000,    // indicates that a callback should happen when setAttribute is called
};

typedef struct attr_ident_pair {
	const char * key;
	int          id;
	int          category;
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const struct attr_ident_pair& rhs) const {
		return strcasecmp(this->key, rhs.key) < 0;
	}
} ATTR_IDENT_PAIR;

// NOTE: !!!
// NOTE: this table MUST be sorted by case-insensitive attribute name!
// NOTE: !!!
#define FILL(attr,cat) { attr, id##attr, cat }
static const ATTR_IDENT_PAIR aSpecialSetAttrs[] = {
	FILL(ATTR_ACCOUNTING_GROUP,   catDirtyPrioRec | catSubmitterIdent),
	FILL(ATTR_CLUSTER_ID,         catJobId),
	FILL(ATTR_CONCURRENCY_LIMITS, catDirtyPrioRec),
	FILL(ATTR_CRON_DAYS_OF_MONTH, catCron),
	FILL(ATTR_CRON_DAYS_OF_WEEK,  catCron),
	FILL(ATTR_CRON_HOURS,         catCron),
	FILL(ATTR_CRON_MINUTES,       catCron),
	FILL(ATTR_CRON_MONTHS,        catCron),
	FILL(ATTR_HOLD_REASON,        0), // used to detect submit of jobs with the magic 'hold for spooling' hold code
	FILL(ATTR_HOLD_REASON_CODE,   0), // used to detect submit of jobs with the magic 'hold for spooling' hold code
	FILL(ATTR_JOB_NOOP,           catDirtyPrioRec),
	FILL(ATTR_JOB_MATERIALIZE_CONSTRAINT, catNewMaterialize | catCallbackTrigger),
	FILL(ATTR_JOB_MATERIALIZE_DIGEST_FILE, catNewMaterialize | catCallbackTrigger),
	FILL(ATTR_JOB_MATERIALIZE_ITEMS_FILE, catNewMaterialize | catCallbackTrigger),
	FILL(ATTR_JOB_MATERIALIZE_LIMIT, catMaterializeState | catCallbackTrigger),
	FILL(ATTR_JOB_MATERIALIZE_MAX_IDLE, catMaterializeState | catCallbackTrigger),
	FILL(ATTR_JOB_MATERIALIZE_PAUSED, catMaterializeState | catCallbackTrigger),
	FILL(ATTR_JOB_PRIO,           catDirtyPrioRec),
	FILL(ATTR_JOB_SET_ID,         catJobId | catJobset | catCallbackTrigger),
	FILL(ATTR_JOB_SET_NAME,       catJobset | catCallbackTrigger),
	FILL(ATTR_JOB_STATUS,         catStatus | catCallbackTrigger),
	FILL(ATTR_JOB_UNIVERSE,       catJobObj),
#ifdef NO_DEPRECATED_NICE_USER
	FILL(ATTR_NICE_USER,          catSubmitterIdent),
#endif
	FILL(ATTR_NUM_JOB_RECONNECTS, 0),
	FILL(ATTR_OWNER,              0),
	FILL(ATTR_PROC_ID,            catJobId),
	FILL(ATTR_RANK,               catTargetScope),
	FILL(ATTR_REQUIREMENTS,       catTargetScope),
	FILL(ATTR_USER,              0),


};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
static int IsSpecialSetAttribute(const char *attr, int* set_cat=nullptr)
{
	const ATTR_IDENT_PAIR* found = nullptr;
	found = BinaryLookup<ATTR_IDENT_PAIR>(
		aSpecialSetAttrs,
		COUNTOF(aSpecialSetAttrs),
		attr, strcasecmp);
	if (found) {
		if (set_cat) *set_cat = found->category;
		return found->id;
	}
	if (set_cat) *set_cat = 0;
	return 0;
}


int
SetSecureAttributeInt(int cluster_id, int proc_id, const char *attr_name, int attr_value, SetAttributeFlags_t flags)
{
	if (attr_name == nullptr ) {return -1;}

	char buf[100];
	snprintf(buf,100,"%d",attr_value);

	// lookup job and set attribute
	JOB_ID_KEY_BUF key;
	IdToKey(cluster_id,proc_id,key);
	JobQueue->SetAttribute(key, attr_name, buf, flags & SetAttribute_SetDirty);

	return 0;
}


int
SetSecureAttributeString(int cluster_id, int proc_id, const char *attr_name, const char *attr_value, SetAttributeFlags_t flags)
{
	if (attr_name == nullptr || attr_value == nullptr) {return -1;}

	// do quoting using oldclassad syntax
	classad::Value tmpValue;
	classad::ClassAdUnParser unparse;
	unparse.SetOldClassAd( true, true );

	tmpValue.SetStringValue(attr_value);
	std::string buf;
	unparse.Unparse(buf, tmpValue);

	// lookup job and set attribute to quoted string
	JOB_ID_KEY_BUF key;
	IdToKey(cluster_id,proc_id,key);
	JobQueue->SetAttribute(key, attr_name, buf.c_str(), flags & SetAttribute_SetDirty);

	return 0;
}

int
SetSecureAttribute(int cluster_id, int proc_id, const char *attr_name, const char *attr_value, SetAttributeFlags_t flags)
{
	if (attr_name == nullptr || attr_value == nullptr) {return -1;}

	// lookup job and set attribute to value
	JOB_ID_KEY_BUF key;
	IdToKey(cluster_id,proc_id,key);
	JobQueue->SetAttribute(key, attr_name, attr_value, flags & SetAttribute_SetDirty);

	return 0;
}

// Internal helper functions for setting UserRec attributes into a transaction
//
int SetUserAttribute(JobQueueUserRec & urec, const char * attr_name, const char * expr_string)
{
	if (attr_name == nullptr || expr_string == nullptr) {return -1;}

	if (JobQueue->SetAttribute(urec.jid, attr_name, expr_string, false)) {
		JobQueue->SetTransactionTriggers(catSetUserRec);
		urec.setDirty();
		return 0;
	}
	return -1;
}

int DeleteUserAttribute(JobQueueUserRec & urec, const char * attr_name)
{
	if (attr_name == nullptr) {return -1;}

	if (JobQueue->DeleteAttribute(urec.jid, attr_name)) {
		JobQueue->SetTransactionTriggers(catSetUserRec);
		urec.setDirty();
		return 0;
	}
	return -1;
}

int SetUserAttributeInt(JobQueueUserRec & urec, const char * attr_name, long long attr_value)
{
	char buf[100];
	snprintf(buf,100,"%lld",attr_value);
	return SetUserAttribute(urec, attr_name, buf);
}

int SetUserAttributeValue(JobQueueUserRec & urec, const char * attr_name, const classad::Value & attr_value)
{
	if (attr_name == nullptr) {return -1;}

	classad::ClassAdUnParser unparse;
	unparse.SetOldClassAd( true, true );

	std::string buf;
	unparse.Unparse(buf, attr_value);

	return SetUserAttribute(urec, attr_name, buf.c_str());
}


// Check whether modification of a job attribute should be allowed.
// return <0 : reject, return error to client
// return 0 : ignore, return success to client
// return >0 : accept, apply change and return success to client
// TODO formalize the return type with an enum?
int
ModifyAttrCheck(const JOB_ID_KEY_BUF &key, const char *attr_name, const char *attr_value, SetAttributeFlags_t flags, CondorError *err)
{
	JobQueueJob    *job = nullptr;

	const char *func_name = (flags & SetAttribute_Delete) ? "DeleteAttribute" : "SetAttribute";

	// Only an authenticated user or the schedd itself can modify an attribute.
	if ( Q_SOCK && !(Q_SOCK->isAuthenticated()) ) {
		if (err) err->push("QMGMT", EACCES, "Authentication is required to set attributes");
		errno = EACCES;
		return -1;
	}

	// job id 0.0 and maybe some other ids are special and cannot normally be modified.
	if ( IsSpecialJobId(key.cluster,key.proc) ) {
		dprintf(D_ALWAYS, "WARNING: %s attempt to edit special ad %d.%d\n", func_name,key.cluster,key.proc);
		errno = EACCES;
		return -1;
	}

	// If someone is trying to do something funny with an invalid
	// attribute name, bail out early
	if (!IsValidAttrName(attr_name)) {
		dprintf(D_ALWAYS, "%s got invalid attribute named %s for job %d.%d\n",
			func_name, attr_name ? attr_name : "(null)", key.cluster, key.proc);
		if (err) err->pushf("QMGMT", EINVAL, "Got invalid attribute named %s for job %d.%d",
			attr_name ? attr_name : "(null)", key.cluster, key.proc);
		errno = EINVAL;
		return -1;
	}

		// If someone is trying to do something funny with an invalid
		// attribute value, bail early
		// This check doesn't apply to DeleteAttribute.
	if ( !(flags & SetAttribute_Delete) ) {
		if (!IsValidAttrValue(attr_value)) {
			dprintf(D_ALWAYS,
				"%s received invalid attribute value '%s' for "
				"job %d.%d, ignoring\n",
				func_name, attr_value ? attr_value : "(null)", key.cluster, key.proc);
			if (err) err->pushf("QMGMT", EINVAL, "Received invalid attribute value '%s' for "
				"job %d.%d; ignoring",
				attr_value ? attr_value : "(null)", key.cluster, key.proc);
			errno = EINVAL;
			return -1;
		}
	}

	// Ensure user is not changing a secure attribute.  Only schedd is
	// allowed to do that, via the internal API.
	if (secure_attrs.find(attr_name) != secure_attrs.end())
	{
		// should we fail or silently succeed?  (old submits set secure attrs)
		const CondorVersionInfo *vers = nullptr;
		if ( Q_SOCK && ! Ignore_Secure_SetAttr_Attempts) {
			vers = Q_SOCK->get_peer_version();
		}
		if (vers && vers->built_since_version( 8, 5, 8 ) ) {
			// new versions should know better!  fail!
			dprintf(D_ALWAYS,
				"%s attempt to edit secure attribute %s in job %d.%d. Failing!\n",
				func_name, attr_name, key.cluster, key.proc);
			if (err) err->pushf("QMGMT", EACCES, "Attempt to edit secure attribute %s"
				" in job %d.%d. Failing!", attr_name, key.cluster, key.proc);
			errno = EACCES;
			return -1;
		} else {
			// old versions get a pass.  succeed (but do nothing).
			// The idea here is we will not set the secure attributes, but we won't
			// propagate the error back because we don't want old condor_submits to not
			// be able to submit jobs.
			dprintf(D_ALWAYS,
				"%s attempt to edit secure attribute %s in job %d.%d. Ignoring!\n",
				func_name, attr_name, key.cluster, key.proc);
			return 0;
		}
	}

	// TODO Add a way to efficiently test if the key refers to an ad
	// created in the currently-active transaction. Currently, submit
	// transforms and late materialization get free reign to modify
	// attributes for invalid job ids.
	if (JobQueue->Lookup(key, job)) {
		// If we made it here, the user is adding or editing attrbiutes
		// to an ad that has already been committed in the queue.

		// Ensure the user is not changing a job they do not own.
		if ( Q_SOCK && !UserCheck(job, EffectiveUserRec(Q_SOCK) )) {
			const char *user = EffectiveUserName(Q_SOCK);
			if (!user) {
				user = "NULL";
			}
			dprintf(D_ALWAYS,
					"UserCheck(%s) failed in %s for job %d.%d\n",
					user, func_name, key.cluster, key.proc);
			if (err) err->pushf("QMGMT", EACCES, "User %s may not change attributes for "
				"jobs they do not own (job %d.%d)", user, key.cluster, key.proc);
			errno = EACCES;
			return -1;
		}

		// Ensure user is not changing an immutable attribute to a committed job
		bool is_immutable_attr = immutable_attrs.find(attr_name) != immutable_attrs.end();
		if (is_immutable_attr)
		{

			dprintf(D_ALWAYS,
					"%s attempt to edit immutable attribute %s in job %d.%d\n",
					func_name, attr_name, key.cluster, key.proc);
			if (err) err->pushf("QMGMT", EACCES, "Attempt to edit immutable attribute"
				" %s in job %d.%d", attr_name, key.cluster, key.proc);
			errno = EACCES;
			return -1;
		}

		// Ensure user is not changing a protected attribute to a committed job
		// unless the real owner (i.e the peer on the socket) is a queue superuser or
		// the user is the schedd itself (in which case Q_SOCK == NULL).
		// Also check the allow_protected_attr_changes_by_superuser flag, which will
		// be set to false when a superuser process (like the shadow) is acting under
		// the direction of a non-super user (like when the shadow is forwarding chirp
		// updates from the user).
	#ifdef USE_JOB_QUEUE_USERREC
		const JobQueueUserRec * auth_user = EffectiveUserRec(Q_SOCK);
	#else
		const char * auth_user = NULL;
		if (user_is_the_new_owner) { auth_user = EffectiveUser(Q_SOCK); }
		else if (Q_SOCK) { auth_user = Q_SOCK->getRealOwner(); }
	#endif
		if ( Q_SOCK && 
			 ( (!isQueueSuperUser(auth_user) && !qmgmt_all_users_trusted) ||
			    !Q_SOCK->getAllowProtectedAttrChanges() ) &&
			 protected_attrs.find(attr_name) != protected_attrs.end() )
		{
			dprintf(D_ALWAYS,
					"%s of protected attribute denied, RealOwner=%s EffectiveOwner=%s AllowPAttrchange=%s Attr=%s in job %d.%d\n",
					func_name,
					Q_SOCK->getRealOwner() ? Q_SOCK->getRealOwner() : "(null)",
					Q_SOCK->getOwner() ? Q_SOCK->getOwner() : "(null)",
					Q_SOCK->getAllowProtectedAttrChanges() ? "true" : "false",
					attr_name, key.cluster, key.proc);
			if (err) err->pushf("QMGMT", EACCES,
				"Unauthorized setting of protected attribute %s denied "
				"for real owner %s (effective owner %s; allowed protected attribute "
				"change = %s) in job %d.%d", attr_name,
				Q_SOCK->getRealOwner() ? Q_SOCK->getRealOwner() : "(null)",
				Q_SOCK->getOwner() ? Q_SOCK->getOwner() : "(null)",
				Q_SOCK->getAllowProtectedAttrChanges() ? "true" : "false",
				key.cluster, key.proc);
			errno = EACCES;
			return -1;
		}
	} else if (flags & SetAttribute_SubmitTransform) {
		// submit transforms come from inside the schedd and have no restrictions
		// on which cluster/proc may be edited (the transform itself guarantees that only
		// jobs in the submit transaction will be edited)
	} else if (flags & SetAttribute_LateMaterialization) {
		// late materialization comes from inside the schedd and has no
		// restrictions on which cluster/proc may be edited
	} else if (JobQueue->InTransaction() && active_cluster_num > 0) {
		// We have an active transaction that's adding new ads.
		// Assume the modification is for one of those ads (we know it's
		// not for an ad that's already in the queue).
		// Restrict to only modifying attributes in the last cluster
		// returned by NewCluster, and also restrict to procs that have
		// been returned by NewProc or the cluster ad (proc -1).
		// If we got a completely bogus job id, we'll still reject it.
		if ((key.cluster != active_cluster_num) || (key.proc >= next_proc_num) || (key.proc < -1)) {
			dprintf(D_ALWAYS,"%s modifying attribute %s in non-active cluster cid=%d acid=%d\n", 
					func_name, attr_name, key.cluster, active_cluster_num);
			if (err) err->pushf("QMGMT", ENOENT, "Modifying attribute %s in "
				"non-active cluster %d (current active cluster is %d)",
				attr_name, key.cluster, active_cluster_num);
			errno = ENOENT;
			return -1;
		}

		// TODO Do we need to do this check for internal calls?
		// If deleting an attribute in an uncommitted ad, check if the
		// attribute is set in the transaction. If not, return failure.
		if ( flags & SetAttribute_Delete ) {
			char *val = nullptr;
			if ( ! JobQueue->LookupInTransaction(key, attr_name, val) ) {
				errno = ENOENT;
				return -1;
			}
			// If we found it in the transaction, we need to free val
			// so we don't leak memory.  We don't use the value, we just
			// wanted to make sure we could find the attribute so we know
			// to return failure if needed.
			free( val );
		}
	} else {
		dprintf(D_ALWAYS,"%s modifying attribute %s in nonexistent job %d.%d\n",
				func_name, attr_name, key.cluster, key.proc);
		if (err) err->pushf("QMGMT", ENOENT, "Modifying attribute %s in "
			"nonexistent job %d.%d",
			attr_name, key.cluster, key.proc);
		errno = ENOENT;
		return -1;
	}

	return 1;
}

int
SetAttribute(int cluster_id, int proc_id, const char *attr_name,
			 const char *attr_value, SetAttributeFlags_t flags,
			 CondorError *err)
{
	JOB_ID_KEY_BUF key;
	JobQueueJob    *job = nullptr;
	JobQueueJobSet *jobset = nullptr;
	std::string		new_value;
	bool query_can_change_only = (flags & SetAttribute_QueryOnly) != 0; // flag for 'just query if we are allowed to change this'

	IdToKey(cluster_id,proc_id,key);

	int rc = ModifyAttrCheck(key, attr_name, attr_value, flags, err);
	if ( rc <= 0 ) {
		return rc;
	}

	JobQueue->Lookup(key, job);
	if (job && job->IsJobSet()) {
		jobset = dynamic_cast<JobQueueJobSet *>(static_cast<JobQueueBase*>(job));
		job = nullptr; // make sure we don't try and edit this as a JobQueueJob
	}

	int attr_category = 0;
	int attr_id = IsSpecialSetAttribute(attr_name, &attr_category);

	// A few special attributes have additional access checks
	// but for most, we have already decided whether or not we can change this attribute
	if (query_can_change_only) {
		if (attr_id == idATTR_OWNER || attr_id == idATTR_USER || (attr_category & catJobId) || (attr_id == idATTR_JOB_STATUS)) {
			// we have more validation to do.
		} else {
			// access check is ok, but we don't actually want to make the change so return now.
			return 0;
		}
	}

	// check for security violations.
	// first, make certain ATTR_OWNER can only be set to who they really are.
	if (attr_id == idATTR_OWNER)
	{
		const char* sock_owner = Q_SOCK ? Q_SOCK->getOwner() : "";
		if( !sock_owner ) {
			sock_owner = "";
		}

		bool is_local_user = true; // owner is in this UID_DOMAIN
		if ( strcasecmp(attr_value,"UNDEFINED")==0 ) {
				// If the user set the owner to be undefined, then
				// just fill in the value of Owner with the owner name
				// of the authenticated socket, provided the UID domain
				// matches.
				//
				// If the UID domain doesn't match (and TRUST_UID_DOMAIN
				// is false), then we are dealing with user that is no local account
			if ( sock_owner && *sock_owner ) {
				new_value = '"';
				new_value += sock_owner;
				if ( ! ignore_domain_mismatch_when_setting_owner) {
					const char * sock_domain = Q_SOCK->getDomain();
					if (sock_domain && *sock_domain) {
						// TODO: need a better way to detect if the sock owner is a local user
					#ifdef WIN32
						CompareUsersOpt opt = COMPARE_DOMAIN_PREFIX;
					#else
						CompareUsersOpt opt = COMPARE_DOMAIN_FULL;
					#endif
						is_local_user = is_same_domain(sock_domain, scheduler.uidDomain(), opt, nullptr);
					}
				}
				new_value += '"';
				attr_value  = new_value.c_str();
			} else {
				// socket not authenticated and Owner is UNDEFINED.
				dprintf(D_ALWAYS, "ERROR SetAttribute violation: "
					"Owner is UNDEFINED, but client not authenticated\n");
				if (err) err->pushf("QMGMT", EACCES, "Client is not authenticated "
					"and owner is undefined");
				errno = EACCES;
				return -1;

			}
		}

			// We can't just use attr_value, since it contains '"'
			// marks.  Carefully remove them here.
		std::string owner_buf;
		char const *owner = attr_value;
		bool owner_is_quoted = false;
		if( *owner == '"' ) {
			owner_buf = owner+1;
			if( owner_buf.length() && owner_buf[owner_buf.length()-1] == '"' )
			{
				owner_buf.erase(owner_buf.length()-1);
				owner_is_quoted = true;
			}
			owner = owner_buf.c_str();
		}

		if (Q_SOCK && ! ignore_domain_mismatch_when_setting_owner) {
			// Similar to the case above, if UID_DOMAIN != socket FQU domain,
			// then we map to 'nobody' unless TRUST_UID_DOMAIN is set.
			//
			// In the case of an internal superuser (e.g., condor@family, condor@parent),
			// we assume the intent is the default domain; this allows the JobRouter
			// (which authenticates as condor@family) to behave as if it is in the same
			// UID domain by default.
			YourStringNoCase sock_domain(Q_SOCK->getDomain());

			if (sock_domain != "family" &&
				sock_domain != "parent" &&
				sock_domain != "child")
			{
			#ifdef WIN32
				// HACK! compare domains to decide whether this is a local user or not
				// we need a better way to test for local user
				if ( ! is_same_domain(sock_domain.c_str(),".", COMPARE_DOMAIN_DEFAULT, scheduler.uidDomain()))
			#else
				if (sock_domain != scheduler.uidDomain())
			#endif
				{
					is_local_user = false;
					dprintf(D_SYSCALLS, "  Owner is not in UID_DOMAIN; setting IsLocalUser=false\n");
				}
			}
		}

		if( !owner_is_quoted ) {
			// For sanity's sake, do not allow setting Owner to something
			// strange, such as an attribute reference that happens to have
			// the same name as the authenticated user.
			dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s which is not a valid string\n",
					attr_value);
			if (err) err->pushf("QMGMT", EACCES, "Unable to set owner to %s as it is"
				" not a valid owner string", attr_value);
			errno = EACCES;
			return -1;
		}

		if (MATCH == strcmp(owner, "root") || MATCH == strcmp(owner, "LOCAL_SYSTEM")) {
			// Never allow the owner to be "root" (Linux) or "LOCAL_SYSTEM" (Windows).
			// Because we run cross-platform, we never allow either name on all platforms
			dprintf(D_ALWAYS, "SetAttribute security violation: setting owner to %s is not permitted\n", attr_value);
			if (err) err->pushf("QMGMT", EACCES, "Setting job owner to %s is not "
				"permitted", attr_value);
			errno = EACCES;
			return -1;
		}

		std::string orig_owner;
		if( GetAttributeString(cluster_id,proc_id,ATTR_OWNER,orig_owner) >= 0
			&& orig_owner != owner
			&& !qmgmt_all_users_trusted )
		{
			// Unless all users are trusted, nobody (not even queue super user)
			// has the ability to change the owner attribute once it is set.
			// See gittrack #1018.
			dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s when previously set to \"%s\"\n",
					attr_value, orig_owner.c_str());
			if (err) err->pushf("QMGMT", EACCES, "Setting owner to %s when previously "
				"set to %s is not permitted.", attr_value, orig_owner.c_str());
			errno = EACCES;
			return -1;
		}

		if (IsDebugVerbose(D_SECURITY)) {
			bool is_super = isQueueSuperUserName(sock_owner);
			bool allowed_owner = SuperUserAllowedToSetOwnerTo(owner);
			dprintf(D_SECURITY | D_VERBOSE, "QGMT: qmgmt_A_U_T %i, owner %s, sock_owner %s, is_Q_SU %i, SU_Allowed %i\n",
				qmgmt_all_users_trusted, owner, sock_owner, is_super, allowed_owner);
		}

		#if defined(WIN32)
		bool not_sock_owner = (strcasecmp(owner,sock_owner) != 0);
		#else
		bool not_sock_owner = (strcmp(owner,sock_owner) != 0);
		#endif

		if (!qmgmt_all_users_trusted
			&& not_sock_owner
			&& (!isQueueSuperUser(EffectiveUserRec(Q_SOCK)) || !SuperUserAllowedToSetOwnerTo(owner)) ) {
				dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s when active owner is \"%s\"\n",
					owner, sock_owner);
				if (err) err->pushf("QMGMT", EACCES, "Setting owner to %s when "
					"active owner is %s is not permitted",
					owner, sock_owner);
				errno = EACCES;
				return -1;
		}

#if !defined(WIN32)
		uid_t user_uid = 0;
		if ( can_switch_ids() && !pcache()->get_user_uid( owner, user_uid ) ) {
			dprintf( D_ALWAYS, "SetAttribute security violation: "
					 "setting owner to %s, which is not a valid user account\n",
					 attr_value );
			if (err) err->pushf("QMGMT", EACCES, "Setting owner to %s, which is not a "
				"valid user account", attr_value);
			errno = EACCES;
			return -1;
		}
#endif
		if (query_can_change_only) {
			return 0;
		}

		// The code that creates a UserRec on submit makes one for the sock owner
		// In the case of a submit portal, we might also need to create a user rec for
		// the +Owner identifier.  In the future, we may move this to set effective owner
		// and not allow user creation as a side-effect of setting the Owner attribute.
		if (not_sock_owner) {
			auto urec = scheduler.lookup_owner_const(owner);
			if ( ! urec) {
				if (allow_submit_from_known_users_only) {
					dprintf(D_ALWAYS, "SetAttribute(Owner): fail because no User record for %s\n", owner);
					if (err) {
						err->pushf("QMGMT",EACCES,"Unknown User %s. Use condor_qusers to add user before submitting jobs.", owner);
					}
					errno = EACCES;
					return -1;
				} else {
					// create user a user record for a new submitter
					// the insert_owner_const will make a pending user record
					// which we then add to the current transaction by calling MakeUserRec
					// TODO: set the NTDomain to a reasonable value for this new User rec
					urec = scheduler.insert_owner_const(owner);
					if ( ! MakeUserRec(urec, true, &scheduler.getUserRecDefaultsAd())) {
						dprintf(D_ALWAYS, "SetAttribute(Owner): failed to create new User record for %s\n", owner);
						errno = EACCES;
						return -1;
					}
				}
			}
		}

		if (user_is_the_new_owner) {
			// Backward compatibility tweak:
			// If the remote user sets the Owner to something different than the
			// socket owner *and* they are in the same UID domain, then we will
			// also set the user.
			if (is_local_user && ((orig_owner != owner) || strcmp(sock_owner, owner)) &&
				(ignore_domain_mismatch_when_setting_owner || MATCH == strcmp(scheduler.uidDomain(), Q_SOCK->getDomain())))
			{
				auto new_user = std::string("\"") + owner + "@" + scheduler.uidDomain() + "\"";
				SetAttribute(cluster_id, proc_id, ATTR_USER, new_user.c_str());
			}
		} else {
				// If we got this far, we're allowing the given value for
				// ATTR_OWNER to be set.  However, now, we should try to
				// insert a value for ATTR_USER, too, so that's always in
				// the job queue.
			auto new_user = std::string("\"") + owner + "@" + scheduler.uidDomain() + "\"";
			SetAttribute(cluster_id, proc_id, ATTR_USER, new_user.c_str());
		}

			// Also update the owner history hash table to track all OS usernames
			// that have jobs in this schedd.
		if (is_local_user) {
			AddOwnerHistory(owner);
		}

	#ifdef USE_JOB_QUEUE_USERREC
		// we do this when ATTR_USER is set, not when ATTR_OWNER is set
	#else
		if (job && ! user_is_the_new_owner) {
			// if editing (rather than creating) a job, update ownerinfo pointer, and mark submitterdata as dirty
			job->ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(owner));
			job->dirty_flags |= JQJ_CACHE_DIRTY_SUBMITTERDATA;
		}
		if (jobset && ! user_is_the_new_owner) {
			// if editing (rather than creating) a jobset, update ownerinfo pointer
			jobset->ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(owner));
			// TODO: update the jobsets alias map
		}
	#endif
	}
	else if (attr_id == idATTR_USER) {

	#ifdef USE_JOB_QUEUE_USERREC
		const char * sock_user = EffectiveUserName(Q_SOCK);

			// User is set to UNDEFINED indicating we should just pull
			// this information from the authenticated socket.
		if (YourStringNoCase("UNDEFINED") == attr_value) {
			if (sock_user && *sock_user) {
				formatstr(new_value, "\"%s\"", sock_user);
				attr_value = new_value.c_str();
			} else {
				// socket not authenticated and Owner is UNDEFINED.
				dprintf(D_ALWAYS, "ERROR SetAttribute violation: "
					"Owner is UNDEFINED, but client not authenticated\n");
				if (err) err->pushf("QMGMT", EACCES, "Client is not authenticated "
					"and owner is undefined");
				errno = EACCES;
				return -1;
			}
		} else {
	#else
		const char * sock_user = Q_SOCK->getRealUser();

			// User is set to UNDEFINED indicating we should just pull
			// this information from the authenticated socket.
		if (MATCH == strcasecmp("UNDEFINED", attr_value)) {
			formatstr(new_value, "\"%s\"", sock_user);
			attr_value = new_value.c_str();
		} else if (user_is_the_new_owner) {
	#endif

			const char * user = sock_user;
			std::string user_buf(attr_value);

				// Strip out the quote characters as with ATTR_OWNER
			user_buf = attr_value;
			bool user_is_valid = false;
			if ((user_buf.size() > 2) && (user_buf[0] == '"') && user_buf[user_buf.size()-1] == '"' )
			{
				user_buf = user_buf.substr(1, user_buf.size() - 2);
				user_is_valid = user_buf.find('"') == std::string::npos;
			}

			if (!user_is_valid) {
				dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting User to %s which is not a valid string\n",
					attr_value);
				errno = EACCES;
				return -1;
			}
			user = user_buf.c_str();

			std::string orig_user;
			if (GetAttributeString(cluster_id, proc_id, ATTR_USER, orig_user) >= 0
				&& orig_user != user
				&& !qmgmt_all_users_trusted )
			{
				dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting User to \"%s\" when previously set to \"%s\"\n",
					user, orig_user.c_str());
				errno = EACCES;
				return -1;
			}

				// Since this value does not map to a Unix username, we are more
				// permissive than with the Owner attribute and do not check
				// SuperUserAllowedToSetOwnerTo
			if (!qmgmt_all_users_trusted
				&& !is_same_user(user, sock_user, COMPARE_DOMAIN_DEFAULT, scheduler.uidDomain())
				&& !isQueueSuperUserName(sock_user))
			{
				dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting User to \"%s\" when active User is \"%s\"\n",
					user, sock_user);
				errno = EACCES;
				return -1;
			}

			// set a transaction trigger so that we know to fixup the job->ownerinfo after the transaction commits
			if (job || jobset) {
				JobQueue->SetTransactionTriggers(catSetOwner);
			}

			// All checks pass - "User" value is valid!
		}
	}
	else if (attr_id == idATTR_JOB_NOOP) {
		// whether the job has an IsNoopJob attribute or not is cached in the job object
		// so if this is set, we need to mark the cached value as dirty.
		if (job) { job->DirtyNoopAttr(); }
	}
	else if (attr_category & catJobId) {
		char *endptr = nullptr;
		int id = (int)strtol(attr_value, &endptr, 10);
		if (attr_id == idATTR_CLUSTER_ID && (*endptr != '\0' || id != cluster_id)) {
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ClusterId to incorrect value (%s!=%d)\n",
				attr_value, cluster_id);
			if (err) err->pushf("QMGMT", EACCES, "Setting cluster ID %d to new value "
				"(%s) is not permitted", cluster_id, attr_value);
			errno = EACCES;
			return -1;
		}
		if (attr_id == idATTR_PROC_ID && (*endptr != '\0' || id != proc_id)) {
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ProcId to incorrect value (%s!=%d)\n",
				attr_value, proc_id);
			if (err) err->pushf("QMGMT", EACCES, "Setting proc ID %d to new value (%s) "
				"is not permitted", proc_id, attr_value);
			errno = EACCES;
			return -1;
		}
		if (attr_id == idATTR_JOB_SET_ID) {
			if (jobset) {
				dprintf(D_ALWAYS, "SetAttribute security violation: cannot change JobSetId of existing jobset\n");
				errno = EACCES;
				return -1;
			} 
			if (job) {
				// TODO: allow id to be set to existing set owned by this user
				dprintf(D_ALWAYS, "SetAttribute security violation: cannot change JobSetId of existing job\n");
				errno = EACCES;
				return -1;
			}
			if (*endptr != '\0' || id <= 0) {
				dprintf(D_ALWAYS, "SetAttribute security violation: setting JobSetId to incorrect value (%s)\n", attr_value);
				errno = EACCES;
				return -1;
			}
		}
	}
	else if (attr_category & catCron) {
		//
		// CronTab Attributes
		// Check to see if this attribute is one of the special
		// ones used for defining a crontab schedule. If it is, then
		// we add the cluster_id to a queue maintained by the schedd.
		// This is because at this point we may not have the proc_id, so
		// we can't add the full job information. The schedd will later
		// take this cluster_id and look at all of its procs to see if 
		// need to be added to the main CronTab list of jobs.
		//
		//PRAGMA_REMIND("tj: move this into the cluster job object?")
		scheduler.addCronTabClusterId( cluster_id );
	} else if (attr_id == idATTR_NUM_JOB_RECONNECTS) {
		int curr_cnt = 0;
		int new_cnt = (int)strtol( attr_value, nullptr, 10 );
		PROC_ID job_id( cluster_id, proc_id );
		shadow_rec *srec = scheduler.FindSrecByProcID(job_id);
		GetAttributeInt( cluster_id, proc_id, ATTR_NUM_JOB_RECONNECTS, &curr_cnt );
		if ( srec && srec->is_reconnect && !srec->reconnect_done &&
			 new_cnt > curr_cnt ) {

			srec->reconnect_done = true;
			scheduler.stats.JobsRestartReconnectsSucceeded += 1;
			scheduler.stats.JobsRestartReconnectsAttempting += -1;
		}
	}
	else if (attr_id == idATTR_JOB_STATUS) {
			// If the status is being set, let's record the previous
			// status, but only if it's different.
			// When changing the status of a HELD job that was previously
			// REMOVED, enforce that it has to go back to REMOVED.
		int curr_status = 0;
		int last_status = 0;
		int release_status = 0;
		int new_status = (int)strtol( attr_value, nullptr, 10 );

		GetAttributeInt( cluster_id, proc_id, ATTR_JOB_STATUS, &curr_status );
		GetAttributeInt( cluster_id, proc_id, ATTR_LAST_JOB_STATUS, &last_status );
		GetAttributeInt( cluster_id, proc_id, ATTR_JOB_STATUS_ON_RELEASE, &release_status );

		if ( new_status != HELD && new_status != REMOVED &&
			 ( curr_status == REMOVED || last_status == REMOVED ||
			   release_status == REMOVED ) ) {
			dprintf( D_ALWAYS, "SetAttribute violation: Attempt to change %s of removed job %d.%d to %d\n",
					 ATTR_JOB_STATUS, cluster_id, proc_id, new_status );
			errno = EINVAL;
			if (err) err->pushf("QMGMT", EINVAL, "Changing %s of removed job %d.%d to "
				"%d is invalid", ATTR_JOB_STATUS, cluster_id, proc_id, new_status);
			return -1;
		}
		if (query_can_change_only) {
			return 0;
		}
		if ( curr_status == REMOVED && new_status == HELD &&
			 release_status != REMOVED ) {
			SetAttributeInt( cluster_id, proc_id, ATTR_JOB_STATUS_ON_RELEASE, REMOVED, flags );
		}
		if ( new_status != curr_status && curr_status > 0 ) {
			SetAttributeInt( cluster_id, proc_id, ATTR_LAST_JOB_STATUS, curr_status, flags );
		}
	}
	else if (attr_id == idATTR_HOLD_REASON_CODE || attr_id == idATTR_HOLD_REASON) {
		// if the hold reason is set to one of the magic values that indicate a hold for spooling
		// data, we want to attach a trigger to the transaction so we know to do filepath fixups
		// after the transaction is committed.
		// if the hold reason code is NOT spooling for data, then we want to update aggregates
		// on the number of holds and number of holds per reason.
		bool is_spooling_hold = false;
		if (attr_id == idATTR_HOLD_REASON_CODE) {
			int hold_reason = (int)strtol( attr_value, nullptr, 10 );
			is_spooling_hold = (CONDOR_HOLD_CODE::SpoolingInput == hold_reason);
			if (!is_spooling_hold) {
				// Update count in job ad of how many times job was put on hold
				incrementJobAdAttr(cluster_id, proc_id, ATTR_NUM_HOLDS);

				// Update count per hold reason in the job ad.
				// If the reason_code int is not a valid CONDOR_HOLD_CODE enum, an exception will be thrown.
				try {
					incrementJobAdAttr(cluster_id, proc_id, (CONDOR_HOLD_CODE::_from_integral(hold_reason))._to_string(), ATTR_NUM_HOLDS_BY_REASON);
				}
				catch (std::runtime_error const&) {
					// Somehow reason_code is not a valid hold reason, so consider it as Unspecified here.
					incrementJobAdAttr(cluster_id, proc_id, (+CONDOR_HOLD_CODE::Unspecified)._to_string(), ATTR_NUM_HOLDS_BY_REASON);
				}
			}
		} else if (attr_id == idATTR_HOLD_REASON) {
			is_spooling_hold = YourString("Spooling input data files") == attr_value;
		}
		if (is_spooling_hold) {
			// set a transaction trigger for spooling hold
			JobQueue->SetTransactionTriggers(catSpoolingHold);
		}
	}

	// All of the checking and validation is done, if we are only querying whether we
	// can change the value, then return now with success.
	if (query_can_change_only) {
		return 0;
	}

	if (job) {
		// if modifying a cluster ad, and the PostSubmitClusterChange flag was passed to SetAttribute
		// then force a PostSubmitClusterChange tansaction trigger.
		if (job->IsCluster() && (flags & SetAttribute_PostSubmitClusterChange)) {
			attr_category |= (catCallbackTrigger | catPostSubmitClusterChange);
		}

		// give the autocluster code a chance to invalidate (or rebuild)
		// based on the changed attribute.
		if (scheduler.autocluster.preSetAttribute(*job, attr_name, attr_value, flags)) {
			DirtyPrioRecArray();
			dprintf(D_FULLDEBUG,
					"Prioritized runnable job list will be rebuilt, because "
					"ClassAd attribute %s=%s changed\n",
					attr_name,attr_value);
		}
	}

	// This block handles rounding of attributes.
	std::string round_param_name;
	round_param_name = "SCHEDD_ROUND_ATTR_";
	round_param_name += attr_name;

	char *round_param = param(round_param_name.c_str());

	if( round_param && *round_param && strcmp(round_param,"0") ) {
		classad::Value::ValueType attr_type = classad::Value::NULL_VALUE;
		ExprTree *tree = nullptr;
		classad::Value val;
		if ( ParseClassAdRvalExpr(attr_value, tree) == 0 &&
			(dynamic_cast<classad::Literal *>(tree) != nullptr)) {
			(dynamic_cast<classad::Literal *>(tree))->GetValue( val );
			if ( val.GetType() == classad::Value::INTEGER_VALUE ) {
				attr_type = classad::Value::INTEGER_VALUE;
			} else if ( val.GetType() == classad::Value::REAL_VALUE ) {
				attr_type = classad::Value::REAL_VALUE;
			}
		}
		delete tree;

		if ( attr_type == classad::Value::INTEGER_VALUE || attr_type == classad::Value::REAL_VALUE ) {
			// first, store the actual value
			std::string raw_attribute = attr_name;
			raw_attribute += "_RAW";
			JobQueue->SetAttribute(key, raw_attribute.c_str(), attr_value, flags & SetAttribute_SetDirty);
			if( flags & SHOULDLOG ) {
				char* old_val = nullptr;
				ExprTree *ltree = nullptr;
				ltree = job->LookupExpr(raw_attribute);
				if( ltree ) {
					old_val = const_cast<char*>(ExprTreeToString(ltree));
				}
				scheduler.WriteAttrChangeToUserLog(key.c_str(), raw_attribute.c_str(), attr_value, old_val);
			}

			int64_t lvalue = 0;
			double fvalue = 0.0;

			if ( attr_type == classad::Value::INTEGER_VALUE ) {
				val.IsIntegerValue( lvalue );
				fvalue = lvalue;
			} else {
				val.IsRealValue( fvalue );
				lvalue = (int64_t) fvalue;	// truncation conversion
			}

			if( strstr(round_param,"%") ) {
					// round to specified percentage of the number's
					// order of magnitude
				char *end=nullptr;
				double percent = strtod(round_param,&end);
				if( !end || end[0] != '%' || end[1] != '\0' ||
					percent > 1000 || percent < 0 )
				{
					EXCEPT("Invalid rounding parameter %s=%s",
						   round_param_name.c_str(),round_param);
				}
				if( fabs(fvalue) < 0.000001 || percent < 0.000001 ) {
					new_value = attr_value; // unmodified
				}
				else {
						// find the closest power of 10
					int magnitude = (int)(log(fabs((double)fvalue/5))/log((double)10) + 1);
					double roundto = pow((double)10,magnitude) * percent/100.0;
					fvalue = ceil( fvalue/roundto )*roundto;

					if( attr_type == classad::Value::INTEGER_VALUE ) {
						new_value = std::to_string((int64_t)fvalue);
					}
					else {
						new_value = std::to_string(fvalue);
					}
				}
			}
			else {
					// round to specified power of 10
				unsigned int base = 0;
				int exp = param_integer(round_param_name.c_str(),0,0,9);

					// now compute the rounded value
					// set base to be 10^exp
				for (base=1 ; exp > 0; exp--, base *= 10) { }

					// round it.  note we always round UP!!  
				lvalue = ((lvalue + base - 1) / base) * base;

					// make it a string
				new_value = std::to_string( lvalue );

					// if it was a float, append ".0" to keep it real
				if ( attr_type == classad::Value::REAL_VALUE ) {
					new_value += ".0";
				}
			}

			// change attr_value, so when we do the SetAttribute below
			// we really set to the rounded value.
			attr_value = new_value.c_str();

		} else {
			dprintf(D_FULLDEBUG,
				"%s=%s, but value '%s' is not a scalar - ignored\n",
				round_param_name.c_str(),round_param,attr_value);
		}
	}
	free( round_param );

	if( !PrioRecArrayIsDirty ) {
		if (attr_category & catDirtyPrioRec) {
			DirtyPrioRecArray();
		} else if(attr_id == idATTR_JOB_STATUS) {
			if( atoi(attr_value) == IDLE ) {
				DirtyPrioRecArray();
			}
		}
		if(PrioRecArrayIsDirty) {
			dprintf(D_FULLDEBUG,
					"Prioritized runnable job list will be rebuilt, because "
					"ClassAd attribute %s=%s changed\n",
					attr_name,attr_value);
		}
	}
	if (attr_category & catSubmitterIdent) {
		if (job) { job->dirty_flags |= JQJ_CACHE_DIRTY_SUBMITTERDATA; }
	}

	if (attr_category & catCallbackTrigger) {
		// remember what callbacks to call when the transaction is committed.
		int triggers = JobQueue->SetTransactionTriggers(attr_category & 0xFFF);
		if (0 == triggers) { // not inside a transaction, triggers will not be recorded... so promote it to trigger NOW
			attr_category |= catCallbackNow;
		}
	}

	int old_nondurable_level = 0;
	if( flags & NONDURABLE ) {
		old_nondurable_level = JobQueue->IncNondurableCommitLevel();
	}

	JobQueue->SetAttribute(key, attr_name, attr_value, flags & SetAttribute_SetDirty);
	if( flags & SHOULDLOG ) {
		const char* old_val = nullptr;
		if (job) {
			ExprTree *tree = job->LookupExpr(attr_name);
			if (tree) { old_val = ExprTreeToString(tree); }
		}
		scheduler.WriteAttrChangeToUserLog(key.c_str(), attr_name, attr_value, old_val);
	}

	int status = 0;
	if( flags & NONDURABLE ) {
		JobQueue->DecNondurableCommitLevel( old_nondurable_level );

		ScheduleJobQueueLogFlush();
	}

	// future
	if (attr_category & catJobObj) { if (job) { job->dirty_flags |= JQJ_CACHE_DIRTY_JOBOBJ; } }

	// Get the job's status and only mark dirty if it is running
	// Note: Dirty attribute notification could work for local and
	// standard universe, but is currently not supported for them.
	int universe = -1;
	GetAttributeInt( cluster_id, proc_id, ATTR_JOB_STATUS, &status );
	GetAttributeInt( cluster_id, proc_id, ATTR_JOB_UNIVERSE, &universe );
	if( ( universe != CONDOR_UNIVERSE_SCHEDULER &&
		  universe != CONDOR_UNIVERSE_LOCAL ) &&
		( flags & SetAttribute_SetDirty ) && 
		( status == RUNNING || (( universe == CONDOR_UNIVERSE_GRID ) && jobExternallyManaged( job ) ) ) ) {

		// Add the key to list of dirty classads
		if( DirtyJobIDs.count( key ) == 0 &&
			SendDirtyJobAdNotification( key ) ) {

			DirtyJobIDs.insert( key );

			// Start timer to ensure notice is confirmed
			if( dirty_notice_timer_id <= 0 ) {
				dprintf(D_FULLDEBUG, "Starting dirty attribute notification timer\n");
				dirty_notice_timer_id = daemonCore->Register_Timer(
									dirty_notice_interval,
									dirty_notice_interval,
									PeriodicDirtyAttributeNotification,
									"PeriodicDirtyAttributeNotification");
			}
		}
	}

	JobQueueDirty = true;

	if (attr_category & catCallbackNow) {
		std::set<std::string> keys;
		keys.insert(key.c_str());
		// TODO: convert the keys to PROC_IDs before calling DoSetAttributeCallbacks?
		DoSetAttributeCallbacks(keys, attr_category);
	}

	// If we are changing the priority of a scheduler/local universe job, we need
	// to update the LocalJobsPrioQueue data
	if ( ( universe == CONDOR_UNIVERSE_SCHEDULER ||
		universe == CONDOR_UNIVERSE_LOCAL ) && attr_id == idATTR_JOB_PRIO) {
		if ( job ) {
			// Walk the prio queue of local jobs
			for ( auto it = scheduler.LocalJobsPrioQueue.begin(); it != scheduler.LocalJobsPrioQueue.end(); it++ ) {
				// If this record matches the job_id passed in, erase it, then
				// add a new record with the updated priority value. The
				// LocalJobsPrioQueue std::set will automatically order it.
				if ( it->job_id.cluster == cluster_id && it->job_id.proc == proc_id ) {
					scheduler.LocalJobsPrioQueue.erase( it );
					JOB_ID_KEY jid = JOB_ID_KEY( cluster_id, proc_id );
					LocalJobRec rec = LocalJobRec( atoi ( attr_value ), jid );
					scheduler.LocalJobsPrioQueue.insert( rec );
					break;
				}
			}
		}
	}

	return 0;
}


// For now this just updates counters for idle/running/held jobs
// but in the future it could dispatch various callbacks based on the flags in triggers.
//
// TODO: add general callback registration/dispatch?
//
void DoSetAttributeCallbacks(const std::set<std::string> &jobids, int triggers)
{
	JobQueueKey job_id;

	// build a set of factories listed explicitly in this transaction
	// we will process them after we handle the catStatus trigger, which may add to the set
	std::set<int> factories;
	if (triggers & catMaterializeState) {
		for (const auto & jobid : jobids) {
			if (! job_id.set(jobid.c_str()) || job_id.cluster <= 0 || job_id.proc >= 0) continue; // cluster ads only
			JobQueueCluster * cad = GetClusterAd(job_id);
			if (! cad) continue; // Ignore if no cluster ad (yet). this happens on submit commits.

			ASSERT(cad->IsCluster());

			// remember the ids of factory clusters
			if (cad->factory) {
				factories.insert(cad->jid.cluster);
			}
		}
	}

	// this trigger happens when the JobStatus attribute of a job is set
	if (triggers & catStatus) {
		for (const auto & jobid : jobids) {
			if ( ! job_id.set(jobid.c_str()) || job_id.cluster <= 0 || job_id.proc < 0) continue; // ignore the cluster ad and '0.0' ad

			JobQueueJob * job = nullptr;
			if ( ! JobQueue->Lookup(job_id, job)) continue; // Ignore if no job ad (yet). this happens on submit commits.

			int universe = job->Universe();
			if ( ! universe) {
				dprintf(D_ALWAYS, "job %s has no universe! in DoSetAttributeCallbacks\n", jobid.c_str());
				continue;
			}

			int job_status = 0;
			job->LookupInteger(ATTR_JOB_STATUS, job_status);
			if (job_status != job->Status()) {

				// update jobsets aggregates for this status change
				if (scheduler.jobSets) {
					scheduler.jobSets->status_change(*job, job_status);
				}

				if (job->ownerinfo) {
					IncrementLiveJobCounter(job->ownerinfo->live, universe, job->Status(), -1);
					IncrementLiveJobCounter(job->ownerinfo->live, universe, job_status, 1);
				}
				//if (job->submitterdata) {
				//	IncrementLiveJobCounter(job->submitterdata->live, universe, job->Status(), -1);
				//	IncrementLiveJobCounter(job->submitterdata->live, universe, job_status, 1);
				//}

				IncrementLiveJobCounter(scheduler.liveJobCounts, universe, job->Status(), -1);
				IncrementLiveJobCounter(scheduler.liveJobCounts, universe, job_status, 1);

				JobQueueCluster* cad = job->Cluster();
				if (cad) {
					// track counts of jobs in various states in the JobQueueCluster class
					cad->JobStatusChanged(job->Status(), job_status);

					// if there is a factory on this cluster, add it to the set of factories to check
					// we do this so that a change of state for a job (idle -> running) can trigger new materialization
					if (cad->factory) {
						factories.insert(cad->jid.cluster);
						triggers |= catMaterializeState;
					}
				}
				job->SetStatus(job_status);
			}
		}
	}

	// check factory clusters to see if there was a state change that justifies new job materialization
	if (scheduler.getAllowLateMaterialize()) {
		if (triggers & catMaterializeState) {
			for (int factory : factories) {
				JobQueueCluster * cad = GetClusterAd(factory);
				if ( ! cad) continue; // Ignore if no cluster ad (yet). this happens on submit commits.

				ASSERT(cad->IsCluster());

				// ignore non-factory clusters
				if ( ! cad->factory) continue;

				// fetch the committed value of the factory pause state, it should be
				// one of mmInvalid=-1, mmRunning=0, mmHold=1, mmNoMoreItems=2
				int paused = mmRunning;
				if ( ! cad->LookupInteger(ATTR_JOB_MATERIALIZE_PAUSED, paused)) {
					paused = mmRunning;
				}

				// sync up the factory pause state with the commited value of the ATTR_JOB_MATERIALIZE_PAUSED attribute
				// if the pause state was changed to mmRunning then we want to schedule materialization for this cluster
				if (CheckJobFactoryPause(cad->factory, paused)) {
					if (paused == mmRunning) {
						// we expect to get here when the factory pause state changes to running.
						ScheduleClusterForJobMaterializeNow(cad->jid.cluster);
					}
				} else if  ((paused == mmRunning) && HasMaterializePolicy(cad)) {
					// we expect to get here when a job starts running and the factory is not paused/completed
					ScheduleClusterForJobMaterializeNow(cad->jid.cluster);
				}
			}
		}

		// note, the catNewMaterialize trigger handling for *new* clusters can't be handled here
		// we will deal with that one later.
	}

}


bool
SendDirtyJobAdNotification(const PROC_ID & job_id)
{
	int pid = -1;

	shadow_rec *srec = scheduler.FindSrecByProcID(job_id);
	if( srec ) {
		pid = srec->pid;
	}
	else {
		pid = scheduler.FindGManagerPid(job_id);
	}

	if ( pid <= 0 ) {
		dprintf(D_ALWAYS, "Failed to send signal %s, no job manager found\n", getCommandString(UPDATE_JOBAD));
		return false;
	} else if ( DirtyPidsSignaled.find(pid) == DirtyPidsSignaled.end() ) {
		dprintf(D_FULLDEBUG, "Sending signal %s, to pid %d\n", getCommandString(UPDATE_JOBAD), pid);
		classy_counted_ptr<DCSignalMsg> msg = new DCSignalMsg(pid, UPDATE_JOBAD);
		daemonCore->Send_Signal_nonblocking(msg.get());
		DirtyPidsSignaled.insert(pid);
	}

	return true;
}

void
PeriodicDirtyAttributeNotification(int /* tid */)
{
	DirtyPidsSignaled.clear();

	auto key_itr = DirtyJobIDs.begin();
	while (key_itr != DirtyJobIDs.end()) {
		if ( SendDirtyJobAdNotification(*key_itr) == false ) {
			// There's no shadow/gridmanager for this job.
			// Mark it clean and remove it from the dirty list.
			// We can't call MarkJobClean() here, as that would
			// disrupt our traversal of DirtyJobIDs.
			JobQueue->ClearClassAdDirtyBits(*key_itr);
			key_itr = DirtyJobIDs.erase(key_itr);
		} else {
			key_itr++;
		}
	}

	if( DirtyJobIDs.empty() && dirty_notice_timer_id > 0 )
	{
		dprintf(D_FULLDEBUG, "Cancelling dirty attribute notification timer\n");
		daemonCore->Cancel_Timer(dirty_notice_timer_id);
		dirty_notice_timer_id = -1;
	}

	DirtyPidsSignaled.clear();
}

void
ScheduleJobQueueLogFlush()
{
		// Flush the log after a short delay so that we avoid spending
		// a lot of time waiting for the disk but we also make things
		// visible to JobRouter within a maximum delay.
	if( flush_job_queue_log_timer_id == -1 ) {
		flush_job_queue_log_timer_id = daemonCore->Register_Timer(
			flush_job_queue_log_delay,
			HandleFlushJobQueueLogTimer,
			"HandleFlushJobQueueLogTimer");
	}
}

void
HandleFlushJobQueueLogTimer(int /* tid */)
{
	flush_job_queue_log_timer_id = -1;
	JobQueue->FlushLog();
}

int
SetTimerAttribute( int cluster, int proc, const char *attr_name, int dur )
{
	int rc = 0;
	if ( xact_start_time == 0 ) {
		dprintf(D_ALWAYS,
				"SetTimerAttribute called for %d.%d outside of a transaction!\n",
				cluster, proc);
		errno = EINVAL;
		return -1;
	}

	rc = SetAttributeInt( cluster, proc, attr_name, xact_start_time + dur, SetAttribute_SetDirty );
	return rc;
}


// start a transaction, or continue one if we already started it
int TransactionWatcher::BeginOrContinue(int id) {
	ASSERT(! completed);
	if (started) {
		lastid = id;
		return 0;
	} else {
		ASSERT( ! JobQueue->InTransaction());
	}
	started = true;
	firstid = lastid = id;
	return BeginTransaction();
}

// commit if we started a transaction
int TransactionWatcher::CommitIfAny(SetAttributeFlags_t flags, CondorError * errorStack)
{
	int rval = 0;
	if (started && ! completed) {
		rval = CommitTransactionAndLive(flags, errorStack);
		if (rval == 0) {
			// Only set the completed flag if our CommitTransaction succeeded... if
			// it failed (perhaps due to SUBMIT_REQUIREMENTS not met), we cannot mark it
			// as completed because that means we will not abort it in the 
			// Transactionwatcher::AbortIfAny() method below and will likely ASSERT.
			completed = true;
		}
	}
	return rval;
}

// cancel if we started a transaction
int TransactionWatcher::AbortIfAny()
{
	int rval = 0;
	if (started && ! completed) {
		ASSERT(JobQueue->InTransaction());
		rval = AbortTransaction();
		completed = true;
	}
	return rval;
}


bool
InTransaction()
{
	return JobQueue->InTransaction();
}

int
BeginTransaction()
{
	jobs_added_this_transaction = 0;
	JobQueue->BeginTransaction();

	// note what time we started the transaction (used by SetTimerAttribute())
	xact_start_time = time( nullptr );

	return 0;
}

int
CheckTransaction( const std::list<std::string> &newAdKeys,
                  CondorError * errorStack )
{
	int rval = 0;

	int triggers = JobQueue->GetTransactionTriggers();
	bool has_spooling_hold = (triggers & catSpoolingHold) != 0;
	bool has_job_factory = (triggers & catNewMaterialize) != 0;

	// If we don't need to perform any submit_requirement checks
	// and we don't need to perform any job transforms, then we should
	// bail out now and avoid all the expensive computation below.
	if ( !scheduler.shouldCheckSubmitRequirements() &&
		 !scheduler.jobTransforms.shouldTransform() &&
		 !has_spooling_hold)
	{
		return 0;
	}

	// in 9.4.0 we started applying transforms to the cluster ad of late materiaization factories
	// We did this because transforms must be applied before submit requirements are evaluated (HTCONDOR-756)
	// This resulted in a problem (HTCONDOR-1369) where a transform of TransferInput would prevent
	// materialization of per-job values for TransferInput. Beginning with 10.0 we will transform proc ads
	// in addition to cluster ads for factory jobs. (i.e. The transforms will be applied twice for each late-mat job.)
	// The knob below is so admins can revert to the behavior of applying transforms to the factory cluster only
	// even though this has the side effect of overriding materialization of those attributes in the job ads.
	// An admin might do this if they want to insure that late materialization will not fail submit requirments
	// In effect, they are making attributes that are set by a submit transform pseudo-immutable. (the root cause of HTCONDOR-1369)
	// TODO: make it possible to declare only some transforms as cluster-only
	bool transform_factory_and_job = param_boolean("TRANSFORM_FACTORY_AND_JOB_ADS", true);

	for(const auto & newAdKey : newAdKeys) {
		bool do_transforms = true;
		ClassAd tmpAd, tmpAd2;
		ClassAd * procAd = &tmpAd;
		classad::References tmpAttrs, *xform_attrs = nullptr;
		JobQueueKey jid( newAdKey.c_str() );
		if (jid.proc < -1 || jid.cluster <= 0) {
			// ignore jobset ads for now
			continue;
		}
		if (jid.proc == -1) { // is this is a cluster ad?
			// we don't transform non-factory cluster ads (for now)
			if (! has_job_factory)
				continue;
			JobQueue->AddAttrsFromTransaction( jid, tmpAd2 );
			// build a fake temporary proc ad for transforms and requirements
			// stuff the factory id into that ad so that it is possible
			// to write a transform or requirement that only applies to factories
			tmpAd.Assign(ATTR_PROC_ID, 0);
			tmpAd.Assign("JobFactoryId", jid.cluster);
			if (!tmpAd2.Lookup(ATTR_JOB_STATUS)) {
				tmpAd.Assign(ATTR_JOB_STATUS, IDLE);
				tmpAd.ChainToAd(&tmpAd2);
			}
			// if we are going to transform the factory only, then we want to know
			// what attrbutes are transformed in the cluster/factory so we can disable
			// materialization of those attributes in the proc ads
			if ( ! transform_factory_and_job) {
				xform_attrs = &tmpAttrs;
			}
		} else {
			JobQueueKeyBuf clusterJid( jid.cluster, -1 );
			JobQueue->AddAttrsFromTransaction( clusterJid, tmpAd );
			JobQueue->AddAttrsFromTransaction( jid, tmpAd );

			JobQueueJob *clusterAd = nullptr;
			if (JobQueue->Lookup(clusterJid, clusterAd)) {
				// If there is a cluster ad in the job queue, chain to that before we evaluate anything.
				// we don't need to unchain - it's a stack object and won't live longer than this function.
				tmpAd.ChainToAd(clusterAd);
				if (dynamic_cast<JobQueueCluster*>(clusterAd)->factory) {
					// we already transformed the factory cluster ad, disable transforms
					// for proc ads if ...and_jobs is false
					do_transforms = transform_factory_and_job;
				}
			}
		}

		// Now that we created a procAd out of the transaction queue,
		// apply job transforms to the procAd.
		// If the transforms fail, bail on the transaction.
		if (do_transforms) {
			rval = scheduler.jobTransforms.transformJob(procAd, jid, xform_attrs, errorStack, has_job_factory);
			if  (rval < 0) {
				if ( errorStack ) {
					errorStack->push( "QMGMT", 30, "Failed to apply a required job transform.\n");
				}
				errno = EINVAL;
				return rval;
			}

			// when transforming a cluster ad, we need to add transformed attributes into the
			// EditedClustrAttrs attribute to prevent job materialization from changing them.
			if (jid.proc == -1 && xform_attrs && !xform_attrs->empty()) {
				std::string cur_list, new_list;
				if (tmpAd2.LookupString(ATTR_EDITED_CLUSTER_ATTRS, cur_list)) {
					add_attrs_from_string_tokens(*xform_attrs, cur_list);
				}
				// store the new value if it changed
				print_attrs(new_list, false, *xform_attrs, ",");
				if (new_list != cur_list) {
					// wrap quotes around the attribute list
					new_list.insert(0, "\"");
					new_list += "\"";
					SetAttribute(jid.cluster, jid.proc, ATTR_EDITED_CLUSTER_ATTRS, new_list.c_str(), SetAttribute_SubmitTransform);
				}
			}
		}

		// Now check that submit_requirements still hold on our (possibly transformed)
		// job ad.
		rval = scheduler.checkSubmitRequirements( procAd, errorStack );
		if( rval < 0 ) {
			errno = EINVAL;
			return rval;
		}

		if (has_spooling_hold) {
			// If this submit put one or more jobs into the special hold for spooling, we
			// need to rewrite some job attributes here..
			//
			// This might be more correct to do be fore before checkSubmitRequirements,
			// but before 8.7.2 it happened after the submit transaction had been committed
			// so the conservative changes puts it here.
			rewriteSpooledJobAd(procAd, jid.cluster, jid.proc, false);
		}
	}

	return 0;
}


// Call this just before committing a submit transaction, it will figure out
// the number of procs in each new cluster and add the ATTR_TOTAL_SUBMIT_PROCS attribute to the commit.
//
void SetSubmitTotalProcs(std::list<std::string> & new_ad_keys)
{
	std::map<int, int> num_procs;

	// figure out the max proc id for each cluster
	JobQueueKeyBuf job_id;
	for(auto & new_ad_key : new_ad_keys) {
		job_id.set(new_ad_key.c_str());
		if (job_id.proc < 0 || job_id.cluster <= 0) continue; // ignore the cluster ad and set ads

		// ignore jobs produced by an existing factory.
		// ATTR_TOTAL_SUBMIT_PROCS is determined by the factory for them.
		JobQueueCluster *clusterad = GetClusterAd(job_id);
		if (clusterad && clusterad->factory) {
			continue;
		}

		auto mit = num_procs.find(job_id.cluster);
		if (mit == num_procs.end()) {
			num_procs[job_id.cluster] = 1;
		} else {
			num_procs[job_id.cluster] += 1;
		}
	}

	// add the ATTR_TOTAL_SUBMIT_PROCS attributes to the transaction
	char number[10];
	for (auto & num_proc : num_procs) {
		job_id.set(num_proc.first, -1);
		snprintf(number, sizeof(number), "%d", num_proc.second);
		JobQueue->SetAttribute(job_id, ATTR_TOTAL_SUBMIT_PROCS, number, false);
	}
}

// Call this just before committing a submit transaction, it will figure out
// the number of procs in each new cluster and add the ATTR_TOTAL_SUBMIT_PROCS attribute to the commit.
//
void AddClusterEditedAttributes(std::set<std::string> & ad_keys)
{
	std::map<JobQueueKey, std::string> to_add; // things we want add to the current transaction

	// examine the current transaction, and add any attributes set or deleted in the cluster ad(s)
	// to the EditedClusterAttrs attribute for that cluster.
	// in this loop we build up a map of clusters an the new value for EditedClusterAttrs
	// then AFTER we have examined the whole transaction,
	// we add records to set new values for EditedClusterAttrs if needed
	JobQueueKey job_id;
	for(const auto & ad_key : ad_keys) {
		job_id.set(ad_key.c_str());
		if (job_id.proc >= 0) continue; // skip keys for jobs, we want cluster keys only

		JobQueueJob *job = nullptr;
		if ( ! JobQueue->Lookup(job_id, job)) continue; // skip keys for which the cluster is still uncommitted

		if ( ! job || ! job->IsCluster()) continue; // just a safety check, we don't expect this to fire.
		auto *cad = dynamic_cast<JobQueueCluster*>(job);

		// get the attrs modified in this transaction
		classad::References attrs;
		if (JobQueue->AddAttrNamesFromTransaction(job_id, attrs)) {
			// if the transaction already contains a SetAttribute or DeleteAttribute
			// of ATTR_EDITED_CLUSTER_ATTRS just skip this cluster without merging new attributes
			// we do this so that when you qedit EditedClusterAttrs, the value passed to qedit wins.
			if (attrs.find(ATTR_EDITED_CLUSTER_ATTRS) != attrs.end()) {
				cad->dirty_flags |= JQJ_CACHE_DIRTY_CLUSTERATTRS; // ATTR_EDITED_CLUSTER_ATTRS changed explictly by user
				continue;
			}

			// merge in the current set from ATTR_EDITED_CLUSTER_ATTRS
			std::string cur_list, new_list;
			if (cad->LookupString(ATTR_EDITED_CLUSTER_ATTRS, cur_list)) {
				add_attrs_from_string_tokens(attrs, cur_list);
			}
			// check to see if there are any changes. if there are, put the new value into
			// the map of things to add to this transaction
			print_attrs(new_list, false, attrs, ",");
			if (new_list != cur_list) {
				// wrap quotes around the attribute list
				new_list.insert(0, "\"");
				new_list += "\"";
				to_add[job_id] = new_list;
				cad->dirty_flags |= JQJ_CACHE_DIRTY_CLUSTERATTRS; // ATTR_EDITED_CLUSTER_ATTRS will be changed soon.
			}
		}
	}

	// add the new values for ATTR_EDITED_CLUSTER_ATTRS to the current transaction.
	for (auto & it : to_add) {
		SetAttribute(it.first.cluster, it.first.proc, ATTR_EDITED_CLUSTER_ATTRS, it.second.c_str(), 0, nullptr);
	}
}

bool
ReadProxyFileIntoAd( const char *file, const char *owner, ClassAd &x509_attrs )
{
#if defined(WIN32)
	(void)file;
	(void)owner;
	(void)x509_attrs;
	return false;
#else
	// owner==NULL means don't try to switch our priv state.
	TemporaryPrivSentry tps( owner != nullptr );
	if ( owner != nullptr ) {
		if ( !init_user_ids( owner, nullptr ) ) {
			dprintf( D_ERROR, "ReadProxyFileIntoAd(%s): Failed to switch to user priv\n", owner );
			return false;
		}
		set_user_priv();
	}

	StatInfo si( file );
	if ( si.Error() == SINoFile ) {
		// If the file doesn't exist, it may be spooled later.
		// Return without printing an error.
		return false;
	}

	X509Credential* proxy_handle = x509_proxy_read( file );

	if ( proxy_handle == nullptr ) {
		dprintf( D_ERROR, "Failed to read job proxy: %s\n",
				 x509_error_string() );
		return false;
	}

	time_t expire_time = x509_proxy_expiration_time( proxy_handle );
	char *proxy_identity = x509_proxy_identity_name( proxy_handle );
	char *proxy_email = x509_proxy_email( proxy_handle );
	char *voname = nullptr;
	char *firstfqan = nullptr;
	char *fullfqan = nullptr;
	extract_VOMS_info( proxy_handle, 0, &voname, &firstfqan, &fullfqan );

	delete proxy_handle;

	x509_attrs.Assign( ATTR_X509_USER_PROXY_EXPIRATION, expire_time );
	x509_attrs.Assign( ATTR_X509_USER_PROXY_SUBJECT, proxy_identity );
	if ( proxy_email ) {
		x509_attrs.Assign( ATTR_X509_USER_PROXY_EMAIL, proxy_email );
	}
	if ( voname ) {
		x509_attrs.Assign( ATTR_X509_USER_PROXY_VONAME, voname );
	}
	if ( firstfqan ) {
		x509_attrs.Assign( ATTR_X509_USER_PROXY_FIRST_FQAN, firstfqan );
	}
	if ( fullfqan ) {
		x509_attrs.Assign( ATTR_X509_USER_PROXY_FQAN, fullfqan );
	}
	free( proxy_identity );
	free( proxy_email );
	free( voname );
	free( firstfqan );
	free( fullfqan );
	return true;
#endif
}

#ifdef USE_JOB_QUEUE_USERREC

// category values for aSpecialUserRecAttrs
enum {
	catUserRecForbidden = -3,	// commands that don't match ATTR_USERREC_OPT_prefix, but should still not be merged into the user ad
	catUserRecLiveValue = -2,   // attributes that are updates live and not stored in the ad
	catUserRecRequiredKey = -1, // immutable/secure attributes that are part of the key
	catUserRecDisable = 2,      // attributes related to enable/disable
};

#define FILL(attr,cat) { attr, id##attr, cat }
#define LIVE(attr) { #attr, idATTR_USERREC_LIVE, catUserRecLiveValue }
// negative value in the category field indicates that the attribute secure/immutable
static const ATTR_IDENT_PAIR aSpecialUserRecAttrs[] = {
	FILL(ATTR_USERREC_OPT_CREATE_DEPRECATED, catUserRecForbidden), // backward compat hack for 10.x and early 23.0.x
	FILL(ATTR_DISABLE_REASON,     catUserRecDisable),
	FILL(ATTR_ENABLED,            catUserRecDisable),
	FILL(ATTR_NT_DOMAIN,          catUserRecRequiredKey),
	LIVE(NumCompleted),
	LIVE(NumHeld),
	LIVE(NumIdle),
	LIVE(NumJobs),
	LIVE(NumRemoved),
	LIVE(NumRunning),
	LIVE(NumSchedulerCompleted),
	LIVE(NumSchedulerHeld),
	LIVE(NumSchedulerIdle),
	LIVE(NumSchedulerJobs),
	LIVE(NumSchedulerRemoved),
	LIVE(NumSchedulerRunning),
	LIVE(NumSuspended),
	FILL(ATTR_OWNER,              catUserRecRequiredKey),
	FILL(ATTR_REQUIREMENTS,       catUserRecForbidden), // don't want to copy the Requirements out of the command ad
	FILL(ATTR_USER,               catUserRecRequiredKey),
};
#undef FILL
#undef LIVE


static int IsSpecialUserRecAttrName(const char * attr, int& cat)
{
	const ATTR_IDENT_PAIR* found = nullptr;
	found = BinaryLookup<ATTR_IDENT_PAIR>(
		aSpecialUserRecAttrs,
		COUNTOF(aSpecialUserRecAttrs),
		attr, strcasecmp);
	if (found) {
		cat = found->category;
		return found->id;
	}
	cat = 0;
	return 0;
}


// merge and update command ad into a UserRec ad
int UpdateUserAttributes(JobQueueKey & key, const ClassAd & cmdAd, bool enabled, struct UpdateUserAttributesInfo& info )
{
	classad::ClassAdUnParser unparse;
	unparse.SetOldClassAd(true, true);
	std::string buf;

	for (auto &[attr, tree] : cmdAd) {
		if (starts_with_ignore_case(attr, ATTR_USERREC_OPT_prefix)) continue;

		// don't allow most special attributes to be set from the command ad
		// the exception is the DisableReason when creating a disabled user
		int cat=0, idAttr = IsSpecialUserRecAttrName(attr.c_str(), cat);
		if (cat < 0 || idAttr == idATTR_ENABLED ||
			(enabled && idAttr == idATTR_DISABLE_REASON)) {
			// count the number of unexpected attributes that are skipped
			// it is expected that ATTR_USER and ATTR_REQUIREMENTS will sometimes be present
			// and should be quietly ignored.
			if (cat != catUserRecForbidden && cat != catUserRecRequiredKey) {
				info.special += 1;
			}
			continue;
		}

		buf.clear();
		unparse.Unparse(buf, tree);
		if (JobQueue->SetAttribute(key, attr.c_str(), buf.c_str(), 0)) {
			info.valid += 1;
			JobQueue->SetTransactionTriggers(catSetUserRec);
		} else {
			info.invalid += 1;
		}
	}

	return 0;
}


static bool MakeUserRec(JobQueueKey & key,
	const char * user,
	const char * owner,
	const char * ntdomain,
	bool enabled,
	const ClassAd * defaults)
{
	if (( ! user || MATCH == strcmp(user, "condor@family") || MATCH == strcmp(user, "condor@child")) ||
		( ! owner || MATCH == strcmp(owner, "condor")) ||
		(ntdomain && (MATCH == strcmp(ntdomain, "family") || MATCH == strcmp(ntdomain, "child")) ))
	{
		dprintf(D_ERROR, "Error: MakeUserRec with illegal identifiers: user=%s, owner=%s, ntdomain=%s\n",
			user?user:"(null)", owner?owner:"(null)", ntdomain?ntdomain:"(null)");
		return false;
	}

	bool rval = JobQueue->NewClassAd(key, OWNER_ADTYPE) &&
		0 == SetSecureAttributeString(key.cluster, key.proc, ATTR_USER, user) &&
		0 == SetSecureAttributeString(key.cluster, key.proc, ATTR_OWNER, owner) &&
		( ! ntdomain || 0 == SetSecureAttributeString(key.cluster, key.proc, ATTR_NT_DOMAIN, ntdomain)) &&
		0 == SetSecureAttributeInt(key.cluster, key.proc, ATTR_ENABLED, enabled?1:0)
		;
	if (rval) {
		// if there is a defaults ad, store those attributes as well
		if (defaults) {
			classad::ClassAdUnParser unparse;
			unparse.SetOldClassAd( true, true );
			std::string buf;

			for (auto &[attr, tree] : *defaults) {
				buf.clear();
				unparse.Unparse(buf, tree);
				JobQueue->SetAttribute(key, attr.c_str(), buf.c_str(), 0);
			}
		}
		JobQueue->SetTransactionTriggers(catNewUser);
	}
	return rval;
}

// make a JobQueueUserRec from a (presumably pending) OwnerInfo
static bool MakeUserRec(const OwnerInfo * owni, bool enabled, const ClassAd * defaults)
{
	const char * user = owni->Name();
	const char * owner = owni->Name();
	const char * ntdomain = owni->NTDomain();
	JobQueueKey key(owni->jid);

	std::string obuf;
	if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		owner = name_of_user(user, obuf);
	} else {
		obuf = std::string(owner) + "@" + scheduler.uidDomain();
		user = obuf.c_str();
	}

	return MakeUserRec(key, user, owner, ntdomain, enabled, defaults);
}

// called during InitJobQueue to create UserRec ads that were determined to be needed by the queue

bool
CreateNeededUserRecs(const std::map<int, OwnerInfo*> &needed_owners)
{
	if (needed_owners.empty()) {
		return true;
	}

	int  fail_count = 0;
	bool already_in_transaction = InTransaction();
	if (!already_in_transaction) {
		BeginTransaction();
	}

	std::string obuf;

	// create the needed UserRecs
	const bool enabled = true;
	for (auto it : needed_owners) {
		if ( ! MakeUserRec(it.second, enabled, &scheduler.getUserRecDefaultsAd())) {
			++fail_count;
			break;
		}
		JobQueueDirty = true;
	}

	if ( ! already_in_transaction) {
		if (fail_count) {
			AbortTransaction();
		} else {
			CommitNonDurableTransactionOrDieTrying();
		}
	}

	return fail_count == 0;
}


bool UserRecDestroy(int userrec_id)
{
	if (userrec_id <= 0) { return false; }

	JobQueueKeyBuf key;
	IdToKey(USERRECID_qkey1, USERRECID_to_qkey2(userrec_id), key);

	return JobQueue->DestroyClassAd(key);
}

void UserRecFixupDefaultsAd(ClassAd & defaultsAd)
{
	classad::References badattrs;
	for (auto &[attr, tree] : defaultsAd) {
		int cat=0, idAttr = IsSpecialUserRecAttrName(attr.c_str(), cat);
		if (idAttr || starts_with_ignore_case(attr, ATTR_USERREC_OPT_prefix)) {
			badattrs.insert(attr);
		}
	}

	if (badattrs.empty())
		return;

	std::string attrs;
	for (auto & attr : badattrs) {
		defaultsAd.Delete(attr);
		if (!attrs.empty()) attrs += ", ";
		attrs += attr;
	}

	dprintf(D_ERROR, "Warning: SCHEDD_USER_DEFAULT_PROPERTIES may not have attributes %s. They will be removed.\n", attrs.c_str());
}

bool UserRecCreate(int userrec_id, const char * username, const ClassAd & cmdAd, const ClassAd & defaultsAd, bool enabled)
{
	if (userrec_id <= 0 || userrec_id >= INT_MAX) { return false; }
	JobQueueKeyBuf key;
	IdToKey(USERRECID_qkey1, USERRECID_to_qkey2(userrec_id), key);

	bool already_in_transaction = InTransaction();
	if (!already_in_transaction) {
		BeginTransaction();
	}

	std::string obuf;
	const char * owner = name_of_user(username, obuf);
	const char * user = username;
	const char * ntdomain = nullptr;
	if (owner == username) { // owner points to username when username has no @
		obuf = std::string(owner) + "@" + scheduler.uidDomain();
		user = obuf.c_str();
	}
	std::string nbuf;
	if (cmdAd.LookupString(ATTR_NT_DOMAIN, nbuf) && ! nbuf.empty()) {
		ntdomain = nbuf.c_str();
	}
#ifdef WIN32
	else {
		// if the supplied username has a domain value that does not match uidDomain
		// treat it as an NTDomain value, and rewrite the user value to be owner@uid_domain
		// this is not ideal, but it is consistent with the way things have always worked.
		YourStringNoCase domain(domain_of_user(username, scheduler.uidDomain()));
		if (domain != scheduler.uidDomain()) {
			ntdomain = domain.ptr();
			nbuf = std::string(owner) + "@" + scheduler.uidDomain();
			user = nbuf.c_str();
		}
	}
#endif

	bool rval = MakeUserRec(key, user, owner, ntdomain, enabled, nullptr);
	if (rval) {
		// do quoting using oldclassad syntax
		classad::ClassAdUnParser unparse;
		unparse.SetOldClassAd( true, true );
		std::string buf;

		// set attributes from the defaults ad if they will not just be overridden
		// by the command ad
		for (auto &[attr, tree] : defaultsAd) {
			if (cmdAd.Lookup(attr)) continue;

			buf.clear();
			unparse.Unparse(buf, tree);
			JobQueue->SetAttribute(key, attr.c_str(), buf.c_str(), 0);
		}

		// populate the new userrec with attributes from the command ad
		struct UpdateUserAttributesInfo dummy; // TODO: expose this to the caller?
		rval = UpdateUserAttributes(key, cmdAd, enabled, dummy) == 0;
	}

	if ( ! already_in_transaction) {
		if ( ! rval) {
			AbortTransaction();
		} else {
			CommitNonDurableTransactionOrDieTrying();
		}
	}

	return rval;
}

JobQueueUserRec*
GetUserRecAd(int userrec_id)
{
	if (userrec_id > 0) {
		JobQueueKey jid(USERRECID_qkey1, USERRECID_to_qkey2(userrec_id));
		JobQueueBase *bad = nullptr;
		if (JobQueue && JobQueue->Lookup(jid, bad)) {
			return dynamic_cast<JobQueueUserRec*>(bad);
		}
	}
	return nullptr;
}

#endif

// called during commitTransaction before the actual commit
// to populate a vector of new jobset ids, and also
// to look for jobsets that should be created as a side effect of creating new cluster ads
// 
// this secondary processing of clusters is to handle old versions of submit that don't know how to create jobsets explicitly
//
static void AddImplicitJobsets(const std::list<std::string> &new_ad_keys, std::vector<unsigned int> & new_jobset_ids)
{
	if (new_ad_keys.empty() || ! scheduler.jobSets)
		return;

	// temporary local map of set name to set id
	// so we know what set_ids to assign to which clusters
	std::string setName, userName;
	std::map<std::string, int> set_names;

	// build a map of new jobsets, and also a list of new cluster ids
	std::vector<int> new_cluster_ids;
	for (const auto& it : new_ad_keys) {
		JobQueueKey jid(it.c_str());
		if (jid.cluster <= 0) continue;

		if (jid.proc == CLUSTERID_qkey2) {
			new_cluster_ids.push_back(jid.cluster);
		} else if (jid.proc == JOBSETID_qkey2) {
			// here is a new jobset being created.
			if (GetAttributeString(jid.cluster, jid.proc, ATTR_JOB_SET_NAME, setName) == 1 &&
				GetAttributeString(jid.cluster, jid.proc, ATTR_USERREC_NAME, userName) >= 0) {
				set_names[JobSets::makeAlias(setName, userName)] = jid.cluster;
			}
			new_jobset_ids.push_back(jid.cluster);
		}
	}

	// now look for cluster ads that want jobsets that are not yet in this transaction
	// Implicit jobset creation is for use with older versions of submit
	// as of 9.10.0 submit will create jobsets explicitly
	for (auto cluster : new_cluster_ids) {
		if (GetAttributeString(cluster, -1, ATTR_JOB_SET_NAME, setName) == 1 &&
			GetAttributeString(cluster, -1, ATTR_USERREC_NAME, userName) >= 0) {
			std::string alias = JobSets::makeAlias(setName, userName);
			int & setId = set_names[alias];
			if (0 == setId) {
				// if we don't have a pending set with this name, look for a pre-existing set
				// and if we don't find one, create one for this cluster.
				setId = scheduler.jobSets->find(alias);
				if (0 == setId) {
					// if we are already creating a new jobset with this id, then we should just ignore this cluster ad's 
					// TODO: force the cluster ad to use the jobset name from the matching jobset ad, or reject the submission
					if (std::find(new_jobset_ids.begin(), new_jobset_ids.end(), cluster) == new_jobset_ids.end()) {
						if (JobSetCreate(cluster, setName.c_str(), userName.c_str())) {
							setId = cluster;
							new_jobset_ids.push_back(setId);
						}
					}
				}
			}
			// add this into the transaction so that the JobQueueCluster ad object will know the set id at init time ?
			// if (setId > 0) { SetSecureAttributeInt(cluster, -1, ATTR_JOB_SET_ID, setId); }
		}
	}
}


static int
AddSessionAttributes(const std::list<std::string> &new_ad_keys, CondorError * errorStack)
{
	if (new_ad_keys.empty()) { return 0; }

	ClassAd policy_ad;
	if (Q_SOCK && Q_SOCK->getReliSock()) {
		const std::string &sess_id = Q_SOCK->getReliSock()->getSessionID();
		daemonCore->getSecMan()->getSessionPolicy(sess_id.c_str(), policy_ad);
	}

	classad::ClassAdUnParser unparse;
	unparse.SetOldClassAd(true, true);

	// See if the values have already been set
	ClassAd *x509_attrs = &policy_ad;
	std::string last_proxy_file;
	ClassAd proxy_file_attrs;

	// Put X509 credential information in cluster ads (from either the
	// job's proxy or the GSI authentication on the CEDAR socket).
	// Proc ads should get X509 credential information only if they
	// have a proxy file different than in their cluster ad.
	for (const auto & new_ad_key : new_ad_keys)
	{
		JobQueueKey jid(new_ad_key.c_str());

		// new cluster and new jobset ads must have
		// a User and an Owner attribute
		if (jid.cluster > 0 && (jid.proc == CLUSTERID_qkey2 || jid.proc == JOBSETID_qkey2)) {
			const char * euser = EffectiveUserName(Q_SOCK);
			if (euser && euser[0]) {
				std::string obuf, ubuf, qbuf;
				bool no_owner = GetAttributeString(jid.cluster, jid.proc, ATTR_OWNER, obuf) == -1;
				bool no_user = GetAttributeString(jid.cluster, jid.proc, ATTR_USER, ubuf) == -1;
			#ifdef USE_JOB_QUEUE_USERREC
				// User is cannonical and Owner must be splitusername(user)[0]
				if (no_user) {
					if (USERREC_NAME_IS_FULLY_QUALIFIED) {
						JobQueue->SetAttribute(jid, ATTR_USER, QuoteAdStringValue(euser, qbuf));
					} else {
						// If User records are keyed by owner, the User attribute *must* be owner@uid_domain
						const char * owner = name_of_user(euser, qbuf);
						if ( ! is_same_user(euser, owner, COMPARE_DOMAIN_DEFAULT, scheduler.uidDomain())) {
							if (job_owner_must_be_UidDomain) {
								if (errorStack) {
									errorStack->pushf("QMGMT", 1,
										"username is '%s', but only users with domain %s can submit jobs.",
										euser, scheduler.uidDomain());
								}
								return -1;
							}
						}
						ubuf = std::string(owner) + "@" + scheduler.uidDomain();
						JobQueue->SetAttribute(jid, ATTR_USER, QuoteAdStringValue(ubuf.c_str(), qbuf));
					}
				} else {
					euser = ubuf.c_str();
				}
				if (no_owner) {
					const char * owner = name_of_user(euser, obuf);
					JobQueue->SetAttribute(jid, ATTR_OWNER, QuoteAdStringValue(owner, qbuf));
				#ifdef WIN32
					const char * ntdomain = domain_of_user(euser,nullptr);
					if (ntdomain) {
						const char * quoted_domain = QuoteAdStringValue(ntdomain, qbuf);
						JobQueue->SetAttribute(jid, ATTR_NT_DOMAIN, quoted_domain);
					}
					dprintf(D_FULLDEBUG, "new ad %d.%d has no Owner attribute, setting Owner=%s NTDomain=%s\n",
						jid.cluster, jid.proc, owner, ntdomain ? ntdomain : "<none>");
				#else
					dprintf(D_FULLDEBUG, "new ad %d.%d has no Owner attribute, setting Owner=%s\n",
						jid.cluster, jid.proc, owner);
				#endif
				}
			#else
				// Owner is cannonical, and User must be owner@uid_domain
				const char * owner = obuf.c_str();
				if (no_owner) {
					owner = name_of_user(euser, obuf);
					JobQueue->SetAttribute(jid, ATTR_OWNER, QuoteAdStringValue(owner, qbuf));
				}
				if (no_user) {
					ubuf = std::string(owner) + "@" + scheduler.uidDomain();
					JobQueue->SetAttribute(jid, ATTR_USER, QuoteAdStringValue(ubuf.c_str(), qbuf));
				}
			#endif
			}
		}

		if (jid.proc < -1 || jid.cluster <= 0) continue; // ignore non-job records for the remainder

		std::string x509up, iwd;
		GetAttributeString(jid.cluster, jid.proc, ATTR_X509_USER_PROXY, x509up);
		GetAttributeString(jid.cluster, jid.proc, ATTR_JOB_IWD, iwd);

		if (jid.proc != -1 && x509up.empty()) {
			if (iwd.empty()) {
				// A proc ad that will inherit its iwd and proxy filename
				// from its cluster ad.
				// Let any x509 attributes from the cluster ad show through.
				continue;
			}
			// This proc ad has a different iwd than its cluster ad.
			// If cluster ad has a relative proxy filename, then the proc
			// ad's altered iwd could result in a different proxy file
			// than in the cluster ad.
			GetAttributeString(jid.cluster, -1, ATTR_X509_USER_PROXY, x509up);
			if (x509up.empty() || x509up[0] == DIR_DELIM_CHAR) {
				// A proc ad with no proxy filename whose cluster ad
				// has no proxy filename or a proxy filename with full path.
				// Let any x509 attributes from the cluster ad show through.
				continue;
			}
		}
		if (jid.proc == -1 && x509up.empty()) {
			// A cluster ad with no proxy file. If the client authenticated
			// with GSI, use the attributes from that credential.
			x509_attrs = &policy_ad;
		} else {
			// We have a cluster ad with a proxy file or a proc ad with a
			// proxy file that may be different than in its cluster's ad.
			std::string full_path;
			if ( x509up[0] == DIR_DELIM_CHAR ) {
				full_path = x509up;
			} else {
				if ( iwd.empty() ) {
					GetAttributeString(jid.cluster, -1, ATTR_JOB_IWD, iwd );
				}
				formatstr( full_path, "%s%c%s", iwd.c_str(), DIR_DELIM_CHAR, x509up.c_str() );
			}
			if (jid.proc != -1) {
				std::string cluster_full_path;
				std::string cluster_x509up;
				GetAttributeString(jid.cluster, -1, ATTR_X509_USER_PROXY, cluster_x509up);
				if (cluster_x509up[0] == DIR_DELIM_CHAR) {
					cluster_full_path = cluster_x509up;
				} else {
					std::string cluster_iwd;
					GetAttributeString(jid.cluster, -1, ATTR_JOB_IWD, cluster_iwd );
					formatstr( cluster_full_path, "%s%c%s", cluster_iwd.c_str(), DIR_DELIM_CHAR, cluster_x509up.c_str() );
				}
				if (full_path == cluster_full_path) {
					// A proc ad with identical full-path proxy
					// filename as its cluster ad.
					// Let any attributes from the cluster ad show through.
					continue;
				}
			}
			if ( full_path != last_proxy_file ) {
				std::string owner;
				if ( GetAttributeString(jid.cluster, jid.proc, ATTR_OWNER, owner) == -1 ) {
					GetAttributeString(jid.cluster, -1, ATTR_OWNER, owner);
				}
				last_proxy_file = full_path;
				proxy_file_attrs.Clear();
				ReadProxyFileIntoAd( last_proxy_file.c_str(), owner.c_str(), proxy_file_attrs );
			}
			if ( proxy_file_attrs.size() > 0 ) {
				x509_attrs = &proxy_file_attrs;
			} else if (jid.proc != -1) {
				// Failed to read proxy for a proc ad.
				// Let any attributes from the cluster ad show through.
				continue;
			} else {
				// Failed to read proxy for a cluster ad.
				// If the client authenticated with GSI, use the attributes
				// from that credential.
				x509_attrs = &policy_ad;
			}
		}

		for (const auto & x509_attr : *x509_attrs)
		{
			std::string attr_value_buf;
			unparse.Unparse(attr_value_buf, x509_attr.second);
			SetSecureAttribute(jid.cluster, jid.proc, x509_attr.first.c_str(), attr_value_buf.c_str());
			dprintf(D_SECURITY, "ATTRS: SetAttribute %i.%i %s=%s\n", jid.cluster, jid.proc, x509_attr.first.c_str(), attr_value_buf.c_str());
		}
	}

	return 0;
}

int CommitTransactionInternal( bool durable, CondorError * errorStack );

void
CommitTransactionOrDieTrying() {
	CondorError errorStack;
	int rval = CommitTransactionInternal( true, & errorStack );
	if( rval < 0 ) {
		if( errorStack.empty() ) {
			dprintf( D_ALWAYS | D_BACKTRACE, "ERROR: CommitTransactionOrDieTrying() failed but did not die.\n" );
		} else {
			dprintf( D_ALWAYS | D_BACKTRACE, "ERROR: CommitTransactionOrDieTrying() failed but did not die, with error '%s'.\n", errorStack.getFullText().c_str() );
		}
	}
}

void
CommitNonDurableTransactionOrDieTrying() {
	CondorError errorStack;
	int rval = CommitTransactionInternal( false, & errorStack );
	if( rval < 0 ) {
		if( errorStack.empty() ) {
			dprintf( D_ALWAYS | D_BACKTRACE, "ERROR: CommitNonDurableTransactionOrDieTrying() failed but did not die.\n" );
		} else {
			dprintf( D_ALWAYS | D_BACKTRACE, "ERROR: CommitNonDurableTransactionOrDieTrying() failed but did not die, with error '%s'.\n", errorStack.getFullText().c_str() );
		}
	}
}

int
CommitTransactionAndLive( SetAttributeFlags_t flags,
                          CondorError * errorStack )
{
	bool durable = !(flags & NONDURABLE);
	if( (durable && flags != 0) || ((!durable) && flags != NONDURABLE) ) {
		dprintf( D_ALWAYS | D_BACKTRACE, "ERROR: CommitTransaction(): Flags other than NONDURABLE not supported.\n" );
	}

	if( errorStack == nullptr ) {
		dprintf( D_ALWAYS | D_BACKTRACE, "ERROR: CommitTransaction() called with NULL error stack.\n" );
	}

	return CommitTransactionInternal( durable, errorStack );
}

int CommitTransactionInternal( bool durable, CondorError * errorStack ) {

	std::list<std::string> new_ad_keys;
	struct ownerinfo_init_state ownerinfo_is = { nullptr, nullptr, false };
	std::string owner;
	
		// get a list of all new ads being created in this transaction
	JobQueue->ListNewAdsInTransaction( new_ad_keys );


	if ( ! new_ad_keys.empty()) { SetSubmitTotalProcs(new_ad_keys); }

	if ( !new_ad_keys.empty() ) {
		int rval = AddSessionAttributes(new_ad_keys, errorStack);
		if (rval < 0) {
			dprintf(D_FULLDEBUG, "AddSessionAttributes error %d : %s\n", rval, errorStack ? errorStack->message() : "");
			return rval;
		}

		rval = CheckTransaction(new_ad_keys, errorStack);
		if ( rval < 0 ) {
			// This transaction failed checks (e.g. SUBMIT_REQUIREMENTS), so now the question
			// is should we abort this transaction (that we refuse to commit) right now right here,
			// or should we simply return an error an rely on our caller to abort the transaction?
			// If we abort it now, our caller may get confused if they call AbortTransaction and it
			// subsequently fails.  On the other hand, we don't want to leave a transaction pending
			// that is never aborted in the event our caller never checks...
			// Current thinking is do not call AbortTransaction here, let the caller do it so 
			// that the logic in AbortTranscationAndRecomputeClusters() works correctly...
			dprintf(D_FULLDEBUG, "CheckTransaction error %d : %s\n", rval, errorStack ? errorStack->message() : "");
			return rval;
		}
	}

	// remember some things about the transaction that we will need to do post-transaction processing.
	// TODO: think about - can we skip this if new_ad_keys is not empty?
	std::set<std::string> ad_keys;
	int triggers = JobQueue->GetTransactionTriggers();
	// we have to do late materialization in a different place than the normal triggers
	bool has_late_materialize = (triggers & catNewMaterialize) != 0;
	triggers &= ~catNewMaterialize;

	// bool has_spooling_hold = (triggers & catSpoolingHold) != 0;
	// spooling hold triggers are handled in CheckTransaction, and while processing new_ad_keys,
	// so we can clear the trigger bit here.
	triggers &= ~catSpoolingHold;

	std::vector<unsigned int> new_jobset_ids;
	bool has_jobsets = (triggers & catJobset) != 0;
	if ( ! new_ad_keys.empty() && has_jobsets && scheduler.jobSets) {
		// build up a collection of new jobset ids, and also potentially
		// create new jobsets on the fly because there is a new cluster ad
		// that requests to be in a jobset.
		AddImplicitJobsets(new_ad_keys, new_jobset_ids);
	}

	if (triggers) {
		JobQueue->GetTransactionKeys(ad_keys);

		// before we commit the transaction, if there were changes to a cluster ad
		// update the EditedClusterAttrs for that cluster
		if (triggers & catPostSubmitClusterChange) {
			triggers &= ~catPostSubmitClusterChange;
			AddClusterEditedAttributes(ad_keys);
		}
	}

	// if job queue timestamps are enabled, build a commit comment
	// consisting of the time and an optional NONDURABLE flag
	// comment buffer sized to hold a timestamp and the word NONDURABLE and some whitespace
	char comment[ISO8601_DateAndTimeBufferMax + 31];
	const char * commit_comment = nullptr;
	if (scheduler.getEnableJobQueueTimestamps()) {
		struct timeval tv{};
		struct tm eventTime{};
		condor_gettimestamp(tv);
		time_t now = tv.tv_sec;
		localtime_r(&now, &eventTime);
		comment[0] = ' '; // leading space
		comment[1] = 0;
		time_to_iso8601(comment+1, eventTime, ISO8601_ExtendedFormat, ISO8601_DateAndTime, false, tv.tv_usec, 6);
		if ( ! durable) { strcat(comment+20, " NONDURABLE"); }
		commit_comment = comment;
	}

	if(! durable) {
		JobQueue->CommitNondurableTransaction(commit_comment);
		ScheduleJobQueueLogFlush();
	}
	else {
		JobQueue->CommitTransaction(commit_comment);
	}

	//----------------------------------------
	// Transaction Post-processing starts here
	//----------------------------------------

#ifdef USE_JOB_QUEUE_USERREC
	// add any just-committed JobQueueUserRec objects to the schedd Owners map
	if (triggers & catNewUser) {
		scheduler.mapPendingOwners();
	}

	// if we modified "User" or "owner" attributes, but not as part of making new ads
	// we need to do a pass to fixup the ownerinfo pointer on the modified jobs and jobsets
	if (new_ad_keys.empty() && (triggers & (catSetOwner|catSetUserRec))) {
		bool set_owner = (triggers & catSetOwner) != 0;
		bool edit_user = (triggers & catSetUserRec) != 0;
		for(const auto& it : ad_keys) {
			JobQueueKey jid(it.c_str());
			JobQueueBase *bad = nullptr;
			if ( ! JobQueue->Lookup(jid, bad) || ! bad) continue; // safety
			if (bad->IsCluster() || bad->IsJob() || bad->IsJobSet()) {
				if (set_owner) { InitOwnerinfo(bad, owner, ownerinfo_is); }
			} else if (bad->IsUserRec() && edit_user) {
				dynamic_cast<JobQueueUserRec*>(bad)->PopulateFromAd();
			}
		}
	}
#endif

	// Now that we've commited for sure, up the TotalJobsCount
	TotalJobsCount += jobs_added_this_transaction; 

	// If the commit failed, we should never get here.

	// setup the JobQueueJobSet header for jobset ads, and insert them
	// into the jobsets collection before we process the other ads
	for (auto id : new_jobset_ids) {
		JobQueueJobSet * jobset = GetJobSetAd(id);
		if (jobset) {
			InitJobsetAd(jobset, owner, ownerinfo_is);
		}
	}

	// Now that the transaction has been commited, we need to chain proc
	// ads to cluster ads if any new clusters have been submitted.
	// Also, if EVENT_LOG is defined in condor_config, we will write
	// submit events into the EVENT_LOG here.
	if ( !new_ad_keys.empty() ) {
		JobQueueKeyBuf job_id;
		int old_cluster_id = -10;
		JobQueueJob *procad = nullptr;
		JobQueueCluster *clusterad = nullptr;
		bool clear_mark_files = true;

		int counter = 0;
		int ad_keys_size = (int)new_ad_keys.size();
		std::list<std::string>::iterator it;
		for( it = new_ad_keys.begin(); it != new_ad_keys.end(); it++ ) {
			++counter;

			job_id.set(it->c_str());

			// do we want to fsync the userLog?
			bool doFsync = false;

			// for the cluster ad, we have some simpler, different processing, just do that up top
			if( job_id.proc == -1 ) {
				clusterad = GetClusterAd(job_id);
				if (clusterad) {
					// attach an OwnerInfo pointer to the clusterad. This can fail if the cluster ad
					// does not have a User attribute. which can happen with older gridmanager or
					// job router submits.  They put the User attribute into the proc ad
					if ( ! clusterad->ownerinfo) {
						InitOwnerinfo(clusterad, owner, ownerinfo_is);
					}
					clusterad->PopulateFromAd();

					if (clear_mark_files) {
						auto_free_ptr cred_dir_krb(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
						auto_free_ptr cred_dir_oauth(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));
						if (cred_dir_krb) {
							credmon_clear_mark(cred_dir_krb, clusterad->ownerinfo->Name());
						}
						if (cred_dir_oauth) {
							credmon_clear_mark(cred_dir_oauth, clusterad->ownerinfo->Name());
						}
					}

					// add the cluster ad to any jobsets it may be in
					if (scheduler.jobSets) {
						clusterad->set_id = 0; // force a setid lookup by name to make sure we get the correct id
						scheduler.jobSets->addToSet(*clusterad);
						if (clusterad->set_id > 0) {
							clusterad->Assign(ATTR_JOB_SET_ID, clusterad->set_id);
						}
					}

					// make the cluster job factory if one is desired and does not already exist.
					if (scheduler.getAllowLateMaterialize() && has_late_materialize) {
						auto found = JobFactoriesSubmitPending.find(clusterad->jid.cluster);
						if (found != JobFactoriesSubmitPending.end()) {
							// factory was created already, just remove it from the pending map and attach it to the clusterad
							AttachJobFactoryToCluster(found->second, clusterad);
							JobFactoriesSubmitPending.erase(found);
						} else {
							// factory was not already created (8.7.1 and 8.7.2), create it now.
							std::string submit_digest;
							if (clusterad->LookupString(ATTR_JOB_MATERIALIZE_DIGEST_FILE, submit_digest)) {
								ASSERT( ! clusterad->factory);

								// we need to let MakeJobFactory know whether the digest has been spooled or not
								// because it needs to know whether to impersonate the user or not.
								std::string spooled_filename;
								GetSpooledSubmitDigestPath(spooled_filename, clusterad->jid.cluster, Spool);
								bool spooled_digest = YourStringNoCase(spooled_filename) == submit_digest;

								std::string errmsg;
								clusterad->factory = MakeJobFactory(clusterad, scheduler.getExtendedSubmitCommands(),
								                                    submit_digest.c_str(), spooled_digest,
								                                    scheduler.getProtectedUrlMap(), errmsg);
								if ( ! clusterad->factory) {
									chomp(errmsg);
									setJobFactoryPauseAndLog(clusterad, mmInvalid, 0, errmsg);
								}
							}
						}
						if (clusterad->factory) {
							int paused = 0;
							if (clusterad->LookupInteger(ATTR_JOB_MATERIALIZE_PAUSED, paused) && paused) {
								PauseJobFactory(clusterad->factory, (MaterializeMode)paused);
							} else {
								ScheduleClusterForJobMaterializeNow(job_id.cluster);
							}
						} else {
							ScheduleClusterForJobMaterializeNow(job_id.cluster);
						}
					}

					// If this is a factory job, log the ClusterSubmit event here
					// TODO: ClusterSubmit events should be logged for all clusters with more
					// than one proc, regardless of whether a factory or not.
					if( clusterad->factory ) {
						scheduler.WriteClusterSubmitToUserLog( clusterad, doFsync );
					}

				}
				continue; // skip remaining processing for cluster ads
			} else if (job_id.proc < 0 || job_id.cluster <= 0) {
				continue; // no further processing of jobset ads or userrec ads needed
			}
			// we want to fsync per cluster and on the last ad
			if ( old_cluster_id == -10 ) {
				old_cluster_id = job_id.cluster;
			}
			if ( old_cluster_id != job_id.cluster || counter == ad_keys_size ) {
				doFsync = true;
				old_cluster_id = job_id.cluster;
			}

			// if the current cluster ad doesn't apply to this job, just clear it and lookup the correct one.
			if (clusterad && (clusterad->jid.cluster != job_id.cluster)) {
				clusterad = nullptr;
			}
			if ( ! clusterad) {
				clusterad = GetClusterAd(job_id.cluster);
				if (clusterad && ! clusterad->ownerinfo) {
					// in case we haven't seen the new cluster yet in this loop, init the cluster ownerinfo now
					InitOwnerinfo(clusterad, owner, ownerinfo_is);
				}
			}
			if (clusterad && JobQueue->Lookup(job_id, procad))
			{
				dprintf(D_FULLDEBUG,"New job: %s\n",job_id.c_str());

					// chain proc ads to cluster ad
				procad->jid = job_id; // can probably remove this...
				procad->ChainToAd(clusterad);
					// this will count the procad as IDLE in the cluster aggregates,
					// DoSetAttributeCallbacks will set the real state if it isn't IDLE
				clusterad->AttachJob(procad);

					// increment the 'recently added' job count for this owner
				if (clusterad->ownerinfo) {
					procad->ownerinfo = clusterad->ownerinfo;
					scheduler.incrementRecentlyAdded(procad->ownerinfo, nullptr);
				} else {
					// HACK! 
					// we get here only when the Cluster ad does not have an Owner or User attribute
					// older versions of the job router and gridmanager submit this way, so fix things up
					// minimally here, a restart of the schedd will fix things fully
					if ( ! InitOwnerinfo(procad, owner, ownerinfo_is) && Q_SOCK) {
						// last ditch effort, just use the socket owner as the job owner
						const char * user = EffectiveUserName(Q_SOCK);
						procad->ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(user));
					}
					ASSERT(procad->ownerinfo);
					clusterad->ownerinfo = procad->ownerinfo;
					clusterad->Assign(ATTR_USERREC_NAME, procad->ownerinfo->Name());
					scheduler.incrementRecentlyAdded(procad->ownerinfo, nullptr);
				}

					// convert any old attributes for backwards compatbility
				ConvertOldJobAdAttrs(procad, false);

					// make sure the job objd and cluster object are populated
				procad->PopulateFromAd();

				// if the cluster is in a jobset, the job is also
				procad->set_id = clusterad->set_id;
				if (scheduler.jobSets) { scheduler.jobSets->addToSet(*procad); }

					// Add the job to various runtime indexes for quick lookups
				scheduler.indexAJob(procad, false);

				PostCommitJobFactoryProc(clusterad, procad);

					// If input files are going to be spooled, rewrite
					// the paths in the job ad to point at our spool
					// area.
				int job_status = -1;
				int hold_code = -1;
				procad->LookupInteger(ATTR_JOB_STATUS, job_status);
				procad->LookupInteger(ATTR_HOLD_REASON_CODE, hold_code);
				if ( job_status == HELD && hold_code == CONDOR_HOLD_CODE::SpoolingInput ) {
					SpooledJobFiles::createJobSpoolDirectory(procad,PRIV_UNKNOWN);
				}

				//don't need to do this... the trigger code below seems to handle it.
				//IncrementLiveJobCounter(scheduler.liveJobCounts, procad->Universe(), job_status, 1);
				//if (ownerinfo) { IncrementLiveJobCounter(ownerinfo->live, procad->Universe(), job_status, 1); }

				std::string warning;
				if(errorStack && (! errorStack->empty())) {
					warning = errorStack->getFullText();
				}
				scheduler.WriteSubmitToUserLog( procad, doFsync, warning.empty() ? nullptr : warning.c_str() );

				int iDup = 0, iTotal = 0;
				iDup = procad->PruneChildAd();
				iTotal = procad->size();

				dprintf(D_FULLDEBUG,"New job: %s, Duplicate Keys: %d, Total Keys: %d \n", job_id.c_str(), iDup, iTotal);
			}

			int max_xfer_input_mb = -1;
			param_integer("MAX_TRANSFER_INPUT_MB",max_xfer_input_mb,true,-1,false,INT_MIN,INT_MAX,procad);
			filesize_t job_max_xfer_input_mb = 0;
			if( procad && procad->LookupInteger(ATTR_MAX_TRANSFER_INPUT_MB,job_max_xfer_input_mb) ) {
				max_xfer_input_mb = job_max_xfer_input_mb;
			}
			if( max_xfer_input_mb >= 0 ) {
				filesize_t xfer_input_size_mb = 0;
				if( procad && procad->LookupInteger(ATTR_TRANSFER_INPUT_SIZE_MB,xfer_input_size_mb) ) {
					if( xfer_input_size_mb > max_xfer_input_mb ) {
						std::string hold_reason;
						formatstr(hold_reason,"%s (%d) is greater than %s (%d) at submit time",
								  ATTR_TRANSFER_INPUT_SIZE_MB, (int)xfer_input_size_mb,
								  "MAX_TRANSFER_INPUT_MB", (int)max_xfer_input_mb);
						holdJob(job_id.cluster, job_id.proc, hold_reason.c_str(),
								CONDOR_HOLD_CODE::MaxTransferInputSizeExceeded, 0);
					}
				}
			}

		}	// end of loop thru clusters
	}	// end of if a new cluster(s) submitted


	// finally, invoke callbacks that were triggered by various SetAttribute calls in the transaction.
	// NOTE: you might be tempted to move this up above the processing of new ad keys, but that won't work
	// because most lookups in the job ad don't work until it has been chained to the cluster ad.
	if (triggers) {
		// TODO: convert the keys to PROC_IDs before calling DoSetAttributeCallbacks?
		DoSetAttributeCallbacks(ad_keys, triggers);
	}

	xact_start_time = 0;
	return 0;
}

int
AbortTransaction()
{
	if (JobQueue->InTransaction()) {
	#ifdef USE_JOB_QUEUE_USERREC
		// delete any speculative JobQueueUserRec objects we created for this transaction
		scheduler.clearPendingOwners();
	#endif
	}
	return JobQueue->AbortTransaction();
}

void
AbortTransactionAndRecomputeClusters()
{
	if (JobQueue->InTransaction()) {
		dprintf(D_ALWAYS | D_BACKTRACE, "AbortTransactionAndRecomputeClusters\n");
	#ifdef USE_JOB_QUEUE_USERREC
		// delete any speculative JobQueueUserRec objects we created for this transaction
		scheduler.clearPendingOwners();
	#endif
	}
	if ( JobQueue->AbortTransaction() ) {
		/*	If we made it here, a transaction did exist that was not
			committed, and we now aborted it.  This would happen if 
			somebody hit ctrl-c on condor_rm or condor_status, etc,
			or if any of these client tools bailed out due to a fatal error.
			Because the removal of ads from the queue has been backed out,
			we need to "back out" from any changes to the ClusterSizeHashTable,
			since this may now contain incorrect values.  Ideally, the size of
			the cluster should just be kept in the cluster ad -- that way, it 
			gets committed or aborted as part of the transaction.  But alas, 
			it is not; same goes a bunch of other stuff: removal of ckpt and 
			ickpt files, appending to the history file, etc.  Sigh.  
			This should be cleaned up someday, probably with the new schedd.
			For now, to "back out" from changes to the ClusterSizeHashTable, we
			use brute force and blow the whole thing away and recompute it. 
			-Todd 2/2000
		*/
		//TODO: move cluster count from hashtable into the cluster's JobQueueJob object.
		ClusterSizeHashTable->clear();
		JobQueueBase *job = nullptr;
		JobQueueKey key;
		JobQueue->StartIterateAllClassAds();
		while (JobQueue->Iterate(key, job)) {
			if (key.cluster > 0 && key.proc >= 0) { // look at job ads only.
				int *numOfProcs = nullptr;
				// count up number of procs in cluster, update ClusterSizeHashTable
				if ( ClusterSizeHashTable->lookup(key.cluster,numOfProcs) == -1 ) {
					// First proc we've seen in this cluster; set size to 1
					ClusterSizeHashTable->insert(key.cluster,1);
				} else {
					// We've seen this cluster_num go by before; increment proc count
					(*numOfProcs)++;
				}
			}
		}

		// now update the clustersize field in the cluster object so that it matches
		// the cluster size hashtable.
		JobQueue->StartIterateAllClassAds();
		while (JobQueue->Iterate(key, job)) {
			if (key.cluster > 0 && key.proc == -1) { // look at cluster ads only
				auto * cad = dynamic_cast<JobQueueCluster*>(job);
				int *numOfProcs = nullptr;
				// count up number of procs in cluster, update ClusterSizeHashTable
				if ( ClusterSizeHashTable->lookup(key.cluster,numOfProcs) == -1 ) {
					cad->SetClusterSize(0); // not in the cluster size hash table, so there are no procs...
				} else {
					cad->SetClusterSize(*numOfProcs); // copy num of procs into the cluster object.
				}
			}
		}

	}	// end of if JobQueue->AbortTransaction == True
}


int
GetAttributeFloat(int cluster_id, int proc_id, const char *attr_name, double *val)
{
	ClassAd	*ad = nullptr;
	JobQueueKeyBuf	key;
	char	*attr_val = nullptr;

	IdToKey(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		ClassAd tmp_ad;
		tmp_ad.AssignExpr(attr_name,attr_val);
		free( attr_val );
		if( tmp_ad.LookupFloat(attr_name, *val) == 1) {
			return 1;
		}
		errno = EINVAL;
		return -1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		errno = ENOENT;
		return -1;
	}

	if (ad->LookupFloat(attr_name, *val) == 1) return 0;
	errno = EINVAL;
	return -1;
}


int
GetAttributeInt(int cluster_id, int proc_id, const char *attr_name, long long *val)
{
	ClassAd	*ad = nullptr;
	JobQueueKeyBuf key;
	char	*attr_val = nullptr;

	IdToKey(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		ClassAd tmp_ad;
		tmp_ad.AssignExpr(attr_name,attr_val);
		free( attr_val );
		if( tmp_ad.LookupInteger(attr_name, *val) == 1) {
			return 1;
		}
		errno = EINVAL;
		return -1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		errno = ENOENT;
		return -1;
	}

	if (ad->LookupInteger(attr_name, *val) == 1) return 0;
	errno = EINVAL;
	return -1;
}

int
GetAttributeInt(int cluster_id, int proc_id, const char *attr_name, long *val)
{
	long long ll_val = *val;
	int rc = GetAttributeInt(cluster_id, proc_id, attr_name, &ll_val);
	if (rc >= 0) {
		*val = (long)ll_val;
	}
	return rc;
}

int
GetAttributeInt(int cluster_id, int proc_id, const char *attr_name, int *val)
{
	long long ll_val = *val;
	int rc = GetAttributeInt(cluster_id, proc_id, attr_name, &ll_val);
	if (rc >= 0) {
		*val = (int)ll_val;
	}
	return rc;
}

int
GetAttributeBool(int cluster_id, int proc_id, const char *attr_name, bool *val)
{
	ClassAd	*ad = nullptr;
	JobQueueKeyBuf key;
	char	*attr_val = nullptr;

	IdToKey(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		ClassAd tmp_ad;
		tmp_ad.AssignExpr(attr_name,attr_val);
		free( attr_val );
		if( tmp_ad.LookupBool(attr_name, *val) == 1) {
			return 1;
		}
		errno = EINVAL;
		return -1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		errno = ENOENT;
		return -1;
	}

	if (ad->LookupBool(attr_name, *val) == 1) return 0;
	errno = EINVAL;
	return -1;
}

// I added this version of GetAttributeString. It is nearly identical 
// to the other version, but it calls a different version of 
// AttrList::LookupString() which allocates a new string. This is a good
// thing, since it doesn't require a buffer that we could easily overflow.
int
GetAttributeStringNew( int cluster_id, int proc_id, const char *attr_name, 
					   char **val )
{
	ClassAd	*ad = nullptr;
	JobQueueKeyBuf key;
	char	*attr_val = nullptr;

	*val = nullptr;

	IdToKey(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		ClassAd tmp_ad;
		tmp_ad.AssignExpr(attr_name,attr_val);
		free( attr_val );
		if( tmp_ad.LookupString(attr_name, val) == 1) {
			return 1;
		}
		errno = EINVAL;
		return -1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		errno = ENOENT;
		return -1;
	}

	if (ad->LookupString(attr_name, val) == 1) {
		return 0;
	}
	errno = EINVAL;
	return -1;
}

// returns -1 if the lookup fails or if the value is not a string, 0 if
// the lookup succeeds in the job queue, 1 if it succeeds in the current
// transaction; val is set to the empty string on failure
int
GetAttributeString( int cluster_id, int proc_id, const char *attr_name,
                    std::string &val )
{
	ClassAd	*ad = nullptr;
	char	*attr_val = nullptr;

	JobQueueKeyBuf key;
	IdToKey(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		ClassAd tmp_ad;
		tmp_ad.AssignExpr(attr_name,attr_val);
		free( attr_val );
		if( tmp_ad.LookupString(attr_name, val) == 1) {
			return 1;
		}
		val = "";
		errno = EINVAL;
		return -1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		val = "";
		errno = ENOENT;
		return -1;
	}

	if (ad->LookupString(attr_name, val) == 1) {
		return 0;
	}
	val = "";
	errno = EINVAL;
	return -1;
}

int
GetAttributeExprNew(int cluster_id, int proc_id, const char *attr_name, char **val)
{
	ClassAd		*ad = nullptr;
	ExprTree	*tree = nullptr;
	char		*attr_val = nullptr;

	*val = nullptr;

	JobQueueKeyBuf key;
	IdToKey(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		*val = attr_val;
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		errno = ENOENT;
		return -1;
	}

	tree = ad->LookupExpr(attr_name);
	if (!tree) {
		errno = EINVAL;
		return -1;
	}

	*val = strdup(ExprTreeToString(tree));

	return 0;
}


int
GetDirtyAttributes(int cluster_id, int proc_id, ClassAd *updated_attrs)
{
	ClassAd 	*ad = nullptr;
	char		*val = nullptr;
	const char	*name = nullptr;
	ExprTree 	*expr = nullptr;

	JobQueueKeyBuf key;
	IdToKey(cluster_id,proc_id,key);

	if(!JobQueue->LookupClassAd(key, ad)) {
		errno = ENOENT;
		return -1;
	}

	for ( auto itr = ad->dirtyBegin(); itr != ad->dirtyEnd(); itr++ )
	{
		name = itr->c_str();
		expr = ad->LookupExpr(name);
		if(expr && !ClassAdAttributeIsPrivateAny(name))
		{
			if(!JobQueue->LookupInTransaction(key, name, val) )
			{
				ExprTree * pTree = expr->Copy();
				updated_attrs->Insert(name, pTree);
			}
			else
			{
				updated_attrs->AssignExpr(name, val);
				free(val);
			}
		}
	}

	return 0;
}


int
DeleteAttribute(int cluster_id, int proc_id, const char *attr_name)
{
	JobQueueKeyBuf key;
	IdToKey(cluster_id,proc_id,key);

	int rc = ModifyAttrCheck(key, attr_name, nullptr, SetAttribute_Delete, nullptr);
	if ( rc <= 0 ) {
		return rc;
	}

	JobQueue->DeleteAttribute(key, attr_name);

	JobQueueDirty = true;

	return 1;
}

// 
int QmgmtHandleSendJobsetAd(int cluster_id, ClassAd & ad, int /*flags*/, int & terrno)
{
	// check to see if the schedd is permitting jobsets
	if ( ! scheduler.jobSets) {
		dprintf(D_FULLDEBUG, "Failing remote SendJobsetAd %d because USE_JOBSETS is false\n", cluster_id);
		terrno = EINVAL;
		return -1;
	}

	JOB_ID_KEY_BUF key;
	IdToKey(cluster_id,JOBSETID_qkey2,key);
#ifdef USE_JOB_QUEUE_USERREC
	const OwnerInfo * owninfo = EffectiveUserRec(Q_SOCK);
	if ( ! owninfo || (owninfo->jid.proc == CONDOR_USERREC_ID)) {
		if (owninfo) { 
			dprintf(D_FULLDEBUG | D_BACKTRACE, "NewJobset %d socket owned by unreal-user %s\n",
				cluster_id, owninfo->Name());
		}
		const char * user = EffectiveUserName(Q_SOCK);
		// we can't use the built-in condor UserRec, there should be a real UserRec,
		// possibly one created by NewCluster or by a previous submit operation
		if (user && user[0]) { owninfo = scheduler.lookup_owner_const(user); }
		if ( ! owninfo) {
			dprintf(D_FULLDEBUG, "Failing remote SendJobsetAd %d because EffectiveUser %s is not found.\n", cluster_id, user);
			terrno = EINVAL;
			return -1;
		}
	}
#else
	const char * username = EffectiveUser(Q_SOCK);
#endif

	// extract jobset name and id from incoming ad
	// and delete the attributes so that we won't try and merge them later
	std::string name;
	ad.LookupString(ATTR_JOB_SET_NAME, name);
	ad.Delete(ATTR_JOB_SET_NAME); // let JobSetCreate control this
	long long input_setid_val = -1;
	ad.LookupInteger(ATTR_JOB_SET_ID, input_setid_val); // we don't expect this, and can't honor it
	ad.Delete(ATTR_JOB_SET_ID);

	// These are reserved for use by the schedd, just ignore incoming values
	// TODO: honor incoming Owner values for queue superusers?
	ad.Delete(ATTR_OWNER);
	ad.Delete(ATTR_USER);

	// is there an existing jobset ad with this id?
	JobQueueBase* bad = nullptr;
	if (JobQueue->Lookup(key, bad)) {
		dprintf(D_ERROR, "There is already a jobset %d. Failing\n", key.cluster);
		// TODO: validate or merge ?
		terrno = ENOENT;
		return -1;
	}

	if (name.empty()) {
		dprintf(D_ERROR, "Jobset ad for cluster %d submitted without a name.  Failing\n", key.cluster);
		terrno = EINVAL;
		return -1;
	}

	// is there an existing jobset ad with this name?
	// if not, create a new jobset ad using the passed-in cluster_id as the jobset_id
#ifdef USE_JOB_QUEUE_USERREC
	const char * username = owninfo->Name();
	int jobset_id = scheduler.jobSets->find(JobSets::makeAlias(name, *owninfo));
#else
	int jobset_id = scheduler.jobSets->find(JobSets::makeAlias(name, username));
#endif
	if ( ! jobset_id) {
		// no existing jobset, so make a new one
		if (cluster_id != active_cluster_num) {
			dprintf(D_ERROR, "Rejecting new jobset ad because jobset_id %d does not match active cluster %d\n", cluster_id, active_cluster_num);
			terrno = EINVAL;
			return -1;
		}
		if ( ! JobSetCreate(cluster_id, name.c_str(), username)) {
			dprintf(D_ERROR, "Could not create jobset %s jobset_id %d for user %s\n", name.c_str(), cluster_id, username);
			terrno = EINVAL;
			return -1;
		}
		jobset_id = cluster_id;
	}


	// TODO: what should we do if we find an existing jobset ad?
	// for now, we will overwrite custom jobset attributes in the existing ad

	key.set(jobset_id, JOBSETID_qkey2);

	// add the custom jobset attributes to the jobset ad
	int rval = 0;
	if (ad.size() > 0) {
		classad::ClassAdUnParser unparse;
		unparse.SetOldClassAd(true, true);
		std::string rhs; rhs.reserve(120);

		for (const auto& it : ad) {
			if (! it.second) continue; // skip if the ExprTree is nullptr

			if (!IsValidAttrName(it.first.c_str())) {
				dprintf(D_ERROR, "got invalid attribute named %s for jobset %d\n",
					it.first.c_str(), key.cluster);
				terrno = EINVAL;
				return -1;
			}

			if (secure_attrs.count(it.first)) {
				dprintf(D_ERROR,
					"attempt to set secure attribute %s in jobset %d. Failing!\n",
					it.first.c_str(), key.cluster);
				terrno = EACCES;
				rval = -1;
				break;
			}

			// TODO: more validation ?

			rhs.clear();
			unparse.Unparse(rhs, it.second);
			if ( ! JobQueue->SetAttribute(key, it.first.c_str(), rhs.c_str())) {
				terrno = EINVAL;
				rval = -1;
			}
		}
	}

	return rval;
}


void
MarkJobClean(PROC_ID proc_id)
{
	JobQueueKeyBuf job_id(proc_id);
	if (JobQueue->ClearClassAdDirtyBits(job_id))
	{
		dprintf(D_FULLDEBUG, "Cleared dirty attributes for job %s\n", job_id.c_str());
	}

	DirtyJobIDs.erase(job_id);

	if( DirtyJobIDs.empty() && dirty_notice_timer_id > 0 )
	{
		dprintf(D_FULLDEBUG, "Cancelling dirty attribute notification timer\n");
		daemonCore->Cancel_Timer(dirty_notice_timer_id);
		dirty_notice_timer_id = -1;
	}
}

void
MarkJobClean(const char* job_id_str)
{
	PROC_ID jid;
	StrToProcIdFixMe(job_id_str, jid);
	MarkJobClean(jid);
}

void
MarkJobClean(int cluster_id, int proc_id)
{
	PROC_ID key( cluster_id, proc_id );
	MarkJobClean(key);
}

ClassAd *
dollarDollarExpand(int cluster_id, int proc_id, ClassAd *ad, ClassAd *startd_ad, bool persist_expansions)
{
	// This is prepended to attributes that we've already expanded,
	// making them available if the match ad is no longer available.
	// So 
	//   GlobusScheduler=$$(RemoteGridSite)
	// matches an ad containing
	//   RemoteGridSide=foobarqux
	// you'll get
	//   MATCH_EXP_GlobusScheduler=foobarqux
	const char * MATCH_EXP = "MATCH_EXP_";
	bool started_transaction = false;

	int	job_universe = -1;
	ad->LookupInteger(ATTR_JOB_UNIVERSE,job_universe);

		// if we made it here, we have the ad, but
		// expStartdAd is true.  so we need to dig up 
		// the startd ad which matches this job ad.
		char *bigbuf2 = nullptr;
		char *attribute_value = nullptr;
		ClassAd *expanded_ad = nullptr;
		char *left = nullptr,*name = nullptr,*right = nullptr,*value = nullptr,*tvalue = nullptr;
		bool value_came_from_jobad = false;

		// we must make a deep copy of the job ad; we do not
		// want to expand the ad we have in memory.
		expanded_ad = new ClassAd(*ad);  

		// Copy attributes from chained parent ad into the expanded ad
		// so if parent is deleted before caller is finished with this
		// ad, things will still be ok.
		ChainCollapse(*expanded_ad);

		// before $$ expansion, we may need to convert the Environment from v1 to v2
		// or switch the v1 delimiter to match the target OS. We do this so that if the 
		// environment has $$ expansions we are using the target OS's expected delim
		// or a format that doesn't have a delim conflict issue.
		if ( startd_ad && job_universe != CONDOR_UNIVERSE_GRID ) {
			// Produce an environment description that is compatible with
			// whatever the starter expects.
			// Note: this code path is skipped when we flock and reconnect
			//  after a disconnection (job lease).  In this case we don't
			//  have a startd_ad!

			// check for a job that has a V1 environment only, and the target OS uses a different separator
			// HTCondor 9.x and earlier does not honor EnvDelim in MergeFrom(Ad) so we have to re-write Env
			// for the Opsys of the STARTD.
			// TODO: add a version check for STARTD's that honor the ATTR_JOB_ENV_V1_DELIM on ingestion
			if (expanded_ad->Lookup(ATTR_JOB_ENV_V1) && ! expanded_ad->LookupExpr(ATTR_JOB_ENVIRONMENT))
			{
				Env env_obj;

				// compare the delim that is natural to the startd to the delim that the job is currently using
				// if they differ, and the job is using V1 environment (and STARTD is earlier than X.Y) we
				// have to re-write the Env attribute and the EnvDelim attributes for target OS
				std::string opsys;
				startd_ad->LookupString(ATTR_OPSYS, opsys);
				char target_delim = env_obj.GetEnvV1Delimiter(opsys.c_str());
				char my_delim = env_obj.GetEnvV1Delimiter(*expanded_ad);
				if (my_delim != target_delim) {
					std::string env_error_msg;
					// ingest using the current delim as specified in the job
					if (env_obj.MergeFrom(expanded_ad, env_error_msg)) {
						// write the environment back into the job with the OPSYS correct delimiter
						if ( ! env_obj.InsertEnvV1IntoClassAd(*expanded_ad, env_error_msg, target_delim)) {
							dprintf( D_FULLDEBUG, "Job expansion using attr Environment because attr Env cannot be converted to opsys=%s : %s\n",
								opsys.c_str(), env_error_msg.c_str());
							// write a V2 Environment attribute into the job,
							// this will take precedence over Env for STARTD's later than 6.7
							env_obj.InsertEnvIntoClassAd(*expanded_ad);
						}
					}
				}
			}
		}

			// Make a list of all attribute names in job ad that are not MATCH_ attributes
		std::vector<std::string> AttrsToExpand;
		for (auto & itr : *expanded_ad) {
			if ( strncasecmp(itr.first.c_str(),"MATCH_",6) == 0 ) {
					// We do not want to expand MATCH_XXX attributes,
					// because these are used to store the result of
					// previous expansions, which could potentially
					// contain literal $$(...) in the replacement text.
				continue;
			} else {
				AttrsToExpand.emplace_back(itr.first);
			}
		}

		std::string cachedAttrName, unparseBuf;

		bool attribute_not_found = false;
		std::string bad_attr_name;
		for (const auto& curr_attr_to_expand: AttrsToExpand) {
			if (attribute_not_found) {
				break;
			}

			cachedAttrName = MATCH_EXP;
			cachedAttrName += curr_attr_to_expand;

			if( !startd_ad ) {
					// No startd ad, so try to find cached value from back
					// when we did have a startd ad.
				ExprTree *cached_value = ad->LookupExpr(cachedAttrName);
				if( cached_value ) {
					const char *cached_value_buf =
						ExprTreeToString(cached_value);
					ASSERT(cached_value_buf);
					expanded_ad->AssignExpr(curr_attr_to_expand,cached_value_buf);
					continue;
				}

					// No cached value, so try to expand the attribute
					// without the cached value.  If it is an
					// expression that refers only to job attributes,
					// it can succeed, even without the startd.
			}

			if (attribute_value != nullptr) {
				free(attribute_value);
				attribute_value = nullptr;
			}

			// Get the current value of the attribute in unparsed form if the value is subject to $$ expansion.
			// Numbers can't be $$ expanded, expressions and strings can.
			unparseBuf.clear();
			ExprTree * tree = expanded_ad->LookupExpr(curr_attr_to_expand);
			if (ExprTreeMayDollarDollarExpand(tree, unparseBuf)) {
				const char *new_value = unparseBuf.c_str();
				if (strchr(new_value, '$')) {
					// make a copy because $$ expansion code need a malloc'ed string
					attribute_value = strdup(unparseBuf.c_str());
				}
			}
			if ( attribute_value == nullptr ) {
				continue;
			}
		
			bool expanded_something = false;
			int search_pos = 0;
			while( !attribute_not_found &&
					next_dollardollar_macro(attribute_value, search_pos, &left, &name, &right) )
			{
				expanded_something = true;
				
				size_t namelen = strlen(name);
				if(name[0] == '[' && name[namelen-1] == ']') {
					// This is a classad expression to be considered

					std::string expr_to_add;
					formatstr(expr_to_add, "string(%s", name + 1);
					expr_to_add[expr_to_add.length()-1] = ')';

						// Any backwacked double quotes or backwacks
						// within the []'s should be unbackwacked.
					size_t read_pos = 0;
					size_t write_pos = 0;
					for( read_pos = 0, write_pos = 0;
						 read_pos < expr_to_add.length();
						 read_pos++, write_pos++ )
					{
						if( expr_to_add[read_pos] == '\\'  &&
							read_pos+1 < expr_to_add.length() &&
							( expr_to_add[read_pos+1] == '"' ||
							  expr_to_add[read_pos+1] == '\\' ) )
						{
							read_pos++; // skip over backwack
						}
						if( read_pos != write_pos ) {
							expr_to_add[write_pos] = expr_to_add[read_pos];
						}
					}
					if( read_pos != write_pos ) { // terminate the string
						expr_to_add.erase(write_pos);
					}

					ClassAd tmpJobAd(*ad);
					const char * INTERNAL_DD_EXPR = "InternalDDExpr";

					bool isok = tmpJobAd.AssignExpr(INTERNAL_DD_EXPR, expr_to_add.c_str());
					if( ! isok ) {
						attribute_not_found = true;
						bad_attr_name = curr_attr_to_expand;
						break;
					}

					std::string result;
					isok = EvalString(INTERNAL_DD_EXPR, &tmpJobAd, startd_ad, result);
					if( ! isok ) {
						attribute_not_found = true;
						bad_attr_name = curr_attr_to_expand;
						break;
					}
					std::string replacement_value;
					replacement_value += left;
					replacement_value += result;
					search_pos = replacement_value.length();
					replacement_value += right;
					expanded_ad->AssignExpr(curr_attr_to_expand, replacement_value.c_str());
					dprintf(D_FULLDEBUG,"$$([]) substitution: %s=%s\n",curr_attr_to_expand.c_str(),replacement_value.c_str());

					free(attribute_value);
					attribute_value = strdup(replacement_value.c_str());


				} else  {
					// This is an attribute from the machine ad

					// If the name contains a colon, then it
					// is a	fallback value, should the startd
					// leave it undefined, e.g.
					// $$(NearestStorage:turkey)

					char *fallback = nullptr;

					fallback = strchr(name,':');
					if(fallback) {
						*fallback = 0;
						fallback++;
					}

					if (strchr(name, '.')) {
						// . is a legal character for some find_config_macros, but not other
						// check here if one snuck through
						attribute_not_found = true;
						bad_attr_name = curr_attr_to_expand;
						break;
						
					}
					// Look for the name in the ad.
					// If it is not there, use the fallback.
					// If no fallback value, then fail.

					if( strcasecmp(name,"DOLLARDOLLAR") == 0 ) {
							// replace $$(DOLLARDOLLAR) with literal $$
						value = strdup("DOLLARDOLLAR = \"$$\"");
						value_came_from_jobad = true;
					}
					else if ( startd_ad ) {
							// We have a startd ad in memory, use it
						value = sPrintExpr(*startd_ad, name);
						value_came_from_jobad = false;
					} else {
							// No startd ad -- use value from last match.
						std::string expr;
						expr = "MATCH_";
						expr += name;
						value = sPrintExpr(*ad, expr.c_str());
						value_came_from_jobad = true;
					}

					if (!value) {
						if(fallback) {
							size_t sz = strlen(name)
								+ 3  // " = "
								+ 1  // optional '"'
								+ strlen(fallback)
								+ 1  // optional '"'
								+ 1; // null terminator
							char *rebuild = (char *) malloc(sz);
                            // fallback is defined as being a string value, encode it thusly:
                            snprintf(rebuild, sz, "%s = \"%s\"", name, fallback);
							value = rebuild;
						}
						if(!fallback || !value) {
							attribute_not_found = true;
							bad_attr_name = curr_attr_to_expand;
							break;
						}
					}


					// we just want the attribute value, so strip
					// out the "attrname=" prefix and any quotation marks 
					// around string value.
					tvalue = strchr(value,'=');
					ASSERT(tvalue);	// we better find the "=" sign !
					// now skip past the "=" sign
					tvalue++;
					while ( *tvalue && isspace(*tvalue) ) {
						tvalue++;
					}
					// insert the expression into the original job ad
					// before we mess with it any further.  however, no need to
					// re-insert it if we got the value from the job ad
					// in the first place.
					if ( !value_came_from_jobad && persist_expansions) {
						std::string expr;
						expr = "MATCH_";
						expr += name;

						if( !started_transaction && !InTransaction() ) {
							started_transaction = true;
								// for efficiency, when storing multiple
								// expansions, do it all in one transaction
							BeginTransaction();
						}	

					// We used to only bother saving the MATCH_ entry for
						// the GRID universe, but we now need it for flocked
						// jobs using disconnected starter-shadow (job-leases).
						// So just always do it.
						if ( SetAttribute(cluster_id,proc_id,expr.c_str(),tvalue) < 0 )
						{
							EXCEPT("Failed to store %s into job ad %d.%d",
								expr.c_str(),cluster_id,proc_id);
						}
					}
					// skip any quotation marks around strings
					if (*tvalue == '"') {
						tvalue++;
						int endquoteindex = (int)strlen(tvalue) - 1;
						if ( endquoteindex >= 0 && 
							 tvalue[endquoteindex] == '"' ) {
								tvalue[endquoteindex] = '\0';
						}
					}
					size_t lenBigbuf = strlen(left) + strlen(tvalue)  + strlen(right) + 1;
					bigbuf2 = (char *) malloc( lenBigbuf );
					ASSERT(bigbuf2);
					snprintf(bigbuf2,lenBigbuf,"%s%s%n%s",left,tvalue,&search_pos,right);
					expanded_ad->AssignExpr(curr_attr_to_expand, bigbuf2);
					dprintf(D_FULLDEBUG,"$$ substitution: %s=%s\n",curr_attr_to_expand.c_str(),bigbuf2);
					free(value);	// must use free here, not delete[]
					free(attribute_value);
					attribute_value = bigbuf2;
					bigbuf2 = nullptr;
				}
			}

			if(expanded_something && ! attribute_not_found && persist_expansions) {
				// Cache the expanded string so that we still
				// have it after, say, a restart and the collector
				// is no longer available.

				if( !started_transaction && !InTransaction() ) {
					started_transaction = true;
						// for efficiency, when storing multiple
						// expansions, do it all in one transaction
					BeginTransaction();
				}
				if ( SetAttribute(cluster_id,proc_id,cachedAttrName.c_str(),attribute_value) < 0 )
				{
					EXCEPT("Failed to store '%s=%s' into job ad %d.%d",
						cachedAttrName.c_str(), attribute_value, cluster_id, proc_id);
				}
			}

		}

		if( started_transaction ) {
			// To reduce the number of fsyncs, we mark this as a non-durable transaction.
			// Otherwise we incur two fsync's per matched job (one here, one for the job start).
			CommitNonDurableTransactionOrDieTrying();
		}

		if ( startd_ad ) {
				// Copy NegotiatorMatchExprXXX attributes from startd ad
				// to the job ad.  These attributes were inserted by the
				// negotiator.
			size_t len = strlen(ATTR_NEGOTIATOR_MATCH_EXPR);
			for (auto & itr : *startd_ad) {
				if( !strncmp(itr.first.c_str(),ATTR_NEGOTIATOR_MATCH_EXPR,len) ) {
					ExprTree *expr = itr.second;
					if( !expr ) {
						continue;
					}
					const char *new_value = nullptr;
					new_value = ExprTreeToString(expr);
					ASSERT(new_value);
					expanded_ad->AssignExpr(itr.first,new_value);

					std::string match_exp_name = MATCH_EXP;
					match_exp_name += itr.first;
					if ( SetAttribute(cluster_id,proc_id,match_exp_name.c_str(),new_value) < 0 )
					{
						EXCEPT("Failed to store '%s=%s' into job ad %d.%d",
						       match_exp_name.c_str(), new_value, cluster_id, proc_id);
					}
				}
			}

			// copy provisioned resources from startd ad to job ad
			std::string resslist;
			if (startd_ad->LookupString(ATTR_MACHINE_RESOURCES, resslist)) {
				expanded_ad->Assign("ProvisionedResources", resslist);
			} else {
				resslist = "Cpus, Disk, Memory";
			}

			for (auto& resname: StringTokenIterator(resslist)) {
				std::string res = resname;
				title_case(res); // capitalize it to make it print pretty.

				std::string attr;
				classad::Value val;
				// mask of the types of values we should propagate into the expanded ad.
				const int value_type_ok = classad::Value::ERROR_VALUE | classad::Value::BOOLEAN_VALUE | classad::Value::INTEGER_VALUE | classad::Value::REAL_VALUE;

				// propagate Disk, Memory, etc attributes into the job ad
				// as DiskProvisionedDisk, MemoryProvisioned, etc.  note that we 
				// evaluate rather than lookup the value so we collapse expressions
				// into literal values at this point.
				if (EvalAttr(resname.c_str(), ad, startd_ad, val)) {
					classad::Value::ValueType vt = val.GetType();
					if (vt & value_type_ok) {
						classad::ExprTree * plit = classad::Literal::MakeLiteral(val);
						if (plit) {
							attr = res + "Provisioned";
							expanded_ad->Insert(attr.c_str(), plit);
						}
					}
				}

				// evaluate RequestDisk, RequestMemory and convert to literal 
				// values in the expanded job ad.
				attr = "Request"; attr += res;
				if (EvalAttr(attr.c_str(), ad, startd_ad, val)) {
					classad::Value::ValueType vt = val.GetType();
					if (vt & value_type_ok) {
						classad::ExprTree * plit = classad::Literal::MakeLiteral(val);
						if (plit) {
							expanded_ad->Insert(attr.c_str(), plit);
						}
					}
				}
			}
			// end copying provisioned resources from startd ad to job ad

		}

		if ( attribute_not_found ) {
			std::string hold_reason;
			// Don't put the $$(expr) literally in the hold message, otherwise
			// if we fix the original problem, we won't be able to expand the one
			// in the hold message
			formatstr(hold_reason,"Cannot expand $$ expression (%s).",name);

			// no ClassAd in the match record; probably
			// an older negotiator.  put the job on hold and send email.
			dprintf( D_ALWAYS, 
				"Putting job %d.%d on hold - cannot expand $$(%s)\n",
				 cluster_id, proc_id, name );
			// SetAttribute does security checks if Q_SOCK is not NULL.
			// So, set Q_SOCK to be NULL before placing the job on hold
			// so that SetAttribute knows this request is not coming from
			// a client.  Then restore Q_SOCK back to the original value.
			QmgmtPeer* saved_sock = Q_SOCK;
			Q_SOCK = nullptr;
			holdJob(cluster_id, proc_id, hold_reason.c_str());
			Q_SOCK = saved_sock;

			char buf[256];
			snprintf(buf,256,"Your job (%d.%d) is on hold",cluster_id,proc_id);
			Email mailer;
			FILE * email = mailer.open_stream( ad, JOB_SHOULD_HOLD, buf );
			if ( email ) {
				fprintf(email,"Condor failed to start your job %d.%d \n",
					cluster_id,proc_id);
				fprintf(email,"because job attribute %s contains $$(%s).\n",
					bad_attr_name.c_str(),name);
				fprintf(email,"\nAttribute $$(%s) cannot be expanded because",
					name);
				fprintf(email,"\nthis attribute was not found in the "
						"machine ClassAd.\n");
				fprintf(email,
					"\n\nPlease correct this problem and release your "
					"job with:\n   \"condor_release %d.%d\"\n\n",
					cluster_id,proc_id);
				mailer.send();
			}
		}

		if ( attribute_value ) free(attribute_value);
		if ( bigbuf2 ) free (bigbuf2);

		if ( attribute_not_found )
			return nullptr;
		else 
			return expanded_ad;
}


// Rewrite the job ad when input files will be spooled from a remote
// submitter. Change Iwd to the job's spool directory and change other
// file paths to be relative to the new Iwd. Save the original values
// as SUBMIT_...
// modify_ad is a boolean that says whether changes should be applied
// directly to the provided job ClassAd or done via the job queue
// interface (e.g. SetAttribute()).
// If SUBMIT_Iwd is already set, we assume rewriting has already been
// performed.
// Return true if any changes were made, false otherwise.
bool
rewriteSpooledJobAd(ClassAd *job_ad, int cluster, int proc, bool modify_ad)
{
		// These three lists must be kept in sync!
	static const int ATTR_ARRAY_SIZE = 6;
	static const char *AttrsToModify[ATTR_ARRAY_SIZE] = {
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY,
		"DataReuseManifestSHA256" };
	static const bool AttrIsList[ATTR_ARRAY_SIZE] = {
		false,
		false,
		true,
		false,
		false,
		false };
	static const char *AttrXferBool[ATTR_ARRAY_SIZE] = {
		ATTR_TRANSFER_EXECUTABLE,
		ATTR_TRANSFER_INPUT,
		nullptr,
		nullptr,
		nullptr,
		nullptr };

	int attrIndex = 0;
	char new_attr_name[500];
	char *buf = nullptr;
	ExprTree *expr = nullptr;
	std::string SpoolSpace;

	snprintf(new_attr_name,500,"SUBMIT_%s",ATTR_JOB_IWD);
	if ( job_ad->LookupExpr( new_attr_name ) ) {
			// Job ad has already been rewritten. Nothing to do.
		return false;
	}

	SpooledJobFiles::getJobSpoolPath(job_ad, SpoolSpace);
	ASSERT(!SpoolSpace.empty());

		// Backup the original IWD at submit time
	job_ad->LookupString(ATTR_JOB_IWD,&buf);
	if ( buf ) {
		if ( modify_ad ) {
			job_ad->Assign(new_attr_name,buf);
		} else {
			SetAttributeString(cluster,proc,new_attr_name,buf);
		}
		free(buf);
		buf = nullptr;
	} else {
		if ( modify_ad ) {
			job_ad->AssignExpr(new_attr_name,"Undefined");
		} else {
			SetAttribute(cluster,proc,new_attr_name,"Undefined");
		}
	}
		// Modify the IWD to point to the spool space
	if ( modify_ad ) {
		job_ad->Assign(ATTR_JOB_IWD,SpoolSpace);
	} else {
		SetAttributeString(cluster,proc,ATTR_JOB_IWD,SpoolSpace.c_str());
	}

		// Backup the original TRANSFER_OUTPUT_REMAPS at submit time
	std::string url_remap_commands;
	expr = job_ad->LookupExpr(ATTR_TRANSFER_OUTPUT_REMAPS);
	snprintf(new_attr_name,500,"SUBMIT_%s",ATTR_TRANSFER_OUTPUT_REMAPS);
	if ( expr ) {

			// Try to parse out the URL remaps; those stay in the original
			// attribute.
		std::string remap_string;
		if (job_ad->EvaluateAttrString(ATTR_TRANSFER_OUTPUT_REMAPS, remap_string)) {
			std::string remap_commands;
			for (auto& command: StringTokenIterator(remap_string, ";")) {
				std::vector<std::string> command_parts = split(command, " =");
				if (command_parts.size() != 2) {continue;}
				auto dest = command_parts[1].c_str();
				if (IsUrl(dest)) {
					if (!url_remap_commands.empty()) {
						url_remap_commands += ";";
					}
					url_remap_commands += command;
				} else {
					if (!remap_commands.empty()) {
						remap_commands += ";";
					}
					remap_commands += command;
				}
			}
			if (modify_ad) {
				job_ad->InsertAttr(new_attr_name, remap_commands);
			} else {
				SetAttributeString(cluster, proc, new_attr_name, remap_commands.c_str());
			}
		} else {
			const char *remap_buf = ExprTreeToString(expr);
			ASSERT(remap_buf);
			if ( modify_ad ) {
				job_ad->AssignExpr(new_attr_name, remap_buf);
			} else {
				SetAttribute(cluster,proc,new_attr_name,remap_buf);
			}
		}
	}
	else if(job_ad->LookupExpr(new_attr_name)) {
			// SUBMIT_TransferOutputRemaps is defined, but
			// TransferOutputRemaps is not; disable the former,
			// so that when somebody fetches the sandbox, nothing
			// gets remapped.
		if ( modify_ad ) {
			job_ad->AssignExpr(new_attr_name,"Undefined");
		} else {
			SetAttribute(cluster,proc,new_attr_name,"Undefined");
		}
	}
		// Set TRANSFER_OUTPUT_REMAPS to Undefined so that we don't
		// do remaps when the job's output files come back into the
		// spool space. We only want to remap when the submitter
		// retrieves the files.
	if ( modify_ad ) {
		if (url_remap_commands.empty()) {
			job_ad->AssignExpr(ATTR_TRANSFER_OUTPUT_REMAPS, "Undefined");
		} else {
			job_ad->InsertAttr(ATTR_TRANSFER_OUTPUT_REMAPS, url_remap_commands);
		}
	} else {
		if (url_remap_commands.empty()) {
			SetAttribute(cluster,proc,ATTR_TRANSFER_OUTPUT_REMAPS,"Undefined");
		} else {
			SetAttributeString(cluster, proc, ATTR_TRANSFER_OUTPUT_REMAPS, url_remap_commands.c_str());
		}
	}

		// Now, for all the attributes listed in 
		// AttrsToModify, change them to be relative to new IWD
		// by taking the basename of all file paths.
	for ( attrIndex = 0; attrIndex < ATTR_ARRAY_SIZE; attrIndex++ ) {
			// Lookup original value
		bool xfer_it = false;
		if (buf) free(buf);
		buf = nullptr;
		job_ad->LookupString(AttrsToModify[attrIndex],&buf);
		if (!buf) {
			// attribute not found, so no need to modify it
			continue;
		}
		if ( nullFile(buf) ) {
			// null file -- no need to modify it
			continue;
		}
		if ( AttrXferBool[attrIndex] &&
			 job_ad->LookupBool( AttrXferBool[attrIndex], xfer_it ) && !xfer_it ) {
				// ad says not to transfer this file, so no need
				// to modify it
			continue;
		}
			// Create new value - deal with the fact that
			// some of these attributes contain a list of pathnames
		std::vector<std::string> old_paths;
		std::string new_paths;
		if ( AttrIsList[attrIndex] ) {
			old_paths = split(buf);
		} else {
			old_paths.emplace_back(buf);
		}
		bool changed = false;
		const char *base = nullptr;
		for (auto& old_path_buf: old_paths) {
			base = condor_basename(old_path_buf.c_str());
			if ((strcmp(AttrsToModify[attrIndex], ATTR_TRANSFER_INPUT_FILES)==0) && IsUrl(old_path_buf.c_str())) {
				base = old_path_buf.c_str();
			} else if ( strcmp(base,old_path_buf.c_str())!=0 ) {
				changed = true;
			}
			if (!new_paths.empty()) new_paths += ',';
			new_paths += base;
		}
		if ( changed ) {
				// Backup original value
			snprintf(new_attr_name,500,"SUBMIT_%s",AttrsToModify[attrIndex]);
			if ( modify_ad ) {
				job_ad->Assign(new_attr_name,buf);
			} else {
				SetAttributeString(cluster,proc,new_attr_name,buf);
			}
				// Store new value
			if ( modify_ad ) {
				job_ad->Assign(AttrsToModify[attrIndex],new_paths);
			} else {
				SetAttributeString(cluster,proc,AttrsToModify[attrIndex],new_paths.c_str());
			}
		}
	}
	if (buf) free(buf);
	return true;
}


JobQueueJob *
GetJobAd(const PROC_ID &job_id)
{
	JobQueueJob	*job = nullptr;
	if (JobQueue && JobQueue->Lookup(JobQueueKey(job_id), job)) {
		return job;
	}
	return nullptr;
}

int GetJobInfo(JobQueueJob *job, const OwnerInfo*& powni)
{
	if (job) {
		powni = job->ownerinfo;
		return job->Universe();
	}
	powni = nullptr;
	return CONDOR_UNIVERSE_MIN;
}

JobQueueJob* 
GetJobAndInfo(const PROC_ID& jid, int &universe, const OwnerInfo *&powni)
{
	universe = CONDOR_UNIVERSE_MIN;
	powni = nullptr;
	JobQueueJob	*job = nullptr;
	if (JobQueue && JobQueue->Lookup(JobQueueKey(jid), job)) {
		universe = GetJobInfo(job, powni);
		return job;
	}
	return nullptr;
}

JobQueueJob*
GetJobAd(int cluster_id, int proc_id)
{
	return GetJobAd(JOB_ID_KEY(cluster_id, proc_id));
}

JobQueueCluster*
GetClusterAd(int cluster_id)
{
	JobQueueKey jid(cluster_id, -1);
	JobQueueJob	*job = nullptr;
	if (JobQueue && JobQueue->Lookup(jid, job)) {
		return dynamic_cast<JobQueueCluster*>(job);
	}
	return nullptr;
}

JobQueueCluster *
GetClusterAd(const PROC_ID &job_id)
{
	return GetClusterAd(job_id.cluster);
}

JobQueueJobSet*
GetJobSetAd(unsigned int jobset_id)
{
	if (jobset_id > 0) {
		JobQueueKey jid(JOBSETID_to_qkey1(jobset_id), JOBSETID_qkey2);
		JobQueueBase *bad = nullptr;
		if (JobQueue && JobQueue->Lookup(jid, bad)) {
			return dynamic_cast<JobQueueJobSet*>(bad);
		}
	}
	return nullptr;
}

ClassAd* GetExpandedJobAd(const PROC_ID& job_id, bool persist_expansions)
{
	JobQueueJob *job = GetJobAd(job_id);
	if ( ! job)
		return nullptr;

	ClassAd *ad = job;
	ClassAd *startd_ad = nullptr;

	if (job->Universe() != CONDOR_UNIVERSE_GRID) {
		// find startd ad via the match rec
		match_rec *mrec = nullptr;
		int sendToDS = 0;
		ad->LookupInteger(ATTR_WANT_PARALLEL_SCHEDULING, sendToDS);
		if ((job->Universe() == CONDOR_UNIVERSE_PARALLEL) ||
			(job->Universe() == CONDOR_UNIVERSE_MPI) ||
			sendToDS) {
			mrec = dedicated_scheduler.FindMRecByJobID( job_id );
		} else {
			mrec = scheduler.FindMrecByJobID( job_id );
		}

		if( mrec ) {
			startd_ad = mrec->my_match_ad;
		} else {
			if (job->Universe() == CONDOR_UNIVERSE_LOCAL) {
				startd_ad = scheduler.getLocalStarterAd();
			} else {
				startd_ad = nullptr;
			}
		}

	}

	return dollarDollarExpand(job_id.cluster, job_id.proc, ad, startd_ad, persist_expansions);
}

// We have to define this to prevent the version in qmgmt_stubs from being pulled into the schedd.
ClassAd * GetJobAd_as_ClassAd(int cluster_id, int proc_id, bool expStartdAd, bool persist_expansions) {
	ClassAd* ad = nullptr;
	if (expStartdAd) {
		ad = GetExpandedJobAd(JOB_ID_KEY(cluster_id, proc_id), persist_expansions);
	} else {
		ad = GetJobAd(JOB_ID_KEY(cluster_id, proc_id));
	}
	return ad;
}

// DO NOT MOVE to condor_utils!! 
// this function has a static expression cache
// because we can't easily change the iterating queue
// code that uses string form constraints..
static bool EvalConstraint(ClassAd *ad, const char *constraint)
{
	// NOTE: static constraint holder so we cache the last-used constraint and save parsing...
	static ConstraintHolder constr;

	if ( ! constraint || ! constraint[0]) return true;

	if (constr.empty() || MATCH != strcmp(constr.c_str(),constraint)) {
		constr.set(strdup(constraint));
		int err = 0;
		if ( ! constr.Expr(&err) && err) {
			dprintf( D_ALWAYS, "can't parse constraint: %s\n", constraint );
			return false;
		}
	}

	classad::Value result;
	if ( !EvalExprToBool( constr.Expr(), ad, nullptr, result ) ) {
		dprintf( D_ALWAYS, "can't evaluate constraint: %s\n", constraint );
		return false;
	}

	bool boolVal = false;
	if( result.IsBooleanValueEquiv( boolVal ) ) {
		return boolVal;
	}

	dprintf( D_FULLDEBUG, "constraint (%s) does not evaluate to bool\n", constraint );
	return false;
}


JobQueueJob *
GetJobByConstraint(const char *constraint)
{
	JobQueuePayload ad = nullptr;
	JobQueueKey key;

	JobQueue->StartIterateAllClassAds();
	while(JobQueue->Iterate(key,ad)) {
		if (ad->IsJob() && EvalConstraint(ad, constraint)) {
			return dynamic_cast<JobQueueJob*>(ad);
		}
	}
	return nullptr;
}

// declare this so that we don't try and pull in the one in send_stubs
ClassAd * GetJobByConstraint_as_ClassAd(const char *constraint) {
	return GetJobByConstraint(constraint);
}

JobQueueJob *
GetNextJob(int initScan)
{
	return GetNextJobByConstraint(nullptr, initScan);
}


JobQueueJob *
GetNextJobByConstraint(const char *constraint, int initScan)
{
	JobQueuePayload ad = nullptr;
	JobQueueKey key;

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->Iterate(key,ad)) {
		if ( ad->IsJob() &&	EvalConstraint(ad, constraint)) {
			return dynamic_cast<JobQueueJob*>(ad);
		}
	}
	return nullptr;
}

JobQueueJob *
GetNextJobOrClusterByConstraint(const char *constraint, int initScan)
{
	JobQueuePayload ad = nullptr;
	JobQueueKey key;

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->Iterate(key,ad)) {
		if ((ad->IsCluster() || ad->IsJob()) && EvalConstraint(ad, constraint)) {
			return dynamic_cast<JobQueueJob*>(ad);
		}
	}
	return nullptr;
}

// declare this so that we don't try and pull in the one in send_stubs
ClassAd * GetNextJobByConstraint_as_ClassAd(const char *constraint, int initScan) {
	return GetNextJobByConstraint(constraint, initScan);
}

JobQueueJob *
GetNextDirtyJobByConstraint(const char *constraint, int initScan)
{
	JobQueueJob *ad = nullptr;

	if (initScan) {
		DirtyJobIDsItr = DirtyJobIDs.begin();
	}

	while (DirtyJobIDsItr != DirtyJobIDs.end()) {
		if( !JobQueue->Lookup( *DirtyJobIDsItr, ad ) ) {
			std::string id_str;
			DirtyJobIDsItr->sprint(id_str);
			dprintf(D_ALWAYS, "Warning: Job %s is marked dirty, but could not find in the job queue.  Skipping\n", id_str.c_str());
		} else if (EvalConstraint(ad, constraint)) {
			return ad;
		}
		DirtyJobIDsItr++;
	}
	return nullptr;
}


/*
void
FreeJobAd(ClassAd *&ad)
{
	ad = NULL;
}
*/

static int
RecvSpoolFileBytes(const char *path)
{
	filesize_t	size = 0;
	Q_SOCK->getReliSock()->decode();
	if (Q_SOCK->getReliSock()->get_file(&size, path) < 0) {
		dprintf(D_ALWAYS,
		        "Failed to receive file from client in SendSpoolFile.\n");
		Q_SOCK->getReliSock()->end_of_message();
		return -1;
	}
	IGNORE_RETURN chmod(path,00755);
	Q_SOCK->getReliSock()->end_of_message();
	dprintf(D_FULLDEBUG, "done with transfer, errno = %d\n", errno);
	return 0;
}

int
SendSpoolFile(char const *)
{
	char * path = nullptr;

		// We ignore the filename that was passed by the client.
		// It is only there for backward compatibility reasons.

	path = GetSpooledExecutablePath(active_cluster_num, Spool);
	ASSERT( path );

	if ( !Q_SOCK || !Q_SOCK->getReliSock() ) {
		EXCEPT( "SendSpoolFile called when Q_SOCK is NULL" );
	}

	if( !make_parents_if_needed( path, 0755, PRIV_CONDOR ) ) {
		int terrno = errno;;
		dprintf(D_ALWAYS, "Failed to create spool directory for %s.\n", path);
		Q_SOCK->getReliSock()->put(-1);
		Q_SOCK->getReliSock()->put(terrno);
		Q_SOCK->getReliSock()->end_of_message();
		free(path);
		return -1;
	}

	/* Tell client to go ahead with file transfer. */
	Q_SOCK->getReliSock()->encode();
	Q_SOCK->getReliSock()->put(0);
	Q_SOCK->getReliSock()->end_of_message();

	int rv = RecvSpoolFileBytes(path);
	free(path); path = nullptr;
	return rv;
}

int
SendSpoolFileIfNeeded(ClassAd& /*ad*/)
{
	if ( !Q_SOCK || !Q_SOCK->getReliSock() ) {
		EXCEPT( "SendSpoolFileIfNeeded called when Q_SOCK is NULL" );
	}
	Q_SOCK->getReliSock()->encode();

	char *path = GetSpooledExecutablePath(active_cluster_num, Spool);
	ASSERT( path );

	StatInfo exe_stat( path );
	if ( exe_stat.Error() == SIGood ) {
		Q_SOCK->getReliSock()->put(1);
		Q_SOCK->getReliSock()->end_of_message();
		free(path);
		return 0;
	}

	if( !make_parents_if_needed( path, 0755, PRIV_CONDOR ) ) {
		dprintf(D_ALWAYS, "Failed to create spool directory for %s.\n", path);
		Q_SOCK->getReliSock()->put(-1);
		Q_SOCK->getReliSock()->end_of_message();
		free(path);
		return -1;
	}

	/* Tell client to go ahead with file transfer. */
	Q_SOCK->getReliSock()->put(0);
	Q_SOCK->getReliSock()->end_of_message();

	if (RecvSpoolFileBytes(path) == -1) {
		free(path); path = nullptr;
		return -1;
	}

	free(path); path = nullptr;
	return 0;
}

} /* should match the extern "C" */


// probes for timing the autoclustering code
schedd_runtime_probe GetAutoCluster_runtime;
schedd_runtime_probe GetAutoCluster_hit_runtime;
schedd_runtime_probe GetAutoCluster_signature_runtime;
schedd_runtime_probe GetAutoCluster_cchit_runtime;
double last_autocluster_runtime;
bool   last_autocluster_make_sig;
int    last_autocluster_type=0;
int    last_autocluster_classad_cache_hit=0;
stats_entry_abs<int> SCGetAutoClusterType;

int get_job_prio(JobQueueJob *job, const JOB_ID_KEY & jid, void *)
{
    int     job_prio = 0, 
            pre_job_prio1 = 0, 
            pre_job_prio2 = 0, 
            post_job_prio1 = 0, 
            post_job_prio2 = 0;
    char    owner[100];
	const char* dummy = nullptr;

	ASSERT(job);

	owner[0] = 0;

		// We must call getAutoClusterid() in get_job_prio!!!  We CANNOT
		// return from this function before we call getAutoClusterid(), so call
		// it early on (before any returns) right now.  The reason for this is
		// getAutoClusterid() performs a mark/sweep algorithm to garbage collect
		// old autocluster information.  If we fail to call getAutoClusterid, the
		// autocluster information for this job will be removed, causing the schedd
		// to ASSERT later on in the autocluster code. 
		// Quesitons?  Ask Todd <tannenba@cs.wisc.edu> 01/04
	last_autocluster_runtime = 0;
	last_autocluster_classad_cache_hit = 1;
	last_autocluster_make_sig = false;

	int auto_id = scheduler.autocluster.getAutoClusterid(job);
	job->autocluster_id = auto_id;

	GetAutoCluster_runtime += last_autocluster_runtime;
	if (last_autocluster_make_sig) { GetAutoCluster_signature_runtime += last_autocluster_runtime; }
	else { GetAutoCluster_hit_runtime += last_autocluster_runtime; }
	SCGetAutoClusterType = last_autocluster_type;
	GetAutoCluster_cchit_runtime += last_autocluster_classad_cache_hit;

	GetAutoCluster_runtime += last_autocluster_runtime;
	if (last_autocluster_make_sig) { GetAutoCluster_signature_runtime += last_autocluster_runtime; }
	else { GetAutoCluster_hit_runtime += last_autocluster_runtime; }
	SCGetAutoClusterType = last_autocluster_type;
	GetAutoCluster_cchit_runtime += last_autocluster_classad_cache_hit;

	// Figure out if we should contine and put this job into the PrioRec array
	// or not.
	if ( ! Runnable(job, dummy) ||
			scheduler.AlreadyMatched(job, job->Universe()))
	{
		return 0;
	}

	// --- Insert this job into the PrioRec array ---

       // If pre/post prios are not defined as forced attributes, set them to INT_MIN
	// to flag priocompare routine to not use them.
	 
    if (!job->LookupInteger(ATTR_PRE_JOB_PRIO1, pre_job_prio1)) {
         pre_job_prio1 = 0;
    }
    if (!job->LookupInteger(ATTR_PRE_JOB_PRIO2, pre_job_prio2)) {
         pre_job_prio2 = 0;
    } 
    if (!job->LookupInteger(ATTR_POST_JOB_PRIO1, post_job_prio1)) {
         post_job_prio1 = 0;
    }	 
    if (!job->LookupInteger(ATTR_POST_JOB_PRIO2, post_job_prio2)) {
         post_job_prio2 = 0;
    }

    job->LookupInteger(ATTR_JOB_PRIO, job_prio);

	char * powner = owner;
	int cremain = sizeof(owner);
#ifdef NO_DEPRECATED_NICE_USER
	if( job->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
		strcpy(powner,NiceUserName);
		strcat(powner,".");
		int cch = (int)strlen(powner);
		powner += cch;
		cremain -= cch;
	}
#endif
		// Note, we should use this method instead of just looking up
		// ATTR_USER directly, since that includes UidDomain, which we
		// don't want for this purpose...
	job->LookupString(ATTR_ACCOUNTING_GROUP, powner, cremain);  // TODDCORE
	if (*powner == '\0') {
		std::string job_user;
		job->LookupString(attr_JobUser, job_user);
		auto last_at = job_user.find_last_of('@');
		auto accounting_domain = scheduler.accountingDomain();
		if (last_at != std::string::npos && !accounting_domain.empty()) {
			strncat(powner, job_user.substr(0, last_at).c_str(), cremain - 1);
			cremain -= last_at;
			strncat(powner, "@", cremain); cremain--;
			strncat(powner, accounting_domain.c_str(), cremain);
		} else {
			strncat(powner, job_user.c_str(), cremain - 1);
		}
	} else if (user_is_the_new_owner) {
		// AccountingGroup does not include a domain, but it needs to for this code
		auto accounting_domain = scheduler.accountingDomain();
		if (!accounting_domain.empty()) {
			strncat(powner, "@", cremain); cremain--;
			strncat(powner, accounting_domain.c_str(), cremain);
		}
	}

    PrioRec[N_PrioRecs].id             = jid;
    PrioRec[N_PrioRecs].job_prio       = job_prio;
    PrioRec[N_PrioRecs].pre_job_prio1  = pre_job_prio1;
    PrioRec[N_PrioRecs].pre_job_prio2  = pre_job_prio2;
    PrioRec[N_PrioRecs].post_job_prio1 = post_job_prio1;
    PrioRec[N_PrioRecs].post_job_prio2 = post_job_prio2;
    PrioRec[N_PrioRecs].not_runnable   = false;
    PrioRec[N_PrioRecs].matched        = false;
	if ( auto_id == -1 ) {
		PrioRec[N_PrioRecs].auto_cluster_id = jid.cluster;
	} else {
		PrioRec[N_PrioRecs].auto_cluster_id = auto_id;
	}

	PrioRec[N_PrioRecs].submitter = powner;

    N_PrioRecs += 1;
	if ( N_PrioRecs == MAX_PRIO_REC ) {
		grow_prio_recs( 2 * N_PrioRecs );
	}

	return 0;
}

bool
jobLeaseIsValid( ClassAd* job, int cluster, int proc )
{
	int last_renewal = 0, duration = 0;
	time_t now = 0;
	if( ! job->LookupInteger(ATTR_JOB_LEASE_DURATION, duration) ) {
		return false;
	}
	if( ! job->LookupInteger(ATTR_LAST_JOB_LEASE_RENEWAL, last_renewal) ) {
		return false;
	}
	now = time(nullptr);
	int diff = now - last_renewal;
	int remaining = duration - diff;
	dprintf( D_FULLDEBUG, "%d.%d: %s is defined: %d\n", cluster, proc, 
			 ATTR_JOB_LEASE_DURATION, duration );
	dprintf( D_FULLDEBUG, "%d.%d: now: %lld, last_renewal: %d, diff: %d\n",
			 cluster, proc, (long long)now, last_renewal, diff );

	if( remaining <= 0 ) {
		dprintf( D_FULLDEBUG, "%d.%d: %s remaining: EXPIRED!\n", 
				 cluster, proc, ATTR_JOB_LEASE_DURATION );
		return false;
	} 
	dprintf( D_FULLDEBUG, "%d.%d: %s remaining: %d\n", cluster, proc,
			 ATTR_JOB_LEASE_DURATION, remaining );
	return true;
}

extern void mark_job_stopped(PROC_ID* job_id);

int mark_idle(JobQueueJob *job, const JobQueueKey& /*key*/, void* /*pvArg*/)
{
	if ( jobExternallyManaged(job) ) {
		// if a job is externally managed, don't touch a damn
		// thing!!!  the gridmanager or schedd-on-the-side is
		// in control.  stay out of its way!  -Todd 9/13/02
		// -amended by Jaime 10/4/05
		return 1;
	}

	int universe = job->Universe();
	int cluster = job->jid.cluster;
	int proc = job->jid.proc;
	PROC_ID job_id = job->jid;

	int status = 0, hosts = 0;
	job->LookupInteger(ATTR_JOB_STATUS, status);
	job->LookupInteger(ATTR_CURRENT_HOSTS, hosts);

	if ( status == COMPLETED || status == JOB_STATUS_FAILED ) {
		DestroyProc(cluster,proc);
	} else if ( status == REMOVED ) {
		// a globus job with a non-null contact string should be left alone
		if ( universe == CONDOR_UNIVERSE_GRID ) {
			if ( job->LookupString( ATTR_GRID_JOB_ID, nullptr, 0 ) && !jobManagedDone(job) )
			{
				// looks like the job's remote job id is still valid,
				// so there is still a job submitted remotely somewhere.
				// don't touch this job -- leave it alone so the gridmanager
				// completes the task of removing it from the remote site.
				return 1;
			}
		}
		dprintf( D_FULLDEBUG, "Job %d.%d was left marked as removed, "
				 "cleaning up now\n", cluster, proc );
		scheduler.WriteAbortToUserLog( job_id );
		DestroyProc( cluster, proc );
	}
	else if ( status == SUSPENDED || status == RUNNING || status == TRANSFERRING_OUTPUT || hosts > 0 ) {
		bool lease_valid = jobLeaseIsValid( job, cluster, proc );
		if( universeCanReconnect(universe) && lease_valid )
		{
			bool result = false;
			dprintf( D_FULLDEBUG, "Job %d.%d might still be alive, "
					 "spawning shadow to reconnect\n", cluster, proc );
			if (universe == CONDOR_UNIVERSE_PARALLEL) {
				dedicated_scheduler.enqueueReconnectJob( job_id);	
			} else {
				result = scheduler.enqueueReconnectJob( job_id );
				if ( result ) {
					scheduler.stats.JobsRestartReconnectsAttempting += 1;
				} else {
					scheduler.stats.JobsRestartReconnectsFailed += 1;
				}
			}
		} else {
			if ( universeCanReconnect(universe) && !lease_valid &&
				 ( universe != CONDOR_UNIVERSE_PARALLEL || proc == 0 ) ) {
				scheduler.stats.JobsRestartReconnectsLeaseExpired += 1;
			}
			mark_job_stopped(&job_id);
		}
	}
		
	int wall_clock_ckpt = 0;
	GetAttributeInt(cluster,proc,ATTR_JOB_WALL_CLOCK_CKPT, &wall_clock_ckpt);
	if (wall_clock_ckpt) {
			// Schedd must have died before committing this job's wall
			// clock time.  So, commit the wall clock saved in the
			// wall clock checkpoint.
		double wall_clock = 0.0;
		GetAttributeFloat(cluster,proc,ATTR_JOB_REMOTE_WALL_CLOCK,&wall_clock);
		wall_clock += wall_clock_ckpt;
		BeginTransaction();
		SetAttributeFloat(cluster,proc,ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock);
		DeleteAttribute(cluster,proc,ATTR_JOB_WALL_CLOCK_CKPT);
			// remove shadow birthdate so if CkptWallClock()
			// runs before a new shadow starts, it won't
			// potentially double-count
		DeleteAttribute(cluster,proc,ATTR_SHADOW_BIRTHDATE);

		double slot_weight = 1;
		GetAttributeFloat(cluster, proc,
						  ATTR_JOB_MACHINE_ATTR_SLOT_WEIGHT0,&slot_weight);
		double slot_time = 0;
		GetAttributeFloat(cluster, proc,
						  ATTR_CUMULATIVE_SLOT_TIME,&slot_time);
		slot_time += wall_clock_ckpt*slot_weight;
		SetAttributeFloat(cluster, proc,
						  ATTR_CUMULATIVE_SLOT_TIME,slot_time);

		// Commit non-durable to speed up recovery; this is ok because a) after
		// all jobs are marked idle in mark_jobs_idle() we force the log, and 
		// b) in the worst case, we would just redo this work in the unfortuante evenent 
		// we crash again before an fsync.
		CommitNonDurableTransactionOrDieTrying();
	}

	return 1;
}

bool InWalkJobQueue() {
	return in_walk_job_queue != 0;
}


// this function for use only inside the schedd
void
WalkJobQueueEntries(int with, queue_scan_func func, void* pv, schedd_runtime_probe & ftm)
{
	double begin = _condor_debug_get_time_double();
	const bool with_jobsets = (with & WJQ_WITH_JOBSETS) != 0;
	const bool with_clusters = (with & WJQ_WITH_CLUSTERS) != 0;
	const bool with_no_jobs = (with & WJQ_WITH_NO_JOBS) != 0;
	int num_jobs = 0;
	bool stopped_early = false;

	// This means the walk terminated early, and we don't have a valid count
	JobsSeenOnQueueWalk = -1;

	if( in_walk_job_queue ) {
		dprintf(D_ALWAYS,"ERROR: WalkJobQueue called recursively!  Generating stack trace:\n");
		dprintf_dump_stack();
	}

	in_walk_job_queue++;

	JobQueue->StartIterateAllClassAds();

	JobQueueKey key;
	JobQueuePayload ad = nullptr;
	while(JobQueue->Iterate(key, ad)) {
		if (ad->IsHeader()) // skip header ad
			continue;
		if (ad->IsJobSet()) {
			if (! with_jobsets) { continue; }
		} else if (ad->IsCluster()) { // cluster ads have cluster > 0 && proc < 0
			if (! with_clusters) { continue; }
		} else if ( ! ad->IsJob()) {
			continue;
		} else { // jobads have cluster > 0 && proc >= 0
			num_jobs++;
			if (with_no_jobs) { continue; }
		}
		int rval = func(ad, key, pv);
		if (rval < 0) {
			stopped_early = true;
			break;
		}
	}

	double runtime = _condor_debug_get_time_double() - begin;
	ftm += runtime;
	WalkJobQ_runtime += runtime;

	if (!stopped_early) {
		JobsSeenOnQueueWalk = num_jobs;
	}

	in_walk_job_queue--;
}

// this function for use only inside the schedd
void
WalkJobQueue3(queue_job_scan_func func, void* pv, schedd_runtime_probe & ftm)
{
	double begin = _condor_debug_get_time_double();
	int num_jobs = 0;
	bool stopped_early = false;

	// This means the walk terminated early, and we don't have a valid count
	JobsSeenOnQueueWalk = -1;

	if( in_walk_job_queue ) {
		dprintf(D_ALWAYS,"ERROR: WalkJobQueue called recursively!  Generating stack trace:\n");
		dprintf_dump_stack();
	}

	in_walk_job_queue++;

	JobQueue->StartIterateAllClassAds();

	JobQueueKey key;
	JobQueuePayload ad = nullptr;
	while(JobQueue->Iterate(key, ad)) {
		if (ad->IsJob()) {
			num_jobs++;
			auto * job = dynamic_cast<JobQueueJob*>(ad);
			int rval = func(job, key, pv);
			if (rval < 0) {
				stopped_early = true;
				break;
			}
		}
	}

	double runtime = _condor_debug_get_time_double() - begin;
	ftm += runtime;
	WalkJobQ_runtime += runtime;

	if (!stopped_early) {
		JobsSeenOnQueueWalk = num_jobs;
	}

	in_walk_job_queue--;
}


int dump_job_q_stats(int cat)
{
	HashTable<JobQueueKey,JobQueuePayload>* table = JobQueue->Table();
	table->startIterations();
	int bucket=0, old_bucket=-1, item=0;

	int cTotalBuckets = 0;
	int cFilledBuckets = 0;
	int cOver1Buckets = 0;
	int cOver2Buckets = 0;
	int cOver3Buckets = 0;
	int cEmptyBuckets = 0;
	int maxItem = 0;
	int cItems = 0;

	std::string vis;
	//bool is_verbose = IsDebugVerbose(cat);

	while (table->iterate_stats(bucket, item) == 1) {
		if (0 == item) {
			int skip = bucket - old_bucket;
			old_bucket = bucket;
			cEmptyBuckets += skip-1;
			//if (is_verbose) { for (int ii = 0; ii < skip; ++ii) { vis += "\n"; } }
			++cFilledBuckets;
		} else if (1 == item) {
			++cOver1Buckets;
		} else if (2 == item) {
			++cOver2Buckets;
		} else if (3 == item) {
			++cOver3Buckets;
		}
		JobQueueKey key;
		table->getCurrentKey(key);
		//if (is_verbose) { vis += key.cluster ? (key.proc>=0 ? "j" : "c") : "0"; }
		maxItem = MAX(item, maxItem);
		++cItems;
	}
	cTotalBuckets = item;

	extern int job_hash_algorithm;
	dprintf(cat, "JobQueue hash(%d) table stats: Items=%d, TotalBuckets=%d, EmptyBuckets=%d, UsedBuckets=%d, OverusedBuckets=%d,%d,%d, LongestList=%d\n",
		job_hash_algorithm, cItems, cTotalBuckets, cEmptyBuckets, cFilledBuckets, cOver1Buckets, cOver2Buckets, cOver3Buckets, maxItem+1);
	//if (is_verbose) dprintf(cat | D_VERBOSE, "JobQueue {%s}\n", vis.c_str());

	return 0;
}


/*
** There should be no jobs running when we start up.  If any were killed
** when the last schedd died, they will still be listed as "running" in
** the job queue.  Here we go in and mark them as idle.
*/
void mark_jobs_idle()
{
	time_t bDay = time(nullptr);
	WalkJobQueue2(mark_idle, &bDay);

	// mark_idle() may have made a lot of commits in non-durable mode in 
	// order to speed up recovery after a crash, so recovery does not incur
	// the overhead of thousands of fsyncs.  Now do one fsync so that if
	// we crash again, we do not have to redo all recovery work just performed.
	JobQueue->ForceLog();
}

/*
** Called on startup to reload the job factories
*/
void load_job_factories()
{
	if ( ! scheduler.getAllowLateMaterialize()) {
		dprintf(D_ALWAYS, "SCHEDD_ALLOW_LATE_MATERIALIZE is false, skipping job factory initialization\n");
		return;
	}

	JobQueue->StartIterateAllClassAds();

	bool allow_unspooled_factories = false;
	if ( ! can_switch_ids()) {
		allow_unspooled_factories = param_boolean("SCHEDD_ALLOW_UNSPOOLED_JOB_FACTORIES", false);;
	}

	std::string submit_digest;

	dprintf(D_ALWAYS, "Reloading job factories\n");
	int num_loaded = 0;
	int num_failed = 0;
	int num_paused = 0;

	JobQueuePayload ad = nullptr;
	JobQueueKey key;
	while(JobQueue->Iterate(key,ad)) {
		if ( ! ad->IsCluster()) { continue; } // ingnore header and job ads

		auto * clusterad = dynamic_cast<JobQueueCluster*>(ad);
		if (clusterad->factory) { continue; } // ignore if factory already loaded

		submit_digest.clear();
		if (clusterad->LookupString(ATTR_JOB_MATERIALIZE_DIGEST_FILE, submit_digest)) {

			// we need to let MakeJobFactory know whether the digest has been spooled or not
			// because it needs to know whether to impersonate the user or not.
			std::string spooled_filename;
			GetSpooledSubmitDigestPath(spooled_filename, clusterad->jid.cluster, Spool);
			bool spooled_digest = ! allow_unspooled_factories || (YourStringNoCase(spooled_filename) == submit_digest);

			std::string errmsg;
			clusterad->factory = MakeJobFactory(clusterad, scheduler.getExtendedSubmitCommands(),
			                                    submit_digest.c_str(), spooled_digest,
			                                    scheduler.getProtectedUrlMap(), errmsg);
			if (clusterad->factory) {
				++num_loaded;
			} else {
				++num_failed;
				//PRAGMA_REMIND("Should this be a durable state change?")
				// if the factory failed to load, put it into a non-durable pause mode
				// a condor_q -factory will show the mmInvalid state, but it doesn't get reflected
				// in the job queue
				chomp(errmsg);
				setJobFactoryPauseAndLog(clusterad, mmInvalid, 0, errmsg);
			}
		}
		if (clusterad->factory) {
			int paused = 0;
			if (clusterad->LookupInteger(ATTR_JOB_MATERIALIZE_PAUSED, paused) && paused) {
				if (paused == mmInvalid && JobFactoryIsRunning(clusterad)) {
					// if the former pause mode was mmInvalid, but the factory loaded OK on this time
					// remove the pause since mmInvalid basically means 'factory failed to load'
					setJobFactoryPauseAndLog(clusterad, mmRunning, 0, "");
				} else {
					PauseJobFactory(clusterad->factory, (MaterializeMode)paused);
					++num_paused;
				}
			} else {
				// schedule materialize.
				ScheduleClusterForJobMaterializeNow(key.cluster);
			}
		}
	}
	dprintf(D_ALWAYS, "Loaded %d job factories, %d were paused, %d failed to load\n", num_loaded, num_paused, num_failed);
}


void DirtyPrioRecArray() {
		// Mark the PrioRecArray as stale. This will trigger a rebuild,
		// though possibly not immediately.
	PrioRecArrayIsDirty = true;
}

// runtime stats for count & time spent building the priorec array
//
schedd_runtime_probe BuildPrioRec_runtime;
schedd_runtime_probe BuildPrioRec_mark_runtime;
schedd_runtime_probe BuildPrioRec_walk_runtime;
schedd_runtime_probe BuildPrioRec_sort_runtime;
schedd_runtime_probe BuildPrioRec_sweep_runtime;

static void DoBuildPrioRecArray() {
	condor_auto_runtime rt(BuildPrioRec_runtime);
	double now = rt.begin;
	scheduler.autocluster.mark();
	BuildPrioRec_mark_runtime += rt.tick(now);

	N_PrioRecs = 0;
	WalkJobQueue(get_job_prio);
	BuildPrioRec_walk_runtime += rt.tick(now);

		// N_PrioRecs might be 0, if we have no jobs to run at the
		// moment. 
	if( N_PrioRecs ) {
		std::sort(PrioRec, PrioRec + N_PrioRecs, prio_compar{});
		BuildPrioRec_sort_runtime += rt.tick(now);
	}

	scheduler.autocluster.sweep();
	BuildPrioRec_sweep_runtime += rt.tick(now);

	if( !scheduler.shadow_prio_recs_consistent() ) {
		scheduler.mail_problem_message();
	}
}

/*
 * Force a rebuild of the PrioRec array if we're beyond the max interval
 * for a rebuild.
 * This is meant to be called periodically as a DaemonCore timer.
 */
void BuildPrioRecArrayPeriodic(int /* tid */)
{
	if ( time(nullptr) >= PrioRecArrayTimeslice.getStartTime().tv_sec + PrioRecRebuildMaxInterval ) {
		PrioRecArrayIsDirty = true;
		BuildPrioRecArray(false);
	}
}

/*
 * Build an array of runnable jobs sorted by priority.  If there are
 * a lot of jobs in the queue, this can be expensive, so avoid building
 * the array too often.
 * Arguments:
 *   no_match_found - caller can't find a runnable job matching
 *                    the requirements of an available startd, so
 *                    consider rebuilding the list sooner
 * Returns:
 *   true if the array was rebuilt; false otherwise
 */
bool BuildPrioRecArray(bool no_match_found /*default false*/) {

		// caller expects PrioRecAutoClusterRejected to be instantiated
		// (and cleared)
	if( ! PrioRecAutoClusterRejected ) {
		PrioRecAutoClusterRejected = new HashTable<int,int>(hashFuncInt);
		ASSERT( PrioRecAutoClusterRejected );
	}
	else {
		PrioRecAutoClusterRejected->clear();
	}

	if( !PrioRecArrayIsDirty ) {
		dprintf(D_FULLDEBUG,
				"Reusing prioritized runnable job list because nothing has "
				"changed.\n");
		return false;
	}

	if ( BuildPrioRecArrayTid < 0 ) {
		BuildPrioRecArrayTid = daemonCore->Register_Timer(
				PrioRecRebuildMaxInterval, PrioRecRebuildMaxInterval,
				&BuildPrioRecArrayPeriodic, "BuildPrioRecArrayPeriodic");
	}

		// run without any delay the first time
	PrioRecArrayTimeslice.setInitialInterval( 0 );

	PrioRecArrayTimeslice.setMaxInterval( PrioRecRebuildMaxInterval );
	if( no_match_found ) {
		PrioRecArrayTimeslice.setTimeslice( PrioRecRebuildMaxTimeSliceWhenNoMatchFound );
	}
	else {
		PrioRecArrayTimeslice.setTimeslice( PrioRecRebuildMaxTimeSlice );
	}

	if( !PrioRecArrayTimeslice.isTimeToRun() ) {

		dprintf(D_FULLDEBUG,
				"Reusing prioritized runnable job list to save time.\n");

		return false;
	}

	PrioRecArrayTimeslice.setStartTimeNow();
	PrioRecArrayIsDirty = false;

	DoBuildPrioRecArray();

	PrioRecArrayTimeslice.setFinishTimeNow();

	dprintf(D_ALWAYS,"Rebuilt prioritized runnable job list in %.3fs.%s\n",
			PrioRecArrayTimeslice.getLastDuration(),
			no_match_found ? "  (Expedited rebuild because no match was found)" : "");

	return true;
}

// whether or not a job should obey the START_VANILLA_UNIVERSE expression
bool UniverseUsesVanillaStartExpr(int universe)
{
	switch (universe) {
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_PARALLEL:
	case CONDOR_UNIVERSE_LOCAL:
	case CONDOR_UNIVERSE_GRID:
		return false;
	default:
		return true;
	}
}

/*
 * Find the job with the highest priority that matches with
 * my_match_ad (which is a startd ad).  If user is NULL, get a job for
 * any user; o.w. only get jobs for specified user.
 */
void FindRunnableJob(PROC_ID & jobid, ClassAd* my_match_ad, 
					 char const * user)
{
	JobQueueJob *ad = nullptr;

	if (user && (strlen(user) == 0)) {
		user = nullptr;
	}

	// this is true only when we are claiming the local startd
	// because the negotiator went missing for too long.
	bool match_any_user = (user == nullptr) ? true : false;

	ASSERT(my_match_ad);

	bool restrict_to_user = false;
	std::string match_user;
	my_match_ad->LookupBool(ATTR_RESTRICT_TO_AUTHENTICATED_IDENTITY, restrict_to_user);
	if (restrict_to_user) {
		my_match_ad->LookupString(ATTR_AUTHENTICATED_IDENTITY, match_user);
	}

		// indicate failure by setting proc to -1.  do this now
		// so if we bail out early anywhere, we say we failed.
	jobid.proc = -1;	

	// we want to use fully qualified username to do OwnerInfo lookup

#ifdef USE_VANILLA_START
	std::string job_attr("JOB");
	bool eval_for_each_job = false;
	bool start_is_true = true;
	VanillaMatchAd vad;
	const OwnerInfo * powni = scheduler.lookup_owner_const(user);
	vad.Init(my_match_ad, powni, nullptr);
	if ( ! scheduler.vanillaStartExprIsConst(vad, start_is_true)) {
		eval_for_each_job = true;
		if (IsDebugLevel(D_MATCH)) {
			std::string slotname = "<none>";
			if (my_match_ad) { my_match_ad->LookupString(ATTR_NAME, slotname); }
			dprintf(D_MATCH, "VANILLA_START is const %d for user=%s, slot=%s\n", start_is_true, user, slotname.c_str());
		}
	} else if ( ! start_is_true) {
		// START_VANILLA is const and false, no job will ever match, nothing more to do
		return;
	}
#endif

	bool rebuilt_prio_rec_array = BuildPrioRecArray();


		// Iterate through the most recently constructed list of
		// jobs, nicely pre-sorted first by submitter, then by job priority

	// Stringify the user to make comparison faster
	std::string user_str = user ? user : "";

	do {
		prio_rec *first = &PrioRec[0];
		prio_rec *end = &PrioRec[N_PrioRecs];

		if (!match_any_user) {
			first = std::lower_bound(first, end, user_str, prio_rec_submitter_lb{});
			end = std::upper_bound(first, end, user_str, prio_rec_submitter_ub{});
		}

		for (prio_rec *p = first; p != end; p++) {
			if ( p->not_runnable || p->matched ) {
					// This record has been disabled, because it is no longer
					// runnable or already matched.
				continue;
			}

			ad = GetJobAd( p->id.cluster, p->id.proc );
			if (!ad) {
					// This ad must have been deleted since we last built
					// runnable job list.
				continue;
			}	

			int junk = 0; // don't care about the value
			if ( PrioRecAutoClusterRejected->lookup( p->auto_cluster_id, junk ) == 0 ) {
					// We have already failed to match a job from this same
					// autocluster with this machine.  Skip it.
				continue;
			}

			if (scheduler.AlreadyMatched(&p->id)) {
				p->matched = true;
			}
			if (!Runnable(&p->id)) {
				p->not_runnable = true;
			}
			if (p->matched || p->not_runnable) {
					// This job's status must have changed since the
					// time it was added to the runnable job list.
					// The not_runnable and matched flags will prevent
					// this job from being considered in any
					// future iterations through the list.
				dprintf(D_FULLDEBUG,
						"record for job %d.%d skipped until PrioRec rebuild (%s)\n",
						p->id.cluster, p->id.proc, p->matched ? "already matched" : "no longer runnable");

					// Move along to the next job in the prio rec array
				continue;
			}

			if (!match_user.empty()) {
				// TODO Get the owner from the JobQueueJob object.
				//   Need to be careful about whether UID_DOMAIN is included.
				std::string job_user;
				ad->LookupString(ATTR_USER, job_user);
				if (match_user != job_user) {
					continue;
				}
			}

				// Now check if the job and the claimed resource match.
				// NOTE : we must do this AFTER we ensure the job is still runnable, which
				// is why we invoke Runnable() above first.
			if ( ! IsAMatch( ad, my_match_ad ) ) {
					// Job and machine do not match.
					// Assume that none of the other jobs in this auto-cluster will match.
					// THIS IS A DANGEROUS ASSUMPTION - what if this job is no longer
					// part of this autocluster?  TODO perhaps we should verify this
					// job is still part of this autocluster here.
				PrioRecAutoClusterRejected->insert( p->auto_cluster_id, 1 );
					// Move along to the next job in the prio rec array
				continue;
			}

				// Now check of the job can be started - this checks various schedd limits
				// as embodied by the START_VANILLA_UNIVERSE expression.
#ifdef USE_VANILLA_START
			if (eval_for_each_job) {
				vad.Insert(job_attr, ad);
				bool runnable = scheduler.evalVanillaStartExpr(vad);
				vad.Remove(job_attr);

				if ( ! runnable) {
					dprintf(D_FULLDEBUG | D_MATCH, "job %d.%d Matches, but START_VANILLA_UNIVERSE is false\n", ad->jid.cluster, ad->jid.proc);
						// Move along to the next job in the prio rec array
					continue;
				}
			}
#endif

				// Make sure that the startd ranks this job >= the
				// rank of the job that initially claimed it.
				// We stashed that rank in the startd ad when
				// the match was created.
				// (As of 6.9.0, the startd does not reject reuse
				// of the claim with lower RANK, but future versions
				// very well may.)

			double current_startd_rank = 0.0;
			if( my_match_ad &&
				my_match_ad->LookupFloat(ATTR_CURRENT_RANK, current_startd_rank) )
			{
				double new_startd_rank = 0;
				if( EvalFloat(ATTR_RANK, my_match_ad, ad, new_startd_rank) )
				{
					if( new_startd_rank < current_startd_rank ) {
						continue;
					}
				}
			}

				// If Concurrency Limits are in play it is
				// important not to reuse a claim from one job
				// that has one set of limits for a job that
				// has a different set. This is because the
				// Accountant is keeping track of limits based
				// on the matches that are being handed out.
				//
				// A future optimization here may be to allow
				// jobs with a subset of the limits given to
				// the current match to reuse it.

			std::string jobLimits, recordedLimits;
			if (param_boolean("CLAIM_RECYCLING_CONSIDER_LIMITS", true)) {
				ad->LookupString(ATTR_CONCURRENCY_LIMITS, jobLimits);
				my_match_ad->LookupString(ATTR_MATCHED_CONCURRENCY_LIMITS,
										  recordedLimits);
				lower_case(jobLimits);
				lower_case(recordedLimits);

				if (jobLimits == recordedLimits) {
					dprintf(D_FULLDEBUG,
							"ConcurrencyLimits match, can reuse claim\n");
				} else {
					dprintf(D_FULLDEBUG,
							"ConcurrencyLimits do not match, cannot "
							"reuse claim\n");
					PrioRecAutoClusterRejected->
						insert(p->auto_cluster_id, 1);
					continue;
				}
			}

			jobid = p->id; // success!
			return;

		}	// end of for loop through PrioRec array

		if(rebuilt_prio_rec_array) {
				// We found nothing, and we had a freshly built job list.
				// Give up.
			break;
		}
			// Try to force a rebuild of the job list, since we
			// are about to throw away a match.
		rebuilt_prio_rec_array = BuildPrioRecArray(true /*no match found*/);

	} while( rebuilt_prio_rec_array );

	// no more jobs to run anywhere.  nothing more to do.  failure.
}

bool Runnable(JobQueueJob *job, const char *& reason)
{
	int cur = 0, max = 1;

	if ( ! job || ! job->IsJob())
	{
		reason = "not runnable (not found)";
		return false;
	}

	if (job->IsNoopJob())
	{
		reason = "not runnable (IsNoopJob)";
		return false;
	}

	if (job->Status() != IDLE) {
		reason = "not runnanble (not IDLE)";
		return false;
	}

	if( !service_this_universe(job->Universe(), job) )
	{
		reason = "not runnable (universe not in service)";
		return false;
	}

	time_t cool_down = 0;
	job->LookupInteger(ATTR_JOB_COOL_DOWN_EXPIRATION, cool_down);
	if (cool_down >= time(nullptr)) {
		reason = "not runnable (in cool-down)";
		return false;
	}

	job->LookupInteger(ATTR_CURRENT_HOSTS, cur);
	job->LookupInteger(ATTR_MAX_HOSTS, max);

	if (cur < max)
	{
		reason = "is runnable";
		return true;
	}
	
	reason = "not runnable (already matched)";
	return false;
}

bool Runnable(PROC_ID* id)
{
	const char * reason = "";
	bool runnable = Runnable(GetJobAd(id->cluster,id->proc), reason);
	dprintf (D_FULLDEBUG, "Job %d.%d: %s\n", id->cluster, id->proc, reason);
	return runnable;
}

void
dirtyJobQueue()
{
	JobQueueDirty = true;
}

int GetJobQueuedCount() {
    return job_queued_count;
}


/**********************************************************************
 * These qmgt function support JobSets - see jobsets.cpp       
*/

bool 
JobSetDestroy(int setid)
{
	if (setid <= 0) { return false; }

	JobQueueKeyBuf key;
	IdToKey(JOBSETID_to_qkey1(setid), JOBSETID_qkey2, key);

	return JobQueue->DestroyClassAd(key);
}

bool JobSetCreate(int setId, const char * setName, const char * ownerinfoName)
{
	if (setId <= 0 || setId >= INT_MAX) { return false; }
	JobQueueKeyBuf key;
	IdToKey(JOBSETID_to_qkey1(setId), JOBSETID_qkey2, key);

	bool already_in_transaction = InTransaction();
	if (!already_in_transaction) {
		BeginTransaction();
	}

	std::string ownbuf;
	const char * owner = ownerinfoName;
	const char * user = nullptr;
	if (USERREC_NAME_IS_FULLY_QUALIFIED) {
		owner = name_of_user(ownerinfoName, ownbuf);
		user = ownerinfoName;
	}

	bool rval = JobQueue->NewClassAd(key, JOB_SET_ADTYPE) &&
		0 == SetSecureAttributeInt(key.cluster, key.proc, ATTR_JOB_SET_ID, setId) &&
		0 == SetSecureAttributeString(key.cluster, key.proc, ATTR_JOB_SET_NAME, setName) &&
		0 == SetSecureAttributeString(key.cluster, key.proc, ATTR_OWNER, owner) &&
		( ! user || 0 == SetSecureAttributeString(key.cluster, key.proc, ATTR_USER, ownerinfoName))
		;

	// make sure that the post transaction jobset handling code runs
	JobQueue->SetTransactionTriggers(catJobset);

	if ( ! already_in_transaction) {
		if ( ! rval) {
			AbortTransaction();
		} else {
			CommitNonDurableTransactionOrDieTrying();
		}
	}

	return rval;
}

