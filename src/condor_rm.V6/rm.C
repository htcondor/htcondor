/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#if defined(Solaris)
#include "condor_fix_timeval.h"
#if !defined(Solaris251)
#include </usr/ucbinclude/sys/rusage.h>
#endif
#endif

#if defined(IRIX53)
#include "condor_fix_unistd.h"
#endif

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_jobqueue.h"
#include "condor_network.h"
#include "sched.h"
#include "proc_obj.h"
#include "alloc.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

#if DBM_QUEUE
DBM		*Q, *OpenJobQueue();
#else
#include "condor_qmgr.h"
#endif

char	*MyName;
char	*Spool;
BOOLEAN	TroubleReported;
BOOLEAN All;
static BOOLEAN Force = FALSE;
#if DBM_QUEUE
ProcFilter	*PFilter = new ProcFilter();
#endif

	// Prototypes of local interest
#if DBM_QUEUE
void do_it( ProcObj * );
#else
void ProcArg(const char*);
#endif
void init_params();
void notify_schedd( int cluster, int proc );
void usage();
#if DBM_QUEUE
extern "C" BOOLEAN	xdr_proc_id(XDR *, PROC_ID *);
#endif

#if !DBM_QUEUE
extern "C" int gethostname(char*, int);
#endif

void
usage()
{
	fprintf( stderr,
		"Usage: %s -a | { cluster | cluster.proc | user } ... \n",
		MyName
	);
	exit( 1 );
}


