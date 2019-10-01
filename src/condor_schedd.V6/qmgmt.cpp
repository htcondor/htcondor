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
#include <param_info.h>

#if defined(HAVE_DLOPEN) || defined(WIN32)
#include "ScheddPlugin.h"
#endif

#if defined(HAVE_GETGRNAM)
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
	bool boolVal;
	int intVal;
	int miss_count = 0;
	Stopwatch sw;
	sw.start();
	while (!(m_cur == end))
	{
		miss_count++;
			// 500 was chosen here based on a queue of 1M jobs and
			// estimated 30ns per clock_gettime call - resulting in
			// an overhead of 0.06 ms from the timing calls to iterate
			// through a whole queue.  Compared to the cost of doing
			// the rest of the iteration (6ms per 10k ads, or 600ms)
			// for the whole queue, I consider this overhead
			// acceptable.  BB, 09/2014.
		if ((miss_count % 500 == 0) && (sw.get_ms() > m_timeslice_ms)) {break;}

		cur = *this;
		AD tmp_ad = (*m_cur++).second;
		if (!tmp_ad) continue;

		// we want to ignore all but job ads, unless the options flag indicates we should
		// also iterate cluster ads, in any case we always want to skip the header ad.
		if ( ! tmp_ad->IsJob()) {
			if ( ! (m_options & JOB_QUEUE_ITERATOR_OPT_INCLUDE_CLUSTERS) || ! tmp_ad->IsCluster()) continue;
		}

		if (m_requirements) {
			classad::ExprTree &requirements = *const_cast<classad::ExprTree*>(m_requirements);
			const classad::ClassAd *old_scope = requirements.GetParentScope();
			requirements.SetParentScope( tmp_ad );
			classad::Value result;
			int retval = requirements.Evaluate(result);
			requirements.SetParentScope(old_scope);
			if (!retval) {
				dprintf(D_FULLDEBUG, "Unable to evaluate ad.\n");
				continue;
			}

			if (!(result.IsBooleanValue(boolVal) && boolVal) &&
					!(result.IsIntegerValue(intVal) && intVal)) {
				continue;
			}
		}
		//int tmp_int;
		//if (!tmp_ad->EvaluateAttrInt(ATTR_CLUSTER_ID, tmp_int) || !tmp_ad->EvaluateAttrInt(ATTR_PROC_ID, tmp_int)) {
		//	continue;
		//}
		cur.m_found_ad = true;
		m_found_ad = true;
		break;
	}
	if ((m_cur == end) && (!m_found_ad)) {
		m_done = true;
	}
	return cur;
}

// force instantiation of the template types needed by the JobQueue
typedef GenericClassAdCollection<JobQueueKey, JobQueuePayload> JobQueueType;
template class ClassAdLog<JobQueueKey,JobQueuePayload>;

extern char *Spool;
extern char *Name;
extern Scheduler scheduler;
extern DedicatedScheduler dedicated_scheduler;

extern "C" {
	int	prio_compar(prio_rec*, prio_rec*);
}

extern  void    cleanup_ckpt_files(int, int, const char*);
extern	bool	service_this_universe(int, ClassAd *);
extern	bool	jobExternallyManaged(ClassAd * ad);
static QmgmtPeer *Q_SOCK = NULL;

// Hash table with an entry for every job owner that
// has existed in the queue since this schedd has been
// running.  Used by SuperUserAllowedToSetOwnerTo().
static HashTable<MyString,int> owner_history(hashFunction);

int		do_Q_request(ReliSock *,bool &may_fork);
#if 0 // not used?
void	FindPrioJob(PROC_ID &);
#endif
void	DoSetAttributeCallbacks(const std::set<std::string> &jobids, int triggers);
int		MaterializeJobs(JobQueueCluster * clusterAd, TransactionWatcher & txn, int & retry_delay);

static bool qmgmt_was_initialized = false;
static JobQueueType *JobQueue = 0;
static StringList DirtyJobIDs;
static std::set<int> DirtyPidsSignaled;
static int next_cluster_num = -1;
// If we ever allow more than one concurrent transaction, and next_proc_num
// being a global, we'll need to make the same change to
// jobs_added_this_transaction.
static int next_proc_num = 0;
static int jobs_added_this_transaction = 0;
int active_cluster_num = -1;	// client is restricted to only insert jobs to the active cluster
static bool JobQueueDirty = false;
static int in_walk_job_queue = 0;
static time_t xact_start_time = 0;	// time at which the current transaction was started
static int cluster_initial_val = 1;		// first cluster number to use
static int cluster_increment_val = 1;	// increment for cluster numbers of successive submissions 
static int cluster_maximum_val = 0;     // maximum cluster id (default is 0, or 'no max')
static int job_queued_count = 0;
static Regex *queue_super_user_may_impersonate_regex = NULL;

static void AddOwnerHistory(const MyString &user);

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
HashTable<int,int> *PrioRecAutoClusterRejected = NULL;
int BuildPrioRecArrayTid = -1;

static int 	MAX_PRIO_REC=INITIAL_MAX_PRIO_REC ;	// INITIAL_MAX_* in prio_rec.h

JOB_ID_KEY_BUF HeaderKey(0,0);

ForkWork schedd_forker;

// Create a hash table which, given a cluster id, tells how
// many procs are in the cluster
typedef HashTable<int, int> ClusterSizeHashTable_t;
static ClusterSizeHashTable_t *ClusterSizeHashTable = 0;
static int TotalJobsCount = 0;
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
static void HandleFlushJobQueueLogTimer();
static int dirty_notice_interval = 0;
static void PeriodicDirtyAttributeNotification();
static void ScheduleJobQueueLogFlush();

bool qmgmt_all_users_trusted = false;
static char	**super_users = NULL;
static int	num_super_users = 0;
static const char *default_super_user =
#if defined(WIN32)
	"Administrator";
#else
	"root";
#endif

// in schedd.cpp
void IncrementLiveJobCounter(LiveJobCounters & num, int universe, int status, int increment /*, JobQueueJob * job*/);

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

static inline JobQueueKey& IdToKey(int cluster, int proc, JobQueueKey& key)
{
	key.cluster = cluster;
	key.proc = proc;
	return key;
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

static
void
KeyToId(JobQueueKey &key,int & cluster,int & proc)
{
	cluster = key.cluster;
	proc = key.proc;
}

ClassAd* ConstructClassAdLogTableEntry<JobQueueJob*>::New(const char * key, const char * /* mytype */) const
{
	JOB_ID_KEY jid(key);
	if (jid.cluster > 0 && jid.proc < 0) {
		return new JobQueueCluster(jid);
	} else {
		return new JobQueueJob();
	}
}


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
	job->parent = NULL;
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
		JobQueueJob * job = q->as<JobQueueJob>();
		job->Unchain();
		job->parent = NULL;
		--num_attached;
		if (num_attached < -100) break; // something is seriously wrong here...
	}
	if (num_attached != 0) {
		dprintf(D_ALWAYS, "ERROR - Cluster %d has num_attached=%d after detaching all jobs\n", jid.cluster, num_attached);
	}
}

// This is where we can clean up any data structures that refer to the job object
void
ConstructClassAdLogTableEntry<JobQueueJob*>::Delete(ClassAd* &ad) const
{
	if ( ! ad) return;
	JobQueueJob * job = (JobQueueJob*)ad;
	if (job->jid.cluster > 0) {
		if (job->jid.proc < 0) {
			// this is a cluster, detach all jobs
			JobQueueCluster * clusterad = static_cast<JobQueueCluster*>(job);
			if (clusterad->HasAttachedJobs()) {
				dprintf(D_ALWAYS, "WARNING - Cluster %d was deleted with proc ads still attached to it. This should only happen during schedd shutdown.\n", clusterad->jid.cluster);
				clusterad->DetachAllJobs();
			}
		} else {
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
	}
	delete job;
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
	}
	else {
		dprintf(D_ALWAYS, "ERROR: ClusterCleanup() could not find ad for"
				" cluster ID %d\n", cluster_id);
	}

	// pull out the owner and hash used for ickpt sharing
	MyString hash, owner, digest;
	GetAttributeString(cluster_id, -1, ATTR_JOB_CMD_HASH, hash);
	if ( ! hash.empty()) {
		GetAttributeString(cluster_id, -1, ATTR_OWNER, owner);
	}
	const char * submit_digest = NULL;
	if (GetAttributeString(cluster_id, -1, ATTR_JOB_MATERIALIZE_DIGEST_FILE, digest) >= 0 && ! digest.empty()) {
		submit_digest = digest.c_str();
	}

	// remove entry in ClusterSizeHashTable 
	ClusterSizeHashTable->remove(cluster_id);

	// delete the cluster classad
	JobQueue->DestroyClassAd( key );

	SpooledJobFiles::removeClusterSpooledFiles(cluster_id, submit_digest);

	// garbage collect the shared ickpt file if necessary
	if (!hash.IsEmpty()) {
		ickpt_share_try_removal(owner.Value(), hash.Value());
	}
}

