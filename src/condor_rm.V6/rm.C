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

#ifdef __GNUG__
#pragma implementation "list.h"
#endif

#include "condor_common.h"
#include "condor_config.h"
#include "condor_network.h"
#include "condor_io.h"
#include "sched.h"
#include "alloc.h"
#include "get_daemon_addr.h"
#include "condor_attributes.h"
#include  "list.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

#include "condor_qmgr.h"

char	*MyName;
BOOLEAN	TroubleReported;
BOOLEAN All;
int nToRemove = 0;
List<PROC_ID> ToRemove;

	// Prototypes of local interest
void ProcArg(const char*);
void notify_schedd();
void usage();

char	DaemonName[512];


void
usage()
{
	fprintf( stderr,
		"Usage: %s [-n schedd_name] { -a | cluster | cluster.proc | user } ... \n",
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
	char*	daemonname;

	MyName = argv[0];

	config( 0 );

	if( argc < 2 ) {
		usage();
	}

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	DaemonName[0] = '\0';
	for( argv++; arg = *argv; argv++ ) {
		if( arg[0] == '-' && arg[1] == 'a' ) {
			All = TRUE;
		} else {
			if( All ) {
				usage();
			}
			if ( arg[0] == '-' && arg[1] == 'n' ) {
				// use the given name as the schedd name to connect to
				argv++;
				if( ! *argv ) {
					fprintf( stderr, "%s: -n requires another argument\n", 
							 MyName);
					exit(1);
				}				
				if( !(daemonname = get_daemon_name(*argv)) ) { 
					fprintf( stderr, "%s: unknown host %s\n", 
							 MyName, get_host_part(*argv) );
					exit(1);
				}
				strcpy( DaemonName, daemonname );
			} else {
				args[nArgs] = arg;
				nArgs++;
			}
		}
	}

		// Open job queue 
	q = ConnectQ(DaemonName);
	if( !q ) {
		if( *DaemonName ) {
			fprintf( stderr, "Failed to connect to queue manager %s\n", 
					 DaemonName );
		} else {
			fprintf( stderr, "Failed to connect to local queue manager\n" );
		}
		exit(1);
	}

	// Set status of requested jobs to REMOVED
	for(i = 0; i < nArgs; i++)
	{
		ProcArg(args[i]);
	}

	// Close job queue
	DisconnectQ(q);

	// Now tell the schedd what we did.  We send the schedd the  KILL_FRGN_JOB
	// command.  We pass the number of jobs to remove _unless_ one or more
	// of the jobs are to be removed via cluster or via user name, in which
	// case we say "-1" jobs to remove.  Telling the schedd there are "-1"
	// jobs to remove forces the schedd to scan the queue looking for jobs
	// which have a REMOVED status.  If all jobs to be removed are via
	// a cluster.proc, we then send the schedd all the cluster.proc numbers
	// to save the schedd from having to scan the entire queue.
	if ( nToRemove != 0 )
		notify_schedd();

#if defined(ALLOC_DEBUG)
	print_alloc_stats();
#endif

	return 0;
}


extern "C" int SetSyscalls( int foo ) { return foo; }


void
notify_schedd()
{
	ReliSock	*sock;
	char		*scheddAddr;
	int			cmd;
	PROC_ID		*job_id;
	int 		i;

	if( (scheddAddr = get_schedd_addr(DaemonName)) == NULL ) {
		if( *DaemonName ) {
			fprintf( stderr, "Can't find schedd address of %s\n", DaemonName);
		} else {
			fprintf( stderr, "Can't find address of local schedd\n" );
		}
		exit(1);
	}

		/* Connect to the schedd */
	sock = new ReliSock(scheddAddr, SCHED_PORT);
	if(!sock->ok()) {
		if( !TroubleReported ) {
			if( *DaemonName ) {
				fprintf( stderr, "Error: Can't connect to schedd %s\n", 
						 DaemonName );
			} else {
				fprintf( stderr, "Error: Can't connect to local schedd\n" );
			}
			TroubleReported = 1;
		}
		delete sock;
		return;
	}

	sock->encode();

	cmd = KILL_FRGN_JOB;
	if( !sock->code(cmd) ) {
		fprintf( stderr,
			"Warning: can't send KILL_JOB command to condor scheduler\n" );
		delete sock;
		return;
	}

	if( !sock->code(nToRemove) ) {
		fprintf( stderr,
			"Warning: can't send num jobs to remove to schedd (%d)\n",nToRemove );
		delete sock;
		return;
	}

	ToRemove.Rewind();
	for (i=0;i<nToRemove;i++) {
		job_id = ToRemove.Next();
		if( !sock->code(*job_id) ) {
			fprintf( stderr,
				"Error: can't send a proc_id to condor scheduler\n" );
			delete sock;
			return;
		}
	}

	if( !sock->end_of_message() ) {
		fprintf( stderr,
			"Warning: can't send end of message to condor scheduler\n" );
		delete sock;
		return;
	}

	delete sock;
}


void ProcArg(const char* arg)
{
	int		c, p;								// cluster/proc #
	char*	tmp;
	PROC_ID *id;

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
			char constraint[250];

			sprintf(constraint, "%s == %d", ATTR_CLUSTER_ID, c);

			if (SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,REMOVED) < 0)
			{
				fprintf( stderr, "Couldn't find/delete cluster %d.\n", c);
			} else {
				fprintf(stderr, "Cluster %d removed.\n", c);
				nToRemove = -1;
			}
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf( stderr, "Invalid proc # from %s.\n", arg);
				return;
			}
			if(*tmp == '\0')
			// delete a proc
			{
				if(SetAttributeInt(c, p, ATTR_JOB_STATUS, REMOVED ) < 0)
				{
					fprintf( stderr, "Couldn't find/delete job %d.%d.\n", c, p );
				} else {
					fprintf(stdout, "Job %d.%d removed.\n", c, p);
					if ( nToRemove != -1 ) {
						nToRemove++;
						id = new PROC_ID;
						id->proc = p;
						id->cluster = c;
						ToRemove.Append(id);
					}
				}
				return;
			}
			fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
			return;
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
	}
	else if(isalpha(*arg))
	// delete by user name
	{
		char	constraint[1000];

		sprintf(constraint, "Owner == \"%s\"", arg);
		if(SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,REMOVED) < 0)
		{
			fprintf( stderr, "Couldn't find/delete user %s's job(s).\n", arg );
		} else {
			fprintf(stdout, "User %s's job(s) removed.\n", arg);
			nToRemove = -1;
		}
	}
	else
	{
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
	}
}
