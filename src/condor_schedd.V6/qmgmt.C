/* 
*e Copyright 1996 by Miron Livny and Jim Pruyne
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
** 
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_xdr.h"
#include <sys/stat.h>
#include <sys/param.h>

#include "condor_debug.h"

static char *_FileName_ = __FILE__;	 /* Used by EXCEPT (see condor_debug.h) */

#include "qmgmt.h"
#include "qmgmt_transaction.h"
#include "log.h"
#include "qmgmt_log.h"
#include "condor_qmgr.h"
#include "condor_updown.h"
#include "prio_rec.h"

extern char *Spool;

extern "C" {
	int get_condor_uid();
	uid_t get_user_uid( char * );
	char *gen_ckpt_name(char *, int, int, int);
	int  RemoveRemoteFile(char *, char *);
	int	 upDown_GetUserPriority(char*, int*);
	int	FileExists(char*, char*);
	int	set_root_euid();
	int	xdr_proc(XDR* xdrs, PROC* proc);
	int	prio_compar(prio_rec*, prio_rec*);
}

extern	int		Parse(const char*, ExprTree*&);

int 	do_Q_request(XDR *);
void	FindRunnableJob(int, int&);
void	FindPrioJob(int&);
int		Runnable(PROC_ID*);
int		get_job_prio(int, int);

enum {
	CONNECTION_CLOSED,
	CONNECTION_ACTIVE,
} connection_state = CONNECTION_ACTIVE;

uid_t	active_owner_uid = 0;
char	*active_owner = 0;
char	*rendevous_file = 0;

Transaction *tr = 0;

void
InvalidateConnection()
{
	if (active_owner != 0) {
		free(active_owner);
	}
	active_owner = 0;
	active_owner_uid = 0;
	connection_state = CONNECTION_CLOSED;
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
	active_owner_uid = 0;
	connection_state = CONNECTION_ACTIVE;
}


