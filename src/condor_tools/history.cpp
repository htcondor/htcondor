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
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "dc_collector.h"
#include "dc_schedd.h"
#include "internet.h"
#include "print_wrapped_text.h"
#include "metric_units.h"
#include "ad_printmask.h"
#include "directory.h"
#include "iso_dates.h"
#include "basename.h" // for condor_dirname
#include "match_prefix.h"
#include "subsystem_info.h"
#include "historyFileFinder.h"
#include "condor_id.h"
#include "userlog_to_classads.h"
#include "setenv.h"
#include "condor_daemon_core.h" // for extractInheritedSocks
#include "console-utils.h"
#include <algorithm> //for std::reverse

#include "classad_helpers.h" // for initStringListFromAttrs
#include "history_utils.h"
#include "backward_file_reader.h"
#include <fcntl.h>  // for O_BINARY

void Usage(const char* name, int iExitCode=1);

void Usage(const char* name, int iExitCode) 
{
	printf ("Usage: %s [source] [restriction-list] [options]\n"
		"\n   where [source] is one of\n"
		"\t-file <file>\t\tRead history data from specified file\n"
		"\t-local\t\t\tRead history data from the configured files\n"
		"\t-startd\t\t\tRead history data for the Startd\n"
		"\t-userlog <file>\t\tRead job data specified userlog file\n"
		"\t-name <schedd-name>\tRemote schedd to read from\n"
		"\t-pool <collector-name>\tPool remote schedd lives in.\n"
		"   If neither -file, -local, -userlog, or -name, is specified, then\n"
		"   the SCHEDD configured by SCHEDD_HOST is queried.  If there\n"
		"   is no configured SCHEDD (the default) the local history file(s) are read.\n"
		"\n   and [restriction-list] is one or more of\n"
		"\t<cluster>\t\tGet information about specific cluster\n"
		"\t<cluster>.<proc>\tGet information about specific job\n"
		"\t<owner>\t\t\tInformation about jobs owned by <owner>\n"
		"\t-constraint <expr>\tInformation about jobs matching <expr>\n"
		"\t-scanlimit <num>\tStop scanning when <num> jobs have been read\n"
		"\t-since <jobid>|<expr>\tStop scanning when <expr> is true or <jobid> is read\n"
		"\t-completedsince <time>\tDisplay jobs completed on/after time\n"
		"\n   and [options] are one or more of\n"
		"\t-help\t\t\tDisplay this screen\n"
		"\t-backwards\t\tList jobs in reverse chronological order\n"
		"\t-forwards\t\tList jobs in chronological order\n"
	//	"\t-inherit\t\tWrite results to a socket inherited from parent\n" // for use only by schedd when invoking HISTORY_HELPER
	//  "\t-stream-results\tWrite an EOM into the socket after each result\n" // for use only by schedd when invoking HISTORY_HELPER
		"\t-limit <number>\t\tLimit the number of jobs displayed\n"
		"\t-match <number>\t\tOld name for -limit\n"
		"\t-long\t\t\tDisplay entire classads\n"
		"\t-xml\t\t\tDisplay classads in XML format\n"
		"\t-json\t\t\tDisplay classads in JSON format\n"
		"\t-jsonl\t\t\tDisplay classads in JSON-Lines format: one ad per line\n"
		"\t-attributes <attr-list>\tDisplay only the given attributes\n"
		"\t-wide[:<width>]\t\tDon't truncate fields to fit into 80 columns\n"
		"\t-format <fmt> <attr>\tDisplay attr using printf formatting\n"
		"\t-autoformat[:jlhVr,tng] <attr> [<attr2> [...]]\n"
		"\t-af[:jlhVr,tng] <attr> [attr2 [...]]\n"
		"\t    Print attr(s) with automatic formatting\n"
		"\t    the [jlhVr,tng] options modify the formatting\n"
		"\t        j   display Job Id\n"
		"\t        l   attribute labels\n"
		"\t        h   attribute column headings\n"
		"\t        V   %%V formatting (string values are quoted)\n"
		"\t        r   %%r formatting (raw/unparsed values)\n"
		"\t        ,   comma after each value\n"
		"\t        t   tab before each value (default is space)\n"
		"\t        n   newline after each value\n"
		"\t        g   newline between ClassAds, no space before values\n"
		"\t    use -af:h to get tabular values with headings\n"
		"\t    use -af:lrng to get -long equivalent format\n"
		"\t-print-format <file>\tUse <file> to specify the attributes and formatting\n"
		, name);
  exit(iExitCode);
}

