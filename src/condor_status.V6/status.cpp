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
#include "condor_state.h"
#include "condor_api.h"
#include "status_types.h"
#include "totals.h"
#include "get_daemon_name.h"
#include "daemon.h"
#include "dc_collector.h"
#include "extArray.h"
#include "sig_install.h"
#include "string_list.h"
#include "condor_string.h"   // for strnewp()
#include "print_wrapped_text.h"
#include "error_utils.h"
#include "condor_distribution.h"
#include "condor_version.h"

// global variables
AttrListPrintMask pm;
char		*DEFAULT= "<default>";
DCCollector* pool = NULL;
AdTypes		type 	= (AdTypes) -1;
ppOption	ppStyle	= PP_NOTSET;
int			wantOnlyTotals 	= 0;
int			summarySize = -1;
bool        expert = false;
Mode		mode	= MODE_NOTSET;
int			diagnose = 0;
char*		direct = NULL;
char*		genericType = NULL;
CondorQuery *query;
char		buffer[1024];
ClassAdList result;
char		*myName;
StringList	*sortConstraints = NULL;
ExtArray<ExprTree*> sortLessThanExprs( 4 );
ExtArray<ExprTree*> sortEqualExprs( 4 );
bool            javaMode = false;
bool			vmMode = false;
char 		*target = NULL;
ClassAd		*targetAd = NULL;
ArgList projList;		// Attributes that we want the server to send us

// instantiate templates

// function declarations
void usage 		();
void firstPass  (int, char *[]);
void secondPass (int, char *[]);
void prettyPrint(ClassAdList &, TrackTotals *);
int  matchPrefix(const char *, const char *, int min_len);
int  lessThanFunc(AttrList*,AttrList*,void*);
int  customLessThanFunc(AttrList*,AttrList*,void*);

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
	myDistro->Init( argc, argv );
	myName = argv[0];
	config();

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
#ifdef WANT_QUILL
	  case QUILL_AD:
		setPPstyle(PP_QUILL_NORMAL, 0, DEFAULT);
		break;