int
ValidateRendevous()
{
	struct stat stat_buf;

	if (rendevous_file == 0) {
		return -1;
	}

	if (stat(rendevous_file, &stat_buf) < 0) {
		return -1;
	}

	unlink(rendevous_file);

	if (stat_buf.st_uid != active_owner_uid && stat_buf.st_uid != 0 &&
		stat_buf.st_uid != get_condor_uid()) {
		return -1;
	}
	connection_state = CONNECTION_ACTIVE;
	return 0;
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


static int log_fd;

LogState(int fd)
{
	Cluster		*cl;
	Job			*job;
	LogRecord	*log;
	char		attr_name[_POSIX_PATH_MAX];
	char		attr_val[_POSIX_PATH_MAX];
	char		*ptr;
	int			got_name;

	for (cl = FirstCluster(); cl != 0; cl = cl->get_next()) {
		log = new LogNewCluster(cl->get_cluster_num());
		log->Write(fd);
		delete log;
		for (job = cl->FirstJob(); job != 0; job = job->get_next()) {
			log = new LogNewProc(cl->get_cluster_num(), job->get_proc());
			log->Write(fd);
			delete log;
			got_name = job->FirstAttributeName(attr_name);
			while (got_name >= 0) {
				attr_val[0] = 0;
				got_name = job->GetAttributeExpr(attr_name, attr_val);
				// Expr returned is in the form Name = Value, we need only
				// the value portion.
				for (ptr = attr_val; *ptr && *ptr != '='; ptr++) {
					// NULL BODY
				}
				ptr++;  // Skip the '='
				for (; *ptr && *ptr == ' '; ptr++) {
					// NULL BODY (again)
				}
				if (got_name >= 0 && *ptr) {
					log = new LogSetAttribute(cl->get_cluster_num(),
											  job->get_proc(),
											  attr_name, ptr);
					log->Write(fd);
					delete log;
					got_name = job->NextAttributeName(attr_name);
				}
			}
		}
	}
}

extern "C" {

TruncLog(char *log_name)
{
	char	tmp_log_name[_POSIX_PATH_MAX];
	int new_log_fd;

	sprintf(tmp_log_name, "%s.tmp", log_name);
	new_log_fd = open(tmp_log_name, O_RDWR | O_CREAT, 0600);
	if (new_log_fd < 0) {
		return new_log_fd;
	}
	LogState(new_log_fd);
	close(log_fd);
	log_fd = new_log_fd;
	if (rename(tmp_log_name, log_name) < 0) {
		return -1;
	}
}


ReadLog(char *log_name)
{
	LogRecord		*log_rec;

	log_fd = open(log_name, O_RDWR | O_CREAT, 0600);
	if (log_fd < 0) {
		return log_fd;
	}
	// Read all of the log records
	while((log_rec = ReadLogEntry(log_fd)) != 0) {
		log_rec->Play();
		delete log_rec;
	}
	TruncLog(log_name);
}

} /* extern "C" */

void
AppendLog(LogRecord *log, Job *job, Cluster *cl)
{

	  // If a transaction is active, we log to the transaction, and mark
	  // the job and cluster dirty.  Otherwise, we log directly to the
	  // file and commit the transaction.
	if (tr != 0) {
		tr->AppendLog(log);
		tr->DirtyJob(job);
		tr->DirtyCluster(cl);
	} else {
		log->Write(log_fd);
		if (job != 0) {
			job->Commit();
		}
		if (cl != 0) {
			cl->Commit();
		}
		fsync(log_fd);
	}
}


/* We want to be able to call these things even from code linked in which
   is written in C, so we make them C declarations
*/
extern "C" {

int
handle_q(XDR *xdrs, struct sockaddr_in* = NULL)
{
	int	rval;

	tr = new Transaction;

	// I think this has to be here because xdr_Init() (called in 
	// qmgr_lib_support.c) does a skiprecord.
	xdrrec_endofrecord(xdrs, TRUE);

	InvalidateConnection();
	do {
		/* Probably should wrap a timer around this */
		rval = do_Q_request( xdrs );
	} while(rval >= 0);
	FreeConnection();

	// tr should be free'd in CloseConnection(), if it wasn't, we must
	// rollback
	if (tr != 0) {
		tr->Rollback();
		delete tr;
		tr = 0;
	}
	dprintf(D_ALWAYS, "Connection closed, Q is:\n");
	if (rendevous_file != 0) {
		unlink(rendevous_file);
		free(rendevous_file);
		rendevous_file = 0;
	}
}


int
InitializeConnection( char *owner, char *tmp_file )
{
	char	*new_file;

	new_file = tempnam("/tmp", "qmgr_");
	strcpy(tmp_file, new_file);
	// man page says string returned by tempnam has been malloc()'d
	free(new_file);
	active_owner_uid = get_user_uid( owner );
	if (active_owner_uid < 0) {
		return -1;
	}
	active_owner = strdup( owner );
	rendevous_file = strdup( tmp_file );
	dprintf(D_ALWAYS, "InitializeConnection returning 0 (%s)\n",
			tmp_file);
	return 0;
}



int
NewCluster()
{
	Cluster *cl;
	LogNewCluster	*log;

	if (CheckConnection() < 0) {
		return -1;
	}

	cl = new Cluster();
	if (cl != 0) {
		log = new LogNewCluster(cl->get_cluster_num());
		AppendLog(log, 0, cl);
		return cl->get_cluster_num();
	} else {
		return -1;
	}
}


int
NewProc(int cluster_id)
{
	int		rval;
	Cluster	*cl;
	Job		*job;
	LogNewProc	*log;

	if (CheckConnection() < 0) {
		return -1;
	}

	cl = FindCluster(cluster_id);
	if (cl == 0) {
		return -1;
	}
	job = cl->new_job();
	if (job == 0) {
		return -1;
	}
	log = new LogNewProc(cluster_id, job->get_proc());
	AppendLog(log, job, cl);
	rval = job->get_proc();
	return rval;
}


int DestroyProc(int cluster_id, int proc_id)
{
	int		rval;
	Job		*job;
	LogDestroyProc	*log;

	if (CheckConnection() < 0) {
		return -1;
	}
	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}

	// Only the owner can delete a proc.
	if (!job->OwnerCheck(active_owner)) {
		return -1;
	}

	job->DeleteSelf();
	log = new LogDestroyProc(cluster_id, proc_id);
	AppendLog(log, job, 0);
	return 0;
}


int DestroyCluster(int cluster_id)
{
	LogDestroyCluster	*log;
	int					rval;
	Cluster				*cl;

	if (CheckConnection() < 0) {
		return -1;
	}

	cl = FindCluster(cluster_id);

	if (cl == 0) {
		return -1;
	}
	
	cl->DeleteSelf();
	log = new LogDestroyCluster(cluster_id);
	AppendLog(log, 0, cl);
	return 0;
}



int DestroyClusterByConstraint(const char* constraint)
{
	Cluster*			cl;
	LogDestroyCluster*	log;
	int					flag = 1;

	if(CheckConnection() < 0)
	{
		return -1;
	}
	for(cl = FirstCluster(); cl; cl = cl->get_next())
	{
		if(cl->OwnerCheck(active_owner) && cl->ApplyToCluster(constraint))
		// delete the whole cluster because one cluster can have only one owner
		{
			cl->DeleteSelf();
			log = new LogDestroyCluster(cl->get_cluster_num());
			AppendLog(log, 0, cl);
			flag = 0;
		}
	}
	if(flag)
	{
		return -1;
	}
	return 0;
}


int
SetAttribute(int cluster_id, int proc_id, char *attr_name, char *attr_value)
{
	Job	*job;
	LogSetAttribute	*log;

	if (CheckConnection() < 0) {
		return -1;
	}

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	if (job->SetAttribute(attr_name, attr_value) < 0) {
		return -1;
	}
	log = new LogSetAttribute(cluster_id, proc_id, attr_name, attr_value);
	AppendLog(log, job, 0);
	return 0;
}


int
CloseConnection()
{
	if (tr != 0) {
		tr->Commit(log_fd);
		delete tr;
		tr = 0;
	}
	/* Just force the clean-up code to happen */
	return -1;
}


int
GetAttributeFloat(int cluster_id, int proc_id, char *attr_name, float *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttribute(attr_name, val);
}


int
GetAttributeInt(int cluster_id, int proc_id, char *attr_name, int *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttribute(attr_name, val);
}


int
GetAttributeString(int cluster_id, int proc_id, char *attr_name, char *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttribute(attr_name, val);
}


int
GetAttributeExpr(int cluster_id, int proc_id, char *attr_name, char *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttributeExpr(attr_name, val);
}


int
DeleteAttribute(int cluster_id, int proc_id, char *attr_name)
{
	int		rval;
	Job		*job;
	LogDeleteAttribute *log;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	rval = job->DeleteAttribute(attr_name);
	if (rval >= 0) {
		log = new LogDeleteAttribute(cluster_id, proc_id, attr_name);
		AppendLog(log, job, 0);
	}
	return rval;
}


int GetNextJob(int cluster_id, int proc_id, int *next_cluster, int *next_proc)
{
	Cluster	*cl;
	Job		*job;

// 	There's probably a more elegant way to implement this.  The semantics
//	are:
//			if (cluster_id == -1) assume the first cluster
//			if (proc_id == -1) assume the first proc in the cluster
//			Otherwise, get the proc which follows (cluster_id, proc_id)
//			even if it means scanning to the next cluster, but not if
//			(cluster_id, proc_id) doesn't exist.
   


	if (cluster_id == -1) {
		cl = FirstCluster();
	} else {
		cl = FindCluster(cluster_id);
	}
	if (cl == 0) {
		return -1;
	}

	if (proc_id == -1) {
		job = cl->FirstJob();
		if (job == 0) {
			return -1;
		}
		*next_cluster = cl->get_cluster_num();
		*next_proc = job->get_proc();
		return 0;
	}

	job = cl->FindJob(proc_id);
	if (job == 0) {
		return -1;
	}

	job = job->get_next();
	if (job != 0) {
		*next_cluster = cl->get_cluster_num();
		*next_proc = job->get_proc();
		return 0;
	}
	cl = cl->get_next();
	if (cl == 0) {
		return -1;
	}
	job = cl->FirstJob();
	if (job == 0) {
		return -1;
	}
	*next_cluster = cl->get_cluster_num();
	*next_proc = job->get_proc();
	return 0;
}


int
FirstAttribute(int cluster_id, int proc_id, char *attr_name)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->FirstAttributeName(attr_name);
}


