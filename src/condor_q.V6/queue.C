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
#include "condor_accountant.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_query.h"
#include "condor_q.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "my_hostname.h"
#include "get_daemon_addr.h"
#include "get_full_hostname.h"
#include "MyString.h"
#include "extArray.h"
#include "files.h"
#include "ad_printmask.h"
#include "internet.h"
#include "sig_install.h"
#include "format_time.h"
#include "daemon.h"
#include "my_hostname.h"

extern 	"C" int SetSyscalls(int val){return val;}
extern  void short_print(int,int,const char*,int,int,int,int,int,const char *);
static  void processCommandLineArguments(int, char *[]);
extern 	"C" void set_debug_flags( char * );

static 	void short_header (void);
static 	void usage (char *);
static 	void displayJobShort (ClassAd *);
static 	void shorten (char *, int);
static	bool show_queue (char*, char*, char*);

static 	int verbose = 0, summarize = 1, global = 0;
static 	int malformed, unexpanded, running, idle, held;

static	CondorQ 	Q;
static	QueryResult result;
static	CondorQuery	scheddQuery(SCHEDD_AD);
static	CondorQuery submittorQuery(SUBMITTOR_AD);
static	ClassAdList	scheddList;

static	bool		querySchedds 	= false;
static	bool		querySubmittors = false;
static	char		constraint[4096];
static	char		*pool = NULL;

// for run failure analysis
static  int			findSubmittor( char * );
static	void 		setupAnalysis();
static 	void		fetchSubmittorPrios();
static	void		doRunAnalysis( ClassAd* );
struct 	PrioEntry { MyString name; float prio; };
static 	bool		analyze	= false;
static	bool		run = false;
static	char		*fixSubmittorName( char*, int );
static	ClassAdList startdAds;
static	ExprTree	*stdRankCondition;
static	ExprTree	*preemptRankCondition;
static	ExprTree	*preemptPrioCondition;
static	ExprTree	*preemptionHold;
static  ExtArray<PrioEntry> prioTable;
#ifndef WIN32
template class ExtArray<PrioEntry>;
#endif

extern 	"C"	int		Termlog;

int main (int argc, char **argv)
{
	ClassAd		*ad;
	bool		first = true;
	char		scheddAddr[64];
	char		scheddName[64];
	char		scheddMachine[64];
	
	// load up configuration file
	config( 0 );

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	// process arguments
	processCommandLineArguments (argc, argv);

	// check if analysis is required
	if( analyze ) {
		setupAnalysis();
	}
	// if we haven't figured out what to do yet, just display the
	// local queue 
	if (!global && !querySchedds && !querySubmittors) {
		Daemon schedd( DT_SCHEDD, 0, 0 );
		sprintf( scheddAddr, "%s", schedd.addr() );
		sprintf( scheddName, "%s", schedd.name() );
		sprintf( scheddMachine, "%s", schedd.fullHostname() );
		if( show_queue(scheddAddr, scheddName, scheddMachine) ) {
			exit( 0 );
		} else {
			fprintf( stderr, "Can't display queue of local schedd\n" );
			exit( 1 );
		}
	}
	
	// if a global queue is required, query the schedds instead of submittors
	if (global) {
		querySchedds = true;
		sprintf( constraint, "%s > 0 || %s > 0", ATTR_TOTAL_RUNNING_JOBS,
				ATTR_TOTAL_IDLE_JOBS );
		result = scheddQuery.addConstraint( constraint );
		if( result != Q_OK ) {
			fprintf( stderr, "Error: Couldn't add constraint %s\n", constraint);
			exit( 1 );
		}		
	}

	// get the list of ads from the collector
	if( pool ) {
		result = querySchedds ? scheddQuery.fetchAds(scheddList, pool) : 
			submittorQuery.fetchAds(scheddList, pool);
	} else {
		result = querySchedds ? scheddQuery.fetchAds(scheddList) : 
			submittorQuery.fetchAds(scheddList);
	}

	if (result != Q_OK) {
		fprintf (stderr, "Error %d: %s\n", result, getStrQueryResult(result));
		exit(1);
	}
	

	// get queue from each ScheddIpAddr in ad
	scheddList.Open();
	while ((ad = scheddList.Next()))
	{
		// get the address of the schedd
		if (!ad->LookupString(ATTR_SCHEDD_IP_ADDR, scheddAddr)	||
			!ad->LookupString(ATTR_NAME, scheddName)			||
			!ad->LookupString(ATTR_MACHINE, scheddMachine))
				continue;
	
		show_queue( scheddAddr, scheddName, scheddMachine );
		first = false;
	}

	// close list
	scheddList.Close();

	if( first ) {
		if( global ) {
			printf( "All queues are empty\n" );
		} else {
			fprintf(stderr,"Error: Collector has no record of "
							"schedd/submitter\n");
			exit(1);
		}
	}

	return 0;
}

