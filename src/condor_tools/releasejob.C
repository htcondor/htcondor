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
#include "condor_config.h"
#include "condor_attributes.h"
#include "alloc.h"
#include "my_hostname.h"
#include "get_daemon_addr.h"

#include "condor_qmgr.h"

char	*param();
char	*MyName;

void ProcArg( const char * );

char	*DaemonName = NULL;

void
usage()
{
	fprintf( stderr, "Usage: %s [-n schedd_name] "
			 "[user | cluster | cluster.proc] ...\n", MyName );
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
	char*	daemonname;

	MyName = argv[0];

	config( 0 );

	if( argc < 2 ) {
		usage();
	}

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for( argv++; arg = *argv; argv++ ) {
		if( arg[0] == '-' && arg[1] == 'n' ) {
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

void UpdateJobAd(int cluster, int proc)
{
	float usage = 0.0;
	int status = IDLE;
	if( (SetAttributeInt(cluster, proc, ATTR_JOB_STATUS, status) < 0) ) {
		fprintf(stderr, "Couldn't set new job status for %d.%d.\n",
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
			sprintf(constraint, "%s == %d && %s == %d", ATTR_CLUSTER_ID,
					cluster, ATTR_JOB_STATUS, HELD);
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
				int status;
				if ((GetAttributeInt(cluster, proc, ATTR_JOB_STATUS,
									 &status) == 0) &&
					(status == HELD)) {
					UpdateJobAd(cluster, proc);
				}
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
		char constraint[100];
		sprintf(constraint, "%s == %d", ATTR_JOB_STATUS, HELD);
		int firstTime = 1;
		while((ad = GetNextJobByConstraint(constraint, firstTime)) != NULL) {
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
		
		sprintf(constraint, "%s == \"%s\" && %s == %d", ATTR_OWNER, arg,
				ATTR_JOB_STATUS, HELD);

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