int
NextAttribute(int cluster_id, int proc_id, char *attr_name)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->NextAttributeName(attr_name);
}

} /* should match the extern "C" */


////////////Beginning of class implementations////////////////

Job::Job(Cluster *cl, int pr)
{
	Job	*other;
	char	tmp[100];

	cluster = cl;
	proc = pr;
	ad = new ClassAd();
	other = cl->get_job_list();
	if (other != 0) {
		next = other;
		prev = other->prev;
		other->prev->next = this;
		other->prev = this;
	} else {
		next = this;
		prev = this;
	}

	if (prev->next != this) {
		dprintf(D_ALWAYS, "WARNING, Queue seems bogus in Job::Job\n");
	}

	hold_ad = 0;

	/* This should be temporary */
	sprintf(tmp, "%d", cl->get_cluster_num());
	SetAttribute("ClusterId", tmp);
	sprintf(tmp, "%d", proc);
	SetAttribute("ProcId", tmp);
	if (active_owner != 0) {
		SetAttribute("Owner", active_owner);
	}
	
	  // Commit() looks at state, gotta clear it first in case it has
	  // an old value.
	state = 0;
	Commit();
	state = NEW_BORN;
}


Job::~Job()
{
	Job		*head_job;
	char	*ckpt_file_name;
	char	owner[_POSIX_PATH_MAX];

	ckpt_file_name =
		gen_ckpt_name( Spool, cluster->get_cluster_num(), proc, 0 );
	(void)unlink( ckpt_file_name );
	/* Also, try to get rid of it from the checkpoint server */
	GetAttribute("Owner", owner);
	RemoveRemoteFile(owner, ckpt_file_name);

	if (ad != 0) {
		delete ad;
		ad = 0;
	}

	if (hold_ad != 0) {
		delete hold_ad;
		hold_ad = 0;
	}

	next->prev = prev;
	prev->next = next;

	// If by removing this job, we're removing the last job in the cluster
	// get rid of the cluster record.
	if (cluster->get_job_list() != 0 && cluster->FirstJob() == 0) {
		delete cluster;
	} else {
		// If the cluster is not empty, we must make sure that we can
		// never find this job in the cluster's lookup cache (used in
		// Cluster::FindJob).
		cluster->clear_lookup_cache(this);
	}
}


