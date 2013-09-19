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
#include "match_prefix.h"    // is_arg_colon_prefix
#include "print_wrapped_text.h"
#include "error_utils.h"
#include "condor_distribution.h"
#include "condor_version.h"

#include <vector>
#include <sstream>
#include <iostream>

using std::vector;
using std::string;
using std::stringstream;

struct SortSpec {
    string arg;
    string keyAttr;
    string keyExprAttr;
    ExprTree* expr;
    ExprTree* exprLT;
    ExprTree* exprEQ;

    SortSpec(): arg(), keyAttr(), keyExprAttr(), expr(NULL), exprLT(NULL), exprEQ(NULL) {}
    ~SortSpec() {
        if (NULL != expr) delete expr;
        if (NULL != exprLT) delete exprLT;
        if (NULL != exprEQ) delete exprEQ;
    }

    SortSpec(const SortSpec& src): expr(NULL), exprLT(NULL), exprEQ(NULL) { *this = src; }
    SortSpec& operator=(const SortSpec& src) {
        if (this == &src) return *this;

        arg = src.arg;
        keyAttr = src.keyAttr;
        keyExprAttr = src.keyExprAttr;
        if (NULL != expr) delete expr;
        expr = src.expr->Copy();
        if (NULL != exprLT) delete exprLT;
        exprLT = src.exprLT->Copy();
        if (NULL != exprEQ) delete exprEQ;
        exprEQ = src.exprEQ->Copy();

        return *this;
    }
};


// global variables
AttrListPrintMask pm;
List<const char> pm_head; // The list of headings for the mask entries
const char		*DEFAULT= "<default>";
DCCollector* pool = NULL;
AdTypes		type 	= (AdTypes) -1;
ppOption	ppStyle	= PP_NOTSET;
int			wantOnlyTotals 	= 0;
int			summarySize = -1;
bool        expert = false;
bool		wide_display = false; // when true, don't truncate field data
bool		invalid_fields_empty = false; // when true, print "" instead of "[?]" for missing data
Mode		mode	= MODE_NOTSET;
int			diagnose = 0;
char*		direct = NULL;
char*       statistics = NULL;
char*		genericType = NULL;
CondorQuery *query;
char		buffer[1024];
ClassAdList result;
char		*myName;
vector<SortSpec> sortSpecs;
bool            javaMode = false;
bool			vmMode = false;
bool        absentMode = false;
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
extern	void setPPstyle (ppOption, int, const char *);
extern	void setType    (const char *, int, const char *);
extern	void setMode 	(Mode, int, const char *);

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
	dprintf_config_tool_on_error(0);

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
		dprintf_WriteOnErrorBuffer(stderr, true);
		fprintf (stderr, "Error:  Out of memory\n");
		exit (1);
	}

	// set pretty print style implied by the type of entity being queried
	// but do it with default priority, so that explicitly requested options
	// can override it
	switch (type)
	{
#ifdef HAVE_EXT_POSTGRESQL
	  case QUILL_AD:
		setPPstyle(PP_QUILL_NORMAL, 0, DEFAULT);
		break;
#endif /* HAVE_EXT_POSTGRESQL */

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
		break;

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
#ifdef HAVE_EXT_POSTGRESQL
	  case MODE_QUILL_NORMAL:
#endif /* HAVE_EXT_POSTGRESQL */

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
		free(genericType);
		genericType = NULL;
		break;

	  case MODE_STARTD_AVAIL:
			  // For now, -avail shows you machines avail to anyone.
		sprintf (buffer, "%s == \"%s\"", ATTR_STATE,
					state_to_string(unclaimed_state));
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addORConstraint (buffer);
		break;


	  case MODE_STARTD_RUN:
		sprintf (buffer, "%s == \"%s\"", ATTR_STATE,
					state_to_string(claimed_state));
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addORConstraint (buffer);
		break;

	  case MODE_STARTD_COD:
	    sprintf (buffer, "%s > 0", ATTR_NUM_COD_CLAIMS );
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addORConstraint (buffer);
		break;

	  default:
		break;
	}	

	if(javaMode) {
		sprintf( buffer, "%s == TRUE", ATTR_HAS_JAVA );
		if (diagnose) {
			printf ("Adding constraint [%s]\n", buffer);
		}
		query->addANDConstraint (buffer);
		
		projList.AppendArg(ATTR_HAS_JAVA);
		projList.AppendArg(ATTR_JAVA_MFLOPS);
		projList.AppendArg(ATTR_JAVA_VENDOR);
		projList.AppendArg(ATTR_JAVA_VERSION);

	}
	
	if(absentMode) {
	    sprintf( buffer, "%s == TRUE", ATTR_ABSENT );
	    if (diagnose) {
	        printf( "Adding constraint %s\n", buffer );
	    }
	    query->addANDConstraint( buffer );
	    
	    projList.AppendArg( ATTR_ABSENT );
	    projList.AppendArg( ATTR_LAST_HEARD_FROM );
	    projList.AppendArg( ATTR_CLASSAD_LIFETIME );
	}

	if(vmMode) {
		sprintf( buffer, "%s == TRUE", ATTR_HAS_VM);
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
		projList.AppendArg("Machine");
		projList.AppendArg("Opsys");
		projList.AppendArg("Arch");
		projList.AppendArg("State");
		projList.AppendArg("Activity");
		projList.AppendArg("LoadAvg");
		projList.AppendArg("Memory");
		projList.AppendArg("ActvtyTime");
		projList.AppendArg("MyCurrentTime");
		projList.AppendArg("EnteredCurrentActivity");
	} else if( ppStyle == PP_VERBOSE ) {
	    // Remove everything from the projection list if we're displaying
	    // the "long form" of the ads.
	    projList.Clear();
	}

	if( projList.Count() > 0 ) {
		char **attr_list = projList.GetStringArray();
		query->setDesiredAttrs(attr_list);
		deleteStringArray(attr_list);
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
		fPrintAd (stdout, queryAd);

		printf ("----------\n");
		fprintf (stderr, "Result of making query ad was:  %d\n", q);
		exit (1);
	}

        // Address (host:port) is taken from requested pool, if given.
	char* addr = (NULL != pool) ? pool->addr() : NULL;
        Daemon* requested_daemon = pool;

        // If we're in "direct" mode, then we attempt to locate the daemon
	// associated with the requested subsystem (here encoded by value of mode)
        // In this case the host:port of pool (if given) denotes which
        // pool is being consulted
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

