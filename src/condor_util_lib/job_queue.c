/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


/**********************************************************************
**	These routines provide access to the CONDOR job queue.  The queue consists
**	of clusters of related processes.  All processes in a cluster share the
**	same executable, (and thus the same initial checkpoint), and are submitted
**	as a group via a single command file.  Individual processes are identified
**	by a cluster_id/proc_id tuple.  Cluster id's are unique for all time,
**	and process id's are 0 - n where there are n processes in the cluster.
**	Individual processes are not removed from a cluster, they are just marked
**	as "completed", "deleted", "running", "idle", etc.  Clusters are removed
**	when all processes in the cluster are completed, but the cluster id's are
**	not re-used.
**
**	Operations:
**
**		Q = OpenJobQueue( pathname, flags, mode )
**			char	*pathname;
**			DBM		*Q;
**
**		CloseJobQueue( Q )
**			DBM		*Q;
**
**		LockJobQueue( Q, operation )
**			DBM		*Q;
**			int		operation;
**
**		ClusterId = CreateCluster( Q ) 
**			DBM		*Q;
**			int		ClusterId;
**
**		StoreProc( Q, Proc )
**			DBM		*Q;
**			PROC	*Proc;
**
**		Proc = FetchProc( Q, id )
**			DBM		*Q;
**			PROC_ID	*id;
**			PROC	*Proc;
**
**		ScanJobQueue( Q, func )
**			DBM		*Q;
**			int		(*func)();
**
**		ScanCluster( Q, cluster, func )
**			DBM		*Q;
**			int		cluster;
**			int		(*func)();
**
**		TerminateCluster( Q, cluster, status )
**			DBM		*Q;
**			int		cluster;
**			int		status;
**		
**		TerminateProc( Q, pid, status )
**			DBM		*Q;
**			PROC_ID	*pid;
**			int		status;
**		
**	The low level database operations are accomplished by ndbm(3) using
**	XDR to pack/unpack the Proc structs to/from contiguous areas of memory.
**	The locking is accomplished by flock(3).
**************************************************************************/


#include <stdio.h>
#include <errno.h>
#include <sys/file.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "proc.h"
#include "debug.h"
#include "except.h"
#include "trace.h"
#include "clib.h"

#if defined(HPUX9)
#	include "fake_flock.h"
#endif

#ifdef NDBM
#include <ndbm.h>
#else NDBM
#include "ndbm_fake.h"
#endif NDBM

#if defined(AIX31) || defined(AIX32) || defined(ULTRIX42) || defined(ULTRIX43)
#define DATA_SIZE_LIMIT 1008	/* They *say* they'll take 4096, but they won't. */
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
char *Spool;
char *gen_ckpt_name();


#define LIST_INCR	100

static PROC_ID ClusterId = { 0, 0 };
static datum	ClusterKey = { (char *)&ClusterId, sizeof(ClusterId) };
static ClusterVersion;

typedef void (*PROC_FUNC_PTR)( PROC * );

DBM	* OpenJobQueue( char *path, int flags, int mode );
int CloseJobQueue( DBM *Q );
int LockJobQueue( DBM *Q, int op );
int CreateCluster( DBM *Q );
int StoreProc( DBM *Q, PROC *proc );
int FetchProc( DBM *Q, PROC *proc );
int ScanJobQueue( DBM *Q, PROC_FUNC_PTR func );
int ScanCluster( DBM *Q, int cluster, PROC_FUNC_PTR func );
int TerminateCluster( DBM *Q, int cluster, int status );
int TerminateProc( DBM *Q, PROC_ID *pid, int status );
static int store_cluster_list( DBM *Q, CLUSTER_LIST *list );
CLUSTER_LIST * fetch_cluster_list( DBM *Q );
static CLUSTER_LIST * grow_cluster_list( CLUSTER_LIST *list, int incr );
static CLUSTER_LIST	* make_cluster_list( int len );
static CLUSTER_LIST * add_new_cluster( CLUSTER_LIST *list );
static CLUSTER_LIST * copy_cluster_list( CLUSTER_LIST *list );
void terminate_proc( PROC *p );
void check_empty_cluster( PROC *p );
void delete_cluster( DBM *Q, int cluster );
void data_too_big( int size );









