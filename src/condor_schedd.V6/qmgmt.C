/* 
** Copyright 1996 by Miron Livny and Jim Pruyne
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Pruyne
** 	        University of Wisconsin, Computer Sciences Dept.
** 
** Cleaned up for V6 by Derek Wright 7/3/97
** 
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_io.h"
#include <sys/stat.h>
#include "condor_fix_socket.h"
#if !defined(WIN32)
#include <netinet/in.h>
#include <sys/param.h>
#endif

#include "condor_debug.h"

static char *_FileName_ = __FILE__;	 /* Used by EXCEPT (see condor_debug.h) */

#include "qmgmt.h"
#include "condor_qmgr.h"
#include "log_transaction.h"
#include "log.h"
#include "classad_log.h"
#include "condor_qmgr.h"
#include "condor_updown.h"
#include "prio_rec.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_adtypes.h"

extern char *Spool;

extern "C" {
	char *gen_ckpt_name(char *, int, int, int);
	int  RemoveRemoteFile(char *, char *);
	int	 upDown_GetUserPriority(char*, int*);
	int	FileExists(char*, char*);
	char *sin_to_string(struct sockaddr_in *sin);
	int get_inet_address(struct in_addr* buffer);
	int	prio_compar(prio_rec*, prio_rec*);
	void abort_job_myself(PROC_ID job_id);
}

extern	int		Parse(const char*, ExprTree*&);
static ReliSock *Q_SOCK;

int		do_Q_request(ReliSock *);
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

static ClassAdLog *JobQueue = 0;
static int next_cluster_num = -1;
static int next_proc_num = 0;
static int active_cluster_num = -1;	// client is restricted to only insert jobs to the active cluster

class Service;

prio_rec	PrioRecArray[INITIAL_MAX_PRIO_REC];
prio_rec	* PrioRec = &PrioRecArray[0];
int			N_PrioRecs = 0;
static int 	MAX_PRIO_REC=INITIAL_MAX_PRIO_REC ;	// INITIAL_MAX_* in prio_rec.h


void
InitJobQueue(const char *job_queue_name)
{
	assert(!JobQueue);
	JobQueue = new ClassAdLog(job_queue_name);
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
	connection_state = CONNECTION_ACTIVE;
}


int
UnauthenticatedConnection()
{
	dprintf( D_ALWAYS, "Unable to authenticate connection.  Setting owner"
			" to \"nobody\"\n" );
	init_user_ids("nobody");
#if !defined(WIN32)
	active_owner_uid = get_user_uid();
#endif
	if (active_owner != 0)
		free(active_owner);
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

	/* user "nobody" represents an unauthenticated user */
	if (strcmp(active_owner, "nobody") == 0) {
		return 0;
	}

	if (rendevous_file == 0) {
		UnauthenticatedConnection();
		return 0;
	}

	if (stat(rendevous_file, &stat_buf) < 0) {
		UnauthenticatedConnection();
		return 0;
	}

	unlink(rendevous_file);

#if !defined(WIN32)
	if (stat_buf.st_uid != active_owner_uid && stat_buf.st_uid != 0 &&
		stat_buf.st_uid != get_condor_uid()) {
		UnauthenticatedConnection();
		return 0;
	}
#endif
	connection_state = CONNECTION_ACTIVE;
	return 0;
}

