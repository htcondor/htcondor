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
#include "condor_string.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "get_daemon_name.h"
#include "MyString.h"
#include "ad_printmask.h"
#include "internet.h"
#include "sig_install.h"
#include "format_time.h"
#include "daemon.h"
#include "dc_collector.h"
#include "basename.h"
#include "metric_units.h"
#include "globus_utils.h"
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
#include <map>
#include <vector>
#include "../classad_analysis/analysis.h"
//#include "pool_allocator.h"
#include "expr_analyze.h"
#include "classad/classadCache.h" // for CachedExprEnvelope stats

static int cleanup_globals(int exit_code); // called on exit to do necessary cleanup
#define exit(n) (exit)(cleanup_globals(n))

#ifdef HAVE_EXT_POSTGRESQL
#include "sqlquery.h"
#endif /* HAVE_EXT_POSTGRESQL */

#define USE_LATE_PROJECTION 1

/* Since this enum can have conditional compilation applied to it, I'm
	specifying the values for it to keep their actual integral values
	constant in case someone decides to write this information to disk to
	pass it to another daemon or something. This enum is used to determine
	who to talk to when using the -direct option. */
enum {
	/* don't know who I should be talking to */
	DIRECT_UNKNOWN = 0,
	/* start at the rdbms and fail over like normal */
	DIRECT_ALL = 1,
#ifdef HAVE_EXT_POSTGRESQL
	/* talk directly to the rdbms system */
	DIRECT_RDBMS = 2,
	/* talk directly to the quill daemon */
	DIRECT_QUILLD = 3,
#endif
	/* talk directly to the schedd */
	DIRECT_SCHEDD = 4
};

struct 	PrioEntry { MyString name; float prio; };

#ifdef WIN32
static int getConsoleWindowSize(int * pHeight = NULL) {
	CONSOLE_SCREEN_BUFFER_INFO ws;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ws)) {
		if (pHeight)
			*pHeight = (int)(ws.srWindow.Bottom - ws.srWindow.Top)+1;
		return (int)ws.dwSize.X;
	}
	return 80;
}
#else
#include <sys/ioctl.h> 
static int getConsoleWindowSize(int * pHeight = NULL) {
    struct winsize ws; 
	if (0 == ioctl(0, TIOCGWINSZ, &ws)) {
		//printf ("lines %d\n", ws.ws_row); 
		//printf ("columns %d\n", ws.ws_col); 
		if (pHeight)
			*pHeight = (int)ws.ws_row;
		return (int) ws.ws_col;
	}
	return 80;
}
#endif

static  int  testing_width = 0;
static int getDisplayWidth() {
	if (testing_width <= 0) {
		testing_width = getConsoleWindowSize();
		if (testing_width <= 0)
			testing_width = 80;
	}
	return testing_width;
}

extern 	"C" int SetSyscalls(int val){return val;}
extern  void short_print(int,int,const char*,int,int,int,int,int,const char *);
static  void processCommandLineArguments(int, char *[]);

static  bool streaming_print_job(void*, ClassAd*);
typedef bool (* buffer_line_processor)(void*, ClassAd *);

static 	void usage (const char *, int other=0);
enum { usage_Universe=1, usage_JobStatus=2, usage_AllOther=0xFF };
static bool render_job_status_char(std::string & result, AttrList*ad, Formatter &);

// functions to fetch job ads and print them out
//
static bool show_file_queue(const char* jobads, const char* userlog);
static bool show_schedd_queue(const char* scheddAddress, const char* scheddName, const char* scheddMachine, int useFastPath);
#ifdef HAVE_EXT_POSTGRESQL
static bool show_db_queue( const char* quill_name, const char* db_ipAddr, const char* db_name, const char* query_password);
#endif
static int dryFetchQueue(const char * file, StringList & proj, int fetch_opts, int limit, buffer_line_processor pfnProcess, void *pvProcess);

#ifdef USE_LATE_PROJECTION
static void initOutputMask(AttrListPrintMask & pqmask, int qdo_mode, bool wide_mode);
//PRAGMA_REMIND("make width of the name column adjust to the display width")
const int name_column_index = 2;
int name_column_width = 14;
bool is_standard_format = false;
bool first_col_is_job_id = false;
bool has_owner_column = false;
int  max_name_column_width = 14;

int max_owner_name = 0; // width of longest owner name, from calls to render_owner or render_dag_owner
int max_batch_name = 0; // width of longest batch name
//int max_cluster_id = 0; // future
//int max_proc_id = 0;    // future
#else
static 	void short_header (void);
static 	void buffer_io_display (std::string & out, ClassAd *);
static 	char * bufferJobShort (ClassAd *);
static void init_output_mask();
static const char* format_owner(const char*, AttrList*);
#endif

static bool read_classad_file(const char *filename, ClassAdList &classads, ClassAdFileParseHelper* pparse_help, const char * constr);
static int read_userprio_file(const char *filename, ExtArray<PrioEntry> & prios);



/* convert the -direct aqrgument prameter into an enum */
unsigned int process_direct_argument(char *arg);

#ifdef HAVE_EXT_POSTGRESQL
static void quillCleanupOnExit(int n);
static void freeConnectionStrings();

// use std::strings to hold these strings so that we don't have to worry about freeing them on exit.
static std::string squillName, squeryPassword, sdbIpAddr, sdbName, squillMachine, quillAddr;
const char *quillName = NULL;
const char *dbIpAddr = NULL;
const char *dbName = NULL;
const char *queryPassword = NULL;
const char *quillMachine = NULL;

/* execute a database query directly */ 
static void exec_db_query(const char *quill_name, const char *db_ipAddr, const char *db_name,const char *query_password);

/* build database connection string */
static char * getDBConnStr(char const *&, char const *&, char const *&, char const *&);

/* get the quill address for the quill_name specified */
static QueryResult getQuillAddrFromCollector(const char *quill_name, std::string &quill_addr);

/* avgqueuetime is used to indicate a request to query average wait time for uncompleted jobs in queue */
static  bool avgqueuetime = false;

/* directDBquery means we will just run a database query and return results directly to user */
static  bool directDBquery = false;
#endif

/* Warn about schedd-wide limits that may confuse analysis code */
void warnScheddLimits(Daemon *schedd,ClassAd *job,MyString &result_buf);

/* who is it that I should directly ask for the queue, defaults to the normal
	failover semantics */
static unsigned int direct = DIRECT_ALL;

static 	int dash_long = 0, summarize = 1, global = 0, show_io = 0, dash_dag = 0, show_held = 0, dash_progress = 0;
static  int use_xml = 0;
static  int dash_autocluster = 0; // can be 0, or CondorQ::fetch_DefaultAutoCluster or CondorQ::fetch_GroupBy
static  int default_fetch_opts = CondorQ::fetch_MyJobs;
static  bool widescreen = false;
//static  int  use_old_code = true;
static  bool expert = false;
static  bool verbose = false; // note: this is !!NOT the same as -long !!!
static  int g_match_limit = -1;

static 	int malformed, running, idle, held, suspended, completed, removed;

static char dash_progress_alt_char = 0;
static  const char *jobads_file = NULL; // NULL, or points to jobads filename from argv
static  const char *machineads_file = NULL; // NULL, or points to machineads filename from argv
static  const char *userprios_file = NULL; // NULL, or points to userprios filename from argv
static  const char *userlog_file = NULL; // NULL, or points to userlog filename from argv
static  bool analyze_with_userprio = false;
static  const char * analyze_memory_usage = NULL;
static  bool dash_profile = false;
//static  bool analyze_dslots = false;
static  bool disable_user_print_files = false;


// Constraint on JobIDs for faster filtering
// a list and not set since we expect a very shallow depth
static std::vector<CondorID> constrID;

typedef std::map<int, std::vector<ClassAd *> > JobClusterMap;

static	CondorQ 	Q;
static	QueryResult result;

static	CondorQuery	scheddQuery(SCHEDD_AD);
static	CondorQuery submittorQuery(SUBMITTOR_AD);

static	ClassAdList	scheddList;

static  ClassAdAnalyzer analyzer;

static bool render_owner(std::string & out, AttrList*, Formatter &);
static bool render_dag_owner(std::string & out, AttrList*, Formatter &);
static bool render_batch_name(std::string & out, AttrList*, Formatter &);


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


/* counters for job matchmaking analysis */
typedef struct {
	int fReqConstraint;   // # of machines that don't match job Requirements
	int fOffConstraint;   // # of machines that match Job requirements, but refuse the job because of their own Requirements
	int fPreemptPrioCond; // match but are serving users with a better priority in the pool
	int fRankCond;        // match but reject the job for unknown reasons
	int fPreemptReqTest;  // match but will not currently preempt their existing job
	int fOffline;         // match but are currently offline
	int available;        // are available to run your job
	int totalMachines;
	int machinesRunningJobs; // number of machines serving other users
	int machinesRunningUsersJobs; 

	void clear() { memset((void*)this, 0, sizeof(*this)); }
} anaCounters;

static int set_print_mask_from_stream(AttrListPrintMask & prmask, const char * streamid, bool is_filename, StringList & attrs);
static void dump_print_mask(std::string & tmp);


static 	printmask_headerfooter_t customHeadFoot = STD_HEADFOOT;
static std::vector<GroupByKeyInfo> group_by_keys;
static  bool		cputime = false;
static	bool		current_run = false;
static 	bool		dash_globus = false;
static	bool		dash_grid = false;
static	bool		dash_run = false;
static	bool		dash_goodput = false;
static	bool		dash_dry_run = false;
static  const char * dry_run_file = NULL;
static  const char		*JOB_TIME = "RUN_TIME";
static	bool		querySchedds 	= false;
static	bool		querySubmittors = false;
static	char		constraint[4096];
static  const char *user_job_constraint = NULL; // just the constraint given by the user
static  const char *user_slot_constraint = NULL; // just the machine constraint given by the user
static  const char *global_schedd_constraint = NULL; // argument from -schedd-constraint
static  bool        single_machine = false;
static	DCCollector* pool = NULL; 
static	char		*scheddAddr;	// used by format_remote_host()
static CollectorList * Collectors = NULL;

// for run failure analysis
static  int			findSubmittor( const char * );
static	void 		setupAnalysis();
static 	int			fetchSubmittorPriosFromNegotiator(ExtArray<PrioEntry> & prios);
static	void		doJobRunAnalysis(ClassAd*, Daemon*, int details);
static const char	*doJobRunAnalysisToBuffer(ClassAd *request, anaCounters & ac, int details, bool noPrio, bool showMachines);
static	void		doSlotRunAnalysis( ClassAd*, JobClusterMap & clusters, Daemon*, int console_width);
static const char	*doSlotRunAnalysisToBuffer( ClassAd*, JobClusterMap & clusters, int console_width);
static	void		buildJobClusterMap(ClassAdList & jobs, const char * attr, JobClusterMap & clusters);
static	int			better_analyze = false;
static	bool		reverse_analyze = false;
static	bool		summarize_anal = false;
static  bool		summarize_with_owner = true;
static  int			cOwnersOnCmdline = 0;
static	const char	*fixSubmittorName( const char*, int );
static	ClassAdList startdAds;
static	ExprTree	*stdRankCondition;
static	ExprTree	*preemptRankCondition;
static	ExprTree	*preemptPrioCondition;
static	ExprTree	*preemptionReq;
static  ExtArray<PrioEntry> prioTable;

static	int			analyze_detail_level = 0; // one or more of detail_xxx enum values above.

const int SHORT_BUFFER_SIZE = 8192;
const int LONG_BUFFER_SIZE = 16384;	
char return_buff[LONG_BUFFER_SIZE * 100];


#ifdef USE_LATE_PROJECTION
static struct {
	StringList attrs;
	AttrListPrintMask mask;
} app;
#else
static bool usingPrintMask = false;
static AttrListPrintMask mask;
StringList attrs(NULL, "\n");; // The list of attrs we want, "" for all
#endif

bool g_stream_results = false;


static int longest_slot_machine_name = 0;
static int longest_slot_name = 0;

// Sort Machine ads by Machine name first, then by slotid, then then by slot name
int SlotSort(ClassAd *ad1, ClassAd *ad2, void *  /*data*/)
{
	std::string name1, name2;
	if ( ! ad1->LookupString(ATTR_MACHINE, name1))
		return 1;
	if ( ! ad2->LookupString(ATTR_MACHINE, name2))
		return 0;
	
	// opportunisticly capture the longest slot machine name.
	longest_slot_machine_name = MAX(longest_slot_machine_name, (int)name1.size());
	longest_slot_machine_name = MAX(longest_slot_machine_name, (int)name2.size());

	int cmp = name1.compare(name2);
	if (cmp < 0) return 1;
	if (cmp > 0) return 0;

	int slot1=0, slot2=0;
	ad1->LookupInteger(ATTR_SLOT_ID, slot1);
	ad2->LookupInteger(ATTR_SLOT_ID, slot2);
	if (slot1 < slot2) return 1;
	if (slot1 > slot2) return 0;

	// PRAGMA_REMIND("TJ: revisit this code to compare dynamic slot id once that exists");
	// for now, the only way to get the dynamic slot id is to use the slot name.
	name1.clear(); name2.clear();
	if ( ! ad1->LookupString(ATTR_NAME, name1))
		return 1;
	if ( ! ad2->LookupString(ATTR_NAME, name2))
		return 0;
	// opportunisticly capture the longest slot name.
	longest_slot_name = MAX(longest_slot_name, (int)name1.size());
	longest_slot_name = MAX(longest_slot_name, (int)name2.size());
	cmp = name1.compare(name2);
	if (cmp < 0) return 1;
	return 0;
}

class CondorQClassAdFileParseHelper : public compat_classad::ClassAdFileParseHelper
{
 public:
	CondorQClassAdFileParseHelper() : is_schedd(false), is_submitter(false) {}
	virtual int PreParse(std::string & line, ClassAd & ad, FILE* file);
	virtual int OnParseError(std::string & line, ClassAd & ad, FILE* file);
	std::string schedd_name;
	std::string schedd_addr;
	bool is_schedd;
	bool is_submitter;
};