#ifdef HAVE_EXT_POSTGRESQL
		case MODE_QUILL_NORMAL:
			d = new Daemon( DT_QUILL, direct, addr );
			break;
#endif /* HAVE_EXT_POSTGRESQL */

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

                // Here is where we actually override 'addr', if we can obtain
                // address of the requested daemon/subsys.  If it can't be
                // located, then fail with error msg.
                // 'd' will be null (unset) if mode is one of above that must go to
                // collector (MODE_ANY_NORMAL, MODE_COLLECTOR_NORMAL, etc)
		if (NULL != d) {
			if( d->locate() ) {
				addr = d->addr();
				requested_daemon = d;
			} else {
				const char* id = d->idStr();
				if (NULL == id) id = d->name();
				dprintf_WriteOnErrorBuffer(stderr, true);
				if (NULL == id) id = "daemon";
				fprintf(stderr, "Error: Failed to locate %s\n", id);
				fprintf(stderr, "%s\n", d->error());
				exit( 1 );
			}
		}
	}

	CondorError errstack;
	if (NULL != addr) {
	        // this case executes if pool was provided, or if in "direct" mode with
	        // subsystem that corresponds to a daemon (above).
                // Here 'addr' represents either the host:port of requested pool, or
                // alternatively the host:port of daemon associated with requested subsystem (direct mode)
		q = query->fetchAds (result, addr, &errstack);
	} else {
                // otherwise obtain list of collectors and submit query that way
		CollectorList * collectors = CollectorList::create();
		q = collectors->query (*query, result, &errstack);
		delete collectors;
	}
		

	// if any error was encountered during the query, report it and exit 
        if (Q_OK != q) {
            dprintf_WriteOnErrorBuffer(stderr, true);
                // we can always provide these messages:
	        fprintf( stderr, "Error: %s\n", getStrQueryResult(q) );
		fprintf( stderr, "%s\n", errstack.getFullText(true).c_str() );

	        if ((NULL != requested_daemon) && ((Q_NO_COLLECTOR_HOST == q) || (requested_daemon->type() == DT_COLLECTOR))) {
                        // Specific long message if connection to collector failed.
		        const char* fullhost = requested_daemon->fullHostname();
                        if (NULL == fullhost) fullhost = "<unknown_host>";
                        const char* daddr = requested_daemon->addr();
                        if (NULL == daddr) daddr = "<unknown>";
                        char info[1000];
                        sprintf(info, "%s (%s)", fullhost, daddr);
		        printNoCollectorContact( stderr, info, !expert );                        
	        } else if ((NULL != requested_daemon) && (Q_COMMUNICATION_ERROR == q)) {
                        // more helpful message for failure to connect to some daemon/subsys
			const char* id = requested_daemon->idStr();
                        if (NULL == id) id = requested_daemon->name();
			if (NULL == id) id = "daemon";
                        const char* daddr = requested_daemon->addr();
                        if (NULL == daddr) daddr = "<unknown>";
           	        fprintf(stderr, "Error: Failed to contact %s at %s\n", id, daddr);
		}

                // fail
                exit (1);
	}

	if (sortSpecs.empty()) {
        // default classad sorting
		result.Sort((SortFunctionType)lessThanFunc);
	} else {
        // User requested custom sorting expressions:
        // insert attributes related to custom sorting
        result.Open();
        while (ClassAd* ad = result.Next()) {
            for (vector<SortSpec>::iterator ss(sortSpecs.begin());  ss != sortSpecs.end();  ++ss) {
                ss->expr->SetParentScope(ad);
                classad::Value v;
                ss->expr->Evaluate(v);
                stringstream vs;
                // This will properly render all supported value types,
                // including undefined and error, although current semantic
                // pre-filters classads where sort expressions are undef/err:
                vs << ((v.IsStringValue())?"\"":"") << v << ((v.IsStringValue())?"\"":"");
                ad->AssignExpr(ss->keyAttr.c_str(), vs.str().c_str());
                // Save the full expr in case user wants to examine on output:
                ad->AssignExpr(ss->keyExprAttr.c_str(), ss->arg.c_str());
            }
        }
        
        result.Open();
		result.Sort((SortFunctionType)customLessThanFunc);
	}

	
	// output result
	prettyPrint (result, &totals);
	
    delete query;

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
		"\t-collector\t\tDisplay collector daemon attributes\n"
		"\t-debug\t\t\tDisplay debugging info to console\n"
		"\t-direct <host>\t\tGet attributes directly from the given daemon\n"
		"\t-java\t\t\tDisplay Java-capable hosts\n"
		"\t-vm\t\t\tDisplay VM-capable hosts\n"
		"\t-license\t\tDisplay attributes of licenses\n"
		"\t-master\t\t\tDisplay daemon master attributes\n"
		"\t-pool <name>\t\tGet information from collector <name>\n"
        "\t-grid\t\t\tDisplay grid resources\n"
		"\t-run\t\t\tSame as -claimed [deprecated]\n"
