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
#include "condor_string.h"
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
#include "basename.h"
#include "metric_units.h"

extern 	"C" int SetSyscalls(int val){return val;}
extern  void short_print(int,int,const char*,int,int,int,int,int,const char *);
extern  void short_print_to_buffer(char*,int,int,const char*,int,int,int,int,
							int, const char *);
static  void processCommandLineArguments(int, char *[]);

static  bool process_buffer_line( ClassAd * );

static 	void short_header (void);
static 	void usage (char *);
static 	void io_display (ClassAd *);
static 	char * buffer_io_display (ClassAd *);
static 	void displayJobShort (ClassAd *);
static 	char * bufferJobShort (ClassAd *);
static 	void shorten (char *, int);
static	bool show_queue (char* scheddAddr, char* scheddName, char* scheddMachine);
static	bool show_queue_buffered (char* scheddAddr, char* scheddName,
								  char* scheddMachine);

static 	int verbose = 0, summarize = 1, global = 0, show_io = 0;
static 	int malformed, unexpanded, running, idle, held;

static	CondorQ 	Q;
static	QueryResult result;
static	CondorQuery	scheddQuery(SCHEDD_AD);
static	CondorQuery submittorQuery(SUBMITTOR_AD);
static	ClassAdList	scheddList;

// clusterProcString is the container where the output strings are
//    stored.  We need the cluster and proc so that we can sort in an
//    orderly manner (and don't need to rely on the cluster.proc to be
//    available....)

typedef struct {
	int cluster;
	int proc;
	char * string;
} clusterProcString;

static  ExtArray<clusterProcString *> *output_buffer;
static	bool		output_buffer_empty = true;

static	bool		usingPrintMask = false;
static 	bool		customFormat = false;
static  bool		cputime = false;
static	bool		current_run = false;
static 	bool		globus = false;
static  char		*JOB_TIME = "RUN_TIME";
static	bool		querySchedds 	= false;
static	bool		querySubmittors = false;
static	char		constraint[4096];
static	char		*pool = NULL;
static	char		scheddAddr[64];	// used by format_remote_host()
static	AttrListPrintMask 	mask;

// for run failure analysis
static  int			findSubmittor( char * );
static	void 		setupAnalysis();
static 	void		fetchSubmittorPrios();
static	void		doRunAnalysis( ClassAd* );
static	char *		doRunAnalysisToBuffer( ClassAd* );
struct 	PrioEntry { MyString name; float prio; };
static 	bool		analyze	= false;
static	bool		run = false;
static	bool		goodput = false;
static	char		*fixSubmittorName( char*, int );
static	ClassAdList startdAds;
static	ExprTree	*stdRankCondition;
static	ExprTree	*preemptRankCondition;
static	ExprTree	*preemptPrioCondition;
static	ExprTree	*preemptionReq;
static  ExtArray<PrioEntry> prioTable;
#ifndef WIN32
template class ExtArray<PrioEntry>;
#endif
	
char return_buff[4096];

extern 	"C"	int		Termlog;