Job *
Job::get_next()
{
	if (next != cluster->get_job_list()) {
		return next;
	} else {
		return 0;
	}
}


int
Job::SetAttribute(char *name, char *value)
{
	char *tmp_expr;
	ExprTree *expr_tree;

	if (!OwnerCheck(active_owner)) {
		return -1;
	}

	// If its already there, we don't want to duplicate it, so just remove
	// it right away.  This also copies our classad so we can rollback.
	DeleteAttribute( name );

	tmp_expr = new char [strlen(name) + strlen(value) + 4];
	sprintf(tmp_expr, "%s = %s", name, value);
		
	Parse(tmp_expr, expr_tree);
		
	delete tmp_expr;
		
	ad->Insert(expr_tree);
}


int
Job::DeleteAttribute(char *name)
{
	char *tmp_expr;
	ExprTree *expr_tree;

	if (!OwnerCheck(active_owner)) {
		return -1;
	}

	if (hold_ad == 0) {
		hold_ad = new ClassAd(*ad);
	}

	expr_tree = ad->Lookup( name );

	if (expr_tree != 0) {
		ad->Delete( name );
		return 0;
	} else {
		return -1;
	}
}


int
Job::EvalAttribute(char *name, EvalResult &result)
{
	ExprTree	*expr_tree;

	expr_tree = ad->Lookup( name );
	if (expr_tree == 0) {
		return -1;
	}

	expr_tree->EvalTree(ad, &result);
}


int
Job::GetAttribute(char *name, float *val)
{
	EvalResult	result;

	if (EvalAttribute(name, result) < 0) {
		return -1;
	}
	if (result.type != LX_FLOAT) {
		return -1;
	}
	*val = result.f;
	return 0;
}


int
Job::GetAttribute(char *name, int *val)
{
	EvalResult	result;

	if (EvalAttribute(name, result) < 0) {
		return -1;
	}
	if (result.type != LX_INTEGER) {
		return -1;
	}
	*val = result.i;
	return 0;
}


int
Job::GetAttribute(char *name, char *val)
{
	EvalResult	result;
	char		*ptr;

	if (ad == 0) {
		*val = 0;
		return -1;
	}
	if (EvalAttribute(name, result) < 0) {
		return -1;
	}
	if (result.type != LX_STRING) {
		return -1;
	}
#if 0
	// The classad puts quotation marks around strings which we'd rather
	// not make people look at.
	ptr = result.s + 1;
	ptr[strlen(ptr) - 1] = '\0';
	strcpy(val, ptr);
#else
	strcpy(val, result.s);
#endif
	return 0;
}


int
Job::GetAttributeExpr(char *name, char *val)
{
	ExprTree	*expr_tree;

	expr_tree = ad->Lookup( name );
	if (expr_tree == 0) {
		return -1;
	}

	*val = '\0';
	expr_tree->PrintToStr(val);
	return 0;
}


int
Job::FirstAttributeName(char *attr_name)
{
	ad->ResetName();
	return NextAttributeName(attr_name);
}


int
Job::NextAttributeName(char *attr_name)
{
	char	*this_name;

	this_name = ad->NextName();
	if (this_name == 0) {
		return -1;
	}
	strcpy(attr_name, this_name);
	return 0;
}


// Test if this owner matches my owner, so they're allowed to update me.
char	*super_users[] = {
	"root",
	"condor"
	};
int
Job::OwnerCheck(char *test_owner)
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
	if (GetAttribute("Owner", my_owner) < 0) {
		return 1;
	}
	if (strcmp(my_owner, test_owner) != 0) {
		errno = EACCES;
		return 0;
	} else {
		return 1;
	}
}


void
Job::Commit()
{
	if (hold_ad != 0) {
		delete hold_ad;
	}
	hold_ad = 0;

	if (prev->next != this) {
		dprintf(D_ALWAYS, "WARNING, Queue seems bogus in Commit\n");
		abort();
	}


	// Commit()ing brings us to middle age
	if (state & NEW_BORN) {
		state &= ~NEW_BORN;
	}
	// If we've been killed, go ahead and delete the record
	if (state & DEATHS_DOOR) {
		delete this;
	}
}


// Weiru
// Return TRUE if the constraint applies; FALSE otherwise.
int
Job::Apply(const char* constraint)
{
	ExprTree*	tree;
	EvalResult	result;

	if(Parse(constraint, tree) != 0)
	{
		dprintf(D_ALWAYS, "Can't parse constraint %s\n", constraint);
		return FALSE;
	}
	if(!tree->EvalTree(ad, &result))
	{
		dprintf(D_ALWAYS, "Can't evaluate constraint %s\n", constraint);
		return FALSE;
	}
	if(result.type == LX_BOOL)
	{
		return result.b;
	}
	dprintf(D_ALWAYS, "constraint must be BOOLEAN expr (%s)\n", constraint);
	return FALSE;
}