static void readHistoryRemote(classad::ExprTree *constraintExpr, bool want_startd=false);
static void readHistoryFromFiles(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromEpochs(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromFileOld(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromFileEx(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr, bool read_backwards);
static void printJobAds(ClassAdList & jobs);
static void printJob(ClassAd & ad);

static int set_print_mask_from_stream(AttrListPrintMask & print_mask, std::string & constraint, StringList & attrs, const char * streamid, bool is_filename);
static int getDisplayWidth();

//------------------------------------------------------------------------
//Structure to hold info needed for ending search once all matches are found
struct ClusterMatchInfo {
	time_t QDate = -1; //QDate of cluster (same for all procs) to be compared against CompletionDate
	JOB_ID_KEY jid;    //Cluster & Proc info for job
	int numProcs = -1; //TotalSubmitProcs of cluster (same for all procs) to be counted for found cluster procs
};
//Structure to hold banner information for cluster/proc matching
struct BannerInfo {
	time_t completion = -1; //Ad completion time
	JOB_ID_KEY jid;         //Cluster & Proc info for job
	int runId = -1;         //Job epoch < 0 = no epochs
	std::string owner = ""; //Job Owner
};
//------------------------------------------------------------------------

static  bool longformat=false;
static  bool diagnostic = false;
static  bool use_xml=false;
static  bool use_json = false;
static  bool use_json_lines = false;
static  bool wide_format=false;
static  int  wide_format_width = 0;
static  bool customFormat=false;
static  bool disable_user_print_files=false;
static  bool backwards=true;
static  AttrListPrintMask mask;
//tj: headings moved into the mask object.
//static  List<const char> headings; // The list of headings for the mask entries
static int cluster=-1, proc=-1;
static int matchCount = 0, adCount = 0;
static int printCount = 0;
static int specifiedMatch = -1, maxAds = -1;
static std::string g_name, g_pool;
static Stream* socks[2] = { NULL, NULL };
static bool writetosocket = false;
static bool streamresults = false;
static bool streamresults_specified = false; // set to true if -stream-results:<bool> argument is supplied
static int writetosocket_failcount = 0;
static bool abort_transfer = false;
static StringList projection;
static classad::References whitelist;
static ExprTree *sinceExpr = NULL;
static bool want_startd_history = false;
static bool read_epoch_ads = false;
static bool delete_epoch_ads = false;
static const char* epochDirectory = NULL;
static std::deque<ClusterMatchInfo> jobIdFilterInfo;
static std::deque<std::string> ownersList;

int getInheritedSocks(Stream* socks[], size_t cMaxSocks, pid_t & ppid)
{
	const char *envName = ENV_CONDOR_INHERIT;
	const char *inherit = GetEnv(envName);

	dprintf(D_FULLDEBUG, "condor_history: getInheritedSocks from %s is '%s'\n", envName, inherit ? inherit : "NULL");

	if ( ! inherit || ! inherit[0]) {
		socks[0] = NULL;
		return 0;
	}

	std::string psinful;
	StringList remaining_items; // for the remainder which we expect to be empty.
	int cSocks = extractInheritedSocks(inherit, ppid, psinful, socks, (int)cMaxSocks, remaining_items);
	UnsetEnv(envName); // prevent this from being passed on to children.
	return cSocks;
}

int
main(int argc, const char* argv[])
{
  const char *owner=NULL;
  bool readfromfile = true;
  bool fileisuserlog = false;
  bool dash_local = false; // set if -local is passed

  const char* JobHistoryFileName=NULL;
  const char * pcolon=NULL;

  bool hasSince = false;
  bool hasForwards = false;

  GenericQuery constraint; // used to build a complex constraint.
  ExprTree *constraintExpr=NULL;

  std::string tmp;

  int i;

  set_priv_initialize(); // allow uid switching if root
  config();

  readfromfile = ! param_defined("SCHEDD_HOST");

  for(i=1; i<argc; i++) {
    if (is_dash_arg_prefix(argv[i],"long",1)) {
      longformat=TRUE;   
    }
    
    else if (is_dash_arg_prefix(argv[i],"xml",3)) {
		use_xml = true;	
		longformat = true;
	}
    
    else if (is_dash_arg_prefix(argv[i],"jsonl",5)) {
		use_json_lines = true;
		longformat = true;
	}

    else if (is_dash_arg_prefix(argv[i],"json",4)) {
		use_json = true;
		longformat = true;
	}

    else if (is_dash_arg_prefix(argv[i],"backwards",1)) {
        backwards=TRUE;
    }

	// must be at least -forw to avoid conflict with -f (for file) and -format
    else if (is_dash_arg_prefix(argv[i],"nobackwards",3) ||
			 is_dash_arg_prefix(argv[i],"forwards",4)) {
		if ( hasSince ) {
			fprintf(stderr,
				"Error: Argument -forwards cannot be combined with -since.\n");
			exit(1);
		}
		if ( ! writetosocket) {
			backwards=FALSE;
		}
		hasForwards = true;
	}

    else if (is_dash_arg_colon_prefix(argv[i],"wide", &pcolon, 1)) {
        wide_format=TRUE;
        if (pcolon) {
            wide_format_width = atoi(++pcolon);
            if ( ! mask.IsEmpty()) mask.SetOverallWidth(getDisplayWidth()-1);
            if (wide_format_width <= 80) wide_format = FALSE;
        }
    }

    else if (is_dash_arg_prefix(argv[i],"match",1) || is_dash_arg_prefix(argv[i],"limit",3)) {
        i++;
        if (argc <= i) {
            fprintf(stderr,
                    "Error: Argument -match requires a number value as a parameter.\n");
            exit(1);
        }
        specifiedMatch = atoi(argv[i]);
    }

    else if (is_dash_arg_prefix(argv[i],"scanlimit",4)) {
        i++;
        if (argc <= i) {
            fprintf(stderr,
                    "Error: Argument %s requires a number value  as a parameter.\n", argv[i]);
            exit(1);
        }
        maxAds = atoi(argv[i]);
    }

    else if (is_dash_arg_prefix(argv[i],"file",2)) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
		readfromfile = true;
    }
	else if (is_dash_arg_prefix(argv[i],"userlog",1)) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
		readfromfile = true;
		fileisuserlog = true;
	}
	else if (is_dash_arg_prefix(argv[i],"local",2)) {
		// -local overrides the existance of SCHEDD_HOST and forces a local query
		if ( ! g_name.empty()) {
			fprintf(stderr, "Error: Arguments -local and -name cannot be used together\n");
			exit(1);
		}
		readfromfile = true;
		dash_local = true;
	}
	else if (is_dash_arg_prefix(argv[i],"startd",3)) {
		// causes "STARTD_HISTORY" to be queried, rather than "HISTORY"
		want_startd_history = true;
	}
	else if (is_dash_arg_prefix(argv[i],"schedd",3)) {
		// causes "HISTORY" to be queried, this is the default
		want_startd_history = false;
	}
	else if (is_dash_arg_colon_prefix(argv[i],"stream-results", &pcolon, 6)) {
		streamresults = true;
		streamresults_specified = true;
		if (pcolon) {
			// permit optional -stream:false argument to force streaming off, but default to on unless it parses as a bool.
			++pcolon;
			if ( ! string_is_boolean_param(pcolon, streamresults)) {
				streamresults = true;
			}
		}
	}
	else if (is_dash_arg_prefix(argv[i],"inherit",-1)) {

		// Start writing to the ToolLog
		dprintf_config("Tool");

		if (IsFulldebug(D_ALWAYS)) {
			std::string myargs;
			for (int ii = 0; ii < argc; ++ii) { formatstr_cat(myargs, "[%d]%s ", ii, argv[ii]); }
			dprintf(D_FULLDEBUG, "args: %s\n", myargs.c_str());
		}

		pid_t ppid;
		if ( ! getInheritedSocks(socks, COUNTOF(socks), ppid)) {
			fprintf(stderr, "could not parse inherited sockets from environment\n");
			exit(1);
		}
		if ( ! socks[0] || socks[0]->type() != Stream::reli_sock) {
			fprintf(stderr, "first inherited socket is not a ReliSock, aborting\n");
			exit(1);
		}
		readfromfile = true;
		writetosocket = true;
		backwards = true;
		longformat = true;
	}
	else if (is_dash_arg_prefix(argv[i],"attributes",2)) {
		if (argc <= i+1 || *(argv[i+1]) == '-') {
			fprintf(stderr, "Error: Argument -attributes must be followed by a list of attributes\n");
			exit(1);
		}
		i++;
		projection.initializeFromString(argv[i]);
	}
    else if (is_dash_arg_prefix(argv[i],"help",1)) {
		Usage(argv[0],0);
    }
    else if (is_dash_arg_prefix(argv[i],"format",1)) {
		if (argc <= i + 2) {
			fprintf(stderr,
					"Error: Argument -format requires a spec and "
					"classad attribute name as parameters.\n");
			fprintf(stderr,
					"\t\te.g. condor_history -format '%%d' ClusterId\n");
			exit(1);
		}
		mask.registerFormatF(argv[i + 1], argv[i + 2], FormatOptionNoTruncate);
		customFormat = true;
		i += 2;
    }
	else if (*(argv[i]) == '-' && 
				(is_arg_colon_prefix(argv[i]+1,"af", &pcolon, 2) ||
				 is_arg_colon_prefix(argv[i]+1,"autoformat", &pcolon, 5))) {
		// make sure we have at least one argument to autoformat
		if (argc <= i+1 || *(argv[i+1]) == '-') {
			fprintf (stderr, "Error: Argument %s requires at last one attribute parameter\n", argv[i]);
			fprintf(stderr, "\t\te.g. condor_history %s ClusterId\n", argv[i]);
			exit(1);
		}
		if (pcolon) ++pcolon; // if there are options, skip over the colon to the options.
		classad::References refs;
		int ixNext = parse_autoformat_args(argc, argv, i+1, pcolon, mask, refs, diagnostic);
		if (ixNext < 0) {
			fprintf(stderr, "Error: invalid expression : %s\n", argv[-ixNext]);
			exit(1);
		}
		initStringListFromAttrs(projection, true, refs, true);
		if (ixNext > i)
			i = ixNext-1;
		customFormat = true;
	}
	else if (is_dash_arg_colon_prefix(argv[i], "print-format", &pcolon, 2)) {
		if ( (argc <= i+1)  || (*(argv[i+1]) == '-' && (argv[i+1])[1] != 0)) {
			fprintf( stderr, "Error: Argument -print-format requires a filename argument\n");
			exit( 1 );
		}
		// hack allow -pr ! to disable use of user-default print format files.
		if (MATCH == strcmp(argv[i+1], "!")) {
			++i;
			disable_user_print_files = true;
			continue;
		}
		if ( ! wide_format) mask.SetOverallWidth(getDisplayWidth()-1);
		customFormat = true;
		++i;
		std::string where_expr;
		if (set_print_mask_from_stream(mask, where_expr, projection, argv[i], true) < 0) {
			fprintf(stderr, "Error: cannot execute print-format file %s\n", argv[i]);
			exit (1);
		}
		if ( ! where_expr.empty()) {
			constraint.addCustomAND(where_expr.c_str());
		}
	}
	else if (is_dash_arg_colon_prefix(argv[i], "epoch", &pcolon, 1)) { //TODO: Add flag to usage when ready to share with the world
		//Designate reading job run instance ads from epoch directory
		read_epoch_ads = true;
		//Get epoch directory and validate passed arg is a directory
		epochDirectory = param("JOB_EPOCH_INSTANCE_DIR");
		if ( epochDirectory != NULL ) {
			StatInfo si(epochDirectory);
			if (!si.IsDirectory() ) {
				fprintf(stderr, "Error: JOB_EPOCH_INSTANCE_DIR is not a directory.\n");
				exit(1);
			}
		} else {
			fprintf( stderr, "Error: No Job Run Instance directory.\n");
			exit(1);
		}
		//Check for :d option
		while ( pcolon && *pcolon != '\0' ) {
			++pcolon;
			if ( *pcolon == 'd' || *pcolon == 'D' ) { delete_epoch_ads = true; break; }
		}
	}
    else if (is_dash_arg_prefix(argv[i],"constraint",1)) {
		// make sure we have at least one more argument
		if (argc <= i+1) {
			fprintf( stderr, "Error: Argument %s requires another parameter\n", argv[i]);
			exit(1);
		}
		i++;
		constraint.addCustomAND(argv[i]);
    }
	else if (is_dash_arg_prefix(argv[i],"since",4)) {
		if ( hasForwards ) {
			fprintf(stderr,
				"Error: Argument -forwards cannot be combined with -since.\n");
			exit(1);
		}
		// make sure we have at least one more argument
		if (argc <= i+1) {
			fprintf( stderr, "Error: Argument %s requires another parameter\n", argv[i]);
			exit(1);
		}
		delete sinceExpr; sinceExpr = NULL;

		++i;

		classad::Value val;
		if (0 != ParseClassAdRvalExpr(argv[i], sinceExpr) || ExprTreeIsLiteral(sinceExpr, val)) {
			delete sinceExpr; sinceExpr = NULL;
			// if the stop constraint doesn't parse, or is a literal.
			// then there are a few special cases...
			// it might be a job id. or it might be a time value
			PROC_ID jid;
			const char * pend;
			if (StrIsProcId(argv[i], jid.cluster, jid.proc, &pend) && !*pend) {
				std::string buf;
				if (jid.proc >= 0) {
					formatstr(buf, "ClusterId == %d && ProcId == %d", jid.cluster, jid.proc);
				} else {
					formatstr(buf, "ClusterId == %d", jid.cluster);
				}
				ParseClassAdRvalExpr(buf.c_str(), sinceExpr);
			}
		}
		if ( ! sinceExpr) {
			fprintf( stderr, "Error: invalid -since constraint: %s\n\tconstraint must be a job-id, or expression.", argv[i] );
			exit(1);
		}
		hasSince = true;
	}
    else if (is_dash_arg_prefix(argv[i],"completedsince",3)) {
		i++;
		if (argc <= i) {
			fprintf(stderr, "Error: Argument -completedsince requires another parameter.\n");
			exit(1);
		}
		
		delete sinceExpr; sinceExpr = NULL;
		std::string buf; formatstr(buf, "CompletionDate <= %s", argv[i]);
		if (0 != ParseClassAdRvalExpr(buf.c_str(), sinceExpr)) {
			fprintf( stderr, "Error: '%s' not valid parameter for -completedsince ", argv[i]);
			exit(1);
		}
    }

    else if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
		std::string jobconst;
		formatstr (jobconst, "%s == %d && %s == %d", 
				 ATTR_CLUSTER_ID, cluster,ATTR_PROC_ID, proc);
		constraint.addCustomOR(jobconst.c_str());
		ClusterMatchInfo jobIdMatch;
		jobIdMatch.jid = JOB_ID_KEY(cluster,proc);
		jobIdFilterInfo.push_back(jobIdMatch);
    }
    else if (sscanf (argv[i], "%d", &cluster) == 1) {
		std::string jobconst;
		formatstr (jobconst, "%s == %d", ATTR_CLUSTER_ID, cluster);
		constraint.addCustomOR(jobconst.c_str());
		ClusterMatchInfo jobIdMatch;
		jobIdMatch.jid = JOB_ID_KEY(cluster,-1);
		jobIdFilterInfo.push_back(jobIdMatch);
    }
    else if (is_dash_arg_colon_prefix(argv[i],"debug",&pcolon,1)) {
          // dprintf to console
          dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
    }
    else if (is_dash_arg_prefix(argv[i],"diagnostic",4)) {
          // dprintf to console
          diagnostic = true;
    }
    else if (is_dash_arg_prefix(argv[i], "name", 1)) {
        i++;
        if (argc <= i)
        {
            fprintf(stderr,
                "Error: Argument -name requires name of a remote schedd\n");
            fprintf(stderr,
                "\t\te.g. condor_history -name submit.example.com \n");
            exit(1);
        }
        if (dash_local) {
            fprintf(stderr, "Error: -local and -name cannot be used together\n");
            exit(1);
        }
        g_name = argv[i];
        readfromfile = false;
    }
    else if (is_dash_arg_prefix(argv[i], "pool", 1)) {
        i++;    
        if (argc <= i)
        {
            fprintf(stderr,
                "Error: Argument -name requires name of a remote schedd\n");
            fprintf(stderr,
                "\t\te.g. condor_history -name submit.example.com \n");
            exit(1);    
        }       
        g_pool = argv[i];
        readfromfile = false;
    }
	else if (argv[i][0] == '-') {
		fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
		break; // quitting now will print usage and exit with an error below.
	}
	else {
		std::string ownerconst;
		owner = argv[i];
		ownersList.push_back(owner);
		formatstr(ownerconst, "%s == \"%s\"", ATTR_OWNER, owner);
		constraint.addCustomOR(ownerconst.c_str());
    }
  }
  if (i<argc) Usage(argv[0]);
  
  // for remote queries, default to requesting streamed results
  // unless the -stream-results flag was passed explicitly.
  if ( ! readfromfile && ! writetosocket && ! streamresults_specified) {
	streamresults = true;
  }

	// Since we only deal with one ad at a time, this doubles the speed of parsing
  classad::ClassAdSetExpressionCaching(false);
 
  std::string my_constraint;
  constraint.makeQuery(my_constraint);
  if (diagnostic) {
	  fprintf(stderr, "Using effective constraint: %s\n", my_constraint.c_str());
  }
  if ( ! my_constraint.empty() && ParseClassAdRvalExpr( my_constraint.c_str(), constraintExpr ) ) {
	  fprintf( stderr, "Error:  could not parse constraint %s\n", my_constraint.c_str() );
	  exit( 1 );
  }

  if ( use_xml + use_json + use_json_lines > 1 ) {
    fprintf( stderr, "Error: Cannot use more than one of XML and JSON[L]\n" );
    exit( 1 );
  }

  if (writetosocket && streamresults) {
	ClassAd ad;
	ad.InsertAttr(ATTR_OWNER, 1);
	ad.InsertAttr("StreamResults", true);
	dprintf(D_FULLDEBUG, "condor_history: sending streaming ACK header ad:\n");
	dPrintAd(D_FULLDEBUG, ad);
	if ( ! putClassAd(socks[0], ad) || ! socks[0]->end_of_message()) {
		dprintf(D_ALWAYS, "condor_history: Failed to write streaming ACK header ad\n");
		exit(1);
	}
  }
  if(readfromfile == true) {
		// some output methods use a whitelist rather than a stringlist projection.
		for (const char * attr = projection.first(); attr != NULL; attr = projection.next()) {
			whitelist.insert(attr);
		}
      // Read from normal history files or Job epoch files if specified
      if ( !read_epoch_ads ) {
      readHistoryFromFiles(fileisuserlog, JobHistoryFileName, my_constraint.c_str(), constraintExpr);
      } else {
      readHistoryFromEpochs(fileisuserlog, JobHistoryFileName, my_constraint.c_str(), constraintExpr);
      }
  }
  else {
      readHistoryRemote(constraintExpr, want_startd_history);
  }
  delete constraintExpr;

  if (writetosocket) {
	ClassAd ad;
	ad.InsertAttr(ATTR_OWNER, 0);
	ad.InsertAttr(ATTR_NUM_MATCHES, matchCount);
	ad.InsertAttr("MalformedAds", writetosocket_failcount);
	ad.InsertAttr("AdCount", adCount);
	dprintf(D_FULLDEBUG, "condor_history: sending final ad:\n");
	dPrintAd(D_FULLDEBUG, ad);
	if ( ! putClassAd(socks[0], ad) || ! socks[0]->end_of_message()) {
		dprintf(D_ALWAYS, "condor_history: Failed to write final ad to client\n");
		exit(1);
	}
	// TJ: if I don't do this. the other end of the connection never gets the final ad....
	ReliSock * sock = (ReliSock*)(socks[0]);
	sock->close();
  }
  return 0;
}