int main (int argc, char **argv)
{
	ClassAd		*ad;
	bool		first = true;
	char		scheddName[64];
	char		scheddMachine[64];
	char		*tmp;

	// load up configuration file
	config();

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
		if ( schedd.locate() ) {
			sprintf( scheddAddr, "%s", schedd.addr() );
			if( (tmp = schedd.name()) ) {
				sprintf( scheddName, "%s", tmp );
			} else {
				sprintf( scheddName, "Unknown" );
			}
			if( (tmp = schedd.fullHostname()) ) {
				sprintf( scheddMachine, "%s", tmp );
			} else {
				sprintf( scheddMachine, "Unknown" );
			}
			if ( verbose ) {
				exit( !show_queue( scheddAddr, scheddName,
							scheddMachine ) );
			} else {
				exit( !show_queue_buffered( scheddAddr, scheddName,
							scheddMachine ) );
			}
		} else {
			fprintf( stderr, "Can't display queue of local schedd\n" );
			exit( 1 );
		}
	}
	
	// if a global queue is required, query the schedds instead of submittors
	if (global) {
		querySchedds = true;
		sprintf( constraint, "%s > 0 || %s > 0 || %s > 0 || %s > 0", 
			ATTR_TOTAL_RUNNING_JOBS, ATTR_TOTAL_IDLE_JOBS,
			ATTR_TOTAL_HELD_JOBS, ATTR_TOTAL_REMOVED_JOBS );
		result = scheddQuery.addANDConstraint( constraint );
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
	
		if ( verbose ) {
			show_queue( scheddAddr, scheddName, scheddMachine );
		} else {
			show_queue_buffered( scheddAddr, scheddName, scheddMachine );
		}
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
				delete [] pool;
			}
			i++;
			if( ! (argv[i] && *argv[i]) ) {
				fprintf( stderr, 
						 "Error: Argument -pool requires another parameter\n" );
				exit(1);
			}
			pool = get_full_hostname((const char *)argv[i]);
			if( ! pool ) {
				fprintf( stderr, "%s: unknown host %s\n", 
						 argv[0], argv[i] );
				exit(1);
			}
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
				fprintf( stderr, 
						 "Error: Argument -name requires another parameter\n" );
				exit(1);
			}

			if( !(daemonname = get_daemon_name(argv[i+1])) ) {
				fprintf( stderr, "%s: unknown host %s\n",
						 argv[0], get_host_part(argv[i+1]) );
				exit(1);
			}
			sprintf (constraint, "%s == \"%s\"", ATTR_NAME, daemonname);
			delete [] daemonname;

			result = scheddQuery.addORConstraint (constraint);
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
				fprintf( stderr, "Error: Argument -submitter requires another "
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
			result = submittorQuery.addORConstraint (constraint);
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
				fprintf( stderr, "Error: Argument -constraint requires "
							"another parameter\n");
				exit(1);
			}

			if (Q.addAND (argv[++i]) != Q_OK) {
				fprintf (stderr, "Error: Argument %d (%s)\n", i, argv[i]);
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
				fprintf( stderr,
						 "Error: Argument -address requires another "
						 "parameter\n" );
				exit(1);
			}
			if( ! is_valid_sinful(argv[i+1]) ) {
				fprintf( stderr, 
					 "Address must be of the form: \"<ip.address:port>\"\n" );
				exit(1);
			}
			sprintf(constraint, "%s == \"%s\"", ATTR_SCHEDD_IP_ADDR, argv[i+1]);
			result = scheddQuery.addORConstraint(constraint);
			if (result != Q_OK) {
				fprintf (stderr, "Argument %d (%s): Error %s\n", i, argv[i],
							getStrQueryResult(result));
				exit (1);
			}
			i++;
			querySchedds = true;
		} 
		else
		if( match_prefix( arg, "format" ) ) {
				// make sure we have at least two more arguments
			if( argc <= i+2 ) {
				fprintf( stderr, "Error: Argument -format requires "
						 "format and attribute parameters\n" );
				exit( 1 );
			}
			customFormat = true;
			mask.registerFormat( argv[i+1], argv[i+2] );
			usingPrintMask = true;
			i+=2;
		}
		else
		if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
			sprintf (constraint, "((%s == %d) && (%s == %d))", 
									ATTR_CLUSTER_ID, cluster,
									ATTR_PROC_ID, proc);
			Q.addOR (constraint);
			summarize = 0;
		} 
		else
		if (sscanf (argv[i], "%d", &cluster) == 1) {
			sprintf (constraint, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
			Q.addOR (constraint);
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
		if (match_prefix( arg, "goodput")) {
			cputime = false;
			goodput = true;
		}
		else
		if (match_prefix( arg, "cputime")) {
			cputime = true;
			goodput = false;
			JOB_TIME = "CPU_TIME";
		}
		else
		if (match_prefix( arg, "currentrun")) {
			current_run = true;
		}
		else
		if( match_prefix( arg, "globus" ) ) {
			Q.addAND( "GlobusStatus =!= UNDEFINED" );
			globus = true;
		}
		else
		if (match_prefix(arg,"io")) {
			show_io = true;
		}   
		else {
			// assume name of owner of job
			if (Q.add (CQ_OWNER, argv[i]) != Q_OK) {
				fprintf (stderr, "Error: Argument %d (%s)\n", i, argv[i]);
				exit (1);
			}
		}
	}
}

static float
job_time(float cpu_time,ClassAd *ad)
{
	if ( cputime ) {
		return cpu_time;
	}

		// here user wants total wall clock time, not cpu time
	int job_status = !RUNNING;
	int cur_time = 0;
	int shadow_bday = 0;
	float previous_runs = 0;

	ad->LookupInteger( ATTR_JOB_STATUS, job_status);
	ad->LookupInteger( ATTR_SERVER_TIME, cur_time);
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	if ( current_run == false ) {
		ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );
	}

		// if we have an old schedd, there is no ATTR_SERVER_TIME,
		// so return a "-1".  This will cause "?????" to show up
		// in condor_q.
	if ( cur_time == 0 ) {
		return -1;
	}

	/* Compute total wall time as follows:  previous_runs is not the 
	 * number of seconds accumulated on earlier runs.  cur_time is the
	 * time from the point of view of the schedd, and shadow_bday is the
	 * epoch time from the schedd's view when the shadow was started for
	 * this job.  So, we figure out the time accumulated on this run by
	 * computing the time elapsed between cur_time & shadow_bday.  
	 * NOTE: shadow_bday is set to zero when the job is RUNNING but the
	 * shadow has not yet started due to JOB_START_DELAY parameter.  And
	 * shadow_bday is undefined (stale value) if the job status is not
	 * RUNNING.  So we only compute the time on this run if shadow_bday
	 * is not zero and the job status is RUNNING.  -Todd <tannenba@cs.wisc.edu>
	 */
	float total_wall_time = previous_runs + 
		(cur_time - shadow_bday)*(job_status == RUNNING && shadow_bday);

	return total_wall_time;
}

