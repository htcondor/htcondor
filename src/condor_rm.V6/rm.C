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

template class List<PROC_ID>;
template class Item<PROC_ID>;

char	*MyName;
BOOLEAN	TroubleReported;
BOOLEAN All = FALSE;
int nToProcess = 0;
List<PROC_ID> ToProcess;

	// Prototypes of local interest
void ProcArg(const char*);
void notify_schedd();
void usage();
void handle_all();

char* DaemonName = NULL;

int mode;

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
	char*	cmd_str;

	MyName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !MyName ) {
		MyName = argv[0];
	} else {
		MyName++;
	}

	cmd_str = strchr( MyName, '_');
	if (cmd_str && strcmp(cmd_str, "_hold") == MATCH) {
		mode = HELD;
	} else {
		mode = REMOVED;
	}

	config( 0 );

	if( argc < 2 ) {
		usage();
	}

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

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
				if( !(DaemonName = get_daemon_name(*argv)) ) { 
					fprintf( stderr, "%s: unknown host %s\n", 
							 MyName, get_host_part(*argv) );
					exit(1);
				}
			} else {
				args[nArgs] = arg;
				nArgs++;
			}
		}
	}

		// Open job queue 
	q = ConnectQ(DaemonName);
	if( !q ) {
		if( DaemonName ) {
			fprintf( stderr, "Failed to connect to queue manager %s\n", 
					 DaemonName );
		} else {
			fprintf( stderr, "Failed to connect to local queue manager\n" );
		}
		exit(1);
	}

	if( All ) {
		handle_all();
	} else {
			// Set status of requested jobs to REMOVED/HELD
		for(i = 0; i < nArgs; i++) {
			ProcArg(args[i]);
		}
	}

	// Close job queue
	DisconnectQ(q);

	// Now tell the schedd what we did.  We send the schedd the
	// KILL_FRGN_JOB command.  We pass the number of jobs to
	// remove/hold _unless_ one or more of the jobs are to be
	// removed/held via cluster or via user name, in which case we say
	// "-1" jobs to remove/hold.  Telling the schedd there are "-1"
	// jobs to remove forces the schedd to scan the queue looking for
	// jobs which have a REMOVED/HELD status.  If all jobs to be removed/held
	// are via a cluster.proc, we then send the schedd all the
	// cluster.proc numbers to save the schedd from having to scan the
	// entire queue.
	if ( nToProcess != 0 )
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
	sock = new ReliSock;
	if(!sock->connect(scheddAddr)) {
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

	if( !sock->code(nToProcess) ) {
		fprintf( stderr,
			"Warning: can't send num jobs to process to schedd (%d)\n",nToProcess );
		delete sock;
		return;
	}

	ToProcess.Rewind();
	for (i=0;i<nToProcess;i++) {
		job_id = ToProcess.Next();
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

			if (SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,mode) < 0)
			{
				fprintf( stderr, "Couldn't find/%s cluster %d.\n",
						 (mode==REMOVED)?"remove":"hold", c);
			} else {
				fprintf(stderr, "Cluster %d %s.\n", c,
						(mode==REMOVED)?"removed":"held");
				nToProcess = -1;
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
				if(SetAttributeInt(c, p, ATTR_JOB_STATUS, mode ) < 0)
				{
					fprintf( stderr, "Couldn't find/%s job %d.%d.\n",
							 (mode==REMOVED)?"remove":"hold", c, p );
				} else {
					fprintf(stdout, "Job %d.%d %s.\n", c, p,
							(mode==REMOVED)?"removed":"held");
					if ( nToProcess != -1 ) {
						nToProcess++;
						id = new PROC_ID;
						id->proc = p;
						id->cluster = c;
						ToProcess.Append(id);
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
		if(SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,mode) < 0)
		{
			fprintf( stderr, "Couldn't find/%s user %s's job(s).\n",
					 (mode==REMOVED)?"remove":"hold", arg );
		} else {
			fprintf(stdout, "User %s's job(s) %s.\n", arg,
					(mode==REMOVED)?"removed":"held");
			nToProcess = -1;
		}
	}
	else 
	{
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
	}
}

void
handle_all()
{
	char	constraint[1000];

		// Remove/Hold all jobs... let queue management code decide
		// which ones we can and can not remove.
	sprintf(constraint, "%s >= %d", ATTR_CLUSTER_ID, 0 );
	if( SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,mode) < 0 ) {
		fprintf( stdout, "%s all of your jobs.\n",
				 (mode==REMOVED)?"Removed":"Held" );
	} else {
		fprintf( stdout, "%s all jobs.\n",
				 (mode==REMOVED)?"Removed":"Held" );
	}
	nToProcess = -1;
}