static void 
processCommandLineArguments (int argc, char *argv[])
{
	int i, cluster, proc;
	char *arg, *at, *daemonname;
	
	for (i = 1; i < argc; i++)
	{
		// if the argument begins with a '-', use only the part following
		// the '-' for prefix matches
		if (*argv[i] == '-') 
			arg = argv[i]+1;
		else
			arg = argv[i];

		if (match_prefix (arg, "long")) {
			verbose = 1;
			summarize = 0;
		} 
		else
		if (match_prefix (arg, "pool")) {
			if( pool ) {
				free( pool );
			}
			pool = get_full_hostname(argv[++i]);
			if( ! pool ) {
				fprintf( stderr, "%s: unknown host %s\n", 
						 argv[0], argv[i] );
				exit(1);
			}
			pool = strdup( pool );
		} 
		else
		if (match_prefix (arg, "D")) {
			Termlog = 1;
			set_debug_flags( argv[++i] );
		} 
		else
		if (match_prefix (arg, "name")) {

			if (querySubmittors) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submitters\n");
				exit(1);
			}

			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Argument -name requires another parameter\n");
				exit(1);
			}

			if( !(daemonname = get_daemon_name(argv[i+1])) ) {
				fprintf( stderr, "%s: unknown host %s\n",
						 argv[0], get_host_part(argv[i+1]) );
				exit(1);
			}
			sprintf (constraint, "%s == \"%s\"", ATTR_NAME, daemonname);
			delete [] daemonname;

			result = scheddQuery.addConstraint (constraint);
			if (result != Q_OK) {
				fprintf (stderr, "Argument %d (%s): Error %s\n", i, argv[i],
							getStrQueryResult(result));
				exit (1);
			}
			i++;
			querySchedds = true;
		} 
		else
		if (match_prefix (arg, "submitter")) {

			if (querySchedds) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submitters\n");
				exit(1);
			}
			
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Argument -submitter requires another "
							"parameter\n");
				exit(1);
			}
				
			i++;
			if ((at = strchr(argv[i],'@'))) {
				// is the name already qualified with a UID_DOMAIN?
				sprintf (constraint, "%s == \"%s\"", ATTR_NAME, argv[i]);
				*at = '\0';
			} else {
				// no ... add UID_DOMAIN
				char *uid_domain = param("UID_DOMAIN");
				if (uid_domain == NULL)
				{
					EXCEPT ("No 'UID_DOMAIN' found in config file");
				}
				sprintf (constraint, "%s == \"%s@%s\"", ATTR_NAME, argv[i], 
							uid_domain);
				free (uid_domain);
			}

			// insert the constraints
			result = submittorQuery.addConstraint (constraint);
			if (result != Q_OK) {
				fprintf (stderr, "Argument %d (%s): Error %s\n", i, argv[i],
							getStrQueryResult(result));
				exit (1);
			}

			{
				char *ownerName = argv[i];
				// ensure that the "nice-user" prefix isn't inserted as part
				// of the job ad constraint
				if( strstr( argv[i] , NiceUserName ) == argv[i] ) {
					ownerName = argv[i]+strlen(NiceUserName)+1;
				}
				if (Q.add (CQ_OWNER, ownerName) != Q_OK) {
					fprintf (stderr, "Error:  Argument %d (%s)\n", i, argv[i]);
					exit (1);
				}
			}

			querySubmittors = true;
		}
		else
		if (match_prefix (arg, "constraint")) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Argument -constraint requires another "
							"parameter\n");
				exit(1);
			}

			if (Q.add (argv[++i]) != Q_OK) {
				fprintf (stderr, "Error:  Argument %d (%s)\n", i, argv[i]);
				exit (1);
			}
			summarize = 0;
		} 

		else
		if (match_prefix (arg, "address")) {

			if (querySubmittors) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submitters\n");
				exit(1);
			}

			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Argument -address requires another parameter\n");
				exit(1);
			}
			if( ! is_valid_sinful(argv[i+1]) ) {
				fprintf( stderr, 
						 "Address must be of the form: \"<ip.address:port>\"\n" );
				exit(1);
			}
			sprintf (constraint, "%s == \"%s\"", ATTR_SCHEDD_IP_ADDR, argv[i+1]);
			result = scheddQuery.addConstraint (constraint);
			if (result != Q_OK) {
				fprintf (stderr, "Argument %d (%s): Error %s\n", i, argv[i],
							getStrQueryResult(result));
				exit (1);
			}
			i++;
			querySchedds = true;
		} 
		else
		if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
			sprintf (constraint, "((%s == %d) && (%s == %d))", 
									ATTR_CLUSTER_ID, cluster,
									ATTR_PROC_ID, proc);
			Q.add (constraint);
			summarize = 0;
		} 
		else
		if (sscanf (argv[i], "%d", &cluster) == 1) {
			sprintf (constraint, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
			Q.add (constraint);
			summarize = 0;
		} 
		else
		if (match_prefix (arg, "global")) {
			global = 1;
		} 
		else
		if (match_prefix (arg, "help")) {
			usage(argv[0]);
			exit(0);
		}
		else
		if (match_prefix( arg , "analyze")) {
			analyze = true;
		}
		else
		if (match_prefix( arg, "run")) {
			Q.add (CQ_STATUS, RUNNING);
			run = true;
		}
		else
		{
			// assume name of owner of job
			if (Q.add (CQ_OWNER, argv[i]) != Q_OK) {
				fprintf (stderr, "Error:  Argument %d (%s)\n", i, argv[i]);
				exit (1);
			}
		}
	}
}


