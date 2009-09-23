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


#include "condor_common.h"
#include "condor_io.h"
#include "string_list.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"

#include "basename.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "log_transaction.h"
#include "log.h"
#include "classad_collection.h"
#include "prio_rec.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_ckpt_name.h"
#include "scheduler.h"	// for shadow_rec definition
#include "dedicated_scheduler.h"
#include "condor_email.h"
#include "condor_universe.h"
#include "globus_utils.h"
#include "env.h"
#include "condor_classad_util.h"
#include "condor_ver_info.h"
#include "condor_scanner.h"	// for Token, etc.
#include "condor_string.h" // for strnewp, etc.
#include "utc_time.h"
#include "condor_crontab.h"
#include "forkwork.h"
#include "condor_open.h"
#include "ickpt_share.h"
#include "classadHistory.h"

#if HAVE_DLOPEN
#include "ScheddPlugin.h"
#endif

#include "file_sql.h"
extern FILESQL *FILEObj;

extern char *Spool;
extern char *Name;
extern Scheduler scheduler;
extern DedicatedScheduler dedicated_scheduler;

extern "C" {
	int	prio_compar(prio_rec*, prio_rec*);
}

extern	int		Parse(const char*, ExprTree*&);
extern  void    cleanup_ckpt_files(int, int, const char*);
extern	bool	service_this_universe(int, ClassAd *);
static QmgmtPeer *Q_SOCK = NULL;

// Hash table with an entry for every job owner that
// has existed in the queue since this schedd has been
// running.  Used by SuperUserAllowedToSetOwnerTo().
static HashTable<MyString,int> owner_history(MyStringHash);

int		do_Q_request(ReliSock *,bool &may_fork);
void	FindPrioJob(PROC_ID &);

static bool qmgmt_was_initialized = false;
static ClassAdCollection *JobQueue = 0;
static int next_cluster_num = -1;
static int next_proc_num = 0;
static int active_cluster_num = -1;	// client is restricted to only insert jobs to the active cluster
static int old_cluster_num = -1;	// next_cluster_num at start of transaction
static bool JobQueueDirty = false;
static time_t xact_start_time = 0;	// time at which the current transaction was started
static int cluster_initial_val = 1;		// first cluster number to use
static int cluster_increment_val = 1;	// increment for cluster numbers of successive submissions 

static void AddOwnerHistory(const MyString &user);

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

static int 	MAX_PRIO_REC=INITIAL_MAX_PRIO_REC ;	// INITIAL_MAX_* in prio_rec.h

const char HeaderKey[] = "0.0";

ForkWork schedd_forker;

// Create a hash table which, given a cluster id, tells how
// many procs are in the cluster
static inline unsigned int compute_clustersize_hash(const int &key) {
	return key;
}
typedef HashTable<int, int> ClusterSizeHashTable_t;
static ClusterSizeHashTable_t *ClusterSizeHashTable = 0;
static int TotalJobsCount = 0;

static int flush_job_queue_log_timer_id = -1;
static int flush_job_queue_log_delay = 0;
static int HandleFlushJobQueueLogTimer(Service *);
static void ScheduleJobQueueLogFlush();

static bool qmgmt_all_users_trusted = false;
static char	**super_users = NULL;
static int	num_super_users = 0;
static char *default_super_user =
#if defined(WIN32)
	"Administrator";
#else
	"root";
#endif

//static int allow_remote_submit = FALSE;

static inline
void
IdToStr(int cluster, int proc, char *buf)
{
	ProcIdToStr(cluster,proc,buf);
}

static
void
StrToId(const char *str,int & cluster,int & proc)
{
	if( !StrToProcId(str,cluster,proc) ) {
		EXCEPT("Qmgmt: Malformed key - '%s'",str);
	}
}