static int getDisplayWidth() {
	if (wide_format_width <= 0) {
		wide_format_width = getConsoleWindowSize();
		if (wide_format_width <= 0)
			wide_format_width = 80;
	}
	return wide_format_width;
}

static void AddPrintColumn(const char * heading, int width, int opts, const char * expr)
{
	mask.set_heading(heading);

	int wid = width ? width : (int)strlen(heading);
	mask.registerFormat("%v", wid, opts, expr);
}

static void AddPrintColumn(const char * heading, int width, int opts, const char * attr, const CustomFormatFn & fmt)
{
	mask.set_heading(heading);
	int wid = width ? width : (int)strlen(heading);
	mask.registerFormat(NULL, wid, opts, fmt, attr);
}

// setup display mask for default output
static void init_default_custom_format()
{
	mask.SetAutoSep(NULL, " ", NULL, "\n");

	int opts = wide_format ? (FormatOptionNoTruncate | FormatOptionAutoWidth) : 0;
	AddPrintColumn(" ID",        -7, FormatOptionNoTruncate | FormatOptionAutoWidth, ATTR_CLUSTER_ID, render_job_id);
	AddPrintColumn("OWNER",     -14, FormatOptionAutoWidth | opts, ATTR_OWNER);
	AddPrintColumn("SUBMITTED",  11,    0, ATTR_Q_DATE, format_real_date);
	AddPrintColumn("RUN_TIME",   12,    0, ATTR_CLUSTER_ID, render_hist_runtime);
	AddPrintColumn("ST",         -2,    0, ATTR_JOB_STATUS, format_job_status_char);
	AddPrintColumn("COMPLETED",  11,    0, ATTR_COMPLETION_DATE, format_real_date);
	AddPrintColumn("CMD",       -15, FormatOptionLeftAlign | FormatOptionNoTruncate, ATTR_JOB_CMD, render_job_cmd_and_args);

	static const char* const attrs[] = {
		ATTR_CLUSTER_ID, ATTR_PROC_ID, ATTR_OWNER, ATTR_JOB_STATUS,
		ATTR_JOB_REMOTE_WALL_CLOCK, ATTR_JOB_REMOTE_USER_CPU,
		ATTR_JOB_CMD, ATTR_JOB_ARGUMENTS1, ATTR_JOB_ARGUMENTS2,
		//ATTR_JOB_DESCRIPTION, "MATCH_EXP_" ATTR_JOB_DESCRIPTION,
		ATTR_Q_DATE, ATTR_COMPLETION_DATE,
	};
	for (int ii = 0; ii < (int)COUNTOF(attrs); ++ii) {
		const char * attr = attrs[ii];
		if ( ! projection.contains_anycase(attr)) projection.append(attr);
	}

	customFormat = TRUE;
}