void
Job::Rollback()
{

	if (state & DEATHS_DOOR) {
		state &= ~DEATHS_DOOR;
	}
	// If we were just created, and then have to rollback, we have to
	// delete ourself and not go on further.
	if (state & NEW_BORN) {
		delete this;
		return;
	}

	if (ad != 0) {
		delete ad;
	}
	ad = hold_ad;
	hold_ad = 0;
}


void
Job::print()
{
	if (prev->next != this) {
		dprintf(D_ALWAYS, "WARNING, Queue seems bogus!!!\n");
	}
	ad->fPrint(stdout);
}


static int next_cluster_num = 1;

int
SetNextClusterNum(int new_value)
{
	int rval = next_cluster_num;
	next_cluster_num = new_value;
	return rval;
}


static Cluster *ClusterList = 0;
static Cluster  *LastCluster = 0;

Cluster::Cluster(int new_cluster_num)
{

	/* If the user didn't give a cluster number, just take the current one */
	if (new_cluster_num == -1) {
		cluster_num = next_cluster_num++;
	} else {
		/* If the user gave a cluster, use that one, and make the next new
		   cluster higher than the current */
		cluster_num = new_cluster_num;
		if (cluster_num >= next_cluster_num) {
			next_cluster_num = cluster_num + 1;
		}
	}
	next_cluster = 0;
	if (LastCluster != 0) {
		LastCluster->next_cluster = this;
	}
	if (ClusterList == 0) {
		ClusterList = this;
	}
	LastCluster = this;

	job_list = 0;
	last_lookup = 0;
	ad = 0;
	next_proc = -1;
	/* Force a dummy onto the job list */
	new_job();
	state = NEW_BORN;
}


Cluster::~Cluster()
{
	Cluster	*other;
	Job		*job, *job1;
	char	*ickpt_file_name;
	char	owner[_POSIX_PATH_MAX];

	ickpt_file_name =
		gen_ckpt_name( Spool, cluster_num, ICKPT, 0 );
	(void)unlink( ickpt_file_name );
	/* Also, try to get rid of it from the checkpoint server */
	if (job_list) {
		job_list->GetAttribute("Owner", owner);
		RemoveRemoteFile(owner, ickpt_file_name);
	}

	// If this is the last cluster in the list, we have to reset the last ptr.
	if (LastCluster == this) {
		// If we're first in the list, then the list will be empty, and we
		// can just set last to be 0.  Otherwise, we must walk the list, and
		// set last to be our predecessor.
		if (ClusterList != this) {
			for (other = ClusterList; other != 0; 
				 other = other->next_cluster) {
				if (other->next_cluster == this) {
					LastCluster = other;
					break;
				}
			}
		} else {
			LastCluster = 0;
		}
	}

	if (ClusterList == this) {
		ClusterList = next_cluster;
	} else {
		for (other = ClusterList; other->next_cluster != 0; 
			 other = other->next_cluster) {
			if (other->next_cluster == this) {
				other->next_cluster = next_cluster;
				break;
			}
		}
	}

	job = job_list;
	job_list = 0;
	while( job != 0 && job->get_next() != job) {
		job1 = job->get_next();
		delete job;
		job = job1;
	}
	if (job != 0) {
		delete job;
	}

	if (ad != 0) {
		delete ad;
		ad = 0;
	}
}


void
Cluster::SetAttribute(char *name, char *value)
{
}


Job *
Cluster::new_job(int proc_num)
{
	int	new_proc_num;
	Job	*the_job;

	if (proc_num == -1) {
		new_proc_num = next_proc;
		next_proc++;
	} else {
		new_proc_num = proc_num;
		if (new_proc_num >= next_proc) {
			next_proc = new_proc_num + 1;
		}
	}
	the_job = new Job(this, new_proc_num);
	if (job_list == 0) {
		job_list = the_job;
	}

	last_lookup = the_job;

	return the_job;
}


Job *
Cluster::FindJob(int proc_num)
{
	Job *job;

	// last_lookup provides a cache of one job.  Often, we'll end up hunting
	// for the same job many times (e.g. setting many attributes on the
	// same job).  This will save us hunting the entire list every time.
	if (last_lookup != 0) {
		if (last_lookup->get_proc() == proc_num) {
			return last_lookup;
		}
	}

	for (job = job_list; job != 0; job = job->get_next()) {
		if (job->get_proc() == proc_num) {
			return job;
		}
	}
	return 0;
}


Job *
Cluster::FirstJob()
{
	Job		*job;

	job = job_list->get_next();
	if (job != job_list) {
		return job;
	} else {
		return 0;
	}
}


void
Cluster::print()
{
	Job	*job;

	dprintf(D_ALWAYS, "----------\n");
	for (job = job_list->get_next(); job != 0; job = job->get_next()) {
		printf("Job %d.%d\n", cluster_num, job->get_proc());
		job->print();
	}
	dprintf(D_ALWAYS, "----------\n");
	fflush(stdout);
}


