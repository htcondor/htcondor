#include "condor_common.h"
#include "condor_config.h"
#include "condor_state.h"
#include "condor_api.h"
#include "status_types.h"
#include "totals.h"
#include "get_daemon_addr.h"

// global variables
AttrListPrintMask pm;
char		*DEFAULT= "<default>";
char 		*pool 	= NULL;
AdTypes		type 	= (AdTypes) -1;
ppOption	ppStyle	= PP_NOTSET;
int			wantOnlyTotals 	= 0;
int			summarySize = -1;
Mode		mode	= MODE_NOTSET;
int			diagnose = 0;
CondorQuery *query;
char		buffer[1024];
ClassAdList result;
char		*myName;

// function declarations
void usage 		();
void firstPass  (int, char *[]);
void secondPass (int, char *[]);
void prettyPrint(ClassAdList &, TrackTotals *);
int  matchPrefix(const char *, const char *);
int  lessThanFunc(ClassAd*,ClassAd*,void*);

extern "C" int SetSyscalls (int) {return 0;}
extern	void setPPstyle (ppOption, int, char *);
extern	void setType    (char *, int, char *);
extern	void setMode 	(Mode, int, char *);

int 
main (int argc, char *argv[])
{
	// initialize to read from config file
	myName = argv[0];
	config ((ClassAd*)NULL);

	// The arguments take two passes to process --- the first pass
	// figures out the mode, after which we can instantiate the required
	// query object.  We add implied constraints from the command line in
	// the second pass.
	firstPass (argc, argv);
	
	// if the mode has not been set, it is STARTD_NORMAL
	if (mode == MODE_NOTSET) {
		setMode (MODE_STARTD_NORMAL, 0, DEFAULT);
	}

	// instantiate query object
	if (!(query = new CondorQuery (type))) {
		fprintf (stderr, "Error:  Out of memory\n");
		exit (1);
	}

	// set pretty print style implied by the type of entity being queried
	// but do it with default priority, so that explicitly requested options
	// can override it
	switch (type)
	{
	  case STARTD_AD:
		setPPstyle(PP_STARTD_NORMAL, 0, DEFAULT);
		break;

	  case SCHEDD_AD:
		setPPstyle(PP_SCHEDD_NORMAL, 0, DEFAULT);
		break;

	  case MASTER_AD:
		setPPstyle(PP_MASTER_NORMAL, 0, DEFAULT);
		break;

	  case CKPT_SRVR_AD:
		setPPstyle(PP_CKPT_SRVR_NORMAL, 0, DEFAULT);
		break;

	  default:
		setPPstyle(PP_VERBOSE, 0, DEFAULT);
	}

	// set the constraints implied by the mode
	switch (mode) {
	  case MODE_STARTD_NORMAL:
	  case MODE_MASTER_NORMAL:
	  case MODE_CKPT_SRVR_NORMAL:
	  case MODE_SCHEDD_NORMAL: 
	  case MODE_SCHEDD_SUBMITTORS:
		break;


	  case MODE_STARTD_AVAIL:
			  // For now, -avail shows you machines avail to anyone.
		sprintf (buffer, "TARGET.%s == \"%s\"", ATTR_STATE, 
					state_to_string(unclaimed_state));
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addConstraint (buffer);
		break;


	  case MODE_STARTD_RUN:
		sprintf (buffer, "TARGET.%s == \"%s\"", ATTR_STATE, 
					state_to_string(claimed_state));
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addConstraint (buffer);
		break;


	  default:
		break;
	}	
									
	// second pass:  add regular parameters and constraints
	if (diagnose) {
		printf ("----------\n");
	}

	secondPass (argc, argv);

	// initialize the totals object
	TrackTotals	totals(ppStyle);

	// fetch the query
	QueryResult q;

	// if diagnose was requested, just print the query ad
	if (diagnose) {
		ClassAd 	queryAd;

		// print diagnostic information about inferred internal state
		setMode ((Mode) 0, 0, NULL);
		setType (NULL, 0, NULL);
		setPPstyle ((ppOption) 0, 0, DEFAULT);
		printf ("----------\n");

		q = query->getQueryAd (queryAd);
		queryAd.fPrint (stdout);

		printf ("----------\n");
		fprintf (stderr, "Result of making query ad was:  %d\n", q);
		exit (1);
	}

	if ((q = query->fetchAds (result, pool)) != Q_OK) {
		fprintf (stderr, "Error:  Could not fetch ads --- error %s\n", 
					getStrQueryResult(q));
		exit (1);
	}

	// sort the ad by ATTR_NAME
	result.Sort ((SortFunctionType)lessThanFunc);

	// output result
	prettyPrint (result, &totals);
	
	// be nice ...
	delete query;

	return 0;
}


void
usage ()
{
	fprintf (stderr,"Usage: %s [options]\n"
		"    where [options] are zero or more of\n"
		"\t[-avail]\t\tPrint information about available resources\n"
		"\t[-ckptsrvr]\t\tDisplay checkpoint server attributes\n"
		"\t[-claimed]\t\tPrint information about claimed resources\n"
		"\t[-constraint <const>]\tAdd constraint on classads\n"
		"\t[-diagnose]\t\tPrint out query ad without performing query\n"
		"\t[-format <fmt> <attr>]\tRegister display format and attribute\n"
		"\t[-help]\t\t\tThis screen\n"
		"\t[-long]\t\t\tDisplay entire classads\n"
		"\t[-master]\t\tDisplay daemon master attributes\n"
		"\t[-pool <name>]\t\tGet information from collector <name>\n"
		"\t[-run]\t\t\tSame as -claimed [deprecated]\n"
		"\t[-schedd]\t\tDisplay attributes of schedds\n"
		"\t[-server]\t\tDisplay important attributes of resources\n"
		"\t[-startd]\t\tDisplay resource attributes\n"
		"\t[-submittors]\tDisplay information about request submittors\n"
		"\t[-total]\t\tDisplay totals only\n"
		"\t[-verbose]\t\tSame as -long\n", 
		myName);
}