/*
** Prepare the local job queue for reading/writing with these routines.
*/
DBM	*
OpenJobQueue( char *path, int flags, int mode )
{
	return dbm_open(path,flags,mode);
}

/*
** Close the job queue.
*/
CloseJobQueue( DBM *Q )
{
	dbm_close( Q );
}

#include "file_lock.h"
/*
** Put a reader's/writer's lock on an open job queue.
*/
LockJobQueue( DBM *Q, int op )
{
	int		blocking;

	blocking = ~(op & LOCK_NB);

	if( op & LOCK_SH ) {
		return lock_file( Q->dbm_pagf, READ_LOCK, blocking );
	} else if (op & LOCK_EX) {
		return lock_file( Q->dbm_pagf, WRITE_LOCK, blocking );
	} else if (op & LOCK_UN ) {
		return lock_file( Q->dbm_pagf, UN_LOCK, blocking );
	} else {
		errno = EINVAL;
		return -1;
	}
}

/*
** Allocate a new cluster id, and store it in the list of active
** cluster id's.
*/
CreateCluster( DBM *Q )
{
	CLUSTER_LIST	*list, *fetch_cluster_list(), *add_new_cluster();
	int				answer;

	list = fetch_cluster_list( Q );

	list = add_new_cluster( list );
	answer = list->next_id - 1;

	store_cluster_list( Q, list );

	return answer;
}

/*
** Store a PROC in the database.
*/
StoreProc( DBM *Q, PROC *proc )
{
	XDR		xdr, *xdrs = &xdr;
	static char	buf[10 * MAXPATHLEN];
	datum	key;
	datum	data;
	int		rval;
	int		foo;


		/* Use XDR to pack the structure into a contiguous buffer */
	xdrmem_create( xdrs, buf, sizeof(buf), XDR_ENCODE );
	ASSERT( xdr_proc(xdrs,proc) );

		/* Store it with dbm */
	key.dptr = (char *)&proc->id;
	key.dsize = sizeof( PROC_ID );
	data.dptr = buf;
	data.dsize = xdrs->x_private - xdrs->x_base;


	rval = dbm_store(Q,key,data,DBM_REPLACE);
	if( rval < 0 ) {
		data_too_big( data.dsize + key.dsize );
		return E2BIG;	/* hope that's the reason */
	} else {
		return rval;
	}
}


/*
** We are given a process structure with the id filled in.  We look up
** the id in the database, and fill in the rest of the structure.
*/
FetchProc( DBM *Q, PROC *proc )
{
	XDR		xdr, *xdrs = &xdr;
	datum	key;
	datum	data;


		/* Fetch the database entry */
	key.dptr = (char *)&proc->id;
	key.dsize = sizeof( PROC_ID );
	data = dbm_fetch( Q, key );
	if( !data.dptr ) {
		return -1;
	}

		/* Use XDR to unpack the structure from the contiguous buffer */
	xdrmem_create( xdrs, data.dptr, data.dsize, XDR_DECODE );
	bzero( (char *)proc, sizeof(PROC) );	/* let XDR allocate all the space */
	if( !xdr_proc(xdrs,proc) ) {
		EXCEPT( "Can't unpack proc %d.%d", proc->id.cluster, proc->id.proc );
	}
	return 0;
}

/*
** Apply the given function to every process in the queue.
*/
ScanJobQueue( DBM *Q, PROC_FUNC_PTR func )
{
	int				i;
	CLUSTER_LIST	*list;


	list = fetch_cluster_list( Q );

	for( i=0; i<list->n_ids; i++ ) {
		ScanCluster( Q, list->id[i], func );
	}

	FREE( (char *)list );
}

/*
** Apply the function to every process in the specified cluster.
*/
ScanCluster( DBM *Q, int cluster, PROC_FUNC_PTR func )
{
	PROC	proc;
	int		proc_id;

	bzero( (char *)&proc, sizeof(PROC) );

	proc.id.cluster = cluster;
	for( proc_id = 0; ;proc_id++) {
		proc.id.proc = proc_id;
		if( FetchProc(Q,&proc) < 0 ) {
			return;
		}
		func( &proc );
		xdr_free_proc( &proc );
	}
}