static void
displayJobShort (ClassAd *ad)
{
	int cluster, proc, date, status, prio, image_size;
	float utime;
	char owner[64], cmd[2048], args[2048];

	if (!ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cluster)		||
		!ad->EvalInteger (ATTR_PROC_ID, NULL, proc)				||
		!ad->EvalInteger (ATTR_Q_DATE, NULL, date)				||
		!ad->EvalFloat   (ATTR_JOB_REMOTE_USER_CPU, NULL, utime)||
		!ad->EvalInteger (ATTR_JOB_STATUS, NULL, status)		||
		!ad->EvalInteger (ATTR_JOB_PRIO, NULL, prio)			||
		!ad->EvalInteger (ATTR_IMAGE_SIZE, NULL, image_size)	||
		!ad->EvalString  (ATTR_OWNER, NULL, owner)				||
		!ad->EvalString  (ATTR_JOB_CMD, NULL, cmd) )
	{
		printf (" --- ???? --- \n");
		return;
	}
	
	int niceUser;
    if( ad->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
        char tmp[100];
        strcpy(tmp,NiceUserName);
        strcat(tmp,".");
        strcat(tmp,owner);
        strcpy(owner,tmp);
    }

	shorten (owner, 14);
	if (ad->EvalString ("Args", NULL, args)) {
		strcat(cmd, " ");
		strcat (cmd, args);
	}
	shorten (cmd, 18);
	short_print (cluster, proc, owner, date, (int)utime, status, prio,
					image_size, cmd); 

	switch (status)
	{
		case UNEXPANDED: unexpanded++; break;
		case IDLE:       idle++;       break;
		case RUNNING:    running++;    break;
		case HELD:		 held++;	   break;
	}
}

static void 
short_header (void)
{
    printf( " %-7s %-14s %11s %12s %-2s %-3s %-4s %-18s\n",
        "ID",
        "OWNER",
        "SUBMITTED",
        "CPU_USAGE",
        "ST",
        "PRI",
        "SIZE",
        "CMD"
    );
}