// this method is called before each line is parsed. 
// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
int CondorQClassAdFileParseHelper::PreParse(std::string & line, ClassAd & /*ad*/, FILE* /*file*/)
{
	// treat blank lines as delimiters.
	if (line.size() <= 0) {
		return 2; // end of classad.
	}

	// standard delimitors are ... and ***
	if (starts_with(line,"\n") || starts_with(line,"...") || starts_with(line,"***")) {
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
			if (ix1 != string::npos) {
				size_t ix2 = schedd_name.find_first_not_of(": \t\n", ix1);
				if (ix2 != string::npos) {
					schedd_addr = schedd_name.substr(ix2);
					ix2 = schedd_addr.find_first_of(" \t\n");
					if (ix2 != string::npos) {
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
		if (line[ix] == '#' || line[ix] == '\n')
			return 0; // skip this line, but don't stop parsing.
		if (line[ix] != ' ' && line[ix] != '\t')
			break;
	}
	return 1; // parse this line
}

// this method is called when the parser encounters an error
// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
int CondorQClassAdFileParseHelper::OnParseError(std::string & line, ClassAd & ad, FILE* file)
{
	// when we get a parse error, skip ahead to the start of the next classad.
	int ee = this->PreParse(line, ad, file);
	while (1 == ee) {
		if ( ! readLine(line, file, false) || feof(file)) {
			ee = 2;
			break;
		}
		ee = this->PreParse(line, ad, file);
	}
	return ee;
}

static void
profile_print(size_t & cbBefore, double & tmBefore, int cAds, bool fCacheStats=true)
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

#ifdef HAVE_EXT_POSTGRESQL
/* this function for checking whether database can be used for querying in local machine */
static bool checkDBconfig() {
	char *tmp;

	if (param_boolean("QUILL_ENABLED", false) == false) {
		return false;
	};

	tmp = param("QUILL_NAME");
	if (!tmp) {
		return false;
	}
	free(tmp);

	tmp = param("QUILL_DB_IP_ADDR");
	if (!tmp) {
		return false;
	}
	free(tmp);

	tmp = param("QUILL_DB_NAME");
	if (!tmp) {
		return false;
	}
	free(tmp);

	tmp = param("QUILL_DB_QUERY_PASSWORD");
	if (!tmp) {
		return false;
	}
	free(tmp);

	return true;
}
#endif /* HAVE_EXT_POSTGRESQL */

int main (int argc, char **argv)
{
	ClassAd		*ad;
	bool		first;
	char		*scheddName=NULL;
	char		scheddMachine[64];
	int		useFastScheddQuery = 0;
	char		*tmp;
	bool        useDB; /* Is there a database to query for a schedd */
	int         retval = 0;

	Collectors = NULL;

	// load up configuration file
	myDistro->Init( argc, argv );
	config();
	dprintf_config_tool_on_error(0);
	dprintf_OnExitDumpOnErrorBuffer(stderr);
	//classad::ClassAdSetExpressionCaching( param_boolean( "ENABLE_CLASSAD_CACHING", false ) );

#ifdef HAVE_EXT_POSTGRESQL
		/* by default check the configuration for local database */
	useDB = checkDBconfig();
#else 
	useDB = FALSE;
	if (useDB) {} /* Done to suppress set-but-not-used warnings */
#endif /* HAVE_EXT_POSTGRESQL */

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	if (param_boolean("CONDOR_Q_ONLY_MY_JOBS", true)) {
		default_fetch_opts |= CondorQ::fetch_MyJobs;
	} else {
		default_fetch_opts &= ~CondorQ::fetch_MyJobs;
	}

	// process arguments
	processCommandLineArguments (argc, argv);

	// Since we are assuming that we want to talk to a DB in the normal
	// case, this will ensure that we don't try to when loading the job queue
	// classads from file.
	if ((jobads_file != NULL) || (userlog_file != NULL)) {
		useDB = FALSE;
	}

	if (Collectors == NULL) {
		Collectors = CollectorList::create();
	}

	// check if analysis is required
	if( better_analyze ) {
		setupAnalysis();
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

			scheddAddr = strdup(schedd.addr());
			if( (tmp = schedd.name()) ) {
				scheddName = strdup(tmp);
				Q.addSchedd(scheddName);
			} else {
				scheddName = strdup("Unknown");
			}
			if( (tmp = schedd.fullHostname()) ) {
				sprintf( scheddMachine, "%s", tmp );
			} else {
				sprintf( scheddMachine, "Unknown" );
			}
			if (schedd.version()) {
				CondorVersionInfo v(schedd.version());
				useFastScheddQuery = v.built_since_version(6,9,3) ? 1 : 0;
				if (v.built_since_version(8, 3, 3)) {
					useFastScheddQuery = 2;
				}
			}
			
#ifdef HAVE_EXT_POSTGRESQL
			if ( directDBquery ) {
				/* perform direct DB query if indicated and exit */

					/* check if database is available */
				if (!useDB) {
					printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
					fprintf(stderr, "Database query not supported on schedd: %s\n", scheddName);
				}

				exec_db_query(NULL, NULL, NULL, NULL);
				exit(EXIT_SUCCESS);
			} 
#endif /* HAVE_EXT_POSTGRESQL */

			/* .. not a direct db query, so just happily continue ... */

#ifdef USE_LATE_PROJECTION
#else
			init_output_mask();
#endif

			/* When an installation has database parameters configured, 
				it means there is quill daemon. If database
				is not accessible, we fail over to
				quill daemon, and if quill daemon
				is not available, we fail over the
				schedd daemon */
			switch(direct)
			{
				case DIRECT_ALL:
					/* always try the failover sequence */

					/* FALL THROUGH */

#ifdef HAVE_EXT_POSTGRESQL
				case DIRECT_RDBMS:
					if (useDB) {

						/* ask the database for the queue */
						retval = show_db_queue(NULL, NULL, NULL, NULL);
						if (retval)
						{
							/* if the queue was retrieved, then I am done */
							exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
						}
						
						fprintf( stderr, 
							"-- Database not reachable or down.\n");

						if (direct == DIRECT_RDBMS) {
							fprintf(stderr,
								"\t- Not failing over due to -direct\n");
							exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
						} 

						fprintf(stderr,
							"\t- Failing over to the quill daemon --\n");

						/* Hmm... couldn't find the database, so try the 
							quilld */
					} else {
						if (direct == DIRECT_RDBMS) {
							fprintf(stderr, 
								"-- Direct query to rdbms on behalf of\n"
								"\tschedd %s(%s) failed.\n"
								"\t- Schedd doesn't appear to be using "
								"quill.\n",
								scheddName, scheddAddr);
							fprintf(stderr,
								"\t- Not failing over due to -direct\n");
							exit(EXIT_FAILURE);
						}
					}

					/* FALL THROUGH */

				case DIRECT_QUILLD:
					if (useDB) {

						Daemon quill( DT_QUILL, 0, 0 );
						char tmp_char[8];
						strcpy(tmp_char, "Unknown");

						/* query the quill daemon */
						if (quill.locate() &&
							(retval = show_schedd_queue(quill.addr(),
								quill.name() ? quill.name() : "Unknown",
								quill.fullHostname() ? quill.fullHostname() : "Unknown",
								false))
							)
						{
							/* if the queue was retrieved, then I am done */
							exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
						}

						fprintf( stderr,
							"-- Quill daemon at %s(%s)\n"
							"\tassociated with schedd %s(%s)\n"
							"\tis not reachable or can't talk to rdbms.\n",
							quill.name(), quill.addr(),
							scheddName, scheddAddr );

						if (direct == DIRECT_QUILLD) {
							fprintf(stderr,
								"\t- Not failing over due to -direct\n");
							exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
						}

						fprintf(stderr,
							"\t- Failing over to the schedd %s(%s).\n", 
							scheddName, scheddAddr);

					} else {
						if (direct == DIRECT_QUILLD) {
							fprintf(stderr,
								"-- Direct query to quilld associated with\n"
								"\tschedd %s(%s) failed.\n"
								"\t- Schedd doesn't appear to be using "
								"quill.\n",
								scheddName, scheddAddr);

							fprintf(stderr,
								"\t- Not failing over due to use of -direct\n");

							exit(EXIT_FAILURE);
						}
					}

#endif /* HAVE_EXT_POSTGRESQL */
				case DIRECT_SCHEDD:
					retval = show_schedd_queue(scheddAddr, scheddName, scheddMachine, useFastScheddQuery);
			
					/* Hopefully I got the queue from the schedd... */
					exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
					break;

				case DIRECT_UNKNOWN:
				default:
					fprintf( stderr,
						"-- Cannot determine any location for queue "
						"using option -direct!\n"
						"\t- This is an internal error and should be "
						"reported to condor-admin@cs.wisc.edu." );
					exit(EXIT_FAILURE);
					break;
			}
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
		sprintf( constraint, "%s > 0 || %s > 0 || %s > 0 || %s > 0 || %s > 0", 
			ATTR_TOTAL_RUNNING_JOBS, ATTR_TOTAL_IDLE_JOBS,
			ATTR_TOTAL_HELD_JOBS, ATTR_TOTAL_REMOVED_JOBS,
			ATTR_TOTAL_JOB_ADS );
		result = scheddQuery.addANDConstraint( constraint );
		if( result != Q_OK ) {
			fprintf( stderr, "Error: Couldn't add schedd-constraint %s\n", constraint);
			exit( 1 );
		}
	}

	// get the list of ads from the collector
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

		/*if(querySchedds && scheddList.MyLength() == 0) {
		  result = Collectors->query(quillQuery, quillList);
		}*/

	first = true;
	// get queue from each ScheddIpAddr in ad
	scheddList.Open();
	while ((ad = scheddList.Next()))
	{
		/* default to true for remotely queryable */
#ifdef HAVE_EXT_POSTGRESQL
		int flag=1;
		freeConnectionStrings();
#endif
		useDB = FALSE;
		if ( ! (ad->LookupString(ATTR_SCHEDD_IP_ADDR, &scheddAddr)  &&
				ad->LookupString(ATTR_NAME, &scheddName) &&
				ad->LookupString(ATTR_MACHINE, scheddMachine, sizeof(scheddMachine))
				)
			)
		{
			/* something is wrong with this schedd/quill ad, try the next one */
			continue;
		}

#ifdef HAVE_EXT_POSTGRESQL
		std::string daemonAdName;
			// get the address of the database
		if (ad->LookupString(ATTR_QUILL_DB_IP_ADDR, sdbIpAddr) &&
			ad->LookupString(ATTR_QUILL_NAME, squillName) &&
			ad->LookupString(ATTR_QUILL_DB_NAME, sdbName) && 
			ad->LookupString(ATTR_QUILL_DB_QUERY_PASSWORD, squeryPassword) &&
			(!ad->LookupBool(ATTR_QUILL_IS_REMOTELY_QUERYABLE,flag) || flag)) {

			dbIpAddr = sdbIpAddr.c_str();
			quillName = squillName.c_str();
			dbName = sdbName.c_str();
			queryPassword = squeryPassword.c_str();

			/* If the quill information is available, try to use it first */
			useDB = TRUE;
	    	if (ad->LookupString(ATTR_SCHEDD_NAME, daemonAdName)) {
				Q.addSchedd(daemonAdName.c_str());
			} else {
				Q.addSchedd(scheddName);
			}

				/* get the quill info for fail-over processing */
			ASSERT(ad->LookupString(ATTR_MACHINE, squillMachine));
			quillMachine = squillMachine.c_str();
		}
#endif 

		first = false;

#ifdef HAVE_EXT_POSTGRESQL
			/* check if direct DB query is indicated */
		if ( directDBquery ) {				
			if (!useDB) {
				printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
				fprintf(stderr, "Database query not supported on schedd: %s\n",
						scheddName);
				continue;
			}
			
			exec_db_query(quillName, dbIpAddr, dbName, queryPassword);

			/* done processing the ad, so get the next one */
			continue;

		}
#endif /* HAVE_EXT_POSTGRESQL */

#ifdef USE_LATE_PROJECTION
#else
		init_output_mask();
#endif

		/* When an installation has database parameters configured, it means 
		   there is quill daemon. If database is not accessible, we fail
		   over to quill daemon, and if quill daemon is not available, 
		   we fail over the schedd daemon */			
		switch(direct)
		{
			case DIRECT_ALL:
				/* FALL THROUGH */
#ifdef HAVE_EXT_POSTGRESQL
			case DIRECT_RDBMS:
				if (useDB) {
					retval = show_db_queue(quillName, dbIpAddr, dbName, queryPassword);
					if (retval)
					{
						/* processed correctly, so do the next ad */
						continue;
					}

					fprintf( stderr, "-- Database server %s\n"
							"\tbeing used by the quill daemon %s\n"
							"\tassociated with schedd %s(%s)\n"
							"\tis not reachable or down.\n",
							dbIpAddr, quillName, scheddName, scheddAddr);

					if (direct == DIRECT_RDBMS) {
						fprintf(stderr, 
							"\t- Not failing over due to -direct\n");
						continue;
					} 

					fprintf(stderr, 
						"\t- Failing over to the quill daemon at %s--\n", 
						quillName);

				} else {
					if (direct == DIRECT_RDBMS) {
						fprintf(stderr, 
							"-- Direct query to rdbms on behalf of\n"
							"\tschedd %s(%s) failed.\n"
							"\t- Schedd doesn't appear to be using quill.\n",
							scheddName, scheddAddr);
						fprintf(stderr,
							"\t- Not failing over due to -direct\n");
						continue;
					}
				}

				/* FALL THROUGH */

			case DIRECT_QUILLD:
				if (useDB) {
					QueryResult result2 = getQuillAddrFromCollector(quillName, quillAddr);

					/* if quillAddr is NULL, then while the collector's 
						schedd's ad had a quill name in it, the collector
						didn't have a quill ad by that name. */

					if((result2 == Q_OK) && !quillAddr.empty() &&
					   (retval = show_schedd_queue(quillAddr.c_str(), quillName, quillMachine, 0))
					   )
					{
						/* processed correctly, so do the next ad */
						continue;
					}

					/* NOTE: it is not impossible that quillAddr could be
						NULL if the quill name is specified in a schedd ad
						but the collector has no record of such quill ad by
						that name. So we deal with that mess here in the 
						debugging output. */

					fprintf( stderr,
						"-- Quill daemon %s(%s)\n"
						"\tassociated with schedd %s(%s)\n"
						"\tis not reachable or can't talk to rdbms.\n",
						quillName, quillAddr.empty()?"<\?\?\?>":quillAddr.c_str(),
						scheddName, scheddAddr);

					if (quillAddr.empty()) {
						fprintf(stderr,
							"\t- Possible misconfiguration of quill\n"
							"\tassociated with this schedd since associated\n"
							"\tquilld ad is missing and was expected to be\n"
							"\tfound in the collector.\n");
					}

					if (direct == DIRECT_QUILLD) {
						fprintf(stderr,
							"\t- Not failing over due to use of -direct\n");
						continue;
					}

					fprintf(stderr,
						"\t- Failing over to the schedd at %s(%s) --\n",
						scheddName, scheddAddr);

				} else {
					if (direct == DIRECT_QUILLD) {
						fprintf(stderr,
							"-- Direct query to quilld associated with\n"
							"\tschedd %s(%s) failed.\n"
							"\t- Schedd doesn't appear to be using quill.\n",
							scheddName, scheddAddr);
						printf("\t- Not failing over due to use of -direct\n");
						continue;
					}
				}

				/* FALL THROUGH */

#endif /* HAVE_EXT_POSTGRESQL */

			case DIRECT_SCHEDD:
				/* database not configured or could not be reached,
					query the schedd daemon directly */
				{
				MyString scheddVersion;
				ad->LookupString(ATTR_VERSION, scheddVersion);
				CondorVersionInfo v(scheddVersion.Value());
				useFastScheddQuery = v.built_since_version(6,9,3) ? (v.built_since_version(8, 3, 3) ? 2 : 1) : 0;
				retval = show_schedd_queue(scheddAddr, scheddName, scheddMachine, useFastScheddQuery);
				}

				break;

			case DIRECT_UNKNOWN:
			default:
				fprintf( stderr,
					"-- Cannot determine any location for queue "
					"using option -direct!\n"
					"\t- This is an internal error and should be "
					"reported to condor-admin@cs.wisc.edu." );
				exit(EXIT_FAILURE);
				break;
		}
	}

	// close list
	scheddList.Close();

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

#ifdef HAVE_EXT_POSTGRESQL
	quillCleanupOnExit(exit_code);
#endif

	// do this to make Valgrind happy.
	startdAds.Clear();
	scheddList.Clear();

	return exit_code;
}

// append all variable references made by expression to references list
static bool
GetAllReferencesFromClassAdExpr(char const *expression,StringList &references)
{
	ClassAd ad;
	return ad.GetExprReferences(expression,NULL,&references);
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

#ifdef USE_LATE_PROJECTION
// this enum encodes the user choice of the various reports that condor_q can show
// The first few are mutually exclusive, the last are flags
enum {
	QDO_NotSet=0,
	QDO_JobNormal,
	QDO_JobRuntime,
	QDO_JobGoodput,
	QDO_JobGlobusInfo,
	QDO_JobGridInfo,
	QDO_JobHold,
	QDO_JobIO,
	QDO_DAG,
	QDO_Totals,
	QDO_Progress,

	QDO_AutoclusterNormal, // Print typical autocluster attributes

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
//void initProjection(StringList & proj, int qdo_mode);
#else
static bool dash_totals = false;
#endif

static void 
processCommandLineArguments (int argc, char *argv[])
{
	int i;
	char *at, *daemonname;
	const char * pcolon;

#ifdef USE_LATE_PROJECTION
	int qdo_mode = QDO_NotSet;
#else
	bool custom_attributes = false;
	attrs.initializeFromString(
		"ClusterId\nProcId\nOwner\nJobStatus\nQDate\nRemoteUserCpu\nServerTime\nShadowBday\n"
		"RemoteWallClockTime\nJobPrio\nImageSize\nCmd\nArgs\nArguments\n"
		"JobDescription\nMATCH_EXP_JobDescription\nTransferringInput\nTransferringOutput\nTransferQueued");
#endif

	for (i = 1; i < argc; i++)
	{
		// no dash means this arg is a cluster/proc, proc, or owner
		if( *argv[i] != '-' ) {
			int cluster, proc;
			const char * pend;
			if (StrIsProcId(argv[i], cluster, proc, &pend) && *pend == 0) {
				constrID.push_back(CondorID(cluster,proc,-1));
			}
			else {
				++cOwnersOnCmdline;
				if( Q.add( CQ_OWNER, argv[i] ) != Q_OK ) {
					// this error doesn't seem very helpful... can't we say more?
					fprintf( stderr, "Error: Argument %d (%s) must be a jobid or user\n", i, argv[i] );
					exit( 1 );
				}
				// dont default to 'my jobs' if an owner was specified.
				default_fetch_opts &= ~CondorQ::fetch_MyJobs;
			}

			continue;
		}

		// the argument began with a '-', so use only the part after
		// the '-' for prefix matches
		const char* dash_arg = argv[i];

		if (is_dash_arg_colon_prefix (dash_arg, "wide", &pcolon, 1)) {
			widescreen = true;
			if (pcolon) {
				testing_width = atoi(++pcolon);
#ifdef USE_LATE_PROJECTION
				// TODO: fix this...
				widescreen = false;
#else
				if ( ! mask.IsEmpty()) mask.SetOverallWidth(getDisplayWidth()-1);
				if (testing_width <= 80) widescreen = false;
#endif
			}
			continue;
		}
		if (is_dash_arg_prefix (dash_arg, "long", 1)) {
			dash_long = 1;
			summarize = 0;
		} 
		else
		if (is_dash_arg_prefix (dash_arg, "xml", 3)) {
			use_xml = 1;
			dash_long = 1;
			summarize = 0;
			customHeadFoot = HF_BARE;
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
			Collectors->append ( new DCCollector( *pool ) );
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
			sprintf (constraint, "%s == \"%s\"", ATTR_NAME, daemonname);
			scheddQuery.addORConstraint (constraint);
			scheddQuery.setLocationLookup(daemonname);
			Q.addSchedd(daemonname);

#ifdef HAVE_EXT_POSTGRESQL
			sprintf (constraint, "%s == \"%s\"", ATTR_QUILL_NAME, daemonname);
			scheddQuery.addORConstraint (constraint);
#endif

			delete [] daemonname;
			i++;
			querySchedds = true;
		} 
		else
		if (is_dash_arg_prefix (dash_arg, "direct", 1)) {
			/* check for one more argument */
			if (argc <= i+1) {
				fprintf( stderr, 
					"Error: -direct requires ["
					#ifdef HAVE_EXT_POSTGRESQL
						"rdbms | quilld | "
					#endif
						"schedd]\n" );
				exit(EXIT_FAILURE);
			}
			direct = process_direct_argument(argv[i+1]);
			i++;
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
			if ((at = strchr(argv[i],'@'))) {
				// is the name already qualified with a UID_DOMAIN?
				sprintf (constraint, "%s == \"%s\"", ATTR_NAME, argv[i]);
				*at = '\0';
			} else {
				// no ... add UID_DOMAIN
				char *uid_domain = param( "UID_DOMAIN" );
				if (uid_domain == NULL)
				{
					EXCEPT ("No 'UID_DOMAIN' found in config file");
				}
				sprintf (constraint, "%s == \"%s@%s\"", ATTR_NAME, argv[i], 
							uid_domain);
				free (uid_domain);
			}
			// dont default to 'my jobs'
			default_fetch_opts &= ~CondorQ::fetch_MyJobs;

			// insert the constraints
			submittorQuery.addORConstraint (constraint);

			{
				char *ownerName = argv[i];
				// ensure that the "nice-user" prefix isn't inserted as part
				// of the job ad constraint
				if( strstr( argv[i] , NiceUserName ) == argv[i] ) {
					ownerName = argv[i]+strlen(NiceUserName)+1;
				}
				// ensure that the group prefix isn't inserted as part
				// of the job ad constraint.
				char *groups = param("GROUP_NAMES");
				if ( groups && ownerName ) {
					StringList groupList(groups);
					char *dotptr = strchr(ownerName,'.');
					if ( dotptr ) {
						*dotptr = '\0';
						if ( groupList.contains_anycase(ownerName) ) {
							// this name starts with a group prefix.
							// strip it off.
							ownerName = dotptr + 1;	// add one for the '.'
						}
						*dotptr = '.';
					}
				}
				if ( groups ) free(groups);
				if (Q.add (CQ_OWNER, ownerName) != Q_OK) {
					fprintf (stderr, "Error:  Argument %d (%s)\n", i, argv[i]);
					exit (1);
				}
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
			sprintf(constraint, "%s == \"%s\"", ATTR_SCHEDD_IP_ADDR, argv[i+1]);
			scheddQuery.addORConstraint(constraint);
			i++;
			querySchedds = true;
		} 
		else
		if (is_dash_arg_colon_prefix (dash_arg, "autocluster", &pcolon, 2)) {
			dash_autocluster = CondorQ::fetch_DefaultAutoCluster;
		}
		else
		if (is_dash_arg_colon_prefix (dash_arg, "group-by", &pcolon, 2)) {
			dash_autocluster = CondorQ::fetch_GroupBy;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "attributes", 2)) {
			if( argc <= i+1 ) {
				fprintf( stderr, "Error: -attributes requires a list of attributes to show\n" );
				exit( 1 );
			}
#ifdef USE_LATE_PROJECTION
			qdo_mode |= QDO_Attribs;
			StringTokenIterator more_attrs(argv[i+1]);
			const char * s;
			while ( (s = more_attrs.next()) ) {
				app.attrs.append(s);
			}
#else
			if( !custom_attributes ) {
				custom_attributes = true;
				attrs.clearAll();
			}
			StringList more_attrs(argv[i+1],",");
			char const *s;
			more_attrs.rewind();
			while( (s=more_attrs.next()) ) {
				attrs.append(s);
			}
#endif
			i++;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "format", 1)) {
				// make sure we have at least two more arguments
			if( argc <= i+2 ) {
				fprintf( stderr, "Error: -format requires format and attribute parameters\n" );
				exit( 1 );
			}
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_Format;
			app.mask.registerFormat( argv[i+1], argv[i+2] );
			if ( ! dash_autocluster) {
				app.attrs.initializeFromString("ClusterId ProcId"); // this is needed to prevent some DAG code from faulting.
			}
			GetAllReferencesFromClassAdExpr(argv[i+2], app.attrs);
#else
			if( !custom_attributes ) {
				custom_attributes = true;
				attrs.clearAll();
				if ( ! dash_autocluster) {
					attrs.initializeFromString("ClusterId\nProcId"); // this is needed to prevent some DAG code from faulting.
				}
			}
			GetAllReferencesFromClassAdExpr(argv[i+2],attrs);
			mask.registerFormat( argv[i+1], argv[i+2] );
			usingPrintMask = true;
#endif
			summarize = 0;
			customHeadFoot = HF_BARE;
			i+=2;
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "autoformat", &pcolon, 5) ||
			is_dash_arg_colon_prefix(dash_arg, "af", &pcolon, 2)) {
				// make sure we have at least one more argument
			if ( (i+1 >= argc)  || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: -autoformat requires at last one attribute parameter\n" );
				exit( 1 );
			}
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_AutoFormat;
			if ( ! dash_autocluster) {
				app.attrs.initializeFromString("ClusterId ProcId"); // this is needed to prevent some DAG code from faulting.
			}
			AttrListPrintMask & prmask = app.mask;
			StringList & attrs = app.attrs;
#else
			if( !custom_attributes ) {
				custom_attributes = true;
				attrs.clearAll();
				if ( ! dash_autocluster) {
					attrs.initializeFromString("ClusterId\nProcId"); // this is needed to prevent some DAG code from faulting.
				}
			}
			usingPrintMask = true;
			AttrListPrintMask & prmask = mask;
#endif
			bool flabel = false;
			bool fCapV  = false;
			bool fheadings = false;
			bool fJobId = false;
			bool fRaw = false;
			const char * prowpre = NULL;
			const char * pcolpre = " ";
			const char * pcolsux = NULL;
			if (pcolon) {
				++pcolon;
				while (*pcolon) {
					switch (*pcolon)
					{
						case ',': pcolsux = ","; break;
						case 'n': pcolsux = "\n"; break;
						case 'g': pcolpre = NULL; prowpre = "\n"; break;
						case 't': pcolpre = "\t"; break;
						case 'l': flabel = true; break;
						case 'V': fCapV = true; break;
						case 'r': case 'o': fRaw = true; break;
						case 'h': fheadings = true; break;
						case 'j': fJobId = true; break;
					}
					++pcolon;
				}
			}
			if (fJobId) {
				if (fheadings || prmask.has_headings()) {
					prmask.set_heading(" ID");
					prmask.registerFormat (flabel ? "ID = %4d." : "%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
					prmask.set_heading(" ");
					prmask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix, ATTR_PROC_ID);
				} else {
					prmask.registerFormat (flabel ? "ID = %d." : "%d.", 0, FormatOptionNoSuffix, ATTR_CLUSTER_ID);
					prmask.registerFormat ("%d", 0, FormatOptionNoPrefix, ATTR_PROC_ID);
				}
			}
			// process all arguments that don't begin with "-" as part
			// of autoformat.
			while (i+1 < argc && *(argv[i+1]) != '-') {
				++i;
				GetAllReferencesFromClassAdExpr(argv[i], attrs);
				MyString lbl = "";
				int wid = 0;
				int opts = FormatOptionNoTruncate;
				if (fheadings || prmask.has_headings()) {
					const char * hd = fheadings ? argv[i] : "(expr)";
					wid = 0 - (int)strlen(hd);
					opts = FormatOptionAutoWidth | FormatOptionNoTruncate; 
					prmask.set_heading(hd);
				}
				else if (flabel) { lbl.formatstr("%s = ", argv[i]); wid = 0; opts = 0; }
				lbl += fRaw ? "%r" : (fCapV ? "%V" : "%v");
				prmask.registerFormat(lbl.Value(), wid, opts, argv[i]);
			}
			prmask.SetAutoSep(prowpre, pcolpre, pcolsux, "\n");
			summarize = 0;
			customHeadFoot = HF_BARE;
			if (fheadings) { customHeadFoot = (printmask_headerfooter_t)(customHeadFoot & ~HF_NOHEADER); }
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
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_PrintFormat | QDO_Custom;
			if ( ! widescreen) app.mask.SetOverallWidth(getDisplayWidth()-1);
			++i;
			if (set_print_mask_from_stream(app.mask, argv[i], true, app.attrs) < 0) {
#else
			if ( ! widescreen) mask.SetOverallWidth(getDisplayWidth()-1);
			if( !custom_attributes ) {
				custom_attributes = true;
				attrs.clearAll();
			}
			usingPrintMask = true;
			++i;
			if (set_print_mask_from_stream(mask, argv[i], true, attrs) < 0) {
#endif
				fprintf(stderr, "Error: invalid select file %s\n", argv[i]);
				exit (1);
			}
		}
		else
		if (is_dash_arg_prefix (dash_arg, "global", 1)) {
			global = 1;
		}
		else
		if (is_dash_arg_prefix (dash_arg, "allusers", 2) || is_dash_arg_prefix (dash_arg, "all-users", 2)) {
			default_fetch_opts &= ~CondorQ::fetch_MyJobs;
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
				if (is_arg_prefix(argv[i], "universe", 3)) {
					other |= usage_Universe;
				} else if (is_arg_prefix(argv[i], "state", 2) || is_arg_prefix(argv[i], "status", 2)) {
					other |= usage_JobStatus;
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
						summarize_anal = true;
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
					} else if (is_arg_prefix(popt, "memory",3)) {
						analyze_memory_usage = opts.next();
						if (analyze_memory_usage) { analyze_memory_usage = strdup(analyze_memory_usage); }
						else { analyze_memory_usage = ATTR_REQUIREMENTS; }
					//} else if (is_arg_prefix(popt, "dslots",2)) {
					//	analyze_dslots = true;
					} else {
						analyze_detail_level = parse_analyze_detail(popt, analyze_detail_level);
					}
				}
			}
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_Analyze;
#else
			attrs.clearAll();
#endif
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "reverse", &pcolon, 3)) {
			reverse_analyze = true; // modify analyze to be reverse analysis
			if (pcolon) { 
				analyze_detail_level |= parse_analyze_detail(++pcolon, analyze_detail_level);
			}
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "reverse-analyze", &pcolon, 10)) {
			reverse_analyze = true; // modify analyze to be reverse analysis
			better_analyze = true;	// enable analysis
			analyze_with_userprio = false;
			if (pcolon) { 
				analyze_detail_level |= parse_analyze_detail(++pcolon, analyze_detail_level);
			}
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_Analyze;
#else
			attrs.clearAll();
#endif
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "dry-run", &pcolon, 3)) {
			dash_dry_run = true;
			if (pcolon && pcolon[1]) { dry_run_file = ++pcolon; }
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
			if( ! dash_dag && ! dash_progress) {
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_JobRuntime;
#else
				attrs.append( ATTR_REMOTE_HOST );
				attrs.append( ATTR_JOB_UNIVERSE );
				attrs.append( ATTR_EC2_REMOTE_VM_NAME ); // for displaying HOST(s) in EC2
#endif
			}
		}
		else
		if (is_dash_arg_prefix(dash_arg, "hold", 2) || is_dash_arg_prefix(dash_arg, "held", 2)) {
			Q.add (CQ_STATUS, HELD);
			show_held = true;
			widescreen = true;
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_JobHold;
#else
			attrs.append( ATTR_ENTERED_CURRENT_STATUS );
			attrs.append( ATTR_HOLD_REASON );
#endif
		}
		else
		if (is_dash_arg_prefix(dash_arg, "goodput", 2)) {
			// goodput and show_io require the same column
			// real-estate, so they're mutually exclusive
			dash_goodput = true;
			show_io = false;
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_JobGoodput;
#else
			attrs.append( ATTR_JOB_COMMITTED_TIME );
			attrs.append( ATTR_SHADOW_BIRTHDATE );
			attrs.append( ATTR_LAST_CKPT_TIME );
			attrs.append( ATTR_JOB_REMOTE_WALL_CLOCK );
#endif
		}
		else
		if (is_dash_arg_prefix(dash_arg, "cputime", 2)) {
			cputime = true;
			JOB_TIME = "CPU_TIME";
#ifdef USE_LATE_PROJECTION
			qdo_mode |= QDO_Cputime;
#else
		 	attrs.append( ATTR_JOB_REMOTE_USER_CPU );
#endif
		}
		else
		if (is_dash_arg_prefix(dash_arg, "currentrun", 2)) {
			current_run = true;
		}
		else
		if (is_dash_arg_prefix(dash_arg, "grid", 2 )) {
			// grid is a superset of globus, so we can't do grid if globus has been specifed
			if ( ! dash_globus) {
				dash_grid = true;
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_JobGridInfo;
#else
				attrs.append( ATTR_GRID_JOB_ID );
				attrs.append( ATTR_GRID_RESOURCE );
				attrs.append( ATTR_GRID_JOB_STATUS );
				attrs.append( ATTR_GLOBUS_STATUS );
    			attrs.append( ATTR_EC2_REMOTE_VM_NAME );
#endif
				Q.addAND( "JobUniverse == 9" );
			}
		}
		else
		if (is_dash_arg_prefix(dash_arg, "globus", 5 )) {
			Q.addAND( "GlobusStatus =!= UNDEFINED" );
			dash_globus = true;
			if (dash_grid) {
				dash_grid = false;
			} else {
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_JobGlobusInfo;
#else
				attrs.append( ATTR_GLOBUS_STATUS );
				attrs.append( ATTR_GRID_RESOURCE );
				attrs.append( ATTR_GRID_JOB_STATUS );
				attrs.append( ATTR_GRID_JOB_ID );
#endif
			}
		}
		else
		if (is_dash_arg_colon_prefix(dash_arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
			if (pcolon && pcolon[1]) {
				set_debug_flags( ++pcolon, 0 );
			}
		}
		else
		if (is_dash_arg_prefix(dash_arg, "io", 2)) {
			// goodput and show_io require the same column
			// real-estate, so they're mutually exclusive
			show_io = true;
			dash_goodput = false;
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_JobIO;
#else
			attrs.append(ATTR_NUM_JOB_STARTS);
			attrs.append(ATTR_NUM_CKPTS_RAW);
			attrs.append(ATTR_FILE_READ_BYTES);
			attrs.append(ATTR_BYTES_RECVD);
			attrs.append(ATTR_FILE_WRITE_BYTES);
			attrs.append(ATTR_BYTES_SENT);
			attrs.append(ATTR_JOB_REMOTE_WALL_CLOCK);
			attrs.append(ATTR_FILE_SEEK_COUNT);
			attrs.append(ATTR_BUFFER_SIZE);
			attrs.append(ATTR_BUFFER_BLOCK_SIZE);
			attrs.append(ATTR_TRANSFERRING_INPUT);
			attrs.append(ATTR_TRANSFERRING_OUTPUT);
			attrs.append(ATTR_TRANSFER_QUEUED);
#endif
		}
		else if (is_dash_arg_prefix(dash_arg, "dag", 2)) {
			dash_dag = true;
#ifdef USE_LATE_PROJECTION
			if ( ! dash_progress) {
				qdo_mode = QDO_DAG;
			}
#else
			attrs.clearAll();
#endif
			if( g_stream_results  ) {
				fprintf( stderr, "-stream-results and -dag are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else if (is_dash_arg_colon_prefix(dash_arg, "batch", &pcolon, 2) ||
			     is_dash_arg_colon_prefix(dash_arg, "progress", &pcolon, 3)) {
			dash_progress = true;
			if (pcolon) {
				StringList opts(++pcolon, ",:");
				opts.rewind();
				while (const char * popt = opts.next()) {
					char ch = *popt;
					if (ch >= '0' && ch <= '9') {
						dash_progress = atoi(popt);
					} else if (strchr("b?*.-_#z", ch)) {
						dash_progress_alt_char = ch;
					}
				}
			}
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_Progress;
#else
			error. cannot do this..
#endif
			if( g_stream_results  ) {
				fprintf( stderr, "-stream-results and -batch are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else if (is_dash_arg_prefix(dash_arg, "nobatch", 3)) {
			dash_progress = false;
			if ((qdo_mode & QDO_BaseMask) == QDO_Progress) {
				qdo_mode = (qdo_mode & ~QDO_BaseMask) | QDO_NotSet;
			}
		}
		else if (is_dash_arg_prefix(dash_arg, "totals", 3)) {
#ifdef USE_LATE_PROJECTION
			qdo_mode = QDO_Totals;
			if (set_print_mask_from_stream(app.mask, "SELECT NOHEADER\nSUMMARY STANDARD", false, app.attrs) < 0) {
#else
			dash_totals = true;
			if( !custom_attributes ) {
				custom_attributes = true;
				attrs.clearAll();
			}
			usingPrintMask = true;
			if (set_print_mask_from_stream(mask, "SELECT NOHEADER\nSUMMARY STANDARD", false, attrs) < 0) {
#endif
				fprintf(stderr, "Error: unexpected error!\n");
				exit (1);
			}
		}
		else if (is_dash_arg_prefix(dash_arg, "expert", 1)) {
			expert = true;
#ifdef USE_LATE_PROJECTION
			/// fix me
#else
			attrs.clearAll();
#endif
		}
		else if (is_dash_arg_prefix(dash_arg, "jobads", 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: -jobads requires a filename\n");
				exit(1);
			} else {
				i++;
				jobads_file = argv[i];
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
		else if (is_dash_arg_prefix(dash_arg, "slotads", 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: -slotads requires a filename\n");
				exit(1);
			} else {
				i++;
				machineads_file = argv[i];
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
#ifdef HAVE_EXT_POSTGRESQL
		else if (is_dash_arg_prefix(dash_arg, "avgqueuetime", 4)) {
				/* if user want average wait time, we will perform direct DB query */
			avgqueuetime = true;
			directDBquery =  true;
		}
#endif /* HAVE_EXT_POSTGRESQL */
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

#ifdef USE_LATE_PROJECTION
	if (dash_dry_run) {
		const char * const amo[] = { "", "run", "goodput", "globus", "grid", "hold", "io", "dag", "totals", "autocluster", "custom", "analyze" };
		fprintf(stderr, "\ncondor_q %s %s\n", amo[qdo_mode & QDO_BaseMask], dash_long ? "-long" : "");
	}
	if ( ! dash_long && ! (qdo_mode & QDO_Format) && (qdo_mode & QDO_BaseMask) < QDO_Custom) {
		initOutputMask(app.mask, qdo_mode, widescreen);
	}
#else
	if (dash_dry_run) {
		const char * pdash = "";
		if (dash_totals) { pdash = "totals"; }
		if (dash_dag) { pdash = "dag"; }
		if (dash_run) { pdash = "run"; }
		else if (dash_goodput) { pdash = "goodput"; }
		else if (dash_globus) { pdash = "globus"; }
		else if (dash_grid) { pdash = "grid"; }
		else if (show_held) { pdash = "hold"; }
		else if (show_io) { pdash = "io"; }
		fprintf(stderr, "\ncondor_q %s %s\n", pdash, dash_long ? "-long" : "");
	}
		//Added so -long or -xml can be listed before other options
	if(dash_long && !custom_attributes) {
		attrs.clearAll();
	}
#endif

	// convert cluster and cluster.proc into constraints
	// if there is a -dag argument, then we look up all children of the dag
	// as well as the dag itself.
	if ( ! constrID.empty()) {
		// dont default to 'my jobs'
		default_fetch_opts &= ~CondorQ::fetch_MyJobs;

		for (std::vector<CondorID>::const_iterator it = constrID.begin(); it != constrID.end(); ++it) {

			// if we aren't doing db queries, do we need to do this?
			Q.addDBConstraint(CQ_CLUSTER_ID, it->_cluster);
			if (it->_proc >= 0) { Q.addDBConstraint(CQ_PROC_ID, it->_proc); }

			// add a constraint to match the jobid.
			if (it->_proc >= 0) {
				sprintf(constraint, ATTR_CLUSTER_ID " == %d && " ATTR_PROC_ID " == %d", it->_cluster, it->_proc);
			} else {
				sprintf(constraint, ATTR_CLUSTER_ID " == %d", it->_cluster);
			}
			Q.addOR(constraint);

			// if we are doing -dag output, then also request any jobs that are inside this dag.
			// we know that a jobid for a dagman job will always never have a proc > 0
			if ((dash_dag || dash_progress) && it->_proc < 1) {
				sprintf(constraint, ATTR_DAGMAN_JOB_ID " == %d", it->_cluster);
				Q.addOR(constraint);
			}
		}
	} else if (show_io) {
#ifdef USE_LATE_PROJECTION
	// InitOutputMask does this now
#else
		// if no job id's specifed, then constrain the query to jobs that are doing file transfer
		Q.addAND("JobUniverse==1 || TransferQueued=?=true || TransferringOutput=?=true || TransferringInput=?=true");
#endif
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
	}

	return total_wall_time;
}

unsigned int process_direct_argument(char *arg)
{
#ifdef HAVE_EXT_POSTGRESQL
	if (strcasecmp(arg, "rdbms") == MATCH) {
		return DIRECT_RDBMS;
	}

	if (strcasecmp(arg, "quilld") == MATCH) {
		return DIRECT_QUILLD;
	}

#endif

	if (strcasecmp(arg, "schedd") == MATCH) {
		return DIRECT_SCHEDD;
	}

#ifdef HAVE_EXT_POSTGRESQL
	fprintf( stderr, 
		"Error: -direct requires [rdbms | quilld | schedd]\n" );
#else
	fprintf( stderr, 
		"Error: Quill feature set is not available.\n"
		"-direct may only take 'schedd' as an option.\n" );
#endif

	exit(EXIT_FAILURE);

	/* Here to make the compiler happy since there is an exit above */
	return DIRECT_UNKNOWN;
}

#ifdef USE_LATE_PROJECTION
#else

static void
buffer_io_display(std::string & out, ClassAd *ad)
{
	int univ=0;
	ad->EvalInteger(ATTR_JOB_UNIVERSE, NULL, univ);

	const char * idIter = ATTR_NUM_JOB_STARTS;
	if (univ==CONDOR_UNIVERSE_STANDARD) idIter = ATTR_NUM_CKPTS_RAW;

	int cluster=0, proc=0, iter=0, status=0;
	char misc[256];
	ad->LookupInteger(ATTR_CLUSTER_ID,cluster);
	ad->LookupInteger(ATTR_PROC_ID,proc);
	ad->LookupInteger(idIter,iter);
	ad->LookupString(ATTR_OWNER, misc, sizeof(misc));
	ad->LookupInteger(ATTR_JOB_STATUS,status);

	formatstr( out, "%4d.%-3d %-14s %4d %2s", cluster, proc, 
				format_owner( misc, ad ),
				iter,
				format_job_status_char(status, ad, *((Formatter*)NULL)) );

	double read_bytes=0, write_bytes=0, wall_clock=-1;
	ad->EvalFloat(ATTR_JOB_REMOTE_WALL_CLOCK,NULL,wall_clock);

	misc[0]=0;

	bool have_bytes = true;
	if (univ==CONDOR_UNIVERSE_STANDARD) {

		ad->EvalFloat(ATTR_FILE_READ_BYTES,NULL,read_bytes);
		ad->EvalFloat(ATTR_FILE_WRITE_BYTES,NULL,write_bytes);

		have_bytes = wall_clock >= 1.0 && (read_bytes >= 1.0 || write_bytes >= 1.0);

		double seek_count=0;
		int buffer_size=0, block_size=0;
		ad->EvalFloat(ATTR_FILE_SEEK_COUNT,NULL,seek_count);
		ad->EvalInteger(ATTR_BUFFER_SIZE,NULL,buffer_size);
		ad->EvalInteger(ATTR_BUFFER_BLOCK_SIZE,NULL,block_size);

		sprintf(misc, " seeks=%d, buf=%d,%d", (int)seek_count, buffer_size, block_size);
	} else {
		// not std universe. show transfer queue stats
		/*
			BytesRecvd/1048576.0 AS " MB_INPUT"  PRINTF "%9.2f"
			BytesSent/1048576.0  AS "MB_OUTPUT"  PRINTF "%9.2f"
			{JobStatus,TransferringInput,TransferringOutput,TransferQueued}[0] AS ST PRINTAS JOB_STATUS
			TransferQueued       AS XFER_Q
			TransferringInput    AS XFER_IN
			TransferringOutput=?=true || JobStatus==6   AS XFER_OUT
		*/
		ad->EvalFloat(ATTR_BYTES_RECVD,NULL,read_bytes);
		ad->EvalFloat(ATTR_BYTES_SENT,NULL,write_bytes);

		have_bytes = (read_bytes > 0 || write_bytes > 0);

		int transferring_input = false;
		int transferring_output = false;
		int transfer_queued = false;
		ad->EvalBool(ATTR_TRANSFERRING_INPUT,NULL,transferring_input);
		ad->EvalBool(ATTR_TRANSFERRING_OUTPUT,NULL,transferring_output);
		ad->EvalBool(ATTR_TRANSFER_QUEUED,NULL,transfer_queued);

		StringList xfer_states;
		if (transferring_input) xfer_states.append("in");
		if (transferring_output) xfer_states.append("out");
		if (transfer_queued) xfer_states.append("queued");
		if ( ! xfer_states.isEmpty()) {
			char * xfer = xfer_states.print_to_string();
			sprintf(misc, " transfer=%s", xfer);
			free(xfer);
		}
	}

	/* If the jobAd values are not set, OR the values are all zero,
		report no data collected. This can be true for a standard universe
		job that has not checkpointed yet. */

	if ( ! have_bytes) {
		//                                                                            12345678 12345678 12345678/s
		out += (univ==CONDOR_UNIVERSE_STANDARD) ? "   [ no i/o data collected ] " : "    .        .        .      ";
	} else {
		if(wall_clock < 1.0) wall_clock=1;

		/*
		Note: metric_units() cannot be used twice in the same
		statement -- it returns a pointer to a static buffer.
		*/

		if (read_bytes > 0) {
			formatstr_cat(out, " %8s", metric_units(read_bytes) );
		} else {
			out += "    .    ";
		}
		if (write_bytes > 0) {
			formatstr_cat(out, " %8s", metric_units(write_bytes) );
		} else {
			out += "    .    ";
		}
		if (read_bytes + write_bytes > 0) {
			formatstr_cat(out, " %8s/s", metric_units((int)((read_bytes+write_bytes)/wall_clock)) );
		} else {
			out += "    .      ";
		}
	}

	out += misc;
	out += "\n";
}


static char *
bufferJobShort( ClassAd *ad ) {
	int cluster, proc, date, status, prio, image_size, memory_usage;

	char encoded_status;
	int last_susp_time;

	double utime  = 0.0;
	char owner[64];
	char *cmd = NULL;
	MyString buffer;

	if (!ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cluster)		||
		!ad->EvalInteger (ATTR_PROC_ID, NULL, proc)				||
		!ad->EvalInteger (ATTR_Q_DATE, NULL, date)				||
		!ad->EvalFloat   (ATTR_JOB_REMOTE_USER_CPU, NULL, utime)||
		!ad->EvalInteger (ATTR_JOB_STATUS, NULL, status)		||
		!ad->EvalInteger (ATTR_JOB_PRIO, NULL, prio)			||
		!ad->EvalInteger (ATTR_IMAGE_SIZE, NULL, image_size)	||
		!ad->EvalString  (ATTR_OWNER, NULL, owner)				||
		!ad->EvalString  (ATTR_JOB_CMD, NULL, &cmd) )
	{
		sprintf (return_buff, " --- ???? --- \n");
		return( return_buff );
	}
	
	// print memory usage unless it's unavailable, then print image size
	// note that memory usage is megabytes but imagesize is kilobytes.
	double memory_used_mb = image_size / 1024.0;
	if (ad->EvalInteger(ATTR_MEMORY_USAGE, NULL, memory_usage)) {
		memory_used_mb = memory_usage;
	}

	std::string description;
	if ( ! ad->EvalString("MATCH_EXP_" ATTR_JOB_DESCRIPTION, NULL, description)) {
		ad->EvalString(ATTR_JOB_DESCRIPTION, NULL, description);
	}
	if ( !description.empty() ){
		buffer.formatstr("%s", description.c_str());
	} else {
		buffer.formatstr( "%s", condor_basename(cmd) );
		MyString args_string;
		ArgList::GetArgsStringForDisplay(ad,&args_string);
		if ( ! args_string.IsEmpty()) {
			buffer.formatstr_cat( " %s", args_string.Value() );
		}
	}
	free(cmd);
	utime = job_time(utime,ad);

	encoded_status = encode_status( status );

	/* The suspension of a job is a second class citizen and is not a true
		status that can exist as a job status ad and is instead
		inferred, so therefore the processing and display of
		said suspension is also second class. */
	if (param_boolean("REAL_TIME_JOB_SUSPEND_UPDATES", false)) {
			if (!ad->EvalInteger(ATTR_LAST_SUSPENSION_TIME,NULL,last_susp_time))
			{
				last_susp_time = 0;
			}
			/* sanity check the last_susp_time against if the job is running
				or not in case the schedd hasn't synchronized the
				last suspension time attribute correctly to job running
				boundaries. */
			if ( status == RUNNING && last_susp_time != 0 )
			{
				encoded_status = 'S';
			}
	}

		// adjust status field to indicate file transfer status
	int transferring_input = false;
	int transferring_output = false;
	ad->EvalBool(ATTR_TRANSFERRING_INPUT,NULL,transferring_input);
	ad->EvalBool(ATTR_TRANSFERRING_OUTPUT,NULL,transferring_output);
	if( transferring_input ) {
		encoded_status = '<';
	}
	if( transferring_output ) {
		encoded_status = '>';
	}

	sprintf( return_buff,
			 widescreen ? description.empty() ? "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f %s\n"
			                                   : "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f (%s)\n"
			            : description.empty() ? "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f %.18s\n"
				                           : "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f (%.17s)\n",
			 cluster,
			 proc,
			 format_owner( owner, ad ),
			 format_date( (time_t)date ),
			 /* In the next line of code there is an (int) typecast. This
			 	has to be there otherwise the compiler produces incorrect code
				here and performs a structural typecast from the float to an
				int(like how a union works) and passes a garbage number to the
				format_time function. The compiler is *supposed* to do a
				functional typecast, meaning generate code that changes the
				float to an int legally. Apparently, that isn't happening here.
				-psilord 09/06/01 */
			 format_time( (int)utime ),
			 encoded_status,
			 prio,
			 memory_used_mb,
			 buffer.Value() );

	return return_buff;
}

static void 
short_header (void)
{
	printf( " %-7s %-14s ", "ID", dash_dag ? "OWNER/NODENAME" : "OWNER" );
	if (dash_goodput) {
		printf( "%11s %12s %-16s\n", "SUBMITTED", JOB_TIME,
				"GOODPUT CPU_UTIL   Mb/s" );
	} else if (dash_globus) {
		printf( "%-10s %-8s %-18s  %-18s\n", 
				"STATUS", "MANAGER", "HOST", "EXECUTABLE" );
	} else if ( dash_grid ) {
		printf( "%-10s  %-16s %-16s  %-18s\n", 
				"STATUS", "GRID->MANAGER", "HOST", "GRID-JOB-ID" );
	} else if ( show_held ) {
		printf( "%11s %-30s\n", "HELD_SINCE", "HOLD_REASON" );
	} else if ( show_io ) {
		printf( "%4s %2s %-8s %-8s %-10s %s\n",
				"RUNS", "ST", " INPUT", " OUTPUT", " RATE", "MISC");
	} else if( dash_run ) {
		printf( "%11s %12s %-16s\n", "SUBMITTED", JOB_TIME, "HOST(S)" );
	} else {
		printf( "%11s %12s %-2s %-3s %-4s %-18s\n",
			"SUBMITTED",
			JOB_TIME,
			"ST",
			"PRI",
			"SIZE",
			"CMD"
		);
	}
}

#endif

static bool
render_remote_host (std::string & result, AttrList *ad, Formatter &)
{
	//static char host_result[MAXHOSTNAMELEN];
	//static char unknownHost [] = "[????????????????]";
	condor_sockaddr addr;

	int universe = CONDOR_UNIVERSE_STANDARD;
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
render_cpu_time (double & cputime, AttrList *ad, Formatter &)
{
	if ( ! ad->EvalFloat(ATTR_JOB_REMOTE_USER_CPU, NULL, cputime))
		return false;

	cputime = job_time(cputime, ad);
	//return format_time( (int) job_time(utime,(ClassAd *)ad) );
	return true;
}

static bool
render_memory_usage(double & mem_used_mb, AttrList *ad, Formatter &)
{
	long long  image_size;
	long long memory_usage;
	// print memory usage unless it's unavailable, then print image size
	// note that memory usage is megabytes but imagesize is kilobytes.
	if (ad->EvalInteger(ATTR_MEMORY_USAGE, NULL, memory_usage)) {
		mem_used_mb = memory_usage;
	} else if (ad->EvalInteger(ATTR_IMAGE_SIZE, NULL, image_size)) {
		mem_used_mb = image_size / 1024.0;
	} else {
		return false;
	}
	return true;
}

static const char *
format_readable_mb(const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi * 1024.0 * 1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0 * 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

static const char *
format_readable_kb(const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi*1024.0;
	} else if (val.IsRealValue(kb)) {
		kb *= 1024.0;
	} else {
		return "        ";
	}
	return metric_units(kb);
}

static const char *
format_readable_bytes(const classad::Value &val, Formatter &)
{
	long long kbi;
	double kb;
	if (val.IsIntegerValue(kbi)) {
		kb = kbi;
	} else if (val.IsRealValue(kb)) {
		// nothing to do.
	} else {
		return "        ";
	}
	return metric_units(kb);
}

static bool
render_job_description(std::string & out, AttrList *ad, Formatter &)
{
	if ( ! ad->EvalString(ATTR_JOB_CMD, NULL, out))
		return false;

	std::string description;
	if ( ! ad->EvalString("MATCH_EXP_" ATTR_JOB_DESCRIPTION, NULL, description)) {
		ad->EvalString(ATTR_JOB_DESCRIPTION, NULL, description);
	}
	if ( ! description.empty()) {
		formatstr(out, "(%s)", description.c_str());
	} else {
		MyString put_result = condor_basename(out.c_str());
		MyString args_string;
		ArgList::GetArgsStringForDisplay(ad,&args_string);
		if ( ! args_string.IsEmpty()) {
			put_result.formatstr_cat(" %s", args_string.Value());
		}
		out = put_result;
	}
	return true;
}

static const char *
format_job_universe(long long job_universe, Formatter &)
{
	return CondorUniverseNameUcFirst(job_universe);
}

static bool
render_job_id(std::string & result, AttrList* ad, Formatter &)
{
	static char str[PROC_ID_STR_BUFLEN];
	int cluster_id=0, proc_id=0;
	if ( ! ad->LookupInteger(ATTR_CLUSTER_ID, cluster_id))
		return false;
	ad->LookupInteger(ATTR_PROC_ID,proc_id);
	ProcIdToStr(cluster_id, proc_id, str);
	result = str;
	return true;
}

static const char *
format_job_status_raw(long long job_status, Formatter &)
{
	switch(job_status) {
	case IDLE:      return "Idle   ";
	case HELD:      return "Held   ";
	case RUNNING:   return "Running";
	case COMPLETED: return "Complet";
	case REMOVED:   return "Removed";
	case SUSPENDED: return "Suspend";
	case TRANSFERRING_OUTPUT: return "XFerOut";
	default:        return "Unk    ";
	}
}

static bool
render_job_status_char(std::string & result, AttrList*ad, Formatter &)
{
	int job_status;
	if ( ! ad->LookupInteger(ATTR_JOB_STATUS, job_status))
		return false;

	static char put_result[3];
	put_result[1] = ' ';
	put_result[2] = 0;

	put_result[0] = encode_status(job_status);

	/* The suspension of a job is a second class citizen and is not a true
		status that can exist as a job status ad and is instead
		inferred, so therefore the processing and display of
		said suspension is also second class. */
	if (param_boolean("REAL_TIME_JOB_SUSPEND_UPDATES", false)) {
		int last_susp_time;
		if (!ad->EvalInteger(ATTR_LAST_SUSPENSION_TIME,NULL,last_susp_time))
		{
			last_susp_time = 0;
		}
		/* sanity check the last_susp_time against if the job is running
			or not in case the schedd hasn't synchronized the
			last suspension time attribute correctly to job running
			boundaries. */
		if ( job_status == RUNNING && last_susp_time != 0 )
		{
			put_result[0] = 'S';
		}
	}

		// adjust status field to indicate file transfer status
	int transferring_input = false;
	int transferring_output = false;
	int transfer_queued = false;
	ad->EvalBool(ATTR_TRANSFERRING_INPUT,NULL,transferring_input);
	ad->EvalBool(ATTR_TRANSFERRING_OUTPUT,NULL,transferring_output);
	ad->EvalBool(ATTR_TRANSFER_QUEUED,NULL,transfer_queued);
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
render_goodput (double & goodput_time, AttrList *ad, Formatter & /*fmt*/)
{
	int job_status;
	if ( ! ad->LookupInteger(ATTR_JOB_STATUS, job_status))
		return false;

	int ckpt_time = 0, shadow_bday = 0, last_ckpt = 0;
	double wall_clock = 0.0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time );
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	ad->LookupInteger( ATTR_LAST_CKPT_TIME, last_ckpt );
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock );
	if ((job_status == RUNNING || job_status == TRANSFERRING_OUTPUT || job_status == SUSPENDED) &&
		shadow_bday && last_ckpt > shadow_bday)
	{
		wall_clock += last_ckpt - shadow_bday;
	}
	if (wall_clock <= 0.0) return false;

	goodput_time = ckpt_time/wall_clock*100.0;
	if (goodput_time > 100.0) goodput_time = 100.0;
	else if (goodput_time < 0.0) return false;
	//sprintf(put_result, " %6.1f%%", goodput_time);
	return true;
}

static bool
render_mbps (double & mbps, AttrList *ad, Formatter & /*fmt*/)
{
	double bytes_sent;
	if ( ! ad->EvalFloat(ATTR_BYTES_SENT, NULL, bytes_sent))
		return false;

	double wall_clock=0.0, bytes_recvd=0.0, total_mbits;
	int shadow_bday = 0, last_ckpt = 0, job_status = IDLE;
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock );
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	ad->LookupInteger( ATTR_LAST_CKPT_TIME, last_ckpt );
	ad->LookupInteger( ATTR_JOB_STATUS, job_status );
	if ((job_status == RUNNING || job_status == TRANSFERRING_OUTPUT || job_status == SUSPENDED) && shadow_bday && last_ckpt > shadow_bday) {
		wall_clock += last_ckpt - shadow_bday;
	}
	ad->LookupFloat(ATTR_BYTES_RECVD, bytes_recvd);
	total_mbits = (bytes_sent+bytes_recvd)*8/(1024*1024); // bytes to mbits
	if (total_mbits <= 0) return false;
	mbps = total_mbits / wall_clock;
	// sprintf(result_format, " %6.2f", mbps);
	return true;
}

static bool
render_cpu_util (double & cputime, AttrList *ad, Formatter & /*fmt*/)
{
	if ( ! ad->EvalFloat(ATTR_JOB_REMOTE_USER_CPU, NULL, cputime))
		return false;

	int ckpt_time = 0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time);
	if (ckpt_time == 0) return false;
	double util = cputime/ckpt_time*100.0;
	if (util > 100.0) util = 100.0;
	else if (util < 0.0) return false;
	util = cputime;
	// printf(result_format, "  %6.1f%%", util);
	return true;
}

static bool
render_buffer_io_misc (std::string & misc, AttrList *ad, Formatter & /*fmt*/)
{
	misc.clear();

	int univ = 0;
	if ( ! ad->EvalInteger(ATTR_JOB_UNIVERSE,NULL,univ))
		return false;

	if (univ==CONDOR_UNIVERSE_STANDARD) {

		double seek_count=0;
		int buffer_size=0, block_size=0;
		ad->EvalFloat(ATTR_FILE_SEEK_COUNT,NULL,seek_count);
		ad->EvalInteger(ATTR_BUFFER_SIZE,NULL,buffer_size);
		ad->EvalInteger(ATTR_BUFFER_BLOCK_SIZE,NULL,block_size);

		formatstr(misc, " seeks=%d, buf=%d,%d", (int)seek_count, buffer_size, block_size);
	} else {

		int ix = 0;
		int bb = false;
		ad->EvalBool(ATTR_TRANSFERRING_INPUT,NULL, bb);
		ix += bb?1:0;

		bb = false;
		ad->EvalBool(ATTR_TRANSFERRING_OUTPUT,NULL,bb);
		ix += bb?2:0;

		bb = false;
		ad->EvalBool(ATTR_TRANSFER_QUEUED,NULL,bb);
		ix += bb?4:0;

		if (ix) {
			static const char * const ax[] = { "in", "out", "in,out", "queued", "in,queued", "out,queued", "in,out,queued" };
			formatstr(misc, " transfer=%s", ax[ix-1]); 
		}
	}

	return true;
}

#ifdef USE_LATE_PROJECTION

static bool
render_owner(std::string & out, AttrList *ad, Formatter & /*fmt*/)
{
	if ( ! ad->LookupString(ATTR_OWNER, out))
		return false;

	int niceUser;
	if (ad->LookupInteger( ATTR_NICE_USER, niceUser) && niceUser ) {
		char tmp[sizeof(NiceUserName)+2];
		strcpy(tmp, NiceUserName);
		strcat(tmp, ".");
		out.insert(0, tmp);
	}
	max_owner_name = MAX(max_owner_name, (int)out.length());
	return true;
}

static bool
render_dag_owner (std::string & out, AttrList *ad, Formatter & fmt)
{
	if (dash_dag && ad->LookupExpr(ATTR_DAGMAN_JOB_ID)) {
		if (ad->LookupString(ATTR_DAG_NODE_NAME, out)) {
			max_owner_name = MAX(max_owner_name, (int)out.length()+3);
			return true;
		} else {
			fprintf(stderr, "DAG node job with no %s attribute!\n", ATTR_DAG_NODE_NAME);
		}
	}
	return render_owner(out, ad, fmt);
}

static bool
render_batch_name (std::string & out, AttrList *ad, Formatter & /*fmt*/)
{
	const bool fold_dagman_sibs = dash_progress && (dash_progress & 2);

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
	} else if (ad->LookupString(ATTR_JOB_CMD, tmp)) {
		const char * name = tmp.c_str();
		if (tmp.length() > 24) { name = condor_basename(name); }
		formatstr(out, "CMD: %s", name);
	} else {
		return false;
	}
	max_batch_name = MAX(max_batch_name, (int)out.length());
	return true;
}

#else

static const char *
format_owner_common (const char *owner, AttrList *ad)
{
	static char result_str[100] = "";

	// [this is a somewhat kludgey place to implement DAG formatting,
	// but for a variety of reasons (maintainability, allowing future
	// changes to be made in only one place, etc.), Todd & I decided
	// it's the best way to do it given the existing code...  --pfc]

	// if -dag is specified, check whether this job was started by a
	// DAGMan (by seeing if it has a valid DAGManJobId attribute), and
	// if so, print DAGNodeName in place of owner

	// (we need to check that the DAGManJobId is valid because DAGMan
	// >= v6.3 inserts "unknown..." into DAGManJobId when run under a
	// pre-v6.3 schedd)

	if ( dash_dag && ad->LookupExpr( ATTR_DAGMAN_JOB_ID ) ) {
			// We have a DAGMan job ID, this means we have a DAG node
			// -- don't worry about what type the DAGMan job ID is.
		if ( ad->LookupString( ATTR_DAG_NODE_NAME, result_str, COUNTOF(result_str) ) ) {
			return result_str;
		} else {
			fprintf(stderr, "DAG node job with no %s attribute!\n",
					ATTR_DAG_NODE_NAME);
		}
	}

	int niceUser;
	if (ad->LookupInteger( ATTR_NICE_USER, niceUser) && niceUser ) {
		sprintf(result_str, "%s.%s", NiceUserName, owner);
	} else {
		strcpy_len(result_str, owner, COUNTOF(result_str));
	}
	return result_str;
}

static const char *
format_owner (const char *owner, AttrList *ad)
{
	static char result_format[24];
	sprintf(result_format, "%-14.14s", format_owner_common(owner, ad));
	return result_format;
}

static const char *
format_dag_owner (const char *owner, AttrList *ad, Formatter & /*fmt*/)
{
	return format_owner_common(owner, ad);
}

static const char *
format_owner_wide (const char *owner, AttrList *ad, Formatter & /*fmt*/)
{
	return format_owner_common(owner, ad);
}
#endif

static bool
render_globusStatus(std::string & result, AttrList * ad, Formatter & /*fmt*/ )
{
	int globusStatus;
	if ( ! ad->LookupInteger(ATTR_GLOBUS_STATUS, globusStatus))
		return false;
#if defined(HAVE_EXT_GLOBUS)
	char result_str[64];
	sprintf(result_str, " %7.7s", GlobusJobStatusName( globusStatus ) );
	result = result_str;
#else
	static const struct {
		int status;
		const char * psz;
	} gram_states[] = {
		{ 1, "PENDING" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING = 1,
		{ 2, "ACTIVE" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE = 2,
		{ 4, "FAILED" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED = 4,
		{ 8, "DONE" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE = 8,
		{16, "SUSPEND" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED = 16,
		{32, "UNSUBMIT" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED = 32,
		{64, "STAGE_IN" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN = 64,
		{128,"STAGE_OUT" },	 //GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT = 128,
	};
	for (size_t ii = 0; ii < COUNTOF(gram_states); ++ii) {
		if (globusStatus == gram_states[ii].status) {
			result = gram_states[ii].psz;
			return true;
		}
	}
	formatstr(result, "%d", globusStatus);
#endif
	return true;
}

// The remote hostname may be in GlobusResource or GridResource.
// We want this function to be called if at least one is defined,
// but it will only be called if the one attribute it's registered
// with is defined. So we register it with an attribute we know will
// always be present and be a string. We then ignore that attribute
// and examine GlobusResource and GridResource.
static bool
render_globusHostAndJM(std::string & result, AttrList *ad, Formatter & /*fmt*/ )
{
	//static char result_format[64];
	char	host[80] = "[?????]";
	char	jm[80] = "fork";
	char	*tmp;
	int	p;
	char *attr_value = NULL;
	char *resource_name = NULL;
	char *grid_type = NULL;

	result.clear();

	if ( ! ad->LookupString( ATTR_GRID_RESOURCE, &attr_value ))
		return false;

	// ATTR_GRID_RESOURCE exists, skip past the initial
	// '<job type> '.
	resource_name = strchr( attr_value, ' ' );
	if ( resource_name ) {
		*resource_name = '\0';
		grid_type = strdup( attr_value );
		resource_name++;
	}

	if ( resource_name != NULL ) {

		if ( grid_type == NULL || !strcasecmp( grid_type, "gt2" ) ||
			 !strcasecmp( grid_type, "gt5" ) ||
			 !strcasecmp( grid_type, "globus" ) ) {

			// copy the hostname
			p = strcspn( resource_name, ":/" );
			if ( p >= (int) sizeof(host) )
				p = sizeof(host) - 1;
			strncpy( host, resource_name, p );
			host[p] = '\0';

			if ( ( tmp = strstr( resource_name, "jobmanager-" ) ) != NULL ) {
				tmp += 11; // 11==strlen("jobmanager-")

				// copy the jobmanager name
				p = strcspn( tmp, ":" );
				if ( p >= (int) sizeof(jm) )
					p = sizeof(jm) - 1;
				strncpy( jm, tmp, p );
				jm[p] = '\0';
			}

		} else if ( !strcasecmp( grid_type, "gt4" ) ) {

			strcpy( jm, "Fork" );

				// GridResource is of the form '<service url> <jm type>'
				// Find the space, zero it out, and grab the jm type from
				// the end (if it's non-empty).
			tmp = strchr( resource_name, ' ' );
			if ( tmp ) {
				*tmp = '\0';
				if ( tmp[1] != '\0' ) {
					strcpy( jm, &tmp[1] );
				}
			}

				// Pick the hostname out of the URL
			if ( strncmp( "https://", resource_name, 8 ) == 0 ) {
				strncpy( host, &resource_name[8], sizeof(host) );
				host[sizeof(host)-1] = '\0';
			} else {
				strncpy( host, resource_name, sizeof(host) );
				host[sizeof(host)-1] = '\0';
			}
			p = strcspn( host, ":/" );
			host[p] = '\0';
		}
	}

	if ( grid_type ) {
		free( grid_type );
	}

	if ( attr_value ) {
		free( attr_value );
	}

	// done --- pack components into the result string and return
	formatstr( result, " %-8.8s %-18.18s  ", jm, host );
	return true;
}

static bool
render_gridStatus( std::string & result, AttrList * ad, Formatter & fmt )
{
	if (ad->LookupString(ATTR_GRID_JOB_STATUS, result)) {
		return true;
	} 
	if (render_globusStatus(result, ad, fmt))
		return true;

	int jobStatus;
	if ( ! ad->LookupInteger(ATTR_GRID_JOB_STATUS, jobStatus))
		return false;

	static const struct {
		int status;
		const char * psz;
	} states[] = {
		{ IDLE, "IDLE" },
		{ RUNNING, "RUNNING" },
		{ COMPLETED, "COMPLETED" },
		{ HELD, "HELD" },
		{ SUSPENDED, "SUSPENDED" },
		{ REMOVED, "REMOVED" },
		{ TRANSFERRING_OUTPUT, "XFER_OUT" },
	};
	for (size_t ii = 0; ii < COUNTOF(states); ++ii) {
		if (jobStatus == states[ii].status) {
			result = states[ii].psz;
			return true;
		}
	}
	formatstr(result, "%d", jobStatus);
	return true;
}

#if 0 // not currently used, disabled to shut up fedora.
static const char *
format_gridType( int , AttrList * ad )
{
	static char result[64];
	char grid_res[64];
	if (ad->LookupString( ATTR_GRID_RESOURCE, grid_res, COUNTOF(grid_res) )) {
		char * p = result;
		char * r = grid_res;
		*p++ = ' ';
		while (*r && *r != ' ') {
			*p++ = *r++;
		}
		while (p < &result[5]) *p++ = ' ';
		*p++ = 0;
	} else {
		strcpy_len(result, "   ?  ", sizeof(result));
	}
	return result;
}
#endif

static bool
render_gridResource(std::string & result, AttrList * ad, Formatter & /*fmt*/ )
{
	std::string grid_type;
	std::string str;
	std::string mgr = "[?]";
	std::string host = "[???]";
	const bool fshow_host_port = false;
	const unsigned int width = 1+6+1+8+1+18+1;

	if ( ! ad->EvalString(ATTR_GRID_RESOURCE, NULL, str))
		return false;

	// GridResource is a string with the format 
	//      "type host_url manager" (where manager can contain whitespace)
	// or   "type host_url/jobmanager-manager"
	unsigned int ixHost = str.find_first_of(' ');
	if (ixHost < str.length()) {
		grid_type = str.substr(0, ixHost);
		ixHost += 1; // skip over space.
	} else {
		grid_type = "globus";
		ixHost = 0;
	}

	unsigned int ix2 = str.find_first_of(' ', ixHost);
	if (ix2 < str.length()) {
		mgr = str.substr(ix2+1);
	} else {
		unsigned int ixMgr = str.find("jobmanager-", ixHost);
		if (ixMgr < str.length()) 
			mgr = str.substr(ixMgr+11);	//sizeof("jobmanager-") == 11
		ix2 = ixMgr;
	}

	unsigned int ix3 = str.find("://", ixHost);
	ix3 = (ix3 < str.length()) ? ix3+3 : ixHost;
	unsigned int ix4 = str.find_first_of(fshow_host_port ? "/" : ":/",ix3);
	if (ix4 > ix2) ix4 = ix2;
	host = str.substr(ix3, ix4-ix3);

	MyString mystr = mgr.c_str();
	mystr.replaceString(" ", "/");
	mgr = mystr.Value();

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

static bool
render_gridJobId(std::string & jid, AttrList *ad, Formatter & /*fmt*/ )
{
	std::string str;
	std::string host;

	if ( ! ad->EvalString(ATTR_GRID_JOB_ID, NULL, str))
		return false;

	std::string grid_type = "globus";
	char grid_res[64];
	if (ad->LookupString( ATTR_GRID_RESOURCE, grid_res, COUNTOF(grid_res) )) {
		char * r = grid_res;
		while (*r && *r != ' ') {
			++r;
		}
		*r = 0;
		grid_type = grid_res;
	}
	bool gram = (MATCH == grid_type.compare("gt5")) || (MATCH == grid_type.compare("gt2"));

	unsigned int ix2 = str.find_last_of(" ");
	ix2 = (ix2 < str.length()) ? ix2 + 1 : 0;

	unsigned int ix3 = str.find("://", ix2);
	ix3 = (ix3 < str.length()) ? ix3+3 : ix2;
	unsigned int ix4 = str.find_first_of("/",ix3);
	ix4 = (ix4 < str.length()) ? ix4 : ix3;
	host = str.substr(ix3, ix4-ix3);

	if (gram) {
		jid = host;
		jid += " : ";
		if (str[ix4] == '/') ix4 += 1;
		unsigned int ix5 = str.find_first_of("/",ix4);
		jid = str.substr(ix4, ix5-ix4);
		if (ix5 < str.length()) {
			if (str[ix5] == '/') ix5 += 1;
			unsigned int ix6 = str.find_first_of("/",ix5);
			jid += ".";
			jid += str.substr(ix5, ix6-ix5);
		}
	} else {
		jid.clear();
		//jid = grid_type;
		//jid += " : ";
		jid += str.substr(ix4);
	}

	return true;
}

static const char *
format_q_date (long long d, Formatter &)
{
	return format_date((int)d);
}


static void
usage (const char *myName, int other)
{
	printf ("Usage: %s [general-opts] [restriction-list] [output-opts | analyze-opts]\n", myName);
	printf ("\n    [general-opts] are\n"
		"\t-global\t\t\tQuery all Schedulers in this pool\n"
		"\t-schedd-constraint\tQuery all Schedulers matching this constraint\n"
		"\t-submitter <submitter>\tGet queue of specific submitter\n"
		"\t-name <name>\t\tName of Scheduler\n"
		"\t-pool <host>\t\tUse host as the central manager to query\n"
#ifdef HAVE_EXT_POSTGRESQL
		"\t-direct <rdbms | schedd>\n"
		"\t\tPerform a direct query to the rdbms\n"
		"\t\tor to the schedd without falling back to the queue\n"
		"\t\tlocation discovery algortihm, even in case of error\n"
		"\t-avgqueuetime\t\tAverage queue time for uncompleted jobs\n"
#endif
		"\t-jobads <file>\t\tRead queue from a file of job ClassAds\n"
		"\t-userlog <file>\t\tRead queue from a user log file\n"
		);

	printf ("\n    [restriction-list] each restriction may be one of\n"
		"\t<cluster>\t\tGet information about specific cluster\n"
		"\t<cluster>.<proc>\tGet information about specific job\n"
		"\t<owner>\t\t\tInformation about jobs owned by <owner>\n"
		"\t-autocluster\t\tGet information about the SCHEDD's autoclusters\n"
		"\t-constraint <expr>\tGet information about jobs that match <expr>\n"
		"\t-allusers\t\tConsider jobs from all users\n"
		);

	printf ("\n    [output-opts] are\n"
		"\t-limit <num>\t\tLimit the number of results to <num>\n"
		"\t-cputime\t\tDisplay CPU_TIME instead of RUN_TIME\n"
		"\t-currentrun\t\tDisplay times only for current run\n"
		"\t-debug\t\t\tDisplay debugging info to console\n"
		"\t-dag\t\t\tSort DAG jobs under their DAGMan\n"
		"\t-expert\t\t\tDisplay shorter error messages\n"
		"\t-globus\t\t\tGet information about jobs of type globus\n"
		"\t-goodput\t\tDisplay job goodput statistics\n"	
		"\t-help [Universe|State]\tDisplay this screen, JobUniverses, JobStates\n"
		"\t-hold\t\t\tGet information about jobs on hold\n"
		"\t-io\t\t\tDisplay information regarding I/O\n"
		"\t-batch\t\t\tDisplay DAGs or batches of similar jobs as a single line\n"
		"\t-nobatch\t\tDisplay one line per job, rather than one line per batch\n"
//FUTURE		"\t-transfer\t\tDisplay information for jobs that are doing file transfer\n"
		"\t-run\t\t\tGet information about running jobs\n"
		"\t-totals\t\t\tDisplay only job totals\n"
		"\t-stream-results \tProduce output as jobs are fetched\n"
		"\t-version\t\tPrint the HTCondor version and exit\n"
		"\t-wide[:<width>]\t\tDon't truncate data to fit in 80 columns.\n"
		"\t\t\t\tTruncates to console width or <width> argument.\n"
		"\t-autoformat[:jlhVr,tng] <attr> [<attr2> [...]]\n"
		"\t-af[:jlhVr,tng] <attr> [attr2 [...]]\n"
		"\t    Print attr(s) with automatic formatting\n"
		"\t    the [jlhVr,tng] options modify the formatting\n"
		"\t        j   Display Job id\n"
		"\t        l   attribute labels\n"
		"\t        h   attribute column headings\n"
		"\t        V   %%V formatting (string values are quoted)\n"
		"\t        r   %%r formatting (raw/unparsed values)\n"
		"\t        t   tab before each value (default is space)\n"
		"\t        g   newline between ClassAds, no space before values\n"
		"\t        ,   comma after each value\n"
		"\t        n   newline after each value\n"
		"\t    use -af:h to get tabular values with headings\n"
		"\t    use -af:lrng to get -long equivalant format\n"
		"\t-format <fmt> <attr>\tPrint attribute attr using format fmt\n"
		"\t-print-format <file>\tUse <file> to set display attributes and formatting\n"
		"\t\t\t\t(experimental, see htcondor-wiki for more information)\n"
		"\t-long\t\t\tDisplay entire ClassAds\n"
		"\t-xml\t\t\tDisplay entire ClassAds, but in XML\n"
		"\t-attributes X,Y,...\tAttributes to show in -xml and -long\n"
		);

	printf ("\n    [analyze-opts] are\n"
		"\t-analyze[:<qual>]\tPerform matchmaking analysis on jobs\n"
		"\t-better-analyze[:<qual>]\tPerform more detailed match analysis\n"
		"\t    <qual> is a comma separated list of one or more of\n"
		"\t    priority\tConsider user priority during analysis\n"
		"\t    summary\tShow a one-line summary for each job or machine\n"
		"\t    reverse\tAnalyze machines rather than jobs\n"
		"\t-machine <name>\t\tMachine name or slot name for analysis\n"
		"\t-mconstraint <expr>\tMachine constraint for analysis\n"
		"\t-slotads <file>\t\tRead Machine ClassAds for analysis from <file>\n"
		"\t\t\t\t<file> can be the output of condor_status -long\n"
		"\t-userprios <file>\tRead user priorities for analysis from <file>\n"
		"\t\t\t\t<file> can be the output of condor_userprio -l\n"
		"\t-nouserprios\t\tDon't consider user priority during analysis\n"
		"\t-reverse\t\tAnalyze Machine requirements against jobs\n"
		"\t-verbose\t\tShow progress and machine names in results\n"
		"\n"
		);

	printf ("\n    Only information about jobs owned by the current user will be returned.\n"
			"This default is overridden when the restriction list has usernames and/or\n"
			"job ids, when the -submitter or -allusers arguments are specified, or\n"
			"when the current user is a queue superuser\n"
		"\n"
		);

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
}

static void
print_full_footer()
{
	// If we want to summarize, do that too.
	if( summarize ) {
		printf( "\n%d jobs; "
				"%d completed, %d removed, %d idle, %d running, %d held, %d suspended",
				idle+running+held+malformed+suspended+completed+removed,
				completed,removed,idle,running,held,suspended);
		if (malformed>0) printf( ", %d malformed",malformed);
		printf("\n");
	}
	if( use_xml ) {
			// keep this consistent with AttrListList::fPrintAttrListList()
		std::string xml;
		AddClassAdXMLFileFooter(xml);
		printf("%s\n", xml.c_str());
	}
}

static void
print_full_header(const char * source_label)
{
	if ( ! dash_long) {
		// print the source label.
		if ( ! (customHeadFoot&HF_NOTITLE)) {
			static bool first_time = false;
			printf ("%s-- %s\n", (first_time ? "" : "\n\n"), source_label);
			first_time = false;
		}
		if ( ! (customHeadFoot&HF_NOHEADER)) {
			// Print the output header
#ifdef USE_LATE_PROJECTION
			if (app.mask.has_headings()) {
				app.mask.display_Headings(stdout);
			}
#else
			if (usingPrintMask && mask.has_headings()) {
				mask.display_Headings(stdout);
			} else if ( ! better_analyze) {
				short_header();
			}
#endif
		}
	}
	if( use_xml ) {
			// keep this consistent with AttrListList::fPrintAttrListList()
		std::string xml;
		AddClassAdXMLFileHeader(xml);
		printf("%s\n", xml.c_str());
	}
}

#ifdef ALLOW_DASH_DAG

static void
edit_string(clusterProcString *cps)
{
	if(!cps->parent) {
		return;	// Nothing to do
	}
	std::string s;
	int generations = 0;
	for( clusterProcString* ccps = cps->parent; ccps; ccps = ccps->parent ) {
		++generations;
	}
	int state = 0;
	for(const char* p = cps->string; *p;){
		switch(state) {
		case 0:
			if(!isspace(*p)){
				state = 1;
			} else {
				s += *p;
				++p;
			}
			break;
		case 1:
			if(isspace(*p)){
				state = 2;
			} else {
				s += *p;
				++p;
			}
			break;
		case 2: if(isspace(*p)){
				s+=*p;
				++p;
			} else {
				for(int i=0;i<generations;++i){
					s+=' ';
				}
				s +="|-";
				state = 3;
			}
			break;
		case 3:
			if(isspace(*p)){
				state = 4;
			} else {
				s += *p;
				++p;	
			}
			break;
		case 4:
			int gen_i;
			for(gen_i=0;gen_i<=generations+1;++gen_i){
				if(isspace(*p)){
					++p;
				} else {
					break;
				}
			}
			if( gen_i < generations || !isspace(*p) ) {
				std::string::iterator sp = s.end();
				--sp;
				*sp = ' ';
			}
			state = 5;
			break;
		case 5:
			s += *p;
			++p;
			break;
		}
	}
	char* cpss = cps->string;
	cps->string = strnewp( s.c_str() );
	delete[] cpss;
}

static void
rewrite_output_for_dag_nodes_in_dag_map()
{
	for(dag_map_type::iterator cps = dag_map.begin(); cps != dag_map.end(); ++cps) {
		edit_string(cps->second);
	}
}

// First Pass: Find all the DAGman nodes, and link them up
// Assumptions of this code: DAG Parent nodes always
// have cluster ids that are less than those of child
// nodes. condor_dagman processes have ProcID == 0 (only one per
// cluster)
static void
linkup_dag_nodes_in_dag_map()
{
	for(dag_map_type::iterator cps = dag_map.begin(); cps != dag_map.end(); ++cps) {
		clusterProcString* cpps = cps->second;
		if(cpps->dagman_cluster_id != cpps->cluster) {
				// Its probably a DAGman job
				// This next line is where we assume all condor_dagman
				// jobs have ProcID == 0
			clusterProcMapper parent_id(cpps->dagman_cluster_id);
			dag_map_type::iterator dmp = dag_map.find(parent_id);
			if( dmp != dag_map.end() ) { // found it!
				cpps->parent = dmp->second;
				dmp->second->children.push_back(cpps);
			} else { // Now search dag_cluster_map
					// Necessary to find children of dags
					// which are themselves children (that is,
					// subdags)
				clusterIDProcIDMapper cipim(cpps->dagman_cluster_id);
				dag_cluster_map_type::iterator dcmti = dag_cluster_map.find(cipim);
				if(dcmti != dag_cluster_map.end() ) {
					cpps->parent = dcmti->second;
					dcmti->second->children.push_back(cpps);
				}
			}
		}
	}
}

static void print_dag_map_node(clusterProcString* cps,int level)
{
	if(cps->parent && level <= 0) {
		return;
	}
	printf("%s",cps->string);
	for(std::vector<clusterProcString*>::iterator p = cps->children.begin();
			p != cps->children.end(); ++p) {
		print_dag_map_node(*p,level+1);
	}
}

static void print_dag_map()
{
	for(dag_map_type::iterator cps = dag_map.begin(); cps != dag_map.end(); ++cps) {
		print_dag_map_node(cps->second,0);
	}
}

static void clear_dag_map()
{
	for(std::map<clusterProcMapper,clusterProcString*,CompareProcMaps>::iterator
			cps = dag_map.begin(); cps != dag_map.end(); ++cps) {
		delete[] cps->second->string;
		delete cps->second;
	}
	dag_map.clear();
	dag_cluster_map.clear();
}
#endif // ALLOW_DASH_DAG

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

#ifdef USE_LATE_PROJECTION

extern const char * const jobDefault_PrintFormat;
extern const char * const jobRuntime_PrintFormat;
extern const char * const jobGoodput_PrintFormat;
extern const char * const jobGlobus_PrintFormat;
extern const char * const jobGrid_PrintFormat;
extern const char * const jobHold_PrintFormat;
extern const char * const jobIO_PrintFormat;
extern const char * const jobDAG_PrintFormat;
extern const char * const jobTotals_PrintFormat;
extern const char * const jobProgress_PrintFormat; // NEW, summarize batch progress
extern const char * const autoclusterNormal_PrintFormat;

static void initOutputMask(AttrListPrintMask & prmask, int qdo_mode, bool wide_mode)
{
	// just  in case we re-enter this function, only setup the mask once
	static bool	setup_mask = false;
	// TJ: before my 2012 refactoring setup_mask didn't protect summarize, so I'm preserving that.
	if ( dash_run || dash_goodput || dash_globus || dash_grid ) 
		summarize = false;
	else if ((customHeadFoot&HF_NOSUMMARY) && ! show_held)
		summarize = false;

	if (setup_mask)
		return;
	setup_mask = true;

	// If no display mode has been set, pick one.
	if ((qdo_mode & QDO_BaseMask) == QDO_NotSet) {
		if (dash_autocluster == CondorQ::fetch_DefaultAutoCluster) { qdo_mode = QDO_AutoclusterNormal; }
		else { qdo_mode |= QDO_JobNormal; }
	}

	static const struct {
		int mode;
		const char * tag;
		const char * fmt;
	} info[] = {
		{ QDO_JobNormal,    "",       jobDefault_PrintFormat },
		{ QDO_JobRuntime,    "RUN",    jobRuntime_PrintFormat },
		{ QDO_JobGoodput,    "GOODPUT",jobGoodput_PrintFormat },
		{ QDO_JobGlobusInfo, "GLOBUS", jobGlobus_PrintFormat },
		{ QDO_JobGridInfo,   "GRID",   jobGrid_PrintFormat },
		{ QDO_JobHold,       "HOLD",   jobHold_PrintFormat },
		{ QDO_JobIO,         "IO",	   jobIO_PrintFormat },
		{ QDO_DAG,           "DAG",	   jobDAG_PrintFormat },
		{ QDO_Totals,        "TOTALS", jobTotals_PrintFormat },
		{ QDO_Progress,      "PROGRESS", jobProgress_PrintFormat },
		{ QDO_AutoclusterNormal, "AUTOCLUSTER", autoclusterNormal_PrintFormat },
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
		MyString param_name("Q_DEFAULT_");
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
			} else if (set_print_mask_from_stream(prmask, pf_file.ptr(), true, app.attrs) < 0) {
				fprintf(stderr, "Warning: default %s print-format file '%s' is invalid\n", tag, pf_file.ptr());
			} else {
				if (customHeadFoot&HF_NOSUMMARY) summarize = false;
				return;
			}
		}
	}

	const char * fmt = info[ixInfo].fmt;
	std::string alt_fmt;
	if (cputime) {
		alt_fmt = fmt;
		size_t ix = alt_fmt.find("RUN_TIME");
		if (ix != string::npos) {
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

	if (set_print_mask_from_stream(prmask, fmt, false, app.attrs) < 0) {
		fprintf(stderr, "Internal error: default %s print-format is invalid !\n", tag);
	}
	customHeadFoot = (printmask_headerfooter_t)(customHeadFoot & ~HF_CUSTOM);
	if (customHeadFoot&HF_NOSUMMARY) summarize = false;
}

#else

static bool init_user_override_output_mask()
{
	// if someone already setup a print mask, then don't use an override file.
	if (usingPrintMask || disable_user_print_files)
		return false;

	bool using_override = false;
	MyString param_name("Q_DEFAULT_");
	const char * pdash = "";
	if (dash_run) { pdash = "RUN"; }
	else if (dash_goodput) { pdash = "GOODPUT"; }
	else if (dash_globus) { pdash = "GLOBUS"; }
	else if (dash_grid) { pdash = "GRID"; }
	else if (show_held) { pdash = "HOLD"; }
	if (pdash[0]) {
		param_name += pdash;
		param_name += "_";
	}
	param_name += "PRINT_FORMAT_FILE";
	char * pf_file = param(param_name.c_str());
	if (pf_file) {
		struct stat stat_buff;
		if (0 != stat(pf_file, &stat_buff)) {
			// do nothing, this is not an error.
		} else if (set_print_mask_from_stream(mask, pf_file, true, attrs) < 0) {
			fprintf(stderr, "Warning: default %s print-format file '%s' is invalid\n", pdash, pf_file);
		} else {
			usingPrintMask = true;
			using_override = true;
			if (customHeadFoot&HF_NOSUMMARY) summarize = false;
		}
		free(pf_file);
	}
	return using_override;
}

static void init_output_mask()
{
	// just  in case we re-enter this function, only setup the mask once
	static bool	setup_mask = false;
	// TJ: before my 2012 refactoring setup_mask didn't protect summarize, so I'm preserving that.
	if ( dash_run || dash_goodput || dash_globus || dash_grid ) 
		summarize = false;
	else if ((customHeadFoot&HF_NOSUMMARY) && ! show_held)
		summarize = false;

	if (setup_mask)
		return;
	setup_mask = true;

	// if there is a user-override output mask, then use that instead of the code below
	if (init_user_override_output_mask()) {
		return;
	}

	int console_width = getDisplayWidth();

	const char * mask_headings = NULL;

	if (dash_run || dash_goodput) {
		//mask.SetAutoSep("<","{","},",">\n"); // for testing.
		mask.SetAutoSep(NULL," ",NULL,"\n");
		mask.registerFormat ("%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
		mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix, ATTR_PROC_ID);
		mask.registerFormat (NULL, -14, AltQuestion | AltWide, format_owner_wide, ATTR_OWNER /*, "[????????????]"*/ );
		//mask.registerFormat(" ", "*bogus*", " ");  // force space
		mask.registerFormat (NULL,  11, AltQuestion | AltWide, format_q_date, ATTR_Q_DATE /*, "[????????????]"*/ );
		//mask.registerFormat(" ", "*bogus*", " ");  // force space
		mask.registerFormat (NULL,  12, AltQuestion | AltWide, format_cpu_time,
							 ATTR_JOB_REMOTE_USER_CPU /*, "[??????????]"*/);
		if (dash_run && ! dash_goodput) {
			mask_headings = (cputime) ? " ID\0 \0OWNER\0  SUBMITTED\0    CPU_TIME\0HOST(S)\0"
			                          : " ID\0 \0OWNER\0  SUBMITTED\0    RUN_TIME\0HOST(S)\0";
			//mask.registerFormat(" ", "*bogus*", " "); // force space
			// We send in ATTR_OWNER since we know it is always
			// defined, and we need to make sure
			// format_remote_host() is always called. We are
			// actually displaying ATTR_REMOTE_HOST if defined,
			// but we play some tricks if it isn't defined.
			mask.registerFormat ( NULL, 0, AltQuestion | AltWide, format_remote_host,
								  ATTR_OWNER /*, "[????????????????]"*/);
		} else {			// goodput
			mask_headings = (cputime) ? " ID\0 \0OWNER\0  SUBMITTED\0    CPU_TIME\0GOODPUT\0CPU_UTIL\0Mb/s\0"
			                          : " ID\0 \0OWNER\0  SUBMITTED\0    RUN_TIME\0GOODPUT\0CPU_UTIL\0Mb/s\0";
			mask.registerFormat (NULL, 8, AltQuestion | AltWide, format_goodput,
								 ATTR_JOB_STATUS /*, "[?????]"*/);
			mask.registerFormat (NULL, 9, AltQuestion | AltWide, format_cpu_util,
								 ATTR_JOB_REMOTE_USER_CPU /*, "[??????]"*/);
			mask.registerFormat (NULL, 7, AltQuestion | AltWide, format_mbps,
								 ATTR_BYTES_SENT /*, "[????]"*/);
		}
		//mask.registerFormat("\n", "*bogus*", "\n");  // force newline
		usingPrintMask = true;
	} else if (dash_globus) {
		mask_headings = " ID\0 \0OWNER\0STATUS\0MANAGER     HOST\0EXECUTABLE\0";
		mask.SetAutoSep(NULL," ",NULL,"\n");
		mask.registerFormat ("%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
		mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix,  ATTR_PROC_ID);
		mask.registerFormat (NULL, -14, AltQuestion | AltWide, format_owner_wide, ATTR_OWNER /*, "[?]"*/ );
		mask.registerFormat(NULL, -8, AltQuestion | AltWide, format_globusStatus, ATTR_GLOBUS_STATUS /*, "[?]"*/ );
		if (widescreen) {
			mask.registerFormat(NULL, -30,  FormatOptionAlwaysCall | FormatOptionAutoWidth | FormatOptionNoTruncate,
				format_globusHostAndJM, ATTR_GRID_RESOURCE);
			mask.registerFormat("%v", -18, FormatOptionAutoWidth | FormatOptionNoTruncate, ATTR_JOB_CMD );
		} else {
			mask.registerFormat(NULL, 30, FormatOptionAlwaysCall,
				format_globusHostAndJM, ATTR_JOB_CMD);
			mask.registerFormat("%-18.18s", ATTR_JOB_CMD);
		}
		usingPrintMask = true;
	} else if( dash_grid ) {
		mask_headings = " ID\0 \0OWNER\0STATUS\0GRID->MANAGER    HOST\0GRID_JOB_ID\0";
		//mask.SetAutoSep("<","{","},",">\n"); // for testing.
		mask.SetAutoSep(NULL," ",NULL,"\n");
		mask.registerFormat ("%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
		mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix, ATTR_PROC_ID);
		if (widescreen) {
			int excess = (console_width - (8+1 + 14+1 + 10+1 + 27+1 + 16));
			int w2 = 27 + MAX(0, (excess-8)/3);
			mask.registerFormat (NULL, -14, FormatOptionAutoWidth | FormatOptionNoTruncate, format_owner_wide, ATTR_OWNER);
			mask.registerFormat (NULL, -10, FormatOptionAutoWidth | FormatOptionNoTruncate, format_gridStatus, ATTR_JOB_STATUS);
			mask.registerFormat (NULL, -w2, FormatOptionAutoWidth | FormatOptionNoTruncate, format_gridResource, ATTR_GRID_RESOURCE);
			mask.registerFormat (NULL, 0, FormatOptionNoTruncate, format_gridJobId, ATTR_GRID_JOB_ID);
		} else {
			mask.registerFormat (NULL, -14, 0, format_owner_wide, ATTR_OWNER);
			mask.registerFormat (NULL, -10,0, format_gridStatus, ATTR_JOB_STATUS);
			mask.registerFormat ("%-27.27s", 0,0, format_gridResource, ATTR_GRID_RESOURCE);
			mask.registerFormat ("%-16.16s", 0,0, format_gridJobId, ATTR_GRID_JOB_ID);
		}
		usingPrintMask = true;
	} else if ( show_held ) {
		mask_headings = " ID\0 \0OWNER\0HELD_SINCE\0HOLD_REASON\0";
		mask.SetAutoSep(NULL," ",NULL,"\n");
		mask.registerFormat ("%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
		mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix, ATTR_PROC_ID);
		mask.registerFormat (NULL, -14, AltQuestion | AltWide, format_owner_wide, ATTR_OWNER /*, "[????????????]"*/ );
		mask.registerFormat (NULL, 11, AltQuestion | AltWide, format_q_date,
							  ATTR_ENTERED_CURRENT_STATUS /*, "[????????????]"*/);
		if (widescreen) {
			mask.registerFormat("%v", -43, FormatOptionAutoWidth | FormatOptionNoTruncate, ATTR_HOLD_REASON );
		} else {
			mask.registerFormat("%-43.43s", ATTR_HOLD_REASON);
		}
		usingPrintMask = true;
	} else if (dash_autocluster && ! usingPrintMask) {
		mask_headings = " AUTO_ID\0JOB_COUNT\0";
		mask.SetAutoSep(NULL, " ", NULL, "\n");
		mask.registerFormat("%8d", 8, FormatOptionAutoWidth, ATTR_AUTO_CLUSTER_ID);
		mask.registerFormat("%9d", 9, FormatOptionAutoWidth, "JobCount");
		usingPrintMask = true;
		summarize = false;
		customHeadFoot = HF_NOSUMMARY;
	}

	if (mask_headings) {
		const char * pszz = mask_headings;
		size_t cch = strlen(pszz);
		while (cch > 0) {
			mask.set_heading(pszz);
			pszz += cch+1;
			cch = strlen(pszz);
		}
	}
}
#endif


// Given a list of jobs, do analysis for each job and print out the results.
//
static bool
print_jobs_analysis(ClassAdList & jobs, const char * source_label, Daemon * pschedd_daemon)
{
	// note: pschedd_daemon may be NULL.

		// check if job is being analyzed
	ASSERT( better_analyze );

	// build a job autocluster map
	if (verbose) { fprintf(stderr, "\nBuilding autocluster map of %d Jobs...", jobs.Length()); }
	JobClusterMap job_autoclusters;
	buildJobClusterMap(jobs, ATTR_AUTO_CLUSTER_ID, job_autoclusters);
	size_t cNoAuto = job_autoclusters[-1].size();
	size_t cAuto = job_autoclusters.size()-1; // -1 because size() counts the entry at [-1]
	if (verbose) { fprintf(stderr, "%d autoclusters and %d Jobs with no autocluster\n", (int)cAuto, (int)cNoAuto); }

	if (reverse_analyze) { 
		if (verbose && (startdAds.Length() > 1)) { fprintf(stderr, "Sorting %d Slots...", startdAds.Length()); }
		longest_slot_machine_name = 0;
		longest_slot_name = 0;
		if (startdAds.Length() == 1) {
			// if there is a single machine ad, then default to analysis
			if ( ! analyze_detail_level) analyze_detail_level = detail_analyze_each_sub_expr;
		} else {
			startdAds.Sort(SlotSort);
		}
		//if (verbose) { fprintf(stderr, "Done, longest machine name %d, longest slot name %d\n", longest_slot_machine_name, longest_slot_name); }
	} else {
		if (analyze_detail_level & detail_better) {
			analyze_detail_level |= detail_analyze_each_sub_expr | detail_always_analyze_req;
		}
	}

	// print banner for this source of jobs.
	printf ("\n\n-- %s\n", source_label);

	if (reverse_analyze) {
		int console_width = widescreen ? getDisplayWidth() : 80;
		if (startdAds.Length() <= 0) {
			// print something out?
		} else {
			bool analStartExpr = /*(better_analyze == 2) || */(analyze_detail_level > 0);
			if (summarize_anal || ! analStartExpr) {
				if (jobs.Length() <= 1) {
					jobs.Open();
					while (ClassAd *job = jobs.Next()) {
						int cluster_id = 0, proc_id = 0;
						job->LookupInteger(ATTR_CLUSTER_ID, cluster_id);
						job->LookupInteger(ATTR_PROC_ID, proc_id);
						printf("%d.%d: Analyzing matches for 1 job\n", cluster_id, proc_id);
					}
					jobs.Close();
				} else {
					int cNoAutoT = (int)job_autoclusters[-1].size();
					int cAutoclustersT = (int)job_autoclusters.size()-1; // -1 because [-1] is not actually an autocluster
					if (verbose) {
						printf("Analyzing %d jobs in %d autoclusters\n", jobs.Length(), cAutoclustersT+cNoAutoT);
					} else {
						printf("Analyzing matches for %d jobs\n", jobs.Length());
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
			startdAds.Open();
			while (ClassAd *slot = startdAds.Next()) {
				doSlotRunAnalysis(slot, job_autoclusters, pschedd_daemon, console_width);
			}
			startdAds.Close();
		}
	} else {
		if (summarize_anal) {
			if (single_machine) {
				printf("%s: Analyzing matches for %d slots\n", user_slot_constraint, startdAds.Length());
			} else {
				printf("Analyzing matches for %d slots\n", startdAds.Length());
			}
			const char * fmt = "%-13s %12s %12s %11s %11s %10s %9s %s\n";
			printf(fmt, "",              " Autocluster", "   Matches  ", "  Machine  ", "  Running  ", "  Serving ", "", "");
			printf(fmt, " JobId",        "Members/Idle", "Requirements", "Rejects Job", " Users Job ", "Other User", "Available", summarize_with_owner ? "Owner" : "");
			printf(fmt, "-------------", "------------", "------------", "-----------", "-----------", "----------", "---------", summarize_with_owner ? "-----" : "");
		}

		for (JobClusterMap::iterator it = job_autoclusters.begin(); it != job_autoclusters.end(); ++it) {
			int cJobsInCluster = (int)it->second.size();
			if (cJobsInCluster <= 0)
				continue;

			// for the the non-autocluster cluster, we have to eval these jobs individually
			int cJobsToEval = (it->first == -1) ? cJobsInCluster : 1;
			int cJobsToInc  = (it->first == -1) ? 1 : cJobsInCluster;
			for (int ii = 0; ii < cJobsToEval; ++ii) {
				ClassAd *job = it->second[ii];
				if (summarize_anal) {
					char achJobId[16], achAutocluster[16], achRunning[16];
					int cluster_id = 0, proc_id = 0;
					job->LookupInteger(ATTR_CLUSTER_ID, cluster_id);
					job->LookupInteger(ATTR_PROC_ID, proc_id);
					sprintf(achJobId, "%d.%d", cluster_id, proc_id);

					string owner;
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
							sprintf(achAutocluster, "%d:%d/%d", it->first, cJobsToInc, cIdle);
						} else {
							sprintf(achAutocluster, "%d/%d", cJobsToInc, cIdle);
						}
					} else {
						achAutocluster[0] = 0;
					}

					anaCounters ac;
					doJobRunAnalysisToBuffer(job, ac, detail_always_analyze_req, true, false);
					const char * fmt = "%-13s %-12s %12d %11d %11s %10d %9d %s\n";

					achRunning[0] = 0;
					if (cRunning) { sprintf(achRunning, "%d/", cRunning); }
					sprintf(achRunning+strlen(achRunning), "%d", ac.machinesRunningUsersJobs);

					printf(fmt, achJobId, achAutocluster,
							ac.totalMachines - ac.fReqConstraint,
							ac.fOffConstraint,
							achRunning,
							ac.machinesRunningJobs - ac.machinesRunningUsersJobs,
							ac.available,
							owner.c_str());
				} else {
					doJobRunAnalysis(job, pschedd_daemon, analyze_detail_level);
				}
			}
		}
	}

	return true;
}

// callback function for processing a job from the Q query that just adds the 
// job into a ClassAdList.
static bool
AddToClassAdList(void * pv, ClassAd* ad) {
	ClassAdList * plist = (ClassAdList*)pv;
	plist->Insert(ad);
	return false; // return false to indicate we took ownership of the ad.
}

#ifdef HAVE_EXT_POSTGRESQL
static bool
show_db_queue( const char* quill_name, const char* db_ipAddr, const char* db_name, const char* query_password)
{
		// initialize counters
	idle = running = held = malformed = suspended = completed = removed = 0;

	ClassAdList jobs;
	CondorError errstack;

	std::string source_label;
	formatstr(source_label, "Quill: %s : %s : %s : ", quill_name, db_ipAddr, db_name);

	// choose a processing option for jobad's as the come off the wire.
	// for -long -xml and -analyze, we need to save off the ad in a ClassAdList
	// for -stream we print out the ad 
	// and for everything else, we 
	//
	buffer_line_processor pfnProcess;
	void *                pvProcess;
	if (dash_long || better_analyze) {
		pfnProcess = AddToClassAdList;
		pvProcess = &jobs;
	} else if (g_stream_results) {
		pfnProcess = process_and_print_job;
		pvProcess = NULL;

		// we are about to print out the jobads, so print an output header now.
		print_full_header(source_label.c_str());
	} else {
		// tj: 2013 refactor---
		// so we can compare the output of old vs. new code. keep both around for now.
		pfnProcess = /*use_old_code ? process_buffer_line_old : */ process_job_and_render_to_dag_map;
		pvProcess = &dag_map;
	}

	// fetch queue directly from the database

#ifdef HAVE_EXT_POSTGRESQL

		char *lastUpdate=NULL;
		char *dbconn=NULL;
		dbconn = getDBConnStr(quill_name, db_ipAddr, db_name, query_password);

		if( Q.fetchQueueFromDBAndProcess( dbconn,
											lastUpdate,
											pfnProcess, pvProcess,
											&errstack) != Q_OK ) {
			fprintf( stderr, 
					"\n-- Failed to fetch ads from db [%s] at database "
					"server %s\n%s\n",
					db_name, db_ipAddr, errstack.getFullText(true).c_str() );

			if(dbconn) {
				free(dbconn);
			}
			if(lastUpdate) {
				free(lastUpdate);
			}
			return false;
		}
		if(dbconn) {
			free(dbconn);
		}
		if(lastUpdate) {
			source_label += lastUpdate;
			free(lastUpdate);
		}
#else
		if (query_password) {} /* Done to suppress set-but-not-used warnings */
#endif /* HAVE_EXT_POSTGRESQL */
	if (pfnProcess || pvProcess) {}

	// if we streamed, then the results have already been printed out.
	// we just need to write the footer/summary
	if (g_stream_results) {
		print_full_footer();
		return true;
	}

	if (better_analyze) {
		return print_jobs_analysis(jobs, source_label.c_str(), NULL);
	}

	// at this point we either have a populated jobs list, or a populated dag_map
	// depending on which processing function we used in the query above.
	// for -long -xml or -analyze, we should have jobs, but no dag_map

	// if outputing dag format, we want to build the parent child links in the dag map
	// and then edit the text for jobs that are dag nodes.
	if (dash_dag && (dag_map.size() > 0)) {
		linkup_dag_nodes_in_dag_map(/*dag_map, dag_cluster_map*/);
		rewrite_output_for_dag_nodes_in_dag_map(/*dag_map*/);
	}

	// we want a header for this schedd if we are not showing the global queue OR if there is any data
	if ( ! global || dag_map.size() > 0 || jobs.Length() > 0) {
		print_full_header(source_label.c_str());
	}

	// print out jobs, either from the dag_map, or the jobs list, we expect only
	// one of these to be populated depending on which processing function we passed to the query
	if (dag_map.size() > 0) {
		print_dag_map(/*dag_map*/);
		// free the strings in the dag_map
		clear_dag_map(/*dag_map, dag_cluster_map*/);
	} else {
		jobs.Open();
		while(ClassAd *job = jobs.Next()) {
			process_and_print_job(NULL, job);
		}
		jobs.Close();
	}

	// print out summary line (if enabled) and/or xml closure
	// we want to print out a footer if we printed any data, OR if this is not 
	// a global query, so that the user can see that we did something.
	//
	if ( ! global || dag_map.size() > 0 || jobs.Length() > 0) {
		print_full_footer();
	}

	return true;
}
#endif // HAVE_EXT_POSTGRESQL


static void count_job(ClassAd *job)
{
	int status = 0;
	job->LookupInteger(ATTR_JOB_STATUS, status);
	switch (status)
	{
		case IDLE:                ++idle;      break;
		case TRANSFERRING_OUTPUT:
		case RUNNING:             ++running;   break;
		case SUSPENDED:           ++suspended; break;
		case COMPLETED:           ++completed; break;
		case REMOVED:             ++removed;   break;
		case HELD:                ++held;      break;
	}
}

#ifdef USE_LATE_PROJECTION
#else
static const char * render_job_text(ClassAd *job, std::string & result_text)
{
	if (use_xml) {
		sPrintAdAsXML(result_text, *job, attrs.isEmpty() ? NULL : &attrs);
	} else if (dash_long) {
		sPrintAd(result_text, *job, false, (dash_autocluster || attrs.isEmpty()) ? NULL : &attrs);
		result_text += "\n";
	} else if (better_analyze) {
		ASSERT(0); // should never get here.
	} else if (show_io) {
		buffer_io_display(result_text, job);
	} else if (usingPrintMask) {
		if (mask.IsEmpty()) return NULL;
		mask.display(result_text, job);
	} else {
		result_text += bufferJobShort(job);
	}
	return result_text.c_str();
}
#endif

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
			if (job->LookupString(group_by_keys[ii].expr.c_str(), value)) {
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

union _jobid {
	struct { int proc; int cluster; };
	long long id;
};

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
	} else if (job->LookupString(ATTR_JOB_CMD, name)) {
		name.insert(0, "CMD: ");
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

static bool process_job_to_rod_per_ad_map(void * pv,  ClassAd* job)
{
	ROD_MAP_BY_ID * pmap = (ROD_MAP_BY_ID *)pv;
	count_job(job);

	ASSERT( ! g_stream_results);

	union _jobid jobid;

	if (dash_autocluster) {
		const char * attr_id = ATTR_AUTO_CLUSTER_ID;
		if (dash_autocluster == CondorQ::fetch_GroupBy) attr_id = "Id";
		job->LookupInteger(attr_id, jobid.id);
	} else {
		job->LookupInteger( ATTR_CLUSTER_ID, jobid.cluster );
		job->LookupInteger( ATTR_PROC_ID, jobid.proc );
	}

	int columns = app.mask.ColCount();
	if (0 == columns)
		return true;

	std::pair<ROD_MAP_BY_ID::iterator,bool> pp = pmap->insert(std::pair<long long, JobRowOfData>(jobid.id,jobid.id));
	if( ! pp.second ) {
		fprintf( stderr, "Error: Two results with the same ID.\n" );
		//tj: 2013 -don't abort without printing jobs
		// exit( 1 );
		return false;
	} else {
		pp.first->second.id = jobid.id;
		pp.first->second.rov.SetMaxCols(columns);
		app.mask.render(pp.first->second.rov, job);

		// if displaying jobs in dag order, also add this job to the set of clusters for this dagid
		if ((dash_dag || dash_progress) && ! dash_autocluster) {
			int dag_cluster = lookup_dagman_job_id(job); // this is actually the dagman cluster id
			if (dag_cluster > 0) {
				union _jobid dag_id;
				dag_id.cluster = dag_cluster; dag_id.proc = 0;
				pp.first->second.dag_id = dag_id.id;
				pp.first->second.flags |= JROD_ISDAGNODE;
				//dag_to_cluster_map[dagid].insert(jobid.cluster);
			} else if (dash_progress) {
				resolve_job_batch_uid(pp.first->second, job);
			}
			int universe = CONDOR_UNIVERSE_MIN;
			if (job->LookupInteger(ATTR_JOB_UNIVERSE, universe) && universe == CONDOR_UNIVERSE_SCHEDULER) {
				pp.first->second.flags |= JROD_SCHEDUNIV;
			}
		}
		group_job(pp.first->second, job);
	}
	return true;
}

static void print_a_result(std::string & buf, JobRowOfData & jrod)
{
	buf.clear();
	app.mask.display(buf, jrod.rov);
	printf("%s", buf.c_str());
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
	if (dash_progress) {
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

static bool
streaming_print_job(void *, ClassAd *job)
{
	count_job(job);

	std::string result_text;
	const char * fmt = "%s";
	if (use_xml) {
		sPrintAdAsXML(result_text, *job, app.attrs.isEmpty() ? NULL : &app.attrs);
	} else if (dash_long) {
		sPrintAd(result_text, *job, false, (dash_autocluster || app.attrs.isEmpty()) ? NULL : &app.attrs);
		fmt = "%s\n";
	} else {
		if (app.mask.ColCount() > 0) { app.mask.display(result_text, job); }
	}
	if ( ! result_text.empty()) { printf(fmt, result_text.c_str()); }
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
};

// callback used by fixup_std_column_widths
static int fnFixupWidthCallback(void* pv, int index, Formatter * fmt, const char * /*attr*/) {
	struct _fixup_width_values * p = (struct _fixup_width_values *)pv;
	char * pf = const_cast<char*>(fmt->printfFmt);
	if (index == 0) {
		if (p->cluster_width > 4 && p->cluster_width <= 9) {
			if (pf && pf[1] == '4') { pf[1] = '0' + p->cluster_width; fmt->width = p->cluster_width+1; }
		}
	} else if (index == 1) {
		if (p->proc_width > 3 && p->proc_width <= 9) {
			if (pf && pf[2] == '3') { pf[2] = '0' + p->proc_width; fmt->width = p->proc_width; }
		}
	} else if (index == name_column_index) { // owner
		fmt->width = MAX(fmt->width, p->name_width);
		p->name_width = fmt->width; // return the actual width
	} else {
		return -1; // stop iterating
	}
	return 1;
}

// fix width of cluster,proc and name columns for all standard display formats other than -batch
//
static void fixup_std_column_widths(int max_cluster, int max_proc, int longest_name) {
	if ( ! is_standard_format || ( ! first_col_is_job_id && ! has_owner_column))
		return; // nothing to do
	if (max_cluster < 9999 && max_proc < 999 && longest_name <= 14)
		return; // nothing to do.

	struct _fixup_width_values vals;
	memset(&vals, 0, sizeof(vals));

	if (first_col_is_job_id) {
		char buf[20];
		sprintf(buf, "%d", max_cluster);
		vals.cluster_width = strlen(buf);
		sprintf(buf, "%d", max_proc);
		vals.proc_width = strlen(buf);
	}

	if (has_owner_column && ! widescreen && longest_name > 14) {
		vals.name_width = MIN(longest_name, max_name_column_width);
	}

	app.mask.adjust_formats(fnFixupWidthCallback, &vals);
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
	const bool fold_dagman_sibs = dash_progress && (dash_progress & 2);

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

	fixup_std_column_widths(max_cluster, max_proc, max_name);
}

static void
format_name_column_for_dag_nodes(ROD_MAP_BY_ID & results, int name_column, int col_width)
{
	std::string buf;
	for(ROD_MAP_BY_ID::iterator it = results.begin(); it != results.end(); ++it) {
		if (it->second.flags & JROD_ISDAGNODE) {
			const char * name = NULL;
			classad::Value * pcolval = it->second.rov.Column(name_column);
			if (pcolval) pcolval->IsStringValue(name);
			int cch = strlen(name);

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
	ATTR_DAG_NODES_TOTAL     " AS ' TOTAL' WIDTH 6 PRINTF %6d OR _\n"  // Total  // 7
	ATTR_JOB_STATUS          " AS JOB_IDS WIDTH 0 PRINTAS JOB_STATUS\n"     // 8
//	ATTR_JOB_REMOTE_USER_CPU " AS '    RUN_TIME'  WIDTH 12   PRINTAS CPU_TIME\n"
//	ATTR_IMAGE_SIZE          " AS SIZE       WIDTH 4    PRINTAS MEMORY_USAGE\n"
//	ATTR_HOLD_REASON         " AS HOLD_REASON\n"
"SUMMARY STANDARD\n";


class  cluster_progress {
public:
	int count;   // number of rows counted
	int jobs;    // number of job rows (i.e. non-dag rows)
	int dags;    // number of dag rows
	int nodes_total; // accumulation of ATTR_DAG_NODES_TOTAL for dag jobs, and of ProcIds for non-dag jobs
	int nodes_done;  // accumulation of ATTR_DAG_NODES_DONE for dag jobs, and a guess of completed procs for non-dag jobs
	int cluster;
	int min_proc;
	int max_proc;
	int states[JOB_STATUS_MAX+1];
	cluster_progress() : count(0), jobs(0), dags(0), nodes_total(0), nodes_done(0),
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

static void reduce_procs(cluster_progress & prog, JobRowOfData & jr)
{
	// these refer to column indexes in the jobProgress_PrintFormat declaration
	const int ixJobStatusCol = 3;
	const int ixUniverseCol  = 4;
	const int ixDagNodesDone = 6;
	const int ixDagNodesTotal = 7;

	prog.count += 1;

	union _jobid jid; jid.id = jr.id;
	prog.cluster = jid.cluster;
	prog.min_proc = prog.max_proc = jid.proc;

	int universe = CONDOR_UNIVERSE_MIN, status = 0;
	if (jr.getNumber(ixUniverseCol, universe) && universe == CONDOR_UNIVERSE_SCHEDULER) {
		jr.flags |= JROD_SCHEDUNIV;
		prog.dags += 1;
		int dag_done = 0, dag_total = 0;
		if (jr.getNumber(ixDagNodesTotal, dag_total)) { prog.nodes_total += dag_total; }
		if (jr.getNumber(ixDagNodesDone, dag_done)) { prog.nodes_done += dag_done; }
	} else {
		prog.jobs += 1;

		jr.getNumber(ixJobStatusCol, status);
		if (status < 0 || status > JOB_STATUS_MAX) status = 0;
		prog.states[status] += 1;
	}

	JobRowOfData *jrod2 = jr.next_proc;
	while (jrod2) {
		if ( ! (jrod2->flags & (JROD_FOLDED | JROD_SKIP))) {
			prog.count += 1;
			prog.jobs += 1; // on the assumption that dagmen can't be multi proc

			int st;
			jrod2->getNumber(ixJobStatusCol, st);
			if (st < 0 || st > JOB_STATUS_MAX) st = 0;
			prog.states[st] += 1;

			union _jobid jid2; jid2.id = jrod2->id;
			prog.min_proc = MIN(prog.min_proc, jid2.proc);
			prog.max_proc = MAX(prog.min_proc, jid2.proc);

			jrod2->flags |= JROD_FOLDED;
		}
		jrod2 = jrod2->next_proc;
	}

	// for non-dagman jobs, we can extrapolate the total number of jobs and the number of 
	// number of completed jobs by looking at the max procid.
	// by counting procids that have left the queue.
	if ( ! (jr.flags & (JROD_SCHEDUNIV | JROD_ISDAGNODE))) {
		const bool guess_at_jobs_done_per_cluster = (dash_progress & 0x10);
		if (guess_at_jobs_done_per_cluster) {
			int total = prog.max_proc+1;
			int done = total - prog.jobs;
			prog.nodes_total += total;
			prog.nodes_done += done;
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
		if ( ! p->any_held) {
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

	const bool fold_dagman_sibs = dash_progress && (dash_progress & 2);

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

#if 1
		//{ union _jobid jidt; jidt.id = jr.id; fprintf(stderr, "reducing %d.%d\n", jidt.cluster, jidt.proc); }
		child_progress prog;
		int depth = 0;
		reduce_children(prog, &jr, depth);
#else
		child_progress prog;
		reduce_procs(prog, jr);
		if ( ! (jr.flags & JROD_SCHEDUNIV)) { prog.min_cluster = prog.max_cluster = prog.cluster; }
		// also fold up dag child nodes
		if (jr.children) { reduce_children(prog, jr.children); }
#endif
		// now write baked results into this row and mark it as cooked
		jr.flags |= JROD_COOKED;

		// rewrite the values in the counter columns with the values
		// we got by summing all of the peers and children
		int num_done    = prog.nodes_done;
		int num_total   = prog.nodes_total;
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

		MyString tmp;
		union _jobid jid; jid.id = jr.id;
		if (jr.flags & JROD_SCHEDUNIV) {
			if (fold_dagman_sibs && jr.batch_uid && jr.next_sib && jr.getString(ixBatchNameCol, name_width)) {
				wids.batch_name_width = MAX(wids.batch_name_width, name_width);
			} else {
			#if 1 // assume batch name column is already correct.
				jr.getString(ixBatchNameCol, name_width);
				wids.batch_name_width = MAX(wids.batch_name_width, name_width);
			#else
				tmp.formatstr("DAG %d", jid.cluster);
				jr.rov.Column(ixBatchNameCol)->SetStringValue(tmp.c_str());
				jr.rov.set_col_valid(ixBatchNameCol, true);
				wids.batch_name_width = MAX(wids.batch_name_width, (int)tmp.length());
			#endif
			}
		} else {
			if (jr.getString(ixBatchNameCol, name_width)) {
				wids.batch_name_width = MAX(wids.batch_name_width, name_width);
			} else {
				tmp.formatstr("Cluster %d", jid.cluster);
				jr.rov.Column(ixBatchNameCol)->SetStringValue(tmp.c_str());
				jr.rov.set_col_valid(ixBatchNameCol, true);
				wids.batch_name_width = MAX(wids.batch_name_width, (int)tmp.length());
			}
		}

		union _jobid jmin, jmax; jmin.id = prog.min_jobid; jmax.id = prog.max_jobid;
		if (jmin.id != jmax.id) {
			if (jmin.cluster == jmax.cluster) {
				tmp.formatstr("%d.%d-%d", jmin.cluster, jmin.proc, jmax.proc);
			} else {
				tmp.formatstr("%d.%d ... %d.%d", jmin.cluster, jmin.proc, jmax.cluster, jmax.proc);
			}
		} else {
			tmp.formatstr("%d.%d", jmin.cluster, jmin.proc);
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
	app.mask.adjust_formats(fnFixupWidthsForProgressFormat, &wids);
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


// query SCHEDD or QUILLD daemon for jobs. and then print out the desired job info.
// this function handles -analyze, -streaming, -dag and all normal condor_q output
// when the source is a SCHEDD or QUILLD.
// TJ isn't sure that the QUILL daemon can use fast path, prior to the 2013 refactor, 
// the old code didn't try.
//
static bool
show_schedd_queue(const char* scheddAddress, const char* scheddName, const char* scheddMachine, int useFastPath)
{
	// initialize counters
	malformed = idle = running = held = completed = suspended = 0;

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
	ClassAdList jobs;  // this will get filled in for -long -xml and -analyze
	CondorError errstack;

	// choose a processing option for jobad's as the come off the wire.
	// for -long -xml and -analyze, we need to save off the ad in a ClassAdList
	// for -stream we print out the ad 
	// and for everything else, we 
	//
	buffer_line_processor pfnProcess = NULL;
	void *                pvProcess = NULL;
	if (better_analyze || (dash_long && ! g_stream_results)) {
		pfnProcess = AddToClassAdList;
		pvProcess = &jobs;
	} else if (g_stream_results) {
		pfnProcess = streaming_print_job;
		// we are about to print out the jobads, so print an output header now.
		print_full_header(source_label.c_str());
	} else {
		// digest each job into a row of values and insert into the rod_result_map
		pfnProcess = process_job_to_rod_per_ad_map;
		pvProcess = &rod_result_map;
	}

	int fetch_opts = 0;
	if (dash_autocluster) {
		fetch_opts = dash_autocluster;
	}
	bool use_v3 = dash_autocluster || param_boolean("CONDOR_Q_USE_V3_PROTOCOL", true);
	if ((useFastPath == 2) && !use_v3) {
		useFastPath = 1;
	}
	if ( ! fetch_opts && (useFastPath == 2)) {
		fetch_opts = default_fetch_opts;
	}
#ifdef USE_LATE_PROJECTION
	StringList *pattrs = &app.attrs;
#else
	StringList *pattrs = &attrs;
#endif
	int fetchResult;
	if (dash_dry_run) {
		fetchResult = dryFetchQueue(dry_run_file, *pattrs, fetch_opts, g_match_limit, pfnProcess, pvProcess);
	} else {
		fetchResult = Q.fetchQueueFromHostAndProcess(scheddAddress, *pattrs, fetch_opts, g_match_limit, pfnProcess, pvProcess, useFastPath, &errstack);
	}
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

	// if we streamed, then the results have already been printed out.
	// we just need to write the footer/summary
	if (g_stream_results) {
		print_full_footer();
		return true;
	}

	if (dash_profile) {
		profile_print(cbBefore, tmBefore, jobs.Length());
	} else if (verbose) {
		fprintf(stderr, " %d ads\n", jobs.Length());
	}

	if (better_analyze) {
		if (jobs.Length() > 1) {
			if (verbose) { fprintf(stderr, "Sorting %d Jobs...", jobs.Length()); }
			jobs.Sort(JobSort);
			if (verbose) { fprintf(stderr, "\n"); }
		}
		
		//PRAGMA_REMIND("TJ: shouldn't this be using scheddAddress instead of scheddName?")
		Daemon schedd(DT_SCHEDD, scheddName, pool ? pool->addr() : NULL );

		return print_jobs_analysis(jobs, source_label.c_str(), &schedd);
	}

	int cResults = (int)rod_result_map.size();

	// at this point we either have a populated jobs list, or a populated rod_result_map
	// if it's a jobs list, then we want to process the jobs into the rod_result_map
	// then we want to linkup the nodes and (possibly) rewrite the name columns
	if (jobs.Length() > 0) {
		jobs.Open();
		while(ClassAd *job = jobs.Next()) {
			if (dash_long) {
				streaming_print_job(NULL, job);
			} else {
				process_job_to_rod_per_ad_map(&rod_result_map, job);
			}
		}
		jobs.Close();
		cResults = (int)rod_result_map.size();
	}

	if (cResults > 0) {
		linkup_nodes_by_id(rod_result_map);
		if (dash_progress) {
			reduce_results(rod_result_map);
		} else if (dash_dag) {
			format_name_column_for_dag_nodes(rod_result_map, name_column_index, name_column_width);
		}
	}

	// we want a header for this schedd if we are not showing the global queue OR if there is any data
	if ( ! global || cResults > 0 || jobs.Length() > 0) {
		print_full_header(source_label.c_str());
	}

	print_results(rod_result_map, rod_sort_key_map, dash_dag);
	clear_results(rod_result_map, rod_sort_key_map);

	// print out summary line (if enabled) and/or xml closure
	// we want to print out a footer if we printed any data, OR if this is not 
	// a global query, so that the user can see that we did something.
	//
	if ( ! global || cResults > 0 || jobs.Length() > 0) {
		print_full_footer();
	}

	return true;
}

static int
dryFetchQueue(const char * file, StringList & proj, int fetch_opts, int limit, buffer_line_processor pfnProcess, void *pvProcess)
{
	// print header
#ifdef USE_LATE_PROJECTION
	int ver = 2;
#else
	int ver = 1;
#endif
	fprintf(stderr, "\nDryRun v%d from %s\n", ver, file ? file : "<null>");

	// get the constraint as a string
	const char * constr = NULL;
	std::string constr_string; // to hold the return from ExprTreeToString()
	ExprTree *tree = NULL;
	Q.rawQuery(tree);
	if (tree) {
		constr_string = ExprTreeToString(tree);
		constr = constr_string.c_str();
		delete tree;
	}

	// print constraint
	fprintf(stderr, "Constraint: %s\n", constr ? constr : "<null>");
	fprintf(stderr, "Opts: fetch=%d limit=%d HF=%x\n", fetch_opts, limit, customHeadFoot);

	// print projection
	proj.qsort();
	auto_free_ptr projection(proj.print_to_delimed_string("\n  "));
	if (projection.empty()) {
		fprintf(stderr, "Projection: <NULL>\n");
	} else {
		fprintf(stderr, "Projection:\n  %s\n",  projection.ptr());
	}

	// print output mask
	std::string tmp("");
	dump_print_mask(tmp);
	fprintf(stderr, "Mask:\n%s\n", tmp.c_str());

	// 
	if ( ! file || ! file[0]) { 
		return Q_OK;
	}

	ClassAdList jobs;

	/* get the "q" from the job ads file */
	CondorQClassAdFileParseHelper jobads_file_parse_helper;
	if ( ! read_classad_file(file, jobs, &jobads_file_parse_helper, constr)) {
		return Q_COMMUNICATION_ERROR;
	}

	jobs.Open();
	while(ClassAd *job = jobs.Next()) {
		if ( ! pfnProcess(pvProcess, job)) {
			jobs.Remove(job);
		}
	}
	jobs.Close();

	return Q_OK;
}

// Read ads from a file, either in classad format, or userlog format.
//
static bool
show_file_queue(const char* jobads, const char* userlog)
{
	// initialize counters
	malformed = idle = running = held = completed = suspended = 0;

	// setup constraint variables to use in some of the later code.
	const char * constr = NULL;
	std::string constr_string; // to hold the return from ExprTreeToString()
	ExprTree *tree = NULL;
	Q.rawQuery(tree);
	if (tree) {
		constr_string = ExprTreeToString(tree);
		constr = constr_string.c_str();
		delete tree;
	}

	if (verbose || dash_profile) {
		fprintf(stderr, "Fetching job ads...");
	}

	size_t cbBefore = 0;
	double tmBefore = 0;
	if (dash_profile) { cbBefore = ProcAPI::getBasicUsage(getpid(), &tmBefore); }

	ClassAdList jobs;
	std::string source_label;

	if (jobads != NULL) {
		/* get the "q" from the job ads file */
		CondorQClassAdFileParseHelper jobads_file_parse_helper;
		if (!read_classad_file(jobads, jobs, &jobads_file_parse_helper, constr)) {
			return false;
		}
		// if we are doing analysis, print out the schedd name and address
		// that we got from the classad file.
		if (better_analyze && ! jobads_file_parse_helper.schedd_name.empty()) {
			const char *scheddName    = jobads_file_parse_helper.schedd_name.c_str();
			const char *scheddAddress = jobads_file_parse_helper.schedd_addr.c_str();
			const char *queryType     = jobads_file_parse_helper.is_submitter ? "Submitter" : "Schedd";
			formatstr(source_label, "%s: %s : %s", queryType, scheddName, scheddAddress);
		}
		
		/* The variable UseDB should be false in this branch since it was set 
			in main() because jobads_file had a good value. */

	} else if (userlog != NULL) {
		CondorID * JobIds = NULL;
		int cJobIds = constrID.size();
		if (cJobIds > 0) JobIds = &constrID[0];

		if (!userlog_to_classads(userlog, jobs, JobIds, cJobIds, constr)) {
			fprintf(stderr, "\nCan't open user log: %s\n", userlog);
			return false;
		}
		formatstr(source_label, "Userlog: %s", userlog);
	} else {
		ASSERT(jobads != NULL || userlog != NULL);
	}

	if (dash_profile) {
		profile_print(cbBefore, tmBefore, jobs.Length());
	} else if (verbose) {
		fprintf(stderr, " %d ads.\n", jobs.Length());
	}

	if (jobs.Length() > 1) {
		if (verbose) { fprintf(stderr, "Sorting %d Jobs...", jobs.Length()); }
		jobs.Sort(JobSort);

		if (dash_profile) {
			profile_print(cbBefore, tmBefore, jobs.Length(), false);
		} else if (verbose) {
			fprintf(stderr, "\n");
		}
	} else if (verbose) {
		fprintf(stderr, "\n");
	}

	if (better_analyze) {
		return print_jobs_analysis(jobs, source_label.c_str(), NULL);
	}

	// TJ: copied this from the top of init_output_mask
	if ( dash_run || dash_goodput || dash_globus || dash_grid )
		summarize = false;
	else if ((customHeadFoot&HF_NOSUMMARY) && ! show_held)
		summarize = false;

		// display the jobs from this submittor
	if( jobs.MyLength() != 0 || !global ) {

		jobs.Open();
		while(ClassAd *job = jobs.Next()) {
			if (dash_long) {
				streaming_print_job(NULL, job);
			} else {
				process_job_to_rod_per_ad_map(&rod_result_map, job);
			}
		}
		jobs.Close();

		if (dash_long) return true;

		int cResults = (int)rod_result_map.size();
		if (cResults > 0) {
			linkup_nodes_by_id(rod_result_map);
			if (dash_progress) {
				reduce_results(rod_result_map);
			} else if (dash_dag) {
				format_name_column_for_dag_nodes(rod_result_map, name_column_index, name_column_width);
			}
		}

		print_full_header(source_label.c_str());
		print_results(rod_result_map, rod_sort_key_map, dash_dag);
		clear_results(rod_result_map, rod_sort_key_map);

		print_full_footer();
	}

	return true;
}


static bool is_slot_name(const char * psz)
{
	// if string contains only legal characters for a slotname, then assume it's a slotname.
	// legal characters are a-zA-Z@_\.\-
	while (*psz) {
		if (*psz != '-' && *psz != '.' && *psz != '@' && *psz != '_' && ! isalnum(*psz))
			return false;
		++psz;
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
	string		remoteUser;
	int			index;

	
	if (verbose || dash_profile) {
		fprintf(stderr, "Fetching Machine ads...");
	}

	double tmBefore = 0.0;
	size_t cbBefore = 0;
	if (dash_profile) { cbBefore = ProcAPI::getBasicUsage(getpid(), &tmBefore); }

	// fetch startd ads
    if (machineads_file != NULL) {
        if (!read_classad_file(machineads_file, startdAds, NULL, NULL)) {
            exit ( 1 );
        }
    } else {
		if (user_slot_constraint) {
			if (is_slot_name(user_slot_constraint)) {
				std::string str;
				formatstr(str, "(%s==\"%s\") || (%s==\"%s\")",
					      ATTR_NAME, user_slot_constraint, ATTR_MACHINE, user_slot_constraint);
				query.addORConstraint (str.c_str());
				single_machine = true;
			} else {
				query.addANDConstraint (user_slot_constraint);
			}
		}
        rval = Collectors->query (query, startdAds);
        if( rval != Q_OK ) {
            fprintf( stderr , "Error:  Could not fetch startd ads\n" );
            exit( 1 );
        }
    }

	if (dash_profile) {
		profile_print(cbBefore, tmBefore, startdAds.Length());
	} else if (verbose) {
		fprintf(stderr, " %d ads.\n", startdAds.Length());
	}

	// fetch user priorities, and propagate the user priority values into the machine ad's
	// we fetch user priorities either directly from the negotiator, or from a file
	// treat a userprios_file of "" as a signal to ignore user priority.
	//
	int cPrios = 0;
	if (userprios_file || analyze_with_userprio) {
		if (verbose || dash_profile) {
			fprintf(stderr, "Fetching user priorities...");
		}

		if (userprios_file != NULL) {
			cPrios = read_userprio_file(userprios_file, prioTable);
			if (cPrios < 0) {
				//PRAGMA_REMIND("TJ: don't exit here, just analyze without userprio information")
				exit (1);
			}
		} else {
			// fetch submittor prios
			cPrios = fetchSubmittorPriosFromNegotiator(prioTable);
			if (cPrios < 0) {
				//PRAGMA_REMIND("TJ: don't exit here, just analyze without userprio information")
				exit(1);
			}
		}

		if (dash_profile) {
			profile_print(cbBefore, tmBefore, cPrios);
		}
		else if (verbose) {
			fprintf(stderr, " %d submitters\n", cPrios);
		}
		/* TJ: do we really want to do this??
		if (cPrios <= 0) {
			printf( "Warning:  Found no submitters\n" );
		}
		*/


		// populate startd ads with remote user prios
		startdAds.Open();
		while( ( ad = startdAds.Next() ) ) {
			if( ad->LookupString( ATTR_REMOTE_USER , remoteUser ) ) {
				if( ( index = findSubmittor( remoteUser.c_str() ) ) != -1 ) {
					sprintf( buffer , "%s = %f" , ATTR_REMOTE_USER_PRIO , 
								prioTable[index].prio );
					ad->Insert( buffer );
				}
			}
			#if defined(ADD_TARGET_SCOPING)
			ad->AddTargetRefs( TargetJobAttrs );
			#endif
		}
		startdAds.Close();
	}
	

	// setup condition expressions
    sprintf( buffer, "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK );
    ParseClassAdRvalExpr( buffer, stdRankCondition );

    sprintf( buffer, "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK );
    ParseClassAdRvalExpr( buffer, preemptRankCondition );

	sprintf( buffer, "MY.%s > TARGET.%s + %f", ATTR_REMOTE_USER_PRIO, 
			ATTR_SUBMITTOR_PRIO, PriorityDelta );
	ParseClassAdRvalExpr( buffer, preemptPrioCondition ) ;

	// setup preemption requirements expression
	if( !( preq = param( "PREEMPTION_REQUIREMENTS" ) ) ) {
		fprintf( stderr, "\nWarning:  No PREEMPTION_REQUIREMENTS expression in"
					" config file --- assuming FALSE\n\n" );
		ParseClassAdRvalExpr( "FALSE", preemptionReq );
	} else {
		if( ParseClassAdRvalExpr( preq , preemptionReq ) ) {
			fprintf( stderr, "\nError:  Failed parse of "
				"PREEMPTION_REQUIREMENTS expression: \n\t%s\n", preq );
			exit( 1 );
		}
#if defined(ADD_TARGET_SCOPING)
		ExprTree *tmp_expr = AddTargetRefs( preemptionReq, TargetJobAttrs );
		delete preemptionReq;
		preemptionReq = tmp_expr;
#endif
		free( preq );
	}

}


static int
fetchSubmittorPriosFromNegotiator(ExtArray<PrioEntry> & prios)
{
	AttrList	al;
	char  	attrName[32], attrPrio[32];
  	char  	name[128];
  	float 	priority;

	Daemon	negotiator( DT_NEGOTIATOR, NULL, pool ? pool->addr() : NULL );

	// connect to negotiator
	Sock* sock;

	if (!(sock = negotiator.startCommand( GET_PRIORITY, Stream::reli_sock, 0))) {
		fprintf( stderr, "Error: Could not connect to negotiator (%s)\n",
				 negotiator.fullHostname() );
		return -1;
	}

	sock->end_of_message();
	sock->decode();
	if( !getClassAdNoTypes(sock, al) || !sock->end_of_message() ) {
		fprintf( stderr, 
				 "Error:  Could not get priorities from negotiator (%s)\n",
				 negotiator.fullHostname() );
		return -1;
	}
	sock->close();
	delete sock;


	int cPrios = 0;
	while( true ) {
		sprintf( attrName , "Name%d", cPrios+1 );
		sprintf( attrPrio , "Priority%d", cPrios+1 );

		if( !al.LookupString( attrName, name, sizeof(name) ) || 
			!al.LookupFloat( attrPrio, priority ) )
			break;

		prios[cPrios].name = name;
		prios[cPrios].prio = priority;
		++cPrios;
	}

	return cPrios;
}

// parse lines of the form "attrNNNN = value" and return attr, NNNN and value as separate fields.
static int parse_userprio_line(const char * line, std::string & attr, std::string & value)
{
	int id = 0;

	// skip over the attr part
	const char * p = line;
	while (*p && isspace(*p)) ++p;

	// parse prefixNNN and set id=NNN and attr=prefix
	const char * pattr = p;
	while (*p) {
		if (isdigit(*p)) {
			attr.assign(pattr, p-pattr);
			id = atoi(p);
			while (isdigit(*p)) ++p;
			break;
		} else if  (isspace(*p)) {
			break;
		}
		++p;
	}
	if (id <= 0)
		return -1;

	// parse = with or without whitespace.
	while (isspace(*p)) ++p;
	if (*p != '=')
		return -1;
	++p;
	while (isspace(*p)) ++p;

	// parse value, remove "" from around strings 
	if (*p == '"')
		value.assign(p+1,strlen(p)-2);
	else
		value = p;
	return id;
}

static int read_userprio_file(const char *filename, ExtArray<PrioEntry> & prios)
{
	int cPrios = 0;

	FILE *file = safe_fopen_wrapper_follow(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Can't open file of user priorities: %s\n", filename);
		return -1;
	} else {
		int lineno = 0;
		while (const char * line = getline_trim(file, lineno)) {
			std::string attr, value;
			int id = parse_userprio_line(line, attr, value);
			if (id <= 0) {
				// there are valid attributes that we want to ignore that return -1 from
				// this call, so just skip them
				continue;
			}

			if (attr == "Priority") {
				float priority = atof(value.c_str());
				prios[id].prio = priority;
				cPrios = MAX(cPrios, id);
			} else if (attr == "Name") {
				prios[id].name = value;
				cPrios = MAX(cPrios, id);
			}
		}
		fclose(file);
	}
	return cPrios;
}

static void
doJobRunAnalysis(ClassAd *request, Daemon *schedd, int details)
{
	//PRAGMA_REMIND("TJ: move this code out of this function so it doesn't output once per job")
	if (schedd) {
		MyString buf;
		warnScheddLimits(schedd, request, buf);
		printf("%s", buf.Value());
	}

	anaCounters ac;
	printf("%s", doJobRunAnalysisToBuffer(request, ac, details, false, verbose));
}

static void append_to_fail_list(std::string & list, const char * entry, size_t limit)
{
	if ( ! limit || list.size() < limit) {
		if (list.size() > 1) { list += ", "; }
		list += entry;
	} else if (list.size() > limit) {
		if (limit > 50) {
			list.erase(limit-3);
			list += "...";
		} else {
			list.erase(limit);
		}
	}
}


static const char *
doJobRunAnalysisToBuffer(ClassAd *request, anaCounters & ac, int details, bool noPrio, bool showMachines)
{
	char	owner[128];
	std::string  user;
	char	buffer[128];
	ClassAd	*offer;
	classad::Value	eval_result;
	bool	val;
	int		cluster, proc;
	int		jobState;
	int		niceUser;
	int		universe;
	int		verb_width = 100;
	int		jobMatched = false;
	std::string job_status = "";

	bool	alwaysReqAnalyze = (details & detail_always_analyze_req) != 0;
	bool	analEachReqClause = (details & detail_analyze_each_sub_expr) != 0;
	bool	useNewPrettyReq = true;
	bool	showJobAttrs = analEachReqClause && ! (details & detail_dont_show_job_attrs);
	bool	withoutPrio = false;

	ac.clear();
	return_buff[0]='\0';

#if defined(ADD_TARGET_SCOPING)
	request->AddTargetRefs( TargetMachineAttrs );
#endif

	if (analyze_memory_usage) {
		int num_skipped = 0;
		QuantizingAccumulator mem_use(0,0);
		const classad::ExprTree* tree = request->LookupExpr(analyze_memory_usage);
		if (tree) {
			AddExprTreeMemoryUse(tree, mem_use, num_skipped);
		}
		size_t raw_mem, quantized_mem, num_allocs;
		raw_mem = mem_use.Value(&quantized_mem, &num_allocs);

		sprintf(return_buff, "\nMemory usage for '%s' is %d bytes from %d allocations requesting %d bytes (%d expr nodes skipped)\n",
				analyze_memory_usage, (int)quantized_mem, (int)num_allocs, (int)raw_mem, num_skipped);
		return return_buff;
	}

	if( !request->LookupString( ATTR_OWNER , owner, sizeof(owner) ) ) return "Nothing here.\n";
	if( !request->LookupInteger( ATTR_NICE_USER , niceUser ) ) niceUser = 0;
	request->LookupString(ATTR_USER, user);

	int last_match_time=0, last_rej_match_time=0;
	request->LookupInteger(ATTR_LAST_MATCH_TIME, last_match_time);
	request->LookupInteger(ATTR_LAST_REJ_MATCH_TIME, last_rej_match_time);

	request->LookupInteger( ATTR_CLUSTER_ID, cluster );
	request->LookupInteger( ATTR_PROC_ID, proc );
	request->LookupInteger( ATTR_JOB_STATUS, jobState );
	request->LookupBool( ATTR_JOB_MATCHED, jobMatched );
	if (jobState == RUNNING || jobState == TRANSFERRING_OUTPUT || jobState == SUSPENDED) {
		job_status = "Request is running.";
	}
	if (jobState == HELD) {
		job_status = "Request is held.";
		MyString hold_reason;
		request->LookupString( ATTR_HOLD_REASON, hold_reason );
		if( hold_reason.Length() ) {
			job_status += "\n\nHold reason: ";
			job_status += hold_reason.Value();
		}
	}
	if (jobState == REMOVED) {
		job_status = "Request is removed.";
	}
	if (jobState == COMPLETED) {
		job_status = "Request is completed.";
	}
	if (jobMatched) {
		job_status = "Request has been matched.";
	}

	// if we already figured out the job status, and we haven't been asked to analyze requirements anyway
	// we are done.
	if ( ! job_status.empty() && ! alwaysReqAnalyze) {
		sprintf(return_buff, "---\n%03d.%03d:  %s\n\n" , cluster, proc, job_status.c_str());
		return return_buff;
	}

	// 
	int ixSubmittor = -1;
	if (noPrio) {
		withoutPrio = true;
	} else {
		findSubmittor(fixSubmittorName(user.c_str(), niceUser));
		if (ixSubmittor < 0) {
			withoutPrio = true;
		} else {
			request->Assign(ATTR_SUBMITTOR_PRIO, prioTable[ixSubmittor].prio);
		}
	}

	startdAds.Open();

	std::string fReqConstraintStr("[");
	std::string fOffConstraintStr("[");
	std::string fOfflineStr("[");
	std::string fPreemptPrioStr("[");
	std::string fPreemptReqTestStr("[");
	std::string fRankCondStr("[");

	while( ( offer = startdAds.Next() ) ) {
		// 0.  info from machine
		ac.totalMachines++;
		offer->LookupString( ATTR_NAME , buffer, sizeof(buffer) );
		//if( verbose ) { strcat(return_buff, buffer); strcat(return_buff, " "); }

		// 1. Request satisfied? 
		if( !IsAHalfMatch( request, offer ) ) {
			//if( verbose ) strcat(return_buff, "Failed request constraint\n");
			ac.fReqConstraint++;
			if (showMachines) append_to_fail_list(fReqConstraintStr, buffer, verb_width);
			continue;
		} 

		// 2. Offer satisfied? 
		if ( !IsAHalfMatch( offer, request ) ) {
			//if( verbose ) strcat( return_buff, "Failed offer constraint\n");
			ac.fOffConstraint++;
			if (showMachines) append_to_fail_list(fOffConstraintStr, buffer, verb_width);
			continue;
		}	

		int offline = 0;
		if( offer->EvalBool( ATTR_OFFLINE, NULL, offline ) && offline ) {
			ac.fOffline++;
			if (showMachines) append_to_fail_list(fOfflineStr, buffer, verb_width);
			continue;
		}

		// 3. Is there a remote user?
		string remoteUser;
		if( !offer->LookupString( ATTR_REMOTE_USER, remoteUser ) ) {
			// no remote user
			if( EvalExprTree( stdRankCondition, offer, request, eval_result ) &&
				eval_result.IsBooleanValue(val) && val ) {
				// both sides satisfied and no remote user
				//if( verbose ) strcat(return_buff, "Available\n");
				ac.available++;
				continue;
			} else {
				// no remote user and std rank condition failed
			  if (last_rej_match_time != 0) {
				ac.fRankCond++;
				if (showMachines) append_to_fail_list(fRankCondStr, buffer, verb_width);
				//if( verbose ) strcat( return_buff, "Failed rank condition: MY.Rank > MY.CurrentRank\n");
				continue;
			  } else {
				ac.available++; // tj: is this correct?
				//PRAGMA_REMIND("TJ: move this out of the machine iteration loop?")
				if (job_status.empty()) {
					job_status = "Request has not yet been considered by the matchmaker.";
				}
				continue;
			  }
			}
		}

		// if we get to here, there is a remote user, if we don't have access to user priorities
		// we can't decide whether we should be able to preempt other users, so we are done.
		ac.machinesRunningJobs++;
		if (withoutPrio) {
			if (user == remoteUser) {
				++ac.machinesRunningUsersJobs;
			} else {
				append_to_fail_list(fPreemptPrioStr, buffer, verb_width); // borrow preempt prio list
			}
			continue;
		}

		// machines running your jobs will never preempt for your job.
		if (user == remoteUser) {
			++ac.machinesRunningUsersJobs;

		// 4. Satisfies preemption priority condition?
		} else if( EvalExprTree( preemptPrioCondition, offer, request, eval_result ) &&
			eval_result.IsBooleanValue(val) && val ) {

			// 5. Satisfies standard rank condition?
			if( EvalExprTree( stdRankCondition, offer , request , eval_result ) &&
				eval_result.IsBooleanValue(val) && val )  
			{
				//if( verbose ) strcat( return_buff, "Available\n");
				ac.available++;
				continue;
			} else {
				// 6.  Satisfies preemption rank condition?
				if( EvalExprTree( preemptRankCondition, offer, request, eval_result ) &&
					eval_result.IsBooleanValue(val) && val )
				{
					// 7.  Tripped on PREEMPTION_REQUIREMENTS?
					if( EvalExprTree( preemptionReq, offer , request , eval_result ) &&
						eval_result.IsBooleanValue(val) && !val ) 
					{
						ac.fPreemptReqTest++;
						if (showMachines) append_to_fail_list(fPreemptReqTestStr, buffer, verb_width);
						/*
						if( verbose ) {
							sprintf_cat( return_buff,
									"%sCan preempt %s, but failed "
									"PREEMPTION_REQUIREMENTS test\n",
									buffer,
									 remoteUser.c_str());
						}
						*/
						continue;
					} else {
						// not held
						/*
						if( verbose ) {
							sprintf_cat( return_buff,
								"Available (can preempt %s)\n", remoteUser.c_str());
						}
						*/
						ac.available++;
					}
				} else {
					// failed 6 and 5, but satisfies 4; so have priority
					// but not better or equally preferred than current
					// customer
					// NOTE: In practice this often indicates some
					// unknown problem.
				  if (last_rej_match_time != 0) {
					ac.fRankCond++;
				  } else {
					if (job_status.empty()) {
						job_status = "Request has not yet been considered by the matchmaker.";
					}
				  }
				}
			} 
		} else {
			ac.fPreemptPrioCond++;
			append_to_fail_list(fPreemptPrioStr, buffer, verb_width);
			/*
			if( verbose ) {
				sprintf_cat( return_buff, "Insufficient priority to preempt %s\n", remoteUser.c_str() );
			}
			*/
			continue;
		}
	}
	startdAds.Close();

	if (summarize_anal)
		return return_buff;

	fReqConstraintStr += "]";
	fOffConstraintStr += "]";
	fOfflineStr += "]";
	fPreemptPrioStr += "]";
	fPreemptReqTestStr += "]";
	fRankCondStr += "]";

	if ( ! job_status.empty()) {
		sprintf(return_buff, "---\n%03d.%03d:  %s\n\n" , cluster, proc, job_status.c_str());
	}

	if ( ! noPrio && ixSubmittor < 0) {
		sprintf(return_buff+strlen(return_buff), "User priority for %s is not available, attempting to analyze without it.\n", user.c_str());
	}

	sprintf( return_buff + strlen(return_buff),
		 "---\n%03d.%03d:  Run analysis summary.  Of %d machines,\n"
		 "  %5d are rejected by your job's requirements %s\n"
		 "  %5d reject your job because of their own requirements %s\n",
		cluster, proc, ac.totalMachines,
		ac.fReqConstraint, showMachines  ? fReqConstraintStr.c_str() : "",
		ac.fOffConstraint, showMachines ? fOffConstraintStr.c_str() : "");

	if (withoutPrio) {
		sprintf( return_buff + strlen(return_buff),
			"  %5d match and are already running your jobs %s\n",
			ac.machinesRunningUsersJobs, "");

		sprintf( return_buff + strlen(return_buff),
			"  %5d match but are serving other users %s\n",
			ac.machinesRunningJobs - ac.machinesRunningUsersJobs, showMachines ? fPreemptPrioStr.c_str() : "");

		if (ac.fOffline > 0) {
			sprintf( return_buff + strlen(return_buff),
				"  %5d match but are currently offline %s\n",
				ac.fOffline, showMachines ? fOfflineStr.c_str() : "");
		}

		sprintf( return_buff + strlen(return_buff),
			"  %5d are available to run your job\n",
			ac.available );
	} else {
		sprintf( return_buff + strlen(return_buff),
			 "  %5d match and are already running one of your jobs%s\n"
			 "  %5d match but are serving users with a better priority in the pool%s %s\n"
			 "  %5d match but reject the job for unknown reasons %s\n"
			 "  %5d match but will not currently preempt their existing job %s\n"
			 "  %5d match but are currently offline %s\n"
			 "  %5d are available to run your job\n",
			ac.machinesRunningUsersJobs, "",
			ac.fPreemptPrioCond, niceUser ? "(*)" : "", showMachines ? fPreemptPrioStr.c_str() : "",
			ac.fRankCond, showMachines ? fRankCondStr.c_str() : "",
			ac.fPreemptReqTest,  showMachines ? fPreemptReqTestStr.c_str() : "",
			ac.fOffline, showMachines ? fOfflineStr.c_str() : "",
			ac.available );

		sprintf(return_buff + strlen(return_buff), 
			 "  %s has a priority of %9.3f\n", prioTable[ixSubmittor].name.Value(), prioTable[ixSubmittor].prio);
	}

	if (last_match_time) {
		time_t t = (time_t)last_match_time;
		sprintf( return_buff, "%s\tLast successful match: %s",
				 return_buff, ctime(&t) );
	} else if (last_rej_match_time) {
		strcat( return_buff, "\tNo successful match recorded.\n" );
	}
	if (last_rej_match_time > last_match_time) {
		time_t t = (time_t)last_rej_match_time;
		string timestring(ctime(&t));
		string rej_str="\tLast failed match: " + timestring + '\n';
		strcat(return_buff, rej_str.c_str());
		string rej_reason;
		if (request->LookupString(ATTR_LAST_REJ_MATCH_REASON, rej_reason)) {
			rej_str="\tReason for last match failure: " + rej_reason + '\n';
			strcat(return_buff, rej_str.c_str());	
		}
	}

	if( niceUser ) {
		sprintf( return_buff, 
				 "%s\n\t(*)  Since this is a \"nice-user\" request, this request "
				 "has a\n\t     very low priority and is unlikely to preempt other "
				 "requests.\n", return_buff );
	}
			

	if( ac.fReqConstraint == ac.totalMachines ) {
		strcat( return_buff, "\nWARNING:  Be advised:\n");
		strcat( return_buff, "   No resources matched request's constraints\n");
        if (!better_analyze) {
            ExprTree *reqExp;
            sprintf( return_buff, "%s   Check the %s expression below:\n\n" , 
                     return_buff, ATTR_REQUIREMENTS );
            if( !(reqExp = request->LookupExpr( ATTR_REQUIREMENTS) ) ) {
                sprintf( return_buff, "%s   ERROR:  No %s expression found" ,
                         return_buff, ATTR_REQUIREMENTS );
            } else {
				sprintf( return_buff, "%s%s = %s\n\n", return_buff,
						 ATTR_REQUIREMENTS, ExprTreeToString( reqExp ) );
            }
        }
	}

	if (better_analyze && ((ac.fReqConstraint > 0) || alwaysReqAnalyze)) {

		// first analyze the Requirements expression against the startd ads.
		//
		std::string suggest_buf = "";
		std::string pretty_req = "";
		analyzer.GetErrors(true); // true to clear errors
		analyzer.AnalyzeJobReqToBuffer( request, startdAds, suggest_buf, pretty_req );
		if ((int)suggest_buf.size() > SHORT_BUFFER_SIZE)
		   suggest_buf.erase(SHORT_BUFFER_SIZE, string::npos);

		bool requirements_is_false = (suggest_buf == "Job ClassAd Requirements expression evaluates to false\n\n");

		if ( ! useNewPrettyReq) {
			strcat(return_buff, pretty_req.c_str());
		}
		pretty_req = "";

		if (useNewPrettyReq) {
			classad::ExprTree* tree = request->LookupExpr(ATTR_REQUIREMENTS);
			if (tree) {
				int console_width = widescreen ? getDisplayWidth() : 80;
				const int indent = 4;
				PrettyPrintExprTree(tree, pretty_req, indent, console_width);
				strcat(return_buff, "\nThe Requirements expression for your job is:\n\n");
				std::string spaces; spaces.insert(0, indent, ' ');
				strcat(return_buff, spaces.c_str());
				strcat(return_buff, pretty_req.c_str());
				strcat(return_buff, "\n\n");
				pretty_req = "";
			}
		}

		// then capture the values of MY attributes refereced by the expression
		// also capture the value of TARGET attributes if there is only a single ad.
		if (showJobAttrs) {
			std::string attrib_values = "";
			attrib_values = "Your job defines the following attributes:\n\n";
			StringList trefs;
			AddReferencedAttribsToBuffer(request, ATTR_REQUIREMENTS, trefs, "    ", attrib_values);
			strcat(return_buff, attrib_values.c_str());
			attrib_values = "";

			if (single_machine || startdAds.Length() == 1) { 
				startdAds.Open(); 
				while (ClassAd * ptarget = startdAds.Next()) {
					attrib_values = "\n";
					AddTargetAttribsToBuffer(trefs, request, ptarget, "    ", attrib_values);
					strcat(return_buff, attrib_values.c_str());
				}
				startdAds.Close();
			}
			strcat(return_buff, "\n");
		}

		// TJ's experimental analysis (now with more anal)
		if (analEachReqClause || requirements_is_false) {
			std::string subexpr_detail;
			anaFormattingOptions fmt = { widescreen ? getConsoleWindowSize() : 80, details, "Requirements", "Job", "Slot" };
			AnalyzeRequirementsForEachTarget(request, ATTR_REQUIREMENTS, startdAds, subexpr_detail, fmt);
			strcat(return_buff, "The Requirements expression for your job reduces to these conditions:\n\n");
			strcat(return_buff, subexpr_detail.c_str());
		}

		// write the analysis/suggestions to the return buffer
		//
		strcat(return_buff, "\nSuggestions:\n\n");
		strcat(return_buff, suggest_buf.c_str());
	}

	if( ac.fOffConstraint == ac.totalMachines ) {
        strcat(return_buff, "\nWARNING:  Be advised:\n   Request did not match any resource's constraints\n");
	}

    if (better_analyze) {
        std::string buffer_string = "";
        char ana_buffer[SHORT_BUFFER_SIZE];
        if( ac.fOffConstraint > 0 ) {
            buffer_string = "";
            analyzer.GetErrors(true); // true to clear errors
            analyzer.AnalyzeJobAttrsToBuffer( request, startdAds, buffer_string );
            strncpy( ana_buffer, buffer_string.c_str( ), SHORT_BUFFER_SIZE);
			ana_buffer[SHORT_BUFFER_SIZE-1] = '\0';
            strcat( return_buff, ana_buffer );
        }
    }

	request->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	bool uses_matchmaking = false;
	MyString resource;
	switch(universe) {
			// Known valid
		case CONDOR_UNIVERSE_STANDARD:
		case CONDOR_UNIVERSE_JAVA:
		case CONDOR_UNIVERSE_VANILLA:
			break;

			// Unknown
		case CONDOR_UNIVERSE_PARALLEL:
		case CONDOR_UNIVERSE_VM:
			break;

			// Maybe
		case CONDOR_UNIVERSE_GRID:
			/* We may be able to detect when it's valid.  Check for existance
			 * of "$$(FOO)" style variables in the classad. */
			request->LookupString(ATTR_GRID_RESOURCE, resource);
			if ( strstr(resource.Value(),"$$") ) {
				uses_matchmaking = true;
				break;
			}  
			if (!uses_matchmaking) {
				sprintf( return_buff, "%s\nWARNING: Analysis is only meaningful for Grid universe jobs using matchmaking.\n", return_buff);
			}
			break;

			// Specific known bad
		case CONDOR_UNIVERSE_MPI:
			sprintf( return_buff, "%s\nWARNING: Analysis is meaningless for MPI universe jobs.\n", return_buff );
			break;

			// Specific known bad
		case CONDOR_UNIVERSE_SCHEDULER:
			/* Note: May be valid (although requiring a different algorithm)
			 * starting some time in V6.7. */
			sprintf( return_buff, "%s\nWARNING: Analysis is meaningless for Scheduler universe jobs.\n", return_buff );
			break;

			// Unknown
			/* These _might_ be meaningful, especially if someone adds a 
			 * universe but fails to update this code. */
		//case CONDOR_UNIVERSE_PIPE:
		//case CONDOR_UNIVERSE_LINDA:
		//case CONDOR_UNIVERSE_MAX:
		//case CONDOR_UNIVERSE_MIN:
		//case CONDOR_UNIVERSE_PVM:
		//case CONDOR_UNIVERSE_PVMD:
		default:
			sprintf( return_buff, "%s\nWARNING: Job universe unknown.  Analysis may not be meaningful.\n", return_buff );
			break;
	}


	//printf("%s",return_buff);
	return return_buff;
}


static void
doSlotRunAnalysis(ClassAd *slot, JobClusterMap & clusters, Daemon * /*schedd*/, int console_width)
{
	printf("%s", doSlotRunAnalysisToBuffer(slot, clusters, console_width));
}

static const char *
doSlotRunAnalysisToBuffer(ClassAd *slot, JobClusterMap & clusters, int console_width)
{
	bool analStartExpr = /*(better_analyze == 2) ||*/ (analyze_detail_level > 0);
	bool showSlotAttrs = ! (analyze_detail_level & detail_dont_show_job_attrs);
	anaFormattingOptions fmt = { console_width, analyze_detail_level, "START", "Slot", "Cluster" };

	return_buff[0] = 0;

#if defined(ADD_TARGET_SCOPING)
	slot->AddTargetRefs(TargetJobAttrs);
#endif

	std::string slotname = "";
	slot->LookupString(ATTR_NAME , slotname);

	int offline = 0;
	if (slot->EvalBool(ATTR_OFFLINE, NULL, offline) && offline) {
		sprintf(return_buff, "%-24.24s  is offline\n", slotname.c_str());
		return return_buff;
	}

	const char * slot_type = "Stat";
	bool is_dslot = false, is_pslot = false;
	if (slot->LookupBool("DynamicSlot", is_dslot) && is_dslot) slot_type = "Dyn";
	else if (slot->LookupBool("PartitionableSlot", is_pslot) && is_pslot) slot_type = "Part";

	int cTotalJobs = 0;
	int cUniqueJobs = 0;
	int cReqConstraint = 0;
	int cOffConstraint = 0;
	int cBothMatch = 0;

	ClassAdListDoesNotDeleteAds jobs;
	for (JobClusterMap::iterator it = clusters.begin(); it != clusters.end(); ++it) {
		int cJobsInCluster = (int)it->second.size();
		if (cJobsInCluster <= 0)
			continue;

		// for the the non-autocluster cluster, we have to eval these jobs individually
		int cJobsToEval = (it->first == -1) ? cJobsInCluster : 1;
		int cJobsToInc  = (it->first == -1) ? 1 : cJobsInCluster;

		cTotalJobs += cJobsInCluster;

		for (int ii = 0; ii < cJobsToEval; ++ii) {
			ClassAd *job = it->second[ii];

			jobs.Insert(job);
			cUniqueJobs += 1;

			#if defined(ADD_TARGET_SCOPING)
			job->AddTargetRefs(TargetMachineAttrs);
			#endif

			// 2. Offer satisfied?
			bool offer_match = IsAHalfMatch(slot, job);
			if (offer_match) {
				cOffConstraint += cJobsToInc;
			}

			// 1. Request satisfied?
			if (IsAHalfMatch(job, slot)) {
				cReqConstraint += cJobsToInc;
				if (offer_match) {
					cBothMatch += cJobsToInc;
				}
			}
		}
	}

	if ( ! summarize_anal && analStartExpr) {

		sprintf(return_buff, "\n-- Slot: %s : Analyzing matches for %d Jobs in %d autoclusters\n", 
				slotname.c_str(), cTotalJobs, cUniqueJobs);

		std::string pretty_req = "";
		static std::string prev_pretty_req;
		classad::ExprTree* tree = slot->LookupExpr(ATTR_REQUIREMENTS);
		if (tree) {
			PrettyPrintExprTree(tree, pretty_req, 4, console_width);

			tree = slot->LookupExpr(ATTR_START);
			if (tree) {
				pretty_req += "\n\n    START is ";
				PrettyPrintExprTree(tree, pretty_req, 4, console_width);
			}

			if (prev_pretty_req.empty() || prev_pretty_req != pretty_req) {
				strcat(return_buff, "\nThe Requirements expression for this slot is\n\n    ");
				strcat(return_buff, pretty_req.c_str());
				strcat(return_buff, "\n\n");
				// uncomment this line to print out Machine requirements only when it changes.
				//prev_pretty_req = pretty_req;
			}
			pretty_req = "";

			// then capture the values of MY attributes refereced by the expression
			// also capture the value of TARGET attributes if there is only a single ad.
			if (showSlotAttrs) {
				std::string attrib_values = "";
				attrib_values = "This slot defines the following attributes:\n\n";
				StringList trefs;
				AddReferencedAttribsToBuffer(slot, ATTR_REQUIREMENTS, trefs, "    ", attrib_values);
				strcat(return_buff, attrib_values.c_str());
				attrib_values = "";

				if (jobs.Length() == 1) {
					jobs.Open();
					while(ClassAd *job = jobs.Next()) {
						attrib_values = "\n";
						AddTargetAttribsToBuffer(trefs, slot, job, "    ", attrib_values);
						strcat(return_buff, attrib_values.c_str());
					}
					jobs.Close();
				}
				//strcat(return_buff, "\n");
				strcat(return_buff, "\nThe Requirements expression for this slot reduces to these conditions:\n\n");
			}
		}

		std::string subexpr_detail;
		AnalyzeRequirementsForEachTarget(slot, ATTR_REQUIREMENTS, (ClassAdList&)jobs, subexpr_detail, fmt);
		strcat(return_buff, subexpr_detail.c_str());

		//formatstr(subexpr_detail, "%-5.5s %8d\n", "[ALL]", cOffConstraint);
		//strcat(return_buff, subexpr_detail.c_str());

		formatstr(pretty_req, "\n%s: Run analysis summary of %d jobs.\n"
			"%5d (%.2f %%) match both slot and job requirements.\n"
			"%5d match the requirements of this slot.\n"
			"%5d have job requirements that match this slot.\n",
			slotname.c_str(), cTotalJobs,
			cBothMatch, cTotalJobs ? (100.0 * cBothMatch / cTotalJobs) : 0.0,
			cOffConstraint, 
			cReqConstraint);
		strcat(return_buff, pretty_req.c_str());
		pretty_req = "";

	} else {
		char fmt[sizeof("%-nnn.nnns %-4s %12d %12d %10.2f\n")];
		int name_width = MAX(longest_slot_machine_name+7, longest_slot_name);
		sprintf(fmt, "%%-%d.%ds", MAX(name_width, 16), MAX(name_width, 16));
		strcat(fmt, " %-4s %12d %12d %10.2f\n");
		sprintf(return_buff, fmt, slotname.c_str(), slot_type, 
				cOffConstraint, cReqConstraint, 
				cTotalJobs ? (100.0 * cBothMatch / cTotalJobs) : 0.0);
	}

	jobs.Clear();

	if (better_analyze) {
		std::string ana_buffer = "";
		strcat(return_buff, ana_buffer.c_str());
	}

	return return_buff;
}


static	void
buildJobClusterMap(ClassAdList & jobs, const char * attr, JobClusterMap & autoclusters)
{
	autoclusters.clear();

	jobs.Open();
	while(ClassAd *job = jobs.Next()) {

		int acid = -1;
		if (job->LookupInteger(attr, acid)) {
			//std::map<int, ClassAdListDoesNotDeleteAds>::iterator it;
			autoclusters[acid].push_back(job);
		} else {
			// stick auto-clusterless jobs into the -1 slot.
			autoclusters[-1].push_back(job);
		}

	}
	jobs.Close();
}


static int
findSubmittor( const char *name ) 
{
	MyString 	sub(name);
	int			last = prioTable.getlast();
	
	for(int i = 0 ; i <= last ; i++ ) {
		if( prioTable[i].name == sub ) return i;
	}

	//prioTable[last+1].name = sub;
	//prioTable[last+1].prio = 0.5;
	//return last+1;

	return -1;
}


static const char*
fixSubmittorName( const char *name, int niceUser )
{
	static 	bool initialized = false;
	static	char * uid_domain = 0;
	static	char buffer[128];

	if( !initialized ) {
		uid_domain = param( "UID_DOMAIN" );
		if( !uid_domain ) {
			fprintf( stderr, "Error: UID_DOMAIN not found in config file\n" );
			exit( 1 );
		}
		initialized = true;
	}

    // potential buffer overflow! Hao
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


static bool read_classad_file(const char *filename, ClassAdList &classads, ClassAdFileParseHelper* pparse_help, const char * constr)
{
	bool success = false;

	FILE* file = safe_fopen_wrapper_follow(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Can't open file of job ads: %s\n", filename);
		return false;
	} else {
		CondorClassAdFileParseHelper generic_helper("\n");
		if ( ! pparse_help)
			pparse_help = &generic_helper;

		for (;;) {
			ClassAd* classad = new ClassAd();

			int error;
			bool is_eof;
			int cAttrs = classad->InsertFromFile(file, is_eof, error, pparse_help);

			bool include_classad = cAttrs > 0 && error >= 0;
			if (include_classad && constr) {
				classad::Value val;
				if (classad->EvaluateExpr(constr,val)) {
					if ( ! val.IsBooleanValueEquiv(include_classad)) {
						include_classad = false;
					}
				}
			}
			if (include_classad) {
				classads.Insert(classad);
			} else {
				delete classad;
			}

			if (is_eof) {
				success = true;
				break;
			}
			if (error < 0) {
				success = false;
				break;
			}
		}

		fclose(file);
	}
	return success;
}


#ifdef HAVE_EXT_POSTGRESQL

/* get the quill address for the quillName specified */
static QueryResult getQuillAddrFromCollector(const char *quill_name, std::string & quill_addr) {
	QueryResult query_result = Q_OK;
	char		query_constraint[1024];
	CondorQuery	quillQuery(QUILL_AD);
	ClassAdList quillList;
	ClassAd		*ad;

	sprintf (query_constraint, "%s == \"%s\"", ATTR_NAME, quill_name);
	quillQuery.addORConstraint (query_constraint);

	query_result = Collectors->query ( quillQuery, quillList );

	quillList.Open();	
	while ((ad = quillList.Next())) {
		ad->LookupString(ATTR_MY_ADDRESS, quill_addr);
	}
	quillList.Close();
	return query_result;
}


static char * getDBConnStr(char const *&quill_name,
                           char const *&databaseIp,
                           char const *&databaseName,
                           char const *&query_password) {
	char            *host, *port, *dbconn;
	const char *ptr_colon;
	char            *tmpquillname, *tmpdatabaseip, *tmpdatabasename, *tmpquerypassword;
	int             len, tmp1, tmp2, tmp3;

	if((!quill_name && !(tmpquillname = param("QUILL_NAME"))) ||
	   (!databaseIp && !(tmpdatabaseip = param("QUILL_DB_IP_ADDR"))) ||
	   (!databaseName && !(tmpdatabasename = param("QUILL_DB_NAME"))) ||
	   (!query_password && !(tmpquerypassword = param("QUILL_DB_QUERY_PASSWORD")))) {
		fprintf( stderr, "Error: Could not find database related parameter\n");
		fprintf(stderr, "\n");
		print_wrapped_text("Extra Info: "
                       "The most likely cause for this error "
                       "is that you have not defined "
                       "QUILL_NAME/QUILL_DB_IP_ADDR/"
                       "QUILL_DB_NAME/QUILL_DB_QUERY_PASSWORD "
                       "in the condor_config file.  You must "
						   "define this variable in the config file", stderr);

		exit( 1 );
	}

	if(!quill_name) {
		quill_name = tmpquillname;
	}
	if(!databaseIp) {
		if(tmpdatabaseip[0] != '<') {
				//2 for the two brackets and 1 for the null terminator
			char *tstr = (char *) malloc(strlen(tmpdatabaseip)+3);
			sprintf(tstr, "<%s>", tmpdatabaseip);
			free(tmpdatabaseip);
			databaseIp = tstr;
		}
		else {
			databaseIp = tmpdatabaseip;
		}
	}

	if(!databaseName) {
		databaseName = tmpdatabasename;
	}
	if(!query_password) {
		query_password = tmpquerypassword;
	}

	tmp1 = strlen(databaseName);
	tmp2 = strlen(query_password);
	len = strlen(databaseIp);

		//the 6 is for the string "host= " or "port= "
		//the rest is a subset of databaseIp so a size of
		//databaseIp is more than enough
	host = (char *) malloc((len+6) * sizeof(char));
	port = (char *) malloc((len+6) * sizeof(char));

		//here we break up the ipaddress:port string and assign the
		//individual parts to separate string variables host and port
	ptr_colon = strchr((char *)databaseIp, ':');
	strcpy(host, "host=");
	strncat(host,
			databaseIp+1,
			ptr_colon - databaseIp -1);
	strcpy(port, "port=");
	strcat(port, ptr_colon+1);
	port[strlen(port)-1] = '\0';

		//tmp3 is the size of dbconn - its size is estimated to be
		//(2 * len) for the host/port part, tmp1 + tmp2 for the
		//password and dbname part and 1024 as a cautiously
		//overestimated sized buffer
	tmp3 = (2 * len) + tmp1 + tmp2 + 1024;
	dbconn = (char *) malloc(tmp3 * sizeof(char));
	sprintf(dbconn, "%s %s user=quillreader password=%s dbname=%s",
			host, port, query_password, databaseName);

	free(host);
	free(port);
	return dbconn;
}

static void exec_db_query(const char *quill_name, const char *db_ipAddr, const char *db_name, const char *query_password) {
	char *dbconn=NULL;
	
	dbconn = getDBConnStr(quill_name, db_ipAddr, db_name, query_password);

	printf ("\n\n-- Quill: %s : %s : %s\n", quill_name, 
			db_ipAddr, db_name);

	if (avgqueuetime) {
		Q.rawDBQuery(dbconn, AVG_TIME_IN_QUEUE);
	}
	if(dbconn) {
		free(dbconn);
	}

}

static void freeConnectionStrings() {
	squillName.clear();
	quillName = NULL;
	squillMachine.clear();
	quillMachine = NULL;
	sdbIpAddr.clear();
	dbIpAddr = NULL;
	sdbName.clear();
	dbName = NULL;
	squeryPassword.clear();
	queryPassword = NULL;
	quillAddr.clear();
}

static void quillCleanupOnExit(int n)
{
	freeConnectionStrings();
}

#endif /* HAVE_EXT_POSTGRESQL */

void warnScheddLimits(Daemon *schedd,ClassAd *job,MyString &result_buf) {
	if( !schedd ) {
		return;
	}
	ASSERT( job );

	ClassAd *ad = schedd->daemonAd();
	if (ad) {
		bool exhausted = false;
		ad->LookupBool("SwapSpaceExhausted", exhausted);
		if (exhausted) {
			result_buf.formatstr_cat("WARNING -- this schedd is not running jobs because it believes that doing so\n");
			result_buf.formatstr_cat("           would exhaust swap space and cause thrashing.\n");
			result_buf.formatstr_cat("           Set RESERVED_SWAP to 0 to tell the scheduler to skip this check\n");
			result_buf.formatstr_cat("           Or add more swap space.\n");
			result_buf.formatstr_cat("           The analysis code does not take this into consideration\n");
		}

		int maxJobsRunning 	= -1;
		int totalRunningJobs= -1;

		ad->LookupInteger( ATTR_MAX_JOBS_RUNNING, maxJobsRunning);
		ad->LookupInteger( ATTR_TOTAL_RUNNING_JOBS, totalRunningJobs);

		if ((maxJobsRunning > -1) && (totalRunningJobs > -1) && 
			(maxJobsRunning == totalRunningJobs)) { 
			result_buf.formatstr_cat("WARNING -- this schedd has hit the MAX_JOBS_RUNNING limit of %d\n", maxJobsRunning);
			result_buf.formatstr_cat("       to run more concurrent jobs, raise this limit in the config file\n");
			result_buf.formatstr_cat("       NOTE: the matchmaking analysis does not take the limit into consideration\n");
		}

		int status = -1;
		job->LookupInteger(ATTR_JOB_STATUS,status);
		if( status != RUNNING && status != TRANSFERRING_OUTPUT && status != SUSPENDED ) {

			int universe = -1;
			job->LookupInteger(ATTR_JOB_UNIVERSE,universe);

			char const *schedd_requirements_attr = NULL;
			switch( universe ) {
			case CONDOR_UNIVERSE_SCHEDULER:
				schedd_requirements_attr = ATTR_START_SCHEDULER_UNIVERSE;
				break;
			case CONDOR_UNIVERSE_LOCAL:
				schedd_requirements_attr = ATTR_START_LOCAL_UNIVERSE;
				break;
			}

			if( schedd_requirements_attr ) {
				MyString schedd_requirements_expr;
				ExprTree *expr = ad->LookupExpr(schedd_requirements_attr);
				if( expr ) {
					schedd_requirements_expr = ExprTreeToString(expr);
				}
				else {
					schedd_requirements_expr = "UNDEFINED";
				}

				int req = 0;
				if( !ad->EvalBool(schedd_requirements_attr,job,req) ) {
					result_buf.formatstr_cat("WARNING -- this schedd's policy %s failed to evalute for this job.\n",schedd_requirements_expr.Value());
				}
				else if( !req ) {
					result_buf.formatstr_cat("WARNING -- this schedd's policy %s evalutes to false for this job.\n",schedd_requirements_expr.Value());
				}
			}
		}
	}
}

const char * const jobDefault_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX  WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX  WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER           WIDTH -14 PRINTAS OWNER OR ??\n"
"   QDate         AS '  SUBMITTED'    WIDTH 11 PRINTAS QDATE\n"
"   RemoteUserCpu AS '    RUN_TIME'   WIDTH 12 PRINTAS CPU_TIME\n"
"   JobStatus     AS ST                       PRINTAS JOB_STATUS\n"
"   JobPrio       AS PRI             WIDTH AUTO\n"
"   ImageSize     AS SIZE            WIDTH 4  PRINTAS MEMORY_USAGE\n"
"   Cmd           AS CMD             WIDTH 0 PRINTAS JOB_DESCRIPTION\n"
"SUMMARY STANDARD\n";

const char * const jobDAG_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX  WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX  WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS 'OWNER/NODENAME' WIDTH -17 PRINTAS DAG_OWNER\n"
"   QDate         AS '  SUBMITTED'    WIDTH 11 PRINTAS QDATE\n"
"   RemoteUserCpu AS '    RUN_TIME'   WIDTH 12 PRINTAS CPU_TIME\n"
"   JobStatus     AS ST                       PRINTAS JOB_STATUS\n"
"   JobPrio       AS PRI             WIDTH AUTO\n"
"   ImageSize     AS SIZE            WIDTH 4  PRINTAS MEMORY_USAGE\n"
"   Cmd           AS CMD             WIDTH 0 PRINTAS JOB_DESCRIPTION\n"
"SUMMARY STANDARD\n";

const char * const jobRuntime_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER          WIDTH -14 PRINTAS OWNER OR ??\n"
"   QDate         AS '  SUBMITTED'   WIDTH 11  PRINTAS QDATE OR ??\n"
"   RemoteUserCpu AS '    RUN_TIME'  WIDTH 12  PRINTAS CPU_TIME OR ??\n"
"   Owner         AS 'HOST(S)'       WIDTH 0   PRINTAS REMOTE_HOST OR ??\n"
"SUMMARY STANDARD\n";

const char * const jobGoodput_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER          WIDTH -14 PRINTAS OWNER OR ??\n"
"   QDate         AS '  SUBMITTED'   WIDTH 11  PRINTAS QDATE OR ??\n"
"   RemoteUserCpu AS '    RUN_TIME'  WIDTH 12  PRINTAS CPU_TIME OR ??\n"
"   JobStatus     AS GOODPUT         WIDTH 8   PRINTAS STDU_GOODPUT OR ??\n"
"   RemoteUserCpu AS CPU_UTIL        WIDTH 9   PRINTAS CPU_UTIL OR ??\n"
"   BytesSent     AS 'Mb/s'          WIDTH 7   PRINTAS STDU_MPBS OR ??\n"
"SUMMARY STANDARD\n";

const char * const jobGrid_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER          WIDTH -14 PRINTAS OWNER\n"
"   JobStatus     AS STATUS          WIDTH -10 PRINTAS GRID_STATUS\n"
"   GridResource  AS 'GRID->MANAGER    HOST' WIDTH -27 PRINTAS GRID_RESOURCE\n"
"   GridJobId     AS GRID_JOB_ID             WIDTH   0 PRINTAS GRID_JOB_ID\n"
"SUMMARY STANDARD\n";

const char * const jobGlobus_PrintFormat = "SELECT\n"
"   ClusterId     AS ' ID'  NOSUFFIX WIDTH 5 PRINTF '%4d.'\n"
"   ProcId        AS ' '    NOPREFIX WIDTH 3 PRINTF '%-3d'\n"
"   Owner         AS  OWNER WIDTH -14 PRINTAS OWNER OR ??\n"
"   GlobusStatus  AS STATUS WIDTH -8 PRINTAS GLOBUS_STATUS OR ??\n"
"   Cmd           AS 'MANAGER    HOST' WIDTH 30 PRINTAS GLOBUS_HOST ALWAYS\n"
"   Cmd           AS EXECUTABLE     WIDTH 0\n"
"SUMMARY STANDARD\n";

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


// !!! ENTRIES IN THIS TABLE MUST BE SORTED BY THE FIRST FIELD !!
static const CustomFormatFnTableItem LocalPrintFormats[] = {
	{ "BATCH_NAME",      ATTR_JOB_CMD, 0, render_batch_name, ATTR_JOB_BATCH_NAME "\0" ATTR_JOB_CMD "\0" ATTR_DAGMAN_JOB_ID "\0" ATTR_DAG_NODE_NAME "\0" },
	{ "BUFFER_IO_MISC",  ATTR_JOB_UNIVERSE, 0, render_buffer_io_misc, ATTR_FILE_SEEK_COUNT "\0" ATTR_BUFFER_SIZE "\0" ATTR_BUFFER_BLOCK_SIZE "\0" ATTR_TRANSFERRING_INPUT "\0" ATTR_TRANSFERRING_OUTPUT "\0" ATTR_TRANSFER_QUEUED "\0" },
	{ "CPU_TIME",        ATTR_JOB_REMOTE_USER_CPU, "%T", render_cpu_time, ATTR_JOB_STATUS "\0" ATTR_SERVER_TIME "\0" ATTR_SHADOW_BIRTHDATE "\0" ATTR_JOB_REMOTE_WALL_CLOCK "\0" },
	{ "CPU_UTIL",        ATTR_JOB_REMOTE_USER_CPU, "%.1f", render_cpu_util, ATTR_JOB_COMMITTED_TIME "\0" },
	{ "DAG_OWNER",       ATTR_OWNER, 0, render_dag_owner, ATTR_NICE_USER "\0" ATTR_DAGMAN_JOB_ID "\0" ATTR_DAG_NODE_NAME "\0"  },
	{ "GLOBUS_HOST",     ATTR_GRID_RESOURCE, 0, render_globusHostAndJM, NULL },
	{ "GLOBUS_STATUS",   ATTR_GLOBUS_STATUS, 0, render_globusStatus, NULL },
	{ "GRID_JOB_ID",     ATTR_GRID_JOB_ID, 0, render_gridJobId, ATTR_GRID_RESOURCE "\0" },
	{ "GRID_RESOURCE",   ATTR_GRID_RESOURCE, 0, render_gridResource, ATTR_EC2_REMOTE_VM_NAME "\0" },
	{ "GRID_STATUS",     ATTR_GRID_JOB_STATUS, 0, render_gridStatus, ATTR_GLOBUS_STATUS "\0" },
	{ "JOB_DESCRIPTION", ATTR_JOB_CMD, 0, render_job_description, ATTR_JOB_ARGUMENTS1 "\0" ATTR_JOB_ARGUMENTS2 "\0" ATTR_JOB_DESCRIPTION "\0MATCH_EXP_" ATTR_JOB_DESCRIPTION "\0" },
	{ "JOB_ID",          ATTR_CLUSTER_ID, 0, render_job_id, ATTR_PROC_ID "\0" },
	{ "JOB_STATUS",      ATTR_JOB_STATUS, 0, render_job_status_char, ATTR_LAST_SUSPENSION_TIME "\0" ATTR_TRANSFERRING_INPUT "\0" ATTR_TRANSFERRING_OUTPUT "\0" ATTR_TRANSFER_QUEUED "\0" },
	{ "JOB_STATUS_RAW",  ATTR_JOB_STATUS, 0, format_job_status_raw, NULL },
	{ "JOB_UNIVERSE",    ATTR_JOB_UNIVERSE, 0, format_job_universe, NULL },
	{ "MEMORY_USAGE",    ATTR_IMAGE_SIZE, "%.1f", render_memory_usage, ATTR_MEMORY_USAGE "\0" },
	{ "OWNER",           ATTR_OWNER, 0, render_owner, ATTR_NICE_USER "\0" },
	{ "QDATE",           ATTR_Q_DATE, "%Y", format_q_date, NULL },
	{ "READABLE_BYTES",  ATTR_BYTES_RECVD, 0, format_readable_bytes, NULL },
	{ "READABLE_KB",     ATTR_REQUEST_DISK, 0, format_readable_kb, NULL },
	{ "READABLE_MB",     ATTR_REQUEST_MEMORY, 0, format_readable_mb, NULL },
	// PRAGMA_REMIND("format_remote_host is using ATTR_OWNER because it is StringCustomFormat, it should be AlwaysCustomFormat and ATTR_REMOTE_HOST
	{ "REMOTE_HOST",     ATTR_OWNER, 0, render_remote_host, ATTR_JOB_UNIVERSE "\0" ATTR_REMOTE_HOST "\0" ATTR_EC2_REMOTE_VM_NAME "\0" ATTR_GRID_RESOURCE "\0" },
	{ "STDU_GOODPUT",    ATTR_JOB_STATUS, "%.1f", render_goodput, ATTR_JOB_REMOTE_WALL_CLOCK "\0" ATTR_SHADOW_BIRTHDATE "\0" ATTR_LAST_CKPT_TIME "\0" },
	{ "STDU_MPBS",       ATTR_BYTES_SENT, "%.2f", render_mbps, ATTR_JOB_REMOTE_WALL_CLOCK "\0" ATTR_SHADOW_BIRTHDATE "\0" ATTR_LAST_CKPT_TIME "\0" ATTR_JOB_STATUS "\0" ATTR_BYTES_RECVD "\0"},
};
static const CustomFormatFnTable LocalPrintFormatsTable = SORTED_TOKENER_TABLE(LocalPrintFormats);

static void dump_print_mask(std::string & tmp)
{
#ifdef USE_LATE_PROJECTION
	app.mask.dump(tmp, &LocalPrintFormatsTable);
#else
	mask.dump(tmp, &LocalPrintFormatsTable);
#endif
}

static int set_print_mask_from_stream(
	AttrListPrintMask & prmask,
	const char * streamid,
	bool is_filename,
	StringList & attrs) // out
{
	std::string where_expr;
	std::string messages;
	printmask_aggregation_t aggregation;

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
					LocalPrintFormatsTable,
					prmask,
					customHeadFoot,
					aggregation,
					group_by_keys,
					where_expr,
					attrs,
					messages);
	delete pstream; pstream = NULL;
	if ( ! err) {
		//usingPrintMask = true;
		if ( ! where_expr.empty()) {
			user_job_constraint = prmask.store(where_expr.c_str());
			if (Q.addAND (user_job_constraint) != Q_OK) {
				formatstr_cat(messages, "WHERE expression is not valid: %s\n", user_job_constraint);
			}
		}
		if (aggregation) {
			if (aggregation == PR_COUNT_UNIQUE) {
				dash_autocluster = CondorQ::fetch_GroupBy;
			} else if (aggregation == PR_FROM_AUTOCLUSTER) {
				dash_autocluster = CondorQ::fetch_DefaultAutoCluster;
			}
		} else {
			// make sure that the projection has ClusterId and ProcId.
			if ( ! attrs.contains_anycase(ATTR_CLUSTER_ID)) attrs.append(ATTR_CLUSTER_ID);
			if ( ! attrs.contains_anycase(ATTR_PROC_ID)) attrs.append(ATTR_PROC_ID);
			// if we are generating a summary line, make sure that we have JobStatus
			if ( ! (customHeadFoot & HF_NOSUMMARY) && ! attrs.contains_anycase(ATTR_JOB_STATUS)) attrs.append(ATTR_JOB_STATUS);
		}
	}
	if ( ! messages.empty()) { fprintf(stderr, "%s", messages.c_str()); }
	return err;
}
