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
#include "condor_network.h"
#include "condor_io.h"
#include "sched.h"
#include "alloc.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

extern "C" char *get_schedd_addr(const char *);

#include "condor_qmgr.h"

char	*MyName;
BOOLEAN	TroubleReported;
BOOLEAN All;
static BOOLEAN Force = FALSE;

	// Prototypes of local interest
void ProcArg(const char*);
void notify_schedd( int cluster, int proc );
void usage();

char				hostname[512];


void
usage()
{
	fprintf( stderr,
		"Usage: %s [-r host] { -a | cluster | cluster.proc | user } ... \n",
		MyName
	);
	exit( 1 );
}


int
main( int argc, char *argv[] )
{
	char	*arg;
	char	**args = (char **)malloc(sizeof(char *)*(argc - 1)); // args of jobs to be deleted
	int					nArgs = 0;				// number of args to be deleted
	int					i;
	Qmgr_connection*	q;

	MyName = argv[0];

	config( 0 );

	if( argc < 2 ) {
		usage();
	}

	hostname[0] = '\0';
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
			} else 
			if ( arg[0] == '-' && arg[1] == 'r' ) {
				// use the given name as the host name to connect to
				argv++;
				strcpy (hostname, *argv);	
			} else {
				args[nArgs] = arg;
				nArgs++;
			}
		}
	}

		/* Open job queue */
	if (hostname[0] == '\0')
	{
		// hostname was not set at command line; obtain from system
		if(gethostname(hostname, 200) < 0)
		{
			EXCEPT("gethostname failed, errno = %d", errno);
		}
	}
	if((q = ConnectQ(hostname)) == 0)
	{
		EXCEPT("Failed to connect to qmgr on host %s", hostname);
	}
	for(i = 0; i < nArgs; i++)
	{
		ProcArg(args[i]);
	}
	DisconnectQ(q);

#if defined(ALLOC_DEBUG)
	print_alloc_stats();
#endif

	return 0;
}


extern "C" int SetSyscalls( int foo ) { return foo; }


void
notify_schedd( int cluster, int proc )
{
	ReliSock	*sock;
	char		*scheddAddr;
	int			cmd;
	PROC_ID		job_id;

	job_id.cluster = cluster;
	job_id.proc = proc;

	if (hostname[0] == '\0')
	{
		// if the hostname was not set at command line, obtain from system
		if( gethostname(hostname,sizeof(hostname)) < 0 ) {
			EXCEPT( "gethostname failed" );
		}
	}

	if ((scheddAddr = get_schedd_addr(hostname)) == NULL)
	{
		EXCEPT("Can't find schedd address on %s\n", hostname);
	}

		/* Connect to the schedd */
	sock = new ReliSock(scheddAddr, SCHED_PORT);
	if(sock->get_file_desc() < 0) {
		if( !TroubleReported ) {
			dprintf( D_ALWAYS, "Warning: can't connect to condor scheduler\n" );
			TroubleReported = 1;
		}
		delete sock;
		return;
	}

	sock->encode();

	cmd = KILL_FRGN_JOB;
	if( !sock->code(cmd) ) {
		dprintf( D_ALWAYS,
			"Warning: can't send KILL_JOB command to condor scheduler\n" );
		delete sock;
		return;
	}

	if( !sock->code(job_id) ) {
		dprintf( D_ALWAYS,
			"Warning: can't send proc_id to condor scheduler\n" );
		delete sock;
		return;
	}

	if( !sock->end_of_message() ) {
		dprintf( D_ALWAYS,
			"Warning: can't send endofrecord to condor scheduler\n" );
		delete sock;
		return;
	}

	dprintf( D_FULLDEBUG, "Sent KILL_FRGN_JOB command to condor scheduler\n" );
	delete sock;
}


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
			fprintf(stderr, "Invalid cluster # from %s.\n", arg);
			return;
		}
		if(*tmp == '\0')
		// delete the cluster
		{
			if(DestroyCluster(c) < 0)
			{
				fprintf(stderr, "Couldn't find/delete cluster %d.\n", c);
			} else {
			fprintf(stderr, "Cluster %d removed.\n", c);
			}
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf(stderr, "Invalid proc # from %s.\n", arg);
				return;
			}
			if(*tmp == '\0')
			// delete a proc
			{
				if(DestroyProc(c, p) < 0)
				{
					fprintf(stderr, "Couldn't find/delete job %d.%d.\n", c, p);
				} else {
					fprintf(stderr, "Job %d.%d removed.\n", c, p);
				}
				return;
			}
			fprintf(stderr, "Warning: unrecognized \"%s\" skipped.\n", arg);
			return;
		}
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped.\n", arg);
	}
	else if(isalpha(*arg))
	// delete by user name
	{
		char	constraint[1000];

		sprintf(constraint, "Owner == \"%s\"", arg);
		if(DestroyClusterByConstraint(constraint) < 0)
		{
			fprintf(stderr, "Couldn't find/delete user %s's job(s).\n", arg);
		} else {
			fprintf(stderr, "User %s's job(s) removed.\n", arg);
		}
	}
	else
	{
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped.\n", arg);
	}
}