#ifdef HAVE_EXT_POSTGRESQL
		"\t-quill\t\t\tDisplay attributes of quills\n"
#endif /* HAVE_EXT_POSTGRESQL */
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
//      "\t-statistics <set>:<n>\tDisplay statistics for <set> at level <n>\n"
//      "\t\t\t\tsee STATISTICS_TO_PUBLISH for valid <set> and level values\n"
//		"\t-world\t\t\tDisplay all pools reporting to UW collector\n"
		"    and [display-opt] is one of\n"
		"\t-long\t\t\tDisplay entire classads\n"
		"\t-sort <expr>\t\tSort entries by expressions\n"
		"\t-total\t\t\tDisplay totals only\n"
		"\t-verbose\t\tSame as -long\n"
		"\t-wide\t\t\tdon't truncate data to fit in 80 columns.\n"
		"\t-xml\t\t\tDisplay entire classads, but in XML\n"
		"\t-attributes X,Y,...\tAttributes to show in -xml or -long \n"
		"\t-expert\t\t\tDisplay shorter error messages\n"
		"    and [custom-opts ...] are one or more of\n"
		"\t-constraint <const>\tAdd constraint on classads\n"
		"\t-format <fmt> <attr>\tRegister display format and attribute\n"
		"\t-autoformat:[V,ntlh] <attr> [attr2 [attr3 ...]]\t    Print attr(s) with automatic formatting\n"
		"\t\tV\tUse %%V formatting\n"
		"\t\t,\tComma separated (default is space separated)\n"
		"\t\tt\tTab separated\n"
		"\t\tn\tNewline after each attribute\n"
		"\t\tl\tLabel each value\n"
		"\t\th\tHeadings\n"
		"\t-target filename\tIf -format or -af is used, the option target classad\n",
		myName);
}