static
void
ClusterCleanup(int cluster_id)
{
	char key[PROC_ID_STR_BUFLEN];
	IdToStr(cluster_id,-1,key);

	// pull out the owner and hash used for ickpt sharing
	MyString hash;
	GetAttributeString(cluster_id, -1, ATTR_JOB_CMD_HASH, hash);
	MyString owner;
	GetAttributeString(cluster_id, -1, ATTR_OWNER, owner);

	// remove entry in ClusterSizeHashTable 
	ClusterSizeHashTable->remove(cluster_id);

	// delete the cluster classad
	JobQueue->DestroyClassAd( key );

	// blow away the initial checkpoint file from the spool dir
	char *ckpt_file_name = gen_ckpt_name( Spool, cluster_id, ICKPT, 0 );
	(void)unlink( ckpt_file_name );

	// garbage collect the shared ickpt file if necessary
	if (!hash.IsEmpty()) {
		ickpt_share_try_removal(owner.Value(), hash.Value());
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
	TotalJobsCount++;

		// return the number of procs in this cluster
	if ( numOfProcs ) {
		return *numOfProcs;
	} else {
		return 1;
	}
}

static
int
DecrementClusterSize(int cluster_id)
{
	int 	*numOfProcs = NULL;

	if ( ClusterSizeHashTable->lookup(cluster_id,numOfProcs) != -1 ) {
		// We've seen this cluster_num go by before; increment proc count
		// NOTICE that numOfProcs is a _reference_ to an int which we
		// fetched out of the hash table via the call to lookup() above.
		(*numOfProcs)--;

		// If this is the last job in the cluster, remove the initial
		//    checkpoint file and the entry in the ClusterSizeHashTable.
		if ( *numOfProcs == 0 ) {
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
		// Convert old job ads to the new format of GridResource
		// and GridJobId. This switch happened on the V6_7-lease
		// branch and should be merged in around the time of 6.7.11.
		// At some future point in time, this code should be
		// removed (sometime in the 6.9 series). - jfrey Aug-11-05
	if ( universe == CONDOR_UNIVERSE_GRID ) {
		MyString grid_type = "gt2";
		MyString grid_resource = "";
		MyString grid_job_id = "";

		job_ad->LookupString( ATTR_JOB_GRID_TYPE, grid_type );
		job_ad->LookupString( ATTR_GRID_RESOURCE, grid_resource );
		job_ad->LookupString( ATTR_GRID_JOB_ID, grid_job_id );

		grid_type.lower_case();

		if ( grid_type == "gt2" || grid_type == "gt3" ||
			 grid_type == "gt4" || grid_type == "nordugrid" ||
			 grid_type == "oracle" || grid_type == "globus" ) {

			MyString attr = "";
			MyString new_value = "";

			if ( grid_type == "globus" ) {
				grid_type = "gt2";
			}

			if ( grid_resource.IsEmpty() &&
				 job_ad->LookupString( ATTR_GLOBUS_RESOURCE, attr ) ) {

				new_value.sprintf( "%s %s", grid_type.Value(),
								   attr.Value() );
				if ( grid_type == "gt4" ) {
					attr = "Fork";
					job_ad->LookupString( ATTR_GLOBUS_JOBMANAGER_TYPE,
										  attr );
					new_value.sprintf_cat( " %s", attr.Value() );
				}
				job_ad->Assign( ATTR_GRID_RESOURCE, new_value.Value() );
				JobQueueDirty = true;
			}

			if ( startup && grid_job_id.IsEmpty() &&
				 job_ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, attr ) ) {

				if ( attr != NULL_JOB_CONTACT ) {

					if ( grid_type == "gt2" ) {
							// For GT2, we need to include the
							// resource name in the job id (so that
							// we can restart the jobmanager if it
							// crashes). If it's undefined, we're
							// hosed anyway, so don't convert it.

							// Ugly: If the job is using match-making,
							// we need to grab the match-substituted
							// version of GlobusResource. That means
							// calling GetJobAd().
						MyString resource;
						ClassAd *job_ad2 = GetJobAd( cluster, proc,
													true, false );
						if ( !job_ad2 ||
							 !job_ad2->LookupString(
													ATTR_GLOBUS_RESOURCE,
													resource ) ) {
							dprintf( D_ALWAYS, "Warning: %s undefined"
									 " when converting %s to %s, not"
									 " converting\n",
									 ATTR_GRID_RESOURCE,
									 ATTR_GLOBUS_CONTACT_STRING,
									 ATTR_GRID_JOB_ID );
						} else {
							new_value.sprintf( "%s %s %s",
											   grid_type.Value(),
											   resource.Value(),
											   attr.Value() );
							job_ad->Assign( ATTR_GRID_JOB_ID,
											new_value.Value() );
						}
						if ( job_ad2 ) {
							delete job_ad2;
						}
					} else {
						new_value.sprintf( "%s %s", grid_type.Value(),
										   attr.Value() );
						job_ad->Assign( ATTR_GRID_JOB_ID,
										new_value.Value() );
					}

					job_ad->AssignExpr( ATTR_GLOBUS_CONTACT_STRING,
										"Undefined" );
					JobQueueDirty = true;
				}
			}
		}

		if ( grid_type == "condor" ) {

			static char *local_pool = NULL;

			if ( local_pool == NULL ) {
				DCCollector collector;
				local_pool = strdup( collector.name() );
			}

			MyString schedd;
			MyString pool = local_pool;
			MyString jobid = "";
			MyString new_value;

			if ( grid_resource.IsEmpty() &&
				 job_ad->LookupString( ATTR_REMOTE_SCHEDD, schedd ) ) {

				job_ad->LookupString( ATTR_REMOTE_POOL, pool );
				new_value.sprintf( "condor %s %s", schedd.Value(),
								   pool.Value() );
				job_ad->Assign( ATTR_GRID_RESOURCE, new_value.Value() );
				JobQueueDirty = true;
			}

			if ( startup && grid_job_id.IsEmpty() &&
				 job_ad->LookupString( ATTR_REMOTE_JOB_ID, jobid ) ) {

					// Ugly: If the job is using match-making, we
					// need to grab the match-substituted versions
					// of RemoteSchedd and RemotePool to stuff into
					// GridJobId. That means calling GetJobAd().
				ClassAd *job_ad2;

				job_ad2 = GetJobAd( cluster, proc, true, false );

				if ( job_ad2 ) {
					schedd = "";
					pool = "";
					job_ad2->LookupString( ATTR_REMOTE_SCHEDD, schedd );
					job_ad2->LookupString( ATTR_REMOTE_POOL, pool );
					new_value.sprintf( "condor %s %s %s", schedd.Value(),
									   pool.Value(), jobid.Value() );
					job_ad->Assign( ATTR_GRID_JOB_ID,
									new_value.Value() );
					job_ad->AssignExpr( ATTR_REMOTE_JOB_ID, "Undefined" );
					JobQueueDirty = true;

					delete job_ad2;

				}
			}

		}

		if ( grid_type == "infn" || grid_type == "blah" ) {

			MyString jobid;

			if ( grid_resource.IsEmpty() ) {

				job_ad->Assign( ATTR_GRID_RESOURCE, "blah" );
				JobQueueDirty = true;
			}

			if ( startup && grid_job_id.IsEmpty() &&
				 job_ad->LookupString( ATTR_REMOTE_JOB_ID, jobid ) ) {

				MyString new_value;

				new_value.sprintf( "blah %s", jobid.Value() );
				job_ad->Assign( ATTR_GRID_JOB_ID, new_value.Value() );
				job_ad->AssignExpr( ATTR_REMOTE_JOB_ID, "Undefined" );
				JobQueueDirty = true;
			}
		}
	}

		// CRUFT
		// Starting in 6.7.11, ATTR_JOB_MANAGED changed from a boolean
		// to a string.
	int ntmp;
	if( startup && job_ad->LookupBool(ATTR_JOB_MANAGED, ntmp) ) {
		if(ntmp) {
			job_ad->Assign(ATTR_JOB_MANAGED, MANAGED_EXTERNAL);
		} else {
			job_ad->Assign(ATTR_JOB_MANAGED, MANAGED_SCHEDD);
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
QmgmtPeer::set(const struct sockaddr_in *s, const char *o)
{
	if ( !s || sock || myendpoint ) {
		// already set, or no input
		return false;
	}
	ASSERT(owner == NULL);

	if ( o ) {
		fquser = strnewp(o);
			// owner is just fquser that stops at the first '@' 
		owner = strnewp(o);
		char *atsign = strchr(owner,'@');
		if (atsign) {
			*atsign = '\0';
		}
	}

	if ( s ) {
		memcpy(&sockaddr,s,sizeof(struct sockaddr_in));
		myendpoint = strnewp( inet_ntoa(s->sin_addr) );
	}

	return true;
}

void
QmgmtPeer::unset()
{
	if (owner) {
		delete owner;
		owner = NULL;
	}
	if (fquser) {
		delete fquser;
		fquser = NULL;
	}
	if (myendpoint) {
		delete [] myendpoint;
		myendpoint = NULL;
	}
	if (sock) sock=NULL;	// note: do NOT delete sock!!!!!
	if (transaction) {
		delete transaction;
		transaction = NULL;
	}

	next_proc_num = 0;
	active_cluster_num = -1;	
	old_cluster_num = -1;	// next_cluster_num at start of transaction
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

const struct sockaddr_in*
QmgmtPeer::endpoint() const
{
	if ( sock ) {
		return sock->peer_addr();
	} else {
		return &sockaddr;
	}
}


const char*
QmgmtPeer::getOwner() const
{
	if ( sock ) {
		return sock->getOwner();
	} else {
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
		return sock->triedAuthentication();
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

	if( DebugFlags & D_FULLDEBUG ) {
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

	schedd_forker.Initialize();
	int max_schedd_forkers = param_integer ("SCHEDD_QUERY_WORKERS",3,0);
	schedd_forker.setMaxWorkers( max_schedd_forkers );

	cluster_initial_val = param_integer("SCHEDD_CLUSTER_INITIAL_VALUE",1,1);
	cluster_increment_val = param_integer("SCHEDD_CLUSTER_INCREMENT_VALUE",1,1);

	flush_job_queue_log_delay = param_integer("SCHEDD_JOB_QUEUE_LOG_FLUSH_DELAY",5,0);
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


void
InitJobQueue(const char *job_queue_name,int max_historical_logs)
{
	ASSERT(qmgmt_was_initialized);	// make certain our parameters are setup
	ASSERT(!JobQueue);
	JobQueue = new ClassAdCollection(job_queue_name,max_historical_logs);
	ClusterSizeHashTable = new ClusterSizeHashTable_t(37,compute_clustersize_hash);
	TotalJobsCount = 0;

	/* We read/initialize the header ad in the job queue here.  Currently,
	   this header ad just stores the next available cluster number. */
	ClassAd *ad;
	ClassAd *clusterad;
	HashKey key;
	int 	cluster_num, cluster, proc, universe;
	int		stored_cluster_num;
	bool	CreatedAd = false;
	char	cluster_str[PROC_ID_STR_BUFLEN];
	MyString	owner;
	MyString	user;
	MyString	correct_user;
	MyString	buf;
	MyString	attr_scheduler;
	MyString	correct_scheduler;
	MyString buffer;

	if (!JobQueue->LookupClassAd(HeaderKey, ad)) {
		// we failed to find header ad, so create one
//		LogNewClassAd *log = new LogNewClassAd(HeaderKey, JOB_ADTYPE,
//											   STARTD_ADTYPE);
//		JobQueue->AppendLog(log);
		JobQueue->NewClassAd(HeaderKey, JOB_ADTYPE, STARTD_ADTYPE);
		CreatedAd = true;
	}

	if (CreatedAd ||
		ad->LookupInteger(ATTR_NEXT_CLUSTER_NUM, stored_cluster_num) != 1) {
		// cluster_num is not already set, so we set a flag to set it from a
		// computed value 
		stored_cluster_num = 0;
	}

		// Figure out what the correct ATTR_SCHEDULER is for any
		// dedicated jobs in this queue.  Since it'll be the same for
		// all jobs, we only have to figure it out once.  We use '%'
		// as the delimiter, since ATTR_NAME might already have '@' in
		// it, and we don't want to confuse things any further.
	correct_scheduler.sprintf( "DedicatedScheduler!%s", Name );

	next_cluster_num = cluster_initial_val;
	JobQueue->StartIterateAllClassAds();
	while (JobQueue->IterateAllClassAds(ad,key)) {
		const char *tmp = key.value();
		if ( *tmp == '0' ) continue;	// skip cluster & header ads
		if ( (cluster_num = atoi(tmp)) ) {

			// find highest cluster, set next_cluster_num to one increment higher
			if (cluster_num >= next_cluster_num) {
				next_cluster_num = cluster_num + cluster_increment_val;
			}

			// link all proc ads to their cluster ad, if there is one
			IdToStr(cluster_num,-1,cluster_str);
			if ( JobQueue->LookupClassAd(cluster_str,clusterad) ) {
				ad->ChainToAd(clusterad);
			}

			if (!ad->LookupString(ATTR_OWNER, owner)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						tmp, ATTR_OWNER);
				JobQueue->DestroyClassAd(tmp);
				continue;
			}

				// initialize our list of job owners
			AddOwnerHistory( owner );

			if (!ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						tmp, ATTR_CLUSTER_ID);
				JobQueue->DestroyClassAd(tmp);
				continue;
			}

			if (cluster != cluster_num) {
				dprintf(D_ALWAYS,
						"Job %s has invalid cluster number %d.  Removing...\n",
						tmp, cluster);
				JobQueue->DestroyClassAd(tmp);
				continue;
			}

			if (!ad->LookupInteger(ATTR_PROC_ID, proc)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						tmp, ATTR_PROC_ID);
				JobQueue->DestroyClassAd(tmp);
				continue;
			}

			if( !ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) ) {
				dprintf( D_ALWAYS,
						 "Job %s has no %s attribute.  Removing....\n",
						 tmp, ATTR_JOB_UNIVERSE );
				JobQueue->DestroyClassAd( tmp );
				continue;
			}

			if( universe <= CONDOR_UNIVERSE_MIN ||
				universe >= CONDOR_UNIVERSE_MAX ) {
				dprintf( D_ALWAYS,
						 "Job %s has invalid %s = %d.  Removing....\n",
						 tmp, ATTR_JOB_UNIVERSE, universe );
				JobQueue->DestroyClassAd( tmp );
				continue;
			}

				// Figure out what ATTR_USER *should* be for this job
			int nice_user = 0;
			ad->LookupInteger( ATTR_NICE_USER, nice_user );
			correct_user.sprintf( "%s%s@%s",
					 (nice_user) ? "nice-user." : "", owner.Value(),
					 scheduler.uidDomain() );

			if (!ad->LookupString(ATTR_USER, user)) {
				dprintf( D_FULLDEBUG,
						"Job %s has no %s attribute.  Inserting one now...\n",
						tmp, ATTR_USER);
				ad->Assign( ATTR_USER, correct_user.Value() );
				JobQueueDirty = true;
			} else {
					// ATTR_USER exists, make sure it's correct, and
					// if not, insert the new value now.
				if( user != correct_user ) {
						// They're different, so insert the right value
					dprintf( D_FULLDEBUG,
							 "Job %s has stale %s attribute.  "
							 "Inserting correct value now...\n",
							 tmp, ATTR_USER );
					ad->Assign( ATTR_USER, correct_user.Value() );
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
							 "Inserting one now...\n", tmp,
							 ATTR_SCHEDULER );
					ad->Assign( ATTR_SCHEDULER, correct_scheduler.Value() );
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
								 tmp, ATTR_SCHEDULER );
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

			// count up number of procs in cluster, update ClusterSizeHashTable
			IncrementClusterSize(cluster_num);

		}
	} // WHILE

	if ( stored_cluster_num == 0 ) {
		snprintf(cluster_str, PROC_ID_STR_BUFLEN, "%d", next_cluster_num);
//		LogSetAttribute *log = new LogSetAttribute(HeaderKey,
//												   ATTR_NEXT_CLUSTER_NUM,
//												   cluster_str);
//		JobQueue->AppendLog(log);
		JobQueue->SetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);
	} else {
		if ( next_cluster_num > stored_cluster_num ) {
			// Oh no!  Somehow the header ad in the queue says to reuse cluster nums!
			EXCEPT("JOB QUEUE DAMAGED; header ad NEXT_CLUSTER_NUM invalid");
		}
		next_cluster_num = stored_cluster_num;
	}
		// Some of the conversions done in ConvertOldJobAdAttrs need to be
		// persisted to disk. Particularly, GlobusContactString/RemoteJobId.
	CleanJobQueue();
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
		if( strcmp( user, super_users[i] ) == 0 ) {
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

	int junk = 0;
	if( owner_history.lookup(user,junk) != -1 ) {
		return true;
	}
	dprintf(D_FULLDEBUG,"Queue super user not allowed to set owner to %s, because this instance of the schedd has never seen that user submit any jobs.\n",user.Value());
	return false;
}

bool
OwnerCheck(int cluster_id,int proc_id)
{
	char				key[PROC_ID_STR_BUFLEN];
	ClassAd				*ad = NULL;

	if (!Q_SOCK) {
		return 0;
	}

	IdToStr(cluster_id,proc_id,key);
	if (!JobQueue->LookupClassAd(key, ad)) {
		return 0;
	}

	return OwnerCheck(ad, Q_SOCK->getOwner());
}

// Test if this owner matches my owner, so they're allowed to update me.
bool
OwnerCheck(ClassAd *ad, const char *test_owner)
{
	// check if the IP address of the peer has daemon core write permission
	// to the schedd.  we have to explicitly check here because all queue
	// management commands come in via one sole daemon core command which
	// has just READ permission.
	if ( daemonCore->Verify("queue management", WRITE, Q_SOCK->endpoint(), Q_SOCK->getFullyQualifiedUser()) == FALSE ) {
		// this machine does not have write permission; return failure
		return false;
	}

	return OwnerCheck2(ad, test_owner);
}


// This code actually checks the owner, and doesn't do a Verify!
bool
OwnerCheck2(ClassAd *ad, const char *test_owner)
{
	MyString	my_owner;

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

	// The super users are always allowed to do updates.  They are
	// specified with the "QUEUE_SUPER_USERS" string list in the
	// config file.  Defaults to root and condor.
	if( isQueueSuperUser(test_owner) ) {
		dprintf( D_FULLDEBUG, "OwnerCheck retval 1 (success), super_user\n" );
		return true;
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
	if( !ad ) {
		dprintf(D_FULLDEBUG,"OwnerCheck retval 1 (success),no ad\n");
		return true;
	}
	if( ad->LookupString(ATTR_OWNER, my_owner) == 0 ) {
		dprintf(D_FULLDEBUG,"OwnerCheck retval 1 (success),no owner\n");
		return true;
	}

		// Finally, compare the owner of the ad with the entity trying
		// to connect to the queue.
#if defined(WIN32)
	// WIN32: user names are case-insensitive
	if (strcasecmp(my_owner.Value(), test_owner) != 0) {
#else
	if (strcmp(my_owner.Value(), test_owner) != 0) {
#endif
		errno = EACCES;
		dprintf( D_FULLDEBUG, "ad owner: %s, queue submit owner: %s\n",
				my_owner.Value(), test_owner );
		return false;
	} 
	else {
		return true;
	}
}


QmgmtPeer*
getQmgmtConnectionInfo()
{
	QmgmtPeer* ret_value = NULL;

	// put all qmgmt state back into QmgmtPeer object for safe keeping
	if ( Q_SOCK ) {
		Q_SOCK->next_proc_num = next_proc_num;
		Q_SOCK->active_cluster_num = active_cluster_num;	
		Q_SOCK->old_cluster_num = old_cluster_num;
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
	if ( peer->old_cluster_num == -1 ) {
		// if old_cluster_num is uninitialized (-1), then
		// store the cluster num so when we commit the transaction, we can easily
		// see if new clusters have been submitted and thus make links to cluster ads
		peer->old_cluster_num = next_cluster_num;
	}
	old_cluster_num = peer->old_cluster_num;
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
	old_cluster_num = reset.old_cluster_num;
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

	JobQueue->BeginTransaction();

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

	char * password = "";

	if (GetMyProxyPassword (cluster_id, proc_id, &password) != 0) {
		// Try not specifying a proc
		if (GetMyProxyPassword (cluster_id, 0, &password) != 0) {
			//return -1;
			// Just return empty string if can't find password
		}
	}


	socket->eom();
	socket->encode();
	if( ! socket->code(password) ) {
		dprintf( D_ALWAYS,
			"get_myproxy_password_handler: Failed to send result.\n" );
		return -1;
	}

	if( ! socket->eom() ) {
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
//	LogSetAttribute *log;
	char cluster_str[PROC_ID_STR_BUFLEN];

	if( Q_SOCK && !OwnerCheck(NULL, Q_SOCK->getOwner()  ) ) {
		dprintf( D_FULLDEBUG, "NewCluser(): OwnerCheck failed\n" );
		return -1;
	}

		//
		// I have seen a weird bug where the getMaxJobsSubmitted() returns
		// zero. I added extra information to the dprintf statement 
		// to help me try to track down when this happens
		// Andy - 06.12.2006
		//
	int maxJobsSubmitted = scheduler.getMaxJobsSubmitted();
	if ( TotalJobsCount >= maxJobsSubmitted ) {
		dprintf(D_ALWAYS,
			"NewCluster(): MAX_JOBS_SUBMITTED exceeded, submit failed. "
			"Current total is %d. Limit is %d\n",
			TotalJobsCount, maxJobsSubmitted );
		return -2;
	}

	next_proc_num = 0;
	active_cluster_num = next_cluster_num;
	next_cluster_num += cluster_increment_val;
	snprintf(cluster_str, PROC_ID_STR_BUFLEN, "%d", next_cluster_num);
//	log = new LogSetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);
//	JobQueue->AppendLog(log);
	JobQueue->SetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);

	// put a new classad in the transaction log to serve as the cluster ad
	char cluster_key[PROC_ID_STR_BUFLEN];
	IdToStr(active_cluster_num,-1,cluster_key);
	JobQueue->NewClassAd(cluster_key, JOB_ADTYPE, STARTD_ADTYPE);

	return active_cluster_num;
}


int
NewProc(int cluster_id)
{
	int				proc_id;
	char			key[PROC_ID_STR_BUFLEN];
//	LogNewClassAd	*log;

	if( Q_SOCK && !OwnerCheck(NULL, Q_SOCK->getOwner() ) ) {
		return -1;
	}

	if ((cluster_id != active_cluster_num) || (cluster_id < 1)) {
#if !defined(WIN32)
		errno = EACCES;
#endif
		return -1;
	}
	
	if ( TotalJobsCount >= scheduler.getMaxJobsSubmitted() ) {
		dprintf(D_ALWAYS,
			"NewProc(): MAX_JOBS_SUBMITTED exceeded, submit failed\n");
		return -2;
	}

	proc_id = next_proc_num++;
	IdToStr(cluster_id,proc_id,key);
//	log = new LogNewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);
//	JobQueue->AppendLog(log);
	JobQueue->NewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);

	IncrementClusterSize(cluster_id);

		// now that we have a real job ad with a valid proc id, then
		// also insert the appropriate GlobalJobId while we're at it.
	MyString gjid = "\"";
	gjid += Name;             // schedd's name
	gjid += "#";
	gjid += cluster_id;
	gjid += ".";
	gjid += proc_id;
	if (param_boolean("GLOBAL_JOB_ID_WITH_TIME", true)) {
		int now = (int)time(0);
		gjid += "#";
		gjid += now;
	}
	gjid += "\"";
	JobQueue->SetAttribute( key, ATTR_GLOBAL_JOB_ID, gjid.Value() );

	return proc_id;
}

int 	DestroyMyProxyPassword (int cluster_id, int proc_id);

int DestroyProc(int cluster_id, int proc_id)
{
	char				key[PROC_ID_STR_BUFLEN];
	ClassAd				*ad = NULL;
//	LogDestroyClassAd	*log;

	IdToStr(cluster_id,proc_id,key);
	if (!JobQueue->LookupClassAd(key, ad)) {
		return DESTROYPROC_ENOENT;
	}

	// Only the owner can delete a proc.
	if ( Q_SOCK && !OwnerCheck(ad, Q_SOCK->getOwner() )) {
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
	int leave_job_in_q = 0;
	{
		ClassAd completeAd(*ad);
		JobQueue->AddAttrsFromTransaction(key,completeAd);
		completeAd.EvalBool(ATTR_JOB_LEAVE_IN_QUEUE,NULL,leave_job_in_q);
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

	int universe = CONDOR_UNIVERSE_STANDARD;
	ad->LookupInteger(ATTR_JOB_UNIVERSE, universe);

	if( (universe == CONDOR_UNIVERSE_PVM) || 
		(universe == CONDOR_UNIVERSE_MPI) ||
		(universe == CONDOR_UNIVERSE_PARALLEL) ) {
			// PVM jobs take up a whole cluster.  If we've been ask to
			// destroy any of the procs in a PVM job cluster, we
			// should destroy the entire cluster.  This hack lets the
			// schedd just destroy the proc associated with the shadow
			// when a multi-class PVM job exits without leaving other
			// procs in the cluster around.  It also ensures that the
			// user doesn't delete only some of the procs in the PVM
			// job cluster, since that's going to really confuse the
			// PVM shadow.
		int ret = DestroyCluster(cluster_id);
		if(ret < 0 ) { return DESTROYPROC_ERROR; }
		return DESTROYPROC_SUCCESS;
	}

	// Append to history file
	AppendHistory(ad);

	// Write a per-job history file (if PER_JOB_HISTORY_DIR param is set)
	WritePerJobHistoryFile(ad, false);

#if HAVE_DLOPEN
  ScheddPluginManager::Archive(ad);
#endif

  if (FILEObj->file_newEvent("History", ad) == QUILL_FAILURE) {
	  dprintf(D_ALWAYS, "AppendHistory Logging History Event --- Error\n");
  }

  // save job ad to the log
	bool already_in_transaction = InTransaction();
	if( !already_in_transaction ) {
		BeginTransaction(); // for performance
	}

	JobQueue->DestroyClassAd(key);

	DecrementClusterSize(cluster_id);

	if( !already_in_transaction ) {
		CommitTransaction();
	}

		// remove any match (startd) ad stored w/ this job
	RemoveMatchedAd(cluster_id,proc_id);


	// ckireyev: Destroy MyProxyPassword
	(void)DestroyMyProxyPassword (cluster_id, proc_id);

	JobQueueDirty = true;

	return DESTROYPROC_SUCCESS;
}


int DestroyCluster(int cluster_id, const char* reason)
{
	ClassAd				*ad=NULL;
	int					c, proc_id;
//	LogDestroyClassAd	*log;
	HashKey				key;

	// cannot destroy the header cluster(s)
	if ( cluster_id < 1 ) {
		return -1;
	}

	JobQueue->StartIterateAllClassAds();

	// Find all jobs in this cluster and remove them.
	while(JobQueue->IterateAllClassAds(ad,key)) {
		StrToId(key.value(),c,proc_id);
		if (c == cluster_id && proc_id > -1) {
				// Only the owner can delete a cluster
				if ( Q_SOCK && !OwnerCheck(ad, Q_SOCK->getOwner() )) {
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
				int leave_job_in_q = 0;
				ad->EvalBool(ATTR_JOB_LEAVE_IN_QUEUE,NULL,leave_job_in_q);
				if ( leave_job_in_q ) {
						// leave it in the queue.... move on to the next one
					continue;
				}

				// Apend to history file
				AppendHistory(ad);
#if HAVE_DLOPEN
				ScheddPluginManager::Archive(ad);
#endif

				if (FILEObj->file_newEvent("History", ad) == QUILL_FAILURE) {
			  		dprintf(D_ALWAYS, "AppendHistory Logging History Event --- Error\n");
		  		}

  // save job ad to the log

				// Write a per-job history file (if PER_JOB_HISTORY_DIR param is set)
				WritePerJobHistoryFile(ad, false);

//				log = new LogDestroyClassAd(key);
//				JobQueue->AppendLog(log);

				cleanup_ckpt_files(cluster_id,proc_id, NULL );

				JobQueue->DestroyClassAd(key.value());

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
SetAttributeByConstraint(const char *constraint, const char *attr_name,
						 const char *attr_value)
{
	ClassAd	*ad;
	int cluster_num, proc_num;
	int found_one = 0, had_error = 0;
	HashKey key;

	JobQueue->StartIterateAllClassAds();
	while(JobQueue->IterateAllClassAds(ad,key)) {
		// check for CLUSTER_ID>0 and proc_id >= 0 to avoid header ad/cluster ads
		StrToId(key.value(),cluster_num,proc_num);
		if ( (cluster_num > 0) &&
			 (proc_num > -1) &&
			 EvalBool(ad, constraint)) {
			found_one = 1;
			if( SetAttribute(cluster_num,proc_num,attr_name,attr_value) < 0 ) {
				had_error = 1;
			}
			FreeJobAd(ad);	// a no-op on the server side
		}
	}

		// If we couldn't find any jobs that matched the constraint,
		// or we could set the attribute on any of the ones we did
		// find, return error (-1).
	// failure (-1)
	if ( had_error || !found_one )
		return -1;
	else
		return 0;
}

int
SetAttribute(int cluster_id, int proc_id, const char *attr_name,
			 const char *attr_value, SetAttributeFlags_t flags )
{
//	LogSetAttribute	*log;
	char			key[PROC_ID_STR_BUFLEN];
	ClassAd			*ad = NULL;
	MyString		new_value;

	// Only an authenticated user or the schedd itself can set an attribute.
	if ( Q_SOCK && !(Q_SOCK->isAuthenticated()) ) {
		return -1;
	}

	// If someone is trying to do something funny with an invalid
	// attribute name, bail out early
	if (!AttrList::IsValidAttrName(attr_name)) {
		dprintf(D_ALWAYS, "SetAttribute got invalid attribute named %s for job %d.%d\n", 
			attr_name ? attr_name : "(null)", cluster_id, proc_id);
		return -1;
	}

		// If someone is trying to do something funny with an invalid
		// attribute value, bail earlyxs
	if (!AttrList::IsValidAttrValue(attr_value)) {
		dprintf(D_ALWAYS,
				"SetAttribute received invalid attribute value '%s' for "
				"job %d.%d, ignoring\n",
				attr_value ? attr_value : "(null)", cluster_id, proc_id);
		return -1;
	}

	IdToStr(cluster_id,proc_id,key);

	if (JobQueue->LookupClassAd(key, ad)) {
		if ( Q_SOCK && !OwnerCheck(ad, Q_SOCK->getOwner() )) {
			const char *owner = Q_SOCK->getOwner( );
			if ( ! owner ) {
				owner = "NULL";
			}
			dprintf(D_ALWAYS,
					"OwnerCheck(%s) failed in SetAttribute for job %d.%d\n",
					owner, cluster_id, proc_id);
			return -1;
		}
	} else {
		// If we made it here, the user is adding attributes to an ad
		// that has not been committed yet (and thus cannot be found
		// in the JobQueue above).  Restrict the user to only adding
		// attributes to the current cluster returned by NewCluster,
		// and also restrict the user to procs that have been 
		// returned by NewProc.
		if ((cluster_id != active_cluster_num) || (proc_id >= next_proc_num)) {
#if !defined(WIN32)
			errno = EACCES;
#endif
			dprintf(D_ALWAYS,"Inserting new attribute %s into non-active cluster cid=%d acid=%d\n", 
					attr_name, cluster_id,active_cluster_num);
			return -1;
		}
	}

	// check for security violations.
	// first, make certain ATTR_OWNER can only be set to who they really are.
	if (stricmp(attr_name, ATTR_OWNER) == 0) 
	{
		const char* sock_owner = Q_SOCK ? Q_SOCK->getOwner() : "";
		if( !sock_owner ) {
			sock_owner = "";
		}

		if ( stricmp(attr_value,"UNDEFINED")==0 ) {
				// If the user set the owner to be undefined, then
				// just fill in the value of Owner with the owner name
				// of the authenticated socket.
			if ( sock_owner && *sock_owner ) {
				new_value.sprintf("\"%s\"",sock_owner);
				attr_value  = new_value.Value();
			} else {
				// socket not authenticated and Owner is UNDEFINED.
#if !defined(WIN32)
				errno = EACCES;
#endif
				dprintf(D_ALWAYS, "ERROR SetAttribute violation: "
					"Owner is UNDEFINED, but client not authenticated\n");
				return -1;

			}
		}

			// We can't just use attr_value, since it contains '"'
			// marks.  Carefully remove them here.
		MyString owner_buf;
		char const *owner = attr_value;
		if( *owner == '"' ) {
			owner_buf = owner+1;
			if( owner_buf.Length() && owner_buf[owner_buf.Length()-1] == '"' )
			{
				owner_buf.setChar(owner_buf.Length()-1,'\0');
			}
			owner = owner_buf.Value();
		}


		if (!qmgmt_all_users_trusted
#if defined(WIN32)
			&& (stricmp(owner,sock_owner) != 0)
#else
			&& (strcmp(owner,sock_owner) != 0)
#endif
			&& (!isQueueSuperUser(sock_owner) || !SuperUserAllowedToSetOwnerTo(owner)) ) {
#ifndef WIN32
				errno = EACCES;
#endif
				dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s when active owner is \"%s\"\n",
					attr_value, sock_owner);
				return -1;
		}


			// If we got this far, we're allowing the given value for
			// ATTR_OWNER to be set.  However, now, we should try to
			// insert a value for ATTR_USER, too, so that's always in
			// the job queue.
		int nice_user = 0;
		MyString user;

		GetAttributeInt( cluster_id, proc_id, ATTR_NICE_USER,
						 &nice_user );
		user.sprintf( "\"%s%s@%s\"", (nice_user) ? "nice-user." : "",
				 owner, scheduler.uidDomain() );
		SetAttribute( cluster_id, proc_id, ATTR_USER, user.Value() );

			// Also update the owner history hash table
		AddOwnerHistory(owner);
	}
	else if (stricmp(attr_name, ATTR_CLUSTER_ID) == 0) {
		if (atoi(attr_value) != cluster_id) {
#if !defined(WIN32)
			errno = EACCES;
#endif
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ClusterId to incorrect value (%d!=%d)\n",
				atoi(attr_value), cluster_id);
			return -1;
		}
	}
	else if (stricmp(attr_name, ATTR_NICE_USER) == 0) {
			// Because we're setting a new value for nice user, we
			// should create a new value for ATTR_USER while we're at
			// it, since that might need to change now that
			// ATTR_NICE_USER is set.
		MyString owner;
		MyString user;
		bool nice_user = false;
		if( ! stricmp(attr_value, "TRUE") ) {
			nice_user = true;
		}
		if( GetAttributeString(cluster_id, proc_id, ATTR_OWNER, owner)
			>= 0 ) {
			user.sprintf( "\"%s%s@%s\"", (nice_user) ? "nice-user." :
					 "", owner.Value(), scheduler.uidDomain() );
			SetAttribute( cluster_id, proc_id, ATTR_USER, user.Value() );
		}
	}
	else if (stricmp(attr_name, ATTR_PROC_ID) == 0) {
		if (atoi(attr_value) != proc_id) {
#if !defined(WIN32)
			errno = EACCES;
#endif
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ProcId to incorrect value (%d!=%d)\n",
				atoi(attr_value), proc_id);
			return -1;
		}
		
		//
		// CronTab Attributes
		// Check to see if this attribte is one of the special
		// ones used for defining a crontab schedule. If it is, then
		// we add the cluster_id to a queue maintained by the schedd.
		// This is because at this point we may not have the proc_id, so
		// we can't add the full job information. The schedd will later
		// take this cluster_id and look at all of its procs to see if 
		// need to be added to the main CronTab list of jobs.
		//
	} else if ( stricmp( attr_name, ATTR_CRON_MINUTES ) == 0 ||
				stricmp( attr_name, ATTR_CRON_HOURS ) == 0 ||
				stricmp( attr_name, ATTR_CRON_DAYS_OF_MONTH ) == 0 ||
				stricmp( attr_name, ATTR_CRON_MONTHS ) == 0 ||
				stricmp( attr_name, ATTR_CRON_DAYS_OF_WEEK ) == 0 ) {
		scheduler.addCronTabClusterId( cluster_id );				
	}
	else if ( stricmp( attr_name, ATTR_JOB_STATUS ) == 0 ) {
			// If the status is being set, let's record the previous
			// status. If there is no status we'll default to
			// UNEXPANDED.
		int status = UNEXPANDED;
		GetAttributeInt( cluster_id, proc_id, ATTR_JOB_STATUS, &status );
		SetAttributeInt( cluster_id, proc_id, ATTR_LAST_JOB_STATUS, status );
	}

	// If any of the attrs used to create the signature are
	// changed, then delete the ATTR_AUTO_CLUSTER_ID, since
	// the signature needs to be recomputed as it may have changed.
	// Note we do this whether or not the transaction is committed - that
	// is ok, and actually is probably more efficient than hitting disk.
	if ( ad ) {
		char * sigAttrs = NULL;
		ad->LookupString(ATTR_AUTO_CLUSTER_ATTRS,&sigAttrs);
		if ( sigAttrs ) {
			StringList attrs(sigAttrs);
			if ( attrs.contains_anycase(attr_name) ) {
				ad->Delete(ATTR_AUTO_CLUSTER_ID);
				ad->Delete(ATTR_AUTO_CLUSTER_ATTRS);
			}
			free(sigAttrs);
			sigAttrs = NULL;
		}
	}

	// This block handles rounding of attributes.
	MyString round_param_name;
	round_param_name = "SCHEDD_ROUND_ATTR_";
	round_param_name += attr_name;

	char *round_param = param(round_param_name.Value());

	if( round_param && *round_param && strcmp(round_param,"0") ) {
		Token token;

			// See if attr_value is a scalar (int or float) by
			// invoking the ClassAd scanner.  We do it this way
			// to make certain we scan the value the same way that
			// the ClassAd library will (i.e. support exponential
			// notation, etc).
		char const *avalue = attr_value; // scanner will modify ptr, so save it
		Scanner(avalue,token);

		if ( token.type == LX_INTEGER || token.type == LX_FLOAT ) {
			// first, store the actual value
			MyString raw_attribute = attr_name;
			raw_attribute += "_RAW";
			JobQueue->SetAttribute(key, raw_attribute.Value(), attr_value);

			long ivalue;
			double fvalue;

			if ( token.type == LX_INTEGER ) {
				ivalue = token.intVal;
				fvalue = token.intVal;
			} else {
				ivalue = (long) token.floatVal;	// truncation conversion
				fvalue = token.floatVal;
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

					if( token.type == LX_INTEGER ) {
						new_value.sprintf("%d",(int)fvalue);
					}
					else {
						new_value.sprintf("%f",fvalue);
					}
				}
			}
			else {
					// round to specified power of 10
				unsigned int base;
				int exp = param_integer(round_param_name.Value(),0,0,9);

					// now compute the rounded value
					// set base to be 10^exp
				for (base=1 ; exp > 0; exp--, base *= 10);

					// round it.  note we always round UP!!  
				ivalue = ((ivalue + base - 1) / base) * base;

					// make it a string, courtesty MyString conversion.
				new_value = ivalue;

					// if it was a float, append ".0" to keep it a float
				if ( token.type == LX_FLOAT ) {
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
		if( stricmp(attr_name, ATTR_JOB_PRIO) == 0 ) {
			PrioRecArrayIsDirty = true;
		}
		if( stricmp(attr_name, ATTR_JOB_STATUS) == 0 ) {
			if( atoi(attr_value) == IDLE ) {
				PrioRecArrayIsDirty = true;
			}
		}
		if(PrioRecArrayIsDirty) {
			dprintf(D_FULLDEBUG,
					"Prioritized runnable job list will be rebuilt, because "
					"ClassAd attribute %s=%s changed\n",
					attr_name,attr_value);
		}
	}

	int old_nondurable_level = 0;
	if( flags & NONDURABLE ) {
		old_nondurable_level = JobQueue->IncNondurableCommitLevel();
	}

	JobQueue->SetAttribute(key, attr_name, attr_value);

	if( flags & NONDURABLE ) {
		JobQueue->DecNondurableCommitLevel( old_nondurable_level );

		ScheduleJobQueueLogFlush();
	}

	JobQueueDirty = true;

	return 0;
}

void
ScheduleJobQueueLogFlush()
{
		// Flush the log after a short delay so that we avoid spending
		// a lot of time waiting for the disk but we also make things
		// visible to JobRouter and Quill within a maximum delay.
	if( flush_job_queue_log_timer_id == -1 ) {
		flush_job_queue_log_timer_id = daemonCore->Register_Timer(
			flush_job_queue_log_delay,
			HandleFlushJobQueueLogTimer,
			"HandleFlushJobQueueLogTimer");
	}
}

int
HandleFlushJobQueueLogTimer(Service *)
{
	flush_job_queue_log_timer_id = -1;
	JobQueue->FlushLog();
	return 0;
}

int
SetTimerAttribute( int cluster, int proc, const char *attr_name, int dur )
{
	int rc = 0;
	if ( xact_start_time == 0 ) {
		dprintf(D_ALWAYS,
				"SetTimerAttribute called for %d.%d outside of a transaction!\n",
				cluster, proc);
		return -1;
	}

	rc = SetAttributeInt( cluster, proc, attr_name, xact_start_time + dur );
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
	filename.sprintf( "%s/mpp.%d.%d", Spool, cluster_id, proc_id);

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
	int fd = safe_open_wrapper(filename.Value(), O_CREAT | O_WRONLY, S_IREAD | S_IWRITE);
	if (fd < 0) {
		set_priv(old_priv);
		return -1;
	}

	char * encoded_value = simple_encode (cluster_id+proc_id, pwd);
	int len = strlen(encoded_value);
	if (write (fd, encoded_value, len) != len) {
		set_priv(old_priv);
		free(encoded_value);
		return -1;
	}
	close (fd);

	// Switch back to non-priviledged user
	set_priv(old_priv);

	free (encoded_value);

	return 0;

}


int
DestroyMyProxyPassword( int cluster_id, int proc_id )
{
	MyString filename;
	filename.sprintf( "%s%cmpp.%d.%d", Spool, DIR_DELIM_CHAR,
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

	return 0;
}


int GetMyProxyPassword (int cluster_id, int proc_id, char ** value) {
	// Create filename

	// Swith to root temporarily
	priv_state old_priv = set_root_priv();
	
	MyString filename;
	filename.sprintf( "%s/mpp.%d.%d", Spool, cluster_id, proc_id);
	int fd = safe_open_wrapper(filename.Value(), O_RDONLY);
	if (fd < 0) {
		set_priv(old_priv);
		return -1;
	}

	char buff[MYPROXY_MAX_PASSWORD_BUFLEN];
	int bytes = read (fd, buff, sizeof(buff));
	if( bytes < 0 ) {
		return -1;
	}
	buff [bytes] = '\0';

	close (fd);

	// Switch back to non-priviledged user
	set_priv(old_priv);

	*value = simple_decode (cluster_id + proc_id, buff);
	return 0;
}




const char * mesh = "Rr1aLvzki/#6`QeNoWl^\"(!x\'=OE3HBn [A)GtKu?TJ.mdS9%Fh;<\\+w~4yPDIq>2Ufs$Xc_@g0Y5Mb|{&*}]7,CpV-j:8Z";

char * simple_encode (int key, const char * src) {

  char * result = (char*)strdup (src);

  char buff[2];
  buff [1]='\n';

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

  for (; j<strlen(src); j++) {

	//
    for (i=0; i<strlen (mesh); i++) {
      if (mesh[i] == src[j]) {
		c = i;
		break;
		}
    }

    c=(c+strlen(mesh)-(key%strlen(mesh)))%strlen(mesh);
    
    snprintf(buff, 2, "%c", c+(int)' ');
    result[j]=buff[0];
    
  }
  return result;
}

bool
InTransaction()
{
	return JobQueue->InTransaction();
}

void
BeginTransaction()
{
	JobQueue->BeginTransaction();

	// note what time we started the transaction (used by SetTimerAttribute())
	xact_start_time = time( NULL );
}


void
CommitTransaction(SetAttributeFlags_t flags /* = 0 */)
{
	if( flags & NONDURABLE ) {
		JobQueue->CommitNondurableTransaction();
		ScheduleJobQueueLogFlush();
	}
	else {
		JobQueue->CommitTransaction();
	}

	xact_start_time = 0;
}


void
AbortTransaction()
{
	JobQueue->AbortTransaction();
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
		ClusterSizeHashTable->clear();
		ClassAd *ad;
		HashKey key;
		const char *tmp;
		int 	*numOfProcs = NULL;	
		int cluster_num;
		JobQueue->StartIterateAllClassAds();
		while (JobQueue->IterateAllClassAds(ad,key)) {
			tmp = key.value();
			if ( *tmp == '0' ) continue;	// skip cluster & header ads
			if ( (cluster_num = atoi(tmp)) ) {
				// count up number of procs in cluster, update ClusterSizeHashTable
				if ( ClusterSizeHashTable->lookup(cluster_num,numOfProcs) == -1 ) {
					// First proc we've seen in this cluster; set size to 1
					ClusterSizeHashTable->insert(cluster_num,1);
				} else {
					// We've seen this cluster_num go by before; increment proc count
					(*numOfProcs)++;
				}

			}
		}
	}	// end of if JobQueue->AbortTransaction == True
}

int
CloseConnection()
{

	JobQueue->CommitTransaction();
		// If this failed, the schedd will EXCEPT.  So, if we got this
		// far, we can always return success.  -Derek Wright 4/2/99

	// Now that the transaction has been commited, we need to chain proc
	// ads to cluster ads if any new clusters have been submitted.
	// Also, if EVENT_LOG is defined in condor_config, we will write
	// submit events into the EVENT_LOG here.
	if ( old_cluster_num != next_cluster_num ) {
		int cluster_id;
		int 	*numOfProcs = NULL;	
		int i;
		ClassAd *procad;
		ClassAd *clusterad;
		bool write_submit_events = false;
			// keep usr_log in outer scope so we don't open/close the 
			// event log over and over.
		WriteUserLog usr_log;
		usr_log.setCreatorName( Name );

		char *eventlog = param("EVENT_LOG");
		if ( eventlog ) {
			write_submit_events = true;
				// don't write to the user log here, since
				// hopefully condor_submit already did.
			usr_log.setWriteUserLog(false);
			usr_log.initialize(0,0,0,NULL);
			free(eventlog);
		}

		for ( cluster_id=old_cluster_num; cluster_id < next_cluster_num; cluster_id++ ) {
			char cluster_key[PROC_ID_STR_BUFLEN];
			IdToStr(cluster_id,-1,cluster_key);
			if ( (JobQueue->LookupClassAd(cluster_key, clusterad)) &&
			     (ClusterSizeHashTable->lookup(cluster_id,numOfProcs) != -1) )
			{
				for ( i = 0; i < *numOfProcs; i++ ) {
					char key[PROC_ID_STR_BUFLEN];
					IdToStr(cluster_id,i,key);
					if (JobQueue->LookupClassAd(key,procad)) {
							// chain proc ads to cluster ad
						procad->ChainToAd(clusterad);

							// convert any old attributes for backwards compatbility
						ConvertOldJobAdAttrs(procad, false);

							// write submit event to global event log
						if ( write_submit_events ) {
							SubmitEvent jobSubmit;
							jobSubmit.initFromClassAd(procad);
							usr_log.setGlobalCluster(cluster_id);
							usr_log.setGlobalProc(i);
							usr_log.writeEvent(&jobSubmit,procad);
						}
					}
				}	// end of loop thru all proc in cluster cluster_id
			}	
		}	// end of loop thru clusters
	}	// end of if a new cluster(s) submitted
	old_cluster_num = next_cluster_num;
					
	xact_start_time = 0;

	return 0;
}


int
GetAttributeFloat(int cluster_id, int proc_id, const char *attr_name, float *val)
{
	ClassAd	*ad;
	char	key[PROC_ID_STR_BUFLEN];
	char	*attr_val;

	IdToStr(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		sscanf(attr_val, "%f", val);
		free( attr_val );
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	if (ad->LookupFloat(attr_name, *val) == 1) return 0;
	return -1;
}


int
GetAttributeInt(int cluster_id, int proc_id, const char *attr_name, int *val)
{
	ClassAd	*ad;
	char	key[PROC_ID_STR_BUFLEN];
	char	*attr_val;

	IdToStr(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		sscanf(attr_val, "%d", val);
		free( attr_val );
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	if (ad->LookupInteger(attr_name, *val) == 1) return 0;
	return -1;
}


int
GetAttributeBool(int cluster_id, int proc_id, const char *attr_name, int *val)
{
	ClassAd	*ad;
	char	key[PROC_ID_STR_BUFLEN];
	char	*attr_val;

	IdToStr(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		sscanf(attr_val, "%d", val);
		free( attr_val );
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	if (ad->LookupBool(attr_name, *val) == 1) return 0;
	return -1;
}

int
GetAttributeString( int cluster_id, int proc_id, const char *attr_name, 
					char *val )
{
	ClassAd	*ad;
	char	key[PROC_ID_STR_BUFLEN];
	char	*attr_val;

	IdToStr(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		int attr_len = strlen( attr_val );
		if ( attr_val[0] != '"' || attr_val[attr_len-1] != '"' ) {
			free( attr_val );
			return -1;
		}
		attr_val[attr_len - 1] = '\0';
		strcpy(val, &attr_val[1]);
		free( attr_val );
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	if (ad->LookupString(attr_name, val) == 1) return 0;
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
	char	key[PROC_ID_STR_BUFLEN];
	char	*attr_val;

	IdToStr(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		int attr_len = strlen( attr_val );
		if ( attr_val[0] != '"' || attr_val[attr_len-1] != '"' ) {
			free( attr_val );
			return -1;
		}
		attr_val[attr_len - 1] = '\0';
		*val = strdup(&attr_val[1]);
		free( attr_val );
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		*val = (char *) calloc(1, sizeof(char));
		return -1;
	}

	if (ad->LookupString(attr_name, val) == 1) {
		return 0;
	}
	*val = (char *) calloc(1, sizeof(char));
	return -1;
}

// returns -1 if the lookup fails or if the value is not a string, 0 if
// the lookup succeeds in the job queue, 1 if it succeeds in the current
// transaction; val is set to the empty string on failure
int
GetAttributeString( int cluster_id, int proc_id, const char *attr_name, 
					MyString &val )
{
	ClassAd	*ad;
	char	key[PROC_ID_STR_BUFLEN];
	char	*attr_val;

	IdToStr(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		int attr_len = strlen( attr_val );
		if ( attr_val[0] != '"' || attr_val[attr_len-1] != '"' ) {
			free( attr_val );
			val = "";
			return -1;
		}
		attr_val[attr_len - 1] = '\0';
		val = attr_val + 1;
		free( attr_val );
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		val = "";
		return -1;
	}

	if (ad->LookupString(attr_name, val) == 1) {
		return 0;
	}
	val = "";
	return -1;
}

int
GetAttributeExpr(int cluster_id, int proc_id, const char *attr_name, char *val)
{
	ClassAd		*ad;
	char		key[PROC_ID_STR_BUFLEN];
	ExprTree	*tree;
	char		*attr_val;

	IdToStr(cluster_id,proc_id,key);

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		strcpy(val, attr_val);
		free( attr_val );
		return 1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	tree = ad->Lookup(attr_name);
	if (!tree) {
		return -1;
	}

	val[0] = '\0';
	tree->PrintToStr(val);

	return 1;
}


int
DeleteAttribute(int cluster_id, int proc_id, const char *attr_name)
{
	ClassAd				*ad;
	char				key[PROC_ID_STR_BUFLEN];
//	LogDeleteAttribute	*log;
	char				*attr_val = NULL;

	IdToStr(cluster_id,proc_id,key);

	if (!JobQueue->LookupClassAd(key, ad)) {
		if( ! JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
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
		return -1;
	}

//	log = new LogDeleteAttribute(key, attr_name);
//	JobQueue->AppendLog(log);
	JobQueue->DeleteAttribute(key, attr_name);

	JobQueueDirty = true;

	return 1;
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
		expanded_ad->ChainCollapse(true);

			// Make a stringlist of all attribute names in job ad.
			// Note: ATTR_JOB_CMD must be first in AttrsToExpand...
		StringList AttrsToExpand;
		const char * curr_attr_to_expand;
		AttrsToExpand.append(ATTR_JOB_CMD);
		ad->ResetName();
		const char *attr_name = ad->NextNameOriginal();
		for ( ; attr_name; attr_name = ad->NextNameOriginal() ) {
			if ( strncasecmp(attr_name,"MATCH_",6) == 0 ) {
					// We do not want to expand MATCH_XXX attributes,
					// because these are used to store the result of
					// previous expansions, which could potentially
					// contain literal $$(...) in the replacement text.
				continue;
			}
			if ( stricmp(attr_name,ATTR_JOB_CMD) ) { 
				AttrsToExpand.append(attr_name);
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
				ExprTree *cached_value = ad->Lookup(cachedAttrName.Value());
				if( cached_value ) {
					char *cached_value_buf = NULL;
					ASSERT( cached_value->RArg() );
					cached_value->RArg()->PrintToNewStr(&cached_value_buf);
					ASSERT(cached_value_buf);
					expanded_ad->AssignExpr(curr_attr_to_expand,cached_value_buf);
					free(cached_value_buf);
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
			ExprTree *tree = ad->Lookup(curr_attr_to_expand);
			if ( tree ) {
				ExprTree *rhs = tree->RArg();
				if ( rhs ) {
					rhs->PrintToNewStr( &attribute_value );
				}
			}

			if ( attribute_value == NULL ) {
					// Did not find the attribute to expand in the job ad.
					// Just move on to the next attribute...
				continue;
			}

				// Some backwards compatibility: if the
				// user just has $$opsys.$$arch in the
				// ATTR_JOB_CMD attribute, convert it to the
				// new format w/ parenthesis: $$(opsys).$$(arch)
			if ( (index == 0) && (attribute_value != NULL)
				 && ((tvalue=strstr(attribute_value,"$$")) != NULL) ) 
			{
				if ( stricmp("$$OPSYS.$$ARCH",tvalue) == MATCH ) 
				{
						// convert to the new format
						// First, we need to re-allocate attribute_value to a bigger
						// buffer.
					int old_size = strlen(attribute_value);
					attribute_value = (char *) realloc(attribute_value, 
											old_size 
											+ 10);  // for the extra parenthesis
					ASSERT(attribute_value);
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
					get_var(attribute_value,&left,&name,&right,NULL,true,search_pos) )
			{
				expanded_something = true;
				
				size_t namelen = strlen(name);
				if(name[0] == '[' && name[namelen-1] == ']') {
					// This is a classad expression to be considered

					MyString expr_to_add;
					expr_to_add.sprintf("string(%s", name + 1);
					expr_to_add.setChar(expr_to_add.Length()-1, ')');

					ClassAd tmpJobAd(*ad);
					const char * INTERNAL_DD_EXPR = "InternalDDExpr";

					bool isok = tmpJobAd.AssignExpr(INTERNAL_DD_EXPR, expr_to_add.Value());
					if( ! isok ) {
						attribute_not_found = true;
						break;
					}

					MyString result;
					isok = tmpJobAd.EvalString(INTERNAL_DD_EXPR, startd_ad, result);
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

					// Look for the name in the ad.
					// If it is not there, use the fallback.
					// If no fallback value, then fail.

					if( stricmp(name,"DOLLARDOLLAR") == 0 ) {
							// replace $$(DOLLARDOLLAR) with literal $$
						value = strdup("DOLLARDOLLAR = \"$$\"");
						value_came_from_jobad = true;
					}
					else if ( startd_ad ) {
							// We have a startd ad in memory, use it
						value = startd_ad->sPrintExpr(NULL,0,name);
						value_came_from_jobad = false;
					} else {
							// No startd ad -- use value from last match.
						MyString expr;
						expr = "MATCH_";
						expr += name;
						value = ad->sPrintExpr(NULL,0,expr.Value());
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
							if(strlen(fallback) == 0) {
								// fallback is nothing?  That confuses all sorts of
								// things.  How about a nothing string instead?
								sprintf(rebuild,"%s = \"%s\"",name,fallback);
							} else {
								sprintf(rebuild,"%s = %s",name,fallback);
							}
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
						int endquoteindex = strlen(tvalue) - 1;
						if ( endquoteindex >= 0 && 
							 tvalue[endquoteindex] == '"' ) {
								tvalue[endquoteindex] = '\0';
						}
					}
					bigbuf2 = (char *) malloc(  strlen(left) 
											  + strlen(tvalue) 
											  + strlen(right)
											  + 1);
					sprintf(bigbuf2,"%s%s%n%s",left,tvalue,&search_pos,right);
					free(attribute_value);
					attribute_value = (char *) malloc(  strlen(curr_attr_to_expand)
													  + 3 // = and quotes
													  + strlen(bigbuf2)
													  + 1);
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
			CommitTransaction();
		}

		if ( startd_ad ) {
				// Copy NegotiatorMatchExprXXX attributes from startd ad
				// to the job ad.  These attributes were inserted by the
				// negotiator.
			startd_ad->ResetName();
			char const *c_name;
			size_t len = strlen(ATTR_NEGOTIATOR_MATCH_EXPR);
			while( (c_name=startd_ad->NextNameOriginal()) ) {
				if( !strncmp(c_name,ATTR_NEGOTIATOR_MATCH_EXPR,len) ) {
					ExprTree *expr = startd_ad->Lookup(c_name);
					ASSERT(expr);
					expr = expr->RArg();
					if( !expr ) {
						continue;
					}
					char *new_value = NULL;
					expr->PrintToNewStr( &new_value );
					ASSERT(new_value);
					expanded_ad->AssignExpr(c_name,new_value);

					MyString match_exp_name = MATCH_EXP;
					match_exp_name += c_name;
					if ( SetAttribute(cluster_id,proc_id,match_exp_name.Value(),new_value) < 0 )
					{
						EXCEPT("Failed to store '%s=%s' into job ad %d.%d",
						       match_exp_name.Value(), new_value, cluster_id, proc_id);
					}
					free( new_value );
				}
			}
		}

		if ( startd_ad && job_universe == CONDOR_UNIVERSE_GRID ) {
				// Can remove our matched ad since we stored all the
				// values we need from it into the job ad.
			RemoveMatchedAd(cluster_id,proc_id);
		}


		if ( attribute_not_found ) {
			MyString hold_reason;
			hold_reason.sprintf("Cannot expand $$(%s).",name);

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
			FILE* email = email_user_open(ad,buf);
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
				email_close(email);
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
				hold_reason.sprintf(
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
				hold_reason.sprintf(
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


ClassAd *
GetJobAd(int cluster_id, int proc_id, bool expStartdAd, bool persist_expansions)
{
	char	key[PROC_ID_STR_BUFLEN];
	ClassAd	*ad;

	IdToStr(cluster_id,proc_id,key);
	if (JobQueue->LookupClassAd(key, ad)) {
		if ( !expStartdAd ) {
			// we're done, return the ad.
			return ad;
		}

		ClassAd *startd_ad = NULL;
		PROC_ID job_id;
		job_id.cluster = cluster_id;
		job_id.proc = proc_id;

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
			if ((job_universe == CONDOR_UNIVERSE_PARALLEL) ||
				(job_universe == CONDOR_UNIVERSE_MPI)) {
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

		return dollarDollarExpand(cluster_id, proc_id, ad, startd_ad, persist_expansions);

	} else {
		// we could not find this job ad
		return NULL;
	}
}


ClassAd *
GetJobByConstraint(const char *constraint)
{
	ClassAd	*ad;
	HashKey key;

	JobQueue->StartIterateAllClassAds();
	while(JobQueue->IterateAllClassAds(ad,key)) {
		if ( *(key.value()) != '0' &&	// avoid cluster and header ads
			EvalBool(ad, constraint)) {
				return ad;
		}
	}
	return NULL;
}


ClassAd *
GetNextJob(int initScan)
{
	return GetNextJobByConstraint(NULL, initScan);
}


ClassAd *
GetNextJobByConstraint(const char *constraint, int initScan)
{
	ClassAd	*ad;
	HashKey key;

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->IterateAllClassAds(ad,key)) {
		if ( *(key.value()) != '0' &&	// avoid cluster and header ads
			(!constraint || !constraint[0] || EvalBool(ad, constraint))) {
			return ad;
		}
	}
	return NULL;
}

ClassAd *
GetNextJobByCluster(int c, int initScan)
{
	ClassAd	*ad;
	HashKey key;
	char cluster[25];
	int len;

	if ( c < 1 ) {
		return NULL;
	}

	snprintf(cluster,25,"%d.",c);
	len = strlen(cluster);

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->IterateAllClassAds(ad,key)) {
		if ( strncmp(cluster,key.value(),len) == 0 ) {
			return ad;
		}
	}

	return NULL;
}

void
FreeJobAd(ClassAd *&ad)
{
	ad = NULL;
}

static int
RecvSpoolFileBytes(const MyString& path)
{
	filesize_t	size;
	Q_SOCK->getReliSock()->decode();
	if (Q_SOCK->getReliSock()->get_file(&size, path.Value()) < 0) {
		dprintf(D_ALWAYS,
		        "Failed to receive file from client in SendSpoolFile.\n");
		Q_SOCK->getReliSock()->eom();
		return -1;
	}
	chmod(path.Value(),00755);
	Q_SOCK->getReliSock()->eom();
	dprintf(D_FULLDEBUG, "done with transfer, errno = %d\n", errno);
	return 0;
}

int
SendSpoolFile(char const *filename)
{
	MyString path;

		/* We are passed in a filename to use to save the ICKPT file.
		   However, we should NOT trust this filename since it comes from 
		   the client!  So here we generate what we think the filename should
		   be based upon the current active_cluster_num in the transaction.
		   If the client does not send what we are expecting, we do the
		   paranoid thing and abort.  Once we are certain that my understanding
		   of this is correct, we could even just ignore the passed-in 
		   filename parameter completely. -Todd Tannenbaum, 2/2005
		*/
	path = gen_ckpt_name(Spool,active_cluster_num,ICKPT,0);
	if ( filename && strcmp(filename, condor_basename(path.Value())) ) {
		dprintf(D_ALWAYS, 
				"ERROR SendSpoolFile aborted due to suspicious path (%s)!\n",
				filename);
		return -1;
	}

	if ( !Q_SOCK || !Q_SOCK->getReliSock() ) {
		EXCEPT( "SendSpoolFile called when Q_SOCK is NULL" );
	}
	/* Tell client to go ahead with file transfer. */
	Q_SOCK->getReliSock()->encode();
	Q_SOCK->getReliSock()->put(0);
	Q_SOCK->getReliSock()->eom();

	return RecvSpoolFileBytes(path);
}

int
SendSpoolFileIfNeeded(ClassAd& ad)
{
	if ( !Q_SOCK || !Q_SOCK->getReliSock() ) {
		EXCEPT( "SendSpoolFileIfNeeded called when Q_SOCK is NULL" );
	}
	Q_SOCK->getReliSock()->encode();

	MyString path = gen_ckpt_name(Spool, active_cluster_num, ICKPT, 0);

	// here we take advantage of ickpt sharing if possible. if a copy
	// of the executable already exists we make a link to it and send
	// a '1' back to the client. if that can't happen but sharing is
	// enabled, the hash variable will be set to a non-empty string that
	// can be used to create a link that can be shared by future jobs
	//
	MyString owner;
	std::string hash;
	if (param_boolean("SHARE_SPOOLED_EXECUTABLES", true)) {
		if (!ad.LookupString(ATTR_OWNER, owner)) {
			dprintf(D_ALWAYS,
			        "SendSpoolFileIfNeeded: no %s attribute in ClassAd\n",
			        ATTR_OWNER);
			Q_SOCK->getReliSock()->put(-1);
			Q_SOCK->getReliSock()->eom();
			return -1;
		}
		if (!OwnerCheck(&ad, Q_SOCK->getOwner())) {
			dprintf(D_ALWAYS, "SendSpoolFileIfNeeded: OwnerCheck failure\n");
			Q_SOCK->getReliSock()->put(-1);
			Q_SOCK->getReliSock()->eom();
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
			if (!hash.empty() &&
			    ickpt_share_try_sharing(owner.Value(), hash, path.Value()))
			{
				Q_SOCK->getReliSock()->put(1);
				Q_SOCK->getReliSock()->eom();
				return 0;
			}
		}
	}

	/* Tell client to go ahead with file transfer. */
	Q_SOCK->getReliSock()->put(0);
	Q_SOCK->getReliSock()->eom();

	if (RecvSpoolFileBytes(path) == -1) {
		return -1;
	}

	if (!hash.empty()) {
		ickpt_share_init_sharing(owner.Value(), hash, path.Value());
	}

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
		ad->fPrint(stdout);
	}
	dprintf(D_ALWAYS, "****End of Queue*********\n");
}


// Returns cur_hosts so that another function in the scheduler can
// update JobsRunning and keep the scheduler and queue manager
// seperate. 
int get_job_prio(ClassAd *job)
{
    int job_prio;
    int job_status;
    PROC_ID id;
    int     q_date;
    char    buf[100];
	char	owner[100];
    int     cur_hosts;
    int     max_hosts;
	int 	niceUser;
	int		universe;

	ASSERT(job);

	buf[0] = '\0';
	owner[0] = '\0';

		// We must call getAutoClusterid() in get_job_prio!!!  We CANNOT
		// return from this function before we call getAutoClusterid(), so call
		// it early on (before any returns) right now.  The reason for this is
		// getAutoClusterid() performs a mark/sweep algorithm to garbage collect
		// old autocluster information.  If we fail to call getAutoClusterid, the
		// autocluster information for this job will be removed, causing the schedd
		// to ASSERT later on in the autocluster code. 
		// Quesitons?  Ask Todd <tannenba@cs.wisc.edu> 01/04
	int auto_id = scheduler.autocluster.getAutoClusterid(job);

	job->LookupInteger(ATTR_JOB_UNIVERSE, universe);
	job->LookupInteger(ATTR_JOB_STATUS, job_status);
    if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) == 0) {
        cur_hosts = ((job_status == RUNNING) ? 1 : 0);
    }
    if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) == 0) {
        max_hosts = ((job_status == IDLE || job_status == UNEXPANDED) ? 1 : 0);
    }
	// Figure out if we should contine and put this job into the PrioRec array
	// or not.
    // No longer judge whether or not a job can run by looking at its status.
    // Rather look at if it has all the hosts that it wanted.
    if (cur_hosts>=max_hosts || job_status==HELD || 
			job_status==REMOVED || job_status==COMPLETED ||
			!service_this_universe(universe,job)) 
	{
        return cur_hosts;
	}


	// --- Insert this job into the PrioRec array ---

    job->LookupInteger(ATTR_JOB_PRIO, job_prio);
    job->LookupInteger(ATTR_Q_DATE, q_date);
	if( job->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
		strcpy(owner,NiceUserName);
		strcat(owner,".");
	}
	buf[0] = '\0';
	job->LookupString(ATTR_ACCOUNTING_GROUP,buf,sizeof(buf));  // TODDCORE
	if ( buf[0] == '\0' ) {
		job->LookupString(ATTR_OWNER, buf, sizeof(buf));  
	}
	strcat(owner,buf);
		// Note, we should use this method instead of just looking up
		// ATTR_USER directly, since that includes UidDomain, which we
		// don't want for this purpose...
	job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	job->LookupInteger(ATTR_PROC_ID, id.proc);

	
    // No longer judge whether or not a job can run by looking at its status.
    // Rather look at if it has all the hosts that it wanted.
    if (cur_hosts>=max_hosts || job_status==HELD)
        return cur_hosts;

    PrioRec[N_PrioRecs].id       = id;
    PrioRec[N_PrioRecs].job_prio = job_prio;
    PrioRec[N_PrioRecs].status   = job_status;
    PrioRec[N_PrioRecs].qdate    = q_date;
	if ( auto_id == -1 ) {
		PrioRec[N_PrioRecs].auto_cluster_id = id.cluster;
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

static bool
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
		dprintf( D_ALWAYS, "%d.%d: %s remaining: EXPIRED!\n", 
				 cluster, proc, ATTR_JOB_LEASE_DURATION );
		return false;
	} 
	dprintf( D_ALWAYS, "%d.%d: %s remaining: %d\n", cluster, proc,
			 ATTR_JOB_LEASE_DURATION, remaining );
	return true;
}

extern void mark_job_stopped(PROC_ID* job_id);

int mark_idle(ClassAd *job)
{
    int     status, cluster, proc, hosts, universe;
	PROC_ID	job_id;
	static time_t bDay = 0;

		// Update ATTR_SCHEDD_BIRTHDATE in job ad at startup
	if (bDay == 0) {
		bDay = time(NULL);
	}
	job->Assign(ATTR_SCHEDD_BIRTHDATE,(int)bDay);

	if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) < 0) {
		universe = CONDOR_UNIVERSE_STANDARD;
	}

	MyString managed_status;
	job->LookupString(ATTR_JOB_MANAGED, managed_status);
	if ( managed_status == MANAGED_EXTERNAL ) {
		// if a job is externally managed, don't touch a damn
		// thing!!!  the gridmanager or schedd-on-the-side is
		// in control.  stay out of its way!  -Todd 9/13/02
		// -amended by Jaime 10/4/05
		return 1;
	}

	int mirror_active = 0;
	job->LookupBool(ATTR_MIRROR_ACTIVE, mirror_active);
	if ( mirror_active ) {
		// Don't touch a job that has an active mirror.  Once we are in count jobs, we will
		// startup a gridmanager for this job that will retrieve the current job status from
		// our schedd mirror.
		return 1;
	}

	job->LookupInteger(ATTR_CLUSTER_ID, cluster);
	job->LookupInteger(ATTR_PROC_ID, proc);
    job->LookupInteger(ATTR_JOB_STATUS, status);
	job->LookupInteger(ATTR_CURRENT_HOSTS, hosts);

	job_id.cluster = cluster;
	job_id.proc = proc;

	if ( status == COMPLETED ) {
		DestroyProc(cluster,proc);
	} else if ( status == REMOVED ) {
		// a globus job with a non-null contact string should be left alone
		if ( universe == CONDOR_UNIVERSE_GRID ) {
			char grid_job_id[20];
			if ( job->LookupString( ATTR_GRID_JOB_ID, grid_job_id,
									sizeof(grid_job_id) ) )
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
	} else if ( status == UNEXPANDED ) {
		SetAttributeInt(cluster,proc,ATTR_JOB_STATUS,IDLE);
		SetAttributeInt( cluster, proc, ATTR_ENTERED_CURRENT_STATUS,
						 (int)time(0) );
		SetAttributeInt( cluster, proc, ATTR_LAST_SUSPENSION_TIME, 0);
	}
	else if ( status == RUNNING || hosts > 0 ) {
		if( universeCanReconnect(universe) &&
			jobLeaseIsValid(job, cluster, proc) )
		{
			dprintf( D_FULLDEBUG, "Job %d.%d might still be alive, "
					 "spawning shadow to reconnect\n", cluster, proc );
			if (universe == CONDOR_UNIVERSE_PARALLEL) {
				dedicated_scheduler.enqueueReconnectJob( job_id);	
			} else {
				scheduler.enqueueReconnectJob( job_id );
			}
		} else {
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
		JobQueue->BeginTransaction();
		SetAttributeFloat(cluster,proc,ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock);
		DeleteAttribute(cluster,proc,ATTR_JOB_WALL_CLOCK_CKPT);
			// remove shadow birthdate so if CkptWallClock()
			// runs before a new shadow starts, it won't
			// potentially double-count
		DeleteAttribute(cluster,proc,ATTR_SHADOW_BIRTHDATE);
		JobQueue->CommitTransaction();
	}

	return 1;
}

void
WalkJobQueue(scan_func func)
{
	ClassAd *ad;
	int rval = 0;

	ad = GetNextJob(1);
	while (ad != NULL && rval >= 0) {
		rval = func(ad);
		if (rval >= 0) {
			FreeJobAd(ad);
			ad = GetNextJob(0);
		}
	}
	if (ad != NULL)
		FreeJobAd(ad);
}

/*
** There should be no jobs running when we start up.  If any were killed
** when the last schedd died, they will still be listed as "running" in
** the job queue.  Here we go in and mark them as idle.
*/
void mark_jobs_idle()
{
    WalkJobQueue( mark_idle );
}

void DirtyPrioRecArray() {
		// Mark the PrioRecArray as stale. This will trigger a rebuild,
		// though possibly not immediately.
	PrioRecArrayIsDirty = true;
}

static void DoBuildPrioRecArray() {
	scheduler.autocluster.mark();

	N_PrioRecs = 0;
	WalkJobQueue( get_job_prio );

		// N_PrioRecs might be 0, if we have no jobs to run at the
		// moment.  If so, we don't want to call qsort(), since that's
		// bad.  We can still try to find the owner in the Owners
		// array, since that's not that expensive, and we need it for
		// all the flocking logic at the end of this function.
		// Discovered by Derek Wright and insure-- on 2/28/01
	if( N_PrioRecs ) {
		qsort( (char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]),
			   (int(*)(const void*, const void*))prio_compar );
	}

	scheduler.autocluster.sweep();

	if( !scheduler.shadow_prio_recs_consistent() ) {
		scheduler.mail_problem_message();
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
	int hash_size = TotalJobsCount/4+1000;
	if( PrioRecAutoClusterRejected &&
	    PrioRecAutoClusterRejected->getTableSize() < 0.8*hash_size )
	{
		delete PrioRecAutoClusterRejected;
		PrioRecAutoClusterRejected = NULL;
	}
	if( ! PrioRecAutoClusterRejected ) {
		PrioRecAutoClusterRejected = new HashTable<int,int>(hash_size,hashFuncInt,rejectDuplicateKeys);
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

/*
 * Find the job with the highest priority that matches with
 * my_match_ad (which is a startd ad).  If user is NULL, get a job for
 * any user; o.w. only get jobs for specified user.
 */
void FindRunnableJob(PROC_ID & jobid, const ClassAd* my_match_ad, 
					 char * user)
{
	ClassAd				*ad;
	bool match_any_user = (user == NULL) ? true : false;

	ASSERT(my_match_ad);

		// indicate failure by setting proc to -1.  do this now
		// so if we bail out early anywhere, we say we failed.
	jobid.proc = -1;	

	// BIOTECH
	char *biotech = param("BIOTECH");
	if (biotech) {
		free(biotech);
		ad = GetNextJob(1);
		while (ad != NULL) {
			if ( Runnable(ad) ) {
				if ( *ad == ((ClassAd &)(*my_match_ad)) )
				{
					ad->LookupInteger(ATTR_CLUSTER_ID, jobid.cluster);
					ad->LookupInteger(ATTR_PROC_ID, jobid.proc);
					if( !scheduler.AlreadyMatched(&jobid) ) {
						break;
					}
				}
			}
			ad = GetNextJob(0);
		}
		return;
	}

	MyString owner = user;
	int at_sign_pos;
	int i;

		// We have been passed user, which is owner@uid.  We want just
		// owner, place a NULL at the '@'.

	at_sign_pos = owner.FindChar('@');
	if ( at_sign_pos >= 0 ) {
		owner.setChar(at_sign_pos,'\0');
	}

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
			else if ( !match_any_user && strcmp(PrioRec[i].owner,owner.Value()) != 0 ) {
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

			if ( ! (*ad == (ClassAd &)(*my_match_ad)) )
				{
						// Job and machine do not match.
					PrioRecAutoClusterRejected->insert( PrioRec[i].auto_cluster_id, 1 );
				}
			else {
				if(!Runnable(&PrioRec[i].id) || scheduler.AlreadyMatched(&PrioRec[i].id)) {
						// This job's status must have changed since the
						// time it was added to the runnable job list.
						// Prevent this job from being considered in any
						// future iterations through the list.
					PrioRec[i].owner[0] = '\0';

						// Ensure that PrioRecArray is rebuilt
						// eventually, because changes in the status
						// of AlreadyMatched() can happen without
						// changes to the status of the job, (not the
						// normal case, but still possible) so the
						// dirty flag may not get set when the job
						// is no longer AlreadyMatched() unless we
						// set it here and keep rebuilding the array.

					PrioRecArrayIsDirty = true;
				}
				else {

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
						if( my_match_ad->EvalFloat(ATTR_RANK, ad, new_startd_rank) )
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
						//
						// Ohh, indented sooo far!
					MyString jobLimits, recordedLimits;
					if (param_boolean("CLAIM_RECYCLING_CONSIDER_LIMITS", true)) {
						ad->LookupString(ATTR_CONCURRENCY_LIMITS, jobLimits);
						my_match_ad->LookupString(ATTR_MATCHED_CONCURRENCY_LIMITS,
												  recordedLimits);
						jobLimits.lower_case();
						recordedLimits.lower_case();
						
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
				}
			}
		}

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

int Runnable(ClassAd *job)
{
	int status, universe, cur = 0, max = 1;

	if ( job->LookupInteger(ATTR_JOB_STATUS, status) == 0 )
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",ATTR_JOB_STATUS);
		return FALSE;
	}
	if (status == HELD)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (HELD)\n");
		return FALSE;
	}
	if (status == REMOVED)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (REMOVED)\n");
		return FALSE;
	}
	if (status == COMPLETED)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (COMPLETED)\n");
		return FALSE;
	}


	if ( job->LookupInteger(ATTR_JOB_UNIVERSE, universe) == 0 )
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",
				ATTR_JOB_UNIVERSE);
		return FALSE;
	}
	if( !service_this_universe(universe,job) )
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (Universe=%s)\n",
			CondorUniverseName(universe) );
		return FALSE;
	}

	job->LookupInteger(ATTR_CURRENT_HOSTS, cur);
	job->LookupInteger(ATTR_MAX_HOSTS, max);

	if (cur < max)
	{
		dprintf (D_FULLDEBUG | D_NOHEADER, " is runnable\n");
		return TRUE;
	}
	
	dprintf (D_FULLDEBUG | D_NOHEADER, " not runnable (default rule)\n");
	return FALSE;
}

int Runnable(PROC_ID* id)
{
	ClassAd *jobad;
	
	dprintf (D_FULLDEBUG, "Job %d.%d:", id->cluster, id->proc);

	if (id->cluster < 1 || id->proc < 0 || (jobad=GetJobAd(id->cluster,id->proc))==NULL )
	{
		dprintf (D_FULLDEBUG | D_NOHEADER, " not runnable\n");
		return FALSE;
	}

	return Runnable(jobad);
}

// From the priority records, find the runnable job with the highest priority
// use the function prio_compar. By runnable I mean that its status is either
// UNEXPANDED or IDLE.
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

void
dirtyJobQueue()
{
	JobQueueDirty = true;
}
