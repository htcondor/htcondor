#include "condor_common.h"
#include "condor_config.h"
#include "condor_api.h"
#include "status_types.h"
#include "totals.h"

// global variables
AttrListPrintMask pm;
char 		*pool 	= NULL;
AdTypes		type 	= (AdTypes) -1;
ppOption	ppStyle	= PP_NOTSET;
int			wantTotals 	= 0;
int			summarySize = -1;
Mode		mode	= MODE_NOTSET;
int			diagnose = 0;
CondorQuery *query;
char		buffer[1024];
ClassAdList result;
Totals		totals;
char		*myName;

// function declarations
void usage 		();
void firstPass  (int, char *[]);
void secondPass (int, char *[]);
void prettyPrint(ClassAdList &);
int  matchPrefix(char *, char *);

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
	
	// if the mode has not been set, it is NORMAL
	if (mode == MODE_NOTSET) {
		setMode (MODE_NORMAL, 0, "<default>");
	}

	// instantiate query object
	if (!(query = new CondorQuery (type))) {
		fprintf (stderr, "Error:  Out of memory\n");
		exit (1);
	}

	// set the constraint implied by the mode
	switch (mode) {
	  case MODE_NORMAL:
	  case MODE_CUSTOM:
		break;

	  case MODE_AVAIL:
		// **** Remeber to change to evaluating Requirements ****
		if (diagnose) {
			printf ("Adding constraint [%s]\n", "TARGET.START == True");
		}
		query->addConstraint ("TARGET.START == True");
		break;

	  case MODE_SUBMITTORS: 
		sprintf (buffer,"TARGET.%s > 0 || TARGET.%s > 0", ATTR_IDLE_JOBS,
			ATTR_RUNNING_JOBS);
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addConstraint (buffer);
		break;

	  case MODE_RUN:
		sprintf (buffer, "TARGET.%s != \"NoJob\"", ATTR_STATE);
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addConstraint (buffer);
		break;
	}	
									
	// second pass:  add regular parameters and constraints
	if (diagnose) {
		printf ("----------\n");
	}

	secondPass (argc, argv);

	// initialize the totals object
	if (wantTotals && summarySize == -1) {
		fprintf (stderr,"Error:  Want totals, but no total support for mode\n");
		exit (1);
	}
	totals.setSummarySize (summarySize);

	// fetch the query
	QueryResult q;

	// if diagnose was requested, just print the query ad
	if (diagnose) {
		ClassAd 	queryAd;

		// print diagnostic information about inferred internal state
		setMode ((Mode) 0, 0, NULL);
		setType (NULL, 0, NULL);
		setPPstyle ((ppOption) 0, 0, NULL);
		printf ("----------\n");

		q = query->getQueryAd (queryAd);
		queryAd.fPrint (stdout);

		printf ("----------\n");
		fprintf (stderr, "Result of making query ad was:  %d\n", q);
		exit (1);
	}

	if ((q = query->fetchAds (result, pool)) != Q_OK) {
		fprintf (stderr, "Error:  Could not fetch ads --- error %d\n", q);
		exit (1);
	}

	// output result
	prettyPrint (result);
	
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
		"\t[-constraint <const>]\tAdd constraint on classads\n"
		"\t[-diagnose]\t\tPrint out query ad without performing query\n"
		"\t[-entity <type>]\tForce query on specified entity\n"
		"\t[-format <fmt> <attr>]\tRegister display format and attribute\n"
		"\t[-help]\t\t\tThis screen\n"
		"\t[-long]\t\t\tDisplay entire classads\n"
		"\t[-pool <poolname>]\tGet information from <poolname>\n"
		"\t[-run]\t\t\tPrint information about employed resources\n"
		"\t[-server]\t\tDisplay important attributes of resources\n"
		"\t[-submittors]\t\tObtain information about submittors\n"
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
			setMode (MODE_AVAIL, i, argv[i]);
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
			pm.registerFormat (argv[i+1], argv[i+2]);
			if (diagnose) {
				printf ("Arg %d --- register format [%s] for [%s]\n", 
						i, argv[i+1], argv[i+2]);
			}
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
		if (matchPrefix (argv[i], "-run")) {
			setMode (MODE_RUN, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-server")) {
			setPPstyle (PP_SERVER, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-submittors")) {
			setMode (MODE_SUBMITTORS, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-total")) {
			wantTotals = 1;
		} else
		if (matchPrefix (argv[i], "-entity")) {
			setMode (MODE_CUSTOM, i, argv[i]);
			setType (argv[i+1], i, argv[i]);
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
	for (int i = 1; i < argc; i++) {
		// omit parameters which qualify switches
		if (matchPrefix (argv[i], "-pool")) {
			i++;
			continue;
		}
		if (matchPrefix (argv[i], "-entity")) {
			i++;
			continue;
		} 
		if (matchPrefix (argv[i], "-format")) {
			i += 2;
			continue;
		}


		// figure out what the other parameters should do
		if (*argv[i] != '-') {
			// display extra information for diagnosis
			if (diagnose) {
				printf ("Arg %d (%s) --- adding constraint", i, argv[i]);
			}

			switch (mode) {
			  case MODE_NORMAL:
    		  case MODE_AVAIL:
    		  case MODE_SUBMITTORS:
				sprintf (buffer, "TARGET.%s == \"%s\"", ATTR_NAME, argv[i]);
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addConstraint (buffer);
				break;

    		  case MODE_RUN:
				sprintf (buffer,"TARGET.%s == \"%s\"",ATTR_REMOTE_USER,argv[i]);
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addConstraint (buffer);
				break;

    		  case MODE_CUSTOM:
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addConstraint (argv[i]);
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
matchPrefix (char *s1, char *s2)
{
	int lenS1 = strlen (s1);
	int lenS2 = strlen (s2);
	int len = (lenS1 < lenS2) ? lenS1 : lenS2;

	return (strncmp (s1, s2, len) == 0);
}
