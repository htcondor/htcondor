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

#include "condor_common.h"
#include "condor_io.h"
#include "string_list.h"
#include "condor_debug.h"
#include "condor_config.h"

#if defined(GSS_AUTHENTICATION)
#include "auth_sock.h"
#else
#define AuthSock ReliSock
#endif

static char *_FileName_ = __FILE__;	 /* Used by EXCEPT (see condor_debug.h) */

#include "qmgmt.h"
#include "condor_qmgr.h"
#include "log_transaction.h"
#include "log.h"
#include "classad_collection.h"
#include "condor_updown.h"
#include "prio_rec.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_ckpt_name.h"

extern char *Spool;
extern char* JobHistoryFileName;

extern "C" {
/*
	char *gen_ckpt_name(char *, int, int, int);
	int  RemoveRemoteFile(char *, char *);
	int	 upDown_GetUserPriority(char*, int*);
	int	FileExists(char*, char*);
	char *sin_to_string(struct sockaddr_in *sin);
	int get_inet_address(struct in_addr* buffer);
*/
	int	prio_compar(prio_rec*, prio_rec*);
}

extern	int		Parse(const char*, ExprTree*&);
extern  void    cleanup_ckpt_files(int, int, char*);
static AuthSock *Q_SOCK;
//mju took out GSSAuthenticated to use Q_SOCK->isAuthenticated() instead.
//int GSSAuthenticated = 0;

int		do_Q_request(AuthSock *);
void	FindRunnableJob(int, int&);
void	FindPrioJob(int&);
int		Runnable(PROC_ID*);
int		get_job_prio(ClassAd *ad);

enum {
	CONNECTION_CLOSED,
	CONNECTION_ACTIVE,
} connection_state = CONNECTION_ACTIVE;

#if !defined(WIN32)
uid_t	active_owner_uid = 0;
#endif
char	*active_owner = 0;
char	*rendevous_file = 0;

static ClassAdCollection *JobQueue = 0;
static int next_cluster_num = -1;
static int next_proc_num = 0;
static int active_cluster_num = -1;	// client is restricted to only insert jobs to the active cluster
static bool JobQueueDirty = false;

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

static char	**super_users = NULL;
static int	num_super_users = 0;
static char *default_super_user = 
#if defined(WIN32)
	"Administrator";
#else
	"root";
#endif

static int allow_remote_submit = FALSE;

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

	allow_remote_submit = FALSE;
	tmp = param( "ALLOW_REMOTE_SUBMIT" );
	if( tmp ) {
		if( *tmp == 'T' || *tmp == 't' ) {
			allow_remote_submit = TRUE;
		}			
		free( tmp );
	}
}