static void printHeader()
{
	// Print header
	if ( ! longformat) {
		if ( ! customFormat) {
			// hack to get backward-compatible formatting
			if ( ! wide_format && 80 == wide_format_width) {
				short_header();
				// for the legacy format output format, set the projection by hand.
				const char * const attrs[] = {
					ATTR_CLUSTER_ID, ATTR_PROC_ID,
					ATTR_JOB_REMOTE_WALL_CLOCK, ATTR_JOB_REMOTE_USER_CPU,
					ATTR_Q_DATE, ATTR_COMPLETION_DATE,
					ATTR_JOB_STATUS, ATTR_JOB_PRIO,
					ATTR_IMAGE_SIZE, ATTR_MEMORY_USAGE,
					ATTR_OWNER,
					ATTR_JOB_CMD, ATTR_JOB_ARGUMENTS1,
				};
				for (int ii = 0; ii < (int)COUNTOF(attrs); ++ii) {
					const char * attr = attrs[ii];
					if ( ! projection.contains_anycase(attr)) projection.append(attr);
				}
			} else {
				init_default_custom_format();
				if ( ! wide_format || 0 != wide_format_width) {
					int console_width = wide_format_width;
					if (console_width <= 0) console_width = getConsoleWindowSize()-1; // -1 because we get double spacing if we use the full width.
					if (console_width < 0) console_width = 1024;
					mask.SetOverallWidth(console_width);
				}
			}
		}
		if (customFormat && mask.has_headings()) {
			mask.display_Headings(stdout);
		}
	}
	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileHeader(out);
		printf("%s\n", out.c_str());
	} else if( use_json ) {
		printf( "[\n" );
	}
}

static void printFooter()
{
	if(longformat && use_xml) {
		std::string out;
		AddClassAdXMLFileFooter(out);
		printf("%s\n", out.c_str());
	} else if( use_json ) {
		printf( "]\n" );
	}
}

