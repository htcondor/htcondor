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
#include "condor_accountant.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_query.h"
#include "condor_q.h"
#include "condor_io.h"
#include "condor_arglist.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "get_daemon_name.h"
#include "ad_printmask.h"
#include "internet.h"
#include "sig_install.h"
#include "format_time.h"
#include "daemon.h"
#include "dc_collector.h"
#include "basename.h"
#include "metric_units.h"
#include "error_utils.h"
#include "print_wrapped_text.h"
#include "condor_distribution.h"
#include "string_list.h"
#include "condor_version.h"
#include "subsystem_info.h"
#include "condor_open.h"
#include "condor_sockaddr.h"
#include "condor_id.h"
#include "userlog_to_classads.h"
#include "ipv6_hostname.h"
#include "../condor_procapi/procapi.h" // for getting cpu time & process memory
#include "classad/classadCache.h" // for CachedExprEnvelope stats
#include "classad_helpers.h"
#include "console-utils.h"

#include "queue_internal.h"


static int cleanup_globals(int exit_code); // called on exit to do necessary cleanup
#define exit(n) (exit)(cleanup_globals(n))

static  int  testing_width = 0;
int getDisplayWidth() {
	if (testing_width <= 0) {
		testing_width = getConsoleWindowSize();
		if (testing_width <= 0)
			testing_width = 80;
	}
	return testing_width;
}

extern  void short_print(int,int,const char*,int,int,int,int,int,const char *);
static  void processCommandLineArguments(int, const char *[]);

static  bool streaming_print_job(void*, ClassAd*);
typedef bool (* buffer_line_processor)(void*, ClassAd *);

static 	void usage (const char *, int other=0);
enum { usage_Universe=1, usage_JobStatus=2, usage_SubmitMethod=4, usage_AllOther=0xFF, usage_DiagOpts=0x100 };

// functions to fetch job ads and print them out
//
static bool show_file_queue(const char* jobads, const char* userlog);
static bool show_schedd_queue(const char* scheddAddress, const char* scheddName, const char* scheddMachine, int useFastPath, CondorClassAdListWriter &writer);
static int dryFetchQueue(const char * file, const classad::References & proj, int fetch_opts, int limit, buffer_line_processor pfnProcess, void *pvProcess);

static void initOutputMask(AttrListPrintMask & pqmask, int qdo_mode, bool wide_mode);
static void init_standard_summary_mask(ClassAd * summary_ad);
static bool is_zero_summary(ClassAd * summary_ad);
bool use_legacy_standard_summary = true;
//PRAGMA_REMIND("make width of the name column adjust to the display width")
const int name_column_index = 2;
int name_column_width = 14;
bool is_standard_format = false;
bool first_col_is_job_id = false;
bool has_owner_column = false;
int  max_name_column_width = 14;
bool append_time_to_source_label = true;
bool querying_partial_clusters = false; // set to true when the condor_q query is selective and so the "DONE" column cannot be calculated.
time_t queue_time = 0; // will be set from the time on the server if possible

double max_mem_used = 0.0; // largest value of MEMORY_USAGE
int max_owner_name = 0; // width of longest owner name, from calls to render_owner or render_dag_owner
int max_batch_name = 0; // width of longest batch name
//int max_cluster_id = 0; // future
//int max_proc_id = 0;    // future

// returns 0, or the index of the argument that was not a valid classad expression.
//
static int parse_format_args(int argc, const char * argv[], AttrListPrintMask & prmask, classad::References & attrs, bool diagnostic)
{
	ClassAd ad;
	const char * pcolon;
	for (int i = 0; i < argc; ++i)
	{
		if (is_dash_arg_prefix (argv[i], "format", 1)) {
			prmask.registerFormatF( argv[i+1], argv[i+2], FormatOptionNoTruncate );
			if ( ! IsValidClassAdExpression(argv[i+2], &attrs, NULL)) {
				return i+2;
			}
			i+=2;
		}
		else
		if (is_dash_arg_colon_prefix(argv[i], "af", &pcolon, 2) ||
			is_dash_arg_colon_prefix(argv[i], "autoformat", &pcolon, 5)) {
			if (pcolon) ++pcolon; // of there are options, skip over the :
			int ixNext = parse_autoformat_args(argc, argv, i+1, pcolon, prmask, attrs, diagnostic);
			if (ixNext < 0) {
				return -ixNext;
			}
			if (ixNext > i) {
				i = ixNext-1;
			}
		}
	}
	return 0;
}


static 	int dash_long = 0, dash_tot = 0, global = 0, show_io = 0, show_held = 0, dash_dag = 0;
static  int dash_batch_specified = 0, dash_batch_is_default = 1, dash_batch = 0;
static  int dash_factory = 0; // if non zero, bits are options, 1=cluster-ads-only, 2=late-materialize-only
static  int dash_jobset = 0;  // if non zero, we also want to see jobset ads
static  int dash_wide = 0; // -wide argument was used
static ClassAdFileParseType::ParseType dash_long_format = ClassAdFileParseType::Parse_auto;
static bool print_attrs_in_hash_order = false;
static bool auto_standard_summary = false; // print standard summary
static  int dash_autocluster = 0; // can be 0, or CondorQ::fetch_DefaultAutoCluster or CondorQ::fetch_GroupBy
static  int default_fetch_opts = QueryFetchOpts::fetch_MyJobs;
bool widescreen = false;
//static  int  use_old_code = true;
static  bool expert = false;
bool verbose = false; // note: this is !!NOT the same as -long !!!
static  int g_match_limit = -1;

static char dash_progress_alt_char = 0;
static  const char *jobads_file = NULL; // NULL, or points to jobads filename from argv
static ClassAdFileParseType::ParseType jobads_file_format = ClassAdFileParseType::Parse_auto;
static  const char *machineads_file = NULL; // NULL, or points to machineads filename from argv
static ClassAdFileParseType::ParseType machineads_file_format = ClassAdFileParseType::Parse_auto;
static  const char *userprios_file = NULL; // NULL, or points to userprios filename from argv
static  const char *userlog_file = NULL; // NULL, or points to userlog filename from argv
static  bool analyze_with_userprio = false;
static  char * analyze_memory_usage = NULL;
bool dash_profile = false;
//static  bool analyze_dslots = false;
static  bool disable_user_print_files = false;
const bool always_write_xml_footer = true;
static bool summarize_with_owner = true;


// Constraint on JobIDs for faster filtering
// a list and not set since we expect a very shallow depth
static std::vector<CondorID> constrID;


static	CondorQ 	Q;
static	QueryResult result;

static	CondorQuery	scheddQuery(SCHEDD_AD);
static	CondorQuery submittorQuery(SUBMITTOR_AD);

static	ClassAdList	scheddList;

#ifdef INCLUDE_ANALYSIS_SUGGESTIONS
// analysis suggestions are mostly useless - usually suggest that you switch opsys
static  ClassAdAnalyzer analyzer;
#endif

static bool local_render_owner(std::string & out, ClassAd*, Formatter &);
static bool local_render_dag_owner(std::string & out, ClassAd*, Formatter &);
static bool local_render_batch_name(std::string & out, ClassAd*, Formatter &);


// the condor q strategy will be to ingest ads and print them into MyRowOfValues structures
// then insert those into a map which is indexed (and thus ordered) by job id.
// Once all of the jobs have arrived we linkup the JobRowOfData structures by job id and dag id.
// Finally, then we adjust column widths and formats based on the actual data and finally print.

// This structure hold the rendered fields from a single job ad, it will be inserted in to a map by job id
// and also linked up using the various pointers
//
class JobRowOfData {
public:
	MyRowOfValues rov;
	long long   id;        // this will probably be cluster/proc, but may be some other unique id (like autocluster id)
	long long   dag_id;    // if non-zero, dag parent job id
	long long   batch_uid; // for non-dag nodes, this is the unique id of the batch name
	int         generation;// when dag_id is non-zero, this is the dag nesting depth
	unsigned int flags;    // JROD_* flags used while processing, also align the pointer fields on 64 bit boundary.
	class JobRowOfData * next_proc; // used at runtime to linkup procs for a cluster
	class JobRowOfData * children;  // used at runtime to linkup dag nodes, points to linked list of children
	class JobRowOfData * next_sib;  // used at runtime to connect up children of a common dag parent or common batch id
	class JobRowOfData * last_sib;  // used at runtime to connect up children of a common dag parent or common batch id
	JobRowOfData(long long _id=0)
		: id(_id), dag_id(0), batch_uid(0), generation(0), flags(0)
		, next_proc(NULL), children(NULL)
		, next_sib(NULL), last_sib(NULL)
	{}

	bool isValid(int index) {
		if ( ! rov.is_valid(index)) return false;
		return rov.Column(index) != NULL;
	}

	template <class t>
	bool getNumber(int index, t & val) {
		val = 0;
		if ( ! rov.is_valid(index)) return false;
		classad::Value * pval = rov.Column(index);
		if ( ! pval) return false;
		return pval->IsNumber(val);
	}

	template <class s>
	bool getString(int index, s & val) {
		if ( ! rov.is_valid(index)) return false;
		classad::Value * pval = rov.Column(index);
		if ( ! pval) return false;
		return pval->IsStringValue(val);
	}
	bool getString(int index, char * buf, int cch) {
		if ( ! rov.is_valid(index)) return false;
		classad::Value * pval = rov.Column(index);
		if ( ! pval) return false;
		return pval->IsStringValue(buf, cch);
	}
};
typedef std::map<long long, JobRowOfData> ROD_MAP_BY_ID;

#define JROD_COOKED  0x0001   // set when the row data has been cooked (i.e. folded into)
#define JROD_SKIP    0x0002   // this row should be skipped entirely
#define JROD_FOLDED  0x0004   // data from this row has been folded into another row
#define JROD_PRINTED 0x0008   // Set during printing so we can know what has already been printed
#define JROD_SCHEDUNIV 0x0010  // job is a scheduler univers job (i.e. a dagman) so we still know after universe column has been baked.
#define JROD_ISDAGNODE 0x0020  // this is a node in a dag (i.e. it has a DAGManJobId)


static int set_print_mask_from_stream(AttrListPrintMask & prmask, const char * streamid, bool is_filename, classad::References & attrs, AttrListPrintMask & sumymask);
static void dump_print_mask(std::string & tmp);


static 	printmask_headerfooter_t customHeadFoot = STD_HEADFOOT;
static std::vector<GroupByKeyInfo> group_by_keys;
static  bool		cputime = false;
static	bool		current_run = true;
static	bool		dash_grid = false;
static	bool		dash_run = false;
static	bool		dash_idle = false;
static	bool		dash_goodput = false;
static	bool		dash_dry_run = false;
static	bool		dash_unmatchable = false;
static  const char * dry_run_file = NULL;
static  const char * capture_raw_results = NULL;
static  FILE*        capture_raw_fp = NULL;
static  const char		*JOB_TIME = "RUN_TIME";
static	bool		querySchedds 	= false;
static	bool		querySubmittors = false;
static	char		constraint[4096];
static  const char *user_job_constraint = NULL; // just the constraint given by the user
static  const char *user_slot_constraint = NULL; // just the machine constraint given by the user
static  const char *global_schedd_constraint = NULL; // argument from -schedd-constraint
static	DCCollector* pool = NULL; 
static	char		*scheddAddr;	// used by format_remote_host()
static CollectorList * Collectors = NULL;

static std::vector<const char *> autoformat_args;

static	int			better_analyze = false;
static	bool		reverse_analyze = false;
static	int			analysis_mode = 0;
static  int			cOwnersOnCmdline = 0;

static	int			analyze_detail_level = 0; // one or more of detail_xxx enum values above.

// these are used in analysis
KeyToClassaAdMap startdAds;
int longest_slot_machine_name = 0;
int longest_slot_name = 0;
bool single_machine = false;
const char* single_machine_label = NULL;

// The schedd will have one of these structures per owner, and one for the schedd as a whole
// these counters are new for 8.7, and used with the code that keeps live counts of jobs
// by tracking state transitions
//
struct LiveJobCounters {
  int JobsSuspended;
  int JobsIdle;             // does not count Local or Scheduler universe jobs, or Grid jobs that are externally managed.
  int JobsRunning;
  int JobsRemoved;
  int JobsCompleted;
  int JobsHeld;
  int SchedulerJobsIdle;
  int SchedulerJobsRunning;
  int SchedulerJobsRemoved;
  int SchedulerJobsCompleted;
  int SchedulerJobsHeld;
  void clear_counters() { memset(this, 0, sizeof(*this)); }
  void publish(ClassAd & ad, const char * prefix) const;
  LiveJobCounters()
	: JobsSuspended(0)
	, JobsIdle(0)
	, JobsRunning(0)
	, JobsRemoved(0)
	, JobsCompleted(0)
	, JobsHeld(0)
	, SchedulerJobsIdle(0)
	, SchedulerJobsRunning(0)
	, SchedulerJobsRemoved(0)
	, SchedulerJobsCompleted(0)
	, SchedulerJobsHeld(0)
  {}
  bool empty() const {
	return !(JobsIdle || JobsRunning || JobsHeld || JobsRemoved || JobsCompleted || JobsSuspended
		|| SchedulerJobsIdle || SchedulerJobsRunning || SchedulerJobsHeld || SchedulerJobsRemoved || SchedulerJobsCompleted);
  }
};

static const std::string & attrjoin(std::string & buf, const char * prefix, const char * attr) {
	if (prefix) { buf = prefix; buf += attr; }
	else { buf = attr; }
	return buf;
}

void LiveJobCounters::publish(ClassAd & ad, const char * prefix) const
{
	std::string buf;
	ad.InsertAttr(attrjoin(buf,prefix,"Jobs"), (long long)(JobsIdle + JobsRunning + JobsHeld + JobsRemoved + JobsCompleted + JobsSuspended));
	ad.InsertAttr(attrjoin(buf,prefix,"Idle"), (long long)JobsIdle);
	ad.InsertAttr(attrjoin(buf,prefix,"Running"), (long long)JobsRunning);
	ad.InsertAttr(attrjoin(buf,prefix,"Held"), (long long)JobsHeld);
	ad.InsertAttr(attrjoin(buf,prefix,"Removed"), (long long)JobsRemoved);
	ad.InsertAttr(attrjoin(buf,prefix,"Completed"), (long long)JobsCompleted);
	ad.InsertAttr(attrjoin(buf,prefix,"Suspended"), (long long)JobsSuspended);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerJobs"), (long long)(SchedulerJobsIdle + SchedulerJobsRunning + SchedulerJobsHeld + SchedulerJobsRemoved + SchedulerJobsCompleted));
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerIdle"), (long long)SchedulerJobsIdle);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerRunning"), (long long)SchedulerJobsRunning);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerHeld"), (long long)SchedulerJobsHeld);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerRemoved"), (long long)SchedulerJobsRemoved);
	ad.InsertAttr(attrjoin(buf,prefix,"SchedulerCompleted"), (long long)SchedulerJobsCompleted);
}

static struct {
	classad::References attrs;
	AttrListPrintMask prmask;
	printmask_headerfooter_t HeadFoot;
	StringList sumyattrs;         // attribute references in summary printmask
	AttrListPrintMask sumymask;   // printmask for summary ad(s)
	LiveJobCounters sumy;         // in case we have to do our own summary, or for -global?
	void init() {
		HeadFoot = STD_HEADFOOT;
		sumy.clear_counters();
	}
} app;

bool g_stream_results = false;


class CondorQClassAdFileParseHelper : public CondorClassAdFileParseHelper
{
 public:
	CondorQClassAdFileParseHelper(ParseType typ=Parse_long)
		: CondorClassAdFileParseHelper("\n", typ)
		, is_schedd(false), is_submitter(false)
	{}
	virtual int PreParse(std::string & line, classad::ClassAd & ad, FILE* file);
	virtual int OnParseError(std::string & line, classad::ClassAd & ad, FILE* file);
	std::string schedd_name;
	std::string schedd_addr;
	bool is_schedd;
	bool is_submitter;
};

// this method is called before each line is parsed. 
// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
int CondorQClassAdFileParseHelper::PreParse(std::string & line, classad::ClassAd & /*ad*/, FILE* /*file*/)
{
	// treat blank lines as delimiters.
	if (line.size() == 0) {
		return 2; // end of classad.
	}

	// standard delimitors are ... and ***
	if (starts_with(line,"...") || starts_with(line,"***")) {
		return 2; // end of classad.
	}

	// the normal output of condor_q -long is "-- schedd-name <addr>"
	// we want to treat that as a delimiter, and also capture the schedd name and addr
	if (starts_with(line, "-- ")) {
		is_schedd = starts_with(line.substr(3), "Schedd:");
		is_submitter = starts_with(line.substr(3), "Submitter:");
		if (is_schedd || is_submitter) {
			size_t ix1 = schedd_name.find(':');
			schedd_name = line.substr(ix1+1);
			ix1 = schedd_name.find_first_of(": \t\n");
			if (ix1 != std::string::npos) {
				size_t ix2 = schedd_name.find_first_not_of(": \t\n", ix1);
				if (ix2 != std::string::npos) {
					schedd_addr = schedd_name.substr(ix2);
					ix2 = schedd_addr.find_first_of(" \t\n");
					if (ix2 != std::string::npos) {
						schedd_addr = schedd_addr.substr(0,ix2);
					}
				}
				schedd_name = schedd_name.substr(0,ix1);
			}
		}
		return 2;
	}


	// check for blank lines or lines whose first character is #
	// tell the parser to skip those lines, otherwise tell the parser to
	// parse the line.
	for (size_t ix = 0; ix < line.size(); ++ix) {
		if (line[ix] == '#')
			return 0; // skip this line, but don't stop parsing.
		if (line[ix] != ' ' && line[ix] != '\t')
			return 1; // parse this line
	}
	return 0; // skip this line, but don't stop parsing.
}

// this method is called when the parser encounters an error
// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
int CondorQClassAdFileParseHelper::OnParseError(std::string & line, classad::ClassAd & ad, FILE* file)
{
	// when we get a parse error, skip ahead to the start of the next classad.
	int ee = this->PreParse(line, ad, file);
	while (1 == ee) {
		if ( ! readLine(line, file, false) || feof(file)) {
			ee = 2;
			break;
		}
		chomp(line);
		ee = this->PreParse(line, ad, file);
	}
	return ee;
}


int GetQueueConstraint(CondorQ & q, ConstraintHolder & constr) {
	int err = 0;
	std::string query;
	q.useDefaultingOperator(true);
	q.rawQuery(query);
	if (query.empty()) {
		constr.clear();
	} else {
		constr.set(strdup(query.c_str()));
	}
	constr.Expr(&err);
	return err;
}


void profile_print(size_t & cbBefore, double & tmBefore, int cAds, bool fCacheStats /*=true*/)
{
       double tmAfter = 0.0;
       size_t cbAfter = ProcAPI::getBasicUsage(getpid(), &tmAfter);
       fprintf(stderr, " %d ads (caching %s)", cAds, classad::ClassAdGetExpressionCaching() ? "ON" : "OFF");
       fprintf(stderr, ", %.3f CPU-sec, %" PRId64 " bytes (from %" PRId64 " to %" PRId64 ")\n",
               (tmAfter - tmBefore), (PRId64_t)(cbAfter - cbBefore), (PRId64_t)cbBefore, (PRId64_t)cbAfter);
       tmBefore = tmAfter; cbBefore = cbAfter;
       if (fCacheStats) {
               classad::CachedExprEnvelope::_debug_print_stats(stderr);
       }
}