void
Cluster::Commit()
{
	// Commit()ing brings us to middle age
	if (state & NEW_BORN) {
		state &= ~NEW_BORN;
	}
	// If we've been killed, go ahead and delete the record
	if (state & DEATHS_DOOR) {
		delete this;
	}
}


// weiru
// Assuming that the user is smart enough to tell the constraint is the same for
// the whole cluster. So we only evaluate the constraint against the first job 
// in the cluster.
int
Cluster::ApplyToCluster(const char* constraint)
{
	Job*	j;

	if((j = FirstJob()))
	{
		return j->Apply(constraint);
	}
	return FALSE;
}


int Cluster::OwnerCheck(char* owner)
{
	Job*	j;

	if((j = FirstJob()))
	{
		return j->OwnerCheck(owner);
	}
	return FALSE;
}


void
Cluster::Rollback()
{
	if (state & DEATHS_DOOR) {
		state &= ~DEATHS_DOOR;
	}
	// If we were just created, and then have to rollback, we have to
	// delete ourself.
	if (state & NEW_BORN) {
		delete this;
	}
}


void
Cluster::clear_lookup_cache(Job *job)
{
	if (last_lookup == job) {
		last_lookup = 0;
	}
}


Cluster *
FirstCluster()
{
	return ClusterList;
}


Cluster *
FindCluster(int cluster_num)
{
	Cluster *cl;

	for (cl = FirstCluster(); cl != 0; cl = cl->get_next()) {
		if (cl->get_cluster_num() == cluster_num) {
			return cl;
		}
	}
	return 0;
}


Job *
FindJob(int cluster_num, int proc_num)
{
	Cluster *cl;
	Job		*job;

	cl = FindCluster(cluster_num);
	if (cl == 0) {
		return 0;
	}
	job = cl->FindJob(proc_num);
	return job;
}


void
PrintQ()
{
	Cluster *cl;

	dprintf(D_ALWAYS, "*******Job Queue*********\n");
	for (cl = FirstCluster(); cl != 0; cl = cl->get_next()) {
		cl->print();
	}
	dprintf(D_ALWAYS, "****End of Queue*********\n");
}

#if DBM_QUEUE
PROC GlobalqEndMarker;
#endif
XDR *Proc_XDRS;

#if DBM_QUEUE
send_proc(PROC* proc )
#else
send_proc(int cluster, int proc)
#endif
{
    XDR *xdrs = Proc_XDRS;  /* Can't pass this routine an arg, so use global */
    PROC    p;
    int     rval;

#if DBM_QUEUE
    if( !xdr_proc( xdrs, proc ) ) {
        return FALSE;
    }
#else
#if 0
    GetProc(cluster, proc, &p);
    rval = xdr_proc(xdrs, p);
    /* GetProc allocates all these things, so we must free them.  Did I miss
       anything?  I haven't really checked yet. */
    FREE( p.cmd[0] );
    FREE( p.args[0] );
    FREE( p.in[0] );
    FREE( p.out[0] );
    FREE( p.err[0] );
    FREE( p.cmd );
    FREE( p.args );
    FREE( p.in );
    FREE( p.out );
    FREE( p.err );
    FREE( p.exit_status );
    FREE( p.remote_usage );
    FREE( p.rootdir );
    FREE( p.iwd );
    FREE( p.requirements );
    FREE( p.preferences );
#else
    return FALSE;
#endif
#endif
}

PROC	GlobalqEndMarker;

int send_all_jobs(XDR* xdrs)
{
    DBM *q = NULL;
	PROC*	end = &GlobalqEndMarker;

    dprintf( D_FULLDEBUG, "Schedd received SEND_ALL_JOBS request\n");

    WalkJobQueue( send_proc );

#if defined(NEW_PROC)
	end->n_cmds = 0;
	end->n_pipes = 0;

	end->owner = end->env = end->rootdir = end->iwd =
	end->requirements = end->preferences = "";
	end->version_num = PROC_VERSION;
#else
    end->owner = end->cmd = end->args = end->env = end->in =
        end->out = end->err = end->rootdir = end->iwd =
        end->requirements = end->preferences = "";
    end->version_num = PROC_VERSION;
#endif

    if( !xdr_proc( xdrs, end ) ) {
        dprintf( D_ALWAYS, "Couldn't send end marker for SEND_ALL_JOBS\n");
        return FALSE;
    }

    if( !xdrrec_endofrecord(xdrs,TRUE) ) {
        dprintf( D_ALWAYS, "Couldn't send end of record for SEND_ALL_JOBS\n");
        return FALSE;
    }

    dprintf( D_FULLDEBUG, "Finished SEND_ALL_JOBS\n");
	return TRUE;
}

prio_rec	PrioRec[MAX_PRIO_REC];
int			N_PrioRecs = 0;