static void io_header()
{
	printf("%-8s %-8s %8s %8s %8s %10s %8s %8s\n", "ID","OWNER","READ","WRITE","SEEK","XPUT","BUFSIZE","BLKSIZE");
}

static void
io_display(ClassAd *ad)
{
	printf("%s", buffer_io_display( ad ) );
}

static char *
buffer_io_display( ClassAd *ad )
{
	int cluster=0, proc=0;
	float read_bytes=0, write_bytes=0, seek_count=0;
	int buffer_size=0, block_size=0;
	float wall_clock=-1;

	char owner[256];

	ad->EvalInteger(ATTR_CLUSTER_ID,NULL,cluster);
	ad->EvalInteger(ATTR_PROC_ID,NULL,proc);
	ad->EvalString(ATTR_OWNER,NULL,owner);

	ad->EvalFloat(ATTR_FILE_READ_BYTES,NULL,read_bytes);
	ad->EvalFloat(ATTR_FILE_WRITE_BYTES,NULL,write_bytes);
	ad->EvalFloat(ATTR_FILE_SEEK_COUNT,NULL,seek_count);
	ad->EvalFloat(ATTR_JOB_REMOTE_WALL_CLOCK,NULL,wall_clock);
	ad->EvalInteger(ATTR_BUFFER_SIZE,NULL,buffer_size);
	ad->EvalInteger(ATTR_BUFFER_BLOCK_SIZE,NULL,block_size);

	sprintf(return_buff, "%4d.%-3d %-8s",cluster,proc,owner);

	/* If the jobAd values are not set, OR the values are all zero,
	   report no data collected.  This could be true for a vanilla
	   job, or for a standard job that has not checkpointed yet. */

	if(wall_clock<0 || (!read_bytes && !write_bytes && !seek_count) ) {
		strcat(return_buff, "          [ no i/o data collected yet ]\n");
	} else {
		if(wall_clock==0) wall_clock=1;

		/*
		Note: metric_units() cannot be used twice in the same
		statement -- it returns a pointer to a static buffer.
		*/

		sprintf( return_buff,"%s %8s", return_buff, metric_units(read_bytes) );
		sprintf( return_buff,"%s %8s", return_buff, metric_units(write_bytes) );
		sprintf( return_buff,"%s %8.0f", return_buff, seek_count );
		sprintf( return_buff,"%s %8s/s", return_buff, metric_units((int)((read_bytes+write_bytes)/wall_clock)) );
		sprintf( return_buff,"%s %8s", return_buff, metric_units(buffer_size) );
		sprintf( return_buff,"%s %8s\n", return_buff, metric_units(block_size) );
	}
	return ( return_buff );
}

static void
displayJobShort (ClassAd *ad)
{ 
	printf( "%s", bufferJobShort( ad ) );
}

static char *
bufferJobShort( ClassAd *ad ) {
	int cluster, proc, date, status, prio, image_size;
	float utime;
	char owner[64], cmd[ATTRLIST_MAX_EXPRESSION], args[ATTRLIST_MAX_EXPRESSION];
	char buffer[ATTRLIST_MAX_EXPRESSION];

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
		sprintf (return_buff, " --- ???? --- \n");
		return( return_buff );
	}
	
	int niceUser;
    if( ad->LookupInteger( ATTR_NICE_USER, niceUser ) && niceUser ) {
        char tmp[100];
        strncpy(tmp,NiceUserName,99);
        strcat(tmp,".");
        strcat(tmp,owner);
        strncpy(owner,tmp, 63);
    }

	shorten (owner, 14);
	if (ad->EvalString ("Args", NULL, args)) {
		sprintf( buffer, "%s %s", basename(cmd), args );
	} else {
		sprintf( buffer, "%s", basename(cmd) );
	}
	utime = job_time(utime,ad);
	short_print_to_buffer (return_buff, cluster, proc, owner, date, (int)utime,
					status, prio, image_size, buffer); 

	switch (status)
	{
		case UNEXPANDED: unexpanded++; break;
		case IDLE:       idle++;       break;
		case RUNNING:    running++;    break;
		case HELD:		 held++;	   break;
	}
	return return_buff;
}

static void 
short_header (void)
{
	if ( goodput || run ) {
		printf( " %-7s %-14s %11s %12s %-16s\n", "ID", "OWNER",
				"SUBMITTED", JOB_TIME,
				run ? "HOST(S)" : "GOODPUT CPU_UTIL   Mb/s" );
	} else if ( globus ) {
		printf( " %-7s %-14s %-7s %-8s %-18s  %-18s\n", 
			"ID", "OWNER", "STATUS", "MANAGER", "HOST", "EXECUTABLE" );
	} else if ( show_io ) {
		io_header();
	} else {
		printf( " %-7s %-14s %11s %12s %-2s %-3s %-4s %-18s\n",
			"ID",
			"OWNER",
			"SUBMITTED",
			JOB_TIME,
			"ST",
			"PRI",
			"SIZE",
			"CMD"
		);
	}
}

