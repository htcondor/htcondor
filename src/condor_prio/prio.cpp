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
	q = ConnectQ(schedd);
	if( !q ) {
		fprintf( stderr, "Failed to connect to queue manager %s\n", 
		         schedd.addr() );
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

void UpdateJobAd(int cluster, int proc, int old_prio)
{
	int new_prio = calc_prio(old_prio);
	if( (SetAttributeInt(cluster, proc, ATTR_JOB_PRIO, new_prio, SHOULDLOG) < 0) ) {
		fprintf(stderr, "Couldn't set new priority for %d.%d.\n",
				cluster, proc);
		return;
	}
}

void ProcArg(const char* arg)
{
	PROC_ID job_id;
	int old_prio;
	char	*tmp;
	std::string proj = ATTR_CLUSTER_ID;
	proj += ',';
	proj += ATTR_PROC_ID;
	proj += ',';
	proj += ATTR_JOB_PRIO;

	std::vector<std::pair<PROC_ID, int>> job_ids;

	if(isdigit(*arg))
	// set prio by cluster/proc #
	{
		job_id.cluster = strtol(arg, &tmp, 10);
		if(job_id.cluster <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s\n", arg);
			return;
		}
		if(*tmp == '\0')
		// update prio for all jobs in the cluster
		{
			char constraint[100];
			snprintf(constraint, sizeof(constraint), "%s == %d", ATTR_CLUSTER_ID, job_id.cluster);
			int rval = GetAllJobsByConstraint_Start(constraint, proj.c_str());
			while (rval == 0) {
				ClassAd ad;
				rval = GetAllJobsByConstraint_Next(ad);
				if (rval != 0) {
					continue;
				}
				ad.LookupInteger(ATTR_PROC_ID, job_id.proc);
				if (!ad.LookupInteger(ATTR_JOB_PRIO, old_prio)) {
					fprintf(stderr, "Couldn't retrieve current proc or priority for %d.\n", job_id.cluster);
					continue;
				}
				job_ids.emplace_back(job_id, old_prio);
			}
		} else if(*tmp == '.') {
			job_id.proc = strtol(tmp + 1, &tmp, 10);
			if(job_id.proc < 0)
			{
				fprintf(stderr, "Invalid proc # from %s\n", arg);
				return;
			}
			if(*tmp != '\0') {
				fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
				return;
			}
			if( (GetAttributeInt(job_id.cluster, job_id.proc, ATTR_JOB_PRIO, &old_prio) < 0) ) {
				fprintf(stderr, "Couldn't retrieve current priority for %d.%d.\n",
				        job_id.cluster, job_id.proc);
				return;
			}
			// update prio for proc
			job_ids.emplace_back(job_id, old_prio);
		} else {
			fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
			return;
		}
	}
	else if(arg[0] == '-' && arg[1] == 'a')
	{
		int rval = GetAllJobsByConstraint_Start("", proj.c_str());
		while (rval == 0) {
			ClassAd ad;
			rval = GetAllJobsByConstraint_Next(ad);
			if (rval != 0) {
				continue;
			}
			ad.LookupInteger(ATTR_CLUSTER_ID, job_id.cluster);
			ad.LookupInteger(ATTR_PROC_ID, job_id.proc);
			if (!ad.LookupInteger(ATTR_JOB_PRIO, old_prio)) {
				fprintf(stderr, "Couldn't retrieve priority for %d.%d.\n", job_id.cluster, job_id.proc);
				continue;
			}
			job_ids.emplace_back(job_id, old_prio);
		}
	}
	else if(isalpha(*arg))
	// update prio by user name
	{
		char	constraint[100];
		snprintf(constraint, sizeof(constraint), "%s == \"%s\"", ATTR_OWNER, arg);

		int rval = GetAllJobsByConstraint_Start(constraint, proj.c_str());
		while (rval == 0) {
			ClassAd ad;
			rval = GetAllJobsByConstraint_Next(ad);
			if (rval != 0) {
				continue;
			}
			ad.LookupInteger(ATTR_CLUSTER_ID, job_id.cluster);
			ad.LookupInteger(ATTR_PROC_ID, job_id.proc);
			if (!ad.LookupInteger(ATTR_JOB_PRIO, old_prio)) {
				fprintf(stderr, "Couldn't retrieve priority for %d.%d.\n", job_id.cluster, job_id.proc);
				continue;
			}
			job_ids.emplace_back(job_id, old_prio);
		}
	}
	else
	{
		fprintf(stderr, "Warning: unrecognized \"%s\" skipped\n", arg);
	}

	// Now set the new priority values for the selected jobs
	for (auto [id, prio]: job_ids) {
		UpdateJobAd(id.cluster, id.proc, prio);
	}
}