int
main( int argc, char *argv[] )
{
#if DBM_QUEUE
	char	queue_name[_POSIX_PATH_MAX];
#endif
	char	*arg;
#if DBM_QUEUE
	List<ProcObj>	*proc_list;
	ProcObj			*p;
#else
	char*				args[argc - 1];			// args of jobs to be deleted
	int					nArgs = 0;				// number of args to be deleted
	int					i;
	Qmgr_connection*	q;
	char				hostname[200];
#endif

	MyName = argv[0];

	config( MyName, (CONTEXT *)0 );
	init_params();

	if( argc < 2 ) {
		usage();
	}

	for( argv++; arg = *argv; argv++ ) {
		if( arg[0] == '-' && arg[1] == 'a' ) {
			All = TRUE;
		} else {
			if( All ) {
				usage();
			}
			if ( arg[0] == '-' && arg[1] == 'f' ) {
				// "-f" is the undocumented "force" feature, which rips
				// out the job from the queue without any checks except for
				// permission.  -Todd 10/95
				Force = TRUE;
			} else {
			#if DBM_QUEUE
				if( !PFilter->Append(arg) ) {
					usage();
				}
			#else
				args[nArgs] = arg;
				nArgs++;
			#endif
			}
		}
	}

		/* Open job queue */
#if DBM_QUEUE
	(void)sprintf( queue_name, "%s/job_queue", Spool );
	if( (Q=OpenJobQueue(queue_name,O_RDWR,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue_name );
	}

		// Create a list of all procs we're interested in
	PFilter->Install();
	proc_list = ProcObj::create_list( Q, ProcFilter::Func );

		// Process the list
	proc_list->Rewind();
	while( p = proc_list->Next() ) {
		do_it( p );
	}

		// Clean up
	LockJobQueue( Q, UNLOCK );
	CloseJobQueue( Q );
	ProcObj::delete_list( proc_list );
	delete PFilter;
#else
	if(gethostname(hostname, 200) < 0)
	{
		EXCEPT("gethostname failed, errno = %d", errno);
	}
	if((q = ConnectQ(hostname)) == 0)
	{
		EXCEPT("Failed to connect to qmgr");
	}
	for(i = 0; i < nArgs; i++)
	{
		ProcArg(args[i]);
	}
	DisconnectQ(q);
#endif

#if defined(ALLOC_DEBUG)
	print_alloc_stats();
#endif

	return 0;
}


extern "C" int SetSyscalls( int foo ) { return foo; }


void
init_params()
{
	Spool = param("SPOOL");
	if( Spool == NULL ) {
		EXCEPT( "SPOOL not specified in config file\n" );
	}
}

#if DBM_QUEUE
void
notify_schedd( int cluster, int proc )
{
	int			sock = -1;
	int			cmd;
	XDR			xdr, *xdrs = NULL;
	char		hostname[512];
	PROC_ID		job_id;

	job_id.cluster = cluster;
	job_id.proc = proc;

	if( gethostname(hostname,sizeof(hostname)) < 0 ) {
		EXCEPT( "gethostname failed" );
	}

		/* Connect to the schedd */
	if( (sock = do_connect(hostname, "condor_schedd", SCHED_PORT)) < 0 ) {
		if( !TroubleReported ) {
			dprintf( D_ALWAYS, "Warning: can't connect to condor scheduler\n" );
			TroubleReported = 1;
		}
		return;
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	cmd = KILL_FRGN_JOB;
	if( !xdr_int(xdrs, &cmd) ) {
		dprintf( D_ALWAYS,
			"Warning: can't send KILL_JOB command to condor scheduler\n" );
		xdr_destroy( xdrs );
		(void)close( sock );
		return;
	}

	if( !xdr_proc_id(xdrs, &job_id) ) {
		dprintf( D_ALWAYS,
			"Warning: can't send proc_id to condor scheduler\n" );
		xdr_destroy( xdrs );
		(void)close( sock );
		return;
	}

	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		dprintf( D_ALWAYS,
			"Warning: can't send endofrecord to condor scheduler\n" );
		xdr_destroy( xdrs );
		(void)close( sock );
		return;
	}

	dprintf( D_FULLDEBUG, "Sent KILL_FRGN_JOB command to condor scheduler\n" );
	xdr_destroy( xdrs );
	(void)close( sock );
}
#endif


#if DBM_QUEUE
void
do_it( ProcObj *p )
{
	PROC_ID	id;

		// Make sure it makes sense to remove the process, unless -f Force command line
		// option is in effect, in which case we trust the user knows better
	if ( Force == FALSE ) {
		switch( p->get_status() ) {
	  	case REMOVED:
	  	case COMPLETED:
	  	case SUBMISSION_ERR:
			return;
		}
	}

	id.cluster = p->get_cluster_id();
	id.proc = p->get_proc_id();

		// Make sure user is allowed to remove it
	if( !p->perm_to_modify() ) {
		fprintf( stderr, "%d.%d: Permission Denied\n", id.cluster, id.proc);
		return;
	}

		// Ok, go ahead and remove it
	TerminateProc( Q, &id, REMOVED );
	notify_schedd( id.cluster, id.proc );
	printf( "Deleted %d.%d\n", id.cluster, id.proc );
}
#else
void ProcArg(const char* arg)
{
	int		c, p;								// cluster/proc #
	char*	tmp;

	if(isdigit(*arg))
	// delete by cluster/proc #
	{
		c = strtol(arg, &tmp, 10);
		if(c <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s\n", arg);
			return;
		}
		if(*tmp == '\0')
		// delete the cluster
		{
			if(DestroyCluster(c) < 0)
			{
				fprintf(stderr, "Couldn't delete %s\n", arg);
			}
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf(stderr, "Invalid proc # from %s\n", arg);
				return;
			}
			if(*tmp == '\0')
			// delete a proc
			{
				if(DestroyProc(c, p) < 0)
				{
					fprintf(stderr, "Couldn't delete %s\n", arg);
				}
				return;
			}
			fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
			return;
		}
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}
	else if(isalpha(*arg))
	// delete by user name
	{
		char	constraint[1000];

		sprintf(constraint, "Owner == \"%s\"", arg);
		if(DestroyClusterByConstraint(constraint) < 0)
		{
			fprintf(stderr, "Couldn't delete %s\n", arg);
		}
	}
	else
	{
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}
}
#endif