static char *
format_remote_host (char *, AttrList *ad)
{
	static char result[20];
	char sinful_string[24];
	struct sockaddr_in sin;
	if (ad->LookupString(ATTR_REMOTE_HOST, sinful_string) == 1 &&
		string_to_sin(sinful_string, &sin) == 1) {
		return sin_to_hostname(&sin, NULL);
	} else {
		int universe = STANDARD;
		ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
		if (universe == SCHED_UNIVERSE) {
			return my_full_hostname();
		} else if (universe == PVM) {
			int current_hosts;
			if (ad->LookupInteger( ATTR_CURRENT_HOSTS, current_hosts ) == 1) {
				sprintf(result, "%d hosts", current_hosts);
				return result;
			}
		}
	}
		
	return "[????????????????]";
}

static char *
format_cpu_time (float utime, AttrList *)
{
	return format_time((int)utime);
}

static char *
format_owner (char *owner, AttrList *ad)
{
	static char result[15];
	int niceUser;
	if (ad->LookupInteger( ATTR_NICE_USER, niceUser) && niceUser ) {
		char tmp[100];
		strcpy(tmp,NiceUserName);
		strcat(tmp, ".");
		strcat(tmp, owner);
		sprintf(result, "%-14.14s", tmp);
	} else {
		sprintf(result, "%-14.14s", owner);
	}
	return result;
}

static char *
format_q_date (int d, AttrList *)
{
	return format_date(d);
}

static void
shorten (char *buff, int len)
{
	if ((unsigned int)strlen (buff) > (unsigned int)len) buff[len] = '\0';
}
		
static void
usage (char *myName)
{
	printf ("Usage: %s [options]\n\twhere [options] are\n"
		"\t\t-global\t\t\tGet global queue\n"
		"\t\t-submitter <submitter>\tGet queue of specific submitter\n"
		"\t\t-help\t\t\tThis screen\n"
		"\t\t-name <name>\t\tName of schedd\n"
		"\t\t-pool <host>\t\tUse host as the central manager to query\n"
		"\t\t-long\t\t\tVerbose output\n"
		"\t\t-analyze\t\tPerform schedulability analysis on jobs\n"
		"\t\t-run\t\t\tGet information about running jobs\n"
		"\t\trestriction list\n"
		"\twhere each restriction may be one of\n"
		"\t\t<cluster>\t\tGet information about specific cluster\n"
		"\t\t<cluster>.<proc>\tGet information about specific job\n"
		"\t\t<owner>\t\t\tInformation about jobs owned by <owner>\n"
		"\t\t-constraint <expr>\tAdd constraint on classads\n",
			myName);
}


