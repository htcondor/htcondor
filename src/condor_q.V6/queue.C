#include "condor_common.h"
#include <iostream.h>
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_query.h"
#include "condor_q.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "my_hostname.h"
#include "get_daemon_addr.h"
#include "files.h"

extern 	"C" int SetSyscalls(int val){return val;}
extern  void short_print(int,int,const char*,int,int,int,int,int,const char *);
static  void processCommandLineArguments(int, char *[]);
extern 	"C" void set_debug_flags( char * );

static	char *_FileName_ = __FILE__;

static 	void short_header (void);
static 	void usage (char *);
static 	void displayJobShort (ClassAd *);
static 	void shorten (char *, int);
static	bool show_queue (char*, char*, char*);

static 	int verbose = 0, summarize = 1, global = 0;
static 	int malformed, unexpanded, running, idle;

static	CondorQ 	Q;
static	QueryResult result;
static	CondorQuery	scheddQuery(SCHEDD_AD);
static	CondorQuery submittorQuery(SUBMITTOR_AD);
static	ClassAdList	scheddList;

static	bool		querySchedds 	= false;
static	bool		querySubmittors = false;
static	char		constraint[4096];

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

	// if we haven't figured out what to do yet, just display the
	// local queue 
	if (!global && !querySchedds && !querySubmittors) {
		char *tmp = get_schedd_addr(0);
		if( !tmp ) {
			fprintf( stderr, "Can't find address of local schedd\n" );
			exit( 1 );
		}
		sprintf( scheddAddr, "%s", tmp );
		sprintf( scheddName, "%s", my_daemon_name("SCHEDD") );		
		sprintf( scheddMachine, "%s", my_full_hostname() );
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
	result = querySchedds ? scheddQuery.fetchAds(scheddList) : 
							submittorQuery.fetchAds(scheddList);
	if (result != Q_OK)
	{
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

	if (first)
	{
		fprintf (stderr,"Error: Collector has no record of schedd/submittor\n");
		exit(1);
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
		if (match_prefix (arg, "D")) {
			Termlog = 1;
			set_debug_flags( argv[++i] );
		} 
		else
		if (match_prefix (arg, "name")) {

			if (querySubmittors) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submittors\n");
				exit(1);
			}

			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Argument -name requires another parameter\n");
				exit(1);
			}

			if( !(daemonname = get_daemon_name(argv[i+1])) ) {
				fprintf( stderr, "%s: unknown host %s\n",
						 argv[0], get_host_part(argv[1+1]) );
				exit(1);
			}
			sprintf (constraint, "%s == \"%s\"", ATTR_NAME, daemonname);

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
		if (match_prefix (arg, "submittor")) {

			if (querySchedds) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submittors\n");
				exit(1);
			}
			
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Argument -submittor requires another "
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

			if (Q.add (CQ_OWNER, argv[i]) != Q_OK) {
				fprintf (stderr, "Error:  Argument %d (%s)\n", i, argv[i]);
				exit (1);
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
	
	shorten (owner, 14);
	if (ad->EvalString ("Args", NULL, args)) strcat (cmd, args);
	shorten (cmd, 18);
	short_print (cluster, proc, owner, date, (int)utime, status, prio,
					image_size, cmd); 

	switch (status)
	{
		case UNEXPANDED: unexpanded++; break;
		case IDLE:       idle++;       break;
		case RUNNING:    running++;    break;
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
		"\t\t-submittor <submittor>\tGet queue of specific submittor\n"
		"\t\t-help\t\t\tThis screen\n"
		"\t\t-name <name>\t\tName of schedd\n"
		"\t\t-constraint <expr>\tAdd constraint on classads\n"
		"\t\t-long\t\t\tVerbose output\n"
		"\t\t<cluster>\t\tGet information about specific cluster\n"
		"\t\t<cluster>.<proc>\tGet information about specific job\n"
		"\t\t<owner>\t\t\tInformation about jobs owned by <owner>\n", myName);
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

		// display the jobs from this submittor
	if( jobs.MyLength() != 0 || !global ) {
			// print header
		if( querySchedds ) {
			printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
		} else {
			printf ("\n\n-- Submittor: %s : %s : %s\n", scheddName, 
					scheddAddr, scheddMachine);	
		}
		
			// initialize counters
		malformed = 0; idle = 0; running = 0; unexpanded = 0;
		
		if( verbose ) {
			jobs.fPrintAttrListList( stdout );
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
					"%d unexpanded, %d idle, %d running, %d malformed\n",
					unexpanded+idle+running+malformed,unexpanded,idle,running, 
					malformed);
		}
	}
	return true;
}