static DBM	*CurJobQ;
static int	CurStatus;
static XDR	*CurHistory;
/*
** Terminate every process in the cluster with the given status.  This
** includes storing the proc structs in the history file and removing
** the cluster from the active cluster list.
*/
TerminateCluster( DBM *Q, int cluster, int status )
{
	XDR		xdr, *OpenHistory();
	int		fd;
	char	ickpt_name[MAXPATHLEN];
	char	*name;
	int		i;

	CurJobQ = Q;
	CurStatus = status;
	CurHistory = OpenHistory( param("HISTORY"), &xdr, &fd );
	(void)LockHistory( CurHistory, WRITER );

	ScanCluster( Q, cluster, terminate_proc );

	CloseHistory( CurHistory );
	delete_cluster( Q, cluster );

	switch( ClusterVersion ) {
	  case 2:
		(void)sprintf( ickpt_name, "%s/job%06d.ickpt", Spool, cluster );
		(void)unlink( ickpt_name );
		break;
	  case 3:
		for( i=0; ; i++ ) {
			name = gen_ckpt_name( Spool, cluster, ICKPT, i );
			if( unlink(name) < 0 ) {
				if( errno != ENOENT ) {
					EXCEPT( "Can't remove \"%s\"\n", name );
				}
				break;
			}
		}
		break;
	  default:
		EXCEPT( "Unknown PROC version (%d)\n", ClusterVersion );
	}

	CurJobQ = NULL;
}

static int	EmptyCluster;
/*
** Terminate a particular process.  We also check to see if this is the last
** active process in the cluster, and if so, we terminate the cluster.
*/
TerminateProc( DBM *Q, PROC_ID *pid, int status )
{
	GENERIC_PROC	gen_proc;
	V2_PROC			*v2_ptr = (V2_PROC *)&gen_proc;
	V3_PROC			*v3_ptr = (V3_PROC *)&gen_proc;
	char	ckpt_name[MAXPATHLEN];
	char	*ckpt_file_name;
	int		i;

		/* We assume that the Id and Version Number are at the
		   same offset in either kind of PROC structure */
	v3_ptr->id.cluster = pid->cluster;
	v3_ptr->id.proc = pid->proc;
	if( FetchProc(Q,v3_ptr) != 0 ) {
		printf( "Process %d.%d doesn't exist\n",
								v3_ptr->id.cluster, v3_ptr->id.proc );
		return -1;
	}

	switch( v2_ptr->version_num ) {
	  case 2:
		ClusterVersion = 2;
		if( v2_ptr->status != COMPLETED ) {
			v2_ptr->status = status;
		}
		break;
	  case 3:
		ClusterVersion = 3;
		if( v3_ptr->status != COMPLETED ) {
			v3_ptr->status = status;
		}
		break;
	  default:
		EXCEPT( "Unknown PROC version (%d)\n", v2_ptr->version_num );
	}

	if( StoreProc(Q,v3_ptr) != 0 ) {
		EXCEPT( "Can't store process struct %d.%d\n",
											pid->cluster, pid->proc );
	}


	switch( v2_ptr->version_num ) {
	  case 2:
		(void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d",
						Spool, v2_ptr->id.cluster, v2_ptr->id.proc );
		(void)unlink( ckpt_name );
		break;
	  case 3:
		for( i=0; i<v3_ptr->n_cmds; i++ ) {
			ckpt_file_name =
				gen_ckpt_name( Spool, v3_ptr->id.cluster, v3_ptr->id.proc, i );
			(void)unlink( ckpt_file_name );
		}

	}

	xdr_free_proc( v3_ptr );

	EmptyCluster = TRUE;
	ScanCluster( Q, pid->cluster, check_empty_cluster );

	if( EmptyCluster ) {
		TerminateCluster( Q, pid->cluster, -1 );
	}
	return 0;
}