int GetSchedulerCapabilities(int /*mask*/, ClassAd & reply)
{
	//reply.Assign("CondorVersion", );
	reply.Assign( "LateMaterialize", scheduler.getAllowLateMaterialize() );
	reply.Assign("LateMaterializeVersion", 2);
	dprintf(D_ALWAYS, "GetSchedulerCapabilities called, returning\n");
	dPrintAd(D_ALWAYS, reply, false);
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
JobMaterializeTimerCallback()
{
	dprintf(D_MATERIALIZE | D_VERBOSE, "in JobMaterializeTimerCallback\n");

	bool allow_materialize = scheduler.getAllowLateMaterialize();
	if ( ! allow_materialize || ! JobQueue) {
		// materialization is disabled, so just clear the list and quit.
		ClustersNeedingMaterialize.clear();
	} else {

		int system_limit = MIN(scheduler.getMaxMaterializedJobsPerCluster(), scheduler.getMaxJobsPerSubmission());
		system_limit = MIN(system_limit, scheduler.getMaxJobsPerOwner());
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

					dprintf(D_MATERIALIZE | D_VERBOSE, "in JobMaterializeTimerCallback, proc_limit=%d, sys_limit=%d MIN(%d,%d,%d), ClusterSize=%d\n",
						proc_limit, system_limit,
						scheduler.getMaxMaterializedJobsPerCluster(), scheduler.getMaxJobsPerSubmission(), scheduler.getMaxJobsPerOwner(),
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
DeferredClusterCleanupTimerCallback()
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
		int *numOfProcs = NULL;
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
	int 	*numOfProcs = NULL;

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
	int 	*numOfProcs = NULL;

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
			numOfProcs = NULL;
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

static 
void
RemoveMatchedAd(int cluster_id, int proc_id)
{
	if ( scheduler.resourcesByProcID ) {
		ClassAd *ad_to_remove = NULL;
		PROC_ID job_id;
		job_id.cluster = cluster_id;
		job_id.proc = proc_id;
		scheduler.resourcesByProcID->lookup(job_id,ad_to_remove);
		if ( ad_to_remove ) {
			delete ad_to_remove;
			scheduler.resourcesByProcID->remove(job_id);
		}
	}
	return;
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
	int universe, cluster, proc;

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
	if ( job_status == HELD && job_ad->LookupExpr( ATTR_HOLD_REASON_CODE ) == NULL ) {
		std::string hold_reason;
		job_ad->LookupString( ATTR_HOLD_REASON, hold_reason );
		if ( hold_reason == "Spooling input data files" ) {
			job_ad->Assign( ATTR_HOLD_REASON_CODE,
							CONDOR_HOLD_CODE_SpoolingInput );
		}
	}

		// CRUFT
		// Starting in 6.7.11, ATTR_JOB_MANAGED changed from a boolean
		// to a string.
	bool ntmp;
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

QmgmtPeer::QmgmtPeer()
{
	owner = NULL;
	fquser = NULL;
	myendpoint = NULL;
	sock = NULL;
	transaction = NULL;
	allow_protected_attr_changes_by_superuser = true;

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
	ASSERT(owner == NULL);

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
	myendpoint = strdup(addr.to_ip_string().Value());

	return true;
}

bool
QmgmtPeer::setAllowProtectedAttrChanges(bool val)
{
	bool old_val = allow_protected_attr_changes_by_superuser;

	allow_protected_attr_changes_by_superuser = val;

	return old_val;
}

bool
QmgmtPeer::setEffectiveOwner(char const *o)
{
	free(owner);
	owner = NULL;

	if ( o ) {
		owner = strdup(o);
	}
	return true;
}

void
QmgmtPeer::unset()
{
	if (owner) {
		free(owner);
		owner = NULL;
	}
	if (fquser) {
		free(fquser);
		fquser = NULL;
	}
	if (myendpoint) {
		free(myendpoint);
		myendpoint = NULL;
	}
	if (sock) sock=NULL;	// note: do NOT delete sock!!!!!
	if (transaction) {
		delete transaction;
		transaction = NULL;
	}

	next_proc_num = 0;
	active_cluster_num = -1;	
	xact_start_time = 0;	// time at which the current transaction was started
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


const char*
QmgmtPeer::getOwner() const
{
	// if effective owner has been set, use that
	if( owner ) {
		return owner;
	}
	if ( sock ) {
		return sock->getOwner();
	}
	return NULL;
}

const char*
QmgmtPeer::getDomain() const
{
	if ( sock ) {
		return sock->getDomain();
	}
	return NULL;
}

const char*
QmgmtPeer::getRealOwner() const
{
	if ( sock ) {
		return sock->getOwner();
	}
	else {
		return owner;
	}
}
	

const char*
QmgmtPeer::getFullyQualifiedUser() const
{
	if ( sock ) {
		return sock->getFullyQualifiedUser();
	} else {
		return fquser;
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
	StringList s_users;
	char* tmp;
	int i;

	qmgmt_was_initialized = true;

	if( super_users ) {
		for( i=0; i<num_super_users; i++ ) {
			delete [] super_users[i];
		}
		delete [] super_users;
	}
	tmp = param( "QUEUE_SUPER_USERS" );
	if( tmp ) {
		s_users.initializeFromString( tmp );
		free( tmp );
	} else {
		s_users.initializeFromString( default_super_user );
	}
	if( ! s_users.contains(get_condor_username()) ) {
		s_users.append( get_condor_username() );
	}
	num_super_users = s_users.number();
	super_users = new char* [ num_super_users ];
	s_users.rewind();
	i = 0;
	while( (tmp = s_users.next()) ) {
		super_users[i] = new char[ strlen( tmp ) + 1 ];
		strcpy( super_users[i], tmp );
		i++;
	}

	if( IsFulldebug(D_FULLDEBUG) ) {
		dprintf( D_FULLDEBUG, "Queue Management Super Users:\n" );
		for( i=0; i<num_super_users; i++ ) {
			dprintf( D_FULLDEBUG, "\t%s\n", super_users[i] );
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
	queue_super_user_may_impersonate_regex = NULL;
	std::string queue_super_user_may_impersonate;
	if( param(queue_super_user_may_impersonate,"QUEUE_SUPER_USER_MAY_IMPERSONATE") ) {
		queue_super_user_may_impersonate_regex = new Regex;
		const char *errptr=NULL;
		int erroffset=0;
		if( !queue_super_user_may_impersonate_regex->compile(queue_super_user_may_impersonate.c_str(),&errptr,&erroffset) ) {
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

	int a;
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
		StringList old_paths(v.c_str(),",");
		StringList new_paths(NULL,",");
		bool changed = false;

		old_paths.rewind();
		char const *op;
		while( (op=old_paths.next()) ) {
			if( !strncmp(op,o,strlen(o)) ) {
				std::string np = n;
				np += op + strlen(o);
				new_paths.append(np.c_str());
				changed = true;
			}
			else {
				new_paths.append(op);
			}
		}

		if( changed ) {
			char *nv = new_paths.print_to_string();
			ASSERT( nv );
			dprintf(D_ALWAYS,"Changing job %d.%d %s from %s to %s\n",
					cluster, proc, attr, v.c_str(), nv);
			job_ad->Assign(attr,nv);
			free( nv );
		}
	}
}


static void
SpoolHierarchyChangePass1(char const *spool,std::list< PROC_ID > &spool_rename_list)
{
	int cluster, proc, subproc;

	Directory spool_dir(spool);
	const char *f;
	while( (f=spool_dir.Next()) ) {
		int len;
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
	int cluster, proc;

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
		this->factory = NULL;
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


// After the job queue is loaded from disk, or a new job is submitted
// the fields in the job object have to be initialized to match the ad
//
void JobQueueJob::PopulateFromAd()
{
	if ( ! entry_type) {
		if ( ! jid.cluster) {
			this->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
			this->LookupInteger(ATTR_PROC_ID, jid.proc);
		}
		if (jid.cluster == 0 && jid.proc == 0) entry_type = entry_type_header;
		else if (jid.cluster > 0) entry_type = (jid.proc < 0) ? entry_type_cluster : entry_type_job;
	}

	if ( ! universe) {
		int uni;
		if (this->LookupInteger(ATTR_JOB_UNIVERSE, uni)) {
			this->universe = uni;
		}
	}


#if 0	// we don't do this anymore, since we update both the ad and the job object
		// when we calculate the autocluster - the on-disk value is never useful.
	if (autocluster_id < 0) {
		this->LookupInteger(ATTR_AUTO_CLUSTER_ID, this->autocluster_id);
	}
#endif
}

void
InitJobQueue(const char *job_queue_name,int max_historical_logs)
{
	ASSERT(qmgmt_was_initialized);	// make certain our parameters are setup
	ASSERT(!JobQueue);

	MyString spool;
	if( !param(spool,"SPOOL") ) {
		EXCEPT("SPOOL must be defined.");
	}

	int spool_min_version = 0;
	int spool_cur_version = 0;
	CheckSpoolVersion(spool.Value(),SPOOL_MIN_VERSION_SCHEDD_SUPPORTS,SPOOL_CUR_VERSION_SCHEDD_SUPPORTS,spool_min_version,spool_cur_version);

	JobQueue = new JobQueueType(new ConstructClassAdLogTableEntry<JobQueuePayload>(),job_queue_name,max_historical_logs);
	ClusterSizeHashTable = new ClusterSizeHashTable_t(hashFuncInt);
	TotalJobsCount = 0;
	jobs_added_this_transaction = 0;


	/* We read/initialize the header ad in the job queue here.  Currently,
	   this header ad just stores the next available cluster number. */
	JobQueueJob *ad = NULL;
	JobQueueCluster *clusterad = NULL;
	JobQueueKey key;
	int 	cluster_num, cluster, proc, universe;
	int		stored_cluster_num;
	bool	CreatedAd = false;
	JobQueueKey cluster_key;
	std::string	owner;
	std::string	user;
	std::string correct_user;
	MyString	buf;
	std::string	attr_scheduler;
	std::string correct_scheduler;
	std::string buffer;

	if (!JobQueue->Lookup(HeaderKey, ad)) {
		// we failed to find header ad, so create one
		JobQueue->NewClassAd(HeaderKey, JOB_ADTYPE, STARTD_ADTYPE);
		CreatedAd = true;
	}

	if (CreatedAd ||
		ad->LookupInteger(ATTR_NEXT_CLUSTER_NUM, stored_cluster_num) != 1) {
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

	next_cluster_num = cluster_initial_val;
	JobQueue->StartIterateAllClassAds();
	while (JobQueue->Iterate(key,ad)) {
		ad->jid = key; // make sure that job object has correct jobid.
		if (key.cluster <= 0 || key.proc < 0 ) {
			if (key.cluster >= 0) {
				if ( ! ad->ownerinfo) {
					if (ad->LookupString(ATTR_OWNER, owner)) {
						AddOwnerHistory( owner );
						ad->ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(owner.c_str()));
					}
				}
				ad->PopulateFromAd();
			}
			continue;  // done with cluster & header ads
		}

		cluster_num = key.cluster;

		// this brace isn't needed anymore, it's here to avoid re-indenting all of the code below.
		{
			JOB_ID_KEY_BUF job_id(key);

			// find highest cluster, set next_cluster_num to one increment higher
			if (cluster_num >= next_cluster_num) {
				next_cluster_num = cluster_num + cluster_increment_val;
			}

			// link all proc ads to their cluster ad, if there is one
#if 1
			clusterad = GetClusterAd(cluster_num);
			ad->ChainToAd(clusterad);
#else
			IdToKey(cluster_num,-1,cluster_key);
			if ( JobQueue->Lookup(cluster_key,clusterad) ) {
				ad->ChainToAd(clusterad);
			}
#endif

			if (!ad->LookupString(ATTR_OWNER, owner)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						job_id.c_str(), ATTR_OWNER);
				JobQueue->DestroyClassAd(job_id);
				continue;
			}

				// initialize our list of job owners
			AddOwnerHistory( owner );
			ad->ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(owner.c_str()));
			if (clusterad)
				clusterad->ownerinfo = ad->ownerinfo;

			if (!ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						job_id.c_str(), ATTR_CLUSTER_ID);
				JobQueue->DestroyClassAd(job_id);
				continue;
			}

			if (cluster != cluster_num) {
				dprintf(D_ALWAYS,
						"Job %s has invalid cluster number %d.  Removing...\n",
						job_id.c_str(), cluster);
				JobQueue->DestroyClassAd(job_id);
				continue;
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

				// Update fields in the newly created JobObject
			ad->autocluster_id = -1;
			ad->Delete(ATTR_AUTO_CLUSTER_ID);
			ad->SetUniverse(universe);
			if (clusterad) {
				clusterad->AttachJob(ad);
				clusterad->SetUniverse(universe);
				clusterad->autocluster_id = -1;
			}
			ad->PopulateFromAd();

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

				// Figure out what ATTR_USER *should* be for this job
			int nice_user = 0;
			ad->LookupInteger( ATTR_NICE_USER, nice_user );
			formatstr( correct_user, "%s%s@%s",
					 (nice_user) ? "nice-user." : "", owner.c_str(),
					 scheduler.uidDomain() );

			if (!ad->LookupString(ATTR_USER, user)) {
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
			if ( job_status == HELD && hold_code == CONDOR_HOLD_CODE_SpoolingInput ) {
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
			// AsyncXfer: Delete in-job output transfer attributes
			if( ad->LookupInteger(ATTR_JOB_TRANSFERRING_OUTPUT,transferring_output) ) {
				ad->Delete(ATTR_JOB_TRANSFERRING_OUTPUT);
				JobQueueDirty = true;
			}
			if( ad->LookupInteger(ATTR_JOB_TRANSFERRING_OUTPUT_TIME,transferring_output) ) {
				ad->Delete(ATTR_JOB_TRANSFERRING_OUTPUT_TIME);
				JobQueueDirty = true;
			}

			// count up number of procs in cluster, update ClusterSizeHashTable
			int num_procs = IncrementClusterSize(cluster_num);
			if (clusterad) clusterad->SetClusterSize(num_procs);
			TotalJobsCount++;
		}
	} // WHILE

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
		SpoolHierarchyChangePass1(spool.Value(),spool_rename_list);
	}


		// Some of the conversions done in ConvertOldJobAdAttrs need to be
		// persisted to disk. Particularly, GlobusContactString/RemoteJobId.
		// The spool renaming also needs to be saved here.  This is not
		// optional, so we cannot just call CleanJobQueue() here, because
		// that does not abort on failure.
	if( JobQueueDirty ) {
		if( !JobQueue->TruncLog() ) {
			EXCEPT("Failed to write the modified job queue log to disk, so cannot continue.");
		}
		JobQueueDirty = false;
	}

	if( spool_cur_version < 1 ) {
		SpoolHierarchyChangePass2(spool.Value(),spool_rename_list);
	}

	if( spool_cur_version != SPOOL_CUR_VERSION_SCHEDD_SUPPORTS ) {
		WriteSpoolVersion(spool.Value(),SPOOL_MIN_VERSION_SCHEDD_WRITES,SPOOL_CUR_VERSION_SCHEDD_SUPPORTS);
	}
}


void
CleanJobQueue()
{
	if (JobQueueDirty) {
		dprintf(D_ALWAYS, "Cleaning job queue...\n");
		JobQueue->TruncLog();
		JobQueueDirty = false;
	}
}


void
DestroyJobQueue( void )
{
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
	JobQueue = NULL;

	DirtyJobIDs.clearAll();

		// There's also our hashtable of the size of each cluster
	delete ClusterSizeHashTable;
	ClusterSizeHashTable = NULL;
	TotalJobsCount = 0;

		// Also, clean up the array of super users
	if( super_users ) {
		int i;
		for( i=0; i<num_super_users; i++ ) {
			delete [] super_users[i];
		}
		delete [] super_users;
	}

	delete queue_super_user_may_impersonate_regex;
	queue_super_user_may_impersonate_regex = NULL;
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
	system_limit = MIN(system_limit, scheduler.getMaxJobsPerOwner());
	//system_limit = MIN(system_limit, scheduler.getMaxJobsRunning());

	int effective_limit = MIN(proc_limit, system_limit);

	dprintf(D_MATERIALIZE | D_VERBOSE, "in MaterializeJobs, proc_limit=%d, sys_limit=%d MIN(%d,%d,%d), ClusterSize=%d\n",
		proc_limit, system_limit,
		scheduler.getMaxMaterializedJobsPerCluster(), scheduler.getMaxJobsPerSubmission(), scheduler.getMaxJobsPerOwner(),
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
int QmgmtHandleSendMaterializeData(int cluster_id, ReliSock * sock, MyString & spooled_filename, int &row_count, int &terrno)
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

	JobFactory * factory = NULL;
	JobFactory*& pending = JobFactoriesSubmitPending[cluster_id];
	if (pending) {
		factory = pending;
	} else {
		// parse the submit digest and (possibly) open the itemdata file.
		factory = NewJobFactory(cluster_id);
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
	int fd;
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
		DestroyJobFactory(factory); factory = NULL;
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

		ClassAd * user_ident = NULL;
	#if 0 // in 8.7.9 we no longer impersonate the user while loading the digest because the itemdata (if any) will be in SPOOL
		ClassAd user_ident_ad;
		if (Q_SOCK) {
			// build a ad with the user identity so we can use set_user_priv_from_ad
			// here just like we would if the cluster ad was available.
			user_ident_ad.Assign(ATTR_OWNER, Q_SOCK->getOwner());
			user_ident_ad.Assign(ATTR_NT_DOMAIN, Q_SOCK->getDomain());
			user_ident = &user_ident_ad;
		}
	#endif

		JobFactory * factory = NULL;
		JobFactory*& pending = JobFactoriesSubmitPending[cluster_id];
		if (pending) {
			factory = pending;
		} else {
			// parse the submit digest and (possibly) open the itemdata file.
			factory = NewJobFactory(cluster_id);
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
			MyString spooled_filename;
			GetSpooledSubmitDigestPath(spooled_filename, cluster_id, Spool);
			const char * filename = spooled_filename.c_str();

			int terrno = 0;
			int fd;
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
			if (factory) { DestroyJobFactory(factory); factory = NULL; }
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
	int i;
	prio_rec *tmp;

	// just return if PrioRec already equal/larger than the size requested
	if ( MAX_PRIO_REC >= newsize ) {
		return 0;
	}

	dprintf(D_FULLDEBUG,"Dynamically growing PrioRec to %d\n",newsize);

	tmp = new prio_rec[newsize];
	if ( tmp == NULL ) {
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


bool
isQueueSuperUser( const char* user )
{
	int i;
	if( ! (user && super_users) ) {
		return false;
	}
	for( i=0; i<num_super_users; i++ ) {
#if defined(HAVE_GETGRNAM)
        if (super_users[i][0] == '%') {
            // this is a user group, so check user against the group membership
            struct group* gr = getgrnam(1+super_users[i]);
            if (gr) {
                for (char** gmem=gr->gr_mem;  *gmem != NULL;  ++gmem) {
                    if (strcmp(user, *gmem) == 0) return true;
                }
            } else {
                dprintf(D_SECURITY, "Group name \"%s\" was not found in defined user groups\n", 1+super_users[i]);
            }
            continue;
        }
#endif
#if defined(WIN32) // usernames on Windows are case-insensitive.
		if( strcasecmp( user, super_users[i] ) == 0 ) {
#else
		if( strcmp( user, super_users[i] ) == 0 ) {
#endif
			return true;
		}
	}
	return false;
}

static void
AddOwnerHistory(const MyString &user) {
	owner_history.insert(user,1);
}

static bool
SuperUserAllowedToSetOwnerTo(const MyString &user) {
		// To avoid giving the queue super user (e.g. condor)
		// the ability to run as innocent people who have never
		// even run a job, only allow them to set the owner
		// attribute of a job to a value we have seen before.
		// The JobRouter depends on this when it is running as
		// root/condor.

	if( queue_super_user_may_impersonate_regex ) {
		if( queue_super_user_may_impersonate_regex->match(user.Value()) ) {
			return true;
		}
		dprintf(D_FULLDEBUG,"Queue super user not allowed to set owner to %s, because this does not match the QUEUE_SUPER_USER_MAY_IMPERSONATE regular expression.\n",user.Value());
		return false;
	}

	int junk = 0;
	if( owner_history.lookup(user,junk) != -1 ) {
		return true;
	}
	dprintf(D_FULLDEBUG,"Queue super user not allowed to set owner to %s, because this instance of the schedd has never seen that user submit any jobs.\n",user.Value());
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
	if( o && real_owner && is_same_user(o,real_owner,COMPARE_DOMAIN_DEFAULT) ) {
		if ( ! is_same_user(o,real_owner,COMPARE_DOMAIN_FULL)) {
			dprintf(D_SECURITY, "SetEffectiveOwner security warning: "
					"assuming \"%s\" is the same as active owner \"%s\"\n",
					o, real_owner);
		}
		// change effective owner --> real owner
		o = NULL;
	}

	if( o && !*o ) {
		// treat empty string equivalently to NULL
		o = NULL;
	}

	// always allow request to set effective owner to NULL,
	// because this means set effective owner --> real owner
	if( o && !qmgmt_all_users_trusted ) {
		if( !isQueueSuperUser(real_owner) ||
			!SuperUserAllowedToSetOwnerTo( o ) )
		{
			dprintf(D_ALWAYS, "SetEffectiveOwner security violation: "
					"setting owner to %s when active owner is \"%s\"\n",
					o, real_owner ? real_owner : "(null)" );
			errno = EACCES;
			return -1;
		}
	}

	if( !Q_SOCK->setEffectiveOwner( o ) ) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

// Test if this owner matches my owner, so they're allowed to update me.
bool
OwnerCheck(ClassAd *ad, const char *test_owner)
{
	// check if the IP address of the peer has daemon core write permission
	// to the schedd.  we have to explicitly check here because all queue
	// management commands come in via one sole daemon core command which
	// has just READ permission.
	condor_sockaddr addr = Q_SOCK->endpoint();
	if ( !Q_SOCK->isAuthorizationInBoundingSet("ADMINISTRATOR") &&
		daemonCore->Verify("queue management", WRITE, addr,
		Q_SOCK->getFullyQualifiedUser()) == FALSE )
	{
		// this machine does not have write permission; return failure
		return false;
	}

	return OwnerCheck2(ad, test_owner);
}


bool
OwnerCheck2(ClassAd *ad, const char *test_owner, const char *job_owner)
{
	std::string	owner_buf;

	// in the very rare event that the admin told us all users 
	// can be trusted, let it pass
	if ( qmgmt_all_users_trusted ) {
		return true;
	}

	// If test_owner is NULL, then we have no idea who the user is.  We
	// do not allow anonymous folks to mess around with the queue, so 
	// have OwnerCheck fail.  Note we only call OwnerCheck in the first place
	// if Q_SOCK is not null; if Q_SOCK is null, then the schedd is calling
	// a QMGMT command internally which is allowed.
	if (test_owner == NULL) {
		dprintf(D_ALWAYS,
				"QMGT command failed: anonymous user not permitted\n" );
		return false;
	}

#if !defined(WIN32) 
		// If we're not root or condor, only allow qmgmt writes from
		// the UID we're running as.
	uid_t 	my_uid = get_my_uid();
	if( my_uid != 0 && my_uid != get_real_condor_uid() ) {
		if( strcmp(get_real_username(), test_owner) == MATCH ) {
			dprintf(D_FULLDEBUG, "OwnerCheck success: owner (%s) matches "
					"my username\n", test_owner );
			return true;
		} else if (isQueueSuperUser(test_owner)) {
            dprintf(D_FULLDEBUG, "OwnerCheck retval 1 (success), super_user\n");
            return true;
        } else {
			errno = EACCES;
			dprintf( D_FULLDEBUG, "OwnerCheck: reject owner: %s non-super\n",
					 test_owner );
			dprintf( D_FULLDEBUG, "OwnerCheck: username: %s, test_owner: %s\n",
					 get_real_username(), test_owner );
			return false;
		}
	}
#endif

		// If we don't have an Owner attribute (or classad) and we've 
		// gotten this far, how can we deny service?
	if( !job_owner ) {
		if( !ad ) {
			dprintf(D_FULLDEBUG,"OwnerCheck retval 1 (success),no ad\n");
			return true;
		}
		else if( ad->LookupString(ATTR_OWNER, owner_buf) == 0 ) {
			dprintf(D_FULLDEBUG,"OwnerCheck retval 1 (success),no owner\n");
			return true;
		}
		job_owner = owner_buf.c_str();
	}

		// Finally, compare the owner of the ad with the entity trying
		// to connect to the queue.
#if defined(WIN32)
	// WIN32: user names are case-insensitive
	if (strcasecmp(job_owner, test_owner) == 0) {
#else
	if (strcmp(job_owner, test_owner) == 0) {
#endif
        return true;
    }

    if (isQueueSuperUser(test_owner)) {
        dprintf(D_FULLDEBUG, "OwnerCheck retval 1 (success), super_user\n");
        return true;
    }

    errno = EACCES;
    dprintf(D_FULLDEBUG, "ad owner: %s, queue submit owner: %s\n", job_owner, test_owner );
    return false;
}


QmgmtPeer*
getQmgmtConnectionInfo()
{
	QmgmtPeer* ret_value = NULL;

	// put all qmgmt state back into QmgmtPeer object for safe keeping
	if ( Q_SOCK ) {
		Q_SOCK->next_proc_num = next_proc_num;
		Q_SOCK->active_cluster_num = active_cluster_num;	
		Q_SOCK->xact_start_time = xact_start_time;
			// our call to getActiveTransaction will clear it out
			// from the lower layers after returning the handle to us
		Q_SOCK->transaction  = JobQueue->getActiveTransaction();

		ret_value = Q_SOCK;
		Q_SOCK = NULL;
		unsetQmgmtConnection();
	}

	return ret_value;
}

bool
setQmgmtConnectionInfo(QmgmtPeer *peer)
{
	bool ret_value;

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
	JobQueue->AbortTransaction();	

	ASSERT(Q_SOCK == NULL);

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
		Q_SOCK = NULL;
	}
	unsetQmgmtConnection();

		// setQSock(NULL) == unsetQSock() == unsetQmgmtConnection(), so...
	if ( rsock == NULL ) {
		return true;
	}

	QmgmtPeer* p = new QmgmtPeer;
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
		Q_SOCK = NULL;
	}
	unsetQmgmtConnection();  
}


int
handle_q(Service *, int, Stream *sock)
{
	int	rval;
	bool all_good;

	all_good = setQSock((ReliSock*)sock);

		// if setQSock failed, unset it to purge any old/stale
		// connection that was never cleaned up, and try again.
	if ( !all_good ) {
		unsetQSock();
		all_good = setQSock((ReliSock*)sock);
	}
	if (!all_good && sock) {
		// should never happen
		EXCEPT("handle_q: Unable to setQSock!!");
	}
	ASSERT(Q_SOCK);

	BeginTransaction();

	bool may_fork = false;
	ForkStatus fork_status = FORK_FAILED;
	do {
		/* Probably should wrap a timer around this */
		rval = do_Q_request( Q_SOCK->getReliSock(), may_fork );

		if( may_fork && fork_status == FORK_FAILED ) {
			fork_status = schedd_forker.NewJob();

			if( fork_status == FORK_PARENT ) {
				break;
			}
		}
	} while(rval >= 0);


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

int GetMyProxyPassword (int, int, char **);

int get_myproxy_password_handler(Service * /*service*/, int /*i*/, Stream *socket) {

	//	For debugging
//	DebugFP = stderr;

	int cluster_id = -1;
	int proc_id = -1;
	int result;

	socket->decode();

	result = socket->code(cluster_id);
	if( !result ) {
		dprintf(D_ALWAYS, "get_myproxy_password_handler: Failed to recv cluster_id.\n");
		return -1;
	}

	result = socket->code(proc_id);
	if( !result ) {
		dprintf(D_ALWAYS, "get_myproxy_password_handler: Failed to recv proc_id.\n");
		return -1;
	}

	char pwd[] = "";
	char * password = pwd;

	if (GetMyProxyPassword (cluster_id, proc_id, &password) != 0) {
		// Try not specifying a proc
		if (GetMyProxyPassword (cluster_id, 0, &password) != 0) {
			//return -1;
			// Just return empty string if can't find password
		}
	}


	socket->end_of_message();
	socket->encode();
	if( ! socket->code(password) ) {
		dprintf( D_ALWAYS,
			"get_myproxy_password_handler: Failed to send result.\n" );
		return -1;
	}

	if( ! socket->end_of_message() ) {
		dprintf( D_ALWAYS,
			"get_myproxy_password_handler: Failed to send end of message.\n");
		return -1;
	}


	return 0;

}


int
InitializeConnection( const char *  /*owner*/, const char *  /*domain*/ )
{
		/*
		  This function used to call init_user_ids(), but we don't
		  need that anymore.  perhaps the whole thing should go
		  away...  however, when i tried that, i got all sorts of
		  strange link errors b/c other parts of the qmgmt code (the
		  sender stubs, etc) seem to depend on it.  i don't have time
		  to do more a thorough purging of it.
		*/
	return 0;
}


int
NewCluster()
{

	if( Q_SOCK && !OwnerCheck(NULL, Q_SOCK->getOwner()  ) ) {
		dprintf( D_FULLDEBUG, "NewCluser(): OwnerCheck failed\n" );
		errno = EACCES;
		return -1;
	}

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
		errno = EINVAL;
		return -2;
	}

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
    ClassAd* test_cluster_ad;
    IdToKey(active_cluster_num,-1,test_cluster_key);
    if (JobQueue->LookupClassAd(test_cluster_key, test_cluster_ad)) {
        dprintf(D_ALWAYS, "NewCluster(): collision with existing cluster id %d\n", active_cluster_num);
        errno = EINVAL;
        return -3;
    }

	char cluster_str[PROC_ID_STR_BUFLEN];
	snprintf(cluster_str, sizeof(cluster_str), "%d", next_cluster_num);
	JobQueue->SetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);

	// put a new classad in the transaction log to serve as the cluster ad
	JobQueueKeyBuf cluster_key;
	IdToKey(active_cluster_num,-1,cluster_key);
	JobQueue->NewClassAd(cluster_key, JOB_ADTYPE, STARTD_ADTYPE);

	return active_cluster_num;
}

// this function is called by external submitters.
// it enforces procs can only be created in the current cluster
// and establishes the owner attribute of the cluster
//
int
NewProc(int cluster_id)
{
	int				proc_id;

	if( Q_SOCK && !OwnerCheck(NULL, Q_SOCK->getOwner() ) ) {
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
		return -2;
	}


	// It is a security violation to change ATTR_OWNER to something other
	// than Q_SOCK->getOwner(), so as long condor_submit and Q_SOCK->getOwner()
	// agree on who the owner is (or until we change the schedd so that /it/
	// sets the Owner string), it's safe to do this rather than complicate
	// things by using the owner attribute from the job ad we don't have yet.
	//
	// The queue super-user(s) can pretend to be someone, in which case it's
	// valid and sensible to apply that someone's quota; or they could change
	// the owner attribute after the submission.  The latter would allow them
	// to bypass the job quota, but that's not necessarily a bad thing.
	if (Q_SOCK) {
		const char * owner = Q_SOCK->getOwner();
		if( owner == NULL ) {
			// This should only happen for job submission via SOAP, but
			// it's unclear how we can verify that.  Regardless, if we
			// don't know who the owner of the job is, we can't enfore
			// MAX_JOBS_PER_OWNER.
			dprintf( D_FULLDEBUG, "Not enforcing MAX_JOBS_PER_OWNER for submit without owner of cluster %d.\n", cluster_id );
		} else {
			const OwnerInfo * ownerInfo = scheduler.insert_owner_const( owner );
			ASSERT( ownerInfo != NULL );
			int ownerJobCount = ownerInfo->num.JobsCounted
								+ ownerInfo->num.JobsRecentlyAdded
								+ jobs_added_this_transaction;

			int maxJobsPerOwner = scheduler.getMaxJobsPerOwner();
			if( ownerJobCount >= maxJobsPerOwner ) {
				dprintf( D_ALWAYS,
					"NewProc(): MAX_JOBS_PER_OWNER exceeded, submit failed.  "
					"Current total is %d.  Limit is %d.\n",
					ownerJobCount, maxJobsPerOwner );
				errno = EINVAL;
				return -3;
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
		return -4;
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
	JobQueue->NewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);

	job_queued_count += 1;

	// can't increment the JobsSubmitted count for other pools yet
	scheduler.OtherPoolStats.DeferJobsSubmitted(cluster_id, proc_id);

		// now that we have a real job ad with a valid proc id, then
		// also insert the appropriate GlobalJobId while we're at it.
	MyString gjid = "\"";
	gjid += Name;             // schedd's name
	gjid += "#";
	gjid += IntToStr( cluster_id );
	gjid += ".";
	gjid += IntToStr( proc_id );
	if (param_boolean("GLOBAL_JOB_ID_WITH_TIME", true)) {
		int now = (int)time(0);
		gjid += "#";
		gjid += IntToStr( now );
	}
	gjid += "\"";
	JobQueue->SetAttribute(key, ATTR_GLOBAL_JOB_ID, gjid.Value());

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
	FILL(ATTR_JOB_STATUS,         1),  // forced into proc ad
	FILL(ATTR_JOB_UNIVERSE,       -1), // forced into cluster ad
	FILL(ATTR_OWNER,              -1), // forced into cluster ad
	FILL(ATTR_PROC_ID,            1),  // forced into proc ad
};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
static int IsForcedProcAttribute(const char *attr)
{
	const ATTR_FORCE_PAIR* found = NULL;
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

	for (auto it = ad->begin(); it != ad->end(); ++it) {
		const std::string & attr = it->first;
		const ExprTree * tree = it->second;
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

int 	DestroyMyProxyPassword (int cluster_id, int proc_id);

int DestroyProc(int cluster_id, int proc_id)
{
	JobQueueKeyBuf		key;
	JobQueueJob			*ad = NULL;

	IdToKey(cluster_id,proc_id,key);
	if (!JobQueue->Lookup(key, ad)) {
		errno = ENOENT;
		return DESTROYPROC_ENOENT;
	}

	// Only the owner can delete a proc.
	if ( Q_SOCK && !OwnerCheck(ad, Q_SOCK->getOwner() )) {
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
			                (int)time(NULL), true /*nondurable*/);
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
		cleanup_ckpt_files(cluster_id,proc_id,NULL );
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

#if defined(HAVE_DLOPEN) || defined(WIN32)
  ScheddPluginManager::Archive(ad);
#endif

  // save job ad to the log
	bool already_in_transaction = InTransaction();
	if( !already_in_transaction ) {
			// For performance, wrap the myproxy attribute change and
			// job deletion into one transaction.
		BeginTransaction();
	}

	// the job ad should have a pointer to the cluster ad, so use that if it has been set.
	// otherwise lookup the cluster ad.
	JobQueueCluster * clusterad = ad->Cluster();
	if ( ! clusterad) {
		clusterad = GetClusterAd(ad->jid);
	}

	// ckireyev: Destroy MyProxyPassword
	(void)DestroyMyProxyPassword (cluster_id, proc_id);

	JobQueue->DestroyClassAd(key);

	DecrementClusterSize(cluster_id, clusterad);

	// We'll need the JobPrio value later after the ad has been destroyed
	int job_prio = 0;
	ad->LookupInteger(ATTR_JOB_PRIO, job_prio);

	int universe = CONDOR_UNIVERSE_STANDARD;
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
		JobQueueJob *otherAd = NULL;
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

		// remove any match (startd) ad stored w/ this job
	RemoveMatchedAd(cluster_id,proc_id);

	JobQueueDirty = true;

	return DESTROYPROC_SUCCESS;
}


int DestroyCluster(int cluster_id, const char* reason)
{
	ClassAd				*ad=NULL;
	int					c, proc_id;
	JobQueueKey			key;

	// cannot destroy the header cluster(s)
	if ( cluster_id < 1 ) {
		errno = EINVAL;
		return -1;
	}

	// find the cluster ad and turn off the job factory
	JobQueueCluster * clusterad = GetClusterAd(cluster_id);
	if (clusterad && clusterad->factory) {
		// Only the owner can delete a cluster
		if ( Q_SOCK && !OwnerCheck(clusterad, Q_SOCK->getOwner() )) {
			errno = EACCES;
			return -1;
		}
		PauseJobFactory(clusterad->factory, mmClusterRemoved);
	}

	JobQueue->StartIterateAllClassAds();

	// Find all jobs in this cluster and remove them.
	while(JobQueue->IterateAllClassAds(ad,key)) {
		KeyToId(key,c,proc_id);
		if (c == cluster_id && proc_id > -1) {
				// Only the owner can delete a cluster
				if ( Q_SOCK && !OwnerCheck(ad, Q_SOCK->getOwner() )) {
					errno = EACCES;
					return -1;
				}

				// Take care of ATTR_COMPLETION_DATE
				int job_status = -1;
				ad->LookupInteger(ATTR_JOB_STATUS, job_status);	
				if ( job_status == COMPLETED ) {
						// if job completed, insert completion time if not already there
					int completion_time = 0;
					ad->LookupInteger(ATTR_COMPLETION_DATE,completion_time);
					if ( !completion_time ) {
						SetAttributeInt(cluster_id,proc_id,
							ATTR_COMPLETION_DATE,(int)time(NULL));
					}
				}

				// Take care of ATTR_REMOVE_REASON
				if( reason ) {
					MyString fixed_reason;
					if( reason[0] == '"' ) {
						fixed_reason += reason;
					} else {
						fixed_reason += '"';
						fixed_reason += reason;
						fixed_reason += '"';
					}
					if( SetAttribute(cluster_id, proc_id, ATTR_REMOVE_REASON, 
									 fixed_reason.Value()) < 0 ) {
						dprintf( D_ALWAYS, "WARNING: Failed to set %s to \"%s\" for "
								 "job %d.%d\n", ATTR_REMOVE_REASON, reason, cluster_id,
								 proc_id );
					}
				}

					// should we leave the job in the queue?
				bool leave_job_in_q = false;
				ad->LookupBool(ATTR_JOB_LEAVE_IN_QUEUE,leave_job_in_q);
				if ( leave_job_in_q ) {
						// leave it in the queue.... move on to the next one
					continue;
				}

				// Apend to history file
				AppendHistory(ad);
#if defined(HAVE_DLOPEN) || defined(WIN32)
				ScheddPluginManager::Archive(ad);
#endif

  // save job ad to the log

				// Write a per-job history file (if PER_JOB_HISTORY_DIR param is set)
				WritePerJobHistoryFile(ad, false);

				cleanup_ckpt_files(cluster_id,proc_id, NULL );

				JobQueue->DestroyClassAd(key);

					// remove any match (startd) ad stored w/ this job
				if ( scheduler.resourcesByProcID ) {
					ClassAd *ad_to_remove = NULL;
					PROC_ID job_id;
					job_id.cluster = cluster_id;
					job_id.proc = proc_id;
					scheduler.resourcesByProcID->lookup(job_id,ad_to_remove);
					if ( ad_to_remove ) {
						delete ad_to_remove;
						scheduler.resourcesByProcID->remove(job_id);
					}
				}
		}

	}

	ClusterCleanup(cluster_id);
	
	// Destroy myproxy password
	DestroyMyProxyPassword (cluster_id, -1);

	JobQueueDirty = true;

	return 0;
}

int
SetAttributeByConstraint(const char *constraint_str, const char *attr_name,
						 const char *attr_value,
						 SetAttributeFlags_t flags)
{
	ClassAd	*ad;
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
	YourString owner;
	MyString owner_expr;
	if (flags & SetAttribute_OnlyMyJobs) {
		owner = Q_SOCK ? Q_SOCK->getOwner() : "unauthenticated";
		if (owner == "unauthenticated") {
			// no job will be owned by "unauthenticated" so just quit now.
			errno = EACCES;
			return -1;
		}
		bool is_super = isQueueSuperUser(owner.Value());
		dprintf(D_COMMAND | D_VERBOSE, "SetAttributeByConstraint w/ OnlyMyJobs owner = \"%s\" (isQueueSuperUser = %d)\n", owner.Value(), is_super);
		if (is_super) {
			// for queue superusers, disable the OnlyMyJobs flag - they get to act on all jobs.
			flags &= ~SetAttribute_OnlyMyJobs;
		} else {
			owner_expr.formatstr("(Owner == \"%s\")", owner.Value());
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
		ExprTree * tree;
		if (0 != ParseClassAdRvalExpr(constraint_str, tree)) {
			dprintf( D_ALWAYS, "can't parse constraint: %s\n", constraint_str );
			errno = EINVAL;
			return -1;
		}
		constraint.set(tree);
	}

	// loop through the job queue, setting attribute on jobs that match
	JobQueue->StartIterateAllClassAds();
	while(JobQueue->IterateAllClassAds(ad,key)) {
		JobQueueJob * job = static_cast<JobQueueJob*>(ad);

		// ignore header and ads.
		if (job->IsHeader())
			continue;

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
	catSpoolingHold = 0x0200,    // hold reason was set to CONDOR_HOLD_CODE_SpoolingInput
	catPostSubmitClusterChange = 0x400, // a cluster ad was changed after submit time which calls for special processing in commit transaction
	catCallbackTrigger = 0x1000, // indicates that a callback should happen on commit of this attribute
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
	FILL(ATTR_JOB_STATUS,         catStatus | catCallbackTrigger),
	FILL(ATTR_JOB_UNIVERSE,       catJobObj),
	FILL(ATTR_NICE_USER,          catSubmitterIdent),
	FILL(ATTR_NUM_JOB_RECONNECTS, 0),
	FILL(ATTR_OWNER,              0),
	FILL(ATTR_PROC_ID,            catJobId),
	FILL(ATTR_RANK,               catTargetScope),
	FILL(ATTR_REQUIREMENTS,       catTargetScope),


};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
static int IsSpecialSetAttribute(const char *attr, int* set_cat=NULL)
{
	const ATTR_IDENT_PAIR* found = NULL;
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
SetSecureAttributeInt(int cluster_id, int proc_id, const char *attr_name, int attr_value, SetAttributeFlags_t flags = 0)
{
	if (attr_name == NULL ) {return -1;}

	char buf[100];
	snprintf(buf,100,"%d",attr_value);

	// lookup job and set attribute
	JOB_ID_KEY_BUF key;
	IdToKey(cluster_id,proc_id,key);
	JobQueue->SetAttribute(key, attr_name, buf, flags & SETDIRTY);

	return 0;
}


int
SetSecureAttributeString(int cluster_id, int proc_id, const char *attr_name, const char *attr_value, SetAttributeFlags_t flags = 0)
{
	if (attr_name == NULL || attr_value == NULL) {return -1;}

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
	JobQueue->SetAttribute(key, attr_name, buf.c_str(), flags & SETDIRTY);

	return 0;
}

int
SetSecureAttribute(int cluster_id, int proc_id, const char *attr_name, const char *attr_value, SetAttributeFlags_t flags = 0)
{
	if (attr_name == NULL || attr_value == NULL) {return -1;}

	// lookup job and set attribute to value
	JOB_ID_KEY_BUF key;
	IdToKey(cluster_id,proc_id,key);
	JobQueue->SetAttribute(key, attr_name, attr_value, flags & SETDIRTY);

	return 0;
}

int
SetAttribute(int cluster_id, int proc_id, const char *attr_name,
			 const char *attr_value, SetAttributeFlags_t flags)
{
	JOB_ID_KEY_BUF key;
	JobQueueJob    *job = NULL;
	MyString		new_value;
	bool query_can_change_only = (flags & SetAttribute_QueryOnly) != 0; // flag for 'just query if we are allowed to change this'

	// Only an authenticated user or the schedd itself can set an attribute.
	if ( Q_SOCK && !(Q_SOCK->isAuthenticated()) ) {
		errno = EACCES;
		return -1;
	}

	// If someone is trying to do something funny with an invalid
	// attribute name, bail out early
	if (!IsValidAttrName(attr_name)) {
		dprintf(D_ALWAYS, "SetAttribute got invalid attribute named %s for job %d.%d\n", 
			attr_name ? attr_name : "(null)", cluster_id, proc_id);
		errno = EINVAL;
		return -1;
	}

		// If someone is trying to do something funny with an invalid
		// attribute value, bail earlyxs
	if (!IsValidAttrValue(attr_value)) {
		dprintf(D_ALWAYS,
				"SetAttribute received invalid attribute value '%s' for "
				"job %d.%d, ignoring\n",
				attr_value ? attr_value : "(null)", cluster_id, proc_id);
		errno = EINVAL;
		return -1;
	}

	// Ensure user is not changing a secure attribute.  Only schedd is
	// allowed to do that, via the internal API.
	if (secure_attrs.find(attr_name) != secure_attrs.end())
	{
		// should we fail or silently succeed?  (old submits set secure attrs)
		const CondorVersionInfo *vers = NULL;
		if ( Q_SOCK && ! Ignore_Secure_SetAttr_Attempts) {
			vers = Q_SOCK->get_peer_version();
		}
		if (vers && vers->built_since_version( 8, 5, 8 ) ) {
			// new versions should know better!  fail!
			dprintf(D_ALWAYS,
				"SetAttribute attempt to edit secure attribute %s in job %d.%d. Failing!\n",
				attr_name, cluster_id, proc_id);
			errno = EACCES;
			return -1;
		} else {
			// old versions get a pass.  succeed (but do nothing).
			// The idea here is we will not set the secure attributes, but we won't
			// propagate the error back because we don't want old condor_submits to not
			// be able to submit jobs.
			dprintf(D_ALWAYS,
				"SetAttribute attempt to edit secure attribute %s in job %d.%d. Ignoring!\n",
				attr_name, cluster_id, proc_id);
			return 0;
		}
	}

	IdToKey(cluster_id,proc_id,key);

	if (JobQueue->Lookup(key, job)) {
		// If we made it here, the user is adding or editing attrbiutes
		// to an ad that has already been committed in the queue.

		// Ensure the user is not changing a job they do not own.
		if ( Q_SOCK && !OwnerCheck(job, Q_SOCK->getOwner() )) {
			const char *owner = Q_SOCK->getOwner( );
			if ( ! owner ) {
				owner = "NULL";
			}
			dprintf(D_ALWAYS,
					"OwnerCheck(%s) failed in SetAttribute for job %d.%d\n",
					owner, cluster_id, proc_id);
			errno = EACCES;
			return -1;
		}

		// Ensure user is not changing an immutable attribute to a committed job
		bool is_immutable_attr = immutable_attrs.find(attr_name) != immutable_attrs.end();
		if (is_immutable_attr) {
			// late materialization is allowed to change some 'immutable' attributes in the cluster ad.
			if ((key.proc == -1) && YourStringNoCase(ATTR_TOTAL_SUBMIT_PROCS) == attr_name) {
				is_immutable_attr = false;
			}
		}
		if (is_immutable_attr)
		{

			dprintf(D_ALWAYS,
					"SetAttribute attempt to edit immutable attribute %s in job %d.%d\n",
					attr_name, cluster_id, proc_id);
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
		if ( Q_SOCK && 
			 ( (!isQueueSuperUser(Q_SOCK->getRealOwner()) && !qmgmt_all_users_trusted) || 
			    !Q_SOCK->getAllowProtectedAttrChanges() ) &&
			 protected_attrs.find(attr_name) != protected_attrs.end() )
		{
			dprintf(D_ALWAYS,
					"SetAttribute of protected attribute denied, RealOwner=%s EffectiveOwner=%s AllowPAttrchange=%s Attr=%s in job %d.%d\n",
					Q_SOCK->getRealOwner() ? Q_SOCK->getRealOwner() : "(null)",
					Q_SOCK->getOwner() ? Q_SOCK->getOwner() : "(null)",
					Q_SOCK->getAllowProtectedAttrChanges() ? "true" : "false",
					attr_name, cluster_id, proc_id);
			errno = EACCES;
			return -1;
		}
	/*
	}
	else if ( ! JobQueue->InTransaction()) {
		dprintf(D_ALWAYS,"Inserting new attribute %s into non-existent job %d.%d outside of a transaction\n", attr_name, cluster_id, proc_id);
		errno = ENOENT;
		return -1;
	*/
	} else if (flags & SetAttribute_SubmitTransform) {
		// submit transforms come from inside the schedd and have no restrictions
		// on which cluster/proc may be edited (the transform itself guarantees that only
		// jobs in the submit transaction will be edited)
	} else if (Q_SOCK != NULL || (flags&NONDURABLE)) {
		// If we made it here, the user (i.e. not the schedd itself)
		// is adding attributes to an ad that has not been committed yet
		// (we know this because it cannot be found in the JobQueue above).
		// Restrict the user to only adding attributes to the current cluster
		// returned by NewCluster, and also restrict the user to procs that have been
		// returned by NewProc.
		if ((cluster_id != active_cluster_num) || (proc_id >= next_proc_num)) {
			dprintf(D_ALWAYS,"Inserting new attribute %s into non-active cluster cid=%d acid=%d\n", 
					attr_name, cluster_id,active_cluster_num);
			errno = ENOENT;
			return -1;
		}
	}

	int attr_category;
	int attr_id = IsSpecialSetAttribute(attr_name, &attr_category);

	// A few special attributes have additional access checks
	// but for most, we have already decided whether or not we can change this attribute
	if (query_can_change_only) {
		if (attr_id == idATTR_OWNER || (attr_category & catJobId) || (attr_id == idATTR_JOB_STATUS)) {
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

		if ( strcasecmp(attr_value,"UNDEFINED")==0 ) {
				// If the user set the owner to be undefined, then
				// just fill in the value of Owner with the owner name
				// of the authenticated socket.
			if ( sock_owner && *sock_owner ) {
				new_value.formatstr("\"%s\"",sock_owner);
				attr_value  = new_value.Value();
			} else {
				// socket not authenticated and Owner is UNDEFINED.
				dprintf(D_ALWAYS, "ERROR SetAttribute violation: "
					"Owner is UNDEFINED, but client not authenticated\n");
				errno = EACCES;
				return -1;

			}
		}

			// We can't just use attr_value, since it contains '"'
			// marks.  Carefully remove them here.
		MyString owner_buf;
		char const *owner = attr_value;
		bool owner_is_quoted = false;
		if( *owner == '"' ) {
			owner_buf = owner+1;
			if( owner_buf.Length() && owner_buf[owner_buf.Length()-1] == '"' )
			{
				owner_buf.truncate(owner_buf.Length()-1);
				owner_is_quoted = true;
			}
			owner = owner_buf.Value();
		}

		if( !owner_is_quoted ) {
			// For sanity's sake, do not allow setting Owner to something
			// strange, such as an attribute reference that happens to have
			// the same name as the authenticated user.
			dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s which is not a valid string\n",
					attr_value);
			errno = EACCES;
			return -1;
		}

		MyString orig_owner;
		if( GetAttributeString(cluster_id,proc_id,ATTR_OWNER,orig_owner) >= 0
			&& orig_owner != owner
			&& !qmgmt_all_users_trusted )
		{
			// Unless all users are trusted, nobody (not even queue super user)
			// has the ability to change the owner attribute once it is set.
			// See gittrack #1018.
			dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s when previously set to \"%s\"\n",
					attr_value, orig_owner.Value());
			errno = EACCES;
			return -1;
		}

		if (!qmgmt_all_users_trusted
#if defined(WIN32)
			&& (strcasecmp(owner,sock_owner) != 0)
#else
			&& (strcmp(owner,sock_owner) != 0)
#endif
			&& (!isQueueSuperUser(sock_owner) || !SuperUserAllowedToSetOwnerTo(owner)) ) {
				dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s when active owner is \"%s\"\n",
					attr_value, sock_owner);
				errno = EACCES;
				return -1;
		}

#if !defined(WIN32)
		uid_t user_uid;
		if ( can_switch_ids() && !pcache()->get_user_uid( owner, user_uid ) ) {
			dprintf( D_ALWAYS, "SetAttribute security violation: "
					 "setting owner to %s, which is not a valid user account\n",
					 attr_value );
			errno = EACCES;
			return -1;
		}
#endif

		if (query_can_change_only) {
			return 0;
		}

			// If we got this far, we're allowing the given value for
			// ATTR_OWNER to be set.  However, now, we should try to
			// insert a value for ATTR_USER, too, so that's always in
			// the job queue.
		int nice_user = 0;
		MyString user;

		GetAttributeInt( cluster_id, proc_id, ATTR_NICE_USER,
						 &nice_user );
		user.formatstr( "\"%s%s@%s\"", (nice_user) ? "nice-user." : "",
				 owner, scheduler.uidDomain() );
		SetAttribute( cluster_id, proc_id, ATTR_USER, user.Value(), flags );

			// Also update the owner history hash table
		AddOwnerHistory(owner);

		if (job) {
			// if editing (rather than creating) a job, update ownerinfo pointer, and mark submitterdata as dirty
			job->ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(owner));
			job->dirty_flags |= JQJ_CACHE_DIRTY_SUBMITTERDATA;
		}
	}
	else if (attr_id == idATTR_NICE_USER) {
			// Because we're setting a new value for nice user, we
			// should create a new value for ATTR_USER while we're at
			// it, since that might need to change now that
			// ATTR_NICE_USER is set.
		MyString owner;
		MyString user;
		bool nice_user = false;
		if( ! strcasecmp(attr_value, "TRUE") ) {
			nice_user = true;
		}
		if( GetAttributeString(cluster_id, proc_id, ATTR_OWNER, owner)
			>= 0 ) {
			user.formatstr( "\"%s%s@%s\"", (nice_user) ? "nice-user." :
					 "", owner.Value(), scheduler.uidDomain() );
			SetAttribute( cluster_id, proc_id, ATTR_USER, user.Value(), flags );
		}
	}
	else if (attr_id == idATTR_JOB_NOOP) {
		// whether the job has an IsNoopJob attribute or not is cached in the job object
		// so if this is set, we need to mark the cached value as dirty.
		if (job) { job->DirtyNoopAttr(); }
	}
	else if (attr_category & catJobId) {
		char *endptr = NULL;
		int id = (int)strtol(attr_value, &endptr, 10);
		if (attr_id == idATTR_CLUSTER_ID && (*endptr != '\0' || id != cluster_id)) {
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ClusterId to incorrect value (%s!=%d)\n",
				attr_value, cluster_id);
			errno = EACCES;
			return -1;
		}
		if (attr_id == idATTR_PROC_ID && (*endptr != '\0' || id != proc_id)) {
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ProcId to incorrect value (%s!=%d)\n",
				attr_value, proc_id);
			errno = EACCES;
			return -1;
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
		int new_cnt = (int)strtol( attr_value, NULL, 10 );
		PROC_ID job_id( cluster_id, proc_id );
		shadow_rec *srec = scheduler.FindSrecByProcID(job_id);
		GetAttributeInt( cluster_id, proc_id, ATTR_NUM_JOB_RECONNECTS, &curr_cnt );
		if ( srec && srec->is_reconnect && !srec->reconnect_succeeded &&
			 new_cnt > curr_cnt ) {

			srec->reconnect_succeeded = true;
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
		int new_status = (int)strtol( attr_value, NULL, 10 );

		GetAttributeInt( cluster_id, proc_id, ATTR_JOB_STATUS, &curr_status );
		GetAttributeInt( cluster_id, proc_id, ATTR_LAST_JOB_STATUS, &last_status );
		GetAttributeInt( cluster_id, proc_id, ATTR_JOB_STATUS_ON_RELEASE, &release_status );

		if ( new_status != HELD && new_status != REMOVED &&
			 ( curr_status == REMOVED || last_status == REMOVED ||
			   release_status == REMOVED ) ) {
			dprintf( D_ALWAYS, "SetAttribute violation: Attempt to change %s of removed job %d.%d to %d\n",
					 ATTR_JOB_STATUS, cluster_id, proc_id, new_status );
			errno = EINVAL;
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
		bool is_spooling_hold = false;
		if (attr_id == idATTR_HOLD_REASON_CODE) {
			int hold_reason = (int)strtol( attr_value, NULL, 10 );
			is_spooling_hold = (CONDOR_HOLD_CODE_SpoolingInput == hold_reason);
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
	MyString round_param_name;
	round_param_name = "SCHEDD_ROUND_ATTR_";
	round_param_name += attr_name;

	char *round_param = param(round_param_name.Value());

	if( round_param && *round_param && strcmp(round_param,"0") ) {
		classad::Value::ValueType attr_type = classad::Value::NULL_VALUE;
		ExprTree *tree = NULL;
		classad::Value val;
		if ( ParseClassAdRvalExpr(attr_value, tree) == 0 &&
			 tree->GetKind() == classad::ExprTree::LITERAL_NODE ) {
			((classad::Literal *)tree)->GetValue( val );
			if ( val.GetType() == classad::Value::INTEGER_VALUE ) {
				attr_type = classad::Value::INTEGER_VALUE;
			} else if ( val.GetType() == classad::Value::REAL_VALUE ) {
				attr_type = classad::Value::REAL_VALUE;
			}
		}
		delete tree;

		if ( attr_type == classad::Value::INTEGER_VALUE || attr_type == classad::Value::REAL_VALUE ) {
			// first, store the actual value
			MyString raw_attribute = attr_name;
			raw_attribute += "_RAW";
			JobQueue->SetAttribute(key, raw_attribute.Value(), attr_value, flags & SETDIRTY);
			if( flags & SHOULDLOG ) {
				char* old_val = NULL;
				ExprTree *ltree;
				ltree = job->LookupExpr(raw_attribute.Value());
				if( ltree ) {
					old_val = const_cast<char*>(ExprTreeToString(ltree));
				}
				scheduler.WriteAttrChangeToUserLog(key.c_str(), raw_attribute.Value(), attr_value, old_val);
			}

			int ivalue;
			double fvalue;

			if ( attr_type == classad::Value::INTEGER_VALUE ) {
				val.IsIntegerValue( ivalue );
				fvalue = ivalue;
			} else {
				val.IsRealValue( fvalue );
				ivalue = (int) fvalue;	// truncation conversion
			}

			if( strstr(round_param,"%") ) {
					// round to specified percentage of the number's
					// order of magnitude
				char *end=NULL;
				double percent = strtod(round_param,&end);
				if( !end || end[0] != '%' || end[1] != '\0' ||
					percent > 1000 || percent < 0 )
				{
					EXCEPT("Invalid rounding parameter %s=%s",
						   round_param_name.Value(),round_param);
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
						new_value.formatstr("%d",(int)fvalue);
					}
					else {
						new_value.formatstr("%f",fvalue);
					}
				}
			}
			else {
					// round to specified power of 10
				unsigned int base;
				int exp = param_integer(round_param_name.Value(),0,0,9);

					// now compute the rounded value
					// set base to be 10^exp
				for (base=1 ; exp > 0; exp--, base *= 10) { }

					// round it.  note we always round UP!!  
				ivalue = ((ivalue + base - 1) / base) * base;

					// make it a string, courtesty MyString conversion.
				new_value = IntToStr( ivalue );

					// if it was a float, append ".0" to keep it a float
				if ( attr_type == classad::Value::REAL_VALUE ) {
					new_value += ".0";
				}
			}

			// change attr_value, so when we do the SetAttribute below
			// we really set to the rounded value.
			attr_value = new_value.Value();

		} else {
			dprintf(D_FULLDEBUG,
				"%s=%s, but value '%s' is not a scalar - ignored\n",
				round_param_name.Value(),round_param,attr_value);
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

	JobQueue->SetAttribute(key, attr_name, attr_value, flags & SETDIRTY);
	if( flags & SHOULDLOG ) {
		const char* old_val = NULL;
		if (job) {
			ExprTree *tree = job->LookupExpr(attr_name);
			if (tree) { old_val = ExprTreeToString(tree); }
		}
		scheduler.WriteAttrChangeToUserLog(key.c_str(), attr_name, attr_value, old_val);
	}

	int status;
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
		  universe != CONDOR_UNIVERSE_LOCAL &&
		  universe != CONDOR_UNIVERSE_STANDARD ) &&
		( flags & SETDIRTY ) && 
		( status == RUNNING || (( universe == CONDOR_UNIVERSE_GRID ) && jobExternallyManaged( job ) ) ) ) {

		// Add the key to list of dirty classads
		if( ! DirtyJobIDs.contains( key.c_str() ) &&
			SendDirtyJobAdNotification( key ) ) {

			DirtyJobIDs.append( key.c_str() );

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
			for ( std::set<LocalJobRec>::iterator it = scheduler.LocalJobsPrioQueue.begin(); it != scheduler.LocalJobsPrioQueue.end(); it++ ) {
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
		for (auto it = jobids.begin(); it != jobids.end(); ++it) {
			if (! job_id.set(it->c_str()) || job_id.cluster <= 0 || job_id.proc >= 0) continue; // cluster ads only
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
		for (auto it = jobids.begin(); it != jobids.end(); ++it) {
			if ( ! job_id.set(it->c_str()) || job_id.cluster <= 0 || job_id.proc < 0) continue; // ignore the cluster ad and '0.0' ad

			JobQueueJob * job = NULL;
			if ( ! JobQueue->Lookup(job_id, job)) continue; // Ignore if no job ad (yet). this happens on submit commits.

			int universe = job->Universe();
			if ( ! universe) {
				dprintf(D_ALWAYS, "job %s has no universe! in DoSetAttributeCallbacks\n", it->c_str());
				continue;
			}

			int job_status = 0;
			job->LookupInteger(ATTR_JOB_STATUS, job_status);
			if (job_status != job->Status()) {
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
			for (auto it = factories.begin(); it != factories.end(); ++it) {
				JobQueueCluster * cad = GetClusterAd(*it);
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
PeriodicDirtyAttributeNotification()
{
	char	*job_id_str;

	DirtyPidsSignaled.clear();

	DirtyJobIDs.rewind();
	while( (job_id_str = DirtyJobIDs.next()) != NULL ) {
		JOB_ID_KEY key(job_id_str);
		if ( SendDirtyJobAdNotification(key) == false ) {
			// There's no shadow/gridmanager for this job.
			// Mark it clean and remove it from the dirty list.
			// We can't call MarkJobClean() here, as that would
			// disrupt our traversal of DirtyJobIDs.
			JobQueue->ClearClassAdDirtyBits(key);
			DirtyJobIDs.deleteCurrent();
		}
	}

	if( DirtyJobIDs.isEmpty() && dirty_notice_timer_id > 0 )
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
HandleFlushJobQueueLogTimer()
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

	rc = SetAttributeInt( cluster, proc, attr_name, xact_start_time + dur, SETDIRTY );
	return rc;
}

char * simple_encode (int key, const char * src);
char * simple_decode (int key, const char * src);

// Store a simply-encoded attribute
int
SetMyProxyPassword (int cluster_id, int proc_id, const char *pwd) {

	// This is sortof a hack
	if (proc_id == -1)	{
		proc_id = 0;
	}

	// Create filename
	MyString filename;
	filename.formatstr( "%s/mpp.%d.%d", Spool, cluster_id, proc_id);

	// Swith to root temporarily
	priv_state old_priv = set_root_priv();
	// Delete the file
	struct stat stat_buff;
	if (stat (filename.Value(), &stat_buff) == 0) {
		// If the file exists, delete it
		if (unlink (filename.Value()) && errno != ENOENT) {
			set_priv(old_priv);
			return -1;
		}
	}

	// Create the file
	int fd = safe_open_wrapper_follow(filename.Value(), O_CREAT | O_WRONLY, S_IREAD | S_IWRITE);
	if (fd < 0) {
		set_priv(old_priv);
		return -1;
	}

	char * encoded_value = simple_encode (cluster_id+proc_id, pwd);
	int len = (int)strlen(encoded_value);
	if (write (fd, encoded_value, len) != len) {
		set_priv(old_priv);
		free(encoded_value);
		close(fd);
		return -1;
	}
	close (fd);

	// Switch back to non-priviledged user
	set_priv(old_priv);

	free (encoded_value);

	if (SetAttribute(cluster_id, proc_id,
					 ATTR_MYPROXY_PASSWORD_EXISTS, "TRUE") < 0) {
		EXCEPT("Failed to record fact that MyProxyPassword file exists on %d.%d",
			   cluster_id, proc_id);
	}

	return 0;

}


int
DestroyMyProxyPassword( int cluster_id, int proc_id )
{
	bool val = false;
	if (GetAttributeBool(cluster_id, proc_id,
						 ATTR_MYPROXY_PASSWORD_EXISTS, &val) < 0 ||
		!val) {
			// It doesn't exist, nothing to destroy.
		return 0;
	}

	MyString filename;
	filename.formatstr( "%s%cmpp.%d.%d", Spool, DIR_DELIM_CHAR,
					  cluster_id, proc_id );

  	// Swith to root temporarily
	priv_state old_priv = set_root_priv();

	// Delete the file
	struct stat stat_buff;
	if( stat(filename.Value(), &stat_buff) == 0 ) {
			// If the file exists, delete it
		if( unlink( filename.Value()) < 0 && errno != ENOENT ) {
			dprintf( D_ALWAYS, "unlink(%s) failed: errno %d (%s)\n",
					 filename.Value(), errno, strerror(errno) );
		 	set_priv(old_priv);
			return -1;

		}
		dprintf( D_FULLDEBUG, "Destroyed MPP %d.%d: %s\n", cluster_id, 
				 proc_id, filename.Value() );
	}

	// Switch back to non-root
	set_priv(old_priv);

	if (SetAttribute(cluster_id, proc_id,
					 ATTR_MYPROXY_PASSWORD_EXISTS, "FALSE") < 0) {
		EXCEPT("Failed to record fact that MyProxyPassword file does no exists on %d.%d",
			   cluster_id, proc_id);
	}

	return 0;
}


int GetMyProxyPassword (int cluster_id, int proc_id, char ** value) {
	// Create filename

	// Swith to root temporarily
	priv_state old_priv = set_root_priv();
	
	MyString filename;
	filename.formatstr( "%s/mpp.%d.%d", Spool, cluster_id, proc_id);
	int fd = safe_open_wrapper_follow(filename.Value(), O_RDONLY);
	if (fd < 0) {
		set_priv(old_priv);
		return -1;
	}

	char buff[MYPROXY_MAX_PASSWORD_BUFLEN];
	int bytes = read (fd, buff, sizeof(buff) - 1);
	if( bytes < 0 ) {
		close(fd);
		return -1;
	}
	buff [bytes] = '\0';

	close (fd);

	// Switch back to non-priviledged user
	set_priv(old_priv);

	*value = simple_decode (cluster_id + proc_id, buff);
	return 0;
}




static const char * mesh = "Rr1aLvzki/#6`QeNoWl^\"(!x\'=OE3HBn [A)GtKu?TJ.mdS9%Fh;<\\+w~4yPDIq>2Ufs$Xc_@g0Y5Mb|{&*}]7,CpV-j:8Z";

char * simple_encode (int key, const char * src) {

  char * result = (char*)strdup (src);

  unsigned int i= 0;
  for (; i<strlen (src); i++) {
    int c = (int)src[i]-(int)' ';
    c=(c+key)%strlen(mesh);
    result[i]=mesh[c];
  }

  return result;
}

char * simple_decode (int key, const char * src) {
  char * result = (char*)strdup (src);

  char buff[2];
  buff[1]='\0';

  unsigned int i= 0;
  unsigned int j =0;
  unsigned int c =0;
  unsigned int cm = (unsigned int)strlen(mesh);

  for (; j<strlen(src); j++) {

	//
    for (i=0; i<cm; i++) {
      if (mesh[i] == src[j]) {
		c = i;
		break;
		}
    }

    c = (c+cm-(key%cm))%cm;
    
    snprintf(buff, 2, "%c", c+' ');
    result[j]=buff[0];
    
  }
  return result;
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
		completed = true;
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
	xact_start_time = time( NULL );

	return 0;
}

int
CheckTransaction( const std::list<std::string> &newAdKeys,
                  CondorError * errorStack )
{
	int rval;

	int triggers = JobQueue->GetTransactionTriggers();
	bool has_spooling_hold = (triggers & catSpoolingHold) != 0;

	// If we don't need to perform any submit_requirement checks
	// and we don't need to perform any job transforms, then we should
	// bail out now and avoid all the expensive computation below.
	if ( !scheduler.shouldCheckSubmitRequirements() &&
		 !scheduler.jobTransforms.shouldTransform() &&
		 !has_spooling_hold)
	{
		return 0;
	}

	for( std::list<std::string>::const_iterator it = newAdKeys.begin(); it != newAdKeys.end(); ++it ) {
		JobQueueKey job( it->c_str() );
		if( job.proc == -1 ) { continue; }
		JobQueueKeyBuf cluster( job.cluster, -1 );

		ClassAd procAd;
		JobQueue->AddAttrsFromTransaction( cluster, procAd );
		JobQueue->AddAttrsFromTransaction( job, procAd );

		JobQueueJob *clusterAd = NULL;
		if (JobQueue->Lookup(cluster, clusterAd)) {
			// If there is a cluster ad in the job queue, chain to that before we evaluate anything.
			// we don't need to unchain - it's a stack object and won't live longer than this function.
			procAd.ChainToAd(clusterAd);
		}

		// Now that we created a procAd out of the transaction queue,
		// apply job transforms to the procAd.
		// If the transforms fail, bail on the transaction.
		rval = scheduler.jobTransforms.transformJob(&procAd,errorStack);
		if  (rval < 0) {
			if ( errorStack ) {
				errorStack->push( "QMGMT", 30, "Failed to apply a required job transform.\n");
			}
			errno = EINVAL;
			return rval;
		}

		// Now check that submit_requirements still hold on our (possibly transformed)
		// job ad.
		rval = scheduler.checkSubmitRequirements( & procAd, errorStack );
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
			rewriteSpooledJobAd(&procAd, job.cluster, job.proc, false);
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
	for(std::list<std::string>::iterator it = new_ad_keys.begin(); it != new_ad_keys.end(); it++ ) {
		job_id.set(it->c_str());
		if (job_id.proc < 0) continue; // ignore the cluster ad.

		// ignore jobs produced by an existing factory.
		// ATTR_TOTAL_SUBMIT_PROCS is determined by the factory for them.
		JobQueueCluster *clusterad = GetClusterAd(job_id);
		if (clusterad && clusterad->factory) {
			continue;
		}

		std::map<int, int>::iterator mit = num_procs.find(job_id.cluster);
		if (mit == num_procs.end()) {
			num_procs[job_id.cluster] = 1;
		} else {
			num_procs[job_id.cluster] += 1;
		}
	}

	// add the ATTR_TOTAL_SUBMIT_PROCS attributes to the transaction
	char number[10];
	for (std::map<int, int>::iterator mit = num_procs.begin(); mit != num_procs.end(); ++mit) {
		job_id.set(mit->first, -1);
		sprintf(number, "%d", mit->second);
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
	for(auto it = ad_keys.begin(); it != ad_keys.end(); it++ ) {
		job_id.set(it->c_str());
		if (job_id.proc >= 0) continue; // skip keys for jobs, we want cluster keys only

		JobQueueJob *job;
		if ( ! JobQueue->Lookup(job_id, job)) continue; // skip keys for which the cluster is still uncommitted

		if ( ! job || ! job->IsCluster()) continue; // just a safety check, we don't expect this to fire.
		JobQueueCluster *cad = static_cast<JobQueueCluster*>(job);

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
	for (auto it = to_add.begin(); it != to_add.end(); ++it) {
		SetAttribute(it->first.cluster, it->first.proc, ATTR_EDITED_CLUSTER_ATTRS, it->second.c_str(), 0);
	}
}

bool
ReadProxyFileIntoAd( const char *file, const char *owner, ClassAd &x509_attrs )
{
#if !defined(HAVE_EXT_GLOBUS)
	(void)file;
	(void)owner;
	(void)x509_attrs;
	return false;
#else
	// owner==NULL means don't try to switch our priv state.
	TemporaryPrivSentry tps( owner != NULL );
	if ( owner != NULL ) {
		if ( !init_user_ids( owner, NULL ) ) {
			dprintf( D_FAILURE, "ReadProxyFileIntoAd(%s): Failed to switch to user priv\n", owner );
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

	globus_gsi_cred_handle_t proxy_handle = x509_proxy_read( file );

	if ( proxy_handle == NULL ) {
		dprintf( D_FAILURE, "Failed to read job proxy: %s\n",
				 x509_error_string() );
		return false;
	}

	time_t expire_time = x509_proxy_expiration_time( proxy_handle );
	char *proxy_identity = x509_proxy_identity_name( proxy_handle );
	char *proxy_email = x509_proxy_email( proxy_handle );
	char *voname = NULL;
	char *firstfqan = NULL;
	char *fullfqan = NULL;
	extract_VOMS_info( proxy_handle, 0, &voname, &firstfqan, &fullfqan );

	x509_proxy_free( proxy_handle );

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

static void
AddSessionAttributes(const std::list<std::string> &new_ad_keys)
{
	if (new_ad_keys.empty()) {return;}

	ClassAd policy_ad;
	if (Q_SOCK && Q_SOCK->getReliSock()) {
		const std::string &sess_id = Q_SOCK->getReliSock()->getSessionID();
		daemonCore->getSecMan()->getSessionPolicy(sess_id.c_str(), policy_ad);
	}

	classad::ClassAdUnParser unparse;
	unparse.SetOldClassAd(true, true);

	// See if the values have already been set
	ClassAd *x509_attrs = &policy_ad;
	string last_proxy_file;
	ClassAd proxy_file_attrs;

	// Put X509 credential information in cluster ads (from either the
	// job's proxy or the GSI authentication on the CEDAR socket).
	// Proc ads should get X509 credential information only if they
	// have a proxy file different than in their cluster ad.
	for (std::list<std::string>::const_iterator it = new_ad_keys.begin(); it != new_ad_keys.end(); ++it)
	{
		string x509up;
		string iwd;
		JobQueueKey job( it->c_str() );
		GetAttributeString(job.cluster, job.proc, ATTR_X509_USER_PROXY, x509up);
		GetAttributeString(job.cluster, job.proc, ATTR_JOB_IWD, iwd);
		if (job.proc != -1 && x509up.empty()) {
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
			GetAttributeString(job.cluster, -1, ATTR_X509_USER_PROXY, x509up);
			if (x509up.empty() || x509up[0] == DIR_DELIM_CHAR) {
				// A proc ad with no proxy filename whose cluster ad
				// has no proxy filename or a proxy filename with full path.
				// Let any x509 attributes from the cluster ad show through.
				continue;
			}
		}
		if (job.proc == -1 && x509up.empty()) {
			// A cluster ad with no proxy file. If the client authenticated
			// with GSI, use the attributes from that credential.
			x509_attrs = &policy_ad;
		} else {
			// We have a cluster ad with a proxy file or a proc ad with a
			// proxy file that may be different than in its cluster's ad.
			string full_path;
			if ( x509up[0] == DIR_DELIM_CHAR ) {
				full_path = x509up;
			} else {
				if ( iwd.empty() ) {
					GetAttributeString(job.cluster, -1, ATTR_JOB_IWD, iwd );
				}
				formatstr( full_path, "%s%c%s", iwd.c_str(), DIR_DELIM_CHAR, x509up.c_str() );
			}
			if (job.proc != -1) {
				string cluster_full_path;
				string cluster_x509up;
				GetAttributeString(job.cluster, -1, ATTR_X509_USER_PROXY, cluster_x509up);
				if (cluster_x509up[0] == DIR_DELIM_CHAR) {
					cluster_full_path = cluster_x509up;
				} else {
					string cluster_iwd;
					GetAttributeString(job.cluster, -1, ATTR_JOB_IWD, cluster_iwd );
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
				string owner;
				if ( GetAttributeString(job.cluster, job.proc, ATTR_OWNER, owner) == -1 ) {
					GetAttributeString(job.cluster, -1, ATTR_OWNER, owner);
				}
				last_proxy_file = full_path;
				proxy_file_attrs.Clear();
				ReadProxyFileIntoAd( last_proxy_file.c_str(), owner.c_str(), proxy_file_attrs );
			}
			if ( proxy_file_attrs.size() > 0 ) {
				x509_attrs = &proxy_file_attrs;
			} else if (job.proc != -1) {
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

		for (ClassAd::const_iterator attr_it = x509_attrs->begin(); attr_it != x509_attrs->end(); ++attr_it)
		{
			std::string attr_value_buf;
			unparse.Unparse(attr_value_buf, attr_it->second);
			SetSecureAttribute(job.cluster, job.proc, attr_it->first.c_str(), attr_value_buf.c_str());
			dprintf(D_SECURITY, "ATTRS: SetAttribute %i.%i %s=%s\n", job.cluster, job.proc, attr_it->first.c_str(), attr_value_buf.c_str());
		}
	}
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

	if( errorStack == NULL ) {
		dprintf( D_ALWAYS | D_BACKTRACE, "ERROR: CommitTransaction() called with NULL error stack.\n" );
	}

	return CommitTransactionInternal( durable, errorStack );
}

int CommitTransactionInternal( bool durable, CondorError * errorStack ) {

	std::list<std::string> new_ad_keys;
		// get a list of all new ads being created in this transaction
	JobQueue->ListNewAdsInTransaction( new_ad_keys );
	if ( ! new_ad_keys.empty()) { SetSubmitTotalProcs(new_ad_keys); }

	if ( !new_ad_keys.empty() ) {
		AddSessionAttributes(new_ad_keys);

		int rval = CheckTransaction(new_ad_keys, errorStack);
		if ( rval < 0 ) {
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
	const char * commit_comment = NULL;
	if (scheduler.getEnableJobQueueTimestamps()) {
		struct timeval tv;
		struct tm eventTime;
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

	// Now that we've commited for sure, up the TotalJobsCount
	TotalJobsCount += jobs_added_this_transaction; 

	// If the commit failed, we should never get here.

	// Now that the transaction has been commited, we need to chain proc
	// ads to cluster ads if any new clusters have been submitted.
	// Also, if EVENT_LOG is defined in condor_config, we will write
	// submit events into the EVENT_LOG here.
	if ( !new_ad_keys.empty() ) {
		JobQueueKeyBuf job_id;
		int old_cluster_id = -10;
		JobQueueJob *procad = NULL;
		JobQueueCluster *clusterad = NULL;

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
					clusterad->jid = job_id;
					clusterad->PopulateFromAd();
					if ( Q_SOCK && Q_SOCK->getOwner() ) {
						clusterad->ownerinfo = const_cast<OwnerInfo*>(scheduler.insert_owner_const(Q_SOCK->getOwner()));
					}

					// make the cluster job factory if one is desired and does not already exist.
					if (scheduler.getAllowLateMaterialize() && has_late_materialize) {
						std::map<int, JobFactory*>::iterator found = JobFactoriesSubmitPending.find(clusterad->jid.cluster);
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
								MyString spooled_filename;
								GetSpooledSubmitDigestPath(spooled_filename, clusterad->jid.cluster, Spool);
								bool spooled_digest = YourStringNoCase(spooled_filename) == submit_digest;

								std::string errmsg;
								clusterad->factory = MakeJobFactory(clusterad, submit_digest.c_str(), spooled_digest, errmsg);
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
				clusterad = NULL;
			}
			if ( ! clusterad) {
				clusterad = GetClusterAd(job_id.cluster);
			}

			if (clusterad && JobQueue->Lookup(job_id, procad))
			{
				dprintf(D_FULLDEBUG,"New job: %s\n",job_id.c_str());

					// increment the 'recently added' job count for this owner
				OwnerInfo * ownerinfo = clusterad->ownerinfo;
				if (ownerinfo) {
					scheduler.incrementRecentlyAdded( ownerinfo, NULL );
				} else if ( Q_SOCK && Q_SOCK->getOwner() ) {
					ownerinfo = scheduler.incrementRecentlyAdded( ownerinfo, Q_SOCK->getOwner() );
					clusterad->ownerinfo = ownerinfo;
				} else {
					ASSERT(ownerinfo);
				}

					// chain proc ads to cluster ad
				procad->ChainToAd(clusterad);

					// convert any old attributes for backwards compatbility
				ConvertOldJobAdAttrs(procad, false);

					// make sure the job objd and cluster object are populated
				procad->jid = job_id;
				clusterad->AttachJob(procad);
				procad->PopulateFromAd();
				procad->ownerinfo = ownerinfo;

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
				if ( job_status == HELD && hold_code == CONDOR_HOLD_CODE_SpoolingInput ) {
					SpooledJobFiles::createJobSpoolDirectory(procad,PRIV_UNKNOWN);
				}

				//don't need to do this... the trigger code below seems to handle it.
				//IncrementLiveJobCounter(scheduler.liveJobCounts, procad->Universe(), job_status, 1);
				//if (ownerinfo) { IncrementLiveJobCounter(ownerinfo->live, procad->Universe(), job_status, 1); }

				std::string version;
				if ( procad->LookupString( ATTR_VERSION, version ) ) {
					CondorVersionInfo vers( version.c_str() );
					// CRUFT If the submitter is older than 7.5.4, then
					// they are responsible for writing the submit event
					// to the user log.
					if ( vers.built_since_version( 7, 5, 4 ) ) {
						std::string warning;
						if(errorStack && (! errorStack->empty())) {
							warning = errorStack->getFullText();
						}
						scheduler.WriteSubmitToUserLog( procad, doFsync, warning.empty() ? NULL : warning.c_str() );
					}
				}

				int iDup, iTotal;
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
								CONDOR_HOLD_CODE_MaxTransferInputSizeExceeded, 0);
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
	return JobQueue->AbortTransaction();
}

void
AbortTransactionAndRecomputeClusters()
{
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
		JobQueueJob *job;
		JobQueueKey key;
		JobQueue->StartIterateAllClassAds();
		while (JobQueue->Iterate(key, job)) {
			if (key.cluster > 0 && key.proc >= 0) { // look at job ads only.
				int *numOfProcs = NULL;
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
				JobQueueCluster * cad = static_cast<JobQueueCluster*>(job);
				int *numOfProcs = NULL;
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
GetAttributeFloat(int cluster_id, int proc_id, const char *attr_name, float *val)
{
	ClassAd	*ad;
	JobQueueKeyBuf	key;
	char	*attr_val;

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
GetAttributeInt(int cluster_id, int proc_id, const char *attr_name, int *val)
{
	ClassAd	*ad;
	JobQueueKeyBuf key;
	char	*attr_val;

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
GetAttributeBool(int cluster_id, int proc_id, const char *attr_name, bool *val)
{
	ClassAd	*ad;
	JobQueueKeyBuf key;
	char	*attr_val;

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
	ClassAd	*ad;
	JobQueueKeyBuf key;
	char	*attr_val;

	*val = NULL;

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
					MyString &val )
{
	std::string strVal;
	int rc = GetAttributeString(cluster_id, proc_id, attr_name, strVal);
	val = strVal;
	return rc;
}

// returns -1 if the lookup fails or if the value is not a string, 0 if
// the lookup succeeds in the job queue, 1 if it succeeds in the current
// transaction; val is set to the empty string on failure
int
GetAttributeString( int cluster_id, int proc_id, const char *attr_name,
                    std::string &val )
{
	ClassAd	*ad = NULL;
	char	*attr_val;
	std::string tmp;

	JobQueueKeyBuf key;
	IdToKey(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		ClassAd tmp_ad;
		tmp_ad.AssignExpr(attr_name,attr_val);
		free( attr_val );
		if( tmp_ad.LookupString(attr_name, tmp) == 1) {
			val = tmp;
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

	if (ad->LookupString(attr_name, tmp) == 1) {
		val = tmp;
		return 0;
	}
	val = "";
	errno = EINVAL;
	return -1;
}

int
GetAttributeExprNew(int cluster_id, int proc_id, const char *attr_name, char **val)
{
	ClassAd		*ad;
	ExprTree	*tree;
	char		*attr_val;

	*val = NULL;

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
	ClassAd 	*ad = NULL;
	char		*val;
	const char	*name;
	ExprTree 	*expr;

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
		if(expr && !ClassAdAttributeIsPrivate(name))
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
	ClassAd				*ad = NULL;
	char				*attr_val = NULL;

	JobQueueKeyBuf key;
	IdToKey(cluster_id,proc_id,key);

	if (!JobQueue->LookupClassAd(key, ad)) {
		if( ! JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
			errno = ENOENT;
			return -1;
		}
	}

		// If we found it in the transaction, we need to free attr_val
		// so we don't leak memory.  We don't use the value, we just
		// wanted to make sure we could find the attribute so we know
		// to return failure if needed.
	if( attr_val ) {
		free( attr_val );
	}
	
	if (Q_SOCK && !OwnerCheck(ad, Q_SOCK->getOwner() )) {
		errno = EACCES;
		return -1;
	}

	JobQueue->DeleteAttribute(key, attr_name);

	JobQueueDirty = true;

	return 1;
}

void
MarkJobClean(PROC_ID proc_id)
{
	JobQueueKeyBuf job_id(proc_id);
	const char * job_id_str = job_id.c_str();
	if (JobQueue->ClearClassAdDirtyBits(job_id))
	{
		dprintf(D_FULLDEBUG, "Cleared dirty attributes for job %s\n", job_id_str);
	}

	DirtyJobIDs.remove(job_id_str);

	if( DirtyJobIDs.isEmpty() && dirty_notice_timer_id > 0 )
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
		char *bigbuf2 = NULL;
		char *attribute_value = NULL;
		ClassAd *expanded_ad;
		int index;
		char *left,*name,*right,*value,*tvalue;
		bool value_came_from_jobad;

		// we must make a deep copy of the job ad; we do not
		// want to expand the ad we have in memory.
		expanded_ad = new ClassAd(*ad);  

		// Copy attributes from chained parent ad into the expanded ad
		// so if parent is deleted before caller is finished with this
		// ad, things will still be ok.
		ChainCollapse(*expanded_ad);

			// Make a stringlist of all attribute names in job ad.
			// Note: ATTR_JOB_CMD must be first in AttrsToExpand...
		StringList AttrsToExpand;
		const char * curr_attr_to_expand;
		AttrsToExpand.append(ATTR_JOB_CMD);
		for ( auto itr = expanded_ad->begin(); itr != expanded_ad->end(); itr++ ) {
			if ( strncasecmp(itr->first.c_str(),"MATCH_",6) == 0 ) {
					// We do not want to expand MATCH_XXX attributes,
					// because these are used to store the result of
					// previous expansions, which could potentially
					// contain literal $$(...) in the replacement text.
				continue;
			}
			if ( strcasecmp(itr->first.c_str(),ATTR_JOB_CMD) ) {
				AttrsToExpand.append(itr->first.c_str());
			}
		}

		index = -1;	
		AttrsToExpand.rewind();
		bool attribute_not_found = false;
		while ( !attribute_not_found ) 
		{
			index++;
			curr_attr_to_expand = AttrsToExpand.next();

			if ( curr_attr_to_expand == NULL ) {
				// all done; no more attributes to try and expand
				break;
			}

			MyString cachedAttrName = MATCH_EXP;
			cachedAttrName += curr_attr_to_expand;

			if( !startd_ad ) {
					// No startd ad, so try to find cached value from back
					// when we did have a startd ad.
				ExprTree *cached_value = ad->LookupExpr(cachedAttrName.Value());
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

			if (attribute_value != NULL) {
				free(attribute_value);
				attribute_value = NULL;
			}

			// Get the current value of the attribute.  We want
			// to use PrintToNewStr() here because we want to work
			// with anything (strings, ints, etc) and because want
			// strings unparsed (for instance, quotation marks should
			// be escaped with backslashes) so that we can re-insert
			// them later into the expanded ClassAd.
			// Note: deallocate attribute_value with free(), despite
			// the mis-leading name PrintTo**NEW**Str.  
			ExprTree *tree = ad->LookupExpr(curr_attr_to_expand);
			if ( tree ) {
				const char *new_value = ExprTreeToString( tree );
				if ( new_value ) {
					attribute_value = strdup( new_value );
				}
			}

			if ( attribute_value == NULL ) {
				continue;
			}

				// Some backwards compatibility: if the
				// user just has $$opsys.$$arch in the
				// ATTR_JOB_CMD attribute, convert it to the
				// new format w/ parenthesis: $$(opsys).$$(arch)
			if ( (index == 0) && (attribute_value != NULL)
				 && ((tvalue=strstr(attribute_value,"$$")) != NULL) ) 
			{
				if ( strcasecmp("$$OPSYS.$$ARCH",tvalue) == MATCH ) 
				{
						// convert to the new format
						// First, we need to re-allocate attribute_value to a bigger
						// buffer.
					int old_size = (int)strlen(attribute_value);
					void * pv = realloc(attribute_value, old_size 
										+ 10);  // for the extra parenthesis
					ASSERT(pv);
					attribute_value = (char *)pv; 
						// since attribute_value may have moved, we need
						// to reset the value of tvalue.
					tvalue = strstr(attribute_value,"$$");	
					ASSERT(tvalue);
					strcpy(tvalue,"$$(OPSYS).$$(ARCH)");
					ad->Assign(curr_attr_to_expand, attribute_value);
				}
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

					MyString expr_to_add;
					expr_to_add.formatstr("string(%s", name + 1);
					expr_to_add.setAt(expr_to_add.Length()-1, ')');

						// Any backwacked double quotes or backwacks
						// within the []'s should be unbackwacked.
					int read_pos;
					int write_pos;
					for( read_pos = 0, write_pos = 0;
						 read_pos < expr_to_add.Length();
						 read_pos++, write_pos++ )
					{
						if( expr_to_add[read_pos] == '\\'  &&
							read_pos+1 < expr_to_add.Length() &&
							( expr_to_add[read_pos+1] == '\"' ||
							  expr_to_add[read_pos+1] == '\\' ) )
						{
							read_pos++; // skip over backwack
						}
						if( read_pos != write_pos ) {
							expr_to_add.setAt(write_pos,expr_to_add[read_pos]);
						}
					}
					if( read_pos != write_pos ) { // terminate the string
						expr_to_add.truncate(write_pos);
					}

					ClassAd tmpJobAd(*ad);
					const char * INTERNAL_DD_EXPR = "InternalDDExpr";

					bool isok = tmpJobAd.AssignExpr(INTERNAL_DD_EXPR, expr_to_add.Value());
					if( ! isok ) {
						attribute_not_found = true;
						break;
					}

					std::string result;
					isok = EvalString(INTERNAL_DD_EXPR, &tmpJobAd, startd_ad, result);
					if( ! isok ) {
						attribute_not_found = true;
						break;
					}
					MyString replacement_value;
					replacement_value += left;
					replacement_value += result;
					search_pos = replacement_value.Length();
					replacement_value += right;
					MyString replacement_attr = curr_attr_to_expand;
					replacement_attr += "=";
					replacement_attr += replacement_value;
					expanded_ad->Insert(replacement_attr.Value());
					dprintf(D_FULLDEBUG,"$$([]) substitution: %s\n",replacement_attr.Value());

					free(attribute_value);
					attribute_value = strdup(replacement_value.Value());


				} else  {
					// This is an attribute from the machine ad

					// If the name contains a colon, then it
					// is a	fallback value, should the startd
					// leave it undefined, e.g.
					// $$(NearestStorage:turkey)

					char *fallback;

					fallback = strchr(name,':');
					if(fallback) {
						*fallback = 0;
						fallback++;
					}

					if (strchr(name, '.')) {
						// . is a legal character for some find_config_macros, but not other
						// check here if one snuck through
						attribute_not_found = true;
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
						MyString expr;
						expr = "MATCH_";
						expr += name;
						value = sPrintExpr(*ad, expr.Value());
						value_came_from_jobad = true;
					}

					if (!value) {
						if(fallback) {
							char *rebuild = (char *) malloc(  strlen(name)
								+ 3  // " = "
								+ 1  // optional '"'
								+ strlen(fallback)
								+ 1  // optional '"'
								+ 1); // null terminator
                            // fallback is defined as being a string value, encode it thusly:
                            sprintf(rebuild,"%s = \"%s\"", name, fallback);
							value = rebuild;
						}
						if(!fallback || !value) {
							attribute_not_found = true;
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
						MyString expr;
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
						if ( SetAttribute(cluster_id,proc_id,expr.Value(),tvalue) < 0 )
						{
							EXCEPT("Failed to store %s into job ad %d.%d",
								expr.Value(),cluster_id,proc_id);
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
					size_t lenBigbuf = strlen(left) + strlen(tvalue)  + strlen(right);
					bigbuf2 = (char *) malloc( lenBigbuf +1 );
					ASSERT(bigbuf2);
					sprintf(bigbuf2,"%s%s%n%s",left,tvalue,&search_pos,right);
					free(attribute_value);
					attribute_value = (char *) malloc(  strlen(curr_attr_to_expand)
													  + 3 // = and quotes
													  + lenBigbuf
													  + 1);
					ASSERT(attribute_value);
					sprintf(attribute_value,"%s=%s",curr_attr_to_expand,
						bigbuf2);
					expanded_ad->Insert(attribute_value);
					dprintf(D_FULLDEBUG,"$$ substitution: %s\n",attribute_value);
					free(value);	// must use free here, not delete[]
					free(attribute_value);
					attribute_value = bigbuf2;
					bigbuf2 = NULL;
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
				if ( SetAttribute(cluster_id,proc_id,cachedAttrName.Value(),attribute_value) < 0 )
				{
					EXCEPT("Failed to store '%s=%s' into job ad %d.%d",
						cachedAttrName.Value(), attribute_value, cluster_id, proc_id);
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
			for ( auto itr = startd_ad->begin(); itr != startd_ad->end(); itr++ ) {
				if( !strncmp(itr->first.c_str(),ATTR_NEGOTIATOR_MATCH_EXPR,len) ) {
					ExprTree *expr = itr->second;
					if( !expr ) {
						continue;
					}
					const char *new_value = NULL;
					new_value = ExprTreeToString(expr);
					ASSERT(new_value);
					expanded_ad->AssignExpr(itr->first.c_str(),new_value);

					MyString match_exp_name = MATCH_EXP;
					match_exp_name += itr->first;
					if ( SetAttribute(cluster_id,proc_id,match_exp_name.Value(),new_value) < 0 )
					{
						EXCEPT("Failed to store '%s=%s' into job ad %d.%d",
						       match_exp_name.Value(), new_value, cluster_id, proc_id);
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
			StringList reslist(resslist.c_str());

			reslist.rewind();
			while (const char * resname = reslist.next()) {
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
				if (EvalAttr(resname, ad, startd_ad, val)) {
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

		if ( startd_ad && job_universe == CONDOR_UNIVERSE_GRID ) {
				// Can remove our matched ad since we stored all the
				// values we need from it into the job ad.
			RemoveMatchedAd(cluster_id,proc_id);
		}


		if ( attribute_not_found ) {
			MyString hold_reason;
			// Don't put the $$(expr) literally in the hold message, otherwise
			// if we fix the original problem, we won't be able to expand the one
			// in the hold message
			hold_reason.formatstr("Cannot expand $$ expression (%s).",name);

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
			Q_SOCK = NULL;
			holdJob(cluster_id, proc_id, hold_reason.Value());
			Q_SOCK = saved_sock;

			char buf[256];
			snprintf(buf,256,"Your job (%d.%d) is on hold",cluster_id,proc_id);
			Email mailer;
			FILE * email = mailer.open_stream( ad, JOB_SHOULD_HOLD, buf );
			if ( email ) {
				fprintf(email,"Condor failed to start your job %d.%d \n",
					cluster_id,proc_id);
				fprintf(email,"because job attribute %s contains $$(%s).\n",
					curr_attr_to_expand,name);
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


		if ( startd_ad && job_universe != CONDOR_UNIVERSE_GRID ) {
			// Produce an environment description that is compatible with
			// whatever the starter expects.
			// Note: this code path is skipped when we flock and reconnect
			//  after a disconnection (job lease).  In this case we don't
			//  have a startd_ad!

			Env env_obj;

			char *opsys = NULL;
			startd_ad->LookupString( ATTR_OPSYS, &opsys);
			char *startd_version = NULL;
			startd_ad->LookupString( ATTR_VERSION, &startd_version);
			CondorVersionInfo ver_info(startd_version);

			MyString env_error_msg;
			if(!env_obj.MergeFrom(expanded_ad,&env_error_msg) ||
			   !env_obj.InsertEnvIntoClassAd(expanded_ad,&env_error_msg,opsys,&ver_info))
			{
				attribute_not_found = true;
				MyString hold_reason;
				hold_reason.formatstr(
					"Failed to convert environment to target syntax"
					" for starter (opsys=%s): %s",
					opsys ? opsys : "NULL",env_error_msg.Value());


				dprintf( D_ALWAYS, 
					"Putting job %d.%d on hold - cannot convert environment"
					" to target syntax for starter (opsys=%s): %s\n",
					cluster_id, proc_id, opsys ? opsys : "NULL",
						 env_error_msg.Value() );

				// SetAttribute does security checks if Q_SOCK is
				// not NULL.  So, set Q_SOCK to be NULL before
				// placing the job on hold so that SetAttribute
				// knows this request is not coming from a client.
				// Then restore Q_SOCK back to the original value.

				QmgmtPeer* saved_sock = Q_SOCK;
				Q_SOCK = NULL;
				holdJob(cluster_id, proc_id, hold_reason.Value());
				Q_SOCK = saved_sock;
			}


			// Now convert the arguments to a form understood by the starter.
			ArgList arglist;
			MyString arg_error_msg;
			if(!arglist.AppendArgsFromClassAd(expanded_ad,&arg_error_msg) ||
			   !arglist.InsertArgsIntoClassAd(expanded_ad,&ver_info,&arg_error_msg))
			{
				attribute_not_found = true;
				MyString hold_reason;
				hold_reason.formatstr(
					"Failed to convert arguments to target syntax"
					" for starter: %s",
					arg_error_msg.Value());


				dprintf( D_ALWAYS, 
					"Putting job %d.%d on hold - cannot convert arguments"
					" to target syntax for starter: %s\n",
					cluster_id, proc_id,
					arg_error_msg.Value() );

				// SetAttribute does security checks if Q_SOCK is
				// not NULL.  So, set Q_SOCK to be NULL before
				// placing the job on hold so that SetAttribute
				// knows this request is not coming from a client.
				// Then restore Q_SOCK back to the original value.

				QmgmtPeer* saved_sock = Q_SOCK;
				Q_SOCK = NULL;
				holdJob(cluster_id, proc_id, hold_reason.Value());
				Q_SOCK = saved_sock;
			}


			if(opsys) free(opsys);
			if(startd_version) free(startd_version);
		}

		if ( attribute_value ) free(attribute_value);
		if ( bigbuf2 ) free (bigbuf2);

		if ( attribute_not_found )
			return NULL;
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
	static const int ATTR_ARRAY_SIZE = 5;
	static const char *AttrsToModify[ATTR_ARRAY_SIZE] = {
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY };
	static const bool AttrIsList[ATTR_ARRAY_SIZE] = {
		false,
		false,
		true,
		false,
		false };
	static const char *AttrXferBool[ATTR_ARRAY_SIZE] = {
		ATTR_TRANSFER_EXECUTABLE,
		ATTR_TRANSFER_INPUT,
		NULL,
		NULL,
		NULL };

	int attrIndex;
	char new_attr_name[500];
	char *buf = NULL;
	ExprTree *expr = NULL;
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
		buf = NULL;
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
			StringList remap_commands_list(remap_string.c_str(), ";");
			remap_commands_list.rewind();
			char *command;
			std::string remap_commands;
			while( (command = remap_commands_list.next()) ) {
				StringList command_parts(command, " =");
				if (command_parts.number() != 2) {continue;}
				command_parts.rewind();
				command_parts.next();
				auto dest = command_parts.next();
				if (IsUrl(dest)) {
					url_remap_commands += command;
					url_remap_commands += ";";
				} else {
					remap_commands += command;
					remap_commands += ";";
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
		bool xfer_it;
		if (buf) free(buf);
		buf = NULL;
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
		StringList old_paths(NULL,",");
		StringList new_paths(NULL,",");
		if ( AttrIsList[attrIndex] ) {
			old_paths.initializeFromString(buf);
		} else {
			old_paths.insert(buf);
		}
		old_paths.rewind();
		char *old_path_buf;
		bool changed = false;
		const char *base = NULL;
		while ( (old_path_buf=old_paths.next()) ) {
			base = condor_basename(old_path_buf);
			if ((strcmp(AttrsToModify[attrIndex], ATTR_TRANSFER_INPUT_FILES)==0) && IsUrl(old_path_buf)) {
				base = old_path_buf;
			} else if ( strcmp(base,old_path_buf)!=0 ) {
				changed = true;
			}
			new_paths.append(base);
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
			char *new_value = new_paths.print_to_string();
			ASSERT(new_value);
			if ( modify_ad ) {
				job_ad->Assign(AttrsToModify[attrIndex],new_value);
			} else {
				SetAttributeString(cluster,proc,AttrsToModify[attrIndex],new_value);
			}
			free(new_value);
		}
	}
	if (buf) free(buf);
	return true;
}


JobQueueJob *
GetJobAd(const PROC_ID &job_id)
{
	JobQueueJob	*job = NULL;
	if (JobQueue && JobQueue->Lookup(JobQueueKey(job_id), job)) {
		return job;
	}
	return NULL;
}

int GetJobInfo(JobQueueJob *job, const OwnerInfo*& powni)
{
	if (job) {
		powni = job->ownerinfo;
		return job->Universe();
	}
	powni = NULL;
	return CONDOR_UNIVERSE_MIN;
}

JobQueueJob* 
GetJobAndInfo(const PROC_ID& jid, int &universe, const OwnerInfo *&powni)
{
	universe = CONDOR_UNIVERSE_MIN;
	powni = NULL;
	JobQueueJob	*job = NULL;
	if (JobQueue && JobQueue->Lookup(JobQueueKey(jid), job)) {
		universe = GetJobInfo(job, powni);
		return job;
	}
	return NULL;
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
	JobQueueJob	*job = NULL;
	if (JobQueue && JobQueue->Lookup(jid, job)) {
		return static_cast<JobQueueCluster*>(job);
	}
	return NULL;
}

JobQueueCluster *
GetClusterAd(const PROC_ID &job_id)
{
	return GetClusterAd(job_id.cluster);
}


ClassAd* GetExpandedJobAd(const PROC_ID& job_id, bool persist_expansions)
{
	JobQueueJob *job = GetJobAd(job_id);
	if ( ! job)
		return NULL;

	ClassAd *ad = job;
	ClassAd *startd_ad = NULL;

	// find the startd ad.  this is done differently if the job
	// is a globus universe jobs or not.
	int	job_universe = -1;
	ad->LookupInteger(ATTR_JOB_UNIVERSE,job_universe);
	if ( job_universe == CONDOR_UNIVERSE_GRID ) {
		// Globus job... find "startd ad" via our simple
		// hash table.
		scheduler.resourcesByProcID->lookup(job_id,startd_ad);
	} else {
		// Not a Globus job... find startd ad via the match rec
		match_rec *mrec;
		int sendToDS = 0;
		ad->LookupInteger(ATTR_WANT_PARALLEL_SCHEDULING, sendToDS);
		if ((job_universe == CONDOR_UNIVERSE_PARALLEL) ||
			(job_universe == CONDOR_UNIVERSE_MPI) ||
			sendToDS) {
			mrec = dedicated_scheduler.FindMRecByJobID( job_id );
		} else {
			mrec = scheduler.FindMrecByJobID( job_id );
		}

		if( mrec ) {
			startd_ad = mrec->my_match_ad;
		} else {
			// no match rec, probably a local universe type job.
			// set startd_ad to NULL and continue on - after all,
			// the expression we are expanding may not even reference
			// a startd attribute.
			startd_ad = NULL;
		}

	}

	return dollarDollarExpand(job_id.cluster, job_id.proc, ad, startd_ad, persist_expansions);
}

// We have to define this to prevent the version in qmgmt_stubs from being pulled into the schedd.
ClassAd * GetJobAd_as_ClassAd(int cluster_id, int proc_id, bool expStartdAd, bool persist_expansions) {
	ClassAd* ad = NULL;
	if (expStartdAd) {
		ad = GetExpandedJobAd(JOB_ID_KEY(cluster_id, proc_id), persist_expansions);
	} else {
		ad = GetJobAd(JOB_ID_KEY(cluster_id, proc_id));
	}
	return ad;
}


JobQueueJob *
GetJobByConstraint(const char *constraint)
{
	JobQueueJob	*ad;
	JobQueueKey key;

	JobQueue->StartIterateAllClassAds();
	while(JobQueue->Iterate(key,ad)) {
		if ( key.cluster > 0 && key.proc >= 0 && // avoid cluster and header ads
			EvalExprBool(ad, constraint)) {
				return ad;
		}
	}
	return NULL;
}

// declare this so that we don't try and pull in the one in send_stubs
ClassAd * GetJobByConstraint_as_ClassAd(const char *constraint) {
	return GetJobByConstraint(constraint);
}

JobQueueJob *
GetNextJob(int initScan)
{
	return GetNextJobByConstraint(NULL, initScan);
}


JobQueueJob *
GetNextJobByConstraint(const char *constraint, int initScan)
{
	JobQueueJob *ad;
	JobQueueKey key;

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->Iterate(key,ad)) {
		if ( key.cluster > 0 && key.proc >= 0 && // avoid cluster and header ads
			(!constraint || !constraint[0] || EvalExprBool(ad, constraint))) {
			return ad;
		}
	}
	return NULL;
}

JobQueueJob *
GetNextJobOrClusterByConstraint(const char *constraint, int initScan)
{
	JobQueueJob *ad;
	JobQueueKey key;

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->Iterate(key,ad)) {
		if ( key.cluster > 0 && // skip the header ad
			(!constraint || !constraint[0] || EvalExprBool(ad, constraint))) {
			return ad;
		}
	}
	return NULL;
}

// declare this so that we don't try and pull in the one in send_stubs
ClassAd * GetNextJobByConstraint_as_ClassAd(const char *constraint, int initScan) {
	return GetNextJobByConstraint(constraint, initScan);
}

JobQueueJob *
GetNextDirtyJobByConstraint(const char *constraint, int initScan)
{
	JobQueueJob *ad = NULL;
	char *job_id_str;

	if (initScan) {
		DirtyJobIDs.rewind( );
	}

	while( (job_id_str = DirtyJobIDs.next( )) != NULL ) {
		JOB_ID_KEY job_id(job_id_str);
		if( !JobQueue->Lookup( job_id, ad ) ) {
			dprintf(D_ALWAYS, "Warning: Job %s is marked dirty, but could not find in the job queue.  Skipping\n", job_id_str);
			continue;
		}

		if ( !constraint || !constraint[0] || EvalExprBool(ad, constraint)) {
			return ad;
		}
	}
	return NULL;
}

JobQueueJob *
GetNextJobByCluster(int c, int initScan)
{
	JobQueueKey key;

	if ( c < 1 ) {
		return NULL;
	}

	JobQueueJob	*ad;

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->Iterate(key,ad)) {
		if ( c == key.cluster ) {
			return ad;
		}
	}

	return NULL;
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
	filesize_t	size;
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
	char * path;

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
	free(path); path = NULL;
	return rv;
}

int
SendSpoolFileIfNeeded(ClassAd& ad)
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

	// here we take advantage of ickpt sharing if possible. if a copy
	// of the executable already exists we make a link to it and send
	// a '1' back to the client. if that can't happen but sharing is
	// enabled, the hash variable will be set to a non-empty string that
	// can be used to create a link that can be shared by future jobs
	//
	std::string owner;
	std::string hash;
	if (param_boolean("SHARE_SPOOLED_EXECUTABLES", true)) {
		if (!ad.LookupString(ATTR_OWNER, owner)) {
			dprintf(D_ALWAYS,
			        "SendSpoolFileIfNeeded: no %s attribute in ClassAd\n",
			        ATTR_OWNER);
			Q_SOCK->getReliSock()->put(-1);
			Q_SOCK->getReliSock()->end_of_message();
			free(path);
			return -1;
		}
		if (!OwnerCheck(&ad, Q_SOCK->getOwner())) {
			dprintf(D_ALWAYS, "SendSpoolFileIfNeeded: OwnerCheck failure\n");
			Q_SOCK->getReliSock()->put(-1);
			Q_SOCK->getReliSock()->end_of_message();
			free(path);
			return -1;
		}
		hash = ickpt_share_get_hash(ad);
		if (!hash.empty()) {
			std::string s = std::string("\"") + hash + "\"";
			int rv = SetAttribute(active_cluster_num,
			                      -1,
			                      ATTR_JOB_CMD_HASH,
			                      s.c_str());
			if (rv < 0) {
					dprintf(D_ALWAYS,
					        "SendSpoolFileIfNeeded: unable to set %s to %s\n",
					        ATTR_JOB_CMD_HASH,
					        hash.c_str());
					hash = "";
			}

			MyString cluster_owner;
			if( GetAttributeString(active_cluster_num,-1,ATTR_OWNER,cluster_owner) == -1 ) {
					// The owner is not set in the cluster ad.  We
					// need it to be set so we can attempt to clean up
					// the shared file when the cluster goes away.
					// Setting the owner in the cluster ad to whatever
					// it is in the ad we were given should be okay.
					// If any other procs in this cluster have a
					// different value for Owner, the cleanup will not
					// be complete, but the files should eventually be
					// cleaned by preen.  It would probably be a good
					// idea to enforce the rule that all jobs in a
					// cluster have the same Owner, but that is outside
					// the scope of the code here.

				rv = SetAttributeString(active_cluster_num,
			                      -1,
			                      ATTR_OWNER,
			                      owner.c_str());

				if (rv < 0) {
					dprintf(D_ALWAYS,
					        "SendSpoolFileIfNeeded: unable to set %s to %s\n",
					        ATTR_OWNER,
					        owner.c_str());
					hash = "";
				}
			}

			if (!hash.empty() &&
			    ickpt_share_try_sharing(owner.c_str(), hash, path))
			{
				Q_SOCK->getReliSock()->put(1);
				Q_SOCK->getReliSock()->end_of_message();
				free(path);
				return 0;
			}
		}
	}

	/* Tell client to go ahead with file transfer. */
	Q_SOCK->getReliSock()->put(0);
	Q_SOCK->getReliSock()->end_of_message();

	if (RecvSpoolFileBytes(path) == -1) {
		free(path); path = NULL;
		return -1;
	}

	if (!hash.empty()) {
		ickpt_share_init_sharing(owner.c_str(), hash, path);
	}

	free(path); path = NULL;
	return 0;
}

} /* should match the extern "C" */


void
PrintQ()
{
	ClassAd		*ad=NULL;

	dprintf(D_ALWAYS, "*******Job Queue*********\n");
	JobQueue->StartIterateAllClassAds();
	while(JobQueue->IterateAllClassAds(ad)) {
		fPrintAd(stdout, *ad);
	}
	dprintf(D_ALWAYS, "****End of Queue*********\n");
}

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

// Returns cur_hosts so that another function in the scheduler can
// update JobsRunning and keep the scheduler and queue manager
// seperate. 
int get_job_prio(JobQueueJob *job, const JOB_ID_KEY & jid, void *)
{
    int     job_prio = 0, 
            pre_job_prio1, 
            pre_job_prio2, 
            post_job_prio1, 
            post_job_prio2;
    int     job_status;
    int     q_date;
    char    owner[100];
    int     cur_hosts;
    int     max_hosts;
    int     niceUser;
    int     universe;

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

	job->LookupInteger(ATTR_JOB_UNIVERSE, universe);
	ASSERT(universe == job->Universe());

	job->LookupInteger(ATTR_JOB_STATUS, job_status);
    if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) == 0) {
        cur_hosts = ((job_status == SUSPENDED || job_status == RUNNING || job_status == TRANSFERRING_OUTPUT) ? 1 : 0);
    }
    if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) == 0) {
        max_hosts = ((job_status == IDLE) ? 1 : 0);
    }
	// Figure out if we should contine and put this job into the PrioRec array
	// or not.
    // No longer judge whether or not a job can run by looking at its status.
    // Rather look at if it has all the hosts that it wanted.
    if (cur_hosts>=max_hosts || job_status==HELD || 
			job_status==REMOVED || job_status==COMPLETED ||
			job->IsNoopJob() ||
			!service_this_universe(universe,job) ||
			scheduler.AlreadyMatched(job, job->Universe()))
	{
        return cur_hosts;
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
    job->LookupInteger(ATTR_Q_DATE, q_date);

	char * powner = owner;
	int cremain = sizeof(owner);
	if( job->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
		strcpy(powner,NiceUserName);
		strcat(powner,".");
		int cch = (int)strlen(powner);
		powner += cch;
		cremain -= cch;
	}
		// Note, we should use this method instead of just looking up
		// ATTR_USER directly, since that includes UidDomain, which we
		// don't want for this purpose...
	job->LookupString(ATTR_ACCOUNTING_GROUP, powner, cremain);  // TODDCORE
	if (*powner == '\0') {
		job->LookupString(ATTR_OWNER, powner, cremain);
	}

    PrioRec[N_PrioRecs].id             = jid;
    PrioRec[N_PrioRecs].job_prio       = job_prio;
    PrioRec[N_PrioRecs].pre_job_prio1  = pre_job_prio1;
    PrioRec[N_PrioRecs].pre_job_prio2  = pre_job_prio2;
    PrioRec[N_PrioRecs].post_job_prio1 = post_job_prio1;
    PrioRec[N_PrioRecs].post_job_prio2 = post_job_prio2;
    PrioRec[N_PrioRecs].status         = job_status;
    PrioRec[N_PrioRecs].qdate          = q_date;
	if ( auto_id == -1 ) {
		PrioRec[N_PrioRecs].auto_cluster_id = jid.cluster;
	} else {
		PrioRec[N_PrioRecs].auto_cluster_id = auto_id;
	}

	strcpy(PrioRec[N_PrioRecs].owner,owner);

    N_PrioRecs += 1;
	if ( N_PrioRecs == MAX_PRIO_REC ) {
		grow_prio_recs( 2 * N_PrioRecs );
	}

	return cur_hosts;
}

bool
jobLeaseIsValid( ClassAd* job, int cluster, int proc )
{
	int last_renewal, duration;
	time_t now;
	if( ! job->LookupInteger(ATTR_JOB_LEASE_DURATION, duration) ) {
		return false;
	}
	if( ! job->LookupInteger(ATTR_LAST_JOB_LEASE_RENEWAL, last_renewal) ) {
		return false;
	}
	now = time(0);
	int diff = now - last_renewal;
	int remaining = duration - diff;
	dprintf( D_FULLDEBUG, "%d.%d: %s is defined: %d\n", cluster, proc, 
			 ATTR_JOB_LEASE_DURATION, duration );
	dprintf( D_FULLDEBUG, "%d.%d: now: %d, last_renewal: %d, diff: %d\n", 
			 cluster, proc, (int)now, last_renewal, diff );

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

int mark_idle(JobQueueJob *job, const JobQueueKey& /*key*/, void* pvArg)
{
		// Update ATTR_SCHEDD_BIRTHDATE in job ad at startup
		// pointer to birthday is passed as an argument...
	time_t * pbDay = (time_t*)pvArg;
	job->Assign(ATTR_SCHEDD_BIRTHDATE, *pbDay);

	std::string managed_status;
	job->LookupString(ATTR_JOB_MANAGED, managed_status);
	if ( managed_status == MANAGED_EXTERNAL ) {
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

	int status, hosts;
	job->LookupInteger(ATTR_JOB_STATUS, status);
	job->LookupInteger(ATTR_CURRENT_HOSTS, hosts);

	if ( status == COMPLETED ) {
		DestroyProc(cluster,proc);
	} else if ( status == REMOVED ) {
		// a globus job with a non-null contact string should be left alone
		if ( universe == CONDOR_UNIVERSE_GRID ) {
			if ( job->LookupString( ATTR_GRID_JOB_ID, NULL, 0 ) )
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
			bool result;
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
		float wall_clock = 0.0;
		GetAttributeFloat(cluster,proc,ATTR_JOB_REMOTE_WALL_CLOCK,&wall_clock);
		wall_clock += wall_clock_ckpt;
		BeginTransaction();
		SetAttributeFloat(cluster,proc,ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock);
		DeleteAttribute(cluster,proc,ATTR_JOB_WALL_CLOCK_CKPT);
			// remove shadow birthdate so if CkptWallClock()
			// runs before a new shadow starts, it won't
			// potentially double-count
		DeleteAttribute(cluster,proc,ATTR_SHADOW_BIRTHDATE);

		float slot_weight = 1;
		GetAttributeFloat(cluster, proc,
						  ATTR_JOB_MACHINE_ATTR_SLOT_WEIGHT0,&slot_weight);
		float slot_time = 0;
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

void
WalkJobQueue3(queue_classad_scan_func func, void* pv, schedd_runtime_probe & ftm)
{
	double begin = _condor_debug_get_time_double();
	ClassAd *ad;
	int rval = 0;

	if( in_walk_job_queue ) {
		dprintf(D_ALWAYS,"ERROR: WalkJobQueue called recursively!  Generating stack trace:\n");
		dprintf_dump_stack();
	}

	in_walk_job_queue++;

	ad = GetNextJob(1);
	while (ad != NULL && rval >= 0) {
		rval = func(ad, pv);
		if (rval >= 0) {
			FreeJobAd(ad);
			ad = GetNextJob(0);
		}
	}
	if (ad != NULL)
		FreeJobAd(ad);

	double runtime = _condor_debug_get_time_double() - begin;
	ftm += runtime;
	WalkJobQ_runtime += runtime;

	in_walk_job_queue--;
}


// this function for use only inside the schedd, external clients will use the one above...
void
WalkJobQueue3(queue_job_scan_func func, void* pv, schedd_runtime_probe & ftm)
{
	double begin = _condor_debug_get_time_double();

	if( in_walk_job_queue ) {
		dprintf(D_ALWAYS,"ERROR: WalkJobQueue called recursively!  Generating stack trace:\n");
		dprintf_dump_stack();
	}

	in_walk_job_queue++;

	JobQueue->StartIterateAllClassAds();

	JobQueueKey key;
	JobQueueJob * job;
	while(JobQueue->Iterate(key, job)) {
		if (key.cluster <= 0 || key.proc < 0) // avoid cluster and header ads
			continue;
		int rval = func(job, key, pv);
		if (rval < 0)
			break;
	}

	double runtime = _condor_debug_get_time_double() - begin;
	ftm += runtime;
	WalkJobQ_runtime += runtime;

	in_walk_job_queue--;
}


int dump_job_q_stats(int cat)
{
	HashTable<JobQueueKey,JobQueueJob*>* table = JobQueue->Table();
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
	time_t bDay = time(NULL);
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

	std::string submit_digest;

	dprintf(D_ALWAYS, "Reloading job factories\n");
	int num_loaded = 0;
	int num_failed = 0;
	int num_paused = 0;

	JobQueueJob *ad = NULL;
	JobQueueKey key;
	while(JobQueue->Iterate(key,ad)) {
		if ( key.cluster <= 0 || key.proc >= 0 ) { continue; } // ingnore header and job ads

		ASSERT(ad->IsCluster());
		JobQueueCluster * clusterad = static_cast<JobQueueCluster*>(ad);
		if (clusterad->factory) { continue; } // ignore if factory already loaded

		submit_digest.clear();
		if (clusterad->LookupString(ATTR_JOB_MATERIALIZE_DIGEST_FILE, submit_digest)) {

			// we need to let MakeJobFactory know whether the digest has been spooled or not
			// because it needs to know whether to impersonate the user or not.
			MyString spooled_filename;
			GetSpooledSubmitDigestPath(spooled_filename, clusterad->jid.cluster, Spool);
			bool spooled_digest = YourStringNoCase(spooled_filename) == submit_digest;

			std::string errmsg;
			clusterad->factory = MakeJobFactory(clusterad, submit_digest.c_str(), spooled_digest, errmsg);
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
					setJobFactoryPauseAndLog(clusterad, mmRunning, 0, NULL);
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
		// moment.  If so, we don't want to call qsort(), since that's
		// bad.  We can still try to find the owner in the Owners
		// array, since that's not that expensive, and we need it for
		// all the flocking logic at the end of this function.
		// Discovered by Derek Wright and insure-- on 2/28/01
	if( N_PrioRecs ) {
		qsort( (char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]),
			   (int(*)(const void*, const void*))prio_compar );
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
void BuildPrioRecArrayPeriodic()
{
	if ( time(NULL) >= PrioRecArrayTimeslice.getStartTime().tv_sec + PrioRecRebuildMaxInterval ) {
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
	JobQueueJob *ad;

	if (user && (strlen(user) == 0)) {
		user = NULL;
	}

	// this is true only when we are claiming the local startd
	// because the negotiator went missing for too long.
	bool match_any_user = (user == NULL) ? true : false;

	ASSERT(my_match_ad);

		// indicate failure by setting proc to -1.  do this now
		// so if we bail out early anywhere, we say we failed.
	jobid.proc = -1;	

	MyString owner = user;
	int at_sign_pos;
	int i;

		// We have been passed user, which is owner@uid.  We want just
		// owner, place a NULL at the '@'.

	at_sign_pos = owner.FindChar('@');
	if ( at_sign_pos >= 0 ) {
		owner.truncate(at_sign_pos);
	}

#ifdef USE_VANILLA_START
	std::string job_attr("JOB");
	bool eval_for_each_job = false;
	bool start_is_true = true;
	VanillaMatchAd vad;
	const OwnerInfo * powni = scheduler.lookup_owner_const(owner.Value());
	vad.Init(my_match_ad, powni, NULL);
	if ( ! scheduler.vanillaStartExprIsConst(vad, start_is_true)) {
		eval_for_each_job = true;
		if (IsDebugLevel(D_MATCH)) {
			std::string slotname = "<none>";
			if (my_match_ad) { my_match_ad->LookupString(ATTR_NAME, slotname); }
			dprintf(D_MATCH, "VANILLA_START is const %d for owner=%s, slot=%s\n", start_is_true, owner.Value(), slotname.c_str());
		}
	} else if ( ! start_is_true) {
		// START_VANILLA is const and false, no job will ever match, nothing more to do
		return;
	}
#endif

	bool rebuilt_prio_rec_array = BuildPrioRecArray();


		// Iterate through the most recently constructed list of
		// jobs, nicely pre-sorted in priority order.

	do {
		for (i=0; i < N_PrioRecs; i++) {

			if ( PrioRec[i].owner[0] == '\0' ) {
					// This record has been disabled, because it is no longer
					// runnable.
				continue;
			}

			if ( !match_any_user && strcmp(PrioRec[i].owner,owner.Value()) != 0 ) {
					// Owner doesn't match.
				continue;
			}

			ad = GetJobAd( PrioRec[i].id.cluster, PrioRec[i].id.proc );
			if (!ad) {
					// This ad must have been deleted since we last built
					// runnable job list.
				continue;
			}	

			int junk; // don't care about the value
			if ( PrioRecAutoClusterRejected->lookup( PrioRec[i].auto_cluster_id, junk ) == 0 ) {
					// We have already failed to match a job from this same
					// autocluster with this machine.  Skip it.
				continue;
			}

			int isRunnable = Runnable(&PrioRec[i].id);
			int isMatched = scheduler.AlreadyMatched(&PrioRec[i].id);
			if( !isRunnable || isMatched ) {
					// This job's status must have changed since the
					// time it was added to the runnable job list.
					// Prevent this job from being considered in any
					// future iterations through the list.
				PrioRec[i].owner[0] = '\0';
				dprintf(D_FULLDEBUG,
						"record for job %d.%d skipped until PrioRec rebuild (%s)\n",
						PrioRec[i].id.cluster, PrioRec[i].id.proc, isRunnable ? "already matched" : "no longer runnable");

					// Move along to the next job in the prio rec array
				continue;
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
				PrioRecAutoClusterRejected->insert( PrioRec[i].auto_cluster_id, 1 );
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

			float current_startd_rank;
			if( my_match_ad &&
				my_match_ad->LookupFloat(ATTR_CURRENT_RANK, current_startd_rank) )
			{
				float new_startd_rank = 0;
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
						insert(PrioRec[i].auto_cluster_id, 1);
					continue;
				}
			}

			jobid = PrioRec[i].id; // success!
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

int Runnable(JobQueueJob *job, const char *& reason)
{
	int status, universe, cur = 0, max = 1;

	if ( ! job || ! job->IsJob())
	{
		reason = "not runnable (not found)";
		return FALSE;
	}

	if (job->IsNoopJob())
	{
		//dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (IsNoopJob)\n");
		reason = "not runnable (IsNoopJob)";
		return FALSE;
	}

	if ( job->LookupInteger(ATTR_JOB_STATUS, status) == 0 )
	{
		//dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",ATTR_JOB_STATUS);
		reason = "not runnable (no " ATTR_JOB_STATUS ")";
		return FALSE;
	}
	if (status == HELD)
	{
		//dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (HELD)\n");
		reason = "not runnable (HELD)";
		return FALSE;
	}
	if (status == REMOVED)
	{
		// dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (REMOVED)\n");
		reason = "not runnable (REMOVED)";
		return FALSE;
	}
	if (status == COMPLETED)
	{
		// dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (COMPLETED)\n");
		reason = "not runnable (COMPLETED)";
		return FALSE;
	}


	if ( job->LookupInteger(ATTR_JOB_UNIVERSE, universe) == 0 )
	{
		//dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n", ATTR_JOB_UNIVERSE);
		reason = "not runnable (no " ATTR_JOB_UNIVERSE ")";
		return FALSE;
	}
	if( !service_this_universe(universe,job) )
	{
		//dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (Universe=%s)\n", CondorUniverseName(universe) );
		reason = "not runnable (universe not in service)";
		return FALSE;
	}

	job->LookupInteger(ATTR_CURRENT_HOSTS, cur);
	job->LookupInteger(ATTR_MAX_HOSTS, max);

	if (cur < max)
	{
		// dprintf (D_FULLDEBUG | D_NOHEADER, " is runnable\n");
		reason = "is runnable";
		return TRUE;
	}
	
	//dprintf (D_FULLDEBUG | D_NOHEADER, " not runnable (default rule)\n");
	reason = "not runnable (default rule)";
	return FALSE;
}

int Runnable(PROC_ID* id)
{
	const char * reason = "";
	int runnable = Runnable(GetJobAd(id->cluster,id->proc), reason);
	dprintf (D_FULLDEBUG, "Job %d.%d: %s\n", id->cluster, id->proc, reason);
	return runnable;
}

#if 0 // not used
// From the priority records, find the runnable job with the highest priority
// use the function prio_compar. By runnable I mean that its status is IDLE.
void FindPrioJob(PROC_ID & job_id)
{
	int			i;								// iterator over all prio rec
	int			flag = FALSE;

	// Compare each job in the priority record list with the first job in the
	// list. If the first job is of lower priority, replace the first job with
	// the job it is compared against.
	if(!Runnable(&PrioRec[0].id))
	{
		for(i = 1; i < N_PrioRecs; i++)
		{
			if(Runnable(&PrioRec[i].id))
			{
				PrioRec[0] = PrioRec[i];
				flag = TRUE;
				break;
			}
		}
		if(!flag)
		{
			job_id.proc = -1;
			return;
		}
	}
	for(i = 1; i < N_PrioRecs; i++)
	{
		if( (PrioRec[0].id.proc == PrioRec[i].id.proc) &&
			(PrioRec[0].id.cluster == PrioRec[i].id.cluster) )
		{
			continue;
		}
		if(prio_compar(&PrioRec[0], &PrioRec[i])!=-1&&Runnable(&PrioRec[i].id))
		{
			PrioRec[0] = PrioRec[i];
		}
	}
	job_id.proc = PrioRec[0].id.proc;
	job_id.cluster = PrioRec[0].id.cluster;
}
#endif

void
dirtyJobQueue()
{
	JobQueueDirty = true;
}

int GetJobQueuedCount() {
    return job_queued_count;
}
