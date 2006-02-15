/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "get_daemon_name.h"
#include "sig_install.h"

#include "condor_qmgr.h"
#include "condor_distribution.h"

char	*param();
char	*MyName;


int		PrioAdjustment;
int		NewPriority;
int		PrioritySet;
int		AdjustmentSet;
int		InvokingUid;		// user id of person ivoking this program
char	*InvokingUserName;	// user name of person ivoking this program


	// Prototypes of local interest only
void usage();
int compute_adj( char * );
void ProcArg( const char * );
int calc_prio( int old_prio );
void init_user_credentials();

char* DaemonName = NULL;

	// Tell folks how to use this program
void
usage()
{
	fprintf( stderr, "Usage: %s [{+|-}priority ] [-p priority] ", MyName );
	fprintf( stderr, "[ -a ] [-n schedd_name] [user | cluster | cluster.proc] ...\n");
	exit( 1 );
}

int
main( int argc, char *argv[] )
{
	char	*arg;
	char	**args = (char **)malloc(sizeof(char *)*(argc - 1));
	int				nArgs = 0;
	int				i;
	Qmgr_connection	*q;

	MyName = argv[0];
	myDistro->Init( argc, argv );

	config();

	if( argc < 2 ) {
		usage();
	}

	PrioritySet = 0;
	AdjustmentSet = 0;

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for( argv++; (arg = *argv); argv++ ) {
		if( (arg[0] == '-' || arg[0] == '+') && isdigit(arg[1]) ) {
			PrioAdjustment = compute_adj(arg);
			AdjustmentSet = TRUE;
		} else if( arg[0] == '-' && arg[1] == 'p' ) {
			argv++;
			NewPriority = atoi(*argv);
			PrioritySet = TRUE;
		} else if( arg[0] == '-' && arg[1] == 'n' ) {
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

	if( PrioritySet == FALSE && AdjustmentSet == FALSE ) {
		fprintf( stderr, 
			"You must specify a new priority or priority adjustment.\n");
		usage();
		exit(1);
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
	for(i = 0; i < nArgs; i++)
	{
		ProcArg(args[i]);
	}
	DisconnectQ(q);

	return 0;
}

/*
  Given the old priority of a given process, calculate the new
  one based on information gotten from the command line arguments.
*/
int
calc_prio( int old_prio )
{
	int		answer;

	if( AdjustmentSet == TRUE && PrioritySet == FALSE ) {
		answer = old_prio + PrioAdjustment;
	} else {
		answer = NewPriority;
	}
	return answer;
}

/*
  Given a command line argument specifing a relative adjustment
  of priority of the form "+adj" or "-adj", return the correct
  value as a positive or negative integer.
*/
int
compute_adj( char *arg )
{
	char *ptr;
	int val;
	
	ptr = arg+1;

	val = atoi(ptr);

	if( *arg == '-' ) {
		return( -val );
	} else {
		return( val );
	}
}

extern "C" int SetSyscalls( int foo ) { return foo; }

void UpdateJobAd(int cluster, int proc)
{
	int old_prio, new_prio;
	if( (GetAttributeInt(cluster, proc, ATTR_JOB_PRIO, &old_prio) < 0) ) {
		fprintf(stderr, "Couldn't retrieve current priority for %d.%d.\n",
				cluster, proc);
		return;
	}
	new_prio = calc_prio( old_prio );
	if( (SetAttributeInt(cluster, proc, ATTR_JOB_PRIO, new_prio) < 0) ) {
		fprintf(stderr, "Couldn't set new priority for %d.%d.\n",
				cluster, proc);
		return;
	}
}

void ProcArg(const char* arg)
{
	int		cluster, proc;
	char	*tmp;

	if(isdigit(*arg))
	// set prio by cluster/proc #
	{
		cluster = strtol(arg, &tmp, 10);
		if(cluster <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s\n", arg);
			return;
		}
		if(*tmp == '\0')
		// update prio for all jobs in the cluster
		{
			ClassAd	*ad;
			char constraint[100];
			sprintf(constraint, "%s == %d", ATTR_CLUSTER_ID, cluster);
			int firstTime = 1;
			while((ad = GetNextJobByConstraint(constraint, firstTime)) != NULL) {
				ad->LookupInteger(ATTR_PROC_ID, proc);
				FreeJobAd(ad);
				UpdateJobAd(cluster, proc);
				firstTime = 0;
			}
			return;
		}
		if(*tmp == '.')
		{
			proc = strtol(tmp + 1, &tmp, 10);
			if(proc < 0)
			{
				fprintf(stderr, "Invalid proc # from %s\n", arg);
				return;
			}
			if(*tmp == '\0')
			// update prio for proc
			{
				UpdateJobAd(cluster, proc);
				return;
			}
			fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
			return;
		}
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}
	else if(arg[0] == '-' && arg[1] == 'a')
	{
		ClassAd *ad;
		int firstTime = 1;
		while((ad = GetNextJob(firstTime)) != NULL) {
			ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
			ad->LookupInteger(ATTR_PROC_ID, proc);
			FreeJobAd(ad);
			UpdateJobAd(cluster, proc);
			firstTime = 0;
		}
	}
	else if(isalpha(*arg))
	// update prio by user name
	{
		char	constraint[100];
		ClassAd	*ad;
		int firstTime = 1;
		
		sprintf(constraint, "%s == \"%s\"", ATTR_OWNER, arg);

		while ((ad = GetNextJobByConstraint(constraint, firstTime)) != NULL) {
			ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
			ad->LookupInteger(ATTR_PROC_ID, proc);
			FreeJobAd(ad);
			UpdateJobAd(cluster, proc);
			firstTime = 0;
		}
		delete ad;
	}
	else
	{
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}
}