/*
** Store the given cluster list in the database.  The list we are given
** is always malloc'd, so we free it here.  ROUTINES WHICH CALL
** THIS ONE MUST NOT MAKE ANY USE OF THE LIST AFTERWARD!
*/
static
store_cluster_list( DBM *Q, CLUSTER_LIST *list )
{
	datum	data;
	int		rval;

	data.dptr = (char *)list;
	data.dsize = sizeof(CLUSTER_LIST) + (list->array_len - 1) * sizeof(int);

	rval = dbm_store( Q, ClusterKey, data, DBM_REPLACE );
	if( rval < 0 ) {
		data_too_big( ClusterKey.dsize + data.dsize );
		return E2BIG;	/* hope that's the reason */
	} else {
		FREE( (char *)list );
		return rval;
	}
}

/*
** Fetch a cluster list from the database.  If none is there, create a
** new and empty one.  In either case the list we give back is always
** malloc'd, and should be free'd later.
*/
CLUSTER_LIST *
fetch_cluster_list( DBM *Q )
{
	datum	data;
	CLUSTER_LIST	*answer, *make_cluster_list(), *copy_cluster_list();

	memset( &data, '\0', sizeof(data) );
	data = dbm_fetch( Q, ClusterKey );
	if( data.dptr ) {
		answer = copy_cluster_list( (CLUSTER_LIST *)data.dptr );
	} else {
		answer = make_cluster_list( LIST_INCR );
	}

	return answer;
}

/*
** Increase the size of a cluster list by (incr).  The list we are given
** is always malloc'd, so we can use realloc for this.
*/
static
CLUSTER_LIST *
grow_cluster_list( CLUSTER_LIST *list, int incr )
{
	list->array_len += incr;

	return (CLUSTER_LIST *)REALLOC( (char *)list,
		(unsigned)(sizeof(CLUSTER_LIST) + (list->array_len-1) * sizeof(int)) );
}

/*
** Return a new and empty cluster list in a malloc'd area.
*/
static
CLUSTER_LIST	*
make_cluster_list( int len )
{
	CLUSTER_LIST	*answer;

	answer = (CLUSTER_LIST *)MALLOC( (unsigned)(sizeof(CLUSTER_LIST) +
											(len - 1) * sizeof(int)) );
	if( answer == NULL ) {
		return NULL;
	}
	answer->n_ids = 0;
	answer->array_len = len;
	answer->next_id = 1;
	return answer;
}

static
CLUSTER_LIST *
add_new_cluster( CLUSTER_LIST *list )
{
	CLUSTER_LIST	*grow_cluster_list();

	if( list->n_ids == list->array_len ) {
		list = grow_cluster_list( list, LIST_INCR );
	}
	list->id[ list->n_ids++ ] = list->next_id++;
	return list;
}

/*
** When we fetch a cluster list from the database, we get a pointer into
** the database's private buffer.  Here we copy it into our own malloc'd
** space so we don't have to worry about the database overwriting it
** later.
*/
static
CLUSTER_LIST *
copy_cluster_list( CLUSTER_LIST *list )
{
	CLUSTER_LIST	*answer;

		/* Malloc space for the new one */
	answer = (CLUSTER_LIST *)MALLOC( (unsigned)(sizeof(CLUSTER_LIST) +
								(list->array_len - 1) * sizeof(int)) );

		/* Copy in the old one */
	bcopy( (char *)list, (char *)answer,
		(unsigned)(sizeof(CLUSTER_LIST) + (list->array_len-1) * sizeof(int)) );
	
	return answer;
}


