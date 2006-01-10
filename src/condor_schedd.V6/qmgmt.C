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

#include "condor_common.h"
#include "condor_io.h"
#include "string_list.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

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

extern char *Spool;
extern char *Name;
extern char* JobHistoryFileName;
extern Scheduler scheduler;
extern DedicatedScheduler dedicated_scheduler;

extern "C" {
	int	prio_compar(prio_rec*, prio_rec*);
}



extern	int		Parse(const char*, ExprTree*&);
extern  void    cleanup_ckpt_files(int, int, const char*);
extern	bool	service_this_universe(int, ClassAd *);
static ReliSock *Q_SOCK = NULL;

int		do_Q_request(ReliSock *);
void	FindRunnableJob(PROC_ID & jobid, const ClassAd* my_match_ad,
					 char * user);
void	FindPrioJob(PROC_ID &);
int		Runnable(PROC_ID*);
int		Runnable(ClassAd*);


static ClassAdCollection *JobQueue = 0;
static int next_cluster_num = -1;
static int next_proc_num = 0;
static int active_cluster_num = -1;	// client is restricted to only insert jobs to the active cluster
static int old_cluster_num = -1;	// next_cluster_num at start of transaction
static bool JobQueueDirty = false;
static time_t xact_start_time = 0;	// time at which the current transaction was started

class Service;

prio_rec	PrioRecArray[INITIAL_MAX_PRIO_REC];
prio_rec	* PrioRec = &PrioRecArray[0];
int			N_PrioRecs = 0;
static int 	MAX_PRIO_REC=INITIAL_MAX_PRIO_REC ;	// INITIAL_MAX_* in prio_rec.h

const char HeaderKey[] = "0.0";

static void AppendHistory(ClassAd*);

// Create a hash table which, given a cluster id, tells how
// many procs are in the cluster
static inline int compute_clustersize_hash(const int &key,int numBuckets) {
	return ( key % numBuckets );
}
typedef HashTable<int, int> ClusterSizeHashTable_t;
static ClusterSizeHashTable_t *ClusterSizeHashTable = 0;
static int TotalJobsCount = 0;

static char	**super_users = NULL;
static int	num_super_users = 0;
static char *default_super_user =
#if defined(WIN32)
	"Administrator";
#else
	"root";
#endif

//static int allow_remote_submit = FALSE;

static
char *
IdToStr(int cluster, int proc)
{
	static char strbuf[35];

	if ( proc == -1 ) {
		// cluster ad key
		sprintf(strbuf,"0%d.-1",cluster);
	} else {
		// proc ad key
		sprintf(strbuf,"%d.%d",cluster,proc);
	}
	return strbuf;
}

static
void
StrToId(const char *str,int & cluster,int & proc)
{
	char *tmp;

	// skip leading zero, if any
	if ( *str == '0' ) 
		str++;

	if ( !(tmp = strchr(str,'.')) ) {
		EXCEPT("Qmgmt: Malformed key - '%s'",str);
	}
	tmp++;

	cluster = atoi(str);
	proc = atoi(tmp);
}

static
void
ClusterCleanup(int cluster_id)
{
	// remove entry in ClusterSizeHashTable 
	ClusterSizeHashTable->remove(cluster_id);

	// delete the cluster classad
	JobQueue->DestroyClassAd( IdToStr(cluster_id,-1) );

	// blow away the initial checkpoint file from the spool dir
	char *ckpt_file_name = gen_ckpt_name( Spool, cluster_id, ICKPT, 0 );
	(void)unlink( ckpt_file_name );
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
// Returns true if the ad was modified, false otherwise.
void
ConvertOldJobAdAttrs( ClassAd *job_ad )
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

			if ( grid_job_id.IsEmpty() &&
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
													true );
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

			if ( grid_job_id.IsEmpty() &&
				 job_ad->LookupString( ATTR_REMOTE_JOB_ID, jobid ) ) {

					// Ugly: If the job is using match-making, we
					// need to grab the match-substituted versions
					// of RemoteSchedd and RemotePool to stuff into
					// GridJobId. That means calling GetJobAd().
				ClassAd *job_ad2;

				job_ad2 = GetJobAd( cluster, proc, true );

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

			if ( grid_job_id.IsEmpty() &&
				 job_ad->LookupString( ATTR_REMOTE_JOB_ID, jobid ) ) {

				MyString new_value;

				new_value.sprintf( "blah %s", jobid.Value() );
				job_ad->Assign( ATTR_GRID_JOB_ID, new_value.Value() );
				job_ad->AssignExpr( ATTR_REMOTE_JOB_ID, "Undefined" );
				JobQueueDirty = true;
			}
		}
	}

}

// Read out any parameters from the config file that we need and
// initialize our internal data structures.
void
InitQmgmt()
{
	StringList s_users;
	char* tmp;
	int i;
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
}


