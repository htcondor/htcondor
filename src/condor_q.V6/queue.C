#include <iostream.h>
#include "condor_common.h"
#include "_condor_fix_resource.h"
#include "condor_config.h"
#include "condor_q.h"
#include "condor_attributes.h"
#include "files.h"

extern "C" int SetSyscalls(int val){return val;}
void short_print (int, int, const char*, int, int, int, int, int, const char *);
extern "C" void set_debug_flags( char * );
extern "C" char *get_schedd_addr(const char *);

static void short_header (void);
static void usage (char *);
static void displayJobShort (ClassAd *);
static void shorten (char *, int);

static int verbose = 0, summarize = 1;
static int malformed, unexpanded, running, idle;

extern	"C" BUCKET*		ConfigTab[];
extern "C" extern int Termlog;

int main (int argc, char **argv)
{
	CondorQ 		  Q;
	ClassAdList 	  jobs; 
	ClassAd           *job;
	int               i;
	int               cluster, proc;
	char              constraint[1024];
	char              *host = 0;
	char*				scheddAddr;
	
	for (i = 1; i < argc; i++)
	{
		if (strcmp (argv[i], "-l") == 0)
		{
			verbose = 1;
			summarize = 0;
		}
		else
		if (strcmp (argv[i], "-D") == 0)
		{
			Termlog = 1;
			set_debug_flags( argv[++i] );
		}
		else
		if (strcmp (argv[i], "-h") == 0)
		{
			host = argv[++i];
		}
		else
		if (strcmp (argv[i], "-C") == 0)
		{
			Q.add (argv[++i]);
			summarize = 0;
		}
		else
		if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2)
		{
			sprintf (constraint, "((%s == %d) && (%s == %d))", 
									ATTR_CLUSTER_ID, cluster,
									ATTR_PROC_ID, proc);
			Q.add (constraint);
			summarize = 0;
		}
		else
		if (sscanf (argv[i], "%d", &cluster) == 1)
		{
			sprintf (constraint, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
			Q.add (constraint);
			summarize = 0;
		}
		else
		{
			usage (argv[0]);
			exit (1);
		}
	}
	
	config( 0 );

	// find ip port of schedd from collector
	if(!host)
	{
		host = new char[256];
		if(gethostname(host, 256) < 0)
		{
			printf("Can't find host\n");
			exit(1);
		}
	}
	if((scheddAddr = get_schedd_addr(host)) == NULL)
	{
		printf("Can't find schedd address on %s\n", host);
		exit(1);
	}
	
	// fetch queue from schedd	
	if (Q.fetchQueueFromHost (jobs, scheddAddr) != Q_OK)
	{
		printf ("Error connecting to job queue\n");
		exit (1);
	}
	
	// initialize counters
	malformed = 0; idle = 0; running = 0; unexpanded = 0;

	if (verbose)
	{
		jobs.fPrintAttrListList (stdout);
	}
	else
	{
		short_header ();
		jobs.Open ();
		while (job = jobs.Next())
		{
			displayJobShort (job);
		}
		jobs.Close ();
	}

	if (summarize)
	{
		printf ("\n%d jobs; %d unexpanded, %d idle, %d running, %d malformed\n",
				unexpanded+idle+running+malformed, unexpanded, idle, running, 
				malformed);
	}

	return 0;
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
		!ad->EvalFloat   (ATTR_JOB_REMOTE_USER_CPU, NULL, utime)	||
		!ad->EvalInteger (ATTR_JOB_STATUS, NULL, status)			||
		!ad->EvalInteger (ATTR_JOB_PRIO, NULL, prio)				||
		!ad->EvalInteger (ATTR_IMAGE_SIZE, NULL, image_size)		||
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
	printf ("usage: %s [-h <host>] [-l] [<constraint> ...]\n", myName);
	printf ("\twhere <host> is one of:\n");
	printf ("\t\thostname\n\t\t<hostname:port>\n\t\t<xx.xx.xx.xx:port>\n");
	printf ("\tand a <constraint> is one of:\n");
	printf ("\t\tcluster\n\t\tcluster.proc\n\t\t-C <ClassAd expression>\n");
}
