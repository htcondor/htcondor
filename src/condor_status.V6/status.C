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
#include "condor_state.h"
#include "condor_api.h"
#include "status_types.h"
#include "totals.h"
#include "get_daemon_addr.h"
#include "get_full_hostname.h"
#include "extArray.h"
#include "sig_install.h"

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
ExtArray<ExprTree*> sortLessThanExprs( 4 );
ExtArray<ExprTree*> sortEqualExprs( 4 );

// instantiate templates
template class ExtArray<ExprTree*>;

// function declarations
void usage 		();
void firstPass  (int, char *[]);
void secondPass (int, char *[]);
void prettyPrint(ClassAdList &, TrackTotals *);
int  matchPrefix(const char *, const char *);
int  lessThanFunc(ClassAd*,ClassAd*,void*);
int  customLessThanFunc(ClassAd*,ClassAd*,void*);

extern "C" int SetSyscalls (int) {return 0;}
extern	void setPPstyle (ppOption, int, char *);
extern	void setType    (char *, int, char *);
extern	void setMode 	(Mode, int, char *);

int 
main (int argc, char *argv[])
{
#if !defined(WIN32)
	install_sig_handler(SIGPIPE, (SIG_HANDLER)SIG_IGN );
#endif

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

	  case COLLECTOR_AD:
		setPPstyle(PP_COLLECTOR_NORMAL, 0, DEFAULT);
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
	  case MODE_COLLECTOR_NORMAL:
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

	// sort the ad 
	if( sortLessThanExprs.getlast() > -1 ) {
		result.Sort((SortFunctionType) customLessThanFunc );
	} else {
		result.Sort ((SortFunctionType)lessThanFunc);
	}

	// output result
	prettyPrint (result, &totals);
	
	// be nice ...
	{
		int last = sortLessThanExprs.getlast();
		delete query;
		for( int i = 0 ; i <= last ; i++ ) {
			if( sortLessThanExprs[i] ) delete sortLessThanExprs[i];
			if( sortEqualExprs[i] ) delete sortEqualExprs[i];
		}
	}

	return 0;
}


void
usage ()
{
	fprintf (stderr,"Usage: %s [help-opt] [query-opt] [display-opt] "
		"[custom-opts ...] [hostname ...]\n"
		"    where [help-opt] is one of\n"
		"\t-help\t\t\tThis screen\n"
		"\t-diagnose\t\tPrint out query ad without performing query\n"
		"    and [query-opt] is one of\n"
		"\t-avail\t\t\tPrint information about available resources\n"
		"\t-ckptsrvr\t\tDisplay checkpoint server attributes\n"
		"\t-claimed\t\tPrint information about claimed resources\n"
//		"\t-collector\t\tSame as -world\n"
		"\t-master\t\t\tDisplay daemon master attributes\n"
		"\t-pool <name>\t\tGet information from collector <name>\n"
		"\t-run\t\t\tSame as -claimed [deprecated]\n"
		"\t-schedd\t\t\tDisplay attributes of schedds\n"
		"\t-license\t\t\tDisplay attributes of licenses\n"
		"\t-server\t\t\tDisplay important attributes of resources\n"
		"\t-startd\t\t\tDisplay resource attributes\n"
		"\t-state\t\t\tDisplay state of resources\n"
		"\t-submitters\t\tDisplay information about request submitters\n"
//		"\t-world\t\t\tDisplay all pools reporting to UW collector\n"
		"    and [display-opt] is one of\n"
		"\t-long\t\t\tDisplay entire classads\n"
		"\t-sort <attr>\t\tSort entries by named attribute\n"
		"\t-total\t\t\tDisplay totals only\n"
		"\t-verbose\t\tSame as -long\n" 
		"    and [custom-opts ...] are one or more of\n"
		"\t-constraint <const>\tAdd constraint on classads\n"
		"\t-format <fmt> <attr>\tRegister display format and attribute\n",
		myName);
}

