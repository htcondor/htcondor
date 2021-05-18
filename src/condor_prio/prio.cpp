/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 

#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "sig_install.h"
#include "daemon.h"
#include "dc_schedd.h"

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


	// Tell folks how to use this program
void
usage()
{
	fprintf( stderr, "Usage: %s [{+|-}priority ] [-p priority] ", MyName );
	fprintf( stderr, "[ -a ] [-n schedd_name] [ -pool pool_name ] [user | cluster | cluster.proc | -a] ...\n");
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
	std::string pool_name;
	std::string schedd_name;
	std::string DaemonName;

	MyName = argv[0];
	myDistro->Init( argc, argv );

	set_priv_initialize(); // allow uid switching if root
	config();

	if( argc < 2 ) {
		usage();
	}

	PrioritySet = 0;
	AdjustmentSet = 0;

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif
	ASSERT(args != NULL);

	for( argv++; (arg = *argv); argv++ ) {
		if( (arg[0] == '-' || arg[0] == '+') && isdigit(arg[1]) ) {
			PrioAdjustment = compute_adj(arg);
			AdjustmentSet = TRUE;
		} else if( strcmp(arg, "-p") == MATCH) {
			argv++;
			NewPriority = atoi(*argv);
			PrioritySet = TRUE;
		} else if( strcmp(arg, "-pool") == MATCH) {
			argv++;
			if( ! *argv ) {
				fprintf( stderr, "%s: -pool requires another argument\n", 
						 MyName);
				exit(1);
			}				
			pool_name = *argv;
		} else if( strcmp(arg, "-n") == MATCH) {
			// use the given name as the schedd name to connect to
			argv++;
			if( ! *argv ) {
				fprintf( stderr, "%s: -n requires another argument\n", 
						 MyName);
				exit(1);
			}				
			schedd_name = *argv;
		} else {
			args[nArgs] = arg;
			nArgs++;
		}
	}

	// Ensure that if -pool is specified, -n was also specified.
	if (pool_name != "" && schedd_name == "") {
		fprintf(stderr, "%s: -pool also requires -n to be specified\n", MyName);
		exit(1);
	}

	// specifically allow a NULL return value for the strings. 
	DCSchedd schedd(schedd_name == "" ? NULL : schedd_name.c_str(),
					pool_name == "" ? NULL : pool_name.c_str());

	if ( schedd.locate() == false ) {
		if (schedd_name == "") {
			fprintf( stderr, "%s: ERROR: Can't find address of local schedd\n",
				MyName );
			exit(1);
		}

		if (pool_name == "") {
			fprintf( stderr, "%s: No such schedd named %s in local pool\n", 
				 MyName, schedd_name.c_str() );
		} else {
			fprintf( stderr, "%s: No such schedd named %s in pool %s\n", 
				 MyName, schedd_name.c_str(), pool_name.c_str() );
		}
		exit(1);
	}

	if( PrioritySet == FALSE && AdjustmentSet == FALSE ) {
		fprintf( stderr, 
			"You must specify a new priority or priority adjustment.\n");
		usage();
		exit(1);
	}

	// Open job queue
	DaemonName = schedd.addr();
	q = ConnectQ(DaemonName.c_str());
	if( !q ) {
		fprintf( stderr, "Failed to connect to queue manager %s\n", 
				 DaemonName.c_str() );
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

void UpdateJobAd(int cluster, int proc)
{
	int old_prio, new_prio;
	if( (GetAttributeInt(cluster, proc, ATTR_JOB_PRIO, &old_prio) < 0) ) {
		fprintf(stderr, "Couldn't retrieve current priority for %d.%d.\n",
				cluster, proc);
		return;
	}
	new_prio = calc_prio( old_prio );
	if( (SetAttributeInt(cluster, proc, ATTR_JOB_PRIO, new_prio, SHOULDLOG) < 0) ) {
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
	}
	else
	{
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}
}