int main (int argc, const char **argv)
{
	ClassAd		*ad;
	bool		first;
	std::string		scheddMachine;
	int		useFastScheddQuery = 0;
	const char	*tmp;
	int         retval = 0;

	Collectors = NULL;
	app.init();

	// load up configuration file
	set_priv_initialize(); // allow uid switching if root
	config();
	dprintf_config_tool_on_error(0);
	dprintf_OnExitDumpOnErrorBuffer(stderr);
	//classad::ClassAdSetExpressionCaching( param_boolean( "ENABLE_CLASSAD_CACHING", false ) );


#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	// Setup a default projection for attributes we examine in the schedd/submittor ads.
	// We do this very early to be a default, as we may override it with more specific
	// options depending upon command line arguments, e.g. -name.
	std::vector<std::string> attrs; attrs.reserve(4);
	attrs.emplace_back(ATTR_SCHEDD_IP_ADDR);
	attrs.emplace_back(ATTR_VERSION);
	attrs.emplace_back(ATTR_NAME);
	attrs.emplace_back(ATTR_MACHINE);
	submittorQuery.setDesiredAttrs(attrs);
	scheddQuery.setDesiredAttrs(attrs);

	if (param_boolean("CONDOR_Q_ONLY_MY_JOBS", true)) {
		default_fetch_opts |= QueryFetchOpts::fetch_MyJobs;
	} else {
		default_fetch_opts &= ~QueryFetchOpts::fetch_MyJobs;
	}

	dash_batch_is_default = param_boolean("CONDOR_Q_DASH_BATCH_IS_DEFAULT", true);
	use_legacy_standard_summary = param_boolean("CONDOR_Q_SHOW_OLD_SUMMARY", use_legacy_standard_summary);

	// process arguments
	processCommandLineArguments (argc, argv);

	if (Collectors == NULL) {
		Collectors = CollectorList::create();
	}

	// check if analysis is required
	if (better_analyze || dash_unmatchable) {
		setupAnalysis(Collectors, machineads_file, machineads_file_format, user_slot_constraint);
		if (userprios_file || analyze_with_userprio) {
			setupUserpriosForAnalysis(pool, userprios_file);
		}
	}

	// if fetching jobads from a file or userlog, we don't need to init any daemon or database communication
	if ((jobads_file != NULL || userlog_file != NULL)) {
		retval = show_file_queue(jobads_file, userlog_file);
		exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
	}

	// if we haven't figured out what to do yet, just display the
	// local queue 
	if (!global && !querySchedds && !querySubmittors) {

		Daemon schedd( DT_SCHEDD, 0, 0 );

		if ( schedd.locate() ) {

			std::string scheddName;
			scheddAddr = strdup(schedd.addr());
			if( (tmp = schedd.name()) ) {
				scheddName = tmp;
				Q.addSchedd(tmp);
			} else {
				scheddName = "Unknown";
			}
			if( (tmp = schedd.fullHostname()) ) {
				scheddMachine = tmp;
			} else {
				scheddMachine = "Unknown";
			}
			if (schedd.version()) {
				CondorVersionInfo v(schedd.version());
				if (v.built_since_version(8,3,3)) {
					bool v3_query_with_auth = v.built_since_version(8,5,6) && (default_fetch_opts & QueryFetchOpts::fetch_MyJobs);
					useFastScheddQuery = v3_query_with_auth ? 3 : 2;
				} else {
					useFastScheddQuery = v.built_since_version(6,9,3) ? 1 : 0;
				}
				Q.useDefaultingOperator(v.built_since_version(8,6,0));
			}

			CondorClassAdListWriter writer(dash_long_format);
			retval = show_schedd_queue(scheddAddr, scheddName.c_str(), scheddMachine.c_str(), useFastScheddQuery, writer);
			if (dash_long) {
				writer.writeFooter(stdout, always_write_xml_footer);
			}
			/* Hopefully I got the queue from the schedd... */
			exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
		} 
		
		/* I couldn't find a local schedd, so dump a message about what
			happened. */

		fprintf( stderr, "Error: %s\n", schedd.error() );
		if (!expert) {
			fprintf(stderr, "\n");
			print_wrapped_text("Extra Info: You probably saw this "
							   "error because the condor_schedd is "
							   "not running on the machine you are "
							   "trying to query.  If the condor_schedd "
							   "is not running, the Condor system "
							   "will not be able to find an address "
							   "and port to connect to and satisfy "
							   "this request.  Please make sure "
							   "the Condor daemons are running and "
							   "try again.\n", stderr );
			print_wrapped_text("Extra Info: "
							   "If the condor_schedd is running on the "
							   "machine you are trying to query and "
							   "you still see the error, the most "
							   "likely cause is that you have setup a " 
							   "personal Condor, you have not "
							   "defined SCHEDD_NAME in your "
							   "condor_config file, and something "
							   "is wrong with your "
							   "SCHEDD_ADDRESS_FILE setting. "
							   "You must define either or both of "
							   "those settings in your config "
							   "file, or you must use the -name "
							   "option to condor_q. Please see "
							   "the Condor manual for details on "
							   "SCHEDD_NAME and "
							   "SCHEDD_ADDRESS_FILE.",  stderr );
		}

		exit( EXIT_FAILURE );
	}
	
	// if a global queue is required, query the schedds instead of submittors
	if (global) {
		querySchedds = true;
		snprintf( constraint, sizeof(constraint), "%s > 0 || %s > 0 || %s > 0 || %s > 0 || %s > 0", 
			ATTR_TOTAL_RUNNING_JOBS, ATTR_TOTAL_IDLE_JOBS,
			ATTR_TOTAL_HELD_JOBS, ATTR_TOTAL_REMOVED_JOBS,
			ATTR_TOTAL_JOB_ADS );
		result = scheddQuery.addANDConstraint( constraint );
		if( result != Q_OK ) {
			fprintf( stderr, "Error: Couldn't add schedd-constraint %s\n", constraint);
			exit( 1 );
		}
	}

	// Get the list of ads from the collector.  
	if( querySchedds ) { 
		result = Collectors->query ( scheddQuery, scheddList );
	} else {
		result = Collectors->query ( submittorQuery, scheddList );
	}

	switch( result ) {
	case Q_OK:
		break;
	case Q_COMMUNICATION_ERROR: 
			// if we're not an expert, we want verbose output
		printNoCollectorContact( stderr, pool ? pool->name() : NULL,
								 !expert ); 
		exit( 1 );
	case Q_NO_COLLECTOR_HOST:
		ASSERT( pool );
		fprintf( stderr, "Error: Can't contact condor_collector: "
				 "invalid hostname: %s\n", pool->name() );
		exit( 1 );
	default:
		fprintf( stderr, "Error fetching ads: %s\n", 
				 getStrQueryResult(result) );
		exit( 1 );
	}

	first = true;
	CondorClassAdListWriter writer(dash_long_format);
	// get queue from each ScheddIpAddr in ad
	scheddList.Open();
	while ((ad = scheddList.Next()))
	{
		/* default to true for remotely queryable */

		/* Warning!! Any attributes you lookup from the ad had better be
		   included in the list of attributes given to scheddQuery.setDesiredAttrs()
		   and to submittorQuery.setDesiredAttrs() !!! This is done early in main().
		*/
		std::string scheddName;
		std::string scheddAddr;
		if ( ! (ad->LookupString(ATTR_SCHEDD_IP_ADDR, scheddAddr)  &&
				ad->LookupString(ATTR_NAME, scheddName) &&
				ad->LookupString(ATTR_MACHINE, scheddMachine)
				)
			)
		{
			/* something is wrong with this schedd/submittor ad, try the next one */
			continue;
		}

		first = false;

		std::string scheddVersion;
		ad->LookupString(ATTR_VERSION, scheddVersion);
		CondorVersionInfo v(scheddVersion.c_str());
		if (v.built_since_version(8, 3, 3)) {
			bool v3_query_with_auth = v.built_since_version(8,5,6) && (default_fetch_opts & QueryFetchOpts::fetch_MyJobs);
			useFastScheddQuery = v3_query_with_auth ? 3 : 2;
		} else {
			useFastScheddQuery = v.built_since_version(6,9,3) ? 1 : 0;
		}
		Q.useDefaultingOperator(v.built_since_version(8,6,0));
		retval = show_schedd_queue(scheddAddr.c_str(), scheddName.c_str(), scheddMachine.c_str(), useFastScheddQuery, writer);
	}

	// close list
	scheddList.Close();

	if (dash_long) {
		writer.writeFooter(stdout, always_write_xml_footer);
	}

	if( first ) {
		if( global ) {
			printf( "All queues are empty\n" );
			retval = 1;
		} else {
			fprintf(stderr,"Error: Collector has no record of "
							"schedd/submitter\n");

			exit(1);
		}
	}

	exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
}

// when exit is called, this is called also
static int cleanup_globals(int exit_code)
{
	// pass the exit code through dprintf_SetExitCode so that it knows
	// whether to print out the on-error buffer or not.
	dprintf_SetExitCode(exit_code);

	// do this to make Valgrind happy.
	cleanupAnalysis();
	scheddList.Clear();

	return exit_code;
}

static int
parse_analyze_detail(const char * pch, int current_details)
{
	//PRAGMA_REMIND("TJ: analyze_detail_level should be enum rather than integer")
	int details = 0;
	int flg = 0;
	while (int ch = *pch) {
		++pch;
		if (ch >= '0' && ch <= '9') {
			flg *= 10;
			flg += ch - '0';
		} else if (ch == ',' || ch == '+') {
			details |= flg;
			flg = 0;
		}
	}
	if (0 == (details | flg))
		return 0;
	return current_details | details | flg;
}


// this enum encodes the user choice of the various reports that condor_q can show
// The first few are mutually exclusive, the last are flags
enum {
	QDO_NotSet=0,
	QDO_JobNormal,
	QDO_JobRuntime,
	QDO_JobIdle,
	QDO_JobGoodput,
	QDO_JobGridInfo,
	QDO_JobGridEC2Info,
	QDO_JobHold,
	QDO_JobIO,
	QDO_Factory,
	QDO_DAG,
	QDO_Totals,
	QDO_Progress,

	QDO_AutoclusterNormal, // Print typical autocluster attributes
	QDO_JobsetNormal,      // Print jobset counters

	QDO_Custom,  // a custom printformat file was loaded, standard outputs should be below this one.
	QDO_Analyze, // not really a print format.

	QDO_BaseMask  = 0x0000FF,

	// modifier flags
	QDO_Cputime  = 0x010000,

	// flags
	QDO_Format      = 0x100000,
	QDO_PrintFormat = 0x200000,
	QDO_AutoFormat  = 0x300000, // Format + PrintFormat == AutoFormat
	QDO_Attribs     = 0x800000,
};