static char *
format_remote_host (char *, AttrList *ad)
{
	static char result[MAXHOSTNAMELEN];
	static char unknownHost [] = "[????????????????]";
	char* tmp;
	struct sockaddr_in sin;

	int universe = STANDARD;
	ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	if (universe == SCHED_UNIVERSE &&
		string_to_sin(scheddAddr, &sin) == 1) {
		if( (tmp = sin_to_hostname(&sin, NULL)) ) {
			strcpy( result, tmp );
			return result;
		} else {
			return unknownHost;
		}
	} else if (universe == PVM) {
		int current_hosts;
		if (ad->LookupInteger( ATTR_CURRENT_HOSTS, current_hosts ) == 1) {
			if (current_hosts == 1) {
				sprintf(result, "1 host");
			} else {
				sprintf(result, "%d hosts", current_hosts);
			}
			return result;
		}
	}
	if (ad->LookupString(ATTR_REMOTE_HOST, result) == 1) {
		if( is_valid_sinful(result) && 
			(string_to_sin(result, &sin) == 1) ) {  
			if( (tmp = sin_to_hostname(&sin, NULL)) ) {
				strcpy( result, tmp );
			} else {
				return unknownHost;
			}
		}
		return result;
	}
	return unknownHost;
}

static char *
format_cpu_time (float utime, AttrList *ad)
{
	return format_time( (int) job_time(utime,(ClassAd *)ad) );
}

static char *
format_goodput (int job_status, AttrList *ad)
{
	static char result[9];
	int ckpt_time = 0, shadow_bday = 0, last_ckpt = 0;
	float wall_clock = 0.0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time );
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	ad->LookupInteger( ATTR_LAST_CKPT_TIME, last_ckpt );
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock );
	if (job_status == RUNNING && shadow_bday && last_ckpt > shadow_bday) {
		wall_clock += last_ckpt - shadow_bday;
	}
	if (wall_clock <= 0.0) return " [?????]";
	float goodput = ckpt_time/wall_clock*100.0;
	if (goodput > 100.0) goodput = 100.0;
	else if (goodput < 0.0) return " [?????]";
	sprintf(result, " %6.1f%%", goodput);
	return result;
}

static char *
format_mbps (float bytes_sent, AttrList *ad)
{
	static char result[10];
	float wall_clock=0.0, bytes_recvd=0.0, total_mbits;
	int shadow_bday = 0, last_ckpt = 0, job_status = IDLE;
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock );
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	ad->LookupInteger( ATTR_LAST_CKPT_TIME, last_ckpt );
	ad->LookupInteger( ATTR_JOB_STATUS, job_status );
	if (job_status == RUNNING && shadow_bday && last_ckpt > shadow_bday) {
		wall_clock += last_ckpt - shadow_bday;
	}
	ad->LookupFloat(ATTR_BYTES_RECVD, bytes_recvd);
	total_mbits = (bytes_sent+bytes_recvd)*8/(1024*1024); // bytes to mbits
	if (total_mbits <= 0) return " [????]";
	sprintf(result, " %6.2f", total_mbits/wall_clock);
	return result;
}

static char *
format_cpu_util (float utime, AttrList *ad)
{
	static char result[10];
	int ckpt_time = 0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time);
	if (ckpt_time == 0) return " [??????]";
	float util = (ckpt_time) ? utime/ckpt_time*100.0 : 0.0;
	if (util > 100.0) util = 100.0;
	else if (util < 0.0) return " [??????]";
	sprintf(result, "  %6.1f%%", util);
	return result;
}

static char *
format_owner (char *owner, AttrList *ad)
{
	static char result[15];
	int niceUser;
	if (ad->LookupInteger( ATTR_NICE_USER, niceUser) && niceUser ) {
		char tmp[100];
		strncpy(tmp,NiceUserName,99);
		strcat(tmp, ".");
		strcat(tmp, owner);
		sprintf(result, "%-14.14s", tmp);
	} else {
		sprintf(result, "%-14.14s", owner);
	}
	return result;
}