static bool
show_queue( char* scheddAddr, char* scheddName, char* scheddMachine )
{
	ClassAdList jobs; 
	ClassAd		*job;

		// fetch queue from schedd	
	if( Q.fetchQueueFromHost(jobs, scheddAddr) != Q_OK ) return false;

		// sort jobs by (cluster.proc)
	jobs.Sort( (SortFunctionType)JobSort );

		// check if job is being analyzed
	if( analyze ) {
			// print header
		if( querySchedds ) {
			printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
		} else {
			printf ("\n\n-- Submitter: %s : %s : %s\n", scheddName, 
					scheddAddr, scheddMachine);	
		}

		jobs.Open();
		while( ( job = jobs.Next() ) ) {
			doRunAnalysis( job );
		}
		jobs.Close();

		return true;
	}

		// display the jobs from this submittor
	if( jobs.MyLength() != 0 || !global ) {
			// print header
		if( querySchedds ) {
			printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
		} else {
			printf ("\n\n-- Submitter: %s : %s : %s\n", scheddName, 
					scheddAddr, scheddMachine);	
		}
		
			// initialize counters
		malformed = 0; idle = 0; running = 0; unexpanded = 0, held = 0;
		
		if( verbose ) {
			jobs.fPrintAttrListList( stdout );
		} else if ( run ) {
			summarize = false;
			AttrListPrintMask mask;
			printf( " %-7s %-14s %11s %12s %-16s\n", "ID", "OWNER",
					"SUBMITTED", "CPU_USAGE",
					"HOST(S)" );
			mask.registerFormat ("%4d.", ATTR_CLUSTER_ID);
			mask.registerFormat ("%-3d ", ATTR_PROC_ID);
			mask.registerFormat ( (StringCustomFmt) format_owner,
								  ATTR_OWNER, "[????????????] " );
			mask.registerFormat(" ", "*bogus*", " ");  // force space
			mask.registerFormat ( (IntCustomFmt) format_q_date,
								  ATTR_Q_DATE, "[????????????]");
			mask.registerFormat(" ", "*bogus*", " ");  // force space
			mask.registerFormat ( (FloatCustomFmt) format_cpu_time,
								  ATTR_JOB_REMOTE_USER_CPU, "[??????????]");
			mask.registerFormat(" ", "*bogus*", " "); // force space
			// We send in ATTR_OWNER since we know it is always
			// defined, and we need to make sure format_remote_host() is
			// always called. We are actually displaying ATTR_REMOTE_HOST if
			// defined, but we play some tricks if it isn't defined.
			mask.registerFormat ( (StringCustomFmt) format_remote_host,
								  ATTR_OWNER, "[????????????????]");
			mask.registerFormat("\n", "*bogus*", "\n");  // force newline
			mask.display(stdout, &jobs);
		} else {
			short_header();
			jobs.Open();
			while( job = jobs.Next() ) {
				displayJobShort( job );
			}
			jobs.Close();
		}

		if( summarize ) {
			printf( "\n%d jobs; "
					"%d idle, %d running, %d held",
					unexpanded+idle+running+held+malformed,
					idle,running,held);
			if (unexpanded>0) printf( ", %d unexpanded",unexpanded);
			if (malformed>0) printf( ", %d malformed",malformed);
            printf("\n");
		}
	}
	return true;
}


