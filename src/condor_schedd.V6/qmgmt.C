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
#include <netinet/in.h>
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
#include "condor_attributes.h"
#include "condor_uid.h"

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
int		get_job_prio(int, int);

enum {
	CONNECTION_CLOSED,
	CONNECTION_ACTIVE,
} connection_state = CONNECTION_ACTIVE;

uid_t	active_owner_uid = 0;
char	*active_owner = 0;
char	*rendevous_file = 0;

Transaction *trans = 0;

prio_rec	PrioRecArray[INITIAL_MAX_PRIO_REC];
prio_rec	* PrioRec = &PrioRecArray[0];
int			N_PrioRecs = 0;
static int 	MAX_PRIO_REC=INITIAL_MAX_PRIO_REC ;	// INITIAL_MAX_* in prio_rec.h


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
	active_owner_uid = 0;
	connection_state = CONNECTION_ACTIVE;
}


int
UnauthenticatedConnection()
{
	dprintf( D_ALWAYS, "Unable to authenticate connection.  Setting owner"
			" to \"nobody\"\n" );
	init_user_ids("nobody");
	active_owner_uid = get_user_uid();
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

	if (stat_buf.st_uid != active_owner_uid && stat_buf.st_uid != 0 &&
		stat_buf.st_uid != get_condor_uid()) {
		UnauthenticatedConnection();
		return 0;
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

void
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

int
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
	return 0;
}


int
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
	return 0;
}

} /* extern "C" */

void
AppendLog(LogRecord *log, Job *job, Cluster *cl)
{

	  // If a transaction is active, we log to the transaction, and mark
	  // the job and cluster dirty.  Otherwise, we log directly to the
	  // file and commit the transaction.
	if (trans != 0) {
		trans->AppendLog(log);
		trans->DirtyJob(job);
		trans->DirtyCluster(cl);
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
handle_q(ReliSock *sock, struct sockaddr_in* = NULL)
{
	int	rval;

	trans = new Transaction;
	Q_SOCK = sock;

	InvalidateConnection();
	do {
		/* Probably should wrap a timer around this */
		rval = do_Q_request( sock );
	} while(rval >= 0);
	FreeConnection();

	// tr should be free'd in CloseConnection(), if it wasn't, we must
	// rollback
	if (trans != 0) {
		trans->Rollback();
		delete trans;
		trans = 0;
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
	init_user_ids( owner );
	active_owner_uid = get_user_uid();
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
	Job		*job;
	LogDestroyProc	*log;
	PROC_ID		job_id;

        job_id.cluster = cluster_id;
        job_id.proc = proc_id;

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

	// notify schedd to abort job
	abort_job_myself( job_id );

	return 0;
}


int DestroyCluster(int cluster_id)
{
	LogDestroyCluster	*log;
	Cluster				*cl;
	Job					*job;
	PROC_ID		job_id;

        job_id.cluster = cluster_id;
        job_id.proc = -1;

	if (CheckConnection() < 0) {
		return -1;
	}

	cl = FindCluster(cluster_id);
	if (cl == 0) {
		return -1;
	}
	
	job = cl->FirstJob();
	if (job == 0) {
		return -1;
	}

	// Only the owner can delete a cluster.
	if (!job->OwnerCheck(active_owner)) {
		return -1;
	}

	cl->DeleteSelf();
	log = new LogDestroyCluster(cluster_id);
	AppendLog(log, 0, cl);

	// notify schedd to abort job
	abort_job_myself( job_id );

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
			PROC_ID		job_id;
			
        		job_id.cluster = cl->get_cluster_num();
        		job_id.proc = -1;

			cl->DeleteSelf();
			log = new LogDestroyCluster(cl->get_cluster_num());
			AppendLog(log, 0, cl);
			flag = 0;
			
			// notify schedd to abort job
			abort_job_myself( job_id );
		}
	}
	if(flag)
	{
		return -1;
	}
	return 0;
}


int
SetAttribute(int cluster_id, int proc_id, const char *attr_name, char *attr_value)
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
	if (trans != 0) {
		trans->Commit(log_fd);
		delete trans;
		trans = 0;
	}
	/* Just force the clean-up code to happen */
	return -1;
}


int
GetAttributeFloat(int cluster_id, int proc_id, const char *attr_name, float *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttribute(attr_name, val);
}


int
GetAttributeInt(int cluster_id, int proc_id, const char *attr_name, int *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttribute(attr_name, val);
}


int
GetAttributeString(int cluster_id, int proc_id, const char *attr_name, char *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttribute(attr_name, val);
}