static char *
format_globusHostJMAndExec( char  *globusResource, AttrList *ad )
{
	static char result[64];
	char	host[80] = "[?????]";
	char	exec[1024] = "[?????]";
	char	jm[80] = "[?????]";
	char	*tmp;
	int	p;

	// copy the hostname
	p = strcspn( globusResource, ":/" );
	if ( p > sizeof(host) )
		p = sizeof(host) - 1;
	strncpy( host, tmp, p );
	host[p] = '\0';

	if ( ( tmp = strstr( globusResource, "jobmanager-" ) ) != NULL ) {
		tmp += 11; // 11==strlen("jobmanager-")

		// copy the jobmanager name
		p = strcspn( tmp, ":" );
		if ( p > sizeof(jm) )
			p = sizeof(jm) - 1;
		strncpy( jm, tmp, p );
		jm[p] = '\0';
	}

	// get the executable name
	ad->LookupString(ATTR_JOB_CMD, result);

	// done --- pack components into the result string and return
	sprintf( result, " %-8.8s %-18.18s  %-18.18s\n", jm, host,
			 basename(exec) );
	return( result );
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
		"\t\t-format <fmt> <attr>\tPrint attribute attr using format fmt\n"
		"\t\t-analyze\t\tPerform schedulability analysis on jobs\n"
		"\t\t-run\t\t\tGet information about running jobs\n"
		"\t\t-goodput\t\tDisplay job goodput statistics\n"	
		"\t\t-cputime\t\tDisplay CPU_TIME instead of RUN_TIME\n"
		"\t\t-currentrun\t\tDisplay times only for current run\n"
		"\t\t-io\t\t\tShow information regarding I/O\n"
		"\t\trestriction list\n"
		"\twhere each restriction may be one of\n"
		"\t\t<cluster>\t\tGet information about specific cluster\n"
		"\t\t<cluster>.<proc>\tGet information about specific job\n"
		"\t\t<owner>\t\t\tInformation about jobs owned by <owner>\n"
		"\t\t-constraint <expr>\tAdd constraint on classads\n",
			myName);
}

int
output_sorter( const void * va, const void * vb ) {

	clusterProcString **a, **b;

	a = ( clusterProcString ** ) va;
	b = ( clusterProcString ** ) vb;

	if ((*a)->cluster < (*b)->cluster ) { return -1; }
	if ((*a)->cluster > (*b)->cluster ) { return  1; }
	if ((*a)->proc    < (*b)->proc    ) { return -1; }
	if ((*a)->proc    > (*b)->proc    ) { return  1; }

	return 0;
}