void
firstPass (int argc, char *argv[])
{
	int had_pool_error = 0;
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
			if( pool ) {
				free( pool );
				had_pool_error = 1;
			}
			pool = get_full_hostname( (const char *)argv[++i]);
			if( !pool ) {
				fprintf( stderr, "Error:  unknown host %s\n", argv[i] );
				exit( 1 );
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
		if (matchPrefix (argv[i], "-license")) {
			setMode (MODE_LICENSE_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-sort")) {
			char	exprString[1024];
			ExprTree	*sortExpr;
			exprString[0] = '\0';
			sprintf( exprString, "MY.%s < TARGET.%s", argv[i+1], argv[i+1] );
			if( Parse( exprString, sortExpr ) ) {
				fprintf( stderr, "Error:  Parse error of: %s\n", exprString );
				exit( 1 );
			}
			sortLessThanExprs[sortLessThanExprs.getlast()+1] = sortExpr;
			sprintf( exprString, "MY.%s == TARGET.%s", argv[i+1], argv[i+1] );
			if( Parse( exprString, sortExpr ) ) {
				fprintf( stderr, "Error:  Parse error of: %s\n", exprString );
				exit( 1 );
			}
			sortEqualExprs[sortEqualExprs.getlast()+1] = sortExpr;
			i++;
		} else
		if (matchPrefix (argv[i], "-submitters")) {
			setMode (MODE_SCHEDD_SUBMITTORS, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-master")) {
			setMode (MODE_MASTER_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-collector")) {
			setMode (MODE_COLLECTOR_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-world")) {
			setMode (MODE_COLLECTOR_NORMAL, i, argv[i]);
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
	if( had_pool_error ) {
		fprintf( stderr, 
				 "Warning:  Multiple -pool arguments given, using \"%s\"\n",
				 pool );
	}
}


void 
secondPass (int argc, char *argv[])
{
	char *daemonname;
	for (int i = 1; i < argc; i++) {
		// omit parameters which qualify switches
		if (matchPrefix (argv[i], "-pool") || matchPrefix( argv[i], "-sort")) {
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
			  case MODE_COLLECTOR_NORMAL:
			  case MODE_CKPT_SRVR_NORMAL:
    		  case MODE_STARTD_AVAIL:
				sprintf (buffer, "(TARGET.%s == \"%s\") || (TARGET.%s == \"%s\")", 
						 ATTR_NAME, daemonname, ATTR_MACHINE, daemonname );
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
			delete [] daemonname;
			daemonname = NULL;
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
	char 	buf1[128];
	char	buf2[128];
	int		val;

	if( !ad1->LookupString(ATTR_OPSYS, buf1) ||
		!ad2->LookupString(ATTR_OPSYS, buf2) ) {
		buf1[0] = '\0';
		buf2[0] = '\0';
	}
	val = strcmp( buf1, buf2 );
	if( val ) { 
		return (val < 0);
	} 

	if( !ad1->LookupString(ATTR_ARCH, buf1) ||
		!ad2->LookupString(ATTR_ARCH, buf2) ) {
		buf1[0] = '\0';
		buf2[0] = '\0';
	}
	val = strcmp( buf1, buf2 );
	if( val ) { 
		return (val < 0);
	} 

	if( !ad1->LookupString(ATTR_MACHINE, buf1) ||
		!ad2->LookupString(ATTR_MACHINE, buf2) ) {
		buf1[0] = '\0';
		buf2[0] = '\0';
	}
	val = strcmp( buf1, buf2 );
	if( val ) { 
		return (val < 0);
	} 

	if (!ad1->LookupString(ATTR_NAME, buf1) ||
		!ad2->LookupString(ATTR_NAME, buf2))
		return 0;
	return ( strcmp( buf1, buf2 ) < 0 );
}

int
customLessThanFunc( ClassAd *ad1, ClassAd *ad2, void *)
{
	EvalResult 	result;
	int			last = sortLessThanExprs.getlast();

	for( int i = 0 ; i <= last ; i++ ) {
		sortLessThanExprs[i]->EvalTree( ad1, ad2, &result );
		if( result.type == LX_INTEGER ) {
			if( result.i ) {
				return 1;
			} else {
				sortEqualExprs[i]->EvalTree( ad1, ad2, &result );
				if( result.type != LX_INTEGER || !result.i )
					return 0;
			}
		} else {
			return 0;
		}
	}

	return 0;
}