void
firstPass (int argc, char *argv[])
{
	int had_pool_error = 0;
	int had_direct_error = 0;
	int had_statistics_error = 0;
	const char * pcolon = NULL;

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
				dprintf_WriteOnErrorBuffer(stderr, true);
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
		if (*argv[i] == '-' &&
			(is_arg_colon_prefix(argv[i]+1, "autoformat", &pcolon, 5) || 
			 is_arg_colon_prefix(argv[i]+1, "af", &pcolon, 2)) ) {
				// make sure we have at least one more argument
			if ( !argv[i+1] || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: Argument %s requires "
						 "at last one attribute parameter\n", argv[i] );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			setPPstyle (PP_CUSTOM, i, argv[i]);
			while (argv[i+1] && *(argv[i+1]) != '-') {
				++i;
			}
			// if autoformat list ends in a '-' without any characters after it, just eat the arg and keep going.
			if (i+1 < argc && '-' == (argv[i+1])[0] && 0 == (argv[i+1])[1]) {
				++i;
			}
		} else
		if (matchPrefix (argv[i], "-wide", 3)) {
			wide_display = true; // when true, don't truncate field data
			//invalid_fields_empty = true;
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
			FILE *targetFile = safe_fopen_wrapper_follow(target, "r");
			int iseof, iserror, empty;
			targetAd = new ClassAd(targetFile, "\n\n", iseof, iserror, empty);
			fclose(targetFile);
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
			dprintf_set_tool_debug("TOOL", 0);
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
		if (matchPrefix (argv[i],"-attributes", 3)){
			if( !argv[i+1] ) {
				fprintf( stderr, "%s: -attributes requires one additional argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			i++;
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
		if (matchPrefix (argv[i], "-absent", 3)) {
			absentMode = true;
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
		if (matchPrefix (argv[i], "-statistics", 6)) {
			if( statistics ) {
				free( statistics );
				had_statistics_error = 1;
			}
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -statistics requires another argument\n",
						 myName );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}
			statistics = strdup( argv[i] );
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
#ifdef HAVE_EXT_POSTGRESQL
		if (matchPrefix (argv[i], "-quill", 2)) {
			setMode (MODE_QUILL_NORMAL, i, argv[i]);
		} else
#endif /* HAVE_EXT_POSTGRESQL */
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

            int jsort = sortSpecs.size();
            SortSpec ss;
			ExprTree* sortExpr = NULL;
			if (ParseClassAdRvalExpr(argv[i], sortExpr)) {
				fprintf(stderr, "Error:  Parse error of: %s\n", argv[i]);
				exit(1);
			}
            ss.expr = sortExpr;

            ss.arg = argv[i];
            formatstr(ss.keyAttr, "CondorStatusSortKey%d", jsort);
            formatstr(ss.keyExprAttr, "CondorStatusSortKeyExpr%d", jsort);

			string exprString;
			formatstr(exprString, "MY.%s < TARGET.%s", ss.keyAttr.c_str(), ss.keyAttr.c_str());
			if (ParseClassAdRvalExpr(exprString.c_str(), sortExpr)) {
                fprintf(stderr, "Error:  Parse error of: %s\n", exprString.c_str());
                exit(1);
			}
			ss.exprLT = sortExpr;

			formatstr(exprString, "MY.%s == TARGET.%s", ss.keyAttr.c_str(), ss.keyAttr.c_str());
			if (ParseClassAdRvalExpr(exprString.c_str(), sortExpr)) {
                fprintf(stderr, "Error:  Parse error of: %s\n", exprString.c_str());
                exit(1);
			}
			ss.exprEQ = sortExpr;

            sortSpecs.push_back(ss);
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
	if( had_statistics_error ) {
		fprintf( stderr,
				 "Warning:  Multiple -statistics arguments given, using \"%s\"\n",
				 statistics );
	}
}


void
secondPass (int argc, char *argv[])
{
	const char * pcolon = NULL;
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

			StringList attributes;
			ClassAd ad;
			if(!ad.GetExprReferences(argv[i+2],attributes,attributes)){
				fprintf( stderr, "Error:  Parse error of: %s\n", argv[i+2]);
				exit(1);
			}

			attributes.rewind();
			char const *s;
			while( (s=attributes.next()) ) {
				projList.AppendArg(s);
			}

			if (diagnose) {
				printf ("Arg %d --- register format [%s] for [%s]\n",
						i, argv[i+1], argv[i+2]);
			}
			i += 2;
			continue;
		}
		if (*argv[i] == '-' &&
			(is_arg_colon_prefix(argv[i]+1, "autoformat", &pcolon, 5) || 
			 is_arg_colon_prefix(argv[i]+1, "af", &pcolon, 2)) ) {
				// make sure we have at least one more argument
			if ( !argv[i+1] || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: Argument %s requires "
						 "at last one attribute parameter\n", argv[i] );
				fprintf( stderr, "Use \"%s -help\" for details\n", myName );
				exit( 1 );
			}

			bool flabel = false;
			bool fCapV  = false;
			bool fheadings = false;
			const char * pcolpre = " ";
			const char * pcolsux = NULL;
			if (pcolon) {
				++pcolon;
				while (*pcolon) {
					switch (*pcolon)
					{
						case ',': pcolsux = ","; break;
						case 'n': pcolsux = "\n"; break;
						case 't': pcolpre = "\t"; break;
						case 'l': flabel = true; break;
						case 'V': fCapV = true; break;
						case 'h': fheadings = true; break;
					}
					++pcolon;
				}
			}
			pm.SetAutoSep(NULL, pcolpre, pcolsux, "\n");

			while (argv[i+1] && *(argv[i+1]) != '-') {
				++i;
				ClassAd ad;
				StringList attributes;
				if(!ad.GetExprReferences(argv[i],attributes,attributes)){
					fprintf( stderr, "Error:  Parse error of: %s\n", argv[i]);
					exit(1);
				}

				attributes.rewind();
				char const *s;
				while ((s = attributes.next())) {
					projList.AppendArg(s);
				}

				MyString lbl = "";
				int wid = 0;
				int opts = FormatOptionNoTruncate;
				if (fheadings || pm_head.Length() > 0) { 
					const char * hd = fheadings ? argv[i] : "(expr)";
					wid = 0 - (int)strlen(hd); 
					opts = FormatOptionAutoWidth | FormatOptionNoTruncate; 
					pm_head.Append(hd);
				}
				else if (flabel) { lbl.formatstr("%s = ", argv[i]); wid = 0; opts = 0; }
				lbl += fCapV ? "%V" : "%v";
				if (diagnose) {
					printf ("Arg %d --- register format [%s] width=%d, opt=0x%x for [%s]\n",
							i, lbl.Value(), wid, opts,  argv[i]);
				}
				pm.registerFormat(lbl.Value(), wid, opts, argv[i]);
			}
			// if autoformat list ends in a '-' without any characters after it, just eat the arg and keep going.
			if (i+1 < argc && '-' == (argv[i+1])[0] && 0 == (argv[i+1])[1]) {
				++i;
			}
			continue;
		}
		if (matchPrefix (argv[i], "-target", 2)) {
			i++;
			continue;
		}
		if( matchPrefix(argv[i], "-sort", 3) ) {
			i++;
			sprintf( buffer, "%s =!= UNDEFINED", argv[i] );
			query->addANDConstraint( buffer );
			continue;
		}
		
		if (matchPrefix (argv[i], "-statistics", 6)) {
			i += 2;
            sprintf(buffer,"STATISTICS_TO_PUBLISH = \"%s\"", statistics);
            if (diagnose) {
               printf ("[%s]\n", buffer);
               }
            query->addExtraAttribute(buffer);
            continue;
        }

		if (matchPrefix (argv[i], "-attributes", 3) ) {
			// parse attributes to be selected and split them along ","
			StringList more_attrs(argv[i+1],",");
			char const *s;
			more_attrs.rewind();
			while( (s=more_attrs.next()) ) {
				projList.AppendArg(s);
			}
			i++;
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
					dprintf_WriteOnErrorBuffer(stderr, true);
					fprintf( stderr, "%s: unknown host %s\n",
								 argv[0], get_host_part(argv[i]) );
					exit(1);
				}
			}

			switch (mode) {
			  case MODE_STARTD_NORMAL:
			  case MODE_STARTD_COD:
#ifdef HAVE_EXT_POSTGRESQL
			  case MODE_QUILL_NORMAL:
#endif /* HAVE_EXT_POSTGRESQL */
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
			  	sprintf(buffer,"(%s==\"%s\") || (%s==\"%s\")",
						ATTR_NAME, daemonname, ATTR_MACHINE, daemonname );
				if (diagnose) {
					printf ("[%s]\n", buffer);
				}
				query->addORConstraint (buffer);
				break;

			  case MODE_STARTD_RUN:
				sprintf (buffer,"%s == \"%s\"",ATTR_REMOTE_USER,argv[i]);
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
	classad::Value lt_result;
	bool val;

	for (unsigned i = 0;  i < sortSpecs.size();  ++i) {
		if (EvalExprTree(sortSpecs[i].exprLT, ad1, ad2, lt_result)
			&& lt_result.IsBooleanValue(val) ) {
			if( val ) {
				return 1;
			} else {
				if (EvalExprTree( sortSpecs[i].exprEQ, ad1,
					ad2, lt_result ) &&
					( !lt_result.IsBooleanValue(val) || !val )){
					return 0;
				}
			}
		} else {
			return 0;
		}
	}
	return 0;
}