static void
setupAnalysis()
{
	CondorQuery	query(STARTD_AD);
	int			rval;
	char		buffer[64];
	char		*phold;
	ClassAd		*ad;
	char		remoteUser[128];
	int			index;

	// fetch startd ads
	if( pool ) {
		rval = query.fetchAds( startdAds , pool );
	} else {
		rval = query.fetchAds( startdAds );
	}
	if( rval != Q_OK ) {
		fprintf( stderr , "Error:  Could not fetch startd ads\n" );
		exit( 1 );
	}

	// fetch submittor prios
	fetchSubmittorPrios();

	// populate startd ads with remote user prios
	startdAds.Open();
	while( ( ad = startdAds.Next() ) ) {
		if( ad->LookupString( ATTR_REMOTE_USER , remoteUser ) ) {
			if( ( index = findSubmittor( remoteUser ) ) != -1 ) {
				sprintf( buffer , "%s = %f" , ATTR_REMOTE_USER_PRIO , 
							prioTable[index].prio );
				ad->Insert( buffer );
			}
		}
	}
	startdAds.Close();
	

	// setup condition expressions
    sprintf( buffer, "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK );
    Parse( buffer, stdRankCondition );

    sprintf( buffer, "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK );
    Parse( buffer, preemptRankCondition );

	sprintf( buffer, "MY.%s > TARGET.%s + %f", ATTR_REMOTE_USER_PRIO, 
			ATTR_SUBMITTOR_PRIO, PriorityDelta );
	Parse( buffer, preemptPrioCondition ) ;

	// setup preemption hold expression
	if( !( phold = param( "PREEMPTION_HOLD" ) ) ) {
		fprintf( stderr, "\nWarning:  No PREEMPTION_HOLD expression in "
					"config file --- assuming FALSE\n\n" );
		Parse( "FALSE", preemptionHold );
	} else {
		if( Parse( phold , preemptionHold ) ) {
			fprintf( stderr, "\nError:  Failed parse of PREEMPTION_HOLD "
				"expression: \n\t%s\n", phold );
			exit( 1 );
		}
		free( phold );
	}

}


static void
fetchSubmittorPrios()
{
	AttrList	al;
	int		numSub;
	char  	attrName[32], attrPrio[32];
  	char  	name[128];
  	float 	priority;
	int		i = 1;

		// Minor hack, if we're talking to a remote pool, assume the
		// negotiator is on the same host as the collector.
	Daemon	negotiator( DT_NEGOTIATOR, pool, pool );

	// connect to negotiator
	ReliSock sock;
	sock.connect( negotiator.addr(), 0 );

	sock.encode();
	if( !sock.put( GET_PRIORITY ) || !sock.end_of_message() ) {
		fprintf( stderr, 
				 "Error:  Could not get priorities from negotiator (%s)\n",
				 negotiator.fullHostname() );
		exit( 1 );
	}

	sock.decode();
	if( !al.get(sock) || !sock.end_of_message() ) {
		fprintf( stderr, 
				 "Error:  Could not get priorities from negotiator (%s)\n",
				 negotiator.fullHostname() );
		exit( 1 );
	}

	i = 1;
	while( i ) {
    	sprintf( attrName , "Name%d", i );
    	sprintf( attrPrio , "Priority%d", i );

    	if( !al.LookupString( attrName, name ) || 
			!al.LookupFloat( attrPrio, priority ) )
            break;

		prioTable[i-1].name = name;
		prioTable[i-1].prio = priority;
		i++;
	}

	if( i == 1 ) {
		printf( "Warning:  Found no submitters\n" );
	}
}


static void
doRunAnalysis( ClassAd *request )
{
	char	owner[128];
	char	remoteUser[128];
	char	buffer[128];
	int		index;
	ClassAd	*offer;
	EvalResult	result;
	int		cluster, proc;
	int		jobState;
	int		niceUser;

	int 	fReqConstraint 	= 0;
	int		fOffConstraint 	= 0;
	int		fRankCond		= 0;
	int		fPreemptPrioCond= 0;
	int		fPreemptHoldTest= 0;
	int		available		= 0;
	int		totalMachines	= 0;

	if( !request->LookupString( ATTR_OWNER , owner ) ) return;
	if( !request->LookupInteger( ATTR_NICE_USER , niceUser ) ) niceUser = 0;

	if( ( index = findSubmittor( fixSubmittorName( owner, niceUser ) ) ) < 0 ) 
		return;

	sprintf( buffer , "%s = %f" , ATTR_SUBMITTOR_PRIO , prioTable[index].prio );
	request->Insert( buffer );

	request->LookupInteger( ATTR_CLUSTER_ID, cluster );
	request->LookupInteger( ATTR_PROC_ID, proc );
	request->LookupInteger( ATTR_JOB_STATUS, jobState );
	if( jobState == RUNNING ) {
		printf( "---\n%03d.%03d:  Request is being serviced\n\n", cluster, 
			proc );
		return;
	}
	if( jobState == HELD ) {
		printf( "---\n%03d.%03d:  Request is held.\n\n", cluster, 
			proc );
		return;
	}
	if( jobState == REMOVED ) {
		printf( "---\n%03d.%03d:  Request is removed.\n\n", cluster, 
			proc );
		return;
	}

	startdAds.Open();
	while( ( offer = startdAds.Next() ) ) {
		// 0.  info from machine
		remoteUser[0] = '\0';
		totalMachines++;
		offer->LookupString( ATTR_NAME , buffer );
		if( verbose ) printf( "%-15.15s ", buffer );

		// 1. Request satisfied? 
		if( !( (*offer) >= (*request) ) ) {
			if( verbose ) printf( "Failed request constraint\n" );
			fReqConstraint++;
			continue;
		} 

		// 2. Offer satisfied? 
		if( !( (*offer) <= (*request) ) ) {
			if( verbose ) printf( "Failed offer constraint\n" );
			fOffConstraint++;
			continue;
		}	

			
		// 3. Is there a remote user?
		if( !offer->LookupString( ATTR_REMOTE_USER, remoteUser ) ) {
			// both sides satisfied and no remote user
			if( verbose ) printf( "Available\n" );
			available++;
			continue;
		}

		// 4. Satisfies preemption priority condition?
		if( preemptPrioCondition->EvalTree( offer, request, &result ) &&
			result.type == LX_INTEGER && result.i == TRUE ) {

			// 5. Satisfies standard rank condition?
			if( stdRankCondition->EvalTree( offer , request , &result ) &&
				result.type == LX_INTEGER && result.i == TRUE )  
			{
				if( verbose ) printf( "Available\n" );
				available++;
				continue;
			} else {
				// 6.  Satisfies preemption rank condition?
				if( preemptRankCondition->EvalTree( offer, request, &result ) &&
					result.type == LX_INTEGER && result.i == TRUE )
				{
					// 7.  Tripped on PREEMPTION_HOLD?
					if( preemptionHold->EvalTree( offer , request , &result ) &&
						result.type == LX_INTEGER && result.i == TRUE ) 
					{
						fPreemptHoldTest++;
						if( verbose ) {
							printf( "Can preempt %s, but failed PreemptionHold "
								"test\n", remoteUser);
						}
						continue;
					} else {
						// not held
						if( verbose ) {
							printf( "Available (can preempt %s)\n", remoteUser);
						}
						available++;
					}
				} else {
					// failed 6 and 5, but satisfies 4; so have priority
					// but not better or equally preferred than current
					// customer
					fRankCond++;
				}
			} 
		} else {
			// failed 4
			fPreemptPrioCond++;
			if( verbose ) {
				printf( "Insufficient priority to preempt %s\n" , 
					remoteUser );
			}
			continue;
		}
	}
	startdAds.Close();

	printf( "---\n%03d.%03d:  Run analysis summary.  Of %d resource offers,\n", 
			cluster, proc, totalMachines );
	printf( "\t%5d do not satisfy the request's constraints\n", fReqConstraint);
	printf( "\t%5d resource offer constraints are not satisfied by this "
			"request\n", fOffConstraint );
	printf( "\t%5d are serving equal or higher priority customers%s\n", 
					fPreemptPrioCond, niceUser ? "(*)" : "" );
	printf( "\t%5d are serving more preferred customers\n", fRankCond );
	printf( "\t%5d cannot preempt because preemption has been held\n", 
			fPreemptHoldTest );
	printf( "\t%5d are available to service your request\n", available );

	if( niceUser ) {
		printf( "\n\t(*)  Since this is a \"nice-user\" request, this request "
			"has a\n\t     very low priority and is unlikely to preempt other "
			"requests.\n" );
	}
			

	if( fReqConstraint == totalMachines ) {
		char reqs[2048];
		ExprTree *reqExp;
		printf( "\nWARNING:  Be advised:\n" );
		printf( "   No resources matched request's constraints\n" );
		printf( "   Check the %s expression below:\n\n" , ATTR_REQUIREMENTS );
		if( !(reqExp = request->Lookup( ATTR_REQUIREMENTS) ) ) {
			printf( "   ERROR:  No %s expression found" , ATTR_REQUIREMENTS );
		} else {
			reqs[0] = '\0';
			reqExp->PrintToStr( reqs );
			printf( "%s\n\n", reqs );
		}
	}

	if( fOffConstraint == totalMachines ) {
		printf( "\nWARNING:  Be advised:" );
		printf("   Request %d.%d did not match any resource's constraints\n\n",
				cluster, proc);
	}
}


static int
findSubmittor( char *name ) 
{
	MyString 	sub(name);
	int			last = prioTable.getlast();
	int			i;
	
	for( i = 0 ; i <= last ; i++ ) {
		if( prioTable[i].name == sub ) return i;
	}

	prioTable[i].name = sub;
	prioTable[i].prio = 0.5;

	return i;
}


static char*
fixSubmittorName( char *name, int niceUser )
{
	static 	bool initialized = false;
	static	char uid_domain[64];
	static	char buffer[128];
			char *at;

	if( !initialized ) {
		char *tmp = param( "UID_DOMAIN" );
		if( !tmp ) {
			fprintf( stderr, "Error:  UID_DOMAIN not found in config file\n" );
			exit( 1 );
		}
		strcpy( uid_domain , tmp );
		free( tmp );
		initialized = true;
	}

	if( strchr( name , '@' ) ) {
		sprintf( buffer, "%s%s%s", 
					niceUser ? NiceUserName : "",
					niceUser ? "." : "",
					name );
		return buffer;
	} else {
		sprintf( buffer, "%s%s%s@%s", 
					niceUser ? NiceUserName : "",
					niceUser ? "." : "",
					name, uid_domain );
		return buffer;
	}

	return NULL;
}