#endif /* WANT_QUILL */

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

	  case STORAGE_AD:
		setPPstyle(PP_STORAGE_NORMAL, 0, DEFAULT);
		break;

	  case NEGOTIATOR_AD:
		setPPstyle(PP_NEGOTIATOR_NORMAL, 0, DEFAULT);
		break;

      case GRID_AD:
        setPPstyle(PP_GRID_NORMAL, 0, DEFAULT);

	  case GENERIC_AD:
		setPPstyle(PP_GENERIC, 0, DEFAULT);
		break;

	  case ANY_AD:
		setPPstyle(PP_ANY_NORMAL, 0, DEFAULT);
		break;

	  default:
		setPPstyle(PP_VERBOSE, 0, DEFAULT);
	}

	// set the constraints implied by the mode
	switch (mode) {
#ifdef WANT_QUILL
	  case MODE_QUILL_NORMAL:
#endif /* WANT_QUILL */

	  case MODE_STARTD_NORMAL:
	  case MODE_MASTER_NORMAL:
	  case MODE_CKPT_SRVR_NORMAL:
	  case MODE_SCHEDD_NORMAL:
	  case MODE_SCHEDD_SUBMITTORS:
	  case MODE_COLLECTOR_NORMAL:
	  case MODE_NEGOTIATOR_NORMAL:
	  case MODE_STORAGE_NORMAL:
	  case MODE_GENERIC_NORMAL:
	  case MODE_ANY_NORMAL:
	  case MODE_GRID_NORMAL:
	  case MODE_HAD_NORMAL:
		break;

	  case MODE_OTHER:
			// tell the query object what the type we're querying is
		query->setGenericQueryType(genericType);
		if(genericType) {
			sprintf( buffer, "TARGET.%s == \"%s\"", ATTR_TARGET_TYPE, genericType);
			if (diagnose) {
				printf ("Adding constraint [%s]\n", buffer);
			}
			query->addANDConstraint (buffer);
		}
		free(genericType);
		genericType = NULL;
		break;

	  case MODE_STARTD_AVAIL:
			  // For now, -avail shows you machines avail to anyone.
		sprintf (buffer, "TARGET.%s == \"%s\"", ATTR_STATE,
					state_to_string(unclaimed_state));
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addORConstraint (buffer);
		break;


	  case MODE_STARTD_RUN:
		sprintf (buffer, "TARGET.%s == \"%s\"", ATTR_STATE,
					state_to_string(claimed_state));
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addORConstraint (buffer);
		break;

	  case MODE_STARTD_COD:
	    sprintf (buffer, "TARGET.%s > 0", ATTR_NUM_COD_CLAIMS );
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addORConstraint (buffer);
		break;

	  default:
		break;
	}	

	if(javaMode) {
		sprintf( buffer, "TARGET.%s == TRUE", ATTR_HAS_JAVA );
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addANDConstraint (buffer);
		
		projList.AppendArg(ATTR_HAS_JAVA);
		projList.AppendArg(ATTR_JAVA_MFLOPS);
		projList.AppendArg(ATTR_JAVA_VENDOR);
		projList.AppendArg(ATTR_JAVA_VERSION);

	}

	if(vmMode) {
		sprintf( buffer, "TARGET.%s == TRUE", ATTR_HAS_VM);
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addANDConstraint (buffer);

		projList.AppendArg(ATTR_VM_TYPE);
		projList.AppendArg(ATTR_VM_MEMORY);
		projList.AppendArg(ATTR_VM_NETWORKING);
		projList.AppendArg(ATTR_VM_NETWORKING_TYPES);
		projList.AppendArg(ATTR_VM_HARDWARE_VT);
		projList.AppendArg(ATTR_VM_AVAIL_NUM);
		projList.AppendArg(ATTR_VM_ALL_GUEST_MACS);
		projList.AppendArg(ATTR_VM_ALL_GUEST_IPS);
		projList.AppendArg(ATTR_VM_GUEST_MAC);
		projList.AppendArg(ATTR_VM_GUEST_IP);

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

	if ((mode == MODE_STARTD_NORMAL) && (ppStyle == PP_STARTD_NORMAL)) {
		projList.AppendArg("Name");
		projList.AppendArg("Opsys");
		projList.AppendArg("Arch");
		projList.AppendArg("State");
		projList.AppendArg("Activity");
		projList.AppendArg("LoadAvg");
		projList.AppendArg("Memory");
		projList.AppendArg("ActvtyTime");
		projList.AppendArg("MyCurrentTime");
		projList.AppendArg("EnteredCurrentActivity");
	}

	// Calculate the projected arguments, and insert into
	// the projection query attribute

	MyString quotedProjStr;
	MyString projStrError;

	projList.GetArgsStringV2Quoted(&quotedProjStr, &projStrError);

	MyString projStr("projection = ");
	projStr += quotedProjStr;
		// If it is empty, it's just quotes
	if (quotedProjStr.Length() > 2) {
		query->addExtraAttribute(projStr.Value());
	}

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

	char* addr = pool ? pool->addr() : NULL;
	if( direct ) {
		Daemon *d = NULL;
		switch( mode ) {
		case MODE_MASTER_NORMAL:
			d = new Daemon( DT_MASTER, direct, addr );
			break;
		case MODE_STARTD_NORMAL:
		case MODE_STARTD_AVAIL:
		case MODE_STARTD_RUN:
		case MODE_STARTD_COD:
			d = new Daemon( DT_STARTD, direct, addr );
			break;

#ifdef WANT_QUILL
		case MODE_QUILL_NORMAL:
			d = new Daemon( DT_QUILL, direct, addr );
			break;
#endif /* WANT_QUILL */

		case MODE_SCHEDD_NORMAL:
		case MODE_SCHEDD_SUBMITTORS:
			d = new Daemon( DT_SCHEDD, direct, addr );
			break;
		case MODE_NEGOTIATOR_NORMAL:
			d = new Daemon( DT_NEGOTIATOR, direct, addr );
			break;
		case MODE_CKPT_SRVR_NORMAL:
		case MODE_COLLECTOR_NORMAL:
		case MODE_LICENSE_NORMAL:
		case MODE_STORAGE_NORMAL:
		case MODE_GENERIC_NORMAL:
		case MODE_ANY_NORMAL:
		case MODE_OTHER:
		case MODE_GRID_NORMAL:
		case MODE_HAD_NORMAL:
				// These have to go to the collector, anyway.
			break;
		default:
            fprintf( stderr, "Error:  Illegal mode %d\n", mode );
			exit( 1 );
			break;
		}
		if( d ) {
			if( d->locate() ) {
				addr = d->addr();
			} else {
				fprintf( stderr, "%s: %s\n", myName, d->error() );
				exit( 1 );
			}
		}
	}

	CondorError errstack;	
	if (addr) {
		q = query->fetchAds (result, addr, &errstack);
	} else {
		CollectorList * collectors = CollectorList::create();
		q = collectors->query (*query, result, &errstack);
		delete collectors;
	}
		

	if (q != Q_OK) {
		if (q == Q_COMMUNICATION_ERROR) {
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
				// if we're not an expert, we want verbose output
			if (errstack.code(0) == CEDAR_ERR_CONNECT_FAILED) {
				printNoCollectorContact( stderr, addr, !expert );
			}
		}
		else {
			fprintf (stderr, "Error:  Could not fetch ads --- %s\n",
					 getStrQueryResult(q));
		}
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
		"[custom-opts ...] [name ...]\n"
		"    where [help-opt] is one of\n"
		"\t-help\t\t\tThis screen\n"
		"\t-diagnose\t\tPrint out query ad without performing query\n"
		"    and [query-opt] is one of\n"
		"\t-avail\t\t\tPrint information about available resources\n"
		"\t-ckptsrvr\t\tDisplay checkpoint server attributes\n"
		"\t-claimed\t\tPrint information about claimed resources\n"
		"\t-cod\t\t\tDisplay Computing On Demand (COD) jobs\n"
//		"\t-collector\t\tSame as -world\n"
		"\t-debug\t\t\tDisplay debugging info to console\n"
		"\t-direct <host>\t\tGet attributes directly from the given daemon\n"
		"\t-java\t\t\tDisplay Java-capable hosts\n"
		"\t-vm\t\t\tDisplay VM-capable hosts\n"
		"\t-license\t\tDisplay attributes of licenses\n"
		"\t-master\t\t\tDisplay daemon master attributes\n"
		"\t-pool <name>\t\tGet information from collector <name>\n"
        "\t-grid\t\t\tDisplay grid resources\n"
		"\t-run\t\t\tSame as -claimed [deprecated]\n"
#ifdef WANT_QUILL
		"\t-quill\t\t\tDisplay attributes of quills\n"
#endif /* WANT_QUILL */
		"\t-schedd\t\t\tDisplay attributes of schedds\n"
		"\t-server\t\t\tDisplay important attributes of resources\n"
		"\t-startd\t\t\tDisplay resource attributes\n"
		"\t-generic\t\tDisplay attributes of 'generic' ads\n"
		"\t-subsystem <type>\tDisplay classads of the given type\n"
		"\t-negotiator\t\tDisplay negotiator attributes\n"
		"\t-storage\t\tDisplay network storage resources\n"
		"\t-any\t\t\tDisplay any resources\n"
		"\t-state\t\t\tDisplay state of resources\n"
		"\t-submitters\t\tDisplay information about request submitters\n"
//		"\t-world\t\t\tDisplay all pools reporting to UW collector\n"
		"    and [display-opt] is one of\n"
		"\t-long\t\t\tDisplay entire classads\n"
		"\t-sort <attr>\t\tSort entries by named attribute\n"
		"\t-total\t\t\tDisplay totals only\n"
		"\t-verbose\t\tSame as -long\n"
		"\t-xml\t\t\tDisplay entire classads, but in XML\n"
		"\t-expert\t\t\tDisplay shorter error messages\n"
		"    and [custom-opts ...] are one or more of\n"
		"\t-constraint <const>\tAdd constraint on classads\n"
		"\t-format <fmt> <attr>\tRegister display format and attribute\n"
		"\t-target filename\tIf -format is used, the option target classad\n",
		myName);
}

