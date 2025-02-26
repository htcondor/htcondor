/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
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
#include <utility> // for std::move

#include "classad_helpers.h"
#include "history_utils.h"
#include "backward_file_reader.h"
#include <fcntl.h>  // for O_BINARY

void Usage(const char* name, int iExitCode=1);

void Usage(const char* name, int iExitCode) 
{
	printf ("Usage: %s [source] [restriction-list] [options]\n"
		"\n   where [source] is one of\n"
		"\t-file <file>\t\tRead history data from specified file\n"
		"\t-search <path>\t\tRead history data from all matching HTCondor time rotated files\n"
		"\t-local\t\t\tRead history data from the configured files\n"
		"\t-schedd\t\tRead history data from Schedd (default)"
		"\t-startd\t\t\tRead history data for the Startd\n"
		"\t-epochs\t\t\tRead epoch (per job run instance) history data\n"
		"\t-userlog <file>\t\tRead job data specified userlog file\n"
		"\t-directory\t\tRead history data from per job epoch history directory"
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
		"\t-extract <file>\t\tCopy historical ClassAd entries into the specified file\n"
		, name);
  exit(iExitCode);
}

static void readHistoryRemote(classad::ExprTree *constraintExpr, bool want_startd=false, bool read_dir=false);
static void readHistoryFromFiles(const char* matchFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromDirectory(const char* searchDirectory, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromSingleFile(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromFileOld(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr);
static void readHistoryFromFileEx(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr, bool read_backwards);
static void printJobAds(ClassAdList & jobs);
static void printJob(ClassAd & ad);

static int set_print_mask_from_stream(AttrListPrintMask & print_mask, std::string & constraint, classad::References & attrs, const char * streamid, bool is_filename);
static int getDisplayWidth();

//------------------------------------------------------------------------
//Structure to hold info needed for ending search once all matches are found
struct ClusterMatchInfo {
	time_t QDate = -1; //QDate of cluster (same for all procs) to be compared against CompletionDate
	JOB_ID_KEY jid;    //Cluster & Proc info for job
	int numProcs = -1; //TotalSubmitProcs of cluster (same for all procs) to be counted for found cluster procs
	bool isDoneMatching = false;
};
//Structure to hold banner information for cluster/proc matching
struct BannerInfo {
	time_t completion = -1; //Ad completion time
	JOB_ID_KEY jid;         //Cluster & Proc info for job
	int runId = -1;         //Job epoch < 0 = no epochs
	std::string owner = ""; //Job Owner
	std::string ad_type;    //Ad Type (Not equivalent to MyType)
	std::string line;       // Line parsed for current banner info
};
// What kind of source file we are reading ads from
enum HistoryRecordSource {
	HRS_AUTO = -1,          //Base value if not overwritten will default to schedd or startd history
	HRS_SCHEDD_JOB_HIST,    //Standard job history from Schedd
	HRS_STARTD_HIST,        //Standard job history from a startd
	HRS_JOB_EPOCH,          //Job Epoch (run instance) history
};

//Source information: Holds Source knob in above enum order
static const char* source_knobs[] = {"HISTORY","STARTD_HISTORY","JOB_EPOCH_HISTORY"};
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
static classad::References projection;
static ExprTree *sinceExpr = NULL;
static bool want_startd_history = false;
static bool delete_epoch_ads = false;
static std::deque<ClusterMatchInfo> jobIdFilterInfo;
static std::deque<std::string> ownersList;
static std::set<std::string> filterAdTypes; // Allow filter of Ad types specified in history banner (different from MyType)
static HistoryRecordSource recordSrc = HRS_AUTO;

static std::deque<std::string> historyCopyAds;
static const char* extractionFile = nullptr;
static FILE* extractionFP = nullptr;

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
	std::vector<std::string> remaining_items; // for the remainder which we expect to be empty.
	int cSocks = extractInheritedSocks(inherit, ppid, psinful, socks, (int)cMaxSocks, remaining_items);
	UnsetEnv(envName); // prevent this from being passed on to children.
	return cSocks;
}

//Condense the match filter info based on jid information to the minimal checks we need
static void condenseJobFilterList(bool sort = false) {
	if (sort) {
		//Sort info by jid from lowest to highest (Note: Just clusters will be first with JID = X.-1)
		std::sort(jobIdFilterInfo.begin(),jobIdFilterInfo.end(),
				  [](const ClusterMatchInfo& left, const ClusterMatchInfo& right) { return left.jid < right.jid; });
		//Here we check for a cluster search and any following cluster.proc searches with the same ClusterId
		int searchCluster = -1;
		for(auto& item : jobIdFilterInfo) {
			//If search cluster = curr filter items cluster and a procid is set then mark as done
			if (searchCluster == item.jid.cluster) {
				if (item.jid.proc >= 0) {
					item.isDoneMatching = true;
				}
			} else if (item.jid.proc < 0) { //New clusterid check if ther is no proc (just cluster match)
				searchCluster = item.jid.cluster;
			}
		}
	}

	// Erase JobId filters that are completed (All possible Cluster.Procs found)
	std::erase_if(jobIdFilterInfo, [](const ClusterMatchInfo& inf){ return inf.isDoneMatching; });
}

//Use passed info to determine if we can set a record source: do so then return or error out
static void SetRecordSource(HistoryRecordSource src, const char* curr_flag, const char* new_flag) {
	ASSERT(src != HRS_AUTO); //Dont set source to AUTO
	//Check if we have already set a source with a flag
	if (curr_flag && recordSrc != src) {
		fprintf(stderr, "Error: %s can not be used in association with %s.\n", curr_flag, new_flag);
		exit(1);
	} else if (recordSrc == src) { return; }
	//check to make sure recordSrc isn't pre set
	if (recordSrc == HRS_AUTO) {
		recordSrc = src;
		return;
	}
	//Something is wrong at this point no previous user setting of source
	//but source enum is set to a value already (future proofing)
	fprintf(stderr,"Error: Failed to set history record source with %s flag.\n", new_flag);
	exit(1);
}

int
main(int argc, const char* argv[])
{
  const char *owner=NULL;
  bool readfromfile = true;
  bool fileisuserlog = false;
  bool dash_local = false; // set if -local is passed
  bool readFromDir = false;

  const char* JobHistoryFileName=NULL;
  const char* searchPath=NULL;
  const char* setRecordSrcFlag=NULL;
  const char * pcolon=NULL;
  auto_free_ptr matchFileName;
  auto_free_ptr searchDirectory;

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
		backwards=FALSE;
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
		SetRecordSource(HRS_STARTD_HIST, setRecordSrcFlag, "-startd");
		setRecordSrcFlag = argv[i];
	}
	else if (is_dash_arg_prefix(argv[i],"schedd",3)) {
		// causes "HISTORY" to be queried, this is the default
		want_startd_history = false;
		SetRecordSource(HRS_SCHEDD_JOB_HIST, setRecordSrcFlag, "-schedd");
		setRecordSrcFlag = argv[i];
	}
	else if (is_dash_arg_colon_prefix(argv[i],"stream-results", &pcolon, 6)) { // Purposefully undocumented (used by history helper)
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
	else if (is_dash_arg_prefix(argv[i],"inherit",-1)) { // Purposefully undocumented (used by history helper)

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
		projection = SplitAttrNames(argv[i]);
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
			fprintf (stderr, "Error: Argument %s requires at least one attribute parameter\n", argv[i]);
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
		projection.insert(refs.begin(), refs.end());
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
	else if (is_dash_arg_colon_prefix(argv[i], "epochs", &pcolon, 2)) {
		SetRecordSource(HRS_JOB_EPOCH, setRecordSrcFlag, "-epochs");
		setRecordSrcFlag = argv[i];
		searchDirectory.clear();
		matchFileName.clear();
		//Get aggregate epoch history file
		matchFileName.set(param("JOB_EPOCH_HISTORY"));
		//Get epoch directory
		searchDirectory.set(param("JOB_EPOCH_HISTORY_DIR"));
		if (!matchFileName && !searchDirectory) {
			fprintf( stderr, "Error: No Job Run Instance recordings to read.\n");
			exit(1);
		}
		//Check for :d option (Only applies for epoch dir files)
		while ( pcolon && *pcolon != '\0' ) {
			++pcolon;
			if ( *pcolon == 'd' || *pcolon == 'D' ) { delete_epoch_ads = true; break; }
		}
	}
	else if (is_dash_arg_prefix(argv[i], "type", 1)) { // Purposefully undocumented (Intended internal use)
		if (argc <= i+1 || *(argv[i+1]) == '-') {
			fprintf(stderr, "Error: Argument %s requires another parameter\n", argv[i]);
			exit(1);
		}
		++i; // Claim the next argument for this option even if we don't use it
		// Process passed Filter Ad Types if 'ALL' has not been specified
		if ( ! filterAdTypes.contains("ALL")) {
			StringTokenIterator adTypes(argv[i]);
			for (auto type : adTypes) {
				upper_case(type);
				if (type == "ALL") {
					// 'ALL' will not filter out any ad types so add to set and break
					filterAdTypes.insert(type);
					break;
				} else if (type == "TRANSFER") {
					// Special handling to specify Plugin Transfer return ads
					std::set<std::string> xferTypes{"INPUT", "OUTPUT", "CHECKPOINT"};
					filterAdTypes.merge(xferTypes);
				} else { // Insert specified type
					filterAdTypes.insert(type);
				}
			}
		}
	}
	else if (is_dash_arg_prefix(argv[i],"directory",3)) {
		readFromDir = true;
	}
	else if (is_dash_arg_prefix(argv[i],"search",3)) {
		if (argc <= i+1) {
			fprintf( stderr, "Error: Argument %s requires another parameter\n", argv[i]);
			exit(1);
		}
		searchPath = argv[++i];
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
	else if (is_dash_arg_prefix(argv[i], "extract", 2)) {
		i++;
		if (argc <= i) {
			fprintf(stderr, "Error: Argument -extract requires another parameter.\n");
			exit(1);
		}

		extractionFile = argv[i];
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
    else if (is_dash_arg_colon_prefix(argv[i],"debug",&pcolon,3)) {
          // dprintf to console
          dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
    }
    else if (is_dash_arg_prefix(argv[i],"diagnostic",4)) { // Purposefully undocumented (Intended internal use)
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

  condenseJobFilterList(true);

  //If record source is still AUTO then set to original history based on want_startd_history
  if (recordSrc == HRS_AUTO) {
    recordSrc = want_startd_history ? HRS_STARTD_HIST : HRS_SCHEDD_JOB_HIST;
  }

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

  if (extractionFile) {
	if ( ! readfromfile) {
		fprintf(stderr, "Error: -extract can only be used when reading directly from a history file\n");
		exit(1);
	} else if (my_constraint.empty()) {
		fprintf(stderr, "Error: -extract requires a constraint for which ClassAds to copy\n");
		exit(1);
	}
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
      // Set Default expected Ad type to be filtered for display per history source
      if (filterAdTypes.empty()) {
          switch(recordSrc) {
              case HRS_SCHEDD_JOB_HIST:
              case HRS_STARTD_HIST:
                  filterAdTypes.insert("JOB");
                  break;
              case HRS_JOB_EPOCH:
                  filterAdTypes.insert("EPOCH");
                  break;
              case HRS_AUTO:
              default:
                  fprintf(stderr,
                          "Error: Invalid history record source (%d) selected for default ad types.\n",
                          (int)recordSrc);
                  exit(1);
          }
      }

      // Attempt to open extraction file before doing laborous reading of history
      if (extractionFile) {
          extractionFP = safe_fopen_wrapper_follow(extractionFile, "w");
          if ( ! extractionFP) {
              fprintf(stderr, "Error: Failed ot open extraction file %s (%d): %s\n",
                      extractionFile, errno, strerror(errno));
              exit(1);
          }
      }

      // Read from single file, matching files, or a directory (if valid option)
      if (JobHistoryFileName) { //Single file to be read passed
      readHistoryFromSingleFile(fileisuserlog, JobHistoryFileName, my_constraint.c_str(), constraintExpr);
      } else if (readFromDir) { //Searching for files in a directory
      readHistoryFromDirectory(searchPath ? searchPath : searchDirectory, my_constraint.c_str(), constraintExpr);
      } else { //Normal search with files
      readHistoryFromFiles(searchPath ? searchPath : matchFileName, my_constraint.c_str(), constraintExpr);
      }
  }
  else {
      readHistoryRemote(constraintExpr, want_startd_history, readFromDir);
  }
  delete constraintExpr;

  if (extractionFile) {
	ASSERT(extractionFP);
	if (backwards) {
		fprintf(stdout, "\nWriting %zu matched ads to %s...\n", historyCopyAds.size(), extractionFile);
		for (const auto& ad : historyCopyAds) {
			size_t bytes = fwrite(ad.c_str(), sizeof(char), ad.size(), extractionFP);
			if (bytes != ad.size()) {
				fprintf(stderr, "Warning: Failed to write ad to extraction file %s\n", extractionFile);
			}
		}
	}
	fclose(extractionFP);
  }

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
		projection.emplace(attrs[ii]);
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
					projection.emplace(attrs[ii]);
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
static void readHistoryRemote(classad::ExprTree *constraintExpr, bool want_startd, bool read_dir)
{
	ASSERT(recordSrc != HRS_AUTO);
	printHeader(); // this has the side effect of setting the projection for the default output

	ClassAd ad;
	if (constraintExpr) { ad.Insert(ATTR_REQUIREMENTS, constraintExpr->Copy()); }
	ad.InsertAttr(ATTR_NUM_MATCHES, specifiedMatch <= 0 ? -1 : specifiedMatch);
	// in 8.5.6, we can request that the remote side stream the results back. othewise
	// the 8.4 protocol will only send EOM after the last result, and thus we print nothing
	// until all of the results have been received.
	if (streamresults || streamresults_specified) {
		bool want_streamresults = streamresults_specified ? streamresults : true;
		ad.InsertAttr("StreamResults", want_streamresults);
	}
	if ( ! backwards) { ad.InsertAttr("HistoryReadForwards", true); }
	if (maxAds >= 0) { ad.InsertAttr("ScanLimit", maxAds); }
	// only 8.5.6 and later will honor this, older schedd's will just ignore it
	if (sinceExpr) ad.Insert("Since", sinceExpr);
	// we may or may not be able to do the projection, we will decide after knowing the daemon version
	bool do_projection = ! projection.empty();

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
		fprintf(stderr, "Unable to locate remote %s (name=%s, pool=%s).\n", daemon_type, g_name.c_str(), g_pool.c_str());
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
				fprintf(stderr, "The version of the startd does not support remote history.\n");
				exit(1);
			}
		}
		//Do Record source version checks here and assign needed Ad information
		std::string err_msg;
		switch (recordSrc) {
			//Nothing special needed for HISTORY or STARTD_HISTORY at the moment
			case HRS_SCHEDD_JOB_HIST:
				break;
			case HRS_STARTD_HIST:
				ad.InsertAttr(ATTR_HISTORY_RECORD_SOURCE,"STARTD");
				break;
			case HRS_JOB_EPOCH:
				if (v.built_since_version(10, 3, 0)) {
					ad.InsertAttr(ATTR_HISTORY_RECORD_SOURCE,"JOB_EPOCH");
					if (read_dir) { ad.InsertAttr("HistoryFromDir",true); }
				} else {
					formatstr(err_msg, "The remote schedd (%s) version does not support remote job epoch history.",g_name.c_str());
				}
				break;
			default:
				err_msg = "No history record source declared. Unsure what ad history to remotely query.";
				break;
		}
		if (!err_msg.empty()) {
			fprintf(stderr,"Error: %s\n",err_msg.c_str());
			exit(1);
		}
		// Add Ad type filter if remote version supports it
		if ( ! filterAdTypes.empty()) {
			if ( ! v.built_since_version(23, 8, 0)) {
				const char* daemonType = (dt == DT_SCHEDD) ? "Schedd" : "StartD";
				fprintf(stderr, "Remote %s does not support -type filering for history queries.\n", daemonType);
				exit(1);
			}
			auto join = [](std::string list, std::string type) -> std::string { return std::move(list) + ',' + type; };
			std::string adFilter = std::accumulate(std::next(filterAdTypes.begin()), filterAdTypes.end(), *(filterAdTypes.begin()), join);
			ad.InsertAttr(ATTR_HISTORY_AD_TYPE_FILTER, adFilter);
		}
	}

	if (do_projection) {
		ad.Assign(ATTR_PROJECTION, JoinAttrNames(projection, ","));
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
static void readHistoryFromFiles(const char* matchFileName, const char* constraint, ExprTree *constraintExpr)
{
	ASSERT(recordSrc != HRS_AUTO);
	printHeader();
	// Default to search for standard job ad history if no files specified
	const char* knob = source_knobs[recordSrc];
	auto_free_ptr origHistory;
	if (!matchFileName && (recordSrc == HRS_SCHEDD_JOB_HIST || recordSrc == HRS_STARTD_HIST)) {
		origHistory.set(param(knob));
		matchFileName = origHistory;
	}

	// This is the last check for history records. If files is empty then nothing to read exit
	if (!matchFileName) {
		fprintf(stderr, "Error: No passed search file and configuration key %s is unset.\n", knob);
		fprintf(stderr, "\nExtra Info: The variable %s is not defined in your config file. If you want Condor to "
						"keep a history of information, you must define %s in your config file\n", knob, knob);
		exit(1);
	}

	// Find all time rotated files matching the filename in its given directory
	std::vector<std::string> historyFiles = findHistoryFiles(matchFileName);

	if (backwards) { std::reverse(historyFiles.begin(), historyFiles.end()); }// Reverse reading of history files vector
	//Debugging code: Display found files in vector order
	//for(auto file : historyFiles) { fprintf(stdout, "%s\n",file.c_str()); }

	// Read files for Ads in order
	for(const auto &file : historyFiles) {
		readHistoryFromFileEx(file.c_str(), constraint, constraintExpr, backwards);
	}

	printFooter();
	return;
}

/*	Function to take a history record sources delimiting banner line and extract
*	the 'Ad Type' (this is different from MyType ad attribute). A banner should
*	always be: '*** Adtype Key=Value Key=Value...' where AdType is optional but
*	If no ad type is found (i.e. older history files) then assume type is standard job ad.
*	@return char* to "begining" of key=value pairs
*
*	'*** AdType Key=Value Key=Value' -> 'AdType' return ' Key=value Key=Value'
*	'*** Key=Value Key=Value' -> 'JOB' return 'Key=Value Key=Value'
*/
static const char* getAdTypeFromBanner(std::string& banner, std::string& ad_type) {
	//Get position of equal sign for first key=value pair
	size_t pos_firstEql = banner.find("=");
	if (pos_firstEql == std::string::npos) { return NULL; }
	const char * p = banner.c_str();
	const char * endp = p + pos_firstEql;

	//Clear start & end pointers of whitespace and '='/'*'
	while (*p == '*') ++p;
	while (isspace(*p)) ++p;
	if (*endp == '=') --endp;
	while (p < endp && isspace(*endp)) --endp;

	//Check for a whitespace between pointers
	size_t len = endp - p;
	std::string temp(p, len);
	//fprintf(stdout, "Reduced banner:%s\n",temp.c_str());
	//If no whitespace (space/tab) then no specified Type assume standard Job
	if (temp.find_first_of(" \t") == std::string::npos) { ad_type = "JOB"; return p; }

	//Make current pointer = start pointer and increment pointer until
	//it is equal to end pointer or whitespace is found
	const char * curp = p;
	while (curp != endp) {
		++curp;
		if (isspace(*curp)) {
			endp = curp; //Set true end pointer to current pointer
		}
	}

	//Get type string len and copy data to ad_type
	len = endp - p;
	ad_type.clear();
	ad_type.insert(0, p, len);
	return endp;
}

static bool parseBanner(BannerInfo& info, std::string banner);

//History source files that we expect to only contain 1 instance of a Job Ad
static bool hasOneJobInstInFile() {
	return recordSrc == HRS_SCHEDD_JOB_HIST || recordSrc == HRS_STARTD_HIST;
}

// Check to see if all possible job ads for cluster or cluster.proc have been found
static bool checkMatchJobIdsFound(BannerInfo &banner, ClassAd *ad = NULL, bool onlyCheckTime = false) {

	//If we have a job ad and are missing data attempt to populate banner info
	if (ad) {
		if (banner.jid.cluster <= 0)
			ad->LookupInteger(ATTR_CLUSTER_ID,banner.jid.cluster);
		if (banner.jid.proc < 0)
			ad->LookupInteger(ATTR_PROC_ID,banner.jid.proc);
		if (banner.completion < 0)
			ad->LookupInteger(ATTR_COMPLETION_DATE,banner.completion);
		if (recordSrc == HRS_JOB_EPOCH && banner.runId < 0)
			ad->LookupInteger(ATTR_NUM_SHADOW_STARTS,banner.runId);
	}

	//For each match item info check record found
	for (auto& match : jobIdFilterInfo) {
		//fprintf(stdout,"Clust=%d | Proc=%d | Sub=%lld | Num=%d | OCT=%s\n",match.jid.cluster,match.jid.proc,match.QDate,match.numProcs,onlyCheckTime ? "true" : "false"); //Debug Match items
		if (match.jid.cluster == banner.jid.cluster) { //If cluster matches do checks
			//Get QDate from job ad if not set in info
			if (match.QDate < 0 && ad) { ad->LookupInteger(ATTR_Q_DATE,match.QDate); }
			if (!onlyCheckTime) {
				if (match.jid.proc >= 0) {
					//If has a specified proc and matched proc and cluster then remove from data structure
					//If not an epoch file then found else if epoch file reading backwards and run_instance is 0 then all epoch ads found
					if (match.jid == banner.jid && (hasOneJobInstInFile() || (backwards && banner.runId == 0))) {
						match.isDoneMatching = true;
						onlyCheckTime = true;
						continue;
					}
				} else { //Else this is a cluster only find
					//If numProcs is negative then set info to current ads info (TotalSubmitProcs)
					if (match.numProcs < 0) {
						int matchFoundOffset = match.numProcs; //Starts off at -1 and decrements at each match
						if (recordSrc == HRS_JOB_EPOCH && banner.runId != 0) { ++matchFoundOffset; } //increment because initial assumed match not guaranteed with epochs
						if (!ad || !ad->LookupInteger(ATTR_TOTAL_SUBMIT_PROCS,match.numProcs)) {
							if (hasOneJobInstInFile() || (backwards && banner.runId == 0)) {
								match.numProcs = --matchFoundOffset;
							}
						} else {
							match.numProcs += matchFoundOffset;
							if (match.numProcs <= 0) {
								match.isDoneMatching = true;
							}
						}
					} else { //If decremented numProcs is 0 then we found all procs in cluster so remove from data structure
						if (hasOneJobInstInFile() || (backwards && banner.runId == 0)) {
							if (--match.numProcs == 0) {
								match.isDoneMatching = true;
							}
						}
					}
					onlyCheckTime = true;
					continue;
				}
			}
		}
		//If the cluster submit time is greater than the current completion date remove from data structure
		if (banner.completion > 0 && match.QDate > banner.completion) {
			match.isDoneMatching = true;
		}
	}

	//Remove all match jid's done searching
	condenseJobFilterList();

	//If all filter matches have been found return true else return false
	if (jobIdFilterInfo.empty())
		return true;
	else
		return false;
}

static bool printJobIfConstraint(ClassAd &ad, const char* constraint, ExprTree *constraintExpr, BannerInfo& banner);

// Read the history from a single file and print it out. 
static void readHistoryFromFileOld(const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr)
{
    bool EndFlag  = false;
    int ErrorFlag = 0;
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

	CondorClassAdFileParseHelper helper("***");
	CompatFileLexerSource LogSource(LogFile, false);

    ClassAd ad;
    while(!EndFlag) {

		ad.Clear();
        int c_attrs = InsertFromStream(LogSource, ad, EndFlag, ErrorFlag, &helper);
        std::string banner(helper.getDelimitorLine());
        if( ErrorFlag ) {
            printf( "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
            ErrorFlag=0;
			ad.Clear();
            continue;
        } 
        //If no attribute were read during insertion reset ad and continue
        if( c_attrs <= 0 ) {
			ad.Clear();
            continue;
        }

		BannerInfo ad_info;
		parseBanner(ad_info, banner);
		bool done = printJobIfConstraint(ad, constraint, constraintExpr, ad_info);

		ad.Clear();

		if (done || (specifiedMatch > 0 && matchCount >= specifiedMatch) || (maxAds > 0 && adCount >= maxAds)) {
			break;
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
		if ( ! putClassAd(socks[0], ad, 0, projection.empty() ? NULL : &projection)) {
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
			fPrintAdAsXML(stdout, ad, projection.empty() ? NULL : &projection);
		} else if ( use_json ) {
			if ( printCount != 0 ) {
				printf(",\n");
			}
			fPrintAdAsJson(stdout, ad, projection.empty() ? NULL : &projection, false);
		} else if ( use_json_lines ) {
			fPrintAdAsJson(stdout, ad, projection.empty() ? NULL : &projection, true);
		} else {
			fPrintAd(stdout, ad, false, projection.empty() ? NULL : &projection);
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
	printJobIfConstraint(ad, constraint, constraintExpr, banner);
}

static bool printJobIfConstraint(ClassAd &ad, const char* constraint, ExprTree *constraintExpr, BannerInfo& banner)
{
	++adCount;

	if (sinceExpr && EvalExprBool(&ad, sinceExpr)) {
		maxAds = adCount; // this will force us to stop scanning
		return true;
	}

	if (!constraint || constraint[0]=='\0' || EvalExprBool(&ad, constraintExpr)) {
		printJob(ad);
		matchCount++; // if control reached here, match has occured
		if (extractionFile) {
			if (backwards) {
				std::string& copy = historyCopyAds.emplace_front();
				sPrintAd(copy, ad);
				copy += banner.line + "\n";
			} else {
				ASSERT(extractionFP);
				std::string copy;
				sPrintAd(copy, ad);
				copy += banner.line + "\n";
				size_t bytes = fwrite(copy.c_str(), sizeof(char), copy.size(), extractionFP);
				if (bytes != copy.size()) {
					fprintf(stderr, "Warning: Failed to write ad to extraction file %s\n", extractionFile);
				}
			}
		}
	}
	if (cluster > 0) { //User specified cluster or cluster.proc.
		if (checkMatchJobIdsFound(banner, &ad)) { //Check if all possible ads have been displayed
			maxAds = adCount;
			return true;
		}
	}
	return false;
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
	newInfo.line = banner;

	const char * p = getAdTypeFromBanner(banner, newInfo.ad_type);
	//Banner contains no Key=value pairs, no info to parse so return true to parse ad
	if (!p) { info = newInfo; return true; }

	upper_case(newInfo.ad_type);
	if ( ! filterAdTypes.contains("ALL") && !filterAdTypes.contains(newInfo.ad_type)) {
		//fprintf(stdout, "Banner Ad Type: %s\n", newInfo.ad_type.c_str());
		return false;
	}

	//fprintf(stdout, "parseBanner(%s)\n", p);
	const char * endp = p + banner.size();

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);

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
		delete tree;
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
	//fprintf(stdout,"Ad type: %s\n",info.ad_type.c_str());
	//fprintf(stdout,"Parsed banner info: %s %d.%d | Comp: %ld | Epoch: %d\n",info.owner.c_str(),info.jid.cluster, info.jid.proc, info.completion, info.runId);

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
		if (starts_with(line.c_str(), "***")) {
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
		if (starts_with(line.c_str(), "***")) {

			if (exprs.size() > 0) {
				printJobIfConstraint(exprs, constraint, constraintExpr, curr_banner);
				exprs.clear();
			} else if (cluster > 0 && checkMatchJobIdsFound(curr_banner, NULL, true)){
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
	classad::References & attrs,
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
		attrs = propt.attrs;
	}
	if ( ! messages.empty()) { fprintf(stderr, "%s", messages.c_str()); }
	return err;
}

//Find and add all epoch files in the given epochDirectory then sort
//based on cluster.proc & backwards/forwards
static void findEpochDirFiles(std::deque<std::string> *epochFiles, const char* epochDirectory){
	std::deque<std::string> searchIds;
	bool oneJob = true;
	int count = 0;
	for(auto& match : jobIdFilterInfo) {
		std::string jobID; //Make jobId to search for .XXX.YY.
		if (match.jid.proc >= 0) { formatstr(jobID,"runs.%d.%d.",match.jid.cluster,match.jid.proc);}
		else { formatstr(jobID,"runs.%d.",match.jid.cluster); oneJob = false; }
		if (++count > 1) { oneJob = false; }
		searchIds.push_back(jobID);
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
			if (cluster > 0) {
				for (const auto& jobID : searchIds) {
					if (file.find(jobID) != std::string::npos) {
						epochFiles->push_back(file);
						if (oneJob) return;
					}
				}
			} else {
				epochFiles->push_back(file);
			}
		}
	} while(file != "");

	//If backwards then reverse the data structure
	if (backwards) { std::reverse(epochFiles->begin(),epochFiles->end()); }
}

static void readHistoryFromDirectory(const char* searchDirectory, const char* constraint, ExprTree *constraintExpr){
	ASSERT(recordSrc != HRS_AUTO);
	printHeader();
	//Make sure match,limit,and/or scanlimit aren't being used with delete function
	if (maxAds != -1 && delete_epoch_ads) {
		fprintf(stderr,"Error: -epoch:d (delete) cannot be used with -scanlimit.\n");
		exit(1);
	}
	else if (specifiedMatch != -1 && delete_epoch_ads) {
		fprintf(stderr,"Error: -epoch:d (delete) cannot be used with -limit or -match.\n");
		exit(1);
	}

	if (!searchDirectory) {
		fprintf(stderr,"Error: No search directory found for locating history files.\n");
		exit(1);
	} else {
		struct stat si = {};
		stat(searchDirectory, &si);
		if ( !(si.st_mode & S_IFDIR) ) {
			fprintf(stderr, "Error: %s is not a valid directory.\n", searchDirectory);
			exit(1);
		}
	}

	std::deque<std::string> recordFiles;
	if (recordSrc == HRS_JOB_EPOCH) { findEpochDirFiles(&recordFiles,searchDirectory); }

	//For each file found read job ads
	for(const auto& file : recordFiles) {
		std::string file_path;
		//Make full path (path+file_name) to read
		if (delete_epoch_ads) {
			/*	If deleting the files then rename the file as before reading
			*	This is to try and prevent a race condition between writing
			*	to the epoch file and reading/deleting it - Cole Bollig 2022-09-13
			*/
			std::string old_name,new_name,base = "READING.txt";
			dircat(searchDirectory,file.c_str(),old_name);
			dircat(searchDirectory,base.c_str(),new_name);
			rename(old_name.c_str(),new_name.c_str());
			dircat(searchDirectory,base.c_str(),file_path);
		} else {
			dircat(searchDirectory,file.c_str(),file_path);
		}
		//Read file
		readHistoryFromFileEx(file_path.c_str(), constraint, constraintExpr, backwards);
		//If deleting then delete the file that was just read.
		if (delete_epoch_ads) {
			int r = remove(file_path.c_str());
			if (r < 0) {
				fprintf(stderr, "Error: Can't delete epoch ad file %s\n", file_path.c_str());
				exit(1);
			}
		}
	}

	printFooter();
	return;
}

static void readHistoryFromSingleFile(bool fileisuserlog, const char *JobHistoryFileName, const char* constraint, ExprTree *constraintExpr) {
	printHeader();

	//If we were passed a specific file name then read that.
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

	printFooter();
	return;
}