void
terminate_proc( PROC *p )
{
	datum	key;
	V2_PROC	*v2_ptr = (V2_PROC *)p;
	V3_PROC	*v3_ptr = (V3_PROC *)p;

	switch( p->version_num ) {
	  case 2:
		if( CurStatus >= 0 && v2_ptr->status != COMPLETED ) {
			v2_ptr->status = CurStatus;
		}

		if( v2_ptr->status != SUBMISSION_ERR ) {
			AppendHistory( CurHistory, v2_ptr );
		}
		break;
	  case 3:
		if( CurStatus >= 0 && v3_ptr->status != COMPLETED ) {
			v3_ptr->status = CurStatus;
		}

		if( v3_ptr->status != SUBMISSION_ERR ) {
			AppendHistory( CurHistory, v3_ptr );
		}
		break;
	  default:
		EXCEPT( "Unknown PROC version (%d)\n", p->version_num );
	}


	key.dptr = (char *)&p->id;
	key.dsize = sizeof( PROC_ID );
	if( dbm_delete(CurJobQ,key) != 0 ) {
		fprintf( stderr, "Can't delete process %d.%d\n",
											p->id.cluster, p->id.proc );
	}
}

void
check_empty_cluster( PROC * p )
{
	V2_PROC	*v2_ptr = (V2_PROC *)p;
	V3_PROC	*v3_ptr = (V3_PROC *)p;
	int		status;

	switch( p->version_num ) {
	  case 2:
		status = v2_ptr->status;
		break;
	  case 3:
		status = v3_ptr->status;
		break;
	  default:
		EXCEPT( "Unknown PROC version (%d)\n", p->version_num );
	}
		
	switch( status ) {
		case UNEXPANDED:
		case IDLE:
		case RUNNING:
			EmptyCluster = FALSE;
			break;
		case REMOVED:
		case COMPLETED:
		default:
			break;
	}
}

/*
** Delete a cluster from the cluster list.  This DOES NOT delete individual
** processes within the structure.  That should be done by a higher level
** routine which also calls this one.
*/
void
delete_cluster( DBM *Q, int cluster )
{
	CLUSTER_LIST	*list, *fetch_cluster_list();
	int				src, dst;

	list = fetch_cluster_list( Q );

	for( src = 0, dst = 0; src < list->n_ids; ) {
		if( list->id[src] != cluster ) {
			list->id[dst++] = list->id[src++];
		} else {
			src++;
		}
	}
	list->n_ids = dst;

	store_cluster_list( Q, list );
}

void
data_too_big( int size )
{
	int	flags;


	flags = D_ALWAYS | D_NOHEADER;

	dprintf( flags, "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n" );
	dprintf( flags, "Job Description Size Limit Exceeded!\n" );
#ifdef DATA_SIZE_LIMIT
	dprintf( flags, "Max is %d bytes, Your Job Description is %d bytes\n\n",
													 DATA_SIZE_LIMIT, size );
#else
	dprintf( flags, "Your Job Description is %d bytes\n", size );
	dprintf( flags, "Check ndbm documentation for max allowable size\n\n" );
#endif
	dprintf( flags, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" );

}

/*
** The current version of ULTRIX doesn't have ndbm.  We'll build a subset
** of that interface here with some loss of functionality.  We won't be
** able to handle multiple open databases, so we'll sacrifice the history
** keeping function for ULTRIX installations.  If ULTRIX ever comes
** up with an ndbm type of database library, this can all go away.
*/

#ifndef NDBM


DBM *
dbm_open( char *file, int flags, int mode )
{
	static DBM dbm;
	char	name[MAXPATHLEN];

	(void)sprintf( name, "%s.dir", file );
	if( (dbm.dbm_dirf = open(name,flags,mode)) < 0 ) {
		return NULL;
	}

	(void)sprintf( name, "%s.pag", file );
	if( (dbm.dbm_pagf = open(name,flags,mode)) < 0 ) {
		(void)close(dbm.dbm_dirf);
		return NULL;
	}

	if( dbminit(file) < 0 ) {
		return NULL;
	}

	return &dbm;
}

dbm_close( DBM *Q )
{
	(void)close( Q->dbm_pagf );
	(void)close( Q->dbm_dirf );
	dbmclose();
}

/* ARGSUSED */
dbm_store( DBM *Q, datum key, datum data, int flags )
{
	return store( key, data );
}

/* ARGSUSED */
datum
dbm_fetch( DBM *Q, datum key )
{
	return fetch( key );
}

/* ARGSUSED */
dbm_delete( DBM *Q, datum key )
{
	return delete( key );
}

#endif NOT NDBM