static bool
show_queue_buffered( char* scheddAddr, char* scheddName, char* scheddMachine )
{
	static bool	setup_mask = false;
	clusterProcString **the_output;
	output_buffer = new ExtArray<clusterProcString*>;

	output_buffer->setFiller( (clusterProcString *) NULL );

		// initialize counters
	unexpanded = idle = running = held = malformed = 0;
	output_buffer_empty = true;

	if ( run || goodput ) {
		summarize = false;
		if (!setup_mask) {
			mask.registerFormat ("%4d.", ATTR_CLUSTER_ID);
			mask.registerFormat ("%-3d ", ATTR_PROC_ID);
			mask.registerFormat ( (StringCustomFmt) format_owner,
								  ATTR_OWNER, "[????????????] " );
			mask.registerFormat(" ", "*bogus*", " ");  // force space
			mask.registerFormat ( (IntCustomFmt) format_q_date,
								  ATTR_Q_DATE, "[????????????]");
			mask.registerFormat(" ", "*bogus*", " ");  // force space
			mask.registerFormat ( (FloatCustomFmt) format_cpu_time,
								  ATTR_JOB_REMOTE_USER_CPU,
								  "[??????????]");
			if ( run ) {
				mask.registerFormat(" ", "*bogus*", " "); // force space
				// We send in ATTR_OWNER since we know it is always
				// defined, and we need to make sure
				// format_remote_host() is always called. We are
				// actually displaying ATTR_REMOTE_HOST if defined,
				// but we play some tricks if it isn't defined.
				mask.registerFormat ( (StringCustomFmt) format_remote_host,
									  ATTR_OWNER, "[????????????????]");
			} else {			// goodput
				mask.registerFormat ( (IntCustomFmt) format_goodput,
									  ATTR_JOB_STATUS,
									  " [?????]");
				mask.registerFormat ( (FloatCustomFmt) format_cpu_util,
									  ATTR_JOB_REMOTE_USER_CPU,
									  " [??????]");
				mask.registerFormat ( (FloatCustomFmt) format_mbps,
									  ATTR_BYTES_SENT,
									  " [????]");
			}
			mask.registerFormat("\n", "*bogus*", "\n");  // force newline
			setup_mask = true;
			usingPrintMask = true;
		}
	} else if( globus ) {
		summarize = false;
		if (!setup_mask) {
			mask.registerFormat ("%4d.", ATTR_CLUSTER_ID);
			mask.registerFormat ("%-3d ", ATTR_PROC_ID);
			mask.registerFormat ( (StringCustomFmt) format_owner,
								  ATTR_OWNER, "[????????????] " );
			mask.registerFormat( "%7s ", "GlobusStatus" );
			mask.registerFormat( (StringCustomFmt)
								 format_globusHostJMAndExec,
								 ATTR_GLOBUS_RESOURCE, "[?????] [?????]\n" );
			setup_mask = true;
			usingPrintMask = true;
		}
	} else if ( customFormat ) {
		summarize = false;
	}

	// fetch queue from schedd and stash it in output_buffer.
	if( Q.fetchQueueFromHostAndProcess( scheddAddr,
									 process_buffer_line ) != Q_OK ) {
		printf ("\n-- Failed to fetch ads from: %s : %s\n", 
									scheddAddr, scheddMachine);	
		delete output_buffer;

		return false;
	}

	// If this is a global, don't print anything if this schedd is empty.
	// If this is NOT global, print out the header and footer to show that we
	//    did something.
	if (!global || !output_buffer_empty) {
		the_output = &(*output_buffer)[0];
		qsort(the_output, output_buffer->getlast()+1, sizeof(clusterProcString*),
			output_sorter);

		if (! customFormat ) {
			if( querySchedds ) {
				printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
			} else {
				printf ("\n\n-- Submitter: %s : %s : %s\n", scheddName, 
						scheddAddr, scheddMachine);	
			}
			// Print the output header
		
			short_header();
		}

		if (!output_buffer_empty) {
			for (int i=0;i<=output_buffer->getlast(); i++) {
				if ((*output_buffer)[i])
					printf("%s",((*output_buffer)[i])->string);
			}
		}

		// If we want to summarize, do that too.
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

	delete output_buffer;

	return true;
}


// process_buffer_line returns 1 so that the ad that is passed
// to it should be deleted.


static bool
process_buffer_line( ClassAd *job )
{
	clusterProcString * tempCPS = new clusterProcString;
	(*output_buffer)[output_buffer->getlast()+1] = tempCPS;
	job->LookupInteger( ATTR_CLUSTER_ID, tempCPS->cluster );
	job->LookupInteger( ATTR_PROC_ID, tempCPS->proc );

	if( analyze ) {
		tempCPS->string = strnewp( doRunAnalysisToBuffer( job ) );
	} else if ( show_io ) {
		tempCPS->string = strnewp( buffer_io_display( job ) );
	} else if ( usingPrintMask ) {
		char * tempSTR = mask.display ( job );
		// strnewp the mask.display return, since its an 8k string.
		tempCPS->string = strnewp( tempSTR );
		delete [] tempSTR;
	} else {
		tempCPS->string = strnewp( bufferJobShort( job ) );
	}

	output_buffer_empty = false;

	return true;
}

static bool
show_queue( char* scheddAddr, char* scheddName, char* scheddMachine )
{
	ClassAdList jobs; 
	ClassAd		*job;
	static bool	setup_mask = false;

		// fetch queue from schedd	
	if( Q.fetchQueueFromHost(jobs, scheddAddr) != Q_OK ) {
		printf ("\n-- Failed to fetch ads from: %s : %s\n", 
									scheddAddr, scheddMachine);	
		return false;
	}

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
		if ( ! customFormat ) {
			if( querySchedds ) {
				printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
			} else {
				printf ("\n\n-- Submitter: %s : %s : %s\n", scheddName, 
					scheddAddr, scheddMachine);	
			}
		}
		
			// initialize counters
		malformed = 0; idle = 0; running = 0; unexpanded = 0, held = 0;
		
		if( verbose ) {
			jobs.fPrintAttrListList( stdout );
		} else if( customFormat ) {
			summarize = false;
			mask.display( stdout, &jobs );
		} else if( globus ) {
			summarize = false;
			printf( " %-7s %-14s %-7s %-8s %-18s  %-18s\n", 
				"ID", "OWNER", "STATUS", "MANAGER", "HOST", "EXECUTABLE" );
			if (!setup_mask) {
				mask.registerFormat ("%4d.", ATTR_CLUSTER_ID);
				mask.registerFormat ("%-3d ", ATTR_PROC_ID);
				mask.registerFormat ( (StringCustomFmt) format_owner,
									  ATTR_OWNER, "[????????????] " );
				mask.registerFormat( "%7s ", "GlobusStatus" );
				mask.registerFormat( (StringCustomFmt)
									 format_globusHostJMAndExec,
									 ATTR_GLOBUS_RESOURCE, "[?????] [?????]\n" );
				setup_mask = true;
				usingPrintMask = true;
			}
			mask.display( stdout, &jobs );
		} else if ( run || goodput ) {
			summarize = false;
			printf( " %-7s %-14s %11s %12s %-16s\n", "ID", "OWNER",
					"SUBMITTED", JOB_TIME,
					run ? "HOST(S)" : "GOODPUT CPU_UTIL   Mb/s" );
			if (!setup_mask) {
				mask.registerFormat ("%4d.", ATTR_CLUSTER_ID);
				mask.registerFormat ("%-3d ", ATTR_PROC_ID);
				mask.registerFormat ( (StringCustomFmt) format_owner,
									  ATTR_OWNER, "[????????????] " );
				mask.registerFormat(" ", "*bogus*", " ");  // force space
				mask.registerFormat ( (IntCustomFmt) format_q_date,
									  ATTR_Q_DATE, "[????????????]");
				mask.registerFormat(" ", "*bogus*", " ");  // force space
				mask.registerFormat ( (FloatCustomFmt) format_cpu_time,
									  ATTR_JOB_REMOTE_USER_CPU,
									  "[??????????]");
				if ( run ) {
					mask.registerFormat(" ", "*bogus*", " "); // force space
					// We send in ATTR_OWNER since we know it is always
					// defined, and we need to make sure
					// format_remote_host() is always called. We are
					// actually displaying ATTR_REMOTE_HOST if defined,
					// but we play some tricks if it isn't defined.
					mask.registerFormat ( (StringCustomFmt) format_remote_host,
										  ATTR_OWNER, "[????????????????]");
				} else {			// goodput
					mask.registerFormat ( (IntCustomFmt) format_goodput,
										  ATTR_JOB_STATUS,
										  " [?????]");
					mask.registerFormat ( (FloatCustomFmt) format_cpu_util,
										  ATTR_JOB_REMOTE_USER_CPU,
										  " [??????]");
					mask.registerFormat ( (FloatCustomFmt) format_mbps,
										  ATTR_BYTES_SENT,
										  " [????]");
				}
				mask.registerFormat("\n", "*bogus*", "\n");  // force newline
				setup_mask = true;
				usingPrintMask = true;
			}
			mask.display(stdout, &jobs);
		} else if( show_io ) {
			io_header();
			jobs.Open();
			while( (job=jobs.Next()) ) {
				io_display( job );
			}
			jobs.Close();
		} else {
			short_header();
			jobs.Open();
			while( (job=jobs.Next()) ) {
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
	char		*preq;
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

	// setup preemption requirements expression
	if( !( preq = param( "PREEMPTION_REQUIREMENTS" ) ) ) {
		fprintf( stderr, "\nWarning:  No PREEMPTION_REQUIREMENTS expression in"
					" config file --- assuming FALSE\n\n" );
		Parse( "FALSE", preemptionReq );
	} else {
		if( Parse( preq , preemptionReq ) ) {
			fprintf( stderr, "\nError:  Failed parse of "
				"PREEMPTION_REQUIREMENTS expression: \n\t%s\n", preq );
			exit( 1 );
		}
		free( preq );
	}

}


static void
fetchSubmittorPrios()
{
	AttrList	al;
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
	printf("%s", doRunAnalysisToBuffer( request) );
}

static char *
doRunAnalysisToBuffer( ClassAd *request )
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
	int		fPreemptReqTest	= 0;
	int		available		= 0;
	int		totalMachines	= 0;

	return_buff[0]='\0';

	if( !request->LookupString( ATTR_OWNER , owner ) ) return "Nothing here.\n";
	if( !request->LookupInteger( ATTR_NICE_USER , niceUser ) ) niceUser = 0;

	if( ( index = findSubmittor( fixSubmittorName( owner, niceUser ) ) ) < 0 ) 
		return "Nothing here.\n";

	sprintf( buffer , "%s = %f" , ATTR_SUBMITTOR_PRIO , prioTable[index].prio );
	request->Insert( buffer );

	request->LookupInteger( ATTR_CLUSTER_ID, cluster );
	request->LookupInteger( ATTR_PROC_ID, proc );
	request->LookupInteger( ATTR_JOB_STATUS, jobState );
	if( jobState == RUNNING ) {
		sprintf( return_buff,
			"---\n%03d.%03d:  Request is being serviced\n\n", cluster, 
			proc );
		return return_buff;
	}
	if( jobState == HELD ) {
		sprintf( return_buff,
			"---\n%03d.%03d:  Request is held.\n\n", cluster, 
			proc );
		return return_buff;
	}
	if( jobState == REMOVED ) {
		sprintf( return_buff,
			"---\n%03d.%03d:  Request is removed.\n\n", cluster, 
			proc );
		return return_buff;
	}

	startdAds.Open();
	while( ( offer = startdAds.Next() ) ) {
		// 0.  info from machine
		remoteUser[0] = '\0';
		totalMachines++;
		offer->LookupString( ATTR_NAME , buffer );
		if( verbose ) sprintf( return_buff, "%-15.15s ", buffer );

		// 1. Request satisfied? 
		if( !( (*offer) >= (*request) ) ) {
			if( verbose ) sprintf( return_buff,
				"%sFailed request constraint\n", return_buff );
			fReqConstraint++;
			continue;
		} 

		// 2. Offer satisfied? 
		if( !( (*offer) <= (*request) ) ) {
			if( verbose ) strcat( return_buff, "Failed offer constraint\n");
			fOffConstraint++;
			continue;
		}	

			
		// 3. Is there a remote user?
		if( !offer->LookupString( ATTR_REMOTE_USER, remoteUser ) ) {
			if( stdRankCondition->EvalTree( offer, request, &result ) &&
					result.type == LX_INTEGER && result.i == TRUE ) {
				// both sides satisfied and no remote user
				if( verbose ) sprintf( return_buff, "%sAvailable\n",
					return_buff );
				available++;
				continue;
			} else {
				// no remote user, but std rank condition failed
				fRankCond++;
				if( verbose ) {
					sprintf( return_buff,
						"%sFailed rank condition: MY.Rank > MY.CurrentRank\n",
						return_buff);
				}
				continue;
			}
		}

		// 4. Satisfies preemption priority condition?
		if( preemptPrioCondition->EvalTree( offer, request, &result ) &&
			result.type == LX_INTEGER && result.i == TRUE ) {

			// 5. Satisfies standard rank condition?
			if( stdRankCondition->EvalTree( offer , request , &result ) &&
				result.type == LX_INTEGER && result.i == TRUE )  
			{
				if( verbose )
					sprintf( return_buff, "%sAvailable\n", return_buff );
				available++;
				continue;
			} else {
				// 6.  Satisfies preemption rank condition?
				if( preemptRankCondition->EvalTree( offer, request, &result ) &&
					result.type == LX_INTEGER && result.i == TRUE )
				{
					// 7.  Tripped on PREEMPTION_REQUIREMENTS?
					if( preemptionReq->EvalTree( offer , request , &result ) &&
						result.type == LX_INTEGER && result.i == FALSE ) 
					{
						fPreemptReqTest++;
						if( verbose ) {
							sprintf( return_buff,
									"%sCan preempt %s, but failed "
									"PREEMPTION_REQUIREMENTS test\n",
									return_buff,
									remoteUser);
						}
						continue;
					} else {
						// not held
						if( verbose ) {
							sprintf( return_buff,
								"%sAvailable (can preempt %s)\n",
								return_buff, remoteUser);
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
				sprintf( return_buff,
					"%sInsufficient priority to preempt %s\n" , 
					return_buff, remoteUser );
			}
			continue;
		}
	}
	startdAds.Close();

	sprintf( return_buff,
		"%s---\n%03d.%03d:  Run analysis summary.  Of %d resource offers,\n" 
		"\t%5d do not satisfy the request's constraints\n"
		"\t%5d resource offer constraints are not satisfied by this request\n"
		"\t%5d are serving equal or higher priority customers%s\n" 
		"\t%5d do not prefer this job\n"
		"\t%5d cannot preempt because PREEMPTION_REQUIREMENTS are false\n"
		"\t%5d are available to service your request\n",

		return_buff, cluster, proc, totalMachines,
		fReqConstraint,
		fOffConstraint,
		fPreemptPrioCond, niceUser ? "(*)" : "",
		fRankCond,
		fPreemptReqTest,
		available );

	int last_match_time=0, last_rej_match_time=0;
	request->LookupInteger(ATTR_LAST_MATCH_TIME, last_match_time);
	request->LookupInteger(ATTR_LAST_REJ_MATCH_TIME, last_rej_match_time);
	if (last_match_time) {
		time_t t = (time_t)last_match_time;
		sprintf( return_buff, "%s\tLast successful match: %s",
				 return_buff, ctime(&t) );
	} else if (last_rej_match_time) {
		strcat( return_buff, "\tNo successful match recorded.\n" );
	}
	if (last_rej_match_time > last_match_time) {
		time_t t = (time_t)last_rej_match_time;
		sprintf( return_buff, "%s\tLast failed match: %s",
				 return_buff, ctime(&t) );
		buffer[0] = '\0';
		request->LookupString(ATTR_LAST_REJ_MATCH_REASON, buffer);
		if (buffer[0]) {
			sprintf( return_buff, "%s\tReason for last match failure: %s\n",
					 return_buff, buffer );
		}
	}

	if( niceUser ) {
		sprintf( return_buff, 
				 "%s\n\t(*)  Since this is a \"nice-user\" request, this request "
				 "has a\n\t     very low priority and is unlikely to preempt other "
				 "requests.\n", return_buff );
	}
			

	if( fReqConstraint == totalMachines ) {
		char reqs[2048];
		ExprTree *reqExp;
		strcat( return_buff, "\nWARNING:  Be advised:\n");
		strcat( return_buff, "   No resources matched request's constraints\n");
		sprintf( return_buff, "%s   Check the %s expression below:\n\n" , 
			return_buff, ATTR_REQUIREMENTS );
		if( !(reqExp = request->Lookup( ATTR_REQUIREMENTS) ) ) {
			sprintf( return_buff, "%s   ERROR:  No %s expression found" ,
				return_buff, ATTR_REQUIREMENTS );
		} else {
			reqs[0] = '\0';
			reqExp->PrintToStr( reqs );
			sprintf( return_buff, "%s%s\n\n", return_buff, reqs );
		}
	}

	if( fOffConstraint == totalMachines ) {
		sprintf( return_buff, "%s\nWARNING:  Be advised:", return_buff );
		sprintf( return_buff, "%s   Request %d.%d did not match any "
			"resource's constraints\n\n", return_buff, cluster, proc);
	}

	//printf("%s",return_buff);
	return return_buff;
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

	if( !initialized ) {
		char *tmp = param( "UID_DOMAIN" );
		if( !tmp ) {
			fprintf( stderr, "Error:  UID_DOMAIN not found in config file\n" );
			exit( 1 );
		}
		strncpy( uid_domain , tmp , 63);
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