void
InitJobQueue(const char *job_queue_name)
{
	assert(!JobQueue);
	JobQueue = new ClassAdCollection(job_queue_name);
	ClusterSizeHashTable = new ClusterSizeHashTable_t(37,compute_clustersize_hash);

	/* We read/initialize the header ad in the job queue here.  Currently,
	   this header ad just stores the next available cluster number. */
	ClassAd *ad;
	int 	cluster_num;
	int		stored_cluster_num;
	int 	*numOfProcs = NULL;	
	bool	CreatedAd = false;
	char	cluster_str[40];
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

	next_cluster_num = 1;
	JobQueue->StartIterateAllClassAds();
	while (JobQueue->IterateAllClassAds(ad)) {
		if (ad->LookupInteger(ATTR_CLUSTER_ID, cluster_num) == 1) {
			if (cluster_num >= next_cluster_num) {
				next_cluster_num = cluster_num + 1;
			}
			if ( ClusterSizeHashTable->lookup(cluster_num,numOfProcs) == -1 ) {
				// First proc we've seen in this cluster; set size to 1
				ClusterSizeHashTable->insert(cluster_num,1);
			} else {
				// We've seen this cluster_num go by before; increment proc count
				(*numOfProcs)++;
			}
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
}


void
CleanJobQueue()
{
	if (JobQueueDirty) {
		dprintf(D_ALWAYS, "Cleaning job queue...\n");
		JobQueue->TruncLog();
		JobQueueDirty = false;
	} else {
		dprintf(D_ALWAYS,
				"CleanJobQueue() doing nothing since !JobQueueDirty.\n");
	}
}


void
InvalidateConnection()
{
	if (active_owner != 0) {
		free(active_owner);
	}
	active_owner = 0;
#if !defined(WIN32)
	active_owner_uid = 0;
#endif
	connection_state = CONNECTION_CLOSED;
	active_cluster_num = -1;
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

/*
   This makes the connection look active, so any local calls to manipulate
   the queue succeed.  This is the normal state of the connection except when
   we receive a request from the outside.
*/

void
FreeConnection()
{
	if (active_owner != 0) {
		free(active_owner);
	}
	active_owner = 0;
#if !defined(WIN32)
	active_owner_uid = 0;
#endif
	uninit_user_ids();
	connection_state = CONNECTION_ACTIVE;
}


int
UnauthenticatedConnection()
{
	dprintf(D_FULLDEBUG,"in UnauthenticatedConnection--\n" );
	dprintf( D_ALWAYS, "Unable to authenticate connection.  Setting active "
			"owner to \"nobody\"\n" );
	init_user_ids("nobody");
#if !defined(WIN32)
	active_owner_uid = get_user_uid();
#endif
	if (active_owner != 0)
		free(active_owner);
	dprintf(D_FULLDEBUG,"in UnauthenticatedConnection, setting active_owner "
			"to \"nobody\"\n" );
	active_owner = strdup( "nobody" );
	if (rendevous_file != 0)
		free(rendevous_file);
	rendevous_file = 0;
	connection_state = CONNECTION_ACTIVE;
	return 0;
}


int
ValidateRendevous()
{
	struct stat stat_buf;

	dprintf(D_FULLDEBUG,"in ValidateRendevous, active_owner is \"%s\"\n",
			active_owner );
	/* user "nobody" represents an unauthenticated user */
	if (strcmp(active_owner, "nobody") == 0) {
		return 0;
	}

//mju: changed logic to use Q_SOCK->isAuthenticated()
//	if (rendevous_file == 0 && !GSSAuthenticated) {
	if (rendevous_file == 0 && !Q_SOCK->isAuthenticated() ) {
		UnauthenticatedConnection();
		return 0;
	}

#if !defined(WIN32)
	// note we do an lstat() instead of a stat() so we do _not_ follow
	// a symbolic link.  this prevents a security attack via sym links.
//	if ( !GSSAuthenticated ) {
	if ( !Q_SOCK->isAuthenticated() ) {
		if (lstat(rendevous_file, &stat_buf) < 0) {
			UnauthenticatedConnection();
			return 0;
		}
	}
	
#endif

//	if ( !GSSAuthenticated ) {
	if ( !Q_SOCK->isAuthenticated() ) {
		unlink(rendevous_file);
	}


#if !defined(WIN32)
//	if ( !GSSAuthenticated ) {
	if ( !Q_SOCK->isAuthenticated() ) {
		// Authentication should fail if a) owner match fails, or b) the
		// file is either a hard or soft link (no links allowed because they
		// could spoof the owner match).  -Todd 3/98
		if ( (stat_buf.st_uid != active_owner_uid) ||
			(stat_buf.st_nlink > 1) ||		// check for hard link
			(S_ISLNK(stat_buf.st_mode)) ) {
			UnauthenticatedConnection();
			return 0;
		}
	}
#endif

	connection_state = CONNECTION_ACTIVE;
	return 0;
}


// Test if this owner matches my owner, so they're allowed to update me.
int
OwnerCheck(ClassAd *ad, char *test_owner)
{
	char	my_owner[_POSIX_PATH_MAX];
	int		i;

	// If we get passed a null test_owner, its probably an internal call,
	// and we let internal callers do anything.
	if (test_owner == 0) {
		return 1;
	}

	// The super users are always allowed to do updates.  They are
	// specified with the "SUPER_USERS" string list in the config
	// file.  Defaults to root and condor.
	for( i=0; i<num_super_users; i++) {
		if( strcmp( test_owner, super_users[i] ) == 0 ) {
			return 1;
		}
	}

#if !defined(WIN32) 
		// If we're not root or condor, only allow qmgmt writes from
		// the UID we're running as.
	uid_t 	my_uid = get_my_uid();
	if( my_uid != 0 && my_uid != get_real_condor_uid() ) {
		if( active_owner_uid == my_uid ) {
			return 1;
		} 
		else {
//			if ( GSSAuthenticated ) {
			if ( Q_SOCK->isAuthenticated() ) {
				dprintf( D_FULLDEBUG,"OwnerCheck authorized %s for remote submit\n",
						test_owner );
				return( 1 );
			}
#if !defined(WIN32)
			errno = EACCES;
#endif
			dprintf( D_FULLDEBUG, "OwnerCheck: reject test_owner: %s non-super\n",
					test_owner );
			dprintf( D_FULLDEBUG,"OwnerCheck: my_uid: %d, active_owner_uid: %d\n",
					my_uid,active_owner_uid );
			return 0;
		}
	}
#endif

	if( !allow_remote_submit && !strcmp(test_owner, "nobody") ) {
#if !defined(WIN32)
		errno = EACCES;
#endif
		dprintf( D_FULLDEBUG, "OwnerCheck: reject %s \"nobody\"\n", test_owner );
		return 0;
	} 

		// If we don't have an Owner attribute (or classad) and we've 
		// gotten this far, how can we deny service?
	if( !ad ) {
		return 1;
	}
	if( ad->LookupString(ATTR_OWNER, my_owner) == 0 ) {
		return 1;
	}

		// Finally, compare the owner of the ad with the entity trying
		// to connect to the queue.
	if (strcmp(my_owner, test_owner) != 0) {
#if !defined(WIN32)
		errno = EACCES;
#endif
		dprintf( D_FULLDEBUG, "ad owner: %s, queue submit owner: %s\n",
				my_owner, test_owner );
		return 0;
	} else {
		return 1;
	}
}


int
CheckConnection()
{

	if (connection_state == CONNECTION_ACTIVE) {
		return 0;
	}
	if (connection_state == CONNECTION_CLOSED && rendevous_file != 0) {
		return ValidateRendevous();
	}
	if ( Q_SOCK->isAuthenticated() ) {
		return 0;
	}
	return -1;
}


/* We want to be able to call these things even from code linked in which
   is written in C, so we make them C declarations
*/
extern "C" {

int
handle_q(Service *, int, Stream *sock)
{
	int	rval;

	JobQueue->BeginTransaction();

	Q_SOCK = (AuthSock *)sock;

	InvalidateConnection();
	do {
		/* Probably should wrap a timer around this */
		rval = do_Q_request( (AuthSock *)sock );
	} while(rval >= 0);
	FreeConnection();
	dprintf(D_FULLDEBUG, "QMGR Connection closed\n");

	// Abort any uncompleted transaction.  The transaction should
	// be committed in CloseConnection().
	JobQueue->AbortTransaction();

	if (rendevous_file != 0) {
		unlink(rendevous_file);
		free(rendevous_file);
		rendevous_file = 0;
	}
	return 0;
}


int
InitializeConnection( char *owner, char *tmp_file, int auth )
{
	char	*new_file;

	if ( !auth ) {
		new_file = tempnam("/tmp", "qmgr_");
		strcpy(tmp_file, new_file);
		// man page says string returned by tempnam has been malloc()'d
		free(new_file);
	}
	else {
//mju took this out, should only be authorizing them DURING auth process!
//		GSSAuthenticated = 1;
	}

	init_user_ids( owner );
#if !defined(WIN32)
	active_owner_uid = get_user_uid();
	if (active_owner_uid == (uid_t)-1) {
		UnauthenticatedConnection();
		return 0;
	}
#endif
	active_owner = strdup( owner );
	if ( !auth ) {
		rendevous_file = strdup( tmp_file );
		dprintf(D_FULLDEBUG, "QMGR InitializeConnection returning 0 (%s)\n",
			tmp_file);
	}
	else {
		dprintf(D_FULLDEBUG, "QMGR InitializeConnection returning 0 "
			"(NO temp file, GSS authenticated)\n" );
	}
	return 0;
}


int
NewCluster()
{
	LogSetAttribute *log;
	char cluster_str[40];
	
	if (CheckConnection() < 0) {
		dprintf( D_FULLDEBUG, "NewCluser(): CheckConnection failed\n" );
		return -1;
	}

	if( !OwnerCheck(NULL, active_owner) ) {
		dprintf( D_FULLDEBUG, "NewCluser(): OwnerCheck failed\n" );
		return -1;
	}

	next_proc_num = 0;
	active_cluster_num = next_cluster_num++;
	sprintf(cluster_str, "%d", next_cluster_num);
//	log = new LogSetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);
//	JobQueue->AppendLog(log);
	JobQueue->SetAttribute(HeaderKey, ATTR_NEXT_CLUSTER_NUM, cluster_str);
	return active_cluster_num;
}


int
NewProc(int cluster_id)
{
	int				proc_id;
	char			key[_POSIX_PATH_MAX];
	LogNewClassAd	*log;
	int				*numOfProcs = NULL;

	if (CheckConnection() < 0) {
		return -1;
	}

	if( !OwnerCheck(NULL, active_owner) ) {
		return -1;
	}

	if ((cluster_id != active_cluster_num) || (cluster_id < 1)) {
#if !defined(WIN32)
		errno = EACCES;
#endif
		return -1;
	}
	proc_id = next_proc_num++;
	sprintf(key, "%d.%d", cluster_id, proc_id);
//	log = new LogNewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);
//	JobQueue->AppendLog(log);
	JobQueue->NewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);

	if ( ClusterSizeHashTable->lookup(cluster_id,numOfProcs) == -1 ) {
		// First proc we've seen in this cluster; set size to 1
		ClusterSizeHashTable->insert(cluster_id,1);
	} else {
		// We've seen this cluster_num go by before; increment proc count
		(*numOfProcs)++;
	}

	return proc_id;
}


int DestroyProc(int cluster_id, int proc_id)
{
	char				key[_POSIX_PATH_MAX];
	char				*ckpt_file_name;
	ClassAd				*ad = NULL;
	LogDestroyClassAd	*log;
	int					*numOfProcs = NULL;

	if (CheckConnection() < 0) {
		return -1;
	}
	sprintf(key, "%d.%d", cluster_id, proc_id);
	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	// Only the owner can delete a proc.
	if (!OwnerCheck(ad, active_owner)) {
		return -1;
	}

	// Remove checkpoint files
	cleanup_ckpt_files(cluster_id,proc_id,active_owner);

    // Append to history file
    AppendHistory(ad);

//	log = new LogDestroyClassAd(key);
//	JobQueue->AppendLog(log);
	JobQueue->DestroyClassAd(key);

	if ( ClusterSizeHashTable->lookup(cluster_id,numOfProcs) != -1 ) {
		// We've seen this cluster_num go by before; increment proc count
		// NOTICE that numOfProcs is a _reference_ to an int which we
		// fetched out of the hash table via the call to lookup() above.
		(*numOfProcs)--;

		// If this is the last job in the cluster, remove the initial 
		//    checkpoint file and the entry in the ClusterSizeHashTable.
		if ( *numOfProcs == 0 ) {
			ClusterSizeHashTable->remove(cluster_id);
			ckpt_file_name = gen_ckpt_name( Spool, cluster_id, ICKPT, 0 );
			(void)unlink( ckpt_file_name );
		}
	}

	JobQueueDirty = true;

	return 0;
}


int DestroyCluster(int cluster_id)
{
	ClassAd				*ad=NULL;
	int					c, proc_id;
	char				key[_POSIX_PATH_MAX];
	LogDestroyClassAd	*log;
	char				*ickpt_file_name;

	if (CheckConnection() < 0) {
		return -1;
	}

	JobQueue->StartIterateAllClassAds();

	// Find all jobs in this cluster and remove them.
	while(JobQueue->IterateAllClassAds(ad)) {
		if (ad->LookupInteger(ATTR_CLUSTER_ID, c) == 1) {
			if (c == cluster_id) {
				ad->LookupInteger(ATTR_PROC_ID, proc_id);

				// Only the owner can delete a cluster
				if (!OwnerCheck(ad, active_owner)) {
					return -1;
				}

				// Apend to history file
                AppendHistory(ad);

				sprintf(key, "%d.%d", cluster_id, proc_id);
//				log = new LogDestroyClassAd(key);
//				JobQueue->AppendLog(log);
				JobQueue->DestroyClassAd(key);
			}
		}
	}

	ickpt_file_name =
		gen_ckpt_name( Spool, cluster_id, ICKPT, 0 );
	(void)unlink( ickpt_file_name );

	ClusterSizeHashTable->remove(cluster_id);

	JobQueueDirty = true; 

	return 0;
}


static bool EvalBool(ClassAd *ad, const char *constraint)
{
	ExprTree *tree;
	EvalResult result;

	if (Parse(constraint, tree) != 0) {
		dprintf(D_ALWAYS, "can't parse constraint: %s\n", constraint);
		return false;
	}

	// Evaluate constraint with ad in the target scope so that constraints
	// have the same semantics as the collector queries.  --RR
	if (!tree->EvalTree(NULL, ad, &result)) {
		dprintf(D_ALWAYS, "can't evaluate constraint: %s\n", constraint);
		delete tree;
		return false;
	}
	delete tree;
	if (result.type == LX_INTEGER) {
		return (bool)result.i;
	}
	dprintf(D_ALWAYS, "contraint (%s) does not evaluate to bool\n", constraint);
	return false;
}

int DestroyClusterByConstraint(const char* constraint)
{
	int			flag = 1;
	ClassAd		*ad=NULL;
	char		key[_POSIX_PATH_MAX];
	LogRecord	*log;
	char		*ickpt_file_name;
	int			prev_cluster = -1;
    PROC_ID		job_id;

	if(CheckConnection() < 0) {
		return -1;
	}

	JobQueue->StartIterateAllClassAds();

	while(JobQueue->IterateAllClassAds(ad)) {
		// check cluster first to avoid header ad
		if (ad->LookupInteger(ATTR_CLUSTER_ID, job_id.cluster) == 1) {
			if (OwnerCheck(ad, active_owner)) {
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


int
SetAttributeByConstraint(const char *constraint, const char *attr_name, char *attr_value)
{
	ClassAd	*ad;
	int cluster_num, proc_num;	
	int found_one = 0, had_error = 0;

	if(CheckConnection() < 0) {
		return -1;
	}

	JobQueue->StartIterateAllClassAds();
	while(JobQueue->IterateAllClassAds(ad)) {
		// check for CLUSTER_ID to avoid queue header ad
		if ((ad->LookupInteger(ATTR_CLUSTER_ID, cluster_num) == 1) &&
			(ad->LookupInteger(ATTR_PROC_ID, proc_num) == 1) &&
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
SetAttribute(int cluster_id, int proc_id, const char *attr_name, char *attr_value)
{
	LogSetAttribute	*log;
	char			key[_POSIX_PATH_MAX];
	ClassAd			*ad;
	
	if (CheckConnection() < 0) {
		return -1;
	}

	sprintf(key, "%d.%d", cluster_id, proc_id);
	if (JobQueue->LookupClassAd(key, ad)) {
		if (!OwnerCheck(ad, active_owner)) {
			dprintf(D_ALWAYS, "OwnerCheck(%s) failed in SetAttribute for job %d.%d\n",
					active_owner, cluster_id, proc_id);
			return -1;
		}
	} else {
		if (cluster_id != active_cluster_num) {
#if !defined(WIN32)
			errno = EACCES;
#endif
			return -1;
		}
	}
		
	// check for security violations
	if (strcmp(attr_name, ATTR_OWNER) == 0) {
		char *test_owner = new char [strlen(active_owner)+3];
		sprintf(test_owner, "\"%s\"", active_owner);
		if (strcmp(attr_value, test_owner) != 0) {
#if !defined(WIN32)
			errno = EACCES;
#endif
			dprintf(D_ALWAYS, "SetAttribute security violation: setting owner to %s when active owner is %s\n",
					attr_value, test_owner);
			delete [] test_owner;
			return -1;
		}
		delete [] test_owner;
	} else if (strcmp(attr_name, ATTR_CLUSTER_ID) == 0) {
		if (atoi(attr_value) != cluster_id) {
#if !defined(WIN32)
			errno = EACCES;
#endif
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ClusterId to incorrect value (%d!=%d)\n",
				atoi(attr_value), cluster_id);
			return -1;
		}
	} else if (strcmp(attr_name, ATTR_PROC_ID) == 0) {
		if (atoi(attr_value) != proc_id) {
#if !defined(WIN32)
			errno = EACCES;
#endif
			dprintf(D_ALWAYS, "SetAttribute security violation: setting ProcId to incorrect value (%d!=%d)\n",
				atoi(attr_value), proc_id);
			return -1;
		}
	}

//	log = new LogSetAttribute(key, attr_name, attr_value);
//	JobQueue->AppendLog(log);
	JobQueue->SetAttribute(key, attr_name, attr_value);

	JobQueueDirty = true;

	return 0;
}


int
CloseConnection()
{
	JobQueue->CommitTransaction();
	/* Just force the clean-up code to happen */
	return -1;
}


int
GetAttributeFloat(int cluster_id, int proc_id, const char *attr_name, float *val)
{
	ClassAd	*ad;
	char	key[_POSIX_PATH_MAX];
	char	*attr_val;
	int		rval;

	sprintf(key, "%d.%d", cluster_id, proc_id);

	rval = JobQueue->LookupInTransaction(key, attr_name, attr_val);
	switch (rval) {
	case 1:
		sscanf(attr_val, "%f", val);
		return 1;
	case 0:
		break;
	default:
		return -1;
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
	int		rval;

	sprintf(key, "%d.%d", cluster_id, proc_id);

	rval = JobQueue->LookupInTransaction(key, attr_name, attr_val);
	switch (rval) {
	case 1:
		sscanf(attr_val, "%d", val);
		return 1;
	case 0:
		break;
	default:
		return -1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	if (ad->LookupInteger(attr_name, *val) == 1) return 0;
	return -1;
}


int
GetAttributeString(int cluster_id, int proc_id, const char *attr_name, char *val)
{
	ClassAd	*ad;
	char	key[_POSIX_PATH_MAX];
	char	*attr_val;
	int		rval;

	sprintf(key, "%d.%d", cluster_id, proc_id);

	rval = JobQueue->LookupInTransaction(key, attr_name, attr_val);
	switch (rval) {
	case 1:
		strcpy(val, attr_val);
		return 1;
	case 0:
		break;
	default:
		return -1;
	}

	if (!JobQueue->LookupClassAd(key, ad)) {
		return -1;
	}

	if (ad->LookupString(attr_name, val) == 1) return 0;
	return -1;
}


int
GetAttributeExpr(int cluster_id, int proc_id, const char *attr_name, char *val)
{
	ClassAd		*ad;
	char		key[_POSIX_PATH_MAX];
	ExprTree	*tree;
	char		*attr_val;
	int			rval;

	sprintf(key, "%d.%d", cluster_id, proc_id);

	rval = JobQueue->LookupInTransaction(key, attr_name, attr_val);
	switch (rval) {
	case 1:
		strcpy(val, attr_val);
		return 1;
	case 0:
		break;
	default:
		return -1;
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
	LogDeleteAttribute	*log;
	char				*attr_val;

	sprintf(key, "%d.%d", cluster_id, proc_id);

	if (!JobQueue->LookupClassAd(key, ad)) {
		if (JobQueue->LookupInTransaction(key, attr_name, attr_val) != 1) {
			return -1;
		}
	}

	if (!OwnerCheck(ad, active_owner)) {
		return -1;
	}

//	log = new LogDeleteAttribute(key, attr_name);
//	JobQueue->AppendLog(log);
	JobQueue->DeleteAttribute(key, attr_name);

	JobQueueDirty = true;

	return 1;
}


ClassAd *
GetJobAd(int cluster_id, int proc_id)
{
	char	key[_POSIX_PATH_MAX];
	ClassAd	*ad;

	sprintf(key, "%d.%d", cluster_id, proc_id);
	if (JobQueue->LookupClassAd(key, ad))
		return ad;
	else
		return NULL;
}


ClassAd *
GetJobByConstraint(const char *constraint)
{
	ClassAd	*ad;
	int cluster_num;	// check for cluster num to avoid header ad

	if(CheckConnection() < 0) {
		return NULL;
	}

	JobQueue->StartIterateAllClassAds();
	while(JobQueue->IterateAllClassAds(ad)) {
		if ((ad->LookupInteger(ATTR_CLUSTER_ID, cluster_num) == 1) &&
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
	int cluster_num;	// check for cluster num to avoid header ad

	if(CheckConnection() < 0) {
		return NULL;
	}

	if (initScan) {
		JobQueue->StartIterateAllClassAds();
	}

	while(JobQueue->IterateAllClassAds(ad)) {
		if ((ad->LookupInteger(ATTR_CLUSTER_ID, cluster_num) == 1) &&
			(!constraint || !constraint[0] || EvalBool(ad, constraint))) {
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


int GetJobList(const char *constraint, ClassAdList &list)
{
	int			flag = 1;
	ClassAd		*ad=NULL, *newad;
	int			cluster_num;	// check for cluster num to avoid header ad

	if(CheckConnection() < 0) {
		return -1;
	}

	JobQueue->StartIterateAllClassAds();

	while(JobQueue->IterateAllClassAds(ad)) {
		if ((ad->LookupInteger(ATTR_CLUSTER_ID, cluster_num) == 1) &&
			(!constraint || !constraint[0] || EvalBool(ad, constraint))) {
			flag = 0;
			newad = new ClassAd(*ad);	// insert copy so list doesn't
			list.Insert(newad);			// destroy the original ad
		}
	}
	if(flag) return -1;
	return 0;
}




int
SendSpoolFile(char *filename)
{
	char path[_POSIX_PATH_MAX];

	if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
		dprintf(D_ALWAYS, "ReceiveFile called with a path (%s)!\n",
				filename);
		return -1;
	}

	sprintf(path, "%s/%s", Spool, filename);

	/* Tell client to go ahead with file transfer. */
	Q_SOCK->encode();
	Q_SOCK->put(0);
	Q_SOCK->eom();

	/* Read file size from client. */
	Q_SOCK->decode();
	if (!Q_SOCK->get_file(path)) {
		dprintf(D_ALWAYS, "Failed to receive file from client in SendSpoolFile.\n");
		Q_SOCK->eom();
		return -1;
	}

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

	buf[0] = '\0';
	owner[0] = '\0';

    job->LookupInteger(ATTR_JOB_STATUS, job_status);
    job->LookupInteger(ATTR_JOB_PRIO, job_prio);
    job->LookupInteger(ATTR_Q_DATE, q_date);

	if( job->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
		strcpy(owner,NiceUserName);
		strcat(owner,".");
	}

    job->LookupString(ATTR_OWNER, buf);
	strcat(owner,buf);

	job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	job->LookupInteger(ATTR_PROC_ID, id.proc);
    if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) == 0) {
        cur_hosts = ((job_status == RUNNING) ? 1 : 0);
    }
    if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) == 0) {
        max_hosts = ((job_status == IDLE || job_status == UNEXPANDED) ? 1 : 0);
    }

	
    // No longer judge whether or not a job can run by looking at its status.
    // Rather look at if it has all the hosts that it wanted.
    if (cur_hosts>=max_hosts || job_status==HELD)
        return cur_hosts;

    PrioRec[N_PrioRecs].id       = id;
    PrioRec[N_PrioRecs].job_prio = job_prio;
    PrioRec[N_PrioRecs].status   = job_status;
    PrioRec[N_PrioRecs].qdate    = q_date;

	strcpy(PrioRec[N_PrioRecs].owner,owner);

	dprintf(D_UPDOWN,"get_job_prio(): added PrioRec %d - id = %d.%d, owner = %s\n",N_PrioRecs,PrioRec[N_PrioRecs].id.cluster,PrioRec[N_PrioRecs].id.proc,PrioRec[N_PrioRecs].owner);
    N_PrioRecs += 1;
	if ( N_PrioRecs == MAX_PRIO_REC ) {
		grow_prio_recs( 2 * N_PrioRecs );
	}

	return cur_hosts;
}

extern void mark_job_stopped(PROC_ID* job_id);

int mark_idle(ClassAd *job)
{
    int     status, cluster, proc;
	PROC_ID	job_id;

	job->LookupInteger(ATTR_CLUSTER_ID, cluster);
	job->LookupInteger(ATTR_PROC_ID, proc);
    job->LookupInteger(ATTR_JOB_STATUS, status);

	job_id.cluster = cluster;
	job_id.proc = proc;

	if ( status == COMPLETED || status == REMOVED ) {
		DestroyProc(cluster,proc);
	}
	else if ( status == UNEXPANDED ) {
		SetAttributeInt(cluster,proc,ATTR_JOB_STATUS,IDLE);
	}
	else if ( status == RUNNING ) {
		mark_job_stopped(&job_id);
	}
		
	return 1;
}

void
WalkJobQueue(scan_func func)
{
	ClassAd *ad;
	int rval = 0;
	int cluster_num;	// check for cluster num to avoid header ad

	ad = GetNextJob(1);
	while (ad != NULL && rval >= 0) {
		if (ad->LookupInteger(ATTR_CLUSTER_ID, cluster_num) == 1) {
			rval = func(ad);
		}
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
 * Weiru
 * This function only look at one cluster. Find the job in the cluster with the
 * highest priority.
 */
void FindRunnableJob(int c, int& rp)
{
	char				constraint[_POSIX_PATH_MAX];
	ClassAdList			joblist;
	ClassAd				*ad;

	sprintf(constraint, "%s == %d", ATTR_CLUSTER_ID, c);
	
	if (GetJobList(constraint, joblist) < 0) {
		rp = -1;
		return;
	}

	N_PrioRecs = 0;
	rp = -1;
	joblist.Open();	// start at beginning of list
	while ((ad = joblist.Next()) != NULL) {
		get_job_prio(ad);
	}
	if(N_PrioRecs == 0)
	// no more jobs to run
	{
		rp = -1;
		return;
	}
	FindPrioJob(rp);
}

int Runnable(PROC_ID* id)
{
	int		cur, max;					// current hosts and max hosts
	int		status, universe;
	
	dprintf (D_FULLDEBUG, "Job %d.%d:", id->cluster, id->proc);

	if(id->cluster < 1 || id->proc < 0)
	{
		dprintf (D_FULLDEBUG | D_NOHEADER, " not runnable\n");
		return FALSE;
	}
	
	if(GetAttributeInt(id->cluster, id->proc, ATTR_JOB_STATUS, &status) < 0)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",ATTR_JOB_STATUS);
		return FALSE;
	}
	if(status == HELD)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (HELD)\n");
		return FALSE;
	}
	if(status == REMOVED)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (REMOVED)\n");
		return FALSE;
	}

	if(GetAttributeInt(id->cluster, id->proc, ATTR_JOB_UNIVERSE,
					   &universe) < 0)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",
				ATTR_JOB_UNIVERSE);
		return FALSE;
	}
	if(universe == SCHED_UNIVERSE)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (SCHED_UNIVERSE)\n");
		return FALSE;
	}
	
	if(GetAttributeInt(id->cluster, id->proc, ATTR_CURRENT_HOSTS, &cur) < 0)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",
				ATTR_CURRENT_HOSTS);
		return FALSE; 
	}
	if(GetAttributeInt(id->cluster, id->proc, ATTR_MAX_HOSTS, &max) < 0)
	{
		dprintf(D_FULLDEBUG | D_NOHEADER," not runnable (no %s)\n",
				ATTR_MAX_HOSTS);
		return FALSE; 
	}
	if(cur < max)
	{
		dprintf (D_FULLDEBUG | D_NOHEADER, " is runnable\n");
		return TRUE;
	}
	dprintf (D_FULLDEBUG | D_NOHEADER, " not runnable (default rule)\n");
	return FALSE;
}

// From the priority records, find the runnable job with the highest priority
// use the function prio_compar. By runnable I mean that its status is either
// UNEXPANDED or IDLE.
void FindPrioJob(int& p)
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
			p = -1;
			return;
		}
	}
	for(i = 1; i < N_PrioRecs; i++)
	{
		if(PrioRec[0].id.proc == PrioRec[i].id.proc)
		{
			continue;
		}
		if(prio_compar(&PrioRec[0], &PrioRec[i])!=-1&&Runnable(&PrioRec[i].id))
		{
			PrioRec[0] = PrioRec[i];
		}
	}
	p = PrioRec[0].id.proc;
}

// --------------------------------------------------------------------------
// Write job ads to history file when they're destroyed
// --------------------------------------------------------------------------

static void AppendHistory(ClassAd* ad)
{
  if (!JobHistoryFileName) return;
  dprintf(D_FULLDEBUG, "Saving classad to history file\n");

  // insert completion time
  char tmp[512];
  sprintf(tmp,"%s = %d",ATTR_COMPLETION_DATE,time(NULL));
  ad->InsertOrUpdate(tmp);
 
  // save job ad to the log
  FILE* LogFile=fopen(JobHistoryFileName,"a");
  if ( !LogFile ) {
	dprintf(D_ALWAYS,"ERROR saving to history file; cannot open %s\n",JobHistoryFileName);
  }

  if (!ad->fPrint(LogFile)) {
    dprintf(D_ALWAYS, "ERROR in Scheduler::LogMatchEnd - failed to write clas ad to log file %s\n",JobHistoryFileName);
	fclose(LogFile);
    return;
  }
  
  fprintf(LogFile,"***\n");   // separator
  fclose(LogFile);
  return;
}