// Test if this owner matches my owner, so they're allowed to update me.
char	*super_users[] = {
#if defined(WIN32)
	"Administrator"
#else
	"root",
#endif
	"condor"
};

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

	// root, and condor always allowed to do updates.  Could maybe put
	// super_users in the config. file to give more flexibility to admins.

	for (i = 0; i < sizeof(super_users) / sizeof(super_users[0]); i++) {
		if (strcmp(test_owner, super_users[i]) == 0) {
			return 1;
		}
	}

	// If we don't have an Owner attribute, how can we deny service?
	if (ad->LookupString(ATTR_OWNER, my_owner) < 0) {
		return 1;
	}
	if (strcmp(my_owner, test_owner) != 0) {
#if !defined(WIN32)
		errno = EACCES;
#endif
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

	Q_SOCK = (ReliSock *)sock;

	InvalidateConnection();
	do {
		/* Probably should wrap a timer around this */
		rval = do_Q_request( (ReliSock *)sock );
	} while(rval >= 0);
	FreeConnection();
	dprintf(D_ALWAYS, "QMGR Connection closed\n");

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
InitializeConnection( char *owner, char *tmp_file )
{
	char	*new_file;

	new_file = tempnam("/tmp", "qmgr_");
	strcpy(tmp_file, new_file);
	// man page says string returned by tempnam has been malloc()'d
	free(new_file);
	init_user_ids( owner );
#if !defined(WIN32)
	active_owner_uid = get_user_uid();
	if (active_owner_uid < 0) {
		return -1;
	}
#endif
	active_owner = strdup( owner );
	rendevous_file = strdup( tmp_file );
	dprintf(D_ALWAYS, "QMGR InitializeConnection returning 0 (%s)\n",
			tmp_file);
	return 0;
}


int
NewCluster()
{
	ClassAd *ad;
	int		cluster_num;

	if (CheckConnection() < 0) {
		return -1;
	}

	if (next_cluster_num == -1) {
		next_cluster_num = 1;
		JobQueue->table.startIterations();
		while (JobQueue->table.iterate(ad) == 1) {
			ad->LookupInteger(ATTR_CLUSTER_ID, cluster_num);
			if (cluster_num >= next_cluster_num) {
				next_cluster_num = cluster_num + 1;
			}
		}
	}
	
	next_proc_num = 0;
	active_cluster_num = next_cluster_num++;
	return active_cluster_num;
}


int
NewProc(int cluster_id)
{
	int				proc_id;
	char			key[_POSIX_PATH_MAX];
	LogNewClassAd	*log;

	if (CheckConnection() < 0) {
		return -1;
	}
	if (cluster_id != active_cluster_num) {
#if !defined(WIN32)
		errno = EACCES;
#endif
		return -1;
	}
	proc_id = next_proc_num++;
	sprintf(key, "%d.%d", cluster_id, proc_id);
	log = new LogNewClassAd(key, JOB_ADTYPE, STARTD_ADTYPE);
	JobQueue->AppendLog(log);
	return proc_id;
}


int DestroyProc(int cluster_id, int proc_id)
{
	char				key[_POSIX_PATH_MAX];
	char				*ckpt_file_name;
	ClassAd				*ad = NULL;
	LogDestroyClassAd	*log;
	PROC_ID				job_id;

	if (CheckConnection() < 0) {
		return -1;
	}
	sprintf(key, "%d.%d", cluster_id, proc_id);
	if (JobQueue->table.lookup(HashKey(key), ad) != 0) {
		return -1;
	}

	// Only the owner can delete a proc.
	if (!OwnerCheck(ad, active_owner)) {
		return -1;
	}

	log = new LogDestroyClassAd(key);
	JobQueue->AppendLog(log);

	// notify schedd to abort job
	// should probably wait until we commit the transaction to do this...
	job_id.cluster = cluster_id;
	job_id.proc = proc_id;
	abort_job_myself( job_id );

	ckpt_file_name =
		gen_ckpt_name( Spool, cluster_id, proc_id, 0 );
	(void)unlink( ckpt_file_name );
	/* Also, try to get rid of it from the checkpoint server */
	RemoveRemoteFile(active_owner, ckpt_file_name);

	return 0;
}


int DestroyCluster(int cluster_id)
{
	ClassAd				*ad=NULL;
	int					c, proc_id;
	char				key[_POSIX_PATH_MAX];
	LogDestroyClassAd	*log;
	char				*ickpt_file_name;
    PROC_ID				job_id;

	if (CheckConnection() < 0) {
		return -1;
	}

	JobQueue->table.startIterations();

	// Find all jobs in this cluster and remove them.
	while(JobQueue->table.iterate(ad) == 1) {
		ad->LookupInteger(ATTR_CLUSTER_ID, c);
		if (c == cluster_id) {
			ad->LookupInteger(ATTR_PROC_ID, proc_id);

			// Only the owner can delete a cluster
			if (!OwnerCheck(ad, active_owner)) {
				return -1;
			}
			sprintf(key, "%d.%d", cluster_id, proc_id);
			log = new LogDestroyClassAd(key);
			JobQueue->AppendLog(log);

			// notify schedd to abort job
			// should probably wait until commit time to do this...
			job_id.cluster = cluster_id;
			job_id.proc = proc_id;
			abort_job_myself( job_id );
		}
	}

	ickpt_file_name =
		gen_ckpt_name( Spool, cluster_id, ICKPT, 0 );
	(void)unlink( ickpt_file_name );

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
	if (!tree->EvalTree(ad, &result)) {
		dprintf(D_ALWAYS, "can't evaluate constraint: %s\n", constraint);
		return false;
	}
	if (result.type == LX_BOOL) {
		return (bool)result.b;
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
	int			prev_cluster = -1, match;
    PROC_ID		job_id;

	if(CheckConnection() < 0) {
		return -1;
	}

	JobQueue->table.startIterations();

	while(JobQueue->table.iterate(ad) == 1) {
		if (OwnerCheck(ad, active_owner)) {
			if (EvalBool(ad, constraint)) {
				if (match) {
					ad->LookupInteger(ATTR_CLUSTER_ID, job_id.cluster);
					ad->LookupInteger(ATTR_PROC_ID, job_id.proc);

					sprintf(key, "%d.%d", job_id.cluster, job_id.proc);
					log = new LogDestroyClassAd(key);
					JobQueue->AppendLog(log);
					flag = 0;

					// notify schedd to abort job
					// should probably wait until commit time to do this...
					abort_job_myself( job_id );

					if (prev_cluster != job_id.cluster) {
						ickpt_file_name =
							gen_ckpt_name( Spool, job_id.cluster, ICKPT, 0 );
						(void)unlink( ickpt_file_name );
						prev_cluster = job_id.cluster;
					}
				}
			}
		}
	}
	if(flag) return -1;
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
	if (JobQueue->table.lookup(HashKey(key), ad) == 0) {
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
			dprintf(D_ALWAYS, "SetAttribute failed: job not found and cluster %d != active cluster %d\n",
					cluster_id, active_cluster_num);
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

	log = new LogSetAttribute(key, attr_name, attr_value);
	JobQueue->AppendLog(log);

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

	if (JobQueue->table.lookup(HashKey(key), ad) != 0) {
		return -1;
	}

	return ad->LookupFloat(attr_name, *val);
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

	if (JobQueue->table.lookup(HashKey(key), ad) != 0) {
		return -1;
	}

	return ad->LookupInteger(attr_name, *val);
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

	if (JobQueue->table.lookup(HashKey(key), ad) != 0) {
		return -1;
	}

	return ad->LookupString(attr_name, val);
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

	if (JobQueue->table.lookup(HashKey(key), ad) != 0) {
		return -1;
	}

	tree = ad->Lookup(attr_name);
	if (!tree) {
		return -1;
	}

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

	if (JobQueue->table.lookup(HashKey(key), ad) != 0) {
		if (JobQueue->LookupInTransaction(key, attr_name, attr_val) != 1) {
			return -1;
		}
	}

	if (!OwnerCheck(ad, active_owner)) {
		return -1;
	}

	log = new LogDeleteAttribute(key, attr_name);
	JobQueue->AppendLog(log);

	return 1;
}


ClassAd *
GetJobAd(int cluster_id, int proc_id)
{
	char	key[_POSIX_PATH_MAX];
	ClassAd	*ad;

	sprintf(key, "%d.%d", cluster_id, proc_id);
	if (JobQueue->table.lookup(HashKey(key), ad) == 1)
		return ad;
	else
		return NULL;
}


ClassAd *
GetJobByConstraint(const char *constraint)
{
	ClassAd	*ad;

	if(CheckConnection() < 0) {
		return NULL;
	}

	JobQueue->table.startIterations();
	while(JobQueue->table.iterate(ad) == 1) {
		if (EvalBool(ad, constraint)) {
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

	if(CheckConnection() < 0) {
		return NULL;
	}

	if (initScan) {
		JobQueue->table.startIterations();
	}

	while(JobQueue->table.iterate(ad) == 1) {
		if (!constraint || !constraint[0] || EvalBool(ad, constraint)) {
			return ad;
		}
	}
	return NULL;
}


void
FreeJobAd(ClassAd *ad)
{
	// NOOP
}


int GetJobList(const char *constraint, ClassAdList &list)
{
	int			flag = 1;
	ClassAd		*ad=NULL, *newad;

	if(CheckConnection() < 0) {
		return -1;
	}

	JobQueue->table.startIterations();

	while(JobQueue->table.iterate(ad) == 1) {
		if (!constraint || !constraint[0] || EvalBool(ad, constraint)) {
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
	int filesize, total=0, nbytes, written;
	FILE *fp;
	char path[_POSIX_PATH_MAX], buf[4*1024];

	if (strchr(filename, '/') != NULL) {
		dprintf(D_ALWAYS, "ReceiveFile called with a path (%s)!\n",
				filename);
		return -1;
	}

	sprintf(path, "%s/%s", Spool, filename);

	if ((fp = fopen(path, "w")) == NULL) {
		dprintf(D_ALWAYS, "Failed to open file %s.\n",
				path);
		return -1;
	}

	/* Tell client to go ahead with file transfer. */
	Q_SOCK->encode();
	Q_SOCK->put(0);
	Q_SOCK->eom();

	/* Read file size from client. */
	Q_SOCK->decode();
	if (!Q_SOCK->code(filesize)) {
		dprintf(D_ALWAYS, "Failed to receive file size from client in SendSpoolFile.\n");
		Q_SOCK->eom();
		return -1;
	}

	while (total < filesize &&
		   (nbytes = Q_SOCK->code_bytes(buf, sizeof(buf))) > 0) {
		dprintf(D_FULLDEBUG, "read %d bytes\n", nbytes);
		if ((written = fwrite(&buf, sizeof(char), nbytes, fp)) < nbytes) {
			dprintf(D_ALWAYS, "failed to write %d bytes (only wrote %d)\n",
					nbytes, written);
			EXCEPT("fwrite");
		}
		dprintf(D_FULLDEBUG, "wrote %d bytes\n", written);
		total += written;
	}
	Q_SOCK->eom();
	dprintf(D_FULLDEBUG, "done with transfer, errno = %d\n", errno);
	dprintf(D_FULLDEBUG, "successfully wrote %s (%d bytes)\n",
			path, total);

	if (fclose(fp) == EOF) {
		EXCEPT("fclose");
	}

	return total;
}

} /* should match the extern "C" */



void
PrintQ()
{
	ClassAd		*ad=NULL;

	dprintf(D_ALWAYS, "*******Job Queue*********\n");
	JobQueue->table.startIterations();
	while(JobQueue->table.iterate(ad) == 1) {
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
    char    buf[100], *owner;
    int     cur_hosts;
    int     max_hosts;

    job->LookupInteger(ATTR_JOB_STATUS, job_status);
    job->LookupInteger(ATTR_JOB_PRIO, job_prio);
    job->LookupInteger(ATTR_Q_DATE, q_date);
    job->LookupString(ATTR_OWNER, buf);
    owner = buf;
	job->LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	job->LookupInteger(ATTR_PROC_ID, id.proc);
    if (job->LookupInteger(ATTR_CURRENT_HOSTS, cur_hosts) < 0) {
        cur_hosts = ((job_status == RUNNING) ? 1 : 0);
    }
    if (job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) < 0) {
        max_hosts = ((job_status == IDLE || job_status == UNEXPANDED) ? 1 : 0);
    }

    // No longer judge whether or not a job can run by looking at its status.
    // Rather look at if it has all the hosts that it wanted.
    if (cur_hosts >= max_hosts) {
		dprintf (D_FULLDEBUG,"In get_job_prio() cur_hosts(%d)>=max_hosts(%d)\n",
								cur_hosts, max_hosts);
        return cur_hosts;
    }

#if 0
    prio = upDown_GetUserPriority(owner,&status); /* UPDOWN */
    if ( status  == Error )
    {
        dprintf(D_UPDOWN,"GetUserPriority returned error\n");
        dprintf(D_UPDOWN,"ERROR : ERROR \n");
        dprintf( D_ALWAYS,
                "job_prio: Can't find user priority for %s, assuming 0\n",
                owner );
        prio = 0;
    }

    PrioRec[N_PrioRecs].prio     = prio;
#endif

    PrioRec[N_PrioRecs].id       = id;
    PrioRec[N_PrioRecs].job_prio = job_prio;
    PrioRec[N_PrioRecs].status   = job_status;
    PrioRec[N_PrioRecs].qdate    = q_date;

#if 0
    if ( DebugFlags & D_UPDOWN )
    {
#endif
		if ( PrioRec[N_PrioRecs].owner ) {
			free( PrioRec[N_PrioRecs].owner );
		}
        PrioRec[N_PrioRecs].owner = strdup( owner );

#if 0
    }
#endif

	dprintf(D_UPDOWN,"get_job_prio(): added PrioRec %d - id = %d.%d, owner = %s\n",N_PrioRecs,PrioRec[N_PrioRecs].id.cluster,PrioRec[N_PrioRecs].id.proc,PrioRec[N_PrioRecs].owner);
    N_PrioRecs += 1;
	if ( N_PrioRecs == MAX_PRIO_REC ) {
		grow_prio_recs( 2 * N_PrioRecs );
	}

	return cur_hosts;
}

/*
** This is different from "job_prio()" because we want to send all jobs
** regardless of the state they are in.
*/

int
all_job_prio(int cluster, int proc)
{
    int job_prio;
    int job_status;
    PROC_ID job_id;
    int job_q_date;
    char    own_buf[100], *owner;
    GetAttributeInt(cluster, proc, ATTR_JOB_PRIO, &job_prio);
    GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &job_status);
    GetAttributeInt(cluster, proc, ATTR_Q_DATE, &job_q_date);
    GetAttributeString(cluster, proc, ATTR_OWNER, own_buf);
    owner = own_buf;
    job_id.cluster = cluster;
    job_id.proc = proc;

#if 0
    prio = upDown_GetUserPriority(owner, &status);
    if ( status == Error ) {
        dprintf( D_ALWAYS,
                "all_job_prio: Can't find user priority for %s, assuming 0\n",
                owner );
        prio = 0;
    }
    PrioRec[N_PrioRecs].prio = prio;
#endif

    PrioRec[N_PrioRecs].id = job_id;
    PrioRec[N_PrioRecs].job_prio = job_prio;
    PrioRec[N_PrioRecs].status = job_status;
    PrioRec[N_PrioRecs].qdate = job_q_date;
    N_PrioRecs += 1;
	if ( N_PrioRecs == MAX_PRIO_REC ) {
		grow_prio_recs( 2 * N_PrioRecs );
	}
	return 0;
}

int mark_idle(ClassAd *job)
{
    char    ckpt_name[MAXPATHLEN];
    int     status, cluster, proc;
    char    owner[_POSIX_PATH_MAX];

    job->LookupInteger(ATTR_JOB_STATUS, status);

    if( status != RUNNING ) {
        return -1;
    }

	job->LookupInteger(ATTR_CLUSTER_ID, cluster);
	job->LookupInteger(ATTR_PROC_ID, proc);

    strcpy(ckpt_name, gen_ckpt_name(Spool, cluster, proc,0) );

    job->LookupString(ATTR_OWNER, owner);

    if (FileExists(ckpt_name, owner)) {
        status = IDLE;
    } else {
        status = UNEXPANDED;
    }

    SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, status);
    SetAttributeInt(cluster, proc, ATTR_CURRENT_HOSTS, 0);
	return 0;
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
 * Weiru
 * This function only look at one cluster. Find the job in the cluster with the
 * highest priority.
 */
void FindRunnableJob(int c, int& rp)
{
	int					p;
	char				constraint[_POSIX_PATH_MAX];
	ClassAdList			joblist;
	ClassAd				*ad;

	sprintf(constraint, "%s = %d", ATTR_CLUSTER_ID, c);
	
	if (GetJobList(constraint, joblist) < 0) {
		rp = -1;
		return;
	}

	N_PrioRecs = 0;
	rp = -1;
	p = -1;
	joblist.Open();	// start at beginning of list
	while ((ad = joblist.Next()) != NULL) {
		ad->LookupInteger(ATTR_PROC_ID, p);
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
		if(prio_compar(&PrioRec[0], &PrioRec[i])==-1&&Runnable(&PrioRec[i].id))
		{
			PrioRec[0] = PrioRec[i];
		}
	}
	p = PrioRec[0].id.proc;
}