void
firstPass (int argc, char *argv[])
{
	// Process arguments:  there are dependencies between them
	// o -l/v and -serv are mutually exclusive
	// o -sub, -avail and -run are mutually exclusive
	// o -pool and -entity may be used at most once
	// o since -c can be processed only after the query has been instantiated,
	//   constraints are added on the second pass
	for (int i = 1; i < argc; i++) {
		if (matchPrefix (argv[i], "-avail")) {
			setMode (MODE_STARTD_AVAIL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-pool")) {
			if (pool == NULL) {
				pool = argv[++i];
			} else {
				fprintf (stderr, "At most one -pool argument may be used\n");
				exit (1);
			}
		} else
		if (matchPrefix (argv[i], "-format")) {
			setPPstyle (PP_CUSTOM, i, argv[i]);
			i += 2;
		} else
		if (matchPrefix (argv[i], "-constraint")) {
			// can add constraints on second pass only
			i++;
		} else
		if (matchPrefix (argv[i], "-diagnose")) {
			diagnose = 1;
		} else
		if (matchPrefix (argv[i], "-help")) {
			usage ();
			exit (1);
		} else
		if (matchPrefix (argv[i],"-long") || matchPrefix (argv[i],"-verbose")) {
			setPPstyle (PP_VERBOSE, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-run") || matchPrefix(argv[i], "-claimed")) {
			setMode (MODE_STARTD_RUN, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-server")) {
			setPPstyle (PP_STARTD_SERVER, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-state")) {
			setPPstyle (PP_STARTD_STATE, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-startd")) {
			setMode (MODE_STARTD_NORMAL,i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-schedd")) {
			setMode (MODE_SCHEDD_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-submittors")) {
			setMode (MODE_SCHEDD_SUBMITTORS, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-master")) {
			setMode (MODE_MASTER_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-ckptsrvr")) {
			setMode (MODE_CKPT_SRVR_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-total")) {
			wantOnlyTotals = 1;
		} else
		if (*argv[i] == '-') {
			fprintf (stderr, "Error:  Unknown option %s\n", argv[i]);
			usage ();
			exit (1);
		}
	}
}


void 
secondPass (int argc, char *argv[])
{
	char *daemonname;
	for (int i = 1; i < argc; i++) {
		// omit parameters which qualify switches
		if (matchPrefix (argv[i], "-pool")) {
			i++;
			continue;
		}
		if (matchPrefix (argv[i], "-format")) {
			pm.registerFormat (argv[i+1], argv[i+2]);
			if (diagnose) {
				printf ("Arg %d --- register format [%s] for [%s]\n", 
						i, argv[i+1], argv[i+2]);
			}
			i += 2;
			continue;
		}


		// figure out what the other parameters should do
		if (*argv[i] != '-') {
			// display extra information for diagnosis
			if (diagnose) {
				printf ("Arg %d (%s) --- adding constraint", i, argv[i]);
			}

			if( !(daemonname = get_daemon_name(argv[i])) ) {
				fprintf( stderr, "%s: unknown host %s\n",
						 argv[0], get_host_part(argv[i]) );
				exit(1);
			}

			switch (mode) {
			  case MODE_STARTD_NORMAL:
			  case MODE_SCHEDD_NORMAL:
			  case MODE_SCHEDD_SUBMITTORS:
			  case MODE_MASTER_NORMAL:
			  case MODE_CKPT_SRVR_NORMAL:
    		  case MODE_STARTD_AVAIL:
				sprintf (buffer, "TARGET.%s == \"%s\"", ATTR_NAME, daemonname);
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addConstraint (buffer);
				break;

    		  case MODE_STARTD_RUN:
				sprintf (buffer,"TARGET.%s == \"%s\"",ATTR_REMOTE_USER,argv[i]);
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addConstraint (buffer);
				break;

			  default:
				fprintf(stderr,"Error: Don't know how to process %s\n",argv[i]);
			}
		} else 
		if (matchPrefix (argv[i], "-constraint")) {
			if (diagnose) {
				printf ("[%s]\n", argv[i+1]);
			}
			query->addConstraint (argv[i+1]);
			i++;
		}
	}
}


int
matchPrefix (const char *s1, const char *s2)
{
	int lenS1 = strlen (s1);
	int lenS2 = strlen (s2);
	int len = (lenS1 < lenS2) ? lenS1 : lenS2;

	return (strncmp (s1, s2, len) == 0);
}


int
lessThanFunc(ClassAd *ad1, ClassAd *ad2, void *)
{
	char 	name1[64];
	char	name2[64];

	if (!ad1->LookupString(ATTR_NAME, name1) ||
		!ad2->LookupString(ATTR_NAME, name2))
			return 0;

	return (strcmp (name1, name2) < 0);
}