static void 
processCommandLineArguments (int argc, const char *argv[])
{
	//Bool for if a job runtime flag (currentrun or cumulative-time) has been passed
	bool runtimeTypeSet = false;
	int i;
	char *daemonname;
	const char * hat;
	const char * pcolon;

	int qdo_mode = QDO_NotSet;

	for (i = 1; i < argc; i++)
	{
		// no dash means this arg is a cluster/proc, proc, or owner
		if( *argv[i] != '-' ) {
			int cluster, proc;
			const char * pend;
			if (StrIsProcId(argv[i], cluster, proc, &pend) && *pend == 0) {
				constrID.emplace_back(cluster,proc,-1);
			}
			else {
				++cOwnersOnCmdline;
				if( Q.add( CQ_OWNER, argv[i] ) != Q_OK ) {
					// this error doesn't seem very helpful... can't we say more?
					fprintf( stderr, "Error: Argument %d (%s) must be a jobid or user\n", i, argv[i] );
					exit( 1 );
				}
				// dont default to 'my jobs' if an owner was specified.
				default_fetch_opts &= ~QueryFetchOpts::fetch_MyJobs;
			}

			continue;
		}

		// the argument began with a '-', so use only the part after
		// the '-' for prefix matches
		const char* dash_arg = argv[i];

		if (is_dash_arg_colon_prefix (dash_arg, "wide", &pcolon, 1)) {
			widescreen = true; // display in wide mode
			dash_wide = true; // remember that there was a -wide argument
			if (pcolon) {
				testing_width = atoi(++pcolon);
				// TODO: fix this...
				widescreen = false;
			}
			continue;
		}
		if (is_dash_arg_colon_prefix (dash_arg, "long", &pcolon, 1)) {
			dash_long = 1;
			//summarize = 0;
			//customHeadFoot = HF_BARE;
			if (pcolon) {
				StringList opts(++pcolon);
				for (const char * opt = opts.first(); opt; opt = opts.next()) {
					if (YourString(opt) == "nosort") {
						print_attrs_in_hash_order = true;
					} else {
						dash_long_format = parseAdsFileFormat(opt, dash_long_format);
					}
				}
			}
		} 
		else
		if (is_dash_arg_prefix (dash_arg, "xml", 3)) {
			//use_xml = 1;
			dash_long = 1;
			if (dash_long_format == ClassAdFileParseType::Parse_json) {
				fprintf( stderr, "Error: Cannot print as both XML and JSON\n" );
				exit( 1 );
			}
			dash_long_format = ClassAdFileParseType::Parse_xml;
			//summarize = 0;
			//customHeadFoot = HF_BARE;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "json", 2)) {
			//use_json = true;
			dash_long = 1;
			if (dash_long_format == ClassAdFileParseType::Parse_xml) {
				fprintf( stderr, "Error: Cannot print as both XML and JSON\n" );
				exit( 1 );
			}
			dash_long_format = ClassAdFileParseType::Parse_json;
			//summarize = 0;
			//customHeadFoot = HF_BARE;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "limit", 3)) {
			if (++i >= argc) {
				fprintf(stderr, "Error: -limit requires the max number of results as an argument.\n");
				exit(1);
			}
			char *endptr;
			g_match_limit = strtol(argv[i], &endptr, 10);
			if (*endptr != '\0')
			{
				fprintf(stderr, "Error: Unable to convert (%s) to a number for -limit.\n", argv[i]);
				exit(1);
			}
			if (g_match_limit <= 0)
			{
				fprintf(stderr, "Error: %d is not a valid for -limit.\n", g_match_limit);
				exit(1);
			}
		}
		else
		if (is_dash_arg_prefix (dash_arg, "pool", 1)) {
			if( pool ) {
				delete pool;
			}
			if( ++i >= argc ) {
				fprintf( stderr,
						 "Error: -pool requires a hostname as an argument.\n" );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: The hostname should be the central "
									   "manager of the Condor pool you wish to work with.",
									   stderr);
				}
				exit(1);
			}
			pool = new DCCollector( argv[i] );
			if( ! pool->addr() ) {
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
				exit(1);
			}
			Collectors = new CollectorList();
				// Add a copy of our DCCollector object, because
				// CollectorList() takes ownership and may even delete
				// this object before we are done.
			Collectors->getList().emplace_back( new DCCollector( *pool ) );
		} 
		else
		if (is_arg_prefix (dash_arg+1, "D", 1)) {
			if( ++i >= argc ) {
				fprintf( stderr, 
						 "Error: %s requires a list of flags as an argument.\n", dash_arg );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: You need to specify debug flags "
									   "as a quoted string. Common flags are D_ALL, and "
									   "D_FULLDEBUG.",
									   stderr);
				}
				exit( 1 );
			}
			set_debug_flags( argv[i], 0 );
		} 
		else
		if (is_dash_arg_prefix (dash_arg, "name", 1)) {

			if (querySubmittors) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submitters\n");
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: You cannot specify both -name and "
									   "-submitter. -name implies you want to only query "
									   "the local schedd, while -submitter implies you want "
									   "to find everything in the entire pool for a given"
									   "submitter.",
									   stderr);
				}
				exit(1);
			}

			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, 
						 "Error: -name requires the name of a schedd as an argument.\n" );
				exit(1);
			}

			if( !(daemonname = get_daemon_name(argv[i+1])) ) {
				fprintf( stderr, "Error: unknown host %s\n",
						 get_host_part(argv[i+1]) );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: The name given with the -name "
									   "should be the name of a condor_schedd process. "
									   "Normally it is either a hostname, or "
									   "\"name@hostname\". "
									   "In either case, the hostname should be the Internet "
									   "host name, but it appears that it wasn't.",
									   stderr);
				}
				exit(1);
			}
			snprintf (constraint, sizeof(constraint), "%s == \"%s\"", ATTR_NAME, daemonname);
			scheddQuery.addORConstraint (constraint);
			scheddQuery.setLocationLookup(daemonname);
			Q.addSchedd(daemonname);

			free(daemonname);
			i++;
			querySchedds = true;
		} 
		else
		if (is_dash_arg_prefix (dash_arg, "direct", 1)) {
			/* check for one more argument */
			if (argc <= i+1) {
				fprintf( stderr, "Error: -direct requires [schedd]\n" );
				exit(EXIT_FAILURE);
			}
			// the direct argument is vistigial, because only schedd is allowed, but we still parse and accept it.
			i++;
			if (MATCH != strcasecmp(argv[i], "schedd")) {
				fprintf( stderr, "Error: -direct may only take 'schedd' as an option.\n" );
				exit(EXIT_FAILURE);
			}
		}
		else
		if (is_dash_arg_prefix (dash_arg, "submitter", 1)) {

			if (querySchedds) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submitters\n");
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: You cannot specify both -name and "
									   "-submitter. -name implies you want to only query "
									   "the local schedd, while -submitter implies you want "
									   "to find everything in the entire pool for a given"
									   "submitter.",
									   stderr);
				}
				exit(1);
			}
			
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: -submitter requires the name of a user.\n");
				exit(1);
			}
				
			i++;
			if ((hat = strchr(argv[i],'@'))) {
				// is the name already qualified with a UID_DOMAIN?
				snprintf (constraint, sizeof(constraint), "%s == \"%s\"", ATTR_NAME, argv[i]);
				//*hat = '\0';
			} else {
				// no ... add UID_DOMAIN
				char *uid_domain = param( "UID_DOMAIN" );
				if (uid_domain == NULL)
				{
					EXCEPT ("No 'UID_DOMAIN' found in config file");
				}
				snprintf (constraint, sizeof(constraint), "%s == \"%s@%s\"", ATTR_NAME, argv[i], uid_domain);
				free (uid_domain);
			}
			// dont default to 'my jobs'
			default_fetch_opts &= ~QueryFetchOpts::fetch_MyJobs;

			// insert the constraints
			submittorQuery.addORConstraint (constraint);

			{
				const char *ownerName = argv[i];
				// ensure that the "nice-user" prefix isn't inserted as part
				// of the job ad constraint
				if( strstr( argv[i] , NiceUserName ) == argv[i] ) {
					ownerName = argv[i]+strlen(NiceUserName)+1;
				}
			#if 1
				if (Q.add (CQ_SUBMITTER, ownerName) != Q_OK) {
					fprintf (stderr, "Error:  Argument %d (%s)\n", i, argv[i]);
					exit (1);
				}
			#else
				const char * dotptr = strchr(ownerName, '.');
				if (dotptr) {
					// ensure that the group prefix isn't inserted as part
					// of the job ad constraint.
					auto_free_ptr groups(param("GROUP_NAMES"));
					if (groups) {
						std::string owner(ownerName, dotptr - ownerName);
						StringList groupList(groups.ptr());
						if ( groupList.contains_anycase(owner.c_str()) ) {
							// this name starts with a group prefix.
							// so use the part after the group name for the owner name.
							ownerName = dotptr + 1;	// add one for the '.'
						}
					}
				}
				if (Q.add (CQ_OWNER, ownerName) != Q_OK) {
					fprintf (stderr, "Error:  Argument %d (%s)\n", i, argv[i]);
					exit (1);
				}
			#endif
			}

			querySubmittors = true;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "constraint", 1)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: -constraint requires another parameter\n");
				exit(1);
			}
			user_job_constraint = argv[++i];
			querying_partial_clusters = true; // we don't know if this constraint will return partial clusters....

			if (Q.addAND (user_job_constraint) != Q_OK) {
				fprintf (stderr, "Error: Argument %d (%s) is not a valid constraint\n", i, user_job_constraint);
				exit (1);
			}
		} 
		else
		if (is_dash_arg_prefix (dash_arg, "slotconstraint", 5) || is_dash_arg_prefix (dash_arg, "mconstraint", 2)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: %s requires another parameter\n", dash_arg);
				exit(1);
			}
			user_slot_constraint = argv[++i];
		}
		else
		if (is_dash_arg_prefix (dash_arg, "machine", 2)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: %s requires another parameter\n", dash_arg);
				exit(1);
			}
			user_slot_constraint = argv[++i];
		}
		else
		if (is_dash_arg_prefix (dash_arg, "address", 1)) {

			if (querySubmittors) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submitters\n");
				exit(1);
			}

			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr,
						 "Error: -address requires another parameter\n" );
				exit(1);
			}
			if( ! is_valid_sinful(argv[i+1]) ) {
				fprintf( stderr, 
					 "Address must be of the form: \"<ip.address:port>\"\n" );
				exit(1);
			}
			//PRAGMA_REMIND("TJ: fix to use address to contact the schedd directly.")
			snprintf(constraint, sizeof(constraint), "%s == \"%s\"", ATTR_MY_ADDRESS, argv[i+1]);
			scheddQuery.addORConstraint(constraint);
			i++;
			querySchedds = true;
		} 
		else
		if (is_dash_arg_colon_prefix (dash_arg, "autocluster", &pcolon, 2)) {
			dash_autocluster = QueryFetchOpts::fetch_DefaultAutoCluster;
		}
		else
		if (is_dash_arg_colon_prefix (dash_arg, "group-by", &pcolon, 2)) {
			dash_autocluster = QueryFetchOpts::fetch_GroupBy;
		}
		else
		if (is_dash_arg_colon_prefix (dash_arg, "factory", &pcolon, 4)) {
			if (pcolon) {
				StringList opts(++pcolon, ",");
				for (const char * opt = opts.first(); opt; opt = opts.next()) {
					if (is_arg_prefix(opt, "clusters_only", 1)) {
						dash_factory |= 1;
					} else if (is_arg_prefix(opt, "late_materialize", 1)) {
						dash_factory |= 2;
					} else {
						fprintf( stderr, "Error: unknown option %s for -factory\n", opt );
						exit (1);
					}
				}
			}
			if ( ! dash_factory) { dash_factory = 3; } // all-cluster-ads | late-materialize-only
		}
		else
		if (is_dash_arg_colon_prefix (dash_arg, "jobset", &pcolon, 5)) {
			dash_jobset = QueryFetchOpts::fetch_IncludeJobsetAds;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "attributes", 2)) {
			if( argc <= i+1 ) {
				fprintf( stderr, "Error: -attributes requires a list of attributes to show\n" );
				exit( 1 );
			}
			qdo_mode |= QDO_Attribs;
			for (const auto& s: StringTokenIterator(argv[i+1])) {
				app.attrs.emplace(s);
			}
			i++;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "format", 1)) {
				// make sure we have at least two more arguments
			if( argc <= i+2 ) {
				fprintf( stderr, "Error: -format requires format and attribute parameters\n" );
				exit( 1 );
			}
			qdo_mode = QDO_Format | QDO_Custom;
			autoformat_args.push_back(argv[i]);
			autoformat_args.push_back(argv[i+1]);
			autoformat_args.push_back(argv[i+2]);
			i+=2;
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "autoformat", &pcolon, 5) ||
			is_dash_arg_colon_prefix(dash_arg, "af", &pcolon, 2)) {
				// make sure we have at least one more argument
			if ( (i+1 >= argc)  || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: -autoformat requires at least one attribute parameter\n" );
				exit( 1 );
			}
			qdo_mode = QDO_AutoFormat | QDO_Custom;
			autoformat_args.push_back(argv[i]);
			// process all arguments that don't begin with "-" as part of autoformat.
			while (i+1 < argc && *(argv[i+1]) != '-') {
				++i;
				autoformat_args.push_back(argv[i]);
			}
			// if autoformat list ends in a '-' without any characters after it, just eat the arg and keep going.
			if (i+1 < argc && '-' == (argv[i+1])[0] && 0 == (argv[i+1])[1]) {
				++i;
			}
		}
		else
		if (is_dash_arg_colon_prefix(argv[i], "print-format", &pcolon, 2)) {
			if ( (i+1 >= argc)  || (*(argv[i+1]) == '-' && (argv[i+1])[1] != 0)) {
				fprintf( stderr, "Error: -print-format requires a filename argument\n");
				exit( 1 );
			}
			// hack allow -pr ! to disable use of user-default print format files.
			if (MATCH == strcmp(argv[i+1], "!")) {
				++i;
				disable_user_print_files = true;
				continue;
			}
			qdo_mode = QDO_PrintFormat | QDO_Custom;
			bool no_width = widescreen || ( ! dash_wide && (getConsoleWindowSize() < 0));
			if ( ! no_width) app.prmask.SetOverallWidth(getDisplayWidth()-1);
			++i;
			if (set_print_mask_from_stream(app.prmask, argv[i], true, app.attrs, app.sumymask) < 0) {
				fprintf(stderr, "Error: invalid select file %s\n", argv[i]);
				exit (1);
			}
			//summarize = (customHeadFoot & HF_NOSUMMARY) ? 0 : 1;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "global", 1)) {
			global = 1;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "allusers", 2) || is_dash_arg_prefix (dash_arg, "all-users", 2)) {
			default_fetch_opts &= ~QueryFetchOpts::fetch_MyJobs;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "schedd-constraint", 5)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: -schedd-constraint requires a constraint argument\n");
				exit(1);
			}
			global_schedd_constraint = argv[++i];

			if (scheddQuery.addANDConstraint (global_schedd_constraint) != Q_OK) {
				fprintf (stderr, "Error: Invalid constraint (%s)\n", global_schedd_constraint);
				exit (1);
			}
			global = 1;
		}
		else
		if (is_dash_arg_prefix (argv[i], "help", 1)) {
			int other = 0;
			while ((i+1 < argc) && *(argv[i+1]) != '-') {
				++i;
				if (is_arg_prefix(argv[i], "universe", 3) || is_arg_prefix(argv[i], "Universe", 3)) {
					other |= usage_Universe;
				} else if (is_arg_prefix(argv[i], "state", 2) || is_arg_prefix(argv[i], "State", 2) || is_arg_prefix(argv[i], "status", 2)) {
					other |= usage_JobStatus;
				} else if (is_arg_prefix(argv[i], "submit", 2) || is_arg_prefix(argv[i], "Submit", 2)) {
					other |= usage_SubmitMethod;
				} else if (is_arg_prefix(argv[i], "diagnostics", 4) || is_arg_prefix(argv[i], "dry-run", 3)) {
					other |= usage_DiagOpts;
				} else if (is_arg_prefix(argv[i], "all", 2)) {
					other |= usage_AllOther;
				}
			}
			usage(argv[0], other);
			exit(0);
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "better-analyze", &pcolon, 2)
			|| is_dash_arg_colon_prefix(dash_arg, "better-analyse", &pcolon, 2)
			|| is_dash_arg_colon_prefix(dash_arg, "analyze", &pcolon, 2)
			|| is_dash_arg_colon_prefix(dash_arg, "analyse", &pcolon, 2)
			) {
			better_analyze = true;
			if (dash_arg[1] == 'b' || dash_arg[2] == 'b') { // if better, default to higher verbosity output.
				analyze_detail_level |= detail_better | detail_analyze_each_sub_expr | detail_always_analyze_req;
			}
			if (pcolon) { 
				StringList opts(++pcolon, ",:");
				opts.rewind();
				while(const char *popt = opts.next()) {
					//printf("parsing opt='%s'\n", popt);
					if (is_arg_prefix(popt, "summary",1)) {
						analysis_mode = anaModeSummary;
						analyze_with_userprio = false;
					} else if (is_arg_prefix(popt, "reverse",1)) {
						reverse_analyze = true;
						analyze_with_userprio = false;
					} else if (is_arg_prefix(popt, "priority",2)) {
						analyze_with_userprio = true;
					} else if (is_arg_prefix(popt, "noprune",3)) {
						analyze_detail_level |= detail_show_all_subexprs;
					} else if (is_arg_prefix(popt, "diagnostic",4)) {
						analyze_detail_level |= detail_diagnostic;
					} else if (is_arg_prefix(popt, "show-work",4)) {
						analyze_detail_level |= detail_dump_intermediates;
					} else if (is_arg_prefix(popt, "slot-exprs",4)) {
						analyze_detail_level |= detail_inline_std_slot_exprs;
					} else if (is_arg_prefix(popt, "ifthenelse",2)) {
						analyze_detail_level |= detail_show_ifthenelse;
					} else if (is_arg_prefix(popt, "memory",3)) {
						if (analyze_memory_usage) {
							free(analyze_memory_usage);
						}
						analyze_memory_usage = opts.next();
						if (analyze_memory_usage) { analyze_memory_usage = strdup(analyze_memory_usage); }
						else { analyze_memory_usage = strdup(ATTR_REQUIREMENTS); }
					//} else if (is_arg_prefix(popt, "dslots",2)) {
					//	analyze_dslots = true;
					} else {
						analyze_detail_level = parse_analyze_detail(popt, analyze_detail_level);
					}
				}
			}
			qdo_mode = QDO_Analyze;
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "reverse-analyze", &pcolon, 3)) {
			reverse_analyze = true; // modify analyze to be reverse analysis
			better_analyze = true;	// enable analysis
			analyze_with_userprio = false;
			if (pcolon) { 
				analyze_detail_level |= parse_analyze_detail(++pcolon, analyze_detail_level);
			}
			qdo_mode = QDO_Analyze;
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "unmatchable", &pcolon, 5)) {
			dash_unmatchable = true;
			//qdo_mode = QDO_Analyze;
			//better_analyze = true; // enable analysis
			//analysis_mode = anaModeUnmatchable;
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "dry-run", &pcolon, 3)) {
			dash_dry_run = true;
			if (pcolon && pcolon[1]) {
				dry_run_file = ++pcolon;
			}
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "capture-raw-results", &pcolon, 7)) {
			capture_raw_results = "-";
			if (pcolon && pcolon[1]) {
				capture_raw_results = ++pcolon;
			}
		}
		else
		if (is_dash_arg_prefix(dash_arg, "verbose", 4)) {
			// chatty output, mostly for for -analyze
			// this is not the same as -long. 
			verbose = true;
		}
		else
		if (is_dash_arg_prefix(dash_arg, "run", 1)) {
			std::string expr;
			formatstr( expr, "%s == %d || %s == %d || %s == %d", ATTR_JOB_STATUS, RUNNING,
					 ATTR_JOB_STATUS, TRANSFERRING_OUTPUT, ATTR_JOB_STATUS, SUSPENDED );
			Q.addAND( expr.c_str() );
			dash_run = true;
			querying_partial_clusters = true;
			if (show_held || dash_idle) {
				fprintf( stderr, "-run and -hold/held or -idle are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else
		if (is_dash_arg_prefix(dash_arg, "hold", 2) || is_dash_arg_prefix(dash_arg, "held", 2)) {
			std::string expr;
			formatstr(expr, ATTR_JOB_STATUS "==%d", HELD);
			Q.addAND (expr.c_str());
			show_held = true;
			querying_partial_clusters = true;
			if (dash_run || dash_idle) {
				fprintf( stderr, "-run and -hold/held or -idle are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else
		if (is_dash_arg_prefix(dash_arg, "idle", 3)) {
			std::string expr;
			formatstr(expr, ATTR_JOB_STATUS "==%d", IDLE);
			Q.addAND (expr.c_str());
			dash_idle = true;
			querying_partial_clusters = true;
			if (dash_run || show_held) {
				fprintf( stderr, "-run and -hold/held or -idle are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else
		if (is_dash_arg_prefix(dash_arg, "goodput", 2)) {
			// goodput and show_io require the same column
			// real-estate, so they're mutually exclusive
			dash_goodput = true;
			show_io = false;
			qdo_mode = QDO_JobGoodput;
		}
		else
		if (is_dash_arg_prefix(dash_arg, "cputime", 2)) {
			cputime = true;
			JOB_TIME = "CPU_TIME";
			qdo_mode |= QDO_Cputime;
		}
		else
		if (is_dash_arg_prefix(dash_arg, "currentrun", 3)) {
			if (runtimeTypeSet) {
				fprintf(stderr,"Error: -currentrun can't be used with -cumulative-time\n");
				exit(1);
			}
			current_run = true;
			runtimeTypeSet = true;
		}
		else
		if (is_dash_arg_prefix(dash_arg, "cumulative-time", 3)) {
			if (runtimeTypeSet) {
				fprintf(stderr,"Error: -cumulative-time can't be used with -currentrun\n");
				exit(1);
			}
			current_run = false;
			runtimeTypeSet = true;
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "grid", &pcolon, 2 )) {
			dash_grid = true;
			qdo_mode = QDO_JobGridInfo;
			Q.addAND( "JobUniverse == 9" );

			if( pcolon && strcmp( ++pcolon, "ec2" ) == 0 ) {
				qdo_mode = QDO_JobGridEC2Info;
			}
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		}
		else
		if (is_dash_arg_prefix(dash_arg, "io", 2)) {
			// goodput and show_io require the same column
			// real-estate, so they're mutually exclusive
			show_io = true;
			dash_goodput = false;
			qdo_mode = QDO_JobIO;
		}
		else if (is_dash_arg_prefix(dash_arg, "dag", 2)) {
			dash_dag = true;
			if( g_stream_results  ) {
				fprintf( stderr, "-stream-results and -dag are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else if (is_dash_arg_colon_prefix(dash_arg, "batch", &pcolon, 2) ||
			     is_dash_arg_colon_prefix(dash_arg, "progress", &pcolon, 3)) {
			dash_batch = true;
			dash_batch_specified = true;
			if (pcolon) {
				StringList opts(++pcolon, ",:");
				opts.rewind();
				while (const char * popt = opts.next()) {
					char ch = *popt;
					if (ch >= '0' && ch <= '9') {
						dash_batch = atoi(popt);
					} else if (strchr("b?*.-_#z", ch)) {
						dash_progress_alt_char = ch;
					}
				}
			}
			qdo_mode = QDO_Progress;
			if( g_stream_results  ) {
				fprintf( stderr, "-stream-results and -batch are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else if (is_dash_arg_prefix(dash_arg, "nobatch", 3)) {
			dash_batch = false;
			dash_batch_specified = true;
			if ((qdo_mode & QDO_BaseMask) == QDO_Progress) {
				qdo_mode = (qdo_mode & ~QDO_BaseMask) | QDO_NotSet;
			}
		}
		else if (is_dash_arg_prefix(dash_arg, "totals", 3)) {
			qdo_mode = QDO_Totals;
			dash_tot = true;
			if (set_print_mask_from_stream(app.prmask, "SELECT NOHEADER\nSUMMARY STANDARD", false, app.attrs, app.sumymask) < 0) {
				fprintf(stderr, "Error: unexpected error!\n");
				exit (1);
			}
		}
		else if (is_dash_arg_prefix(dash_arg, "expert", 1)) {
			expert = true;
			/// fix me
		}
		else if (is_dash_arg_colon_prefix(dash_arg, "jobads", &pcolon, 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: -jobads requires a filename\n");
				exit(1);
			} else {
				i++;
				jobads_file = argv[i];
			}
			if (pcolon) {
				jobads_file_format = parseAdsFileFormat(++pcolon, jobads_file_format);
			}
		}
		else if (is_dash_arg_prefix(dash_arg, "userlog", 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: -userlog requires a filename\n");
				exit(1);
			} else {
				i++;
				userlog_file = argv[i];
			}
		}
		else if (is_dash_arg_colon_prefix(dash_arg, "slotads", &pcolon, 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: -slotads requires a filename\n");
				exit(1);
			} else {
				i++;
				machineads_file = argv[i];
			}
			if (pcolon) {
				machineads_file_format = parseAdsFileFormat(++pcolon, machineads_file_format);
			}
		}
		else if (is_dash_arg_colon_prefix(dash_arg, "userprios", &pcolon, 5)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: -userprios requires a filename argument\n");
				exit(1);
			} else {
				i++;
				userprios_file = argv[i];
				analyze_with_userprio = true;
			}
		}
		else if (is_dash_arg_colon_prefix(dash_arg, "nouserprios", &pcolon, 7)) {
			analyze_with_userprio = false;
		}
		else if (is_dash_arg_prefix(dash_arg, "version", 1)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit(0);
		}
		else if (is_dash_arg_colon_prefix(dash_arg, "profile", &pcolon, 4)) {
			dash_profile = true;
			if (pcolon) {
				StringList opts(++pcolon, ",:");
				opts.rewind();
				while (const char * popt = opts.next()) {
					if (is_arg_prefix(popt, "on", 2)) {
						classad::ClassAdSetExpressionCaching(true);
					} else if (is_arg_prefix(popt, "off", 2)) {
						classad::ClassAdSetExpressionCaching(false);
					}
				}
			}
		}
		else
		if (is_dash_arg_prefix (dash_arg, "stream-results", 2)) {
			g_stream_results = true;
			if( dash_dag || (qdo_mode == QDO_Progress)) {
				fprintf( stderr, "-stream-results and -dag or -batch are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else {
			fprintf( stderr, "Error: unrecognized argument %s\n", dash_arg );
			usage(argv[0]);
			exit( 1 );
		}
	}

	// when we get to here, the command line arguments have been processed
	// now we can work out some of the implications

	// the -unmatchable flag is either a query modifier, or changes the output of -analyze
	// for now, it doesn't work with -batch, -autocluster or -group-by
	if (dash_unmatchable) {
		if (better_analyze) {
			analysis_mode = anaModeUnmatchable;
		} else if (dash_batch_specified) {
			fprintf( stderr, "Error: -batch conflicts with -unmatchable\n");
			exit( 1 );
		} else if (dash_autocluster) {
			fprintf( stderr, "Error: -autocluster conflicts with -unmatchable\n");
			exit( 1 );
		}
		dash_batch_is_default = false;
	}

	// parse the autoformat args and use them to set prmask or sumymask and the projection
	if ( ! autoformat_args.empty()) {
		auto_standard_summary = false; // we will either have a custom summary, or none.

		int nargs = (int)autoformat_args.size();
		autoformat_args.push_back(NULL); // have the last argument be NULL, like argv[cargs] is.
		classad::References refs;
		if (dash_tot) {
			customHeadFoot = (printmask_headerfooter_t)(HF_NOHEADER | HF_NOTITLE | HF_CUSTOM);
			parse_format_args(nargs, &autoformat_args[0], app.sumymask, refs, dash_dry_run);
		} else {
			customHeadFoot = (printmask_headerfooter_t)(HF_BARE | HF_CUSTOM);
			parse_format_args(nargs, &autoformat_args[0], app.prmask, refs, dash_dry_run);

			// if not querying only totals, or querying from the autocluser, we MUST have cluser and proc as part of the projection
			if ( ! dash_autocluster) {
				refs.insert(ATTR_CLUSTER_ID);
				refs.insert(ATTR_PROC_ID);
			}
			app.attrs.insert(refs.begin(), refs.end());
		}
		if (app.prmask.has_headings()) {
			customHeadFoot = (printmask_headerfooter_t)(customHeadFoot & ~HF_NOHEADER);
		}
	}

	if (dash_long) {
		customHeadFoot = HF_BARE;
		if (dash_tot) {
			customHeadFoot = (printmask_headerfooter_t)(customHeadFoot & ~HF_NOSUMMARY);
		}
	} else {
		// default batch mode to on if appropriate
		if ( ! dash_batch_specified && ! dash_autocluster && ! show_held) {
			int mode = qdo_mode & QDO_BaseMask;
			if (mode == QDO_NotSet ||
				mode == QDO_JobNormal ||
				mode == QDO_JobRuntime || // TODO: need a custom format for -batch -run
			//	mode == QDO_JobIdle || // TODO: need a custom format for -batch -idle
				mode == QDO_DAG) { // DAG and batch go naturally together
				if ( ! dash_factory && ! dash_run && ! dash_idle && ! dash_jobset) {
					dash_batch = dash_batch_is_default;
				}
			}
		}

		// for now, can't use both -batch and one of the aggregation modes.
		if (dash_autocluster && (dash_batch || dash_jobset)) {
			if (dash_batch_specified) {
				const char * dashopt = dash_jobset ? "-jobset" :  "-autocluster";
				fprintf( stderr, "Error: -batch conflicts with %s\n",
					(dash_autocluster == QueryFetchOpts::fetch_GroupBy) ? "-group-by" : dashopt );
				exit( 1 );
			}
			dash_batch = false;
		}
	}

	if (dash_dry_run) {
		const char * const amo[] = { "", "normal", "run", "goodput", "grid", "grid:ec2", "hold", "io", "factory", "dag", "totals", "batch", "autocluster", "custom", "analyze" };
		fprintf(stderr, "\ncondor_q %s %s\n", amo[qdo_mode & QDO_BaseMask], dash_long ? "-long" : "");
	}
	if ( ! dash_long && ! (qdo_mode & QDO_Format) && (qdo_mode & QDO_BaseMask) < QDO_Custom) {
		// if the user did not specify -wide or -wide:<num>, and the width of the screen cannot be
		// determined (because we are writing to a pipe or file), then don't truncate the output to fit the screen
		if ( ! dash_wide && (getConsoleWindowSize() < 0)) widescreen = true;
		initOutputMask(app.prmask, qdo_mode, widescreen);
	} else {
		// handle flags that just set a constraint when used with a formatting option, but
		// set a constraint and a format when used alone.
		if (dash_factory) {
			if (dash_factory & 1) Q.addAND("ProcId is undefined");
			if (dash_factory & 2) Q.addAND("JobMaterializeDigestFile isnt undefined");
		}
	}

	// convert cluster and cluster.proc into constraints
	// if there is a -dag argument, then we look up all children of the dag
	// as well as the dag itself.
	if ( ! constrID.empty()) {
		// dont default to 'my jobs'
		default_fetch_opts &= ~QueryFetchOpts::fetch_MyJobs;

		for (std::vector<CondorID>::const_iterator it = constrID.begin(); it != constrID.end(); ++it) {

			// add a constraint to match the jobid.
			if (it->_proc >= 0) {
				snprintf(constraint, sizeof(constraint), ATTR_CLUSTER_ID " == %d && " ATTR_PROC_ID " == %d", it->_cluster, it->_proc);
				querying_partial_clusters = true;
			} else {
				snprintf(constraint, sizeof(constraint), ATTR_CLUSTER_ID " == %d", it->_cluster);
			}
			Q.addOR(constraint);

			// if we are doing -dag output, then also request any jobs that are inside this dag.
			// we know that a jobid for a dagman job will always never have a proc > 0
			if ((dash_dag || dash_batch) && it->_proc < 1) {
				snprintf(constraint, sizeof(constraint), ATTR_DAGMAN_JOB_ID " == %d", it->_cluster);
				Q.addOR(constraint);
				snprintf(constraint, sizeof(constraint), "int(split(" ATTR_JOB_BATCH_ID ", \".\")[0]) == %d", it->_cluster);
				Q.addOR(constraint);
			}
		}
	} else if (show_io) {
	// InitOutputMask does this now
	}
}

static double
job_time(double cpu_time,ClassAd *ad)
{
	if ( cputime ) {
		return cpu_time;
	}

		// here user wants total wall clock time, not cpu time
	int job_status = 0;
	int cur_time = 0;
	int shadow_bday = 0;
	double previous_runs = 0;

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
	double total_wall_time = previous_runs;
	if ( ( job_status == RUNNING || job_status == TRANSFERRING_OUTPUT || job_status == SUSPENDED) && shadow_bday ) {
		total_wall_time += cur_time - shadow_bday;
	} else if ( (job_status == IDLE || job_status == HELD || job_status == JOB_STATUS_BLOCKED) && current_run) {
		ad->LookupFloat( ATTR_JOB_LAST_REMOTE_WALL_CLOCK, total_wall_time );
	}

	return total_wall_time;
}


static bool
local_render_remote_host (std::string & result, ClassAd *ad, Formatter &)
{
	//static char host_result[MAXHOSTNAMELEN];
	//static char unknownHost [] = "[????????????????]";
	condor_sockaddr addr;

	int universe = CONDOR_UNIVERSE_VANILLA;
	ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	if (((universe == CONDOR_UNIVERSE_SCHEDULER) || (universe == CONDOR_UNIVERSE_LOCAL)) &&
		addr.from_sinful(scheddAddr) == true) {
		result = get_hostname(addr);
		return result.length();
	} else if (universe == CONDOR_UNIVERSE_GRID) {
		if (ad->LookupString(ATTR_EC2_REMOTE_VM_NAME,result) == 1)
			return true;
		else if (ad->LookupString(ATTR_GRID_RESOURCE,result) == 1 )
			return true;
		else
			return false;
	}

	if (ad->LookupString(ATTR_REMOTE_HOST, result) == 1) {
		if( is_valid_sinful(result.c_str()) &&
			addr.from_sinful(result.c_str()) == true ) {
			result = get_hostname(addr);
			return result.length() > 0;
		}
		return true;
	}
	return false;
}

static bool
local_render_cpu_time (double & cputime, ClassAd *ad, Formatter &)
{
	if ( ! ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, cputime))
		return false;

	cputime = job_time(cputime, ad);
	//return format_time( (int) job_time(utime,(ClassAd *)ad) );
	return true;
}

static bool
local_render_memory_usage(double & mem_used_mb, ClassAd *ad, Formatter &)
{
	long long  image_size;
	long long memory_usage;
	// print memory usage unless it's unavailable, then print image size
	// note that memory usage is megabytes but imagesize is kilobytes.
	if (ad->LookupInteger(ATTR_MEMORY_USAGE, memory_usage)) {
		mem_used_mb = memory_usage;
		max_mem_used = MAX(max_mem_used, mem_used_mb);
	} else if (ad->LookupInteger(ATTR_IMAGE_SIZE, image_size)) {
		mem_used_mb = image_size / 1024.0;
		max_mem_used = MAX(max_mem_used, mem_used_mb);
	} else {
		return false;
	}
	return true;
}

static bool
local_render_job_status_char (std::string & result, ClassAd*ad, Formatter &)
{
	int job_status;
	if ( ! ad->LookupInteger(ATTR_JOB_STATUS, job_status))
		return false;

	char put_result[3];
	put_result[1] = ' ';
	put_result[2] = 0;

	put_result[0] = encode_status(job_status);
	/* The suspension of a job is a second class citizen and is not a true
	*  status that can exist as a job status ad and is instead
	*  inferred, so therefore the processing and display of
	*  said suspension is also second class.
	*/
	if (param_boolean("REAL_TIME_JOB_SUSPEND_UPDATES", false)) {
		int last_susp_time;
		if (!ad->LookupInteger(ATTR_LAST_SUSPENSION_TIME,last_susp_time))
		{
			last_susp_time = 0;
		}
		/* sanity check the last_susp_time against if the job is running
		*  or not in case the schedd hasn't synchronized the
		*  last suspension time attribute correctly to job running
		*  boundaries.
		*/
		if ( job_status == RUNNING && last_susp_time != 0 )
		{
			put_result[0] = 'S';
		}
	}

	// adjust status field to indicate file transfer status
	bool transferring_input = false;
	bool transferring_output = false;
	bool transfer_queued = false;
	ad->LookupBool(ATTR_TRANSFERRING_INPUT,transferring_input);
	ad->LookupBool(ATTR_TRANSFERRING_OUTPUT,transferring_output);
	ad->LookupBool(ATTR_TRANSFER_QUEUED,transfer_queued);
	if( transferring_input ) {
		put_result[0] = '<';
		put_result[1] = transfer_queued ? 'q' : ' ';
	}
	if( transferring_output || job_status == TRANSFERRING_OUTPUT) {
		put_result[0] = transfer_queued ? 'q' : ' ';
		put_result[1] = '>';
	}
	result = put_result;
	return true;
}

static bool
local_render_owner(std::string & out, ClassAd *ad, Formatter & /*fmt*/)
{
	if ( ! ad->LookupString(ATTR_OWNER, out))
		return false;

#ifdef NO_DEPRECATE_NICE_USER
	int niceUser;
	if (ad->LookupInteger( ATTR_NICE_USER, niceUser) && niceUser ) {
		char tmp[sizeof(NiceUserName)+2];
		strcpy(tmp, NiceUserName);
		strcat(tmp, ".");
		out.insert(0, tmp);
	}
#endif
	max_owner_name = MAX(max_owner_name, (int)out.length());
	return true;
}

static bool
local_render_dag_owner (std::string & out, ClassAd *ad, Formatter & fmt)
{
	if (dash_dag && ad->LookupExpr(ATTR_DAGMAN_JOB_ID)) {
		if (ad->LookupString(ATTR_DAG_NODE_NAME, out)) {
			max_owner_name = MAX(max_owner_name, (int)out.length()+3);
			return true;
		} else {
			fprintf(stderr, "DAG node job with no %s attribute!\n", ATTR_DAG_NODE_NAME);
		}
	}
	return local_render_owner(out, ad, fmt);
}

static bool
local_render_batch_name (std::string & out, ClassAd *ad, Formatter & /*fmt*/)
{
	const bool fold_dagman_sibs = dash_batch && (dash_batch & 2); // hack -batch:2 gives experimental behaviour

	int universe = 0;
	std::string tmp;
	if (ad->LookupString(ATTR_JOB_BATCH_NAME, out)) {
		// got it.
	} else if ( ! fold_dagman_sibs && ad->LookupInteger(ATTR_JOB_UNIVERSE, universe) && universe == CONDOR_UNIVERSE_SCHEDULER) {
		// set batch name to dag id, but not if we allow folding of multiple root dagmans into a single batchname
		int cluster = 0;
		ad->LookupInteger(ATTR_CLUSTER_ID, cluster);
		formatstr(out, "DAG: %d", cluster);
	} else if (ad->LookupExpr(ATTR_DAGMAN_JOB_ID)
				&& ad->LookupString(ATTR_DAG_NODE_NAME, out)) {
		out.insert(0,"NODE: ");
#if 1 // don't batch jobs by exe
#else // batch jobs that have a common exe together
	} else if (ad->LookupString(ATTR_JOB_CMD, tmp)) {
		const char * name = tmp.c_str();
		if (tmp.length() > 24) { name = condor_basename(name); }
		formatstr(out, "CMD: %s", name);
#endif
	} else {
		return false;
	}
	max_batch_name = MAX(max_batch_name, (int)out.length());
	return true;
}

static bool
local_render_grid_resource(std::string & result, ClassAd * ad, Formatter & /*fmt*/ )
{
	std::string grid_type;
	std::string str;
	std::string mgr = "[?]";
	std::string host = "[???]";
	const bool fshow_host_port = false;
	const size_t width = 1+6+1+8+1+18+1;

	if ( ! ad->LookupString(ATTR_GRID_RESOURCE, str))
		return false;

	// GridResource is a string with the format 
	//      "type host_url manager" (where manager can contain whitespace)
	// or   "type host_url/jobmanager-manager"
	size_t ixHost = str.find_first_of(' ');
	if (ixHost < str.length()) {
		grid_type = str.substr(0, ixHost);
		ixHost += 1; // skip over space.
	} else {
		grid_type = "globus";
		ixHost = 0;
	}

	size_t ix2 = str.find_first_of(' ', ixHost);
	if (ix2 < str.length()) {
		mgr = str.substr(ix2+1);
	} else {
		size_t ixMgr = str.find("jobmanager-", ixHost);
		if (ixMgr < str.length()) 
			mgr = str.substr(ixMgr+11);	//sizeof("jobmanager-") == 11
		ix2 = ixMgr;
	}

	size_t ix3 = str.find("://", ixHost);
	ix3 = (ix3 < str.length()) ? ix3+3 : ixHost;
	size_t ix4 = str.find_first_of(fshow_host_port ? "/" : ":/",ix3);
	if (ix4 > ix2) ix4 = ix2;
	host = str.substr(ix3, ix4-ix3);

	replace_str(mgr, " ", "/");

    static char result_str[1024];
    if( MATCH == grid_type.compare( "ec2" ) ) {
        // mgr = str.substr( ixHost, ix3 - ixHost - 3 );
        char rvm[MAXHOSTNAMELEN];
        if( ad->LookupString( ATTR_EC2_REMOTE_VM_NAME, rvm, sizeof( rvm ) ) ) {
            host = rvm;
        }
        
        snprintf( result_str, 1024, "%s %s", grid_type.c_str(), host.c_str() );
    } else {
    	snprintf(result_str, 1024, "%s->%s %s", grid_type.c_str(), mgr.c_str(), host.c_str());
    }
	result_str[COUNTOF(result_str)-1] = 0;

	ix2 = strlen(result_str);
	if ( ! widescreen) ix2 = width;
	result_str[ix2] = 0;
	result = result_str;
	return true;
}

static void
usage (const char *myName, int other)
{
	printf ("Usage: %s [general-opts] [restriction-list] [output-opts | analyze-opts]\n", myName);
	printf ("\n    [general-opts] are\n"
		"\t-global\t\t\t Query all Schedulers in this pool\n"
		"\t-schedd-constraint\t Query all Schedulers matching this constraint\n"
		"\t-submitter <submitter>\t Get queue of specific submitter\n"
		"\t-name <name>\t\t Name of Scheduler\n"
		"\t-pool <host>\t\t Use host as the central manager to query\n"
		"\t-jobads[:<form>] <file>\t Read queue from a file of job ClassAds\n"
		"\t           where <form> is one of:\n"
		"\t       auto    default, guess the format from reading the input stream\n"
		"\t       long    The traditional -long form\n"
		"\t       xml     XML form, the same as -xml\n"
		"\t       json    JSON classad form, the same as -json\n"
		"\t       new     'new' classad form without newlines\n"
		"\t-userlog <file>\t\t Read queue from a user log file\n"
		);

	printf ("\n    [restriction-list] each restriction may be one of\n"
		"\t<cluster>\t\t Get information about specific cluster\n"
		"\t<cluster>.<proc>\t Get information about specific job\n"
		"\t<owner>\t\t\t Information about jobs owned by <owner>\n"
		"\t-factory\t\t Get information about late materialization job factories\n"
		"\t-jobset\t\t\t Get information from jobset ads\n"
		"\t-autocluster\t\t Get information about the SCHEDD's autoclusters\n"
		"\t-constraint <expr>\t Get information about jobs that match <expr>\n"
		"\t-unmatchable\t\t Get information about jobs that do not match any machines\n"
		"\t-allusers\t\t Consider jobs from all users\n"
		);

	printf ("\n    [output-opts] are\n"
		"\t-limit <num>\t\t Limit the number of results to <num>\n"
		"\t-cputime\t\t Display CPU_TIME instead of RUN_TIME\n"
		"\t-currentrun\t\t Display times only for current run\n"
		"\t-debug\t\t\t Display debugging info to console\n"
		"\t-dag\t\t\t Sort DAG jobs under their DAGMan\n"
		"\t-expert\t\t\t Display shorter error messages\n"
		"\t-grid\t\t\t Get information about grid jobs\n"
		"\t-goodput\t\t Display job goodput statistics\n"
		"\t-help [Universe|State]\t Display this screen, JobUniverses, JobStates\n"
		"\t-hold\t\t\t Get information about jobs on hold\n"
		"\t-io\t\t\t Display information regarding I/O\n"
		"\t-batch\t\t\t Display DAGs or batches of similar jobs as a single line\n"
		"\t-nobatch\t\t Display one line per job, rather than one line per batch\n"
//FUTURE		"\t-transfer\t\t Display information for jobs that are doing file transfer\n"
		"\t-idle\t\t\t Get information about idle jobs\n"
		"\t-run\t\t\t Get information about running jobs\n"
		"\t-totals\t\t\t Display only job totals\n"
		"\t-stream-results \t Produce output as jobs are fetched\n"
		"\t-version\t\t Print the HTCondor version and exit\n"
		"\t-wide[:<width>]\t\t Don't truncate data to fit in 80 columns.\n"
		"\t\t\t\t Truncates to console width or <width> argument.\n"
		"\t-autoformat[:jlhVr,tng] <attr> [<attr2> [...]]\n"
		"\t-af[:jlhVr,tng] <attr> [attr2 [...]]\n"
		"\t    Print attr(s) with automatic formatting\n"
		"\t    the [jlhVr,tng] options modify the formatting\n"
		"\t        j   Display Job id\n"
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
		"\t-format <fmt> <attr>\t Print attribute attr using format fmt\n"
		"\t-print-format <file>\t Use <file> to set display attributes and formatting\n"
		"\t\t\t\t (experimental, see htcondor-wiki for more information)\n"
		"\t-long[:<form>]\t\t Display entire ClassAds in <form> format\n"
		"\t\t\t\t See -jobads for <form> choices\n"
		"\t-xml\t\t\t Display entire ClassAds in XML form\n"
		"\t-json\t\t\t Display entire ClassAds in JSON form\n"
		"\t-attributes X,Y,...\t Attributes to show in -xml, -json, and -long\n"
		);

	printf ("\n    [analyze-opts] are\n"
		"\t-analyze[:<qual>]\t Perform matchmaking analysis on jobs\n"
		"\t-better-analyze[:<qual>] Perform more detailed match analysis\n"
		"\t    <qual> is a comma separated list of one or more of\n"
		"\t    priority\tConsider user priority during analysis\n"
		"\t    summary\tShow a one-line summary for each job or machine\n"
		"\t    reverse\tAnalyze machines rather than jobs\n"
		"\t-machine <name>\t\t Machine name or slot name for analysis\n"
		"\t-mconstraint <expr>\t Machine constraint for analysis\n"
		"\t-slotads[:<form>] <file> Read Machine ClassAds for analysis from <file>\n"
		"\t\t\t\t <file> can be the output of condor_status -long\n"
		"\t-userprios <file>\t Read user priorities for analysis from <file>\n"
		"\t\t\t\t <file> can be the output of condor_userprio -l\n"
		"\t-nouserprios\t\t Don't consider user priority during analysis (default)\n"
		"\t-reverse-analyze\t Analyze Machine requirements against jobs\n"
		"\t-verbose\t\t Show progress and machine names in results\n"
		"\n"
		);

	printf ("\n    Only information about jobs owned by the current user will be returned.\n"
			"This default is overridden when the restriction list has usernames and/or\n"
			"job ids, when the -submitter or -allusers arguments are specified, or\n"
			"when the current user is a queue superuser\n"
		"\n"
		);

	if (other & usage_DiagOpts) {
		printf("There are options for testing condor_q itself.  they are\n"
			"\t-dry-run[:<file>]\t Print classad formatting configuration and query information\n"
			"\t                 \t but do not do the query. If a <file> is specified dry-run\n"
			"\t                 \t output is written to a file. otherwise to stdout.\n"
			"\t-capture[:[+]<file>]\t Print the raw classads that are the result of the query as\n"
			"\t                    \t they arrive. If a <file> is specified the classads are printed\n"
			"\t                    \t to the file. if the filename is prefixed with + then ads are\n"
			"\t                    \t appended to the file. Filename can be - to print to stdout and\n"
			"\t                    \t -2 to print to stderr. default is to print to stdout.\n"
			"\n    The file produced by -capture-raw-results can be used with the -jobads argument to reproduce\n"
			"the results a particular query repeatedly, and to see how a projection is expanded to pick up\n"
			"referenced attributes.\n"
		);
		printf("\n");
	}

	if (other & usage_Universe) {
		printf("    %s codes:\n", ATTR_JOB_UNIVERSE);
		for (int uni = CONDOR_UNIVERSE_MIN+1; uni < CONDOR_UNIVERSE_MAX; ++uni) {
			if (uni == CONDOR_UNIVERSE_PIPE) { // from PIPE -> Linda is obsolete.
				uni = CONDOR_UNIVERSE_LINDA;
				continue;
			}
			printf("\t%2d %s\n", uni, CondorUniverseNameUcFirst(uni));
		}
		printf("\n");
	}
	if (other & usage_JobStatus) {
		printf("    %s codes:\n", ATTR_JOB_STATUS);
		for (int st = JOB_STATUS_MIN; st <= JOB_STATUS_MAX; ++st) {
			printf("\t%2d %c %s\n", st, encode_status(st), getJobStatusString(st));
		}
		printf("\n");
	}
	if (other & usage_SubmitMethod) {
		printf("    %s codes:\n", ATTR_JOB_SUBMIT_METHOD);
		printf("\t%3s  %s\n", "UND", "Undefined");
		for (int met = JOB_SUBMIT_METHOD_MIN; met <= JOB_SUBMIT_METHOD_MAX; ++met) {
			std::string display = getSubmitMethodString(met);
			//If number yeilds final enum key then output and break out of loop
			if(display.compare("Portal/User-Set") == 0){
				printf("\t%3d+ %s\n", JSM_USER_SET, display.c_str());
				break;
			}
			printf("\t%3d  %s\n", met, display.c_str());
		}
		printf("\n");
	}
}


static void
print_full_footer(ClassAd * summary_ad, CondorClassAdListWriter * writer)
{
#if 1
	if (customHeadFoot & HF_NOSUMMARY) {
		return;
	}

	std::string text;
	text.reserve(4096);
	if ( ! dash_tot || ( ! dash_long && use_legacy_standard_summary)) {
		// legacy mode has a blank linke before to totals line with -tot mode
		text = "\n";
	}

	if (dash_long) {
		if (writer && summary_ad) {
			writer->appendAd(*summary_ad, text, NULL, print_attrs_in_hash_order);
		}
	} else {
		if (auto_standard_summary && summary_ad) {
			init_standard_summary_mask(summary_ad);
		}
		if (app.sumymask.has_headings()) {
			app.sumymask.display_Headings(stdout);
		}
		app.sumymask.display(text, summary_ad);
		text += "\n";
	}
	fputs(text.c_str(), stdout);
#else
	// If we want to summarize, do that too.
	if( ! (customHeadFoot && HF_NOSUMMARY) ) {
		printf( "\n%d jobs; "
				"%d completed, %d removed, %d idle, %d running, %d held, %d suspended",
				idle+running+held+malformed+suspended+completed+removed,
				completed,removed,idle,running,held,suspended);
		if (malformed>0) printf( ", %d malformed",malformed);
		printf("\n");

		if (summary_ad) {
			AttrListPrintMask prtot;
			StringList totattrs;
			std::string text;

			static const char * totfmt = "SELECT\n"
				"JobsCompleted AS Completed PRINTF 'Total for query: %d Completed,'\n"
				"JobsRemoved AS Removed PRINTF '%d Removed,'\n"
				"JobsIdle AS Idle PRINTF '%d Idle,'\n"
				"JobsRunning AS Running PRINTF '%d Running,'\n"
				"JobsHeld AS Held PRINTF '%d Held,'\n"
				"JobsSuspended AS Suspended PRINTF '%d Suspended'\n"
				
				"MyJobsCompleted AS Completed PRINTF '\\nTotal for user: %d Completed,'\n"
				"MyJobsRemoved AS Removed PRINTF '%d Removed,'\n"
				"MyJobsIdle AS Idle PRINTF '%d Idle,'\n"
				"MyJobsRunning AS Running PRINTF '%d Running,'\n"
				"MyJobsHeld AS Held PRINTF '%d Held,'\n"
				"MyJobsSuspended AS Suspended PRINTF '%d Suspended'\n"

				"AllusersJobsCompleted AS Idle PRINTF '\\nTotal for all users: %d Completed,'\n"
				"AllusersJobsRemoved AS Idle PRINTF '%d Removed,'\n"
				"AllusersJobsIdle AS Idle PRINTF '%d Idle,'\n"
				"AllusersJobsRunning AS Running PRINTF '%d Running,'\n"
				"AllusersJobsHeld AS Held PRINTF '%d Held,'\n"
				"AllusersJobsSuspended AS Suspended PRINTF '%d Suspended'\n"
				;
			set_print_mask_from_stream(prtot, totfmt, false, totattrs);
			prtot.display(text, summary_ad);

			//CondorClassAdListWriter writer;
			//writer.appendAd(*summary_ad, text);
			printf("%s\n", text.c_str());

		}
	}
#endif
}

static void
print_full_header(const char * source_label)
{
	if ( ! dash_long) {
		// print the source label.
		if ( ! (customHeadFoot&HF_NOTITLE)) {
			static bool first_time = false;
			char time_str[80]; time_str[0] = 0;
			if (append_time_to_source_label) {
				if ( ! queue_time) { queue_time = time(NULL); }
				strftime(time_str, 80, " @ %m/%d/%y %H:%M:%S", localtime(&queue_time));
			}
			printf ("%s-- %s%s\n", (first_time ? "" : "\n\n"), source_label, time_str);
			first_time = false;
		}
		if ( ! (customHeadFoot & HF_NOHEADER)) {
			// Print the output header
			if (app.prmask.has_headings()) {
				app.prmask.display_Headings(stdout);
			}
		}
	}
}

/* print-format definition for default "bufferJobShort" output
SELECT
   ClusterId     AS " ID"  NOSUFFIX WIDTH 4
   ProcId        AS " "    NOPREFIX             PRINTF ".%-3d"
   Owner         AS "OWNER"         WIDTH -14   PRINTAS OWNER
   QDate         AS "  SUBMITTED"   WIDTH 11    PRINTAS QDATE
   RemoteUserCpu AS "    RUN_TIME"  WIDTH 12    PRINTAS CPU_TIME
   JobStatus     AS ST                          PRINTAS JOB_STATUS
   JobPrio       AS PRI
   ImageSize     AS SIZE            WIDTH 4     PRINTAS MEMORY_USAGE
   Cmd           AS CMD             WIDTH -18   PRINTAS JOB_DESCRIPTION
SUMMARY STANDARD
*/

extern const char * const jobDefault_PrintFormat;
extern const char * const jobRuntime_PrintFormat;
extern const char * const jobIdle_PrintFormat;
extern const char * const jobGoodput_PrintFormat;
extern const char * const jobGrid_PrintFormat;
extern const char * const jobGridEC2_PrintFormat;
extern const char * const jobHold_PrintFormat;
extern const char * const jobIO_PrintFormat;
extern const char * const jobFactory_PrintFormat;
extern const char * const jobDAG_PrintFormat;
extern const char * const jobTotals_PrintFormat;
extern const char * const jobProgress_PrintFormat; // NEW, summarize batch progress
extern const char * const autoclusterNormal_PrintFormat;
extern const char * const jobsetNormal_PrintFormat;

static void initOutputMask(AttrListPrintMask & prmask, int qdo_mode, bool wide_mode)
{
	// just  in case we re-enter this function, only setup the mask once
	static bool	setup_mask = false;
	// TJ: before my 2012 refactoring setup_mask didn't protect summarize, so I'm preserving that.
#if 1
	//PRAGMA_REMIND("tj: do I need to do anything to adjust the summarize mask here?")
#else
	if ( dash_run || dash_goodput || dash_grid ) 
		summarize = false;
	else if ((customHeadFoot&HF_NOSUMMARY) && ! show_held)
		summarize = false;
#endif

	if (setup_mask)
		return;
	setup_mask = true;

	// If no display mode has been set, pick one.
	if ((qdo_mode & QDO_BaseMask) == QDO_NotSet) {
		if (dash_autocluster == QueryFetchOpts::fetch_DefaultAutoCluster) { qdo_mode = QDO_AutoclusterNormal; }
		else if (dash_jobset == QueryFetchOpts::fetch_IncludeJobsetAds) { qdo_mode = QDO_JobsetNormal; }
		else { 
			int mode = QDO_JobNormal;
			if (show_held) {
				mode = QDO_JobHold;
				wide_mode = true;
			} else if (dash_batch) {
				mode = QDO_Progress;
			} else if (dash_dag) {
				mode = QDO_DAG;
			} else if (dash_run) {
				mode = QDO_JobRuntime;
			} else if (dash_idle) {
				mode = QDO_JobIdle;
			} else if (dash_factory) {
				mode = QDO_Factory;
			}
			qdo_mode |= mode;
		}
	}

	static const struct {
		const int mode;
		const char * tag;
		const char * fmt;
	} info[] = {
		{ QDO_JobNormal,      "",         jobDefault_PrintFormat },
		{ QDO_JobRuntime,     "RUN",      jobRuntime_PrintFormat },
		{ QDO_JobIdle,        "IDLE",     jobIdle_PrintFormat },
		{ QDO_JobGoodput,     "GOODPUT",  jobGoodput_PrintFormat },
		{ QDO_JobGridInfo,    "GRID",     jobGrid_PrintFormat },
		{ QDO_JobGridEC2Info, "GRID_EC2", jobGridEC2_PrintFormat },
		{ QDO_JobHold,        "HOLD",     jobHold_PrintFormat },
		{ QDO_JobIO,          "IO",       jobIO_PrintFormat },
		{ QDO_DAG,            "DAG",      jobDAG_PrintFormat },
		{ QDO_Factory,        "FACTORY",  jobFactory_PrintFormat },
		{ QDO_Totals,         "TOTALS",   jobTotals_PrintFormat },
		{ QDO_Progress,       "PROGRESS", jobProgress_PrintFormat },
		{ QDO_AutoclusterNormal, "AUTOCLUSTER", autoclusterNormal_PrintFormat },
		{ QDO_JobsetNormal,   "JOBSET",   jobsetNormal_PrintFormat },
	};

	int ixInfo = -1;
	for (int ix = 0; ix < (int)COUNTOF(info); ++ix) {
		if (info[ix].mode == (qdo_mode & QDO_BaseMask)) {
			ixInfo = ix;
			break;
		}
	}

	if (ixInfo < 0)
		return;

	const char * tag = info[ixInfo].tag;

	// if there is a user-override output mask, then use that instead of the code below
	if ( ! disable_user_print_files) {
		std::string param_name("Q_DEFAULT_");
		if (tag[0]) {
			param_name += tag;
			param_name += "_";
		}
		param_name += "PRINT_FORMAT_FILE";

		auto_free_ptr pf_file(param(param_name.c_str()));
		if (pf_file) {
			struct stat stat_buff;
			if (0 != stat(pf_file.ptr(), &stat_buff)) {
				// do nothing, this is not an error.
			} else if (set_print_mask_from_stream(prmask, pf_file.ptr(), true, app.attrs, app.sumymask) < 0) {
				fprintf(stderr, "Warning: default %s print-format file '%s' is invalid\n", tag, pf_file.ptr());
			} else {
				//if (customHeadFoot&HF_NOSUMMARY) summarize = false;
				return;
			}
		}
	}

	const char * fmt = info[ixInfo].fmt;
	std::string alt_fmt;
	if (cputime) {
		alt_fmt = fmt;
		size_t ix = alt_fmt.find("RUN_TIME");
		if (ix != std::string::npos) {
			alt_fmt[ix] = 'C';
			alt_fmt[ix+1] = 'P';
			alt_fmt[ix+2] = 'U';
			fmt = alt_fmt.c_str();
		}
	}

	is_standard_format = true;
	has_owner_column = first_col_is_job_id = qdo_mode < QDO_Totals;
	if ( ! wide_mode) {
		int display_wid = getDisplayWidth();
		prmask.SetOverallWidth(display_wid-1);
		max_name_column_width = 14 + (display_wid - 80);
		if (dash_dag) {
			max_name_column_width += 3;
			name_column_width = 17;
		}
	}

	if (set_print_mask_from_stream(prmask, fmt, false, app.attrs, app.sumymask) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}

#if 1
	//PRAGMA_REMIND("tj: do I need to do anything to adjust the app.headfoot?")
#else
	customHeadFoot = (printmask_headerfooter_t)(customHeadFoot & ~HF_CUSTOM);
	if (customHeadFoot&HF_NOSUMMARY) summarize = false;
#endif
}


static void count_job(LiveJobCounters & num, ClassAd *job)
{
#ifdef CONDOR_Q_HANDLE_CLUSTER_AD
	if ( ! job->Lookup(ATTR_PROC_ID)) return;
#endif

	int status = 0, universe = CONDOR_UNIVERSE_VANILLA;
	job->LookupInteger(ATTR_JOB_STATUS, status);
	job->LookupInteger(ATTR_JOB_UNIVERSE, universe);
	if (status == TRANSFERRING_OUTPUT) status = RUNNING;
	switch (universe) {
	case CONDOR_UNIVERSE_SCHEDULER:
		if (status > 0 && status <= HELD) {
			(&num.SchedulerJobsIdle)[status-1] += 1;
		}
		break;
	/*
	case CONDOR_UNIVERSE_LOCAL:
		if (status > 0 && status <= HELD) {
			(&num.LocalJobsIdle)[status-1] += increment;
		}
		break;
	*/
	default:
		if (status > 0 && status <= HELD) {
			(&num.JobsIdle)[status-1] += 1;
		} else if (status == SUSPENDED) {
			num.JobsSuspended += 1;
		}
		break;
	}
}

union _jobid {
	struct { int proc; int cluster; };
	long long id;
};

static union _jobid sequence_id = { {0, INT_MAX} };
static bool assume_cluster_ad_if_no_proc_id = false; // set to true when we expect to get clusterad ads that don't have a ProcId attribute

// callback function for processing a job from the Q query that just adds the job into a IdToClassaAdMap.
static bool AddJobToClassAdCollection(void * pv, ClassAd* ad) {
	IdToClassaAdMap * pmap = (IdToClassaAdMap*)pv;

	// if doing -unmatchable filtering, and NOT doing analysis output, just skip jobs that match here
	if (dash_unmatchable && ! better_analyze) {
		anaCounters ac;
		ac.clear();
		std::string job_status;
		doJobRunAnalysis(ad, NULL, job_status, anaMatchModePslot, ac, NULL, NULL);
		if (ac.both_match) {
			return true;
		}
	}

	count_job(app.sumy, ad);

	// default to using the sequence number as the unique id for the ad.
	union _jobid jobid = sequence_id;
	sequence_id.proc += 1;

	if (dash_autocluster) {
		const char * attr_id = ATTR_AUTO_CLUSTER_ID;
		if (dash_autocluster == QueryFetchOpts::fetch_GroupBy) attr_id = "Id";
		ad->LookupInteger(attr_id, jobid.id);
	} else {
		if ( ! ad->LookupInteger(ATTR_CLUSTER_ID, jobid.cluster)) {
			// Classads that have no ClusterId, but do have a JobSetId must be JOBSET ads
			// we will sort these as cluster=0, proc=jobsetid
			if (ad->LookupInteger(ATTR_JOB_SET_ID, jobid.proc)) {
				jobid.cluster = 0;
			}
		}
		if ( ! ad->LookupInteger( ATTR_PROC_ID, jobid.proc ) && jobid.cluster > 0 && assume_cluster_ad_if_no_proc_id) {
			jobid.proc = -1;
		}
	}

	auto pp = pmap->insert(std::pair<long long, UniqueClassAdPtr>(jobid.id,UniqueClassAdPtr()));
	if ( ! pp.second) {
		fprintf( stderr, "Error: Two results with the same ID.\n" );
		// return true to indicate that the caller still owns the ad.
		return true;
	} else {
		// give the ad pointer to the map, it will now be responsible for freeing it.
		pp.first->second.reset(ad);
	}

	return false; // return false to indicate we took ownership of the ad.
}


typedef std::map<long long, long long>   IdToIdMap;    // maps a integer key into another integer key
typedef std::map<std::string, long long> KeyToIdMap; // maps a string key into a index in the JobDisplayData vector
typedef std::map<long long, std::string> IdToKeyMap; // maps a string key into a index in the JobDisplayData vector
typedef std::map<int, std::set<int> >    IdToIdsMap;   // maps a integer key into a list of integer keys, use for dagid->[clusters] or clusterid->[procs]

ROD_MAP_BY_ID rod_result_map;   // jobs are rendered and inserted here before processing and printing
KeyToIdMap    rod_sort_key_map; // this indexes the above when we want to sort by something other than id
//IdToIdsMap    dag_to_cluster_map;

// this code assumes that the proc id of a dagman job will always be 0.
// and returns the cluster id of the dag job that is the parent of the current job.
static int lookup_dagman_job_id(ClassAd* job)
{
	// the old (obsolete) form of dagman job id is a string
	// the current form is an integer, we want to handle either.

	classad::Value dagid;
	if ( ! job->EvaluateAttr(ATTR_DAGMAN_JOB_ID, dagid))
		return -1;

	long long id;
	if (dagid.IsIntegerValue(id)) return id;

	const char * idstr = NULL;
	if (dagid.IsStringValue(idstr)) {
		// We've gotten a string, probably something like "201.0"
		// we want to convert it to the numbers 201 and 0. To be safe, we
		// use atoi on either side of the period. We could just use
		// sscanf, but I want to know each fail case, so I can set them
		// to reasonable values.
		char * endptr = NULL;
		id = strtoll(idstr, &endptr, 10);
		if (id > 0 && endptr > idstr && (*endptr == '.' || *endptr == 0)) {
			return id;
		}
	}
	return -1;
}

static void group_job(JobRowOfData & jrod, ClassAd* job)
{
	std::string key;
	//PRAGMA_REMIND("TJ: this should be a sort of arbitrary keys, including numeric keys.")
	//PRAGMA_REMIND("TJ: fix to honor ascending/descending.")
	if ( ! group_by_keys.empty()) {
		for (size_t ii = 0; ii < group_by_keys.size(); ++ii) {
			std::string value;
			if (job->LookupString(group_by_keys[ii].expr, value)) {
				key += value;
				key += "\n";
			}
		}

		// tack on the row unique adentifier as a final sort key
		// (this will usually be cluster/proc or autocluster id)
		formatstr_cat(key, "%016llX", jrod.id);
		rod_sort_key_map[key] = jrod.id;
	}
}

// used to hold a map of unique batch names with the key being the name
// and the values being the cluster/proc of the first job to use that name.
// aka. the "batch id"
std::map<std::string, int> batch_name_to_batch_uid_map;
std::map<int, long long> batch_uid_to_first_job_id_map;
static int next_batch_uid = 0;

static long long resolve_job_batch_uid(JobRowOfData & jrod, ClassAd* job) 
{
	std::string name;
	if (job->LookupString(ATTR_JOB_BATCH_NAME, name)) {
	} else if (job->LookupExpr(ATTR_DAGMAN_JOB_ID) && job->LookupString(ATTR_DAG_NODE_NAME, name)) {
		name.insert(0, "NODE: ");
	#if 1
		// don't batch jobs together by command
	#else
	} else if (job->LookupString(ATTR_JOB_CMD, name)) {
		name.insert(0, "CMD: ");
	#endif
	} else {
		return 0;
	}

	// we want batch names to be unique by owner, so append the owner as well.
	std::string owner;
	if (job->LookupString(ATTR_OWNER, owner)) {
		name += "\n";
		name += owner;
	}

	std::map<std::string, int>::iterator it = batch_name_to_batch_uid_map.find(name);
	if (it != batch_name_to_batch_uid_map.end()) {
		jrod.batch_uid = it->second;
	} else {
		int batch_uid = ++next_batch_uid;
		batch_name_to_batch_uid_map[name] = batch_uid;
		jrod.batch_uid = batch_uid;
	}
	// max_batch_name = MAX(max_batch_name, (int)name.length());
	return true;
}

std::map<int, int> materialized_next_proc_by_cluster_map;
static void track_job_materialize_info(int cluster_id, ClassAd* job) {

	int proc_id = -1;
	if ( ! job->LookupInteger(ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, proc_id) || proc_id < 0) {
		return;
	}
	materialized_next_proc_by_cluster_map[cluster_id] = proc_id;
}

// returns an interator that points to the first use of a batch uid with first
// being defined as the first to call this function with a unique batch_uid value.
long long resolve_first_use_of_batch_uid(int batch_uid, long long id)
{
	std::map<int, long long>::iterator it = batch_uid_to_first_job_id_map.find(batch_uid);
	if (it != batch_uid_to_first_job_id_map.end()) {
		return it->second;
	} else {
		batch_uid_to_first_job_id_map[batch_uid] = id;
		return 0;
	}
}

// if we hold on to the previous ad until the next ad has been parsed, we get a large
// speedup in parsing the incoming classads because we get a much better (60% vs 0%)
// hit rate in the classad cache.
static struct _cache_optimizer {
	ClassAd* previous_jobs[4];
	buffer_line_processor pfn;
	int ix;
} cache_optimizer = { {NULL, NULL, NULL, NULL}, NULL, 0 };

bool cache_optimized_process_job(void * pv,  ClassAd* job) {
	bool done_with_job = cache_optimizer.pfn(pv, job);
	if (done_with_job) {
		delete cache_optimizer.previous_jobs[cache_optimizer.ix];
		cache_optimizer.previous_jobs[cache_optimizer.ix] = job;
		cache_optimizer.ix = (int)((cache_optimizer.ix+1) % COUNTOF(cache_optimizer.previous_jobs));
		done_with_job = false;
	}
	return done_with_job;
}

buffer_line_processor init_cache_optimizer(buffer_line_processor pfn) {
	cache_optimizer.pfn = pfn;
	return cache_optimized_process_job;
}

void cleanup_cache_optimizer()
{
	for (size_t ix = 0; ix < COUNTOF(cache_optimizer.previous_jobs); ++ix) {
		delete cache_optimizer.previous_jobs[ix];
		cache_optimizer.previous_jobs[ix] = NULL;
	}
	cache_optimizer.pfn = NULL;
	cache_optimizer.ix = 0;
}

// write an ad to the given fp in standard long classad form
static bool dump_long_to_fp(void * pv, ClassAd *job)
{
	std::string line;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );
	for (auto itr = job->begin(); itr != job->end(); ++itr) {
		line = itr->first.c_str();
		line += "=";
		unp.Unparse(line, itr->second);
		line += "\n";
		fputs(line.c_str(), (FILE*)pv);
	}
	fputs("\n", (FILE*)pv);

	return true;
}


static bool process_job_to_rod_per_ad_map(void * pv,  ClassAd* job)
{
	ROD_MAP_BY_ID * pmap = (ROD_MAP_BY_ID *)pv;
	count_job(app.sumy, job);

	ASSERT( ! g_stream_results);
	if (capture_raw_results) {
		if ( ! capture_raw_fp) {
			if (MATCH == strcmp(capture_raw_results, "-")) {
				capture_raw_fp = stdout;
			} else if (MATCH == strcmp(capture_raw_results, "-2")) {
				capture_raw_fp = stderr;
			} else {
				const char * mode = "wb";
				const char * filename = capture_raw_results;
				if (*filename == '+') {
					++filename;
					mode = "ab";
				}
				capture_raw_fp = safe_fopen_wrapper(filename, mode);
			}
			// if we just opened the file, print a banner
			if (capture_raw_fp) {
				fprintf(capture_raw_fp, "# condor_q v" CONDOR_VERSION " raw query results\n");
			}
		}
		if (capture_raw_fp) {
			dump_long_to_fp(capture_raw_fp, job);
		}
	}

	union _jobid jobid;

	if (dash_autocluster) {
		const char * attr_id = ATTR_AUTO_CLUSTER_ID;
		if (dash_autocluster == QueryFetchOpts::fetch_GroupBy) attr_id = "Id";
		job->LookupInteger(attr_id, jobid.id);
	} else {
		if ( ! job->LookupInteger(ATTR_CLUSTER_ID, jobid.cluster)) {
			// Classads that have no ClusterId, but do have a JobSetId must be JOBSET ads
			// we will sort these as cluster=0, proc=jobsetid
			if (job->LookupInteger(ATTR_JOB_SET_ID, jobid.proc)) {
				jobid.cluster = 0;
				// TJ: HACK! for now stuff cluster and proc for use by the prmask.render code
				// it is ok to do this here because we will discard the classad afterward
				job->Assign(ATTR_CLUSTER_ID, jobid.cluster);
				job->Assign(ATTR_PROC_ID, jobid.proc);
			}
		}
		if ( ! job->LookupInteger( ATTR_PROC_ID, jobid.proc )) { jobid.proc = -1; }
	}

	if (append_time_to_source_label && ! queue_time) {
		job->LookupInteger( ATTR_SERVER_TIME, queue_time );
	}

	int columns = app.prmask.ColCount();
	if (0 == columns)
		return true;

	std::pair<ROD_MAP_BY_ID::iterator,bool> pp = pmap->insert(std::pair<long long, JobRowOfData>(jobid.id,jobid.id));
	if( ! pp.second ) {
		fprintf( stderr, "Error: Two results with the same ID.\n" );
		//tj: 2013 -don't abort without printing jobs
		// exit( 1 );
		return true;
	} else {
		pp.first->second.id = jobid.id;
		pp.first->second.rov.SetMaxCols(columns);
		app.prmask.render(pp.first->second.rov, job);

		// if displaying jobs in dag order, also add this job to the set of clusters for this dagid
		if ((dash_dag || dash_batch) && ! dash_autocluster) {
			int dag_cluster = lookup_dagman_job_id(job); // this is actually the dagman cluster id
			if (dag_cluster > 0) {
				union _jobid dag_id;
				dag_id.cluster = dag_cluster; dag_id.proc = 0;
				pp.first->second.dag_id = dag_id.id;
				pp.first->second.flags |= JROD_ISDAGNODE;
				//dag_to_cluster_map[dagid].insert(jobid.cluster);
			} else if (dash_batch) {
				resolve_job_batch_uid(pp.first->second, job);
				track_job_materialize_info(jobid.cluster, job);
			}
			int universe = CONDOR_UNIVERSE_MIN;
			if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) && universe == CONDOR_UNIVERSE_SCHEDULER) {
				pp.first->second.flags |= JROD_SCHEDUNIV;
			}
		}
		group_job(pp.first->second, job);
	}
	return true; // true means caller can delete the job, we are done with it.
}

static void trim_whitespace_before_newline(std::string & buf)
{
	if (buf.empty()) return;
	std::string::reverse_iterator it = buf.rbegin();
	if (*it != '\n') return;
	std::string::reverse_iterator end = ++it;
	size_t num = 0;
	for (it = end; it != buf.rend(); ++it) {
		if ( ! isspace(*it)) break;
		++num;
	}
	if (num) buf.erase(it.base(), end.base());
}

static void print_a_result(std::string & buf, JobRowOfData & jrod)
{
	buf.clear();
	app.prmask.display(buf, jrod.rov);
	if (widescreen) trim_whitespace_before_newline(buf);
	fputs(buf.c_str(), stdout);
	jrod.flags |= JROD_PRINTED; // for debugging, keep track of what we already printed.
}

// print all children, grandchildren etc of this job
static void print_children(std::string & buf, JobRowOfData * pjrod)
{
	// we can follow the next_sib link to find all jobs that are siblings
	// but these will be in the reverse order of what we want to print.
	while (pjrod) {
		print_a_result(buf, *pjrod);
		if (pjrod->children) print_children(buf, pjrod->children);
		pjrod = pjrod->next_sib;
	}
}

// print the results array, optionally floating children up under their parents
// As we print the results, if we do float the children up we will see
// the children again as part of the normal iteration, so we want to skip
// them the second time.
void print_results(ROD_MAP_BY_ID & results, KeyToIdMap order, bool children_under_parents)
{
	std::string buf;
	if (dash_batch) {
		// when in batch/progress mode, we print only the cooked rows.
		for(ROD_MAP_BY_ID::iterator it = results.begin(); it != results.end(); ++it) {
			if (!(it->second.flags & JROD_COOKED)) continue;
			print_a_result(buf, it->second);
		}
	}
	else if ( ! order.empty()) {
		for (KeyToIdMap::const_iterator it = order.begin(); it != order.end(); ++it) {
			ROD_MAP_BY_ID::iterator jt = results.find(it->second);
			if (jt != results.end()) {
				if (children_under_parents && (jt->second.flags & JROD_PRINTED)) continue;
				print_a_result(buf, jt->second);
				if (children_under_parents && jt->second.children) {
					print_children(buf, jt->second.children);
				}
			}
		}
	} else {
		for(ROD_MAP_BY_ID::iterator it = results.begin(); it != results.end(); ++it) {
			if (children_under_parents && (it->second.flags & JROD_PRINTED)) continue;
			print_a_result(buf, it->second);
			if (children_under_parents && it->second.children) {
				print_children(buf, it->second.children);
			}
		}
	}

	// This fixes a random failure we see where condor_q correctly prints out
	// the results to stdout, but they don't make it to the stdout of a
	// backtick'ed caller in a perl test script.
	fflush(stdout);
}

void clear_results(ROD_MAP_BY_ID & results, KeyToIdMap & order)
{
	/*
	for(ROD_MAP_BY_ID::iterator it = results.begin(); it != results.end(); ++it) {
		free (it->second);
	}
	*/
	order.clear();
	results.clear();
}

// render a long form ad and append it to the given std::string buffer
static void append_long_ad(std::string & out, CondorClassAdListWriter & writer, ClassAd & ad)
{
	size_t start = out.size();

	classad::References * proj = (dash_autocluster || app.attrs.empty()) ? NULL : &app.attrs;
	if (proj && print_attrs_in_hash_order
		&& (dash_long_format <= ClassAdFileParseType::Parse_long || dash_long_format >= ClassAdFileParseType::Parse_auto)) {
		// special case for debugging, if we have a projection, but also a request not to sort the attributes
		// make a special effort to print attributes in hashtable order for -long output.
		classad::ClassAdUnParser unp;
		unp.SetOldClassAd( true, true );
		for (classad::ClassAd::const_iterator itr = ad.begin(); itr != ad.end(); ++itr) {
			if (proj->contains(itr->first)) {
				out += itr->first.c_str();
				out += " = ";
				unp.Unparse(out, itr->second);
				out += "\n";
			}
		}
		// if we wrote any attributes, append a blank line
		if (out.size() > start) { out += "\n"; }
	} else {
		writer.appendAd(ad, out, proj, print_attrs_in_hash_order);
	}
}

static bool
streaming_print_job(void * pv, ClassAd *job)
{
	count_job(app.sumy, job);
	if (dash_tot && ! verbose)
		return true;

	std::string result_text;

	if ( ! dash_long) {
		if (app.prmask.ColCount() > 0) { app.prmask.display(result_text, job); }
	} else {
		result_text.clear();
		append_long_ad(result_text, *(CondorClassAdListWriter *)pv, *job);
	}
	if ( ! result_text.empty()) { fputs(result_text.c_str(), stdout); }
	return true;
}


/*
static long long make_parentage_sort_key(long long id, std::string & key, ROD_MAP_BY_ID & results)
{
	long long root_id = id;
	ROD_MAP_BY_ID::const_iterator ht = results.find(id);
	// put parents id (recursively) first, then append our own id.
	if (ht != results.end() && ht->second.dag_id < id) {
		root_id = make_parentage_sort_key(ht->second.dag_id, key, results);
	}
	formatstr_cat(key, "%016llX\n", id);
}
*/

// instance data for fixup_std_column_widths callback
struct _fixup_width_values {
	int cluster_width;
	int proc_width;
	int name_width;
	double max_mem_usage;
};

// callback used by fixup_std_column_widths
static int fnFixupWidthCallback(void* pv, int index, Formatter * fmt, const char * /*attr*/) {
	struct _fixup_width_values * p = (struct _fixup_width_values *)pv;
	char * pf = const_cast<char*>(fmt->printfFmt);
	if (index == 0) {
		if (p->cluster_width > 4 && p->cluster_width <= 9) {
			if (pf && pf[1] >= '4' && pf[1] <= '9') { pf[1] = '0' + p->cluster_width; fmt->width = p->cluster_width+1; }
		}
	} else if (index == 1) {
		if (p->proc_width > 3 && p->proc_width <= 9) {
			if (pf && pf[2] >= '3' && pf[2] <= '9') { pf[2] = '0' + p->proc_width; fmt->width = p->proc_width; }
		}
	} else if (index == name_column_index) { // owner
		fmt->width = MAX(fmt->width, p->name_width);
		p->name_width = fmt->width; // return the actual width
	} else if (fmt->fr == local_render_memory_usage) {
		char buf[30];
		int wid = snprintf(buf, sizeof(buf), fmt->printfFmt ? fmt->printfFmt : "%.1f", p->max_mem_usage);
		fmt->width = MAX(fmt->width, wid);
	} else {
		// return -1; // stop iterating
	}
	return 1;
}

// fix width of cluster,proc and name columns for all standard display formats other than -batch
//
static void fixup_std_column_widths(int max_cluster, int max_proc, int longest_name, double max_mem) {
	if ( ! is_standard_format || ( ! first_col_is_job_id && ! has_owner_column))
		return; // nothing to do
	if (max_cluster < 9999 && max_proc < 999 && longest_name <= 14)
		return; // nothing to do.

	struct _fixup_width_values vals;
	memset(&vals, 0, sizeof(vals));
	vals.max_mem_usage = max_mem;

	if (first_col_is_job_id) {
		char buf[20];
		snprintf(buf, sizeof(buf), "%d", max_cluster);
		vals.cluster_width = (int)strlen(buf);
		snprintf(buf, sizeof(buf), "%d", max_proc);
		vals.proc_width = (int)strlen(buf);
	}

	if (has_owner_column && ! widescreen && longest_name > 14) {
		vals.name_width = MIN(longest_name, max_name_column_width);
	}

	app.prmask.adjust_formats(fnFixupWidthCallback, &vals);
	name_column_width = vals.name_width; // propage the returned width
}

static void append_sibling(JobRowOfData * sib_list, JobRowOfData * sib)
{
	// the first sibling gets the first sib slot, thereafter we want to append
	// siblings, so we keep track of the last entry in the sibling list as well.
	if ( ! sib_list->next_sib) {
		sib_list->next_sib = sib;
		sib_list->last_sib = sib; // remember last added so we can easily append another
	} else {
		sib_list->last_sib->next_sib = sib;
		sib_list->last_sib = sib; // remember last added so we can easily append another
	}
}

// link multi-proc clusters into a peer list (with the lowest proc id being the first peer)
// and children to parents (i.e. dag nodes to their owning dagman), or jobs that share a batch-name together
// when this function returns
//  * all procs in a cluster will be linked via the next_proc links in order of increasing procid
//  * all cluster jobs in a dag will be pointed to by either the children link of the parent dag node
//    or the next_sib link of a sibling node. the last_sib link of the first child will point to the last sibling.
//    when following the children links or next_sib links, jobids will always increase.
//  * all cluster non-dag jobs which have the same batch-name will be connected by a chain of 
//    next_sib links in order of increasing cluster id.
//  (a cluster job is the job in a cluster with the lowest procid. this is usually procid 0, but is not guranteed to be so)
static void linkup_nodes_by_id(ROD_MAP_BY_ID & results)
{
	const bool fold_dagman_sibs = dash_batch && (dash_batch & 2);  // hack -batch:2 gives experimental behavior

	ROD_MAP_BY_ID::iterator it = results.begin();
	if (it == results.end()) return;

	union _jobid idp, idn;
	idp.id = it->second.id;
	if (it->second.batch_uid) {
		resolve_first_use_of_batch_uid(it->second.batch_uid, it->second.id);
	}

	int max_cluster = idp.cluster;
	int max_proc = idp.proc;

	int max_name = max_owner_name; // set by calls to render_owner or render_dag_owner

	ROD_MAP_BY_ID::iterator prev = it++;
	while (it != results.end()) {

		idp.id = prev->second.id;
		idn.id = it->second.id;
		bool first_proc = (idp.cluster != idn.cluster);
		if (first_proc) {
			prev->second.next_proc = NULL;
		} else {
			// link all procs in a cluster to the first proc
			prev->second.next_proc = &it->second;
		}
		max_cluster = MAX(max_cluster, idn.cluster);
		max_proc = MAX(max_proc, idn.proc);

		// also link dag children to their parent.
		// 
		if (it->second.dag_id > 0) {
			ROD_MAP_BY_ID::iterator parent = results.find(it->second.dag_id);
			if (parent != results.end()) {
				it->second.generation = parent->second.generation + 1;
				if ( ! parent->second.children) {
					parent->second.children = &it->second;
				} else if (first_proc) {
					// link the first proc in a cluster to other clusters
					// that are also children of the same parent.
					JobRowOfData * sib_list = parent->second.children;
					append_sibling(sib_list, &it->second);
				}

				// also adjust name column width to account for nesting depth if we have nested dags
				// (the 3 characters needed for the level 1 " |-" are already accounted for.
				if (dash_dag && it->second.generation > 1) {
					int wid;
					classad::Value * pcolval = it->second.rov.Column(name_column_index);
					if (pcolval && pcolval->IsStringValue(wid)) {
						int cch = wid + 2 + it->second.generation;
						max_name = MAX(max_name, cch);
					}
				}
			}
		} else if (it->second.batch_uid && first_proc) {
			// when we call this function for all jobs that have a batch_uid, it will always
			// return the id of the FIRST job that passed it a given unique id.  since we are
			// calling this function on data sorted by jobid, we will thus always get back
			// the lowest job id.
			bool link_sibs = fold_dagman_sibs || ! (it->second.flags & JROD_SCHEDUNIV);
			long long id = resolve_first_use_of_batch_uid(it->second.batch_uid, it->second.id);
			if (link_sibs && id && (id != it->second.id)) {
				ROD_MAP_BY_ID::iterator oldest = results.find(id);
				if (oldest != results.end()) {
					JobRowOfData * sib_list = &oldest->second;
					append_sibling(sib_list, &it->second);
				}
			}
		}

		prev = it++;
	}

	fixup_std_column_widths(max_cluster, max_proc, max_name, max_mem_used);
}

static void
format_name_column_for_dag_nodes(ROD_MAP_BY_ID & results, int name_column, int col_width)
{
	std::string buf;
	for(ROD_MAP_BY_ID::iterator it = results.begin(); it != results.end(); ++it) {
		if (it->second.flags & JROD_ISDAGNODE) {
			classad::Value * pcolval = it->second.rov.Column(name_column);
			if ( ! pcolval) continue;

			const char * name = NULL;
			pcolval->IsStringValue(name);
			int cch = (int)strlen(name);

			buf.clear();
			buf.reserve(cch + 3 + it->second.generation);
			for (int ii = 0; ii < it->second.generation; ++ii) buf += ' ';
			buf += "|-";
			int ix = (int)buf.size();
			if (cch+ix > col_width) cch = col_width-ix;

			buf.append(name, cch);
			pcolval->SetStringValue(buf);
		}
	}
}

// this defines the attributes to fetch for the -batch output
// but does not actually define the output because the results are
// extensively post-processed.
const char * const jobProgress_PrintFormat = "SELECT\n"
	ATTR_OWNER               " AS OWNER           WIDTH AUTO PRINTAS DAG_OWNER OR ??\n"  // 0
	ATTR_JOB_CMD             " AS BATCH_NAME      WIDTH -12  PRINTAS BATCH_NAME\n" // 1
	ATTR_Q_DATE              " AS '   SUBMITTED'  WIDTH 12   PRINTAS QDATE\n" // 2
	ATTR_JOB_STATUS          " AS '  DONE' WIDTH 6 PRINTF %6d OR _\n"  // Done   // 3
	ATTR_JOB_UNIVERSE        " AS '  RUN ' WIDTH 6 PRINTF %6d OR _\n"  // Active // 4
	ATTR_DAG_NODES_QUEUED    " AS '  IDLE' WIDTH 6 PRINTF %6d OR _\n"  // Idle   // 5
	ATTR_DAG_NODES_DONE      " AS '  HOLD' WIDTH 6 PRINTF %6d OR _\n"  // Held   // 6
	ATTR_DAG_NODES_TOTAL "?:" ATTR_TOTAL_SUBMIT_PROCS " AS ' TOTAL' WIDTH 6 PRINTF %6d OR _\n"  // Total  // 7
	ATTR_JOB_STATUS "?:" ATTR_JOB_MATERIALIZE_NEXT_PROC_ID " AS JOB_IDS WIDTH 0 PRINTAS JOB_STATUS\n"     // 8
//	ATTR_JOB_REMOTE_USER_CPU " AS '    RUN_TIME'  WIDTH 12   PRINTAS CPU_TIME\n"
//	ATTR_IMAGE_SIZE          " AS SIZE        WIDTH 4    PRINTAS MEMORY_USAGE\n"
//	ATTR_HOLD_REASON         " AS HOLD_REASON\n"
"SUMMARY STANDARD\n";


class  cluster_progress {
public:
	int count;   // number of rows counted
	int jobs;    // number of job rows (i.e. non-dag rows)
	int dags;    // number of dag rows
	int nodes_total; // accumulation of ATTR_DAG_NODES_TOTAL for dag jobs, and of ATTR_TOTAL_SUBMIT_PROCS for non-dag jobs
	int nodes_done;  // accumulation of ATTR_DAG_NODES_DONE for dag jobs, and a guess of completed procs for non-dag jobs
	int unknown_total; // number of clusters that have neither ATTR_DAG_NODES_TOTAL nor ATTR_TOTAL_SUBMIT_PROCS
	int cluster;
	int min_proc;
	int max_proc;
	int states[JOB_STATUS_MAX+1];
	cluster_progress() : count(0), jobs(0), dags(0), nodes_total(0), nodes_done(0), unknown_total(0),
		cluster(0), min_proc(INT_MAX), max_proc(0) {
		memset(states, 0, sizeof(states));
	}
};

class child_progress : public cluster_progress {
public:
	long long min_jobid; // use union _jobid to extract cluster/proc
	long long max_jobid;
	child_progress() : min_jobid(0), max_jobid(0) {}
};

// roll up all of the procs in a cluster, calculating totals for the job states and how many jobs are done.
//
static void reduce_procs(cluster_progress & prog, JobRowOfData & jr)
{
	// these refer to column indexes in the jobProgress_PrintFormat declaration
	const int ixJobStatusCol = 3;
	const int ixUniverseCol  = 4;
	const int ixDagNodesDone = 6;
	const int ixDagNodesTotal = 7; // ATTR_DAG_NODES_TOTAL ?: ATTR_TOTAL_SUBMIT_PROCS
	int total_submit_procs = 0;
	int num_procs = 0;

	prog.count += 1;

	union _jobid jid; jid.id = jr.id;
	prog.cluster = jid.cluster;
	if (jid.proc >= 0) {
		prog.min_proc = prog.max_proc = jid.proc;
	}

	int universe = CONDOR_UNIVERSE_MIN, status = 0;
	if (jr.getNumber(ixUniverseCol, universe) && universe == CONDOR_UNIVERSE_SCHEDULER) {
		jr.flags |= JROD_SCHEDUNIV;
		prog.dags += 1;
		int dag_done = 0, dag_total = 0;
		if (jr.getNumber(ixDagNodesTotal, dag_total)) { prog.nodes_total += dag_total; }
		if (jr.getNumber(ixDagNodesDone, dag_done)) { prog.nodes_done += dag_done; }
	} else {
		if (jid.proc >= 0) {
			prog.jobs += 1;
		}

		// as of 8.5.7 the schedd will inject ATTR_TOTAL_SUBMIT_PROCS into the cluster ad
		// and we will have fetched it in the ixDagNodesTotal field.
		// We don't add it to the total unless this cluster is NOT a node of a dag.
		if (jr.getNumber(ixDagNodesTotal, num_procs)) { total_submit_procs = num_procs; num_procs = 1; }

		if (jid.proc >= 0) {
			jr.getNumber(ixJobStatusCol, status);
			if (status < 0 || status > JOB_STATUS_MAX) status = 0;
			prog.states[status] += 1;
		}
	}

	JobRowOfData *jrod2 = jr.next_proc;
	while (jrod2) {
		union _jobid jid2; jid2.id = jrod2->id;
		if ( ! (jrod2->flags & (JROD_FOLDED | JROD_SKIP))) {
			if (jid2.proc >= 0) {
				prog.count += 1;
				prog.jobs += 1; // on the assumption that dagmen can't be multi proc

				int st;
				jrod2->getNumber(ixJobStatusCol, st);
				if (st < 0 || st > JOB_STATUS_MAX) st = 0;
				prog.states[st] += 1;

				prog.min_proc = MIN(prog.min_proc, jid2.proc);
				prog.max_proc = MAX(prog.min_proc, jid2.proc);
			}
			jrod2->flags |= JROD_FOLDED;
		}
		++num_procs;
		jrod2 = jrod2->next_proc;
	}

	// for non-dagman jobs, we can use the (new for 8.5.7 TotalSubmitProcs attribute) or
	// we can extrapolate the total number of jobs looking at the max procid.
	// We can then assume that completed jobs are jobs that have left the queue.
	if ( ! (jr.flags & (JROD_SCHEDUNIV | JROD_ISDAGNODE))) {
		const bool guess_at_jobs_done_per_cluster = (dash_batch & 0x10);
		int total = 0;
		if (total_submit_procs) {
			total = total_submit_procs;
		} else if (guess_at_jobs_done_per_cluster) {
			total = prog.max_proc+1;
		}
		if (total) {
			int done = total - num_procs;
			if (materialized_next_proc_by_cluster_map.find(prog.cluster) != materialized_next_proc_by_cluster_map.end()) { 
				int materialized_jobs = materialized_next_proc_by_cluster_map[prog.cluster];
				done = materialized_jobs - prog.jobs;
			}
			prog.nodes_total += total;
			if ( ! querying_partial_clusters) {
				prog.nodes_done += done;
			}
		} else {
			prog.unknown_total += 1;
		}
	}
}

static void reduce_children(child_progress & prog, JobRowOfData * pjr, int & depth)
{
	// we can follow the next_sib link to find all jobs that are siblings
	// and the children link to find child jobs.
	while (pjr) {
		//{ union _jobid jidt; jidt.id = pjr->id; fprintf(stderr, " %sfolding %x %d.%d\n", "                    "+20-MIN(depth,20), pjr->flags, jidt.cluster, jidt.proc); }
		ASSERT(!(pjr->flags & JROD_FOLDED));
		reduce_procs(prog, *pjr);
		if ( ! (pjr->flags & JROD_SCHEDUNIV)) {
			union _jobid jid;
			jid.cluster = prog.cluster;
			jid.proc = prog.min_proc;
			if ( ! prog.min_jobid) {
				prog.min_jobid = jid.id;
			} else {
				prog.min_jobid = MIN(prog.min_jobid, jid.id);
			}
			jid.proc = prog.max_proc;
			prog.max_jobid = MAX(prog.max_jobid, jid.id);
		}
		pjr->flags |= JROD_FOLDED;
		if (pjr->children) {
			depth += 1;
			reduce_children(prog, pjr->children, depth);
			depth -= 1;
		}
		pjr = pjr->next_sib;
	}
}

// passed to fnFixupWidthsForProgressFormat
struct _fixup_progress_width_values {
	int owner_width;
	int batch_name_width;
	int ids_width;
	bool any_held;
	bool zeros_as_dashes;
	char alt_kind;
};

// hacky way to adjust column widths based on the data.
static int fnFixupWidthsForProgressFormat(void* pv, int index, Formatter * fmt, const char * /*attr*/) {
	struct _fixup_progress_width_values * p = (struct _fixup_progress_width_values *)pv;
	//char * pf = const_cast<char*>(fmt->printfFmt);
	if (index == 0) { // owner
		if (p->owner_width < fmt->width) {
			fmt->width = MAX(8, p->owner_width);
			fmt->options &= ~(FormatOptionNoTruncate | FormatOptionAutoWidth);
		} else {
			fmt->width = p->owner_width;
		}
		p->owner_width = fmt->width; // return the actual width
	} else if (index == 1) { // batch name
		int wid = (fmt->width < 0) ? -(fmt->width) : fmt->width;
		wid = MAX(10, p->batch_name_width);
		p->batch_name_width = wid; // return the actual width
		fmt->width = -wid;
	} else if (index == 6) { // held
		if ( ! p->any_held && ! global) {
			fmt->options |= FormatOptionHideMe;
		}
	}
	if (p->zeros_as_dashes && (index >= 3 && index <= 7)) {
		fmt->altKind = p->alt_kind;
	}
	return 1;
}

// reduce the data by summarizing all of the procs in a cluster
// and all of the nodes in a dag into a single line of output.
//
static void
reduce_results(ROD_MAP_BY_ID & results) {
	//const int ixJobStatusCol = 2;
	//const int ixUniverseCol  = 3;
	int ixOwnerCol = 0;
	int ixBatchNameCol = 1;
	int ixFirstCounterCol = 3;
	int ixJobIdsCol = 8;

	const bool fold_dagman_sibs = dash_batch && (dash_batch & 2); // hack -batch:2 gives experimental behavior

	struct _fixup_progress_width_values wids;
	memset(&wids, 0, sizeof(wids));
	static const char alt_char_map[] = "b?*.-_#z"; // map cmd line option char to alt_kind index
	for (int ix = 0; ix < (int)sizeof(alt_char_map)-1; ++ix) {
		if (alt_char_map[ix] == dash_progress_alt_char) {
			wids.zeros_as_dashes = true;
			wids.alt_kind = (char)ix;
		}
	}

	for(ROD_MAP_BY_ID::iterator it = results.begin(); it != results.end(); ++it) {
		JobRowOfData & jr = it->second;
		if (jr.flags & (JROD_FOLDED | JROD_SKIP | JROD_COOKED))
			continue;

		//{ union _jobid jidt; jidt.id = jr.id; fprintf(stderr, "reducing %d.%d\n", jidt.cluster, jidt.proc); }
		child_progress prog;
		int depth = 0;
		reduce_children(prog, &jr, depth);
		// now write baked results into this row and mark it as cooked
		jr.flags |= JROD_COOKED;

		// rewrite the values in the counter columns with the values
		// we got by summing all of the peers and children
		int num_done    = prog.unknown_total ? 0 : prog.nodes_done;
		int num_total   = prog.unknown_total ? 0 : prog.nodes_total;
		//int num_jobs    = prog.jobs;
		int num_idle    = prog.states[IDLE] + prog.states[SUSPENDED];
		int num_active  = prog.states[RUNNING] + prog.states[TRANSFERRING_OUTPUT];
		int num_held    = prog.states[HELD];
		if (num_held > 0) wids.any_held = true;

		int ixCol = ixFirstCounterCol; // starting column for counters
		jr.rov.Column(ixCol)->SetIntegerValue(num_done);
		jr.rov.set_col_valid(ixCol, (bool)(num_done > 0));

		++ixCol;
		jr.rov.Column(ixCol)->SetIntegerValue(num_active);
		jr.rov.set_col_valid(ixCol, (bool)(num_active > 0));

		++ixCol;
		jr.rov.Column(ixCol)->SetIntegerValue(num_idle);
		jr.rov.set_col_valid(ixCol, (bool)(num_idle > 0));

		++ixCol;
		jr.rov.Column(ixCol)->SetIntegerValue(num_held);
		jr.rov.set_col_valid(ixCol, (bool)(num_held > 0));

		++ixCol;
		jr.rov.Column(ixCol)->SetIntegerValue(num_total);
		jr.rov.set_col_valid(ixCol, (bool)(num_total > 0));

		int name_width = 0;
		jr.getString(ixOwnerCol, name_width);
		wids.owner_width = MAX(wids.owner_width, name_width);

		std::string tmp;
		union _jobid jid; jid.id = jr.id;
		if (jr.flags & JROD_SCHEDUNIV) {
			if (fold_dagman_sibs && jr.batch_uid && jr.next_sib && jr.getString(ixBatchNameCol, name_width)) {
				wids.batch_name_width = MAX(wids.batch_name_width, name_width);
			} else {
			#if 1 // assume batch name column is already correct.
				jr.getString(ixBatchNameCol, name_width);
				wids.batch_name_width = MAX(wids.batch_name_width, name_width);
			#else
				formatstr(tmp, "DAG %d", jid.cluster);
				jr.rov.Column(ixBatchNameCol)->SetStringValue(tmp.c_str());
				jr.rov.set_col_valid(ixBatchNameCol, true);
				wids.batch_name_width = MAX(wids.batch_name_width, (int)tmp.length());
			#endif
			}
		} else {
			if (jr.getString(ixBatchNameCol, name_width)) {
				wids.batch_name_width = MAX(wids.batch_name_width, name_width);
			} else {
				formatstr(tmp, "ID: %d", jid.cluster);
				jr.rov.Column(ixBatchNameCol)->SetStringValue(tmp.c_str());
				jr.rov.set_col_valid(ixBatchNameCol, true);
				wids.batch_name_width = MAX(wids.batch_name_width, (int)tmp.length());
			}
		}

		union _jobid jmin, jmax; jmin.id = prog.min_jobid; jmax.id = prog.max_jobid;
		if (jmin.id != jmax.id) {
			if (jmin.cluster == jmax.cluster) {
				formatstr(tmp, "%d.%d-%d", jmin.cluster, jmin.proc, jmax.proc);
			} else {
				formatstr(tmp, "%d.%d ... %d.%d", jmin.cluster, jmin.proc, jmax.cluster, jmax.proc);
			}
		} else {
			formatstr(tmp, "%d.%d", jmin.cluster, jmin.proc);
		}
		jr.rov.Column(ixJobIdsCol)->SetStringValue(tmp.c_str());
		jr.rov.set_col_valid(ixJobIdsCol, true);
		wids.ids_width = MAX(wids.ids_width, (int)tmp.length());
	}

	// now fixup columns to fit the data, we have 50 chars worth of fixed width
	// 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789
	//   1/5  11:45    319                    9    329 52521977.50 ... 52524203.277
	if ( ! widescreen) {
		int display_wid = getDisplayWidth();
		int var_wid = display_wid - 50;
		if (wids.batch_name_width + wids.owner_width + wids.ids_width > var_wid) {
			int var_min = 8 + 10 + 9; // 8 for name and 10 for batch, 9 for ids
			int bw,nw,iw;
			bw = nw = iw = (var_wid - var_min)/3; // start by distributing the surplus evenly

			// if we are really short of pixels, give the ids only half of what they requested (i.e show the first id only)
			if (iw < wids.ids_width) { 
				int needed = (wids.batch_name_width + wids.owner_width) - (bw+nw+10+8);
				if (needed > 7 && bw < 10) { wids.ids_width /= 2; }
			}
			if (iw+9 > wids.ids_width) { int rw = iw+9 - wids.ids_width; nw += rw/2; bw += rw - rw/2; }
			if (bw+10 > wids.batch_name_width) { int rw = bw+10 - wids.batch_name_width; nw = MIN(8+nw+rw, wids.owner_width) - 8; }
			if (nw+8 > wids.owner_width) { int rw = nw+8 - wids.owner_width; bw =  MIN(10+bw+rw, wids.batch_name_width) - 10; }

			wids.batch_name_width = MIN(wids.batch_name_width, bw+10);
			wids.owner_width = MIN(wids.owner_width, nw+8);
		}
	}
	app.prmask.adjust_formats(fnFixupWidthsForProgressFormat, &wids);
}


//PRAGMA_REMIND("Write this properly as a method on the sinful object.")

const char * summarize_sinful_for_display(std::string & addrsumy, const char * addr)
{
	addrsumy = addr;
	const char * quest = strchr(addr, '?');
	if ( ! quest) {
	   addrsumy = addr;
	   return addr;
	}
	
	addrsumy.clear();
	addrsumy.insert(0, addr, 0, quest - addr);
	addrsumy += "?...";
	return addrsumy.c_str();
}


// Given a list of jobs, do analysis for each job and print out the results.
//
bool print_jobs_analysis (
	IdToClassaAdMap & jobs,
	const char * source_label,
	DaemonAllowLocateFull * pschedd_daemon)
{
	int console_width = widescreen ? getDisplayWidth() : 80;

	// note: pschedd_daemon may be NULL.

	int cJobs = (int)jobs.size();
	int cSlots = (int)startdAds.size();

	// build a job autocluster map
	JobClusterMap job_autoclusters;
	if (reverse_analyze || analysis_mode >= anaModeSummary) {
		if (verbose) { fprintf(stderr, "\nBuilding autocluster map of %d Jobs...", cJobs); }
		buildJobClusterMap(jobs, ATTR_AUTO_CLUSTER_ID, job_autoclusters);
		size_t cNoAuto = job_autoclusters[-1].size();
		size_t cAuto = job_autoclusters.size()-1; // -1 because size() counts the entry at [-1]
		if (verbose) { fprintf(stderr, "%d autoclusters and %d Jobs with no autocluster\n", (int)cAuto, (int)cNoAuto); }
	}

	if (reverse_analyze) { 
		// if (verbose && (cSlots > 1)) { fprintf(stderr, "Sorting %d Slots...", cSlots); }
		if (cSlots == 1) {
			// if there is a single machine ad, then default to analysis
			if ( ! analyze_detail_level) analyze_detail_level = detail_analyze_each_sub_expr;
		}
		//if (verbose) { fprintf(stderr, "Done, longest machine name %d, longest slot name %d\n", longest_slot_machine_name, longest_slot_name); }
	} else {
		if (analyze_detail_level & detail_better) {
			analyze_detail_level |= detail_analyze_each_sub_expr | detail_always_analyze_req;
		}
	}

	// print banner for this source of jobs.
	printf ("\n\n-- %s\n", source_label);

	// if there is a schedd ad, and we are not summarizing, check and report the schedd's global limits
	if (pschedd_daemon && (analysis_mode > anaModeBetter) && ! reverse_analyze) {
		std::string buf;
		if (warnScheddGlobalLimits(pschedd_daemon, buf)) {
			fputs(buf.c_str(), stdout);
		}
	}

	if (reverse_analyze) {
		if (cSlots <= 0) {
			// print something out?
		} else {
			if (analyze_detail_level <= 0 && analysis_mode <= anaModeBetter) { analysis_mode = anaModeSummary; }
			if (analysis_mode == anaModeSummary) {
				if (cJobs <= 1) {
					// PRAGMA_REMIND("this does the wrong thing if jobs collection is really an autocluster collection")
					for (auto it = jobs.begin(); it != jobs.end(); ++it) {
						ClassAd * job = it->second.get();
						int cluster_id = 0, proc_id = 0;
						job->LookupInteger(ATTR_CLUSTER_ID, cluster_id);
						job->LookupInteger(ATTR_PROC_ID, proc_id);
						printf("%d.%d: Analyzing matches for 1 job\n", cluster_id, proc_id);
					}
				} else {
					int cNoAutoT = (int)job_autoclusters[-1].size();
					int cAutoclustersT = (int)job_autoclusters.size()-1; // -1 because [-1] is not actually an autocluster
					if (verbose) {
						printf("Analyzing %d jobs in %d autoclusters\n", cJobs, cAutoclustersT+cNoAutoT);
					} else {
						printf("Analyzing matches for %d jobs\n", cJobs);
					}
				}
				std::string fmt;
				int name_width = MAX(longest_slot_machine_name+7, longest_slot_name);
				formatstr(fmt, "%%-%d.%ds", MAX(name_width, 16), MAX(name_width, 16));
				fmt += " %4s %12.12s %12.12s %10.10s\n";

				printf(fmt.c_str(), ""    , "Slot", "Slot's Req ", "  Job's Req ", "Both   ");
				printf(fmt.c_str(), "Name", "Type", "Matches Job", "Matches Slot", "Match %");
				printf(fmt.c_str(), "------------------------", "----", "------------", "------------", "----------");
			}
			for (auto it = startdAds.begin(); it != startdAds.end(); ++it) {
				doSlotRunAnalysis(it->second.get(), job_autoclusters, console_width, analyze_detail_level, analysis_mode >= anaModeSummary);
			}
		}
	} else if (analysis_mode == anaModeSummary) {
		if (single_machine) {
			printf("%s: Analyzing matches for %d slots\n", single_machine_label, cSlots);
		} else {
			printf("Analyzing matches for %d slots\n", cSlots);
		}
		const char * fmt = "%-13s %12s %12s %11s %11s %10s %9s %s\n";
		printf(fmt, "",              " Autocluster", "   Matches  ", "  Machine  ", "  Running  ", "  Serving ", "", "");
		printf(fmt, " JobId",        "Members/Idle", "Requirements", "Rejects Job", " Users Job ", "Other User", "Available", summarize_with_owner ? "Owner" : "");
		printf(fmt, "-------------", "------------", "------------", "-----------", "-----------", "----------", "---------", summarize_with_owner ? "-----" : "");

		for (JobClusterMap::iterator it = job_autoclusters.begin(); it != job_autoclusters.end(); ++it) {
			int cJobsInCluster = (int)it->second.size();
			if (cJobsInCluster <= 0)
				continue;

			// for the the non-autocluster cluster, we have to eval these jobs individually
			int cJobsToEval = (it->first == -1) ? cJobsInCluster : 1;
			int cJobsToInc  = (it->first == -1) ? 1 : cJobsInCluster;
			for (int ii = 0; ii < cJobsToEval; ++ii) {
				ClassAd *job = it->second[ii];

				char achJobId[32], achAutocluster[48], achRunning[32];
				int cluster_id = 0, proc_id = 0;
				job->LookupInteger(ATTR_CLUSTER_ID, cluster_id);
				job->LookupInteger(ATTR_PROC_ID, proc_id);
				snprintf(achJobId, sizeof(achJobId), "%d.%d", cluster_id, proc_id);

				std::string owner;
				if (summarize_with_owner) job->LookupString(ATTR_OWNER, owner);
				if (owner.empty()) owner = "";

				int cIdle = 0, cRunning = 0;
				for (int jj = 0; jj < cJobsToInc; ++jj) {
					ClassAd * jobT = it->second[jj];
					int jobState = 0;
					if (jobT->LookupInteger(ATTR_JOB_STATUS, jobState)) {
						if (IDLE == jobState) 
							++cIdle;
						else if (TRANSFERRING_OUTPUT == jobState || RUNNING == jobState || SUSPENDED == jobState)
							++cRunning;
					}
				}

				if (it->first >= 0) {
					if (verbose) {
						snprintf(achAutocluster, sizeof(achAutocluster), "%d:%d/%d", it->first, cJobsToInc, cIdle);
					} else {
						snprintf(achAutocluster, sizeof(achAutocluster), "%d/%d", cJobsToInc, cIdle);
					}
				} else {
					achAutocluster[0] = 0;
				}

				anaPrio prio;
				anaCounters ac;
				std::string job_status;
				doJobRunAnalysis(job, NULL, job_status, true, ac, &prio, NULL);
				const char * fmt = "%-13s %-12s %12d %11d %11s %10d %9d %s\n";

				achRunning[0] = 0;
				if (cRunning) { snprintf(achRunning, sizeof(achRunning), "%d/", cRunning); }
				snprintf(achRunning+strlen(achRunning), 12, "%d", ac.machinesRunningUsersJobs);

				printf(fmt, achJobId, achAutocluster,
						ac.totalMachines - ac.fReqConstraint,
						ac.fOffConstraint,
						achRunning,
						ac.machinesRunningJobs - ac.machinesRunningUsersJobs,
						ac.available,
						owner.c_str());
			}
		}
	} else if (analysis_mode == anaModeUnmatchable) {

		std::string buf;
		for (JobClusterMap::iterator it = job_autoclusters.begin(); it != job_autoclusters.end(); ++it) {
			int cJobsInCluster = (int)it->second.size();
			if (cJobsInCluster <= 0)
				continue;

			// for the the non-autocluster cluster, we have to eval these jobs individually
			int cJobsToEval = (it->first == -1) ? cJobsInCluster : 1;
			int cJobsToInc  = (it->first == -1) ? 1 : cJobsInCluster;
			for (int ii = 0; ii < cJobsToEval; ++ii) {
				ClassAd *job = it->second[ii];

				anaCounters ac;
				ac.clear();
				buf.clear();

				doJobRunAnalysis(job, pschedd_daemon, buf, anaMatchModePslot, ac, NULL, NULL);
				if ( ! ac.both_match) {
					// this job is not a match for any slot that it is willing to run on. print something
					buf = (ac.job_matches_slot) ? "All slots reject" : "No slots match";
					formatstr_cat(buf, " autocluster %d, jobs :", it->first);
					for (int jj = 0; jj < cJobsToInc; ++jj) {
						job = it->second[jj];
						int cluster, proc;
						job->LookupInteger( ATTR_CLUSTER_ID, cluster );
						job->LookupInteger( ATTR_PROC_ID, proc );
						formatstr_cat(buf, " %d.%d", cluster, proc);
					}
					buf += "\n";
					fputs(buf.c_str(), stdout);
				}
			}
		}

	} else { // the regular condor_q -better output, not reversed, not summarized.

		for (auto it = jobs.begin(); it != jobs.end(); ++it) {
			ClassAd *job = it->second.get();
			printJobRunAnalysis(job, pschedd_daemon, analyze_detail_level, analyze_with_userprio);
		}
	}

	return true;
}

#if 0
	if (analyze_memory_usage) {
		for (JobClusterMap::iterator it = job_autoclusters.begin(); it != job_autoclusters.end(); ++it) {
			int cJobsInCluster = (int)it->second.size();
			if (cJobsInCluster <= 0)
				continue;

			// for the the non-autocluster cluster, we have to eval these jobs individually
			int cJobsToEval = (it->first == -1) ? cJobsInCluster : 1;
			int cJobsToInc  = (it->first == -1) ? 1 : cJobsInCluster;
			for (int ii = 0; ii < cJobsToEval; ++ii) {
				ClassAd *job = it->second[ii];
				int num_skipped = 0;
				QuantizingAccumulator mem_use(0,0);
				const classad::ExprTree* tree = job->LookupExpr(analyze_memory_usage);
				if (tree) {
					AddExprTreeMemoryUse(tree, mem_use, num_skipped);
				}
				size_t raw_mem, quantized_mem, num_allocs;
				raw_mem = mem_use.Value(&quantized_mem, &num_allocs);

				printf("\nMemory usage for '%s' is %d bytes from %d allocations requesting %d bytes (%d expr nodes skipped)\n",
					analyze_memory_usage, (int)quantized_mem, (int)num_allocs, (int)raw_mem, num_skipped);
				return true;
			}
		}
	}
#endif

// query SCHEDD daemon for jobs. and then print out the desired job info.
// this function handles -analyze, -streaming, -dag and all normal condor_q output
// when the source is a SCHEDD.
static bool
show_schedd_queue(const char* scheddAddress, const char* scheddName, const char* scheddMachine, int useFastPath,CondorClassAdListWriter &writer )
{
	// initialize counters
	app.sumy.clear_counters();

	std::string addr_summary;
	summarize_sinful_for_display(addr_summary, scheddAddress);
	std::string source_label;
	if (querySubmittors) {
		formatstr(source_label, "Submitter: %s : %s : %s", scheddName, addr_summary.c_str(), scheddMachine);
	} else {
		formatstr(source_label, "Schedd: %s : %s", scheddName, addr_summary.c_str());
	}

	if (verbose || dash_profile) {
		fprintf(stderr, "Fetching job ads...");
	}
	size_t cbBefore = 0;
	double tmBefore = 0;
	if (dash_profile) { cbBefore = ProcAPI::getBasicUsage(getpid(), &tmBefore); }

	// fetch queue from schedd
	IdToClassaAdMap ads;
	CondorError errstack;
	ClassAd * summary_ad = NULL; // points to a final summary ad when we query an actual schedd.

	// choose a processing option for jobad's as the come off the wire.
	// for -long -json -xml and -analyze, we need to save off the ad in a ClassAdList
	// for -stream we print out the ad 
	// and for everything else, we 
	//
	buffer_line_processor pfnProcess = NULL;
	void *                pvProcess = NULL;
	if (better_analyze || dash_unmatchable || (dash_long && ! g_stream_results)) {
		if (dash_factory) {
			// if we will be fetching clusterads, they will not have a ProcId attribute
			// so we should treat that a ProcId == -1. 
			// If NOT fetching factory ads, then we should use the sequence number as the sort key
			// when the ProdId is missing. This means that -factory -job will potentially generate
			// errors when the files being read have no ProcId attribute.
			// we call that user error, not a bug.
			assume_cluster_ad_if_no_proc_id = app.attrs.empty() || app.attrs.contains(ATTR_PROC_ID);
		}
		pfnProcess = AddJobToClassAdCollection;
		pvProcess = &ads;
	} else if (g_stream_results) {
		pfnProcess = streaming_print_job;
		pvProcess = &writer;
		// we are about to print out the jobads, so print an output header now.
		print_full_header(source_label.c_str());
	} else {
		// digest each job into a row of values and insert into the rod_result_map
		pfnProcess = init_cache_optimizer(process_job_to_rod_per_ad_map);
		pvProcess = &rod_result_map;
	}

	int fetch_opts = 0;
	if (dash_autocluster) {
		fetch_opts = dash_autocluster;
	}
	bool use_v3 = dash_autocluster || param_boolean("CONDOR_Q_USE_V3_PROTOCOL", true);
	if ((useFastPath > 1) && !use_v3) {
		useFastPath = 1;
	}
	if ( ! fetch_opts && (useFastPath > 1)) {
		fetch_opts = default_fetch_opts;
	}
	if ((useFastPath > 1) && ((fetch_opts & QueryFetchOpts::fetch_FromMask) == QueryFetchOpts::fetch_Jobs)) {
		if (dash_tot && ! dash_unmatchable) {
			fetch_opts |= QueryFetchOpts::fetch_SummaryOnly;
#ifdef CONDOR_Q_HANDLE_CLUSTER_AD 
		} else if (dash_factory && (dash_long || ! dash_batch)) {
			fetch_opts |= QueryFetchOpts::fetch_IncludeClusterAd;
#endif
		} else if (dash_jobset) {
			fetch_opts |= QueryFetchOpts::fetch_JobsetAds;
		}
	}
	classad::References no_attrs;
	classad::References *pattrs = &app.attrs;
	if (dash_unmatchable) pattrs = &no_attrs; // we need all of the attrs to do matchmaking.
	int fetchResult;
	if (dash_dry_run) {
		fetchResult = dryFetchQueue(dry_run_file, *pattrs, fetch_opts, g_match_limit, pfnProcess, pvProcess);
	} else {
		if (dash_long || pattrs->contains(ATTR_SERVER_TIME)) {
			// Ask the schedd to add the ServerTime attribute to the each of the job ads.
			// this was the default before 9.10.0 (see HTCONDOR-1125)
			// we do this so that a subsequent "condor_q -jobs <file> -nobatch" will show the correct job times.
			Q.requestServerTime(true);
		}
		StringList attrs_sl(JoinAttrNames(*pattrs,","));
		fetchResult = Q.fetchQueueFromHostAndProcess(scheddAddress, attrs_sl, fetch_opts, g_match_limit, pfnProcess, pvProcess, useFastPath, &errstack, &summary_ad);
		// In support of HTCONDOR-1125, grab queue time from summary ad if it is there.
		if (summary_ad) { summary_ad->LookupInteger(ATTR_SERVER_TIME, queue_time); }
	}
	cleanup_cache_optimizer();
	if (fetchResult != Q_OK) {
		// The parse + fetch failed, print out why
		switch(fetchResult) {
		case Q_PARSE_ERROR:
		case Q_INVALID_CATEGORY:
			fprintf(stderr, "\n-- Parse error in constraint expression \"%s\"\n", user_job_constraint);
			break;
		case Q_UNSUPPORTED_OPTION_ERROR:
			fprintf(stderr, "\n-- Unsupported query option at: %s : %s\n", scheddAddress, scheddMachine);
			break;
		default:
			fprintf(stderr,
				"\n-- Failed to fetch ads from: %s : %s\n%s\n",
				scheddAddress, scheddMachine, errstack.getFullText(true).c_str() );
		}
		return false;
	}

	// if we opened a raw capture file, we can close it now.
	if (capture_raw_results && *capture_raw_results != '-' && capture_raw_fp) {
		fclose(capture_raw_fp);
		capture_raw_fp = NULL;
	}

	// Modern schedds will return as summary ad. otherwise we create one from our own totals
	// in either case, we (sometimes) want to skip printing of the summary ad when there are no jobs
	// so set a flag now we are refer to later.
	bool empty_summary = true;
	if (summary_ad && ! dash_unmatchable) {
		empty_summary = is_zero_summary(summary_ad);
	} else {
		// fake up a summary ad
		summary_ad = new ClassAd();
		summary_ad->Assign(ATTR_MY_TYPE, "Summary");
		app.sumy.publish(*summary_ad, NULL);
		empty_summary = app.sumy.empty();
	}

	// if we streamed, then the results have already been printed out.
	// we just need to write the footer/summary
	if (g_stream_results) {
		print_full_footer(summary_ad, &writer);
		delete summary_ad;
		return true;
	}

	// at this point we have either a collection of results or a collection of ads
	int cFullAds = (int)ads.size();
	int cResults = (int)rod_result_map.size();

	if (dash_profile) {
		int cAds = cFullAds + (int)rod_result_map.size();
		profile_print(cbBefore, tmBefore, cAds);
	} else if (verbose) {
		fprintf(stderr, " %d ads\n", cFullAds);
	}

	if (better_analyze) {
		
		//PRAGMA_REMIND("TJ: shouldn't this be using scheddAddress instead of scheddName?")
		DaemonAllowLocateFull schedd(DT_SCHEDD, scheddName, pool ? pool->addr() : NULL );
		delete summary_ad;

		return print_jobs_analysis(ads, source_label.c_str(), &schedd);
	}

	if (dash_long) {
		if (global && empty_summary) {
			// print nothing for -global when there are no results
		} else {
			std::string buf;
			for (auto it = ads.begin(); it != ads.end(); ++it) {
				buf.clear();
				append_long_ad(buf, writer, *it->second);
				if ( ! buf.empty()) { fputs(buf.c_str(), stdout); }
			}
		}
		print_full_footer(summary_ad, &writer);
		delete summary_ad;
		return true;
	}

	// at this point we either have a populated ad collection, or a populated rod_result_map
	// if it's an ad collection, then we want to process the ads into the rod_result_map
	// then we want to linkup the nodes and (possibly) rewrite the name columns
	if (cFullAds > 0) {
		app.sumy.clear_counters(); // so we don't double count. (although since we captured the summary ad already, it probably doesn't matter)
		for (auto it = ads.begin(); it != ads.end(); ++it) {
			process_job_to_rod_per_ad_map(&rod_result_map, it->second.get());
		}
		cResults = (int)rod_result_map.size();
	}

	if (cResults > 0) {
		linkup_nodes_by_id(rod_result_map);
		if (dash_batch) {
			reduce_results(rod_result_map);
		} else if (dash_dag) {
			format_name_column_for_dag_nodes(rod_result_map, name_column_index, name_column_width);
		}
	}


	// we want a header for this schedd if we are not showing the global queue OR if there is any data
	if ( ! global || cResults > 0 || !empty_summary) {
		print_full_header(source_label.c_str());
	}

	print_results(rod_result_map, rod_sort_key_map, dash_dag);
	clear_results(rod_result_map, rod_sort_key_map);

	// print out summary line (if enabled) and/or xml closure
	// we want to print out a footer if we printed any data, OR if this is not 
	// a global query, so that the user can see that we did something.
	//
	if ( ! global || cResults > 0 || !empty_summary) {
		print_full_footer(summary_ad, &writer);
	}

	delete summary_ad;
	return true;
}

static int
dryFetchQueue(const char * file, const classad::References & proj, int fetch_opts, int limit, buffer_line_processor pfnProcess, void *pvProcess)
{
	// print header
	int ver = 2;
	fprintf(stderr, "\nDryRun v%d from %s\n", ver, file ? file : "<null>");

	// get the constraint as a string
	ConstraintHolder constr;
	if (GetQueueConstraint(Q, constr) < 0) {
		fprintf(stderr, "Invalid constraint: %s\n", constr.c_str());
	}
	// print constraint
	const char * constr_str = constr.Str();
	fprintf(stderr, "Constraint: %s\n", constr_str ? constr_str : "<null>");
	int cluster = -99, proc = -99;
	bool cluster_only = false, dagman_job_id = false;
	bool is_job_id_constr = ExprTreeIsJobIdConstraint(constr.Expr(), cluster, proc, cluster_only, dagman_job_id);
	fprintf(stderr, "  IsJobId: %s %d.%d%s%s\n",
		is_job_id_constr ? "true" : "false",
		cluster, proc,
		cluster_only ? " cluster_only" : "",
		dagman_job_id ? " dag_id" : ""
	);

	fprintf(stderr, "Opts: fetch=%d limit=%d HF=%x\n", fetch_opts, limit, customHeadFoot);

	// print projection
	std::string projection = JoinAttrNames(proj, "\n  ");
	if (projection.empty()) {
		fprintf(stderr, "Projection: <NULL>\n");
	} else {
		fprintf(stderr, "Projection:\n  %s\n",  projection.c_str());
	}

	// print the sort keys
	std::string tmp("");
	for (auto it = group_by_keys.begin(); it != group_by_keys.end(); ++it) {
		if ( ! tmp.empty()) tmp += "] [";
		tmp += it->expr;
	}
	fprintf(stderr, "Sort: [%s]\n", tmp.c_str());
	tmp.clear();

	// print output mask
	dump_print_mask(tmp);
	fprintf(stderr, "Mask:\n%s\n", tmp.c_str());

	// 
	if ( ! file || ! file[0]) { 
		return Q_OK;
	}

	FILE* fh;
	bool close_file = false;
	if (MATCH == strcmp(file, "-")) {
		fh = stdin;
		close_file = false;
	} else {
		fh = safe_fopen_wrapper_follow(file, "r");
		if (fh == NULL) {
			fprintf(stderr, "Can't open file of ClassAds: %s\n", file);
			return Q_COMMUNICATION_ERROR;
		}
		close_file = true;
	}

	CondorQClassAdFileParseHelper jobads_file_parse_helper(jobads_file_format);

	CondorClassAdFileIterator adIter;
	if ( ! adIter.begin(fh, close_file, jobads_file_parse_helper)) {
		if (close_file) { fclose(fh); fh = NULL; }
		return Q_COMMUNICATION_ERROR;
	} else {
		ClassAd * job;
		while ((job = adIter.next(constr.Expr()))) {
			if (pfnProcess(pvProcess, job)) {
				delete job; job = NULL;
			}
		}
	}
	fh = NULL;

	return Q_OK;
}

// Read ads from a file in classad format
bool iter_ads_from_file(const char *filename,
	bool (*pfnProcess)(void* pv, ClassAd* ad), void* pvProcess,
	CondorClassAdFileParseHelper & parse_helper, ExprTree * constraint)
{
	FILE* fh;
	bool close_file = false;
	if (MATCH == strcmp(filename, "-")) {
		fh = stdin;
		close_file = false;
	} else {
		fh = safe_fopen_wrapper_follow(filename, "r");
		if (fh == NULL) {
			fprintf(stderr, "Can't open file of ClassAds: %s\n", filename);
			return false;
		}
		close_file = true;
	}

	CondorClassAdFileIterator adIter;
	if ( ! adIter.begin(fh, close_file, parse_helper)) {
		if (close_file) { fclose(fh); fh = NULL; }
		return false;
	} else {
		ClassAd * ad;
		while ((ad = adIter.next(constraint))) {
			if (pfnProcess(pvProcess, ad)) {
				delete ad; ad = NULL;
			}
		}
	}
	fh = NULL;
	return true;
}

// Read ads from a file, either in classad format, or userlog format.
//
static bool
show_file_queue(const char* jobads, const char* userlog)
{
	// initialize counters
	app.sumy.clear_counters();

	ConstraintHolder constr;
	if (GetQueueConstraint(Q, constr) < 0) {
		fprintf(stderr, "Invalid constraint: %s\n", constr.c_str());
	}

	if (verbose || dash_profile) {
		fprintf(stderr, "Fetching job ads...");
	}

	size_t cbBefore = 0;
	double tmBefore = 0;
	if (dash_profile) { cbBefore = ProcAPI::getBasicUsage(getpid(), &tmBefore); }

	IdToClassaAdMap jobs;
	std::string source_label;

	if (jobads != NULL) {
		/* get the "q" from the job ads file */
		CondorQClassAdFileParseHelper jobads_file_parse_helper(jobads_file_format);
		if ( ! iter_ads_from_file(jobads, AddJobToClassAdCollection, &jobs, jobads_file_parse_helper, constr.Expr())) {
			return false;
		}

		// if we are doing analysis, print out the schedd name and address
		// that we got from the classad file.
		if (better_analyze && ! jobads_file_parse_helper.schedd_name.empty()) {
			const char *scheddName    = jobads_file_parse_helper.schedd_name.c_str();
			const char *scheddAddress = jobads_file_parse_helper.schedd_addr.c_str();
			const char *queryType     = jobads_file_parse_helper.is_submitter ? "Submitter" : "Schedd";
			formatstr(source_label, "%s: %s : %s", queryType, scheddName, scheddAddress);
		} else {
			formatstr(source_label, "Jobads: %s", jobads);
		}
		
	} else if (userlog != NULL) {
		CondorID * JobIds = NULL;
		int cJobIds = (int)constrID.size();
		if (cJobIds > 0) JobIds = &constrID[0];

		if ( ! userlog_to_classads(userlog, AddJobToClassAdCollection, &jobs, JobIds, cJobIds, constr.Expr())) {
			fprintf(stderr, "\nCan't open user log: %s\n", userlog);
			return false;
		}
		formatstr(source_label, "Userlog: %s", userlog);
	} else {
		ASSERT(jobads != NULL || userlog != NULL);
	}

	int cJobs = (int)jobs.size();

	if (dash_profile) {
		profile_print(cbBefore, tmBefore, cJobs);
	} else if (verbose) {
		fprintf(stderr, " %d ads.\n", cJobs);
	}

	if (cJobs > 1) {
		if (dash_profile) {
			profile_print(cbBefore, tmBefore, cJobs, false);
		} else if (verbose) {
			fprintf(stderr, "\n");
		}
	} else if (verbose) {
		fprintf(stderr, "\n");
	}

	if (better_analyze) {
		return print_jobs_analysis(jobs, source_label.c_str(), NULL);
	}

	CondorClassAdListWriter writer(dash_long_format);

		// display the jobs from this submittor
	if( cJobs != 0 || !global ) {

		app.sumy.clear_counters();
		for (auto it = jobs.begin(); it != jobs.end(); ++it) {
			ClassAd * job = it->second.get();
			if (dash_long) {
				streaming_print_job(&writer, job);
			} else {
				process_job_to_rod_per_ad_map(&rod_result_map, job);
			}
		}

		std::unique_ptr<ClassAd> summary_ad(new ClassAd);

		summary_ad->Assign(ATTR_MY_TYPE, "Summary");
		app.sumy.publish(*summary_ad.get(), NULL);

		if (dash_long) {
			print_full_footer(summary_ad.get(), &writer);
			writer.writeFooter(stdout, always_write_xml_footer);
			return true;
		}

		int cResults = (int)rod_result_map.size();
		if (cResults > 0) {
			linkup_nodes_by_id(rod_result_map);
			if (dash_batch) {
				reduce_results(rod_result_map);
			} else if (dash_dag) {
				format_name_column_for_dag_nodes(rod_result_map, name_column_index, name_column_width);
			}
		}

		print_full_header(source_label.c_str());
		print_results(rod_result_map, rod_sort_key_map, dash_dag);
		clear_results(rod_result_map, rod_sort_key_map);

		print_full_footer(summary_ad.get(), &writer);
	}
	if (dash_long) { writer.writeFooter(stdout, always_write_xml_footer); }

	return true;
}


const char * const jobDefault_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX  WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX  WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER           WIDTH -14 PRINTAS OWNER OR ??\n"
"   QDate         AS '  SUBMITTED'    WIDTH 11 PRINTAS QDATE\n"
"   RemoteUserCpu AS '    RUN_TIME'   WIDTH 12 PRINTAS CPU_TIME\n"
"   JobStatus     AS ST                       PRINTAS JOB_STATUS\n"
"   JobPrio       AS PRI             WIDTH AUTO\n"
"   ImageSize     AS SIZE            WIDTH 4 PRINTAS MEMORY_USAGE\n"
"   Cmd           AS CMD             WIDTH 0 PRINTAS JOB_DESCRIPTION\n"
"SUMMARY STANDARD\n";

const char * const jobFactory_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX  WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX  WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER           WIDTH -14 PRINTAS OWNER OR ??\n"
"   QDate         AS '  SUBMITTED'    WIDTH 11 PRINTAS QDATE\n"
    ATTR_JOB_MATERIALIZE_LIMIT        " AS ' LIMIT' WIDTH 6 PRINTF %6d\n"
   "JobsPresent   AS 'PRESNT' WIDTH 6 PRINTF %6d\n"
   "JobsRunning   AS '  RUN ' WIDTH 6 PRINTF %6d\n"
   "JobsIdle      AS '  IDLE' WIDTH 6 PRINTF %6d\n"
   "JobsHeld      AS '  HOLD' WIDTH 6 PRINTF %6d\n"
    ATTR_JOB_MATERIALIZE_NEXT_PROC_ID " AS 'NEXTID' WIDTH 6 PRINTF %6d\n"
    ATTR_JOB_MATERIALIZE_PAUSED "?:JobFactoryPaused AS MODE PRINTAS JOB_FACTORY_MODE OR _\n"
    ATTR_JOB_MATERIALIZE_DIGEST_FILE  " AS DIGEST\n"
"WHERE (ProcId is undefined) && (" ATTR_JOB_MATERIALIZE_DIGEST_FILE " isnt undefined)\n"
"SUMMARY NONE\n";

const char * const jobDAG_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX  WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX  WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS 'OWNER/NODENAME' WIDTH -17 PRINTAS DAG_OWNER\n"
"   QDate         AS '  SUBMITTED'    WIDTH 11 PRINTAS QDATE\n"
"   RemoteUserCpu AS '    RUN_TIME'   WIDTH 12 PRINTAS CPU_TIME\n"
"   JobStatus     AS ST                       PRINTAS JOB_STATUS\n"
"   JobPrio       AS PRI             WIDTH AUTO\n"
"   ImageSize     AS SIZE            WIDTH 4 PRINTAS MEMORY_USAGE\n"
"   Cmd           AS CMD             WIDTH 0 PRINTAS JOB_DESCRIPTION\n"
"SUMMARY STANDARD\n";

const char * const jobRuntime_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER          WIDTH -14 PRINTAS OWNER OR ??\n"
"   QDate         AS '  SUBMITTED'   WIDTH 11  PRINTAS QDATE OR ??\n"
"   RemoteUserCpu AS '    RUN_TIME'  WIDTH 12  PRINTAS CPU_TIME OR ??\n"
"   Owner         AS 'HOST(S)'       WIDTH 0   PRINTAS REMOTE_HOST OR ??\n"
"SUMMARY NONE\n";

const char * const jobIdle_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER          WIDTH -14 PRINTAS OWNER OR ??\n"
"   JobUniverse   AS 'UNIVERSE '  PRINTAS JOB_UNIVERSE\n"
"   RequestCpus   AS CPUS         PRINTF '%4d'\n"
"   RequestMemory AS MEMORY       PRINTF '%6d'\n"
"   NumShadowStarts?:0 AS STARTS  PRINTF '%6d'\n"
"   LastRemoteHost AS LAST_HOST WIDTH 0\n"
"SUMMARY NONE\n";

const char * const jobGoodput_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER          WIDTH -14 PRINTAS OWNER OR ??\n"
"   QDate         AS '  SUBMITTED'   WIDTH 11  PRINTAS QDATE OR ??\n"
"   RemoteUserCpu AS '    RUN_TIME'  WIDTH 12  PRINTAS CPU_TIME OR ??\n"
"   JobStatus     AS GOODPUT         WIDTH 8   PRINTAS STDU_GOODPUT OR ??\n"
"   RemoteUserCpu AS CPU_UTIL        WIDTH 9   PRINTAS CPU_UTIL OR ??\n"
"   BytesSent     AS 'Mb/s'          WIDTH 7   PRINTAS STDU_MPBS OR ??\n"
"SUMMARY NONE\n";

const char * const jobGrid_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER          WIDTH -14 PRINTAS OWNER\n"
"   GridJobStatus AS STATUS          WIDTH -10 PRINTAS GRID_STATUS\n"
"   GridResource  AS 'GRID->MANAGER    HOST' WIDTH -27 PRINTAS GRID_RESOURCE\n"
"   GridJobId     AS GRID_JOB_ID             WIDTH   0 PRINTAS GRID_JOB_ID\n"
"SUMMARY NONE\n";

const char * const jobGridEC2_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH    5 PRINTF  '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH    3 PRINTF  '%-3d'\n"
"   Owner         AS OWNER           WIDTH  -14 PRINTAS OWNER\n"
"   GridJobStatus AS STATUS          WIDTH  -10 PRINTAS GRID_STATUS\n"
"   GridJobId     AS 'INSTANCE ID'   WIDTH AUTO PRINTAS GRID_JOB_ID\n"
"   Cmd           AS CMD             WIDTH AUTO PRINTF  '%s'\n"
"SUMMARY NONE\n";

const char * const jobHold_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER WIDTH -14 PRINTAS OWNER OR ??\n"
"   EnteredCurrentStatus  AS HELD_SINCE WIDTH 11 PRINTAS QDATE OR ??\n"
"   HoldReason           AS HOLD_REASON WIDTH 0\n"
"SUMMARY STANDARD\n";

const char * const jobIO_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER WIDTH -14 PRINTAS OWNER OR ??\n"
"   IfThenElse(JobUniverse==1,NumCkpts_RAW,NumJobStarts) AS RUNS PRINTF '%4d' OR ?\n"
"   JobStatus     AS ST                       PRINTAS JOB_STATUS\n"
"   IfThenElse(JobUniverse==1,FileReadBytes,BytesRecvd) AS ' INPUT' FORMATAS READABLE_BYTES OR ??\n"
"   IfThenElse(JobUniverse==1,FileWriteBytes,BytesSent) AS ' OUTPUT' FORMATAS READABLE_BYTES OR ??\n"
"   IfThenElse(JobUniverse==1,FileReadBytes+FileWriteBytes,BytesRecvd+BytesSent)   AS ' RATE' WIDTH 10 FORMATAS READABLE_BYTES OR ??\n"
"   JobUniverse AS 'MISC' FORMATAS BUFFER_IO_MISC\n"
"WHERE JobUniverse==1 || TransferQueued=?=true || TransferringOutput=?=true || TransferringInput=?=true\n"
"SUMMARY STANDARD\n";

const char * const jobTotals_PrintFormat = "SELECT NOHEADER\nSUMMARY STANDARD";

const char * const autoclusterNormal_PrintFormat = "SELECT\n"
"   AutoClusterId AS '   ID'    WIDTH 5 PRINTF %5d\n"
"   JobCount      AS COUNT      WIDTH 5 PRINTF %5d\n"
"   JobUniverse   AS UINVERSE   WIDTH -8 PRINTAS JOB_UNIVERSE OR ??\n"
"   RequestCPUs   AS CPUS       WIDTH 4 PRINTF %4d OR ??\n"
"   RequestMemory AS MEMORY     WIDTH 6 PRINTF %6d OR ??\n"
"   RequestDisk   AS '    DISK' WIDTH 8 PRINTF %8d OR ??\n"
"   Requirements  AS REQUIREMENTS PRINTF %r\n"
"SUMMARY NONE\n";

const char * const jobsetNormal_PrintFormat = "SELECT\n"
"   JobSetId      AS  JOBSETID  WIDTH 8 PRINTF %8d\n"
"   JobSetName    AS  NAME      WIDTH AUTO\n"
"   Owner         AS  OWNER     WIDTH AUTO PRINTAS OWNER OR ??\n"
"   NumCompleted  AS  COMPLETE  WIDTH 8 PRINTF %8d\n"
"   NumRemoved    AS  REMOVED   WIDTH 7 PRINTF %7d\n"
"   NumIdle       AS '   IDLE'  WIDTH 7 PRINTF %7d\n"
"   NumRunning    AS  RUNNING   WIDTH 7 PRINTF %7d\n"
"   NumHeld       AS '   HELD'  WIDTH 7 PRINTF %7d\n"
"WHERE ProcId is undefined\n"
"SUMMARY NONE\n";


// !!! ENTRIES IN THIS TABLE MUST BE SORTED BY THE FIRST FIELD !!
static const CustomFormatFnTableItem LocalPrintFormats[] = {
	{ "BATCH_NAME",      ATTR_JOB_CMD, 0, local_render_batch_name, ATTR_JOB_BATCH_NAME "\0" ATTR_JOB_CMD "\0" ATTR_DAGMAN_JOB_ID "\0" ATTR_DAG_NODE_NAME "\0" },
	{ "CPU_TIME",        ATTR_JOB_REMOTE_USER_CPU, "%T", local_render_cpu_time, ATTR_JOB_STATUS "\0" ATTR_SERVER_TIME "\0" ATTR_SHADOW_BIRTHDATE "\0" ATTR_JOB_REMOTE_WALL_CLOCK "\0" ATTR_JOB_LAST_REMOTE_WALL_CLOCK "\0"},
	{ "DAG_OWNER",       ATTR_OWNER, 0, local_render_dag_owner, ATTR_NICE_USER_deprecated "\0" ATTR_DAGMAN_JOB_ID "\0" ATTR_DAG_NODE_NAME "\0"  },
	{ "GRID_RESOURCE",   ATTR_GRID_RESOURCE, 0, local_render_grid_resource, ATTR_EC2_REMOTE_VM_NAME "\0" },
	{ "JOB_STATUS",      ATTR_JOB_STATUS, 0, local_render_job_status_char, ATTR_LAST_SUSPENSION_TIME "\0" ATTR_TRANSFERRING_INPUT "\0" ATTR_TRANSFERRING_OUTPUT "\0" ATTR_TRANSFER_QUEUED "\0" },
	{ "MEMORY_USAGE",    ATTR_IMAGE_SIZE, "%.1f", local_render_memory_usage, ATTR_MEMORY_USAGE "\0" },
	{ "OWNER",           ATTR_OWNER, 0, local_render_owner, ATTR_NICE_USER_deprecated "\0" },
	{ "REMOTE_HOST",     ATTR_OWNER, 0, local_render_remote_host, ATTR_JOB_UNIVERSE "\0" ATTR_REMOTE_HOST "\0" ATTR_EC2_REMOTE_VM_NAME "\0" ATTR_GRID_RESOURCE "\0" },
};
static const CustomFormatFnTable LocalPrintFormatsTable = SORTED_TOKENER_TABLE(LocalPrintFormats);

static void dump_print_mask(std::string & tmp)
{
	app.prmask.dump(tmp, &LocalPrintFormatsTable);
}

static int set_print_mask_from_stream(
	AttrListPrintMask & prmask,
	const char * streamid,
	bool is_filename,
	classad::References & attrs,
	AttrListPrintMask & sumymask)
{
	PrintMaskMakeSettings propt;
	std::string messages;

	SimpleInputStream * pstream = NULL;
	propt.headfoot = customHeadFoot;

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
					&LocalPrintFormatsTable,
					prmask,
					propt,
					group_by_keys,
					&sumymask,
					messages);
	delete pstream; pstream = NULL;
	if ( ! err) {
		customHeadFoot = propt.headfoot;
		if ( ! propt.where_expression.empty()) {
			user_job_constraint = prmask.store(propt.where_expression.c_str());
			if (Q.addAND (user_job_constraint) != Q_OK) {
				formatstr_cat(messages, "WHERE expression is not valid: %s\n", user_job_constraint);
			}
		}
		if (propt.aggregate) {
			if (propt.aggregate == PR_COUNT_UNIQUE) {
				dash_autocluster = QueryFetchOpts::fetch_GroupBy;
				attrs.insert(propt.attrs.begin(), propt.attrs.end());
			} else if (propt.aggregate == PR_FROM_AUTOCLUSTER) {
				dash_autocluster = QueryFetchOpts::fetch_DefaultAutoCluster;
			}
		} else {
			// make sure that the projection has ClusterId and ProcId.
			propt.attrs.insert(ATTR_CLUSTER_ID);
			propt.attrs.insert(ATTR_PROC_ID);
			if (dash_jobset) propt.attrs.insert(ATTR_JOB_SET_ID);
			if ( ! (propt.headfoot & HF_NOSUMMARY)) {
				// in case we are generating the summary line, make sure that we have JobStatus and JobUniverse
				propt.attrs.insert(ATTR_JOB_STATUS);
				propt.attrs.insert(ATTR_JOB_UNIVERSE);
			}
			attrs.insert(propt.attrs.begin(), propt.attrs.end());

			// if using the standard summary, we need to set that up now.
			if ((propt.headfoot & (HF_NOSUMMARY | HF_CUSTOM)) == 0) {
				auto_standard_summary = true;
			}
		}
	}
	if ( ! messages.empty()) { fprintf(stderr, "%s", messages.c_str()); }
	return err;
}

static bool is_zero_summary(ClassAd * summary_ad) 
{
	if (summary_ad) {
		long long count = 0;
		if (summary_ad->LookupInteger("Jobs", count) && count)
			return false;
		if (summary_ad->LookupInteger("SchedulerJobs", count) && count)
			return false;

		// if displaying the legacy summary, we don't care about Allusers
		// because -all will have the same values in the query totals as the Allusers totals
		if (use_legacy_standard_summary)
			return true;

		if (summary_ad->LookupInteger("AllusersJobs", count) && count)
			return false;
	}
	return true;
}

/*
const char * const standard_summary1 = "SELECT\n"
	"Name PRINTF 'Total for %s:'\n"
	"Jobs PRINTF '%d jobs;'\n"
	"Completed PRINTF '%d completed,'\n"
	"Removed PRINTF '%d removed,'\n"
	"Idle PRINTF '%d idle,'\n"
	"Running PRINTF '%d running,'\n"
	"Held PRINTF '%d held,'\n"
	"Suspended PRINTF '%d suspended'\n"
;
*/

const char * const standard_summary2 = "SELECT\n"
	"Jobs PRINTF 'Total for query: %d jobs;'\n"
	"Completed PRINTF '%d completed,'\n"
	"Removed PRINTF '%d removed,'\n"
	"Idle PRINTF '%d idle,'\n"
	"Running PRINTF '%d running,'\n"
	"Held PRINTF '%d held,'\n"
	"Suspended PRINTF '%d suspended'\n"

	"AllusersJobs AS Jobs PRINTF '\\nTotal for all users: %d jobs;'\n"
	"AllusersCompleted AS Completed PRINTF '%d completed,'\n"
	"AllusersRemoved AS Removed PRINTF '%d removed,'\n"
	"AllusersIdle AS Idle PRINTF '%d idle,'\n"
	"AllusersRunning AS Running PRINTF '%d running,'\n"
	"AllusersHeld AS Held PRINTF '%d held,'\n"
	"AllusersSuspended AS Suspended PRINTF '%d suspended'\n"
;

const char * const standard_summary3 = "SELECT\n"
	"Jobs PRINTF 'Total for query: %d jobs;'\n"
	"Completed PRINTF '%d completed,'\n"
	"Removed PRINTF '%d removed,'\n"
	"Idle PRINTF '%d idle,'\n"
	"Running PRINTF '%d running,'\n"
	"Held PRINTF '%d held,'\n"
	"Suspended PRINTF '%d suspended'\n"

	"MyJobs AS Jobs PRINTF '\\nTotal for $(ME): %d jobs;'\n"
	"MyCompleted AS Completed PRINTF '%d completed,'\n"
	"MyRemoved AS Removed PRINTF '%d removed,'\n"
	"MyIdle AS Idle PRINTF '%d idle,'\n"
	"MyRunning AS Running PRINTF '%d running,'\n"
	"MyHeld AS Held PRINTF '%d held,'\n"
	"MySuspended AS Suspended PRINTF '%d suspended'\n"

	"AllusersJobs AS Jobs PRINTF '\\nTotal for all users: %d jobs;'\n"
	"AllusersCompleted AS Completed PRINTF '%d completed,'\n"
	"AllusersRemoved AS Removed PRINTF '%d removed,'\n"
	"AllusersIdle AS Idle PRINTF '%d idle,'\n"
	"AllusersRunning AS Running PRINTF '%d running,'\n"
	"AllusersHeld AS Held PRINTF '%d held,'\n"
	"AllusersSuspended AS Suspended PRINTF '%d suspended'\n"
;

const char * const standard_summary_legacy = "SELECT\n"
	"Jobs + SchedulerJobs AS Jobs PRINTF '%d jobs;'\n"
	"Completed + SchedulerCompleted AS Completed PRINTF '%d completed,'\n"
	"Removed + SchedulerRemoved AS Removed PRINTF '%d removed,'\n"
	"Idle + SchedulerIdle AS Idle PRINTF '%d idle,'\n"
	"Running + SchedulerRunning AS Running PRINTF '%d running,'\n"
	"Held + SchedulerHeld AS Held PRINTF '%d held,'\n"
	"Suspended PRINTF '%d suspended'\n"
;

static void init_standard_summary_mask(ClassAd * summary_ad)
{
	std::string messages;
	PrintMaskMakeSettings dummySettings;
	std::vector<GroupByKeyInfo> dummyGrpBy;
	std::string sumyformat(standard_summary2);
	std::string myname;
	if (summary_ad->LookupString("MyName", myname)) { 
		sumyformat = standard_summary3;
		replace_str(sumyformat, "$(ME)", myname);
	}
	if (use_legacy_standard_summary) {
		sumyformat = standard_summary_legacy;
	}
	StringLiteralInputStream stream(sumyformat.c_str());
	dummySettings.reset();
	app.sumymask.clearFormats();
	SetAttrListPrintMaskFromStream(stream, &LocalPrintFormatsTable, app.sumymask, dummySettings, dummyGrpBy, NULL, messages);

	// dont' actually want to display headings for the summary lines when using standard_summary2 or standard_summary3
	app.sumymask.clear_headings();
}