// Read history from a remote schedd or startd
static void readHistoryRemote(classad::ExprTree *constraintExpr, bool want_startd)
{
	printHeader(); // this has the side effect of setting the projection for the default output

	ClassAd ad;
	ad.Insert(ATTR_REQUIREMENTS, constraintExpr);
	ad.InsertAttr(ATTR_NUM_MATCHES, specifiedMatch <= 0 ? -1 : specifiedMatch);
	// in 8.5.6, we can request that the remote side stream the results back. othewise
	// the 8.4 protocol will only send EOM after the last result, and thus we print nothing
	// until all of the results have been received.
	if (streamresults || streamresults_specified) {
		bool want_streamresults = streamresults_specified ? streamresults : true;
		ad.InsertAttr("StreamResults", want_streamresults);
	}
	// only 8.5.6 and later will honor this, older schedd's will just ignore it
	if (sinceExpr) ad.Insert("Since", sinceExpr);
	// we may or may not be able to do the projection, we will decide after knowing the daemon version
	bool do_projection = ! projection.isEmpty();

	daemon_t dt = DT_SCHEDD;
	const char * daemon_type = "schedd";
	int history_cmd = QUERY_SCHEDD_HISTORY;
	if (want_startd) {
		dt = DT_STARTD;
		daemon_type = "startd";
		history_cmd = GET_HISTORY;
	}

	Daemon daemon(dt, g_name.size() ? g_name.c_str() : NULL, g_pool.size() ? g_pool.c_str() : NULL);
	if (!daemon.locate(Daemon::LOCATE_FOR_LOOKUP)) {
		fprintf(stderr, "Unable to locate remote schedd (name=%s, pool=%s).\n", g_name.c_str(), g_pool.c_str());
		exit(1);
	}

	// do some version checks
	if (daemon.version()) {
		CondorVersionInfo v(daemon.version());
		// some versions of the schedd can't handle the projection, we need to check theversion
		if (dt == DT_SCHEDD) {
			if (v.built_since_version(8, 5, 5) || (v.built_since_version(8, 4, 7) && ! v.built_since_version(8, 5, 0))) {
			} else {
				do_projection = false;
			}
		} else if (dt == DT_STARTD) {
			// remote history to the startd was added in 8.9.7
			if (!v.built_since_version(8, 9, 7)) {
				fprintf(stderr, "The version of the startd does not support remote history");
				exit(1);
			}
		}
	}

	if (do_projection) {
		auto_free_ptr proj_string(projection.print_to_delimed_string(","));
		ad.Assign(ATTR_PROJECTION, proj_string.ptr());
	}

	Sock* sock;
	if (!(sock = daemon.startCommand(history_cmd, Stream::reli_sock, 0))) {
		fprintf(stderr, "Unable to send history command to remote %s;\n"
			"Typically, either the %s is not responding, does not authorize you, or does not support remote history.\n", daemon_type, daemon_type);
		exit(1);
	}
	classad_shared_ptr<Sock> sock_sentry(sock);

	if (!putClassAd(sock, ad) || !sock->end_of_message()) {
		fprintf(stderr, "Unable to send request to remote %s; likely a server or network error.\n", daemon_type);
		exit(1);
	}

	bool eom_after_each_ad = false;
	while (true) {
		ClassAd ad;
		if (!getClassAd(sock, ad)) {
			fprintf(stderr, "Failed to receive remote ad.\n");
			exit(1);
		}
		long long intVal;
		if (ad.EvaluateAttrInt(ATTR_OWNER, intVal)) {
			//fprintf(stderr, "Got Owner=%d ad\n", (int)intVal);
			if (!sock->end_of_message()) {
				fprintf(stderr, "Unable to close remote socket.\n");
			}
			if (intVal == 1) { // first ad.
				if ( ! ad.EvaluateAttrBool("StreamResults", eom_after_each_ad)) eom_after_each_ad = true;
				continue;
			} else if (intVal == 0) { // last ad
				sock->close();
				std::string errorMsg;
				if (ad.EvaluateAttrInt(ATTR_ERROR_CODE, intVal) && intVal && ad.EvaluateAttrString(ATTR_ERROR_STRING, errorMsg)) {
					fprintf(stderr, "Error %lld: %s\n", intVal, errorMsg.c_str());
					exit(intVal);
				}
				if (ad.EvaluateAttrInt("MalformedAds", intVal) && intVal) {
					fprintf(stderr, "Remote side had parse errors on history file");
					exit(1);
				}
				if (!ad.EvaluateAttrInt(ATTR_NUM_MATCHES, intVal) || (intVal != matchCount)) {
					fprintf(stderr, "Client and server do not agree on number of ads sent;\n"
						"Indicates lost network packets or an internal error\n");
					exit(1);
				}
			} else {
				fprintf(stderr, "Error: Unexpected Owner=%d\n", (int)intVal);
				exit(1);
			}
			break;
		}
		if (eom_after_each_ad && ! sock->end_of_message()) {
			fprintf(stderr, "failed receive EOM after ad\n");
			exit(1);
		}
		matchCount++;
		printJob(ad);
	}

	printFooter();
}

static bool AddToClassAdList(void* pv, ClassAd* ad) {
	ClassAdList * plist = (ClassAdList*)pv;
	plist->Insert(ad);
	return false; // return false to indicate we took ownership of the ad.
}

// Read the history from the specified history file, or from all the history files.
// There are multiple history files because we do rotation. 
static void readHistoryFromFiles(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr)
{
	printHeader();

    if (JobHistoryFileName) {
        if (fileisuserlog) {
            ClassAdList jobs;
            if ( ! userlog_to_classads(JobHistoryFileName, AddToClassAdList, &jobs, NULL, 0, constraintExpr)) {
                fprintf(stderr, "Error: Can't open userlog %s\n", JobHistoryFileName);
                exit(1);
            }
            printJobAds(jobs);
            jobs.Clear();
        } else {
            // If the user specified the name of the file to read, we read that file only.
            readHistoryFromFileEx(JobHistoryFileName, constraint, constraintExpr, backwards);
        }
    } else {
        // The user didn't specify the name of the file to read, so we read
        // the history file, and any backups (rotated versions). 
        int numHistoryFiles;
        const char **historyFiles;

		const char * knob = want_startd_history ? "STARTD_HISTORY" : "HISTORY";
        historyFiles = findHistoryFiles(knob, &numHistoryFiles);
		if (!historyFiles) {
			fprintf( stderr, "Error: No history file is defined\n");
			fprintf(stderr, "\nExtra Info: The variable %s is not defined in your config file. If you want Condor to "
						   "keep a history of past jobs, you must define %s in your config file\n", knob, knob );
			exit(1);
		}
        if (historyFiles && numHistoryFiles > 0) {
            int fileIndex;
            if (backwards) { // Reverse reading of history files array
                for(fileIndex = numHistoryFiles - 1; fileIndex >= 0; fileIndex--) {
                    readHistoryFromFileEx(historyFiles[fileIndex], constraint, constraintExpr, backwards);
                }
            }
            else {
                for (fileIndex = 0; fileIndex < numHistoryFiles; fileIndex++) {
                    readHistoryFromFileEx(historyFiles[fileIndex], constraint, constraintExpr, backwards);
                }
            }
            freeHistoryFilesList(historyFiles);
        }
    }
    printFooter();
    return;
}