void
firstPass (int argc, char *argv[])
{
	int had_pool_error = 0;
	int had_direct_error = 0;
	// Process arguments:  there are dependencies between them
	// o -l/v and -serv are mutually exclusive
	// o -sub, -avail and -run are mutually exclusive
	// o -pool and -entity may be used at most once
	// o since -c can be processed only after the query has been instantiated,
	//   constraints are added on the second pass
	for (int i = 1; i < argc; i++) {
		if (matchPrefix (argv[i], "-avail", 3)) {
			setMode (MODE_STARTD_AVAIL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-pool", 2)) {
			if( pool ) {
				delete pool;
				had_pool_error = 1;
			}
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -pool requires a hostname as an argument.\n",
						 myName );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: The hostname should be the central "
									   "manager of the Condor pool you wish to work with.",
									   stderr);
					printf("\n");
				}
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			pool = new DCCollector( argv[i] );
			if( !pool->addr() ) {
				fprintf( stderr, "Error: %s\n", pool->error() );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: You specified a hostname for a pool "
									   "(the -pool argument). That should be the Internet "
									   "host name for the central manager of the pool, "
									   "but it does not seem to "
									   "be a valid hostname. (The DNS lookup failed.)",
									   stderr);
				}
				exit( 1 );
			}
		} else
		if (matchPrefix (argv[i], "-format", 2)) {
			setPPstyle (PP_CUSTOM, i, argv[i]);
			if( !argv[i+1] || !argv[i+2] ) {
				fprintf( stderr, "%s: -format requires two other arguments\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i += 2;			
		} else
		if (matchPrefix (argv[i], "-target", 5)) {
			if( !argv[i+1] ) {
				fprintf( stderr, "%s: -target requires one additional argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i += 1;
			target = argv[i];
			FILE *targetFile = safe_fopen_wrapper(target, "r");
			int iseof, iserror, empty;
			targetAd = new ClassAd(targetFile, "\n\n", iseof, iserror, empty);
		} else
		if (matchPrefix (argv[i], "-constraint", 4)) {
			// can add constraints on second pass only
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -constraint requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
		} else
		if (matchPrefix (argv[i], "-direct", 4)) {
			if( direct ) {
				free( direct );
				had_direct_error = 1;
			}
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -direct requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			direct = strdup( argv[i] );
		} else
		if (matchPrefix (argv[i], "-diagnose", 4)) {
			diagnose = 1;
		} else
		if (matchPrefix (argv[i], "-debug", 3)) {
			// dprintf to console
			Termlog = 1;
			dprintf_config ("TOOL");
		} else
		if (matchPrefix (argv[i], "-help", 2)) {
			usage ();
			exit (0);
		} else
		if (matchPrefix (argv[i], "-long", 2) || matchPrefix (argv[i],"-verbose", 3)) {
			setPPstyle (PP_VERBOSE, i, argv[i]);
		} else
		if (matchPrefix (argv[i],"-xml", 2)){
			setPPstyle (PP_XML, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-run", 2) || matchPrefix(argv[i], "-claimed", 3)) {
			setMode (MODE_STARTD_RUN, i, argv[i]);
		} else
		if( matchPrefix (argv[i], "-cod", 4) ) {
			setMode (MODE_STARTD_COD, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-java", 2)) {
			javaMode = true;
		} else
		if (matchPrefix (argv[i], "-vm", 3)) {
			vmMode = true;
		} else
		if (matchPrefix (argv[i], "-server", 3)) {
			setPPstyle (PP_STARTD_SERVER, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-state", 5)) {
			setPPstyle (PP_STARTD_STATE, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-startd", 5)) {
			setMode (MODE_STARTD_NORMAL,i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-schedd", 3)) {
			setMode (MODE_SCHEDD_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-grid", 2)) {
			setMode (MODE_GRID_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-subsystem", 5)) {
			i++;
			if( !argv[i] ) {
				fprintf( stderr, "%s: -subsystem requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			if (matchPrefix (argv[i], "schedd", 6)) {
				setMode (MODE_SCHEDD_NORMAL, i, argv[i]);
			} else
			if (matchPrefix (argv[i], "startd", 6)) {
				setMode (MODE_STARTD_NORMAL, i, argv[i]);
			} else
			if (matchPrefix (argv[i], "quill", 5)) {
				setMode (MODE_QUILL_NORMAL, i, argv[i]);
			} else
			if (matchPrefix (argv[i], "negotiator", 10)) {
				setMode (MODE_NEGOTIATOR_NORMAL, i, argv[i]);
			} else
			if (matchPrefix (argv[i], "master", 6)) {
				setMode (MODE_MASTER_NORMAL, i, argv[i]);
			} else
			if (matchPrefix (argv[i], "collector", 9)) {
				setMode (MODE_COLLECTOR_NORMAL, i, argv[i]);
			} else
			if (matchPrefix (argv[i], "generic", 7)) {
				setMode (MODE_GENERIC_NORMAL, i, argv[i]);
			} else
			if (matchPrefix (argv[i], "had", 3)) {
				setMode (MODE_HAD_NORMAL, i, argv[i]);
			} else
			if (*argv[i] == '-') {
				fprintf(stderr, "%s: -subsystem requires another argument\n",
						myName);
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit(1);
			} else {
				genericType = strdup(argv[i]);
				setMode (MODE_OTHER, i, argv[i]);
			}
		} else
#ifdef WANT_QUILL
		if (matchPrefix (argv[i], "-quill", 2)) {
			setMode (MODE_QUILL_NORMAL, i, argv[i]);
		} else
#endif /* WANT_QUILL */
		if (matchPrefix (argv[i], "-license", 3)) {
			setMode (MODE_LICENSE_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-storage", 4)) {
			setMode (MODE_STORAGE_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-negotiator", 2)) {
			setMode (MODE_NEGOTIATOR_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-generic", 3)) {
			setMode (MODE_GENERIC_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-any", 3)) {
			setMode (MODE_ANY_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-sort", 3)) {
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -sort requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			char	exprString[1024];
			ExprTree	*sortExpr;
			exprString[0] = '\0';
			sprintf( exprString, "MY.%s < TARGET.%s", argv[i], argv[i] );
			if( ParseClassAdRvalExpr( exprString, sortExpr ) ) {
				fprintf( stderr, "Error:  Parse error of: %s\n", exprString );
				exit( 1 );
			}
			sortLessThanExprs[sortLessThanExprs.getlast()+1] = sortExpr;
			sprintf( exprString, "MY.%s == TARGET.%s", argv[i], argv[i] );
			if( ParseClassAdRvalExpr( exprString, sortExpr ) ) {
				fprintf( stderr, "Error:  Parse error of: %s\n", exprString );
				exit( 1 );
			}
			sortEqualExprs[sortEqualExprs.getlast()+1] = sortExpr;

				// the silent constraint TARGET.%s =!= UNDEFINED is added
				// as a customAND constraint on the second pass
		} else
		if (matchPrefix (argv[i], "-submitters", 5)) {
			setMode (MODE_SCHEDD_SUBMITTORS, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-master", 2)) {
			setMode (MODE_MASTER_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-collector", 4)) {
			setMode (MODE_COLLECTOR_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-world", 2)) {
			setMode (MODE_COLLECTOR_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-ckptsrvr", 3)) {
			setMode (MODE_CKPT_SRVR_NORMAL, i, argv[i]);
		} else
		if (matchPrefix (argv[i], "-total", 2)) {
			wantOnlyTotals = 1;
		} else
		if (matchPrefix(argv[i], "-expert", 2)) {
			expert = true;
		} else
		if (matchPrefix(argv[i], "-version", 4)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit(0);
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
				 pool->name() );
	}
	if( had_direct_error ) {
		fprintf( stderr,
				 "Warning:  Multiple -direct arguments given, using \"%s\"\n",
				 direct );
	}
}


void
secondPass (int argc, char *argv[])
{
	char *daemonname;
	for (int i = 1; i < argc; i++) {
		// omit parameters which qualify switches
		if( matchPrefix(argv[i],"-pool", 2) || matchPrefix(argv[i],"-direct", 4) ) {
			i++;
			continue;
		}
		if( matchPrefix(argv[i],"-subsystem", 5) ) {
			i++;
			continue;
		}
		if (matchPrefix (argv[i], "-format", 2)) {
			pm.registerFormat (argv[i+1], argv[i+2]);
			projList.AppendArg(argv[i+2]);
			if (diagnose) {
				printf ("Arg %d --- register format [%s] for [%s]\n",
						i, argv[i+1], argv[i+2]);
			}
			i += 2;
			continue;
		}
		if (matchPrefix (argv[i], "-target", 2)) {
			i += 2;
			continue;
		}
		if( matchPrefix(argv[i], "-sort", 3) ) {
			i++;
			sprintf( buffer, "TARGET.%s =!= UNDEFINED", argv[i] );
			query->addANDConstraint( buffer );
			continue;
		}


		// figure out what the other parameters should do
		if (*argv[i] != '-') {
			// display extra information for diagnosis
			if (diagnose) {
				printf ("Arg %d (%s) --- adding constraint", i, argv[i]);
			}

			if( !(daemonname = get_daemon_name(argv[i])) ) {
				if ( (mode==MODE_SCHEDD_SUBMITTORS) && strchr(argv[i],'@') ) {
					// For a submittor query, it is possible that the
					// hostname is really a UID_DOMAIN.  And there is
					// no requirement that UID_DOMAIN actually have
					// an inverse lookup in DNS...  so if get_daemon_name()
					// fails with a fully qualified submittor lookup, just
					// use what we are given and do not flag an error.
					daemonname = strnewp(argv[i]);
				} else {
					fprintf( stderr, "%s: unknown host %s\n",
								 argv[0], get_host_part(argv[i]) );
					exit(1);
				}
			}

			switch (mode) {
			  case MODE_STARTD_NORMAL:
			  case MODE_STARTD_COD:
#ifdef WANT_QUILL
			  case MODE_QUILL_NORMAL:
#endif /* WANT_QUILL */
			  case MODE_SCHEDD_NORMAL:
			  case MODE_SCHEDD_SUBMITTORS:
			  case MODE_MASTER_NORMAL:
			  case MODE_COLLECTOR_NORMAL:
			  case MODE_CKPT_SRVR_NORMAL:
			  case MODE_NEGOTIATOR_NORMAL:
			  case MODE_STORAGE_NORMAL:
			  case MODE_ANY_NORMAL:
			  case MODE_GENERIC_NORMAL:
			  case MODE_STARTD_AVAIL:
			  case MODE_OTHER:
			  case MODE_GRID_NORMAL:
			  case MODE_HAD_NORMAL:
			  	sprintf(buffer,"(TARGET.%s==\"%s\") || (TARGET.%s==\"%s\")",
						ATTR_NAME, daemonname, ATTR_MACHINE, daemonname );
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addORConstraint (buffer);
				break;

			  case MODE_STARTD_RUN:
				sprintf (buffer,"TARGET.%s == \"%s\"",ATTR_REMOTE_USER,argv[i]);
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addORConstraint (buffer);
				break;

			  default:
				fprintf(stderr,"Error: Don't know how to process %s\n",argv[i]);
			}
			delete [] daemonname;
			daemonname = NULL;
		} else
		if (matchPrefix (argv[i], "-constraint", 4)) {
			if (diagnose) {
				printf ("[%s]\n", argv[i+1]);
			}
			query->addANDConstraint (argv[i+1]);
			i++;
		}
	}
}


int
matchPrefix (const char *s1, const char *s2, int min_len)
{
	int lenS1 = strlen (s1);
	int lenS2 = strlen (s2);
	int len = (lenS1 < lenS2) ? lenS1 : lenS2;
	if(len < min_len) {
		return 0;
	}

	return (strncmp (s1, s2, len) == 0);
}


int
lessThanFunc(AttrList *ad1, AttrList *ad2, void *)
{
	MyString  buf1;
	MyString  buf2;
	int       val;

	if( !ad1->LookupString(ATTR_OPSYS, buf1) ||
		!ad2->LookupString(ATTR_OPSYS, buf2) ) {
		buf1 = "";
		buf2 = "";
	}
	val = strcmp( buf1.Value(), buf2.Value() );
	if( val ) {
		return (val < 0);
	}

	if( !ad1->LookupString(ATTR_ARCH, buf1) ||
		!ad2->LookupString(ATTR_ARCH, buf2) ) {
		buf1 = "";
		buf2 = "";
	}
	val = strcmp( buf1.Value(), buf2.Value() );
	if( val ) {
		return (val < 0);
	}

	if( !ad1->LookupString(ATTR_MACHINE, buf1) ||
		!ad2->LookupString(ATTR_MACHINE, buf2) ) {
		buf1 = "";
		buf2 = "";
	}
	val = strcmp( buf1.Value(), buf2.Value() );
	if( val ) {
		return (val < 0);
	}

	if (!ad1->LookupString(ATTR_NAME, buf1) ||
		!ad2->LookupString(ATTR_NAME, buf2))
		return 0;
	return ( strcmp( buf1.Value(), buf2.Value() ) < 0 );
}

int
customLessThanFunc( AttrList *ad1, AttrList *ad2, void *)
{
	EvalResult 	lt_result;
	int			last = sortLessThanExprs.getlast();

	for( int i = 0 ; i <= last ; i++ ) {
		sortLessThanExprs[i]->EvalTree( ad1, ad2, &lt_result );
		if( lt_result.type == LX_INTEGER ) {
			if( lt_result.i ) {
				return 1;
			} else {
				sortEqualExprs[i]->EvalTree( ad1, ad2, &lt_result );
				if( lt_result.type != LX_INTEGER || !lt_result.i )
					return 0;
			}
		} else {
			return 0;
		}
	}

	return 0;
}