int
GetAttributeExpr(int cluster_id, int proc_id, const char *attr_name, char *val)
{
	Job		*job;

	job = FindJob(cluster_id, proc_id);
	if (job == 0) {
		return -1;
	}
	return job->GetAttributeExpr(attr_name, val);
}


int
DeleteAttribute(int cluster_id, int proc_id, const char *attr_name)
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

int
SendSpoolFile(char *filename, char *address)
{
	int sockfd, pid, len;
	FILE *fp;
	struct sockaddr_in addr;
	char path[_POSIX_PATH_MAX];
	struct in_addr myaddr;

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

	if ((sockfd = socket(PF_INET,SOCK_STREAM,0)) < 0) {
		dprintf(D_ALWAYS, "failed to create socket in ReceiveFile()\n");
		EXCEPT("socket");
	}

	get_inet_address(&myaddr);
	memset( (char *)&addr, 0,sizeof(addr));   /* zero out */
	addr.sin_family = AF_INET;
	addr.sin_addr = myaddr;
	addr.sin_port = htons(0);

     if(bind(sockfd,(struct sockaddr *)&addr, sizeof(addr))<0) {      
		 dprintf(D_ALWAYS, "bind failed in ReceiveFile()\n");
		 EXCEPT("bind");
     }

	len = sizeof(addr);
	if (getsockname(sockfd, (struct sockaddr *)&addr, &len)<0) {
		dprintf(D_ALWAYS, "getsockname failed in ReceiveFile()\n");
		EXCEPT("getsockname");
	}

	strcpy(address, sin_to_string(&addr));
	dprintf(D_FULLDEBUG, "receiving %s on %s\n", path, address);

	if ((pid = fork()) < 0) {
		dprintf(D_ALWAYS, "fork failed in ReceiveFile()\n");
		EXCEPT("fork");
	}

	if (pid == 0) {				// child
		char buf[4 * 1024];
		struct sockaddr from;
		int nbytes=0, written=0, total=0, xfersock, fromlen = sizeof(from);
		int filesize;
		if (listen(sockfd, 1) == -1) {
			EXCEPT("listen");
		}
		if ((xfersock = accept(sockfd, (struct sockaddr *)&from,
							   &fromlen)) == -1) {
			EXCEPT("accept");
		}
		
		if (read(xfersock, &filesize, sizeof(int)) < 0) {
			EXCEPT("read");
		}
		filesize = ntohl(filesize);

		while (total < filesize &&
			   (nbytes = read(xfersock, &buf, sizeof(buf))) > 0) {
			dprintf(D_FULLDEBUG, "read %d bytes\n", nbytes);
			if ((written = fwrite(&buf, sizeof(char), nbytes, fp)) < nbytes) {
				dprintf(D_ALWAYS, "failed to write %d bytes (only wrote %d)\n",
						nbytes, written);
				EXCEPT("fwrite");
			}
			dprintf(D_FULLDEBUG, "wrote %d bytes\n", written);
			total += written;
		}
		dprintf(D_FULLDEBUG, "done with transfer, errno = %d\n", errno);
		total = htonl(total);
		if (write(xfersock, &total, sizeof(int)) < 0) {
			dprintf(D_ALWAYS, "failed to send ack in ReceiveFile()\n");
			EXCEPT("write");
		}
		dprintf(D_FULLDEBUG, "successfully wrote %s (%d bytes)\n",
				path, total);
		exit(0);
	}

	if (fclose(fp) == EOF) {
		EXCEPT("fclose");
	}
	if (close(sockfd) == -1) {
		EXCEPT("close");
	}

	return 0;
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
	char	*ckpt_file_name;
	char	owner[_POSIX_PATH_MAX];

	ckpt_file_name =
		gen_ckpt_name( Spool, cluster->get_cluster_num(), proc, 0 );
	(void)unlink( ckpt_file_name );
	/* Also, try to get rid of it from the checkpoint server */
	GetAttribute(ATTR_OWNER, owner);
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
Job::SetAttribute(const char *name, char *value)
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
		
	delete []tmp_expr;

	ad->Insert(expr_tree);
	return 0;
}


int
Job::DeleteAttribute(const char *name)
{
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
	return 0;
}


int
Job::EvalAttribute(const char *name, EvalResult &result)
{
	ExprTree	*expr_tree;

	expr_tree = ad->Lookup( name );
	if (expr_tree == 0) {
		return -1;
	}

	expr_tree->EvalTree(ad, &result);
}