// Check to see if all possible job ads for cluster or cluster.proc have been found
static bool checkMatchJobIdsFound(BannerInfo &banner, ClassAd *ad = NULL) {

	//If we have a job ad and are missing data attempt to populate banner info
	if (ad) {
		if (banner.jid.cluster <= 0)
			ad->LookupInteger(ATTR_CLUSTER_ID,banner.jid.cluster);
		if (banner.jid.proc < 0)
			ad->LookupInteger(ATTR_PROC_ID,banner.jid.proc);
		if (banner.completion < 0)
			ad->LookupInteger(ATTR_COMPLETION_DATE,banner.completion);
		if (read_epoch_ads && banner.runId < 0)
			ad->LookupInteger(ATTR_NUM_SHADOW_STARTS,banner.runId);
	}

	//For each match item info check record found
	for (auto& match : jobIdFilterInfo) {
		//fprintf(stdout,"Clust=%d | Proc=%d | Sub=%lld | Num=%d\n",match.jid.cluster,match.jid.proc,match.QDate,match.numProcs); //Debug Match items
		//If has a specified proc and matched proc and cluster then remove from data structure
		if (match.jid.proc >= 0 && match.jid == banner.jid) {
			//If not an epoch file then found else if epoch file reading backwards and run_instance is 0 then all epoch ads found
			if (!read_epoch_ads || (backwards && banner.runId == 0)) {
				std::swap(match,jobIdFilterInfo.back());
				jobIdFilterInfo.pop_back();
			}
		} else { //Else this is a cluster only find
			//If the cluster submit time is greater than the current completion date remove from data structure
			if (banner.completion > 0 && match.QDate > banner.completion) {
				std::swap(match,jobIdFilterInfo.back());
				jobIdFilterInfo.pop_back();
			} else if (match.jid.cluster == banner.jid.cluster) { //If cluster matches do checks
				//Get QDate from job ad if not set in info
				if (match.QDate < 0) { ad->LookupInteger(ATTR_Q_DATE,match.QDate); }
				//If numProcs is negative then set info to current ads info (TotalSubmitProcs)
				if (match.numProcs < 0) {
					int matchFoundOffset = match.numProcs; //Starts off at -1 and decrements at each match
					if (read_epoch_ads && banner.runId != 0) { ++matchFoundOffset; } //increment because initial assumed match not guaranteed with epochs
					if (!ad->LookupInteger(ATTR_TOTAL_SUBMIT_PROCS,match.numProcs)) {
						if (!read_epoch_ads || (backwards && banner.runId == 0)) {
							match.numProcs = --matchFoundOffset;
						}
					} else {
						match.numProcs += matchFoundOffset;
						if (match.numProcs <= 0) {
							std::swap(match,jobIdFilterInfo.back());
							jobIdFilterInfo.pop_back();
						}
					}
				} else { //If decremented numProcs is 0 then we found all procs in cluster so remove from data structure
					if (!read_epoch_ads || (backwards && banner.runId == 0)) {
						--match.numProcs;
						if (match.numProcs == 0) {
							std::swap(match,jobIdFilterInfo.back());
							jobIdFilterInfo.pop_back();
						}
					}
				}
			}
		}
	}
	//If data structer is empty we have found everything return true else return false
	if (jobIdFilterInfo.empty())
		return true;
	else
		return false;
}