int get_job_prio(int cluster, int proc)
{
    int prio;
    int job_prio;
    int status;
    int job_status;
    struct shadow_rec *srp;
    PROC_ID id;
    int     q_date;
    char    buf[100], *owner;
    int     cur_hosts;
    int     max_hosts;

    GetAttributeInt(cluster, proc, "Status", &job_status);
    GetAttributeInt(cluster, proc, "Prio", &job_prio);
    GetAttributeInt(cluster, proc, "Q_date", &q_date);
    GetAttributeString(cluster, proc, "Owner", buf);
    owner = buf;
    id.cluster = cluster;
    id.proc = proc;
    if (GetAttributeInt(cluster, proc, "CurrentHosts", &cur_hosts) < 0) {
        cur_hosts = ((job_status == RUNNING) ? 1 : 0);
    }
    if (GetAttributeInt(cluster, proc, "MaxHosts", &max_hosts) < 0) {
        max_hosts = ((job_status == IDLE || job_status == UNEXPANDED) ? 1 : 0);
    }

	// this is changed by return cur_hosts and have another function do the
	// incrementation of sched->JobsRunning. This is to keep the queue manager
	// separated from the scheduler.
    // sched->JobsRunning += cur_hosts;

    // No longer judge whether or not a job can run by looking at its status.
    // Rather look at if it has all the hosts that it wanted.
    if (cur_hosts >= max_hosts) {
        return cur_hosts;
    }

    /* No longer do this since a shadow can be running, and still want more
       machines. */
#if 0
    if( (srp=find_shadow_rec(&id)) ) {
        dprintf( D_ALWAYS,
            "Job %d.%d marked UNEXPANDED or IDLE, but shadow %d not exited\n",
            id.cluster, id.proc, srp->pid
        );
        return;
    }
#endif

    prio = upDown_GetUserPriority(owner,&status); /* UPDOWN */
    if ( status  == Error )
    {
        dprintf(D_UPDOWN,"GetUserPriority returned error\n");
        dprintf(D_UPDOWN,"ERROR : ERROR \n");
/*      EXCEPT("Can't evaluate \"PRIO\""); */
        dprintf( D_ALWAYS,
                "job_prio: Can't find user priority for %s, assuming 0\n",
                owner );
        prio = 0;
    }

    PrioRec[N_PrioRecs].id      = id;
    PrioRec[N_PrioRecs].prio    = prio;
    PrioRec[N_PrioRecs].job_prio    = job_prio;
    PrioRec[N_PrioRecs].status  = job_status;
    PrioRec[N_PrioRecs].qdate   = q_date;
    if ( DebugFlags & D_UPDOWN )
    {
        PrioRec[N_PrioRecs].owner= (char *)malloc(strlen(owner) + 1 );
        strcpy( PrioRec[N_PrioRecs].owner, owner);
    }
    N_PrioRecs += 1;
	return cur_hosts;
}

/*
** This is different from "job_prio()" because we want to send all jobs
** regardless of the state they are in.
*/
#if DBM_QUEUE
all_job_prio(PROC* proc)        /* UPDOWN */
#else
all_job_prio(int cluster, int proc)
#endif
{
    int prio;
    int status;
    int job_prio;
    int job_status;
    PROC_ID job_id;
    int job_q_date;
    char    own_buf[100], *owner;

#if DBM_QUEUE
    job_prio = proc->prio;
    job_status = proc->status;
    owner = proc->owner;
    job_q_date = proc->q_date;
    job_id = proc->id;
#else
    GetAttributeInt(cluster, proc, "Prio", &job_prio);
    GetAttributeInt(cluster, proc, "Status", &job_status);
    GetAttributeInt(cluster, proc, "Q_date", &job_q_date);
    GetAttributeString(cluster, proc, "Owner", own_buf);
    owner = own_buf;
    job_id.cluster = cluster;
    job_id.proc = proc;
#endif
    prio = upDown_GetUserPriority(owner, &status);
    if ( status == Error ) {
        dprintf( D_ALWAYS,
                "all_job_prio: Can't find user priority for %s, assuming 0\n",
                owner );
/*      EXCEPT( "Can't evaluate \"PRIO\"" ); */
        prio = 0;
    }

    PrioRec[N_PrioRecs].id = job_id;
    PrioRec[N_PrioRecs].prio = prio;
    PrioRec[N_PrioRecs].job_prio = job_prio;
    PrioRec[N_PrioRecs].status = job_status;
    PrioRec[N_PrioRecs].qdate = job_q_date;
    N_PrioRecs += 1;

}