void
InitJobQueue(const char *job_queue_name)
{
	assert(!JobQueue);
	JobQueue = new ClassAdCollection(job_queue_name);
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
	char	cluster_str[40];
	char	owner[_POSIX_PATH_MAX];
	char	user[_POSIX_PATH_MAX];
	char	correct_user[_POSIX_PATH_MAX];
	char	buf[_POSIX_PATH_MAX];
	char	attr_scheduler[_POSIX_PATH_MAX];
	char	correct_scheduler[_POSIX_PATH_MAX];

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
	sprintf( correct_scheduler, "DedicatedScheduler!%s", Name );

	next_cluster_num = 1;
	JobQueue->StartIterateAllClassAds();
	while (JobQueue->IterateAllClassAds(ad,key)) {
		const char *tmp = key.value();
		if ( *tmp == '0' ) continue;	// skip cluster & header ads
		if ( (cluster_num = atoi(tmp)) ) {

			// find highest cluster, set next_cluster_num to one higher
			if (cluster_num >= next_cluster_num) {
				next_cluster_num = cluster_num + 1;
			}

			// link all proc ads to their cluster ad, if there is one
			sprintf(cluster_str,"0%d.-1",cluster_num);
			if ( JobQueue->LookupClassAd(cluster_str,clusterad) ) {
				ad->ChainToAd(clusterad);
			}

			int ntmp;
			if( ad->LookupBool(ATTR_JOB_MANAGED, ntmp) ) {
				// "Managed" is no longer a bool.  It's a string state.
				if(ntmp) {
					ad->Assign(ATTR_JOB_MANAGED, MANAGED_EXTERNAL);
				} else {
					ad->Assign(ATTR_JOB_MANAGED, MANAGED_SCHEDD);
				}
			}

			if (!ad->LookupString(ATTR_OWNER, owner)) {
				dprintf(D_ALWAYS,
						"Job %s has no %s attribute.  Removing....\n",
						tmp, ATTR_OWNER);
				JobQueue->DestroyClassAd(tmp);
				continue;
			}

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

				// Figure out what ATTR_USER *should* be for this job
			int nice_user = 0;
			ad->LookupInteger( ATTR_NICE_USER, nice_user );
			sprintf( correct_user, "%s%s@%s",
					 (nice_user) ? "nice-user." : "", owner,
					 scheduler.uidDomain() );

			if (!ad->LookupString(ATTR_USER, user)) {
				dprintf( D_FULLDEBUG,
						"Job %s has no %s attribute.  Inserting one now...\n",
						tmp, ATTR_USER);
				sprintf( buf, "%s = \"%s\"", ATTR_USER, correct_user );
				ad->Insert( buf );
				JobQueueDirty = true;
			} else {
					// ATTR_USER exists, make sure it's correct, and
					// if not, insert the new value now.
				if( strcmp(user,correct_user) ) {
						// They're different, so insert the right value
					dprintf( D_FULLDEBUG,
							 "Job %s has stale %s attribute.  "
							 "Inserting correct value now...\n",
							 tmp, ATTR_USER );
					sprintf( buf, "%s = \"%s\"", ATTR_USER, correct_user );
					ad->Insert( buf );
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
					sprintf( buf, "%s = \"%s\"", ATTR_SCHEDULER,
							 correct_scheduler );
					ad->Insert( buf );
					JobQueueDirty = true;
				} else {

						// ATTR_SCHEDULER exists, make sure it's correct,
						// and if not, insert the new value now.
					if( strcmp(attr_scheduler,correct_scheduler) ) {
							// They're different, so insert the right
							// value 
						dprintf( D_FULLDEBUG,
								 "Job %s has stale %s attribute.  "
								 "Inserting correct value now...\n",
								 tmp, ATTR_SCHEDULER );
						sprintf( buf, "%s = \"%s\"", ATTR_SCHEDULER,
								 correct_scheduler );
						ad->Insert( buf );
						JobQueueDirty = true;
					}
				}
			}

			ConvertOldJobAdAttrs( ad );

			// count up number of procs in cluster, update ClusterSizeHashTable
			IncrementClusterSize(cluster_num);

		}
	}

	if ( stored_cluster_num == 0 ) {
		sprintf(cluster_str, "%d", next_cluster_num);
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

bool
OwnerCheck(int cluster_id,int proc_id)
{
	char				key[_POSIX_PATH_MAX];
	ClassAd				*ad = NULL;

	if (!Q_SOCK) {
		return 0;
	}

	strcpy(key, IdToStr(cluster_id,proc_id) );
	if (!JobQueue->LookupClassAd(key, ad)) {
		return 0;
	}

	return OwnerCheck(ad, Q_SOCK->getOwner());
}

// Test if this owner matches my owner, so they're allowed to update me.
bool
OwnerCheck(ClassAd *ad, const char *test_owner)
{
	char	my_owner[_POSIX_PATH_MAX];

	// If test_owner is NULL, then we have no idea who the user is.  We
	// do not allow anonymous folks to mess around with the queue, so 
	// have OwnerCheck fail.  Note we only call OwnerCheck in the first place
	// if Q_SOCK is not null; if Q_SOCK is null, then the schedd is calling
	// a QMGMT command internally which is allowed.
	if (test_owner == NULL) {
		dprintf(D_ALWAYS,"QMGT command failed: anonymous user not permitted\n" );
		return false;
	}

	// check if the IP address of the peer has daemon core write permission
	// to the schedd.  we have to explicitly check here because all queue
	// management commands come in via one sole daemon core command which
	// has just READ permission.
	if ( daemonCore->Verify(WRITE, Q_SOCK->endpoint(), Q_SOCK->getFullyQualifiedUser()) == FALSE ) {
		// this machine does not have write permission; return failure
		dprintf(D_ALWAYS,"QMGT command failed: no WRITE permission for %s\n",
			Q_SOCK->endpoint_ip_str() );
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
	if (strcmp(my_owner, test_owner) != 0) {
#if !defined(WIN32)
		errno = EACCES;
#endif
		dprintf( D_FULLDEBUG, "ad owner: %s, queue submit owner: %s\n",
				my_owner, test_owner );
		return false;
	} 
	else {
		return true;
	}
}


/* We want to be able to call these things even from code linked in which
   is written in C, so we make them C declarations
*/
extern "C" {


bool
setQSock( ReliSock* rsock )
{
	// initialize per-connection variables.  back in the day this
	// was essentially InvalidateConnection().  of particular 
	// importance is setting Q_SOCK... this tells the rest of the QMGMT
	// code the request is from an external user instead of originating
	// from within the schedd itself.
	// If rsock is NULL, that means the request is internal to the schedd
	// itself, and thus anything goes!

	// store the cluster num so when we commit the transaction, we can easily
	// see if new clusters have been submitted and thus make links to cluster ads
	old_cluster_num = next_cluster_num;	
	active_cluster_num = -1;
	Q_SOCK = rsock;
	return true;
}


void
unsetQSock()
{
	// reset the per-connection variables.  of particular 
	// importance is setting Q_SOCK back to NULL. this tells the rest of 
	// the QMGMT code the request originated internally, and it should
	// be permitted (i.e. we only call OwnerCheck if Q_SOCK is not NULL).

	Q_SOCK = NULL;
}


int
handle_q(Service *, int, Stream *sock)
{
	int	rval;

	setQSock((ReliSock*)sock);

	JobQueue->BeginTransaction();

	// note what time we started the transaction (used by SetTimerAttribute())
	xact_start_time = time( NULL );

	// store the cluster num so when we commit the transaction, we can easily
	// see if new clusters have been submitted and thus make links to cluster ads
	old_cluster_num = next_cluster_num;

	// initialize per-connection variables.  back in the day this
	// was essentially InvalidateConnection().  of particular
	// importance is setting Q_SOCK... this tells the rest of the QMGMT
	// code the request is from an external user instead of originating
	// from within the schedd itself.
	Q_SOCK = (ReliSock *)sock;
	//Q_SOCK->unAuthenticate();

	active_cluster_num = -1;

	do {
		/* Probably should wrap a timer around this */
		rval = do_Q_request( (ReliSock *)Q_SOCK );
	} while(rval >= 0);


	unsetQSock();

	dprintf(D_FULLDEBUG, "QMGR Connection closed\n");

	// Abort any uncompleted transaction.  The transaction should
	// be committed in CloseConnection().
	AbortTransactionAndRecomputeClusters();

	return 0;
}

int GetMyProxyPassword (int, int, char **);

int get_myproxy_password_handler(Service * service, int i, Stream *socket) {

	//	For debugging
//	DebugFP = stderr;

	int cluster_id;
	int proc_id;
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
InitializeConnection( const char *owner, const char *domain )
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
	char cluster_str[40];

	if( Q_SOCK && !OwnerCheck(NULL, Q_SOCK->getOwner()  ) ) {
		dprintf( D_FULLDEBUG, "NewCluser(): OwnerCheck failed\n" );
		return -1;
	}

	if ( TotalJobsCount >= scheduler.getMaxJobsSubmitted() ) {
		dprintf(D_ALWAYS,
			"NewCluster(): MAX_JOBS_SUBMITTED exceeded, submit failed\n");
		return -2;
	}

	next_proc_num = 0;
	active_cluster_num = next_cluster_num++;
	sprintf(cluster_str, "%d", next_cluster_num);
//	log = new LogSetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);
//	JobQueue->AppendLog(log);
	JobQueue->SetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);

	// put a new classad in the transaction log to serve as the cluster ad
	JobQueue->NewClassAd(IdToStr(active_cluster_num,-1), JOB_ADTYPE, STARTD_ADTYPE);

	return active_cluster_num;
}


int
NewProc(int cluster_id)
{
	int				proc_id;
	char			key[_POSIX_PATH_MAX];
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
	sprintf(key, "%d.%d", cluster_id, proc_id);
//	log = new LogNewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);
//	JobQueue->AppendLog(log);
	JobQueue->NewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);

	IncrementClusterSize(cluster_id);

		// now that we have a real job ad with a valid proc id, then
		// also insert the appropriate GlobalJobId while we're at it.
	MyString gjid = "\"";
	gjid += Name;             // schedd's name
	int now = (int)time(0);
	gjid += "#";
	gjid += now;
	gjid += "#";
	gjid += cluster_id;
	gjid += ".";
	gjid += proc_id;
	gjid += "\"";
	JobQueue->SetAttribute( key, ATTR_GLOBAL_JOB_ID, gjid.Value() );

	return proc_id;
}

int 	DestroyMyProxyPassword (int cluster_id, int proc_id);

int DestroyProc(int cluster_id, int proc_id)
{
	char				key[_POSIX_PATH_MAX];
	ClassAd				*ad = NULL;
//	LogDestroyClassAd	*log;

	strcpy(key, IdToStr(cluster_id,proc_id) );
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
														(int)time(NULL));
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
	ad->EvalBool(ATTR_JOB_LEAVE_IN_QUEUE,NULL,leave_job_in_q);
	if ( leave_job_in_q ) {
		return DESTROYPROC_SUCCESS_DELAY;
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

//	log = new LogDestroyClassAd(key);
//	JobQueue->AppendLog(log);
	JobQueue->DestroyClassAd(key);

	DecrementClusterSize(cluster_id);

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

// DestroyClusterByContraint not used anywhere, so it is commented out.
#if 0
int DestroyClusterByConstraint(const char* constraint)
{
	int			flag = 1;
	ClassAd		*ad=NULL;
	char		key[_POSIX_PATH_MAX];
//	LogRecord	*log;
	char		*ickpt_file_name;
	int			prev_cluster = -1;
    PROC_ID		job_id;

	JobQueue->StartIterateAllClassAds();

	while(JobQueue->IterateAllClassAds(ad)) {
		// check cluster first to avoid header ad
		if (ad->LookupInteger(ATTR_CLUSTER_ID, job_id.cluster) == 1) {
			if ( !Q_SOCK || OwnerCheck(ad, Q_SOCK->getOwner() )) {
				if (EvalBool(ad, constraint)) {
					ad->LookupInteger(ATTR_PROC_ID, job_id.proc);

					// Apend to history file
    	            AppendHistory(ad);

					sprintf(key, "%d.%d", job_id.cluster, job_id.proc);
//					log = new LogDestroyClassAd(key);
//					JobQueue->AppendLog(log);
					JobQueue->DestroyClassAd(key);
					flag = 0;

					if (prev_cluster != job_id.cluster) {
						ickpt_file_name =
							gen_ckpt_name( Spool, job_id.cluster, ICKPT, 0 );
						(void)unlink( ickpt_file_name );
						prev_cluster = job_id.cluster;
						ClusterSizeHashTable->remove(job_id.cluster);
					}
				}
			}
		}
	}

	JobQueueDirty = true;

	if(flag) return -1;
	return 0;
}
#endif


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
			 const char *attr_value)
{
//	LogSetAttribute	*log;
	char			key[_POSIX_PATH_MAX];
	ClassAd			*ad = NULL;
	char			alternate_attrname_buf[_POSIX_PATH_MAX];

	// Only an authenticated user or the schedd itself can set an attribute.
	if ( Q_SOCK && !(Q_SOCK->isAuthenticated()) ) {
		return -1;
	}

	strcpy(key, IdToStr(cluster_id,proc_id) );

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
	if (Q_SOCK && stricmp(attr_name, ATTR_OWNER) == 0) // TODD SOAP CHANGE!  REVIEW!  TODO
	{
		if ( !Q_SOCK ) {
			EXCEPT( "Trying to setAttribute( ATTR_OWNER ) and Q_SOCK is NULL" );
		}

		sprintf(alternate_attrname_buf, "\"%s\"", Q_SOCK->getOwner() );
		if (strcmp(attr_value, alternate_attrname_buf) != 0) {
			if ( stricmp(attr_value,"UNDEFINED")==0 ) {
					// If the user set the owner to be undefined, then
					// just fill in the value of Owner with the owner name
					// of the authenticated socket.
				attr_value  = alternate_attrname_buf;
			} else {
#if !defined(WIN32)
				errno = EACCES;
#endif
				dprintf(D_ALWAYS, "SetAttribute security violation: "
					"setting owner to %s when active owner is %s\n",
					attr_value, alternate_attrname_buf);
				return -1;
			}
		}
	}

	if (stricmp(attr_name, ATTR_OWNER) == 0) {
			// If we got this far, we're allowing the given value for
			// ATTR_OWNER to be set.  However, now, we should try to
			// insert a value for ATTR_USER, too, so that's always in
			// the job queue.
		int nice_user = 0;
		char user[_POSIX_PATH_MAX];
			// We can't just use attr_value, since it contains '"'
			// marks.  Carefully remove them here.
		char owner_buf[_POSIX_PATH_MAX];
		strcpy( owner_buf, attr_value );
		char *owner = owner_buf;
		if( *owner == '"' ) {
			owner++;
			int endquoteindex = strlen(owner) - 1;
			if ( endquoteindex >= 0 && owner[endquoteindex] == '"' ) {
				owner[endquoteindex] = '\0';
			}
		}
		GetAttributeInt( cluster_id, proc_id, ATTR_NICE_USER,
						 &nice_user );
		sprintf( user, "\"%s%s@%s\"", (nice_user) ? "nice-user." : "",
				 owner, scheduler.uidDomain() );
		SetAttribute( cluster_id, proc_id, ATTR_USER, user );
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
		char owner_buf[ _POSIX_PATH_MAX];
		char *owner = owner_buf;
		char user[_POSIX_PATH_MAX];
		bool nice_user = false;
		if( ! stricmp(attr_value, "TRUE") ) {
			nice_user = true;
		}
		if( GetAttributeString(cluster_id, proc_id, ATTR_OWNER, owner_buf)
			>= 0 ) {
				// carefully strip off any '"' marks from ATTR_OWNER.
			if( *owner == '"' ) {
				owner++;
				int endquoteindex = strlen(owner) - 1;
				if ( endquoteindex >= 0 && owner[endquoteindex] == '"' ) {
					owner[endquoteindex] = '\0';
				}
			}
			sprintf( user, "\"%s%s@%s\"", (nice_user) ? "nice-user." :
					 "", owner, scheduler.uidDomain() );
			SetAttribute( cluster_id, proc_id, ATTR_USER, user );
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

//	log = new LogSetAttribute(key, attr_name, attr_value);
//	JobQueue->AppendLog(log);
	JobQueue->SetAttribute(key, attr_name, attr_value);

	JobQueueDirty = true;

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
	char filename[_POSIX_PATH_MAX];
	sprintf (filename, "%s/mpp.%d.%d", Spool, cluster_id, proc_id);

	// Swith to root temporarily
	priv_state old_priv = set_root_priv();
	// Delete the file
	struct stat stat_buff;
	if (stat (filename, &stat_buff) == 0) {
		// If the exists, delete it
		if (unlink (filename)) {
			set_priv(old_priv);
			return -1;
		}
	}

	// Create the file
	int fd = open (filename, O_CREAT | O_WRONLY, S_IREAD | S_IWRITE);
	if (fd < 0) {
		set_priv(old_priv);
		return -1;
	}

	char * encoded_value = simple_encode (cluster_id+proc_id, pwd);
	int len = strlen(encoded_value);
	if (write (fd, encoded_value, len) != len) {
		set_priv(old_priv);
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
			// If the exists, delete it
		if( unlink( filename.Value()) < 0 ) {
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
	
	char filename[_POSIX_PATH_MAX];
	sprintf (filename, "%s/mpp.%d.%d", Spool, cluster_id, proc_id);
	int fd = open (filename, O_RDONLY);
	if (fd < 0) {
		set_priv(old_priv);
		return -1;
	}

	char buff[_POSIX_PATH_MAX];
	int bytes = read (fd, buff, _POSIX_PATH_MAX);
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
    
    sprintf(buff, "%c", c+(int)' ');
    result[j]=buff[0];
    
  }
  return result;
}


void
BeginTransaction()
{
	JobQueue->BeginTransaction();

	// note what time we started the transaction (used by SetTimerAttribute())
	xact_start_time = time( NULL );
}


void
CommitTransaction()
{
	JobQueue->CommitTransaction();

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
	if ( old_cluster_num != next_cluster_num ) {
		int cluster_id;
		int 	*numOfProcs = NULL;	
		int i;
		ClassAd *procad;
		ClassAd *clusterad;

		for ( cluster_id=old_cluster_num; cluster_id < next_cluster_num; cluster_id++ ) {
			if ( (JobQueue->LookupClassAd(IdToStr(cluster_id,-1), clusterad)) &&
			     (ClusterSizeHashTable->lookup(cluster_id,numOfProcs) != -1) )
			{
				for ( i = 0; i < *numOfProcs; i++ ) {
					if (JobQueue->LookupClassAd(IdToStr(cluster_id,i),procad)) {
						procad->ChainToAd(clusterad);
						ConvertOldJobAdAttrs(procad);
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
	char	key[_POSIX_PATH_MAX];
	char	*attr_val;

	strcpy(key, IdToStr(cluster_id,proc_id) );

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
	char	key[_POSIX_PATH_MAX];
	char	*attr_val;

	strcpy(key, IdToStr(cluster_id,proc_id) );

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
	char	key[_POSIX_PATH_MAX];
	char	*attr_val;

	strcpy(key, IdToStr(cluster_id,proc_id) );

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
	char	key[_POSIX_PATH_MAX];
	char	*attr_val;

	strcpy(key, IdToStr(cluster_id,proc_id) );

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		strcpy(val, attr_val);
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
	char	key[_POSIX_PATH_MAX];
	char	*attr_val;

	strcpy(key, IdToStr(cluster_id,proc_id) );

	if( JobQueue->LookupInTransaction(key, attr_name, attr_val) ) {
		*val = strdup(attr_val);
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

int
GetAttributeExpr(int cluster_id, int proc_id, const char *attr_name, char *val)
{
	ClassAd		*ad;
	char		key[_POSIX_PATH_MAX];
	ExprTree	*tree;
	char		*attr_val;

	strcpy(key, IdToStr(cluster_id,proc_id) );

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
	char				key[_POSIX_PATH_MAX];
//	LogDeleteAttribute	*log;
	char				*attr_val = NULL;

	strcpy(key, IdToStr(cluster_id,proc_id) );

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
GetJobAd(int cluster_id, int proc_id, bool expStartdAd)
{
	char	key[_POSIX_PATH_MAX];
	ClassAd	*ad;

	strcpy(key, IdToStr(cluster_id,proc_id) );
	if (JobQueue->LookupClassAd(key, ad)) {
		if ( !expStartdAd ) {
			// we're done, return the ad.
			return ad;
		}

		// if we made it here, we have the ad, but
		// expStartdAd is true.  so we need to dig up 
		// the startd ad which matches this job ad.
		char *bigbuf2 = NULL;
		char *attribute_value = NULL;
		PROC_ID job_id;
		shadow_rec *srec;
		ClassAd *startd_ad;
		ClassAd *expanded_ad;
		int index;
		char *left,*name,*right,*value,*tvalue;
		bool value_came_from_jobad;

		// we must make a deep copy of the job ad; we do not
		// want to expand the ad we have in memory.
		expanded_ad = new ClassAd(*ad);  

		job_id.cluster = cluster_id;
		job_id.proc = proc_id;

		// find the startd ad.  this is done differently if the job
		// is a globus universe jobs or not.
		int	job_universe;
		ad->LookupInteger(ATTR_JOB_UNIVERSE,job_universe);
		if ( job_universe == CONDOR_UNIVERSE_GRID ) {
			// Globus job... find "startd ad" via our simple
			// hash table.
			startd_ad = NULL;
			scheduler.resourcesByProcID->lookup(job_id,startd_ad);
		} else {
			// Not a Globus job... find startd ad via the shadow rec
			if ((srec = scheduler.FindSrecByProcID(job_id)) == NULL) {
				// pretty weird... no shadow, nothing we can do
				return expanded_ad;
			}
			if ( srec->match == NULL ) {
				// pretty weird... no match rec, nothing we can do
				// could be a PVM job?
				return expanded_ad;
			}
			startd_ad = srec->match->my_match_ad;
		}

			// Make a stringlist of all attribute names in job ad.
			// Note: ATTR_JOB_CMD must be first in AttrsToExpand...
		StringList AttrsToExpand;
		const char * curr_attr_to_expand;
		AttrsToExpand.append(ATTR_JOB_CMD);
		ad->ResetName();
		const char *attr_name = ad->NextNameOriginal();
		while ( attr_name ) {
			if ( stricmp(attr_name,ATTR_JOB_CMD) ) { 
				AttrsToExpand.append(attr_name);
			}
			attr_name = ad->NextNameOriginal();
		}

		index = -1;	
		AttrsToExpand.rewind();
		bool no_startd_ad = false;
		bool attribute_not_found = false;
		while ( !no_startd_ad && !attribute_not_found ) 
		{
			index++;
			curr_attr_to_expand = AttrsToExpand.next();

			if ( curr_attr_to_expand == NULL ) {
				// all done; no more attributes to try and expand
				break;
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
					bigbuf2 = (char *) malloc(strlen(curr_attr_to_expand)
											  + 3 // for the equal and the quotes
											  + strlen(attribute_value)
											  + 1); // for the null terminator.
					sprintf(bigbuf2,"%s=\"%s\"",curr_attr_to_expand,
						attribute_value);
					ad->Insert(bigbuf2);
					free(bigbuf2);
				}
			}

			while( !no_startd_ad && !attribute_not_found &&
					get_var(attribute_value,&left,&name,&right,NULL,true) )
			{
				if (!startd_ad && job_universe != CONDOR_UNIVERSE_GRID) {
					no_startd_ad = true;
					break;
				}

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

				if ( startd_ad ) {
						// We have a startd ad in memory, use it
					value = startd_ad->sPrintExpr(NULL,0,name);
					value_came_from_jobad = false;
				} else {
						// No startd ad -- use value from last match.
						// Note: we will only do this for GRID universe.
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
				if ( !value_came_from_jobad ) {
					MyString expr;
					expr = "MATCH_";
					expr += name;
						// If we are GRID universe, we must
						// store the values from the startd ad
						// persistantly to disk right now.  
						// If any other universe, just updating
						// our RAM image is fine since we will get
						// a new match every time.
					if ( job_universe == CONDOR_UNIVERSE_GRID ) {
						if ( SetAttribute(cluster_id,proc_id,expr.Value(),tvalue) < 0 )
						{
							EXCEPT("Failed to store %s into job ad %d.%d",
								expr.Value(),cluster_id,proc_id);
						}
					} else {
						expr += "=";
						expr += tvalue;
						ad->Insert(expr.Value());
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
				sprintf(bigbuf2,"%s%s%s",left,tvalue,right);
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

		if ( startd_ad && job_universe == CONDOR_UNIVERSE_GRID ) {
				// Can remove our matched ad since we stored all the
				// values we need from it into the job ad.
			RemoveMatchedAd(cluster_id,proc_id);
		}


		if ( no_startd_ad || attribute_not_found ) {
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
			ReliSock* saved_sock = Q_SOCK;
			Q_SOCK = NULL;
			holdJob(cluster_id, proc_id, hold_reason.Value());
			Q_SOCK = saved_sock;

			char buf[256];
			sprintf(buf,"Your job (%d.%d) is on hold",cluster_id,proc_id);
			FILE* email = email_user_open(ad,buf);
			if ( email ) {
				fprintf(email,"Condor failed to start your job %d.%d \n",
					cluster_id,proc_id);
				fprintf(email,"because job attribute %s contains $$(%s).\n",
					curr_attr_to_expand,name);
				fprintf(email,"\nAttribute $$(%s) cannot be expanded because",
					name);
				if ( attribute_not_found ) {
					fprintf(email,"\nthis attribute was not found in the "
							"machine ClassAd.\n");
				} else {
					fprintf(email,
							"\nthis feature is not supported by the version"
							" of the Condor Central Manager\n"
							"currently installed.  Please ask your site's"
							" Condor administrator to upgrade the\n"
							"Central Manager to a more recent revision.\n");
				}
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
					" for starter (opsys=%s): %s\n",
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

				ReliSock* saved_sock = Q_SOCK;
				Q_SOCK = NULL;
				holdJob(cluster_id, proc_id, hold_reason.Value());
				Q_SOCK = saved_sock;
			}

			if(opsys) free(opsys);
			if(startd_version) free(startd_version);
		}

		if ( attribute_value ) free(attribute_value);
		if ( bigbuf2 ) free (bigbuf2);

		if ( no_startd_ad || attribute_not_found )
			return NULL;
		else 
			return expanded_ad;
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

	sprintf(cluster,"%d.",c);
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


int
SendSpoolFile(char *filename)
{
	char path[_POSIX_PATH_MAX];

		/* We are passed in a filename to use to save the ICKPT file.
		   However, we should NOT trust this filename since it comes from 
		   the client!  So here we generate what we think the filename should
		   be based upon the current active_cluster_num in the transaction.
		   If the client does not send what we are expecting, we do the
		   paranoid thing and abort.  Once we are certain that my understanding
		   of this is correct, we could even just ignore the passed-in 
		   filename parameter completely. -Todd Tannenbaum, 2/2005
		*/
	strcpy(path,gen_ckpt_name(Spool,active_cluster_num,ICKPT,0));
	if ( filename && strcmp(filename, condor_basename(path)) ) {
		dprintf(D_ALWAYS, 
				"ERROR SendSpoolFile aborted due to suspicious path (%s)!\n",
				filename);
		return -1;
	}

	if ( !Q_SOCK ) {
		EXCEPT( "SendSpoolFile called when Q_SOCK is NULL" );
	}
	/* Tell client to go ahead with file transfer. */
	Q_SOCK->encode();
	Q_SOCK->put(0);
	Q_SOCK->eom();

	/* Read file size from client. */
	filesize_t	size;
	Q_SOCK->decode();
	if (Q_SOCK->get_file(&size, path) < 0) {
		dprintf(D_ALWAYS, "Failed to receive file from client in SendSpoolFile.\n");
		Q_SOCK->eom();
		return -1;
	}

	chmod(path,00755);

	// Q_SOCK->eom();
	dprintf(D_FULLDEBUG, "done with transfer, errno = %d\n", errno);
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
int get_job_prio(ClassAd *job, bool compute_autoclusters)
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

	dprintf(D_UPDOWN,"get_job_prio(): added PrioRec %d - id = %d.%d, owner = %s\n",N_PrioRecs,PrioRec[N_PrioRecs].id.cluster,PrioRec[N_PrioRecs].id.proc,PrioRec[N_PrioRecs].owner);
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
	static char birthdateAttr[200];
	static time_t bDay = 0;

		// Update ATTR_SCHEDD_BIRTHDATE in job ad at startup
	if (bDay == 0) {
		bDay = time(NULL);
		sprintf(birthdateAttr,"%s=%d",ATTR_SCHEDD_BIRTHDATE,(int)bDay);
	}
	job->Insert(birthdateAttr);

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
			char job_id[20];
			if ( job->LookupString( ATTR_GRID_JOB_ID, job_id,
									sizeof(job_id) ) )
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

/*
 * Find the job with the highest priority that matches with my_match_ad (which
 * is a startd ad).  We first try to find a match within the same cluster.  If
 * a match is not found, we scan the entire queue for jobs submitted from the
 * same owner which match.
 * If my_match_ad is NULL, this pool has an older negotiator,
 * so we just scan within the current cluster for the highest priority job.
 */
void FindRunnableJob(PROC_ID & jobid, const ClassAd* my_match_ad, 
					 char * user)
{
	char				constraintbuf[_POSIX_PATH_MAX];
	char				*constraint = constraintbuf;
	ClassAd				*ad;
	char				*the_at_sign;
	static char			nice_user_prefix[50];	// static since no need to recompute
	static int			nice_user_prefix_len = 0;

		// First, see if jobid points to a runnable job.  If it does,
		// there's no need to build up a PrioRec array, since we
		// requested matches in priority order in the first place.
		// And, we'd like to start the job we used to get the match if
		// possible, just to keep things simple.
	if (jobid.proc >= 0 && Runnable(&jobid)) {
		return;
	}


	// BIOTECH
	char *biotech = param("BIOTECH");
	if (biotech) {
		free(biotech);
		ad = GetNextJob(1);
		while (ad != NULL) {
			if ( Runnable(ad) ) {
				if ( (my_match_ad && 
					     (*ad == ((ClassAd &)(*my_match_ad)))) 
					 || (my_match_ad == NULL) ) 
				{
					ad->LookupInteger(ATTR_CLUSTER_ID, jobid.cluster);
					ad->LookupInteger(ATTR_PROC_ID, jobid.proc);
					break;
				}
			}
			ad = GetNextJob(0);
		}
		return;
	}


	if ( nice_user_prefix_len == 0 ) {
		strcpy(nice_user_prefix,NiceUserName);
		strcat(nice_user_prefix,".");
		nice_user_prefix_len = strlen(nice_user_prefix);
	}

		// indicate failure by setting proc to -1.  do this now
		// so if we bail out early anywhere, we say we failed.
	jobid.proc = -1;	
	
		// First, try to find a job in the same cluster
	N_PrioRecs = 0;
	ad = GetNextJobByCluster(jobid.cluster, 1);	// init a new scan
	while ( ad ) {
			// If job ad and resource ad match, put into prio rec array.
			// If we do not have a resource ad, it is because this pool
			// has an old negotiator -- so put it in prio rec array anyway.
		if ( (my_match_ad && (*ad == ((ClassAd &)(*my_match_ad)))) || (my_match_ad == NULL) ) 
		{
			get_job_prio(ad);
		} 
		ad = GetNextJobByCluster(jobid.cluster, 0);
	}

	if(N_PrioRecs == 0 && my_match_ad)
	// no more jobs in this cluster can run on this resource.
	// so if we have a my_match_ad, look for any job in any cluster in 
	// the queue with the same owner.
	{
		
		// Form a constraint.  We have been passed user, which is
		// owner@uid.  We want just owner, place a NULL at the '@'.
		if ( (the_at_sign = strchr(user,'@')) ) {
			*the_at_sign = '\0';
		}	
		
		// Now, deal with nice-user jobs.  If this user name has the
		// nice-user prefix, strip it, and add it to our constraint.
		// Otherwise, ad NiceUser == False to our constraint.
		if ( strncmp(user,nice_user_prefix,nice_user_prefix_len) == 0 ) {
			sprintf(constraint, "(%s == TRUE) && ", ATTR_NICE_USER);
			constraint += strlen(constraintbuf);
			user += nice_user_prefix_len;
		} else {
			sprintf(constraint, "(%s == FALSE) && ", ATTR_NICE_USER);
			constraint += strlen(constraintbuf);
		}

		sprintf(constraint, "((%s =?= \"%s\") || (%s =?= UNDEFINED && %s =?= \"%s\"))", 
			ATTR_ACCOUNTING_GROUP,user,ATTR_ACCOUNTING_GROUP,ATTR_OWNER,user);  // TODDCORE

			// OK, we're finished building the constraint now, so set
			// the constraint pointer to the head of the string.
		constraint = constraintbuf;
		
		// Put the '@' back if we changed it
		if ( the_at_sign ) {
			*the_at_sign = '@';
		}	

		ad = GetNextJobByConstraint(constraint, 1);	// init a new scan
		while ( ad ) {
			// If job ad and resource ad match, put into prio rec array.
			if ( (my_match_ad && (*ad == ((ClassAd &)(*my_match_ad)))) ) 
			{
				get_job_prio(ad);
			}
			ad = GetNextJobByConstraint(constraint, 0);
		}

	}

	if(N_PrioRecs == 0)
	// no more jobs to run anywhere.  nothing more to do.  failure.
	{
		return;
	}


	FindPrioJob(jobid);
}

int Runnable(ClassAd *job)
{
	int status, universe, cur, max;

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

	if ( job->LookupInteger(ATTR_CURRENT_HOSTS, cur) == 0 )
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",
				ATTR_CURRENT_HOSTS);
		return FALSE; 
	}
	if ( job->LookupInteger(ATTR_MAX_HOSTS, max) == 0 )
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",
				ATTR_MAX_HOSTS);
		return FALSE; 
	}
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

// --------------------------------------------------------------------------
// Write job ads to history file when they're destroyed
// --------------------------------------------------------------------------

static void AppendHistory(ClassAd* ad)
{
  bool failed = false;
  static bool sent_mail_about_bad_history = false;

  if (!JobHistoryFileName) return;
  dprintf(D_FULLDEBUG, "Saving classad to history file\n");

  // save job ad to the log
  // Note that we are passing O_LARGEFILE, which lets us deal with files
  // that are larger than 2GB. On systems where O_LARGEFILE isn't defined, 
  // the Condor source defines it to be 0 which has no effect. So we'll take
  // advantage of large files where we can, but not where we can't.
  int fd = open(JobHistoryFileName,
                O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE,
                0644);
  if (fd < 0) {
	dprintf(D_ALWAYS,"ERROR saving to history file (%s): %s\n",
			JobHistoryFileName, strerror(errno));
    failed = true;
  } else {
	  FILE *LogFile=fdopen(fd, "a");
      if (!LogFile) {
          dprintf(D_ALWAYS,"ERROR saving to history file (%s): %s\n",
                  JobHistoryFileName, strerror(errno));
          failed = true;
      } else {
          if (!ad->fPrint(LogFile)) {
              dprintf(D_ALWAYS, 
                      "ERROR: failed to write job class ad to history file %s\n",
                      JobHistoryFileName);
              fclose(LogFile);
              failed = true;
          } else {
              fprintf(LogFile,"***\n");   // separator
              fclose(LogFile);
          }
      }
  }

  if ( failed ) {
	  if ( !sent_mail_about_bad_history ) {
		  FILE* email_fp = email_admin_open("Failed to write to HISTORY file");
		  if ( email_fp ) {
			sent_mail_about_bad_history = true;
			fprintf(email_fp,
			 "Failed to write completed job class ad to HISTORY file:\n"
			 "      %s\n"
			 "If you do not wish for Condor to save completed job ClassAds\n"
			 "for later viewing via the condor_history command, you can \n"
			 "remove the 'HISTORY' parameter line specified in the condor_config\n"
			 "file(s) and issue a condor_reconfig command.\n"
			 ,JobHistoryFileName);
			email_close(email_fp);
		  }
	  }
  } else {
	  // did not fail, reset our email flag.
	  sent_mail_about_bad_history = false;
  }
  
  return;
}


void
dirtyJobQueue()
{
	JobQueueDirty = true;
}