// Read the history from a single file and print it out. 
static void readHistoryFromFileOld(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr)
{
    int EndFlag   = 0;
    int ErrorFlag = 0;
    int EmptyFlag = 0;
    ClassAd *ad = NULL;
    std::string buf;

	int flags = 0;
	if( !backwards ) {
			// Currently, the file position manipulations used in -backwards
			// do not work with files > 2GB on platforms with 32-bit file
			// offsets.
		flags = O_LARGEFILE;
	}
	int LogFd = safe_open_wrapper_follow(JobHistoryFileName,flags,0);
	if (LogFd < 0) {
		fprintf(stderr,"Error opening history file %s: %s\n", JobHistoryFileName,strerror(errno));
#ifdef EFBIG
		if( (errno == EFBIG) && backwards ) {
			fprintf(stderr,"The -backwards option does not support files this large.\n");
		}
#endif
		exit(1);
	}

	FILE *LogFile = fdopen(LogFd,"r");
	if (!LogFile) {
		fprintf(stderr,"Error opening history file %s: %s\n", JobHistoryFileName,strerror(errno));
		exit(1);
	}

	// In case of rotated history files, check if we have already reached the number of 
	// matches specified by the user before reading the next file
	if ((specifiedMatch > 0 && matchCount >= specifiedMatch) || (maxAds > 0 && adCount >= maxAds)) {
		fclose(LogFile);
		return;
	}

    while(!EndFlag) {

        if( !( ad=new ClassAd ) ){
            fprintf( stderr, "Error:  Out of memory\n" );
            exit( 1 );
        }
        InsertFromFile(LogFile,*ad,"***", EndFlag, ErrorFlag, EmptyFlag);
        if( ErrorFlag ) {
            printf( "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
            ErrorFlag=0;
            if(ad) {
                delete ad;
                ad = NULL;
            }
            continue;
        } 
        if( EmptyFlag ) {
            EmptyFlag=0;
            if(ad) {
                delete ad;
                ad = NULL;
            }
            continue;
        }
        if (!constraint || constraint[0]=='\0' || EvalExprBool(ad, constraintExpr)) {
            if (longformat) { 
				if( use_xml ) {
					fPrintAdAsXML(stdout, *ad, projection.isEmpty() ? NULL : &projection);
				} else if ( use_json ) {
					if ( printCount != 0 ) {
						printf(",\n");
					}
					fPrintAdAsJson(stdout, *ad, projection.isEmpty() ? NULL : &projection, false);
				} else if ( use_json_lines ) {
					fPrintAdAsJson(stdout, *ad, projection.isEmpty() ? NULL : &projection, true);
				}
				else {
					fPrintAd(stdout, *ad, false, projection.isEmpty() ? NULL : &projection);
				}
				printf("\n"); 
            } else {
                if (customFormat) {
                    mask.display(stdout, ad);
                } else {
                    displayJobShort(ad);
                }
            }

            printCount++;
            matchCount++; // if control reached here, match has occured

            if (specifiedMatch > 0) { // User specified a match number
                if (matchCount == specifiedMatch) { // Found n number of matches, cleanup  
                    if (ad) {
                        delete ad;
                        ad = NULL;
                    }
                    
                    fclose(LogFile);
                    return;
                }
            }
        }

		if (cluster > 0) { //User specified cluster or cluster.proc.
			int cluster = -1, proc = -1;
			BannerInfo ad_info;
			ad->LookupInteger(ATTR_CLUSTER_ID,cluster);
			ad->LookupInteger(ATTR_PROC_ID,proc);
			ad_info.jid = JOB_ID_KEY(cluster,proc);
			ad->LookupInteger(ATTR_COMPLETION_DATE,ad_info.completion);
			if (checkMatchJobIdsFound(ad_info, ad)) { //Check if all possible matches have been found
				if (ad) {
					delete ad;
					ad = NULL;
				}
				fclose(LogFile);
				return;
			}
		}

        if(ad) {
            delete ad;
            ad = NULL;
        }
    }
    fclose(LogFile);
    return;
}

// return true if p1 starts with p2
// if ppEnd is not NULL, return a pointer to the first non-matching char of p1
static bool starts_with(const char * p1, const char * p2, const char ** ppEnd = NULL)
{
	if ( ! p2 || ! *p2)
		return false;

	const char * p1e = p1;
	const char * p2e = p2;
	while (*p2e) {
		if (*p1e != *p2e)
			return false;
		++p2e; ++p1e;
	}
	if (ppEnd)
		*ppEnd = p1e;
	return true;
}

// print a single job ad
//
static void printJob(ClassAd & ad)
{
	if (writetosocket) {
		if ( ! putClassAd(socks[0], ad, 0, whitelist.empty() ? NULL : &whitelist)) {
			++writetosocket_failcount;
			abort_transfer = true;
		} else if (streamresults && ! socks[0]->end_of_message()) {
			++writetosocket_failcount;
			abort_transfer = true;
		}
		return;
	}

	// NOTE: fPrintAd does not expand the projection, while putClassAd does
	// so the socket and stdout printing are not quite equivalent. We should
	// maybe fix that someday, but as far as I know, the non-socket printing
	// functionality of this code isn't actually used except for debugging.
	if (longformat) {
		if (use_xml) {
			fPrintAdAsXML(stdout, ad, projection.isEmpty() ? NULL : &projection);
		} else if ( use_json ) {
			if ( printCount != 0 ) {
				printf(",\n");
			}
			fPrintAdAsJson(stdout, ad, projection.isEmpty() ? NULL : &projection, false);
		} else if ( use_json_lines ) {
			fPrintAdAsJson(stdout, ad, projection.isEmpty() ? NULL : &projection, true);
		} else {
			fPrintAd(stdout, ad, false, projection.isEmpty() ? NULL : &projection);
		}
		printf("\n");
	} else {
		if (customFormat) {
			mask.display(stdout, &ad);
		} else {
			displayJobShort(&ad);
		}
	}
	printCount++;
}

// convert list of expressions into a classad
//
static void printJobIfConstraint(std::vector<std::string> & exprs, const char* constraint, ExprTree *constraintExpr, BannerInfo& banner)
{
	if ( ! exprs.size())
		return;

	ClassAd ad;
	ad.rehash(521); // big enough to prevent regrowing hash table

	size_t ix;

	// convert lines vector into classad.
	while ((ix = exprs.size()) > 0) {
		if ( ! ad.Insert(exprs[ix-1])) {
			const char * pexpr = exprs[ix-1].c_str();
			dprintf(D_ALWAYS,"condor_history: failed to create classad; bad expr = '%s'\n", pexpr);
			printf( "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
			exprs.clear();
			return;
		}
		exprs.pop_back();
	}
	++adCount;

	if (sinceExpr && EvalExprBool(&ad, sinceExpr)) {
		maxAds = adCount; // this will force us to stop scanning
		return;
	}

	if (!constraint || constraint[0]=='\0' || EvalExprBool(&ad, constraintExpr)) {
		printJob(ad);
		matchCount++; // if control reached here, match has occured
	}
	if (cluster > 0) { //User specified cluster or cluster.proc.
		if (checkMatchJobIdsFound(banner, &ad)) { //Check if all possible ads have been displayed
			maxAds = adCount;
			return;
		}
	}
}

static void printJobAds(ClassAdList & jobs)
{
	jobs.Open();
	ClassAd	*job;
	while (( job = jobs.Next())) {
		printJob(*job);
		if (abort_transfer) break;
	}
	jobs.Close();
}

static bool isvalidattrchar(char ch) { return isalnum(ch) || ch == '_'; }

/*
*	Function to parse banner information of both history and epoch files (They vary slightly)
*	and fill info into Banner info struct. After parsing banner line we will then determine
*	if we are only matching for Cluster, Cluster.Proc, or Owner then check if banner info
*	matches to determine if we parse the upcoming job ad.
*/
static bool parseBanner(BannerInfo& info, std::string banner) {
	//Parse Banner info
	BannerInfo newInfo;

	const char * p = banner.c_str();
	const char * endp = p + banner.size();

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);

	//fprintf(stdout, "parseBanner(%s)\n", p);
	while (*p == '*') ++p;
	while (isspace(*p)) ++p;

	const char * rhs;
	std::string attr;
	while (p < endp && SplitLongFormAttrValue(p, attr, rhs)) {
		int end = 0;
		ExprTree * tree = parser.ParseExpression(rhs);
		if (!tree) { break; }
		//fprintf(stdout, "%s=%s\n", attr.c_str(), ExprTreeToString(tree));
		long long valueNum = 0;
		if (strcasecmp(attr.c_str(),"ClusterId") == MATCH) {
			if (ExprTreeIsLiteralNumber(tree,valueNum))
				if (valueNum <= INT_MAX && valueNum > 0)
					newInfo.jid.cluster = static_cast<int>(valueNum);
		} else if (strcasecmp(attr.c_str(),"ProcId") == MATCH) {
			if (ExprTreeIsLiteralNumber(tree,valueNum))
				if (valueNum <= INT_MAX && valueNum >= 0)
					newInfo.jid.proc = static_cast<int>(valueNum);
		} else if (strcasecmp(attr.c_str(),"RunInstanceId") == MATCH) {
			if (ExprTreeIsLiteralNumber(tree,valueNum))
				if (valueNum <= INT_MAX && valueNum >= 0)
					newInfo.runId = static_cast<int>(valueNum);
		} else if (strcasecmp(attr.c_str(),"Owner") == MATCH) {
			ExprTreeIsLiteralString(tree,newInfo.owner);
		} else if (strcasecmp(attr.c_str(),"CurrentTime") == MATCH || strcasecmp(attr.c_str(),"CompletionDate") == MATCH) {
			if (ExprTreeIsLiteralNumber(tree,valueNum))
				newInfo.completion = valueNum;
		}
		// workaound the fact that the offset we get back from the parser has eaten the next attribute name
		while (end > 0 && isspace(rhs[end-1])) --end;
		while (end > 0 && isvalidattrchar(rhs[end-1])) --end;
		// advance p to point to the next key=value pair
		size_t dist = strcspn(rhs," ");
		p = rhs + dist;
		while (isspace(*p)) ++p;
	}
	info = newInfo;
	//For testing output of banner
	//fprintf(stdout,"Parsed banner info: %s %d.%d | Comp: %lld | Epoch: %d\n",info.owner.c_str(),info.jid.cluster, info.jid.proc, info.completion, info.runId);

	if(jobIdFilterInfo.empty() && ownersList.empty()) { return true; } //If no searches were specified then return true to print job ad
	else if (info.jid.cluster <= 0 && !jobIdFilterInfo.empty()) { return true; } //If failed to get cluster info and we are searching for job id info return true
	else if (info.owner.empty() && !ownersList.empty()) { return true; }//If failed to parse owner and we are searching for an owner return true

	//Check to see if cluster exists in matching job info
	for(auto& item : jobIdFilterInfo) { if(info.jid.cluster == item.jid.cluster){ return true; } }
	//Check to see if owner is being searched for
	for(auto& name : ownersList) { if(strcasecmp(info.owner.c_str(),name.c_str()) == MATCH){ return true; } }
	//If here then no match
	return false;
}

static void readHistoryFromFileEx(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr, bool read_backwards)
{
	// In case of rotated history files, check if we have already reached the number of 
	// matches specified by the user before reading the next file
	if ((specifiedMatch > 0 && matchCount >= specifiedMatch) || (maxAds > 0 && adCount >= maxAds)) {
		return;
	}

	// the old function doesn't work for backwards, but it does work for forwards so go ahead and call it.
	//
	if ( ! read_backwards) {
		readHistoryFromFileOld(JobHistoryFileName, constraint, constraintExpr);
		return;
	}

	// do backwards reading.
	BackwardFileReader reader(JobHistoryFileName, O_RDONLY);
	if (reader.LastError()) {
		// report error??
		fprintf(stderr,"Error opening history file %s: %s\n", JobHistoryFileName,strerror(reader.LastError()));
		exit(1);
	}

	std::string line;        // holds the current line from the log file.
	std::string banner_line; // the contents of the "*** " banner line for the job we are scanning

	// the last line in the file should be a "*** " banner line.
	// we want to scan backwards until we find it, what is above that in the file is the job
	// information for that banner line.
	while (reader.PrevLine(line)) {
		if (starts_with(line.c_str(), "*** ")) {
			banner_line = line;
			break;
		}
	}

	BannerInfo curr_banner;
	bool read_ad = parseBanner(curr_banner, banner_line);
	std::vector<std::string> exprs;
	while (reader.PrevLine(line)) {

		// the banner is at the end of the job information, so when we get to on, we 
		// know that we are done accumulating expressions into the vector.
		if (starts_with(line.c_str(), "*** ")) {

			if (exprs.size() > 0) {
				printJobIfConstraint(exprs, constraint, constraintExpr, curr_banner);
				exprs.clear();
			} else if (cluster > 0 && checkMatchJobIdsFound(curr_banner)){
				//If we don't print an ad we can still check for completion dates vs QDates
				//for done jobs. If function returns true then we are done
				break;
			}

			// the current line is the banner that starts (ends) the next job record
			banner_line = line;
			//Parse banner for next ad
			read_ad = parseBanner(curr_banner, banner_line);
			// if we already hit our match count, we can stop now.
			if ((specifiedMatch > 0 && matchCount >= specifiedMatch) || (maxAds > 0 && adCount >= maxAds))
				break;
			if (abort_transfer)
				break;

		} else {

			// we have to parse the lines in from the start of the file to the end
			// to handle chained ads correctly, so here we just push the lines into
			// a vector as they arrive.  note that this puts them in the vector backwards
			// comments can be discarded at this point.
			if (!read_ad) { continue; }

			if ( ! line.empty()) {
				const char * psz = line.c_str();
				while (*psz == ' ' || *psz == '\t') ++psz;
				if (*psz != '#') {
					exprs.push_back(line);
				}
			}
		}
	}

	// when we hit the start of the file, we may still have 1 job record to print out.
	if (exprs.size() > 0) {
		if ((specifiedMatch > 0 && matchCount >= specifiedMatch) || (maxAds > 0 && adCount >= maxAds)) {
			// do nothing
		} else {
			printJobIfConstraint(exprs, constraint, constraintExpr, curr_banner);
		}
		exprs.clear();
	}

	reader.Close();
}

//PRAGMA_REMIND("tj: TODO fix to handle summary print format")
static int set_print_mask_from_stream(
	AttrListPrintMask & print_mask,
	std::string & constraint,
	StringList & attrs,
	const char * streamid,
	bool is_filename)
{
	std::string messages;
	PrintMaskMakeSettings propt;
	std::vector<GroupByKeyInfo> group_by_keys;
	SimpleInputStream * pstream = NULL;

	FILE *file = NULL;
	if (MATCH == strcmp("-", streamid)) {
		pstream = new SimpleFileInputStream(stdin, false);
	} else if (is_filename) {
		file = safe_fopen_wrapper_follow(streamid, "r");
		if (file == NULL) {
			fprintf(stderr, "Can't open select file: %s\n", streamid);
			return -1;
		}
		pstream = new SimpleFileInputStream(file, true);
	} else {
		pstream = new StringLiteralInputStream(streamid);
	}
	ASSERT(pstream);

	int err = SetAttrListPrintMaskFromStream(
					*pstream,
					NULL,
					print_mask,
					propt,
					group_by_keys,
					NULL,
					messages);
	delete pstream; pstream = NULL;
	if ( ! err) {
		customFormat = true;
		constraint = propt.where_expression;
		if ( ! constraint.empty()) {
			ExprTree *constraintExpr=NULL;
			if ( ! ParseClassAdRvalExpr(constraint.c_str(), constraintExpr)) {
				formatstr_cat(messages, "WHERE expression is not valid: %s\n", constraint.c_str());
				err = -1;
			} else {
				delete constraintExpr;
			}
		}
		if (propt.aggregate) {
			formatstr_cat(messages, "UNIQUE aggregation is not supported.\n");
			err = -1;
		}
		initStringListFromAttrs(attrs, false, propt.attrs);
	}
	if ( ! messages.empty()) { fprintf(stderr, "%s", messages.c_str()); }
	return err;
}

//Find and add all epoch files in the given epochDirectory then sort
//based on cluster.proc & backwards/forwards
static void findEpochFiles(std::deque<std::string> *epochFiles){
	std::string jobID; //Make jobId to search for .XXX.YY.
	bool oneJob = false;
	if (cluster > 0) {
		if (proc >= 0) { formatstr(jobID,".%d.%d.",cluster,proc); oneJob = true; }
		else { formatstr(jobID,".%d.",cluster); }
	}

	//Get all the epoch ads we want
	Directory dir(epochDirectory);
	std::string file;
	do {
		//Get file name in directory
		const char *curr = dir.Next();
		file = curr ? curr : "";
		//Check if filename matched job.runs.X.Y.ads
		if (starts_with(file,"job.runs.") && ends_with(file,".ads")) {
			//If no jobID then add all matching epoch ads
			if (jobID == "") {
				epochFiles->push_back(file);
			} else { //Otherwise filter based on if the jobID .XXX.YY. is found in the filename
				if (file.find(jobID) != std::string::npos) {
					epochFiles->push_back(file);
					if (oneJob) return;
				}
			}
		}
	} while(file != "");

	//If backwards then reverse the data structure
	if (backwards) { std::reverse(epochFiles->begin(),epochFiles->end()); }
}

static void readHistoryFromEpochs(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr){
	printHeader();

	//If we were passed a specific file name then read that.
	if (JobHistoryFileName) {
		if (fileisuserlog) {
			ClassAdList jobs;
			if ( ! userlog_to_classads(JobHistoryFileName, AddToClassAdList, &jobs, NULL, 0, constraintExpr)) {
				fprintf(stderr, "Error: Can't open userlog %s\n", JobHistoryFileName);
				exit(1);
			}
			printJobAds(jobs);
			jobs.Clear();
		} else {
			// If the user specified the name of the file to read, we read that file only.
			readHistoryFromFileEx(JobHistoryFileName, constraint, constraintExpr, backwards);
		}
	} else {
		// The user didn't specify the name of the file to read, so we read
		// Job Epoch (Run Instance) files for job ads

		//Make sure match,limit,and/or scanlimit aren't being used with delete function
		if (maxAds != -1 && delete_epoch_ads) {
			fprintf(stderr,"Error: -epoch:d (delete) cannot be used with -scanlimit.\n");
			exit(1);
		}
		else if (specifiedMatch != -1 && delete_epoch_ads) {
			fprintf(stderr,"Error: -epoch:d (delete) cannot be used with -limit or -match.\n");
			exit(1);
		}

		std::deque<std::string> epochFiles;
		findEpochFiles(&epochFiles);
		//For each file found read job ads
		for(auto file : epochFiles) {
			std::string file_path;
			//Make full path (path+file_name) to read
			if (delete_epoch_ads) {
				/*	If deleting the files then rename the file as before reading
				*	This is to try and prevent a race condition between writing
				*	to the epoch file and reading/deleting it - Cole Bollig 2022-09-13
				*/
				std::string old_name,new_name,base = "READING.txt";
				dircat(epochDirectory,file.c_str(),old_name);
				dircat(epochDirectory,base.c_str(),new_name);
				rename(old_name.c_str(),new_name.c_str());
				dircat(epochDirectory,base.c_str(),file_path);
			} else {
				dircat(epochDirectory,file.c_str(),file_path);
			}
			//Read file
			readHistoryFromFileEx(file_path.c_str(), constraint, constraintExpr, backwards);
			//If deleting then delete the file that was just read.
			if (delete_epoch_ads) {
				remove(file_path.c_str());
			}
		}
	}
	printFooter();
	return;
}