send_all_jobs_prioritized(XDR* xdrs)
{
#if defined(NEW_PROC)
    GENERIC_PROC    buf;
    PROC            *proc  = (PROC *)&buf;
#else
    char            buf[1024];
    PROC            *proc  = (PROC *)buf;
#endif
    int     i;

    dprintf( D_FULLDEBUG, "\n" );
    dprintf( D_FULLDEBUG, "Entered send_all_jobs_prioritized\n" );

    N_PrioRecs = 0;

    WalkJobQueue( all_job_prio );

    qsort( (char *)PrioRec, N_PrioRecs, sizeof(PrioRec[0]), (int(*)(const void*, const void*))prio_compar );

    /*
    ** for( i=0; i<N_PrioRecs; i++ ) {
    **  dprintf( D_FULLDEBUG, "PrioRec[%d] = %d.%d %3.10f\n",
    **      i, PrioRec[i].id.cluster, PrioRec[i].id.proc, PrioRec[i].prio );
    ** }
    */
    xdrs->x_op = XDR_ENCODE;

#if	DBM_QUEUE
#if defined(NEW_PROC)
    end->n_cmds = 0;
    end->n_pipes = 0;

    end->owner = end->env = end->rootdir = end->iwd =
        end->requirements = end->preferences = "";
    end->version_num = PROC_VERSION;
#else
    end->owner = end->cmd = end->args = end->env = end->in =
        end->out = end->err = end->rootdir = end->iwd =
        end->requirements = end->preferences = "";
    end->version_num = PROC_VERSION;
#endif


    if( !xdr_proc( xdrs, end ) ) {
        dprintf( D_ALWAYS, "Couldn't send end marker for SEND_ALL_JOBS\n");
        return FALSE;
    }
#endif

    if( !xdrrec_endofrecord(xdrs,TRUE) ) {
        dprintf( D_ALWAYS, "Couldn't send end of record for SEND_ALL_JOBS\n");
        return FALSE;
    }

    dprintf( D_FULLDEBUG, "Finished send_all_jobs_prioritized\n");
}

int mark_idle(int cluster, int proc)
{
    char    ckpt_name[MAXPATHLEN];
    int     status;
    char    owner[_POSIX_PATH_MAX];

    GetAttributeInt(cluster, proc, "Status", &status);

    if( status != RUNNING ) {
        return -1;
    }

#if defined(V2)
    (void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d",
                                Spool, proc->id.cluster, proc->id.proc  );
#else
    strcpy(ckpt_name, gen_ckpt_name(Spool, cluster, proc,0) );
#endif

    GetAttributeString(cluster, proc, "Owner", owner);

    if (FileExists(ckpt_name, owner)) {
        status = IDLE;
    } else {
        status = UNEXPANDED;
    }

    SetAttributeInt(cluster, proc, "Status", status);
    SetAttributeInt(cluster, proc, "CurrentHosts", 0);
	return 0;
}

/*
** There should be no jobs running when we start up.  If any were killed
** when the last schedd died, they will still be listed as "running" in
** the job queue.  Here we go in and mark them as idle.
*/
void mark_jobs_idle()
{
    char    queue[MAXPATHLEN];

    /* set_condor_euid(); */

    WalkJobQueue(mark_idle );

    if (getuid()==0) {
        set_root_euid();
    }
}

/*
 * Weiru
 * This function only look at one cluster. It first look at the job "c.p" to
 * see if it's runnable. If it is, return it. Otherwise, return the runnable
 * job in the cluster with the highest priority.
 */
void FindRunnableJob(int c, int& rp)
{
	int					rval;
	int					p;
	int					nc, np;					// next cluster and proc
	Qmgr_connection*	con;

	if(c <= 0)
	{
		rp = -1;
		return;
	}

	dprintf(D_FULLDEBUG, "Look for runnable job in cluster %d\n", c);
	con = ConnectQ(0);
	// job c.p is not runnable. Sort the cluster on priorities.
	N_PrioRecs = 0;
	rp = -1;
	p = -1;
	do {
		rval = GetNextJob(c, p, &nc, &np);
		if(rval >= 0)
		{
			if(nc != c)
			// no more jobs in this cluster
			{
				dprintf(D_FULLDEBUG, "%d jobs in prio records\n", N_PrioRecs);
				DisconnectQ(con);
				break;
			}
			get_job_prio(nc, np);
			p = np;
		}
		else
		{
			dprintf(D_FULLDEBUG, "%d jobs in prio records\n", N_PrioRecs);
			DisconnectQ(con);
			break;
		}
	} while(rval != -1);
	if(N_PrioRecs == 0)
	// no more jobs to run
	{
		dprintf(D_FULLDEBUG, "No more jobs to run\n");
		rp = -1;
		return;
	}
	FindPrioJob(rp);
	DisconnectQ(con);
}

int Runnable(PROC_ID* id)
{
	int		cur, max;					// current hosts and max hosts

	if(GetAttributeInt(id->cluster, id->proc, "CurrentHosts", &cur) < 0)
	{
		cur = 0;
	}
	if(GetAttributeInt(id->cluster, id->proc, "MaxHosts", &max) < 0)
	{
		max = 1;
	}
	if(cur < max)
	{
		return TRUE;
	}
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