int
Job::GetAttribute(const char *name, float *val)
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
Job::GetAttribute(const char *name, int *val)
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
Job::GetAttribute(const char *name, char *val)
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
	if (val) {
		strcpy(val, result.s);
	} else {
		val = strdup(result.s);
	}
	return 0;
}


int
Job::GetAttributeExpr(const char *name, char *val)
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
	delete [] this_name;
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
	if (GetAttribute(ATTR_OWNER, my_owner) < 0) {
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
		job_list->GetAttribute(ATTR_OWNER, owner);
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


// Returns cur_hosts so that another function in the scheduler can
// update JobsRunning and keep the scheduler and queue manager
// seperate. 
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

    GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &job_status);
    GetAttributeInt(cluster, proc, ATTR_JOB_PRIO, &job_prio);
    GetAttributeInt(cluster, proc, ATTR_Q_DATE, &q_date);
    GetAttributeString(cluster, proc, ATTR_OWNER, buf);
    owner = buf;
    id.cluster = cluster;
    id.proc = proc;
    if (GetAttributeInt(cluster, proc, ATTR_CURRENT_HOSTS, &cur_hosts) < 0) {
        cur_hosts = ((job_status == RUNNING) ? 1 : 0);
    }
    if (GetAttributeInt(cluster, proc, ATTR_MAX_HOSTS, &max_hosts) < 0) {
        max_hosts = ((job_status == IDLE || job_status == UNEXPANDED) ? 1 : 0);
    }

    // No longer judge whether or not a job can run by looking at its status.
    // Rather look at if it has all the hosts that it wanted.
    if (cur_hosts >= max_hosts) {
		dprintf (D_FULLDEBUG,"In get_job_prio() cur_hosts(%d)>=max_hosts(%d)\n",
								cur_hosts, max_hosts);
        return cur_hosts;
    }

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

    PrioRec[N_PrioRecs].id       = id;
    PrioRec[N_PrioRecs].prio     = prio;
    PrioRec[N_PrioRecs].job_prio = job_prio;
    PrioRec[N_PrioRecs].status   = job_status;
    PrioRec[N_PrioRecs].qdate    = q_date;
    if ( DebugFlags & D_UPDOWN )
    {
		if ( PrioRec[N_PrioRecs].owner ) {
			free( PrioRec[N_PrioRecs].owner );
		}
        PrioRec[N_PrioRecs].owner = strdup( owner );
    }
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

all_job_prio(int cluster, int proc)
{
    int prio;
    int status;
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
    prio = upDown_GetUserPriority(owner, &status);
    if ( status == Error ) {
        dprintf( D_ALWAYS,
                "all_job_prio: Can't find user priority for %s, assuming 0\n",
                owner );
        prio = 0;
    }
    PrioRec[N_PrioRecs].id = job_id;
    PrioRec[N_PrioRecs].prio = prio;
    PrioRec[N_PrioRecs].job_prio = job_prio;
    PrioRec[N_PrioRecs].status = job_status;
    PrioRec[N_PrioRecs].qdate = job_q_date;
    N_PrioRecs += 1;
	if ( N_PrioRecs == MAX_PRIO_REC ) {
		grow_prio_recs( 2 * N_PrioRecs );
	}
}

int mark_idle(int cluster, int proc)
{
    char    ckpt_name[MAXPATHLEN];
    int     status;
    char    owner[_POSIX_PATH_MAX];

    GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &status);

    if( status != RUNNING ) {
        return -1;
    }

    strcpy(ckpt_name, gen_ckpt_name(Spool, cluster, proc,0) );

    GetAttributeString(cluster, proc, ATTR_OWNER, owner);

    if (FileExists(ckpt_name, owner)) {
        status = IDLE;
    } else {
        status = UNEXPANDED;
    }

    SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, status);
    SetAttributeInt(cluster, proc, ATTR_CURRENT_HOSTS, 0);
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
    WalkJobQueue( mark_idle );
}

/*
 * Weiru
 * This function only look at one cluster. Find the job in the cluster with the
 * highest priority.
 */
void FindRunnableJob(int c, int& rp)
{
	int					rval;
	int					p;
	int					nc, np;					// next cluster and proc

	if(c <= 0)
	{
		rp = -1;
		return;
	}

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
				break;
			}
			get_job_prio(nc, np);
			p = np;
		}
		else
		{
			dprintf (D_FULLDEBUG, "GetNextJob(%d, %d) returned %d\n",c,p,rval);
			break;
		}
	} while(rval != -1);
	if(N_PrioRecs == 0)
	// no more jobs to run
	{
		rp = -1;
		return;
	}
	FindPrioJob(rp);
//	DisconnectQ(con);
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
