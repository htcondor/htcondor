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
#include "classad/classadCache.h" // for CachedExprEnvelope

// pass the exit code through dprintf_SetExitCode so that it knows
// whether to print out the on-error buffer or not.
#define exit(n) (exit)(dprintf_SetExitCode(n))

#ifdef HAVE_EXT_POSTGRESQL
#include "sqlquery.h"
#endif /* HAVE_EXT_POSTGRESQL */

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

//static  bool process_buffer_line_old(void*, ClassAd *);
static  bool process_job_and_render_to_dag_map(void*, ClassAd *);
static  bool process_and_print_job(void*, ClassAd *);
typedef bool (* buffer_line_processor)(void*, ClassAd*);

static 	void short_header (void);
static 	void usage (const char *, int other=0);
enum { usage_Universe=1, usage_JobStatus=2, usage_AllOther=0xFF };
static 	char * buffer_io_display (ClassAd *);
static 	char * bufferJobShort (ClassAd *);

// functions to fetch job ads and print them out
//
static bool show_file_queue(const char* jobads, const char* userlog);
static bool show_schedd_queue(const char* scheddAddress, const char* scheddName, const char* scheddMachine, bool useFastPath);
#ifdef HAVE_EXT_POSTGRESQL
static bool show_db_queue( const char* quill_name, const char* db_ipAddr, const char* db_name, const char* query_password);
#endif

static void init_output_mask();

static bool read_classad_file(const char *filename, ClassAdList &classads, ClassAdFileParseHelper* pparse_help, const char * constr);
static int read_userprio_file(const char *filename, ExtArray<PrioEntry> & prios);

/* convert the -direct aqrgument prameter into an enum */
unsigned int process_direct_argument(char *arg);

#ifdef HAVE_EXT_POSTGRESQL
/* execute a database query directly */ 
static void exec_db_query(const char *quill_name, const char *db_ipAddr, const char *db_name,const char *query_password);

/* build database connection string */
static char * getDBConnStr(char const *&, char const *&, char const *&, char const *&);

/* get the quill address for the quill_name specified */
static QueryResult getQuillAddrFromCollector(char *quill_name, char *&quill_addr);

/* avgqueuetime is used to indicate a request to query average wait time for uncompleted jobs in queue */
static  bool avgqueuetime = false;
#endif

/* Warn about schedd-wide limits that may confuse analysis code */
void warnScheddLimits(Daemon *schedd,ClassAd *job,MyString &result_buf);

/* directDBquery means we will just run a database query and return results directly to user */
static  bool directDBquery = false;

/* who is it that I should directly ask for the queue, defaults to the normal
	failover semantics */
static unsigned int direct = DIRECT_ALL;

static 	int dash_long = 0, summarize = 1, global = 0, show_io = 0, dag = 0, show_held = 0;
static  int use_xml = 0;
static  bool widescreen = false;
//static  int  use_old_code = true;
static  bool expert = false;
static  bool verbose = false; // note: this is !!NOT the same as -long !!!

static 	int malformed, running, idle, held, suspended, completed, removed;

static  char *jobads_file = NULL;
static  char *machineads_file = NULL;
static  char *userprios_file = NULL;
static  bool analyze_with_userprio = false;
static  bool dash_profile = false;
//static  bool analyze_dslots = false;

static  char *userlog_file = NULL;

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

static const char* format_owner( char*, AttrList*);
static const char* format_owner_wide( char*, AttrList*, Formatter &);

// clusterProcString is the container where the output strings are
//    stored.  We need the cluster and proc so that we can sort in an
//    orderly manner (and don't need to rely on the cluster.proc to be
//    available....)

class clusterProcString {
public:
	clusterProcString();
	int dagman_cluster_id;
	int dagman_proc_id;
	int cluster;
	int proc;
	char * string;
	clusterProcString *parent;
	std::vector<clusterProcString*> children;
};

class clusterProcMapper {
public:
	int dagman_cluster_id;
	int dagman_proc_id;
	int cluster;
	int proc;
	clusterProcMapper(const clusterProcString& cps) : dagman_cluster_id(cps.dagman_cluster_id),
		dagman_proc_id(cps.dagman_proc_id), cluster(cps.cluster), proc(cps.proc) {}
	clusterProcMapper(int dci) : dagman_cluster_id(dci), dagman_proc_id(0), cluster(dci),
		proc(0) {}
};

class clusterIDProcIDMapper {
public:
	int cluster;
	int proc;
	clusterIDProcIDMapper(const clusterProcString& cps) :  cluster(cps.cluster), proc(cps.proc) {}
	clusterIDProcIDMapper(int dci) : cluster(dci), proc(0) {}
};

class CompareProcMaps {
public:
	bool operator()(const clusterProcMapper& a, const clusterProcMapper& b) const {
		if (a.cluster < 0 || a.proc < 0) return true;
		if (b.cluster < 0 || b.proc < 0) return false;
		if (a.dagman_cluster_id < b.dagman_cluster_id) { return true; }
		if (a.dagman_cluster_id > b.dagman_cluster_id) { return false; }
		if (a.dagman_proc_id < b.dagman_proc_id) { return true; }
		if (a.dagman_proc_id > b.dagman_proc_id) { return false; }
		if (a.cluster < b.cluster ) { return true; }
		if (a.cluster > b.cluster ) { return false; }
		if (a.proc    < b.proc    ) { return true; }
		if (a.proc    > b.proc    ) { return false; }
		return false;
	}
};

class CompareProcIDMaps {
public:
	bool operator()(const clusterIDProcIDMapper& a, const clusterIDProcIDMapper& b) const {
		if (a.cluster < 0 || a.proc < 0) return true;
		if (b.cluster < 0 || b.proc < 0) return false;
		if (a.cluster < b.cluster ) { return true; }
		if (a.cluster > b.cluster ) { return false; }
		if (a.proc    < b.proc    ) { return true; }
		if (a.proc    > b.proc    ) { return false; }
		return false;
	}
};

/* To save typing */
typedef std::map<clusterProcMapper,clusterProcString*,CompareProcMaps> dag_map_type;
typedef std::map<clusterIDProcIDMapper,clusterProcString*,CompareProcIDMaps> dag_cluster_map_type;
dag_map_type dag_map;
dag_cluster_map_type dag_cluster_map;

clusterProcString::clusterProcString() : dagman_cluster_id(-1), dagman_proc_id(-1),
	cluster(-1), proc(-1), string(0), parent(0) {}

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


static	bool		usingPrintMask = false;
static 	bool		customFormat = false;
static  bool		cputime = false;
static	bool		current_run = false;
static 	bool		dash_globus = false;
static	bool		dash_grid = false;
static	bool		dash_run = false;
static	bool		dash_goodput = false;
static  const char		*JOB_TIME = "RUN_TIME";
static	bool		querySchedds 	= false;
static	bool		querySubmittors = false;
static	char		constraint[4096];
static  const char *user_job_constraint = NULL; // just the constraint given by the user
static  const char *user_slot_constraint = NULL; // just the machine constraint given by the user
static  bool        single_machine = false;
static	DCCollector* pool = NULL; 
static	char		*scheddAddr;	// used by format_remote_host()
static	AttrListPrintMask 	mask;
static  List<const char> mask_head; // The list of headings for the mask entries
//static const char * mask_headings = NULL;
static CollectorList * Collectors = NULL;

// for run failure analysis
static  int			findSubmittor( const char * );
static	void 		setupAnalysis();
static 	int			fetchSubmittorPriosFromNegotiator(ExtArray<PrioEntry> & prios);
static	void		doJobRunAnalysis(ClassAd*, Daemon*, int details);
static	const char *doJobRunAnalysisToBuffer(ClassAd*, anaCounters & ac, int details, bool noPrio, bool showMachines);
static	void		doSlotRunAnalysis( ClassAd*, JobClusterMap & clusters, Daemon*, int console_width);
static	const char *doSlotRunAnalysisToBuffer( ClassAd*, JobClusterMap & clusters, Daemon*, int console_width);
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

enum {
	detail_analyze_each_sub_expr = 0x01, // analyze each sub expression, not just the top level ones.
	detail_smart_unparse_expr    = 0x02, // show unparsed expressions at analyze step when needed
	detail_always_unparse_expr   = 0x04, // always show unparsed expressions at each analyze step
	detail_always_analyze_req    = 0x10, // always analyze requirements, (better analize does this)
	detail_dont_show_job_attrs   = 0x20, // suppress display of job attributes in verbose analyze
	detail_better                = 0x80, // -better rather than  -analyze was specified.
	detail_diagnostic            = 0x100,// show steps of expression analysis
};
static	int			analyze_detail_level = 0; // one or more of detail_xxx enum values above.

const int SHORT_BUFFER_SIZE = 8192;
const int LONG_BUFFER_SIZE = 16384;	
char return_buff[LONG_BUFFER_SIZE * 100];

char *quillName = NULL;
char *quillAddr = NULL;
char *quillMachine = NULL;
char *dbIpAddr = NULL;
char *dbName = NULL;
char *queryPassword = NULL;

StringList attrs(NULL, "\n");; // The list of attrs we want, "" for all

bool g_stream_results = false;

static void freeConnectionStrings(bool for_exit=true) {
	if(quillName) {
		free(quillName);
		quillName = NULL;
	}
	if(quillAddr) {
		free(quillAddr);
		quillAddr = NULL;
	}
	if(quillMachine) {
		free(quillMachine);
		quillMachine = NULL;
	}
	if(dbIpAddr) {
		free(dbIpAddr);
		dbIpAddr = NULL;
	}
	if(dbName) {
		free(dbName);
		dbName = NULL;
	}
	if(queryPassword) {
		free(queryPassword);
		queryPassword = NULL;
	}
	if (for_exit) {
		startdAds.Clear();
		scheddList.Clear();
	}
}

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

	PRAGMA_REMIND("TJ: revisit this code to compare dynamic slot id once that exists");
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
	virtual int PreParse(std::string & line, ClassAd & ad, FILE* file);
	virtual int OnParseError(std::string & line, ClassAd & ad, FILE* file);
	std::string schedd_name;
	std::string schedd_addr;
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
		if (starts_with(line.substr(3), "Schedd:")) {
			schedd_name = line.substr(3+8);
			size_t ix1 = schedd_name.find_first_of(": \t\n");
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
	bool        useFastScheddQuery = false;
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
		freeConnectionStrings();
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
				useFastScheddQuery = v.built_since_version(6,9,3);
			}
			
			if ( directDBquery ) {
				/* perform direct DB query if indicated and exit */
#ifdef HAVE_EXT_POSTGRESQL

					/* check if database is available */
				if (!useDB) {
					printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
					fprintf(stderr, "Database query not supported on schedd: %s\n", scheddName);
				}

				exec_db_query(NULL, NULL, NULL, NULL);
				freeConnectionStrings();
				exit(EXIT_SUCCESS);
#endif /* HAVE_EXT_POSTGRESQL */
			} 

			/* .. not a direct db query, so just happily continue ... */

			init_output_mask();
			
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
							freeConnectionStrings();
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
							freeConnectionStrings();
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
					freeConnectionStrings();
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

		freeConnectionStrings();
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
			fprintf( stderr, "Error: Couldn't add constraint %s\n", constraint);
			freeConnectionStrings();
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
		freeConnectionStrings();
		exit( 1 );
	case Q_NO_COLLECTOR_HOST:
		ASSERT( pool );
		fprintf( stderr, "Error: Can't contact condor_collector: "
				 "invalid hostname: %s\n", pool->name() );
		freeConnectionStrings();
		exit( 1 );
	default:
		fprintf( stderr, "Error fetching ads: %s\n", 
				 getStrQueryResult(result) );
		freeConnectionStrings();
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
#endif

		freeConnectionStrings(false);
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
		char		daemonAdName[128];
			// get the address of the database
		if (ad->LookupString(ATTR_QUILL_DB_IP_ADDR, &dbIpAddr) &&
			ad->LookupString(ATTR_QUILL_NAME, &quillName) &&
			ad->LookupString(ATTR_QUILL_DB_NAME, &dbName) && 
			ad->LookupString(ATTR_QUILL_DB_QUERY_PASSWORD, &queryPassword) &&
			(!ad->LookupBool(ATTR_QUILL_IS_REMOTELY_QUERYABLE,flag) || flag)) {

			/* If the quill information is available, try to use it first */
			useDB = TRUE;
	    	if (ad->LookupString(ATTR_SCHEDD_NAME, daemonAdName)) {
				Q.addSchedd(daemonAdName);
			} else {
				Q.addSchedd(scheddName);
			}

				/* get the quill info for fail-over processing */
			ASSERT(ad->LookupString(ATTR_MACHINE, &quillMachine));
		}
#endif 

		first = false;

			/* check if direct DB query is indicated */
		if ( directDBquery ) {				
#ifdef HAVE_EXT_POSTGRESQL
			if (!useDB) {
				printf ("\n\n-- Schedd: %s : %s\n", scheddName, scheddAddr);
				fprintf(stderr, "Database query not supported on schedd: %s\n",
						scheddName);
				continue;
			}
			
			exec_db_query(quillName, dbIpAddr, dbName, queryPassword);

			/* done processing the ad, so get the next one */
			continue;

#endif /* HAVE_EXT_POSTGRESQL */
		}

		init_output_mask();

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

					if((result2 == Q_OK) && quillAddr &&
					   (retval = show_schedd_queue(quillAddr, quillName, quillMachine, false))
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
						quillName, quillAddr!=NULL?quillAddr:"<\?\?\?>", 
						scheddName, scheddAddr);

					if (quillAddr == NULL) {
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
				useFastScheddQuery = v.built_since_version(6,9,3);
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
		} else {
			fprintf(stderr,"Error: Collector has no record of "
							"schedd/submitter\n");

			freeConnectionStrings();
			exit(1);
		}
	}

	freeConnectionStrings();
	exit(retval?EXIT_SUCCESS:EXIT_FAILURE);
}

// append all variable references made by expression to references list
static void
GetAllReferencesFromClassAdExpr(char const *expression,StringList &references)
{
	ClassAd ad;
	ad.GetExprReferences(expression,references,references);
}

static int
parse_analyze_detail(const char * pch, int current_details)
{
	PRAGMA_REMIND("TJ: analyze_detail_level should be enum rather than integer")
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

static void 
processCommandLineArguments (int argc, char *argv[])
{
	int i, cluster, proc;
	char *arg, *at, *daemonname;
	const char * pcolon;

	bool custom_attributes = false;
	attrs.initializeFromString("ClusterId\nProcId\nQDate\nRemoteUserCPU\nJobStatus\nServerTime\nShadowBday\nRemoteWallClockTime\nJobPrio\nImageSize\nOwner\nCmd\nArgs\nJobDescription\nTransferringInput\nTransferringOutput");

	for (i = 1; i < argc; i++)
	{
		if( *argv[i] != '-' ) {
			// no dash means this arg is a cluster/proc, proc, or owner
			if( sscanf( argv[i], "%d.%d", &cluster, &proc ) == 2 ) {
				sprintf( constraint, "((%s == %d) && (%s == %d))",
					ATTR_CLUSTER_ID, cluster, ATTR_PROC_ID, proc );
				Q.addOR( constraint );
				Q.addDBConstraint(CQ_CLUSTER_ID, cluster);
				Q.addDBConstraint(CQ_PROC_ID, proc);
				constrID.push_back(CondorID(cluster,proc,-1));
				summarize = 0;
			} 
			else if( sscanf ( argv[i], "%d", &cluster ) == 1 ) {
				sprintf( constraint, "(%s == %d)", ATTR_CLUSTER_ID, cluster );
				Q.addOR( constraint );
				Q.addDBConstraint(CQ_CLUSTER_ID, cluster);
				constrID.push_back(CondorID(cluster,-1,-1));
				summarize = 0;
			} 
			else {
				++cOwnersOnCmdline;
				if( Q.add( CQ_OWNER, argv[i] ) != Q_OK ) {
					// this error doesn't seem very helpful... can't we say more?
					fprintf( stderr, "Error: Argument %d (%s)\n", i, argv[i] );
					exit( 1 );
				}
			}
			continue;
		}

		// the argument began with a '-', so use only the part after
		// the '-' for prefix matches
		arg = argv[i]+1;

		if (is_arg_colon_prefix (arg, "wide", &pcolon, 1)) {
			widescreen = true;
			if (pcolon) { testing_width = atoi(++pcolon); }
			continue;
		}
		/*
		if (is_arg_colon_prefix (arg, "new", &pcolon, 3)) {
			use_old_code = false;
			if (pcolon) { use_old_code = (pcolon[1] == '0'); }
			fprintf(stderr, "Using %s code\n", use_old_code ? "Old" : "New");
			continue;
		}
		*/
		if (is_arg_prefix (arg, "long", 1)) {
			dash_long = 1;
			summarize = 0;
		} 
		else
		if (is_arg_prefix (arg, "xml", 3)) {
			use_xml = 1;
			dash_long = 1;
			summarize = 0;
			customFormat = true;
		}
		else
		if (is_arg_prefix (arg, "pool", 1)) {
			if( pool ) {
				delete pool;
			}
			if( ++i >= argc ) {
				fprintf( stderr,
						 "Error: Argument -pool requires a hostname as an argument.\n" );
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
		if (is_arg_prefix (arg, "D", 1)) {
			if( ++i >= argc ) {
				fprintf( stderr, 
						 "Error: Argument -%s requires a list of flags as an argument.\n", arg );
				if (!expert) {
					printf("\n");
					print_wrapped_text("Extra Info: You need to specify debug flags "
									   "as a quoted string. Common flags are D_ALL, and "
									   "D_ALWAYS.",
									   stderr);
				}
				exit( 1 );
			}
			set_debug_flags( argv[i], 0 );
		} 
		else
		if (is_arg_prefix (arg, "name", 1)) {

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
						 "Error: Argument -name requires the name of a schedd as a parameter.\n" );
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
			Q.addSchedd(daemonname);

			sprintf (constraint, "%s == \"%s\"", ATTR_QUILL_NAME, daemonname);
			scheddQuery.addORConstraint (constraint);

			delete [] daemonname;
			i++;
			querySchedds = true;
		} 
		else
		if (is_arg_prefix (arg, "direct", 1)) {
			/* check for one more argument */
			if (argc <= i+1) {
				fprintf( stderr, 
					"Error: Argument -direct requires "
						"[rdbms | quilld | schedd]\n" );
				exit(EXIT_FAILURE);
			}
			direct = process_direct_argument(argv[i+1]);
			i++;
		}
		else
		if (is_arg_prefix (arg, "submitter", 1)) {

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
				fprintf( stderr, "Error: Argument -submitter requires the name of a "
						 "user.\n");
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
		if (is_arg_prefix (arg, "constraint", 1)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument -constraint requires "
							"another parameter\n");
				exit(1);
			}
			user_job_constraint = argv[++i];
			summarize = 0;

			if (Q.addAND (user_job_constraint) != Q_OK) {
				fprintf (stderr, "Error: Argument %d (%s)\n", i, user_job_constraint);
				exit (1);
			}
		} 
		else
		if (is_arg_prefix(arg, "slotconstraint", 5) || is_arg_prefix(arg, "mconstraint", 2)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument -%s requires another parameter\n", arg);
				exit(1);
			}
			user_slot_constraint = argv[++i];
		}
		else
		if (is_arg_prefix(arg, "machine", 2)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument -%s requires another parameter\n", arg);
				exit(1);
			}
			user_slot_constraint = argv[++i];
		}
		else
		if (is_arg_prefix(arg, "address", 1)) {

			if (querySubmittors) {
				// cannot query both schedd's and submittors
				fprintf (stderr, "Cannot query both schedd's and submitters\n");
				exit(1);
			}

			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr,
						 "Error: Argument -address requires another "
						 "parameter\n" );
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
		if (is_arg_prefix(arg, "attributes", 2)) {
			if( argc <= i+1 ) {
				fprintf( stderr, "Error: Argument -attributes requires "
						 "a list of attributes to show\n" );
				exit( 1 );
			}
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
			i++;
		}
		else
		if (is_arg_prefix(arg, "format", 1)) {
				// make sure we have at least two more arguments
			if( argc <= i+2 ) {
				fprintf( stderr, "Error: Argument -format requires "
						 "format and attribute parameters\n" );
				exit( 1 );
			}
			if( !custom_attributes ) {
				custom_attributes = true;
				attrs.clearAll();
				attrs.initializeFromString("ClusterId\nProcId"); // this is needed to prevent some DAG code from faulting.
			}
			GetAllReferencesFromClassAdExpr(argv[i+2],attrs);
				
			customFormat = true;
			mask.registerFormat( argv[i+1], argv[i+2] );
			usingPrintMask = true;
			i+=2;
		}
		else
		if (is_arg_colon_prefix(arg, "autoformat", &pcolon, 5) || 
			is_arg_colon_prefix(arg, "af", &pcolon, 2)) {
				// make sure we have at least one more argument
			if ( (i+1 >= argc)  || *(argv[i+1]) == '-') {
				fprintf( stderr, "Error: Argument -autoformat requires "
						 "at last one attribute parameter\n" );
				exit( 1 );
			}
			if( !custom_attributes ) {
				custom_attributes = true;
				attrs.clearAll();
				attrs.initializeFromString("ClusterId\nProcId"); // this is needed to prevent some DAG code from faulting.
			}
			bool flabel = false;
			bool fCapV  = false;
			bool fheadings = false;
			bool fJobId = false;
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
						case 'j': fJobId = true; break;
					}
					++pcolon;
				}
			}
			if (fJobId) {
				if (fheadings || mask_head.Length() > 0) {
					mask_head.Append(" ID");
					mask_head.Append(" ");
					mask.registerFormat (flabel ? "ID = %4d." : "%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
					mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix, ATTR_PROC_ID);
				} else {
					mask.registerFormat (flabel ? "ID = %d." : "%d.", 0, FormatOptionNoSuffix, ATTR_CLUSTER_ID);
					mask.registerFormat ("%d", 0, FormatOptionNoPrefix, ATTR_PROC_ID);
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
				if (fheadings || mask_head.Length() > 0) { 
					const char * hd = fheadings ? argv[i] : "(expr)";
					wid = 0 - (int)strlen(hd); 
					opts = FormatOptionAutoWidth | FormatOptionNoTruncate; 
					mask_head.Append(hd);
				}
				else if (flabel) { lbl.formatstr("%s = ", argv[i]); wid = 0; opts = 0; }
				lbl += fCapV ? "%V" : "%v";
				mask.registerFormat(lbl.Value(), wid, opts, argv[i]);
			}
			mask.SetAutoSep(NULL, pcolpre, pcolsux, "\n");
			customFormat = true;
			usingPrintMask = true;
			// if autoformat list ends in a '-' without any characters after it, just eat the arg and keep going.
			if (i+1 < argc && '-' == (argv[i+1])[0] && 0 == (argv[i+1])[1]) {
				++i;
			}
		}
		else
		if (is_arg_prefix (arg, "global", 1)) {
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
		if (is_arg_colon_prefix(arg, "better-analyze", &pcolon, 2)
			|| is_arg_colon_prefix(arg, "better-analyse", &pcolon, 2)
			|| is_arg_colon_prefix(arg, "analyze", &pcolon, 2)
			|| is_arg_colon_prefix(arg, "analyse", &pcolon, 2)
			) {
			better_analyze = true;
			if (arg[0] == 'b') { // if better, default to higher verbosity output.
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
					//} else if (is_arg_prefix(popt, "dslots",2)) {
					//	analyze_dslots = true;
					} else {
						analyze_detail_level = parse_analyze_detail(popt, analyze_detail_level);
					}
				}
			}
			attrs.clearAll();
		}
		else
		if (is_arg_colon_prefix(arg, "reverse", &pcolon, 3)) {
			reverse_analyze = true; // modify analyze to be reverse analysis
			if (pcolon) { 
				analyze_detail_level |= parse_analyze_detail(++pcolon, analyze_detail_level);
			}
		}
		else
		if (is_arg_colon_prefix(arg, "reverse-analyze", &pcolon, 10)) {
			reverse_analyze = true; // modify analyze to be reverse analysis
			better_analyze = true;	// enable analysis
			analyze_with_userprio = false;
			if (pcolon) { 
				analyze_detail_level |= parse_analyze_detail(++pcolon, analyze_detail_level);
			}
			attrs.clearAll();
		}
		else
		if (is_arg_prefix(arg, "verbose", 4)) {
			// chatty output, mostly for for -analyze
			// this is not the same as -long. 
			verbose = true;
		}
		else
		if (is_arg_prefix(arg, "run", 1)) {
			std::string expr;
			formatstr( expr, "%s == %d || %s == %d || %s == %d", ATTR_JOB_STATUS, RUNNING,
					 ATTR_JOB_STATUS, TRANSFERRING_OUTPUT, ATTR_JOB_STATUS, SUSPENDED );
			Q.addAND( expr.c_str() );
			dash_run = true;
			if( !dag ) {
				attrs.append( ATTR_REMOTE_HOST );
				attrs.append( ATTR_JOB_UNIVERSE );
				attrs.append( ATTR_EC2_REMOTE_VM_NAME ); // for displaying HOST(s) in EC2
			}
		}
		else
		if (is_arg_prefix(arg, "hold", 2) || is_arg_prefix( arg, "held", 2)) {
			Q.add (CQ_STATUS, HELD);		
			show_held = true;
			widescreen = true;
			attrs.append( ATTR_ENTERED_CURRENT_STATUS );
			attrs.append( ATTR_HOLD_REASON );
		}
		else
		if (is_arg_prefix(arg, "goodput", 2)) {
			// goodput and show_io require the same column
			// real-estate, so they're mutually exclusive
			dash_goodput = true;
			show_io = false;
			attrs.append( ATTR_JOB_COMMITTED_TIME );
			attrs.append( ATTR_SHADOW_BIRTHDATE );
			attrs.append( ATTR_LAST_CKPT_TIME );
			attrs.append( ATTR_JOB_REMOTE_WALL_CLOCK );
		}
		else
		if (is_arg_prefix(arg, "cputime", 2)) {
			cputime = true;
			JOB_TIME = "CPU_TIME";
		 	attrs.append( ATTR_JOB_REMOTE_USER_CPU );
		}
		else
		if (is_arg_prefix(arg, "currentrun", 2)) {
			current_run = true;
		}
		else
		if (is_arg_prefix(arg, "grid", 2 )) {
			// grid is a superset of globus, so we can't do grid if globus has been specifed
			if ( ! dash_globus) {
				dash_grid = true;
				attrs.append( ATTR_GRID_JOB_ID );
				attrs.append( ATTR_GRID_RESOURCE );
				attrs.append( ATTR_GRID_JOB_STATUS );
				attrs.append( ATTR_GLOBUS_STATUS );
    			attrs.append( ATTR_EC2_REMOTE_VM_NAME );
				Q.addAND( "JobUniverse == 9" );
			}
		}
		else
		if (is_arg_prefix(arg, "globus", 5 )) {
			Q.addAND( "GlobusStatus =!= UNDEFINED" );
			dash_globus = true;
			if (dash_grid) {
				dash_grid = false;
			} else {
				attrs.append( ATTR_GLOBUS_STATUS );
				attrs.append( ATTR_GRID_RESOURCE );
				attrs.append( ATTR_GRID_JOB_STATUS );
				attrs.append( ATTR_GRID_JOB_ID );
			}
		}
		else
		if (is_arg_colon_prefix(arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", 0);
			if (pcolon && pcolon[1]) {
				set_debug_flags( ++pcolon, 0 );
			}
		}
		else
		if (is_arg_prefix(arg, "io", 1)) {
			// goodput and show_io require the same column
			// real-estate, so they're mutually exclusive
			show_io = true;
			dash_goodput = false;
			attrs.append(ATTR_FILE_READ_BYTES);
			attrs.append(ATTR_FILE_WRITE_BYTES);
			attrs.append(ATTR_FILE_SEEK_COUNT);
			attrs.append(ATTR_JOB_REMOTE_WALL_CLOCK);
			attrs.append(ATTR_BUFFER_SIZE);
			attrs.append(ATTR_BUFFER_BLOCK_SIZE);
		}
		else if (is_arg_prefix(arg, "dag", 2)) {
			dag = true;
			attrs.clearAll();
			if( g_stream_results  ) {
				fprintf( stderr, "-stream-results and -dag are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else if (is_arg_prefix(arg, "expert", 1)) {
			expert = true;
			attrs.clearAll();
		}
		else if (is_arg_prefix(arg, "jobads", 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument -jobads requires a filename\n");
				exit(1);
			} else {
				i++;
				jobads_file = strdup(argv[i]);
			}
		}
		else if (is_arg_prefix(arg, "userlog", 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument -userlog requires a filename\n");
				exit(1);
			} else {
				i++;
				userlog_file = strdup(argv[i]);
			}
		}
		else if (is_arg_prefix(arg, "slotads", 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument -slotads requires a filename\n");
				exit(1);
			} else {
				i++;
				machineads_file = strdup(argv[i]);
			}
		}
		else if (is_arg_colon_prefix(arg, "userprios", &pcolon, 5)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument -userprios requires a filename\n");
				exit(1);
			} else {
				i++;
				userprios_file = strdup(argv[i]);
				analyze_with_userprio = true;
			}
		}
		else if (is_arg_colon_prefix(arg, "nouserprios", &pcolon, 7)) {
			analyze_with_userprio = false;
		}
#ifdef HAVE_EXT_POSTGRESQL
		else if (match_prefix(arg, "avgqueuetime")) {
				/* if user want average wait time, we will perform direct DB query */
			avgqueuetime = true;
			directDBquery =  true;
		}
#endif /* HAVE_EXT_POSTGRESQL */
        else if (is_arg_prefix(arg, "version", 1)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			exit(0);
        }
		else if (is_arg_colon_prefix(arg, "profile", &pcolon, 4)) {
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
		if (is_arg_prefix (arg, "stream", 2)) {
			g_stream_results = true;
			if( dag ) {
				fprintf( stderr, "-stream-results and -dag are incompatible\n" );
				usage( argv[0] );
				exit( 1 );
			}
		}
		else {
			fprintf( stderr, "Error: unrecognized argument -%s\n", arg );
			usage(argv[0]);
			exit( 1 );
		}
	}

		//Added so -long or -xml can be listed before other options
	if(dash_long && !custom_attributes) {
		attrs.clearAll();
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
		"Error: Argument -direct requires [rdbms | quilld | schedd]\n" ); 
/*		"Error: Argument -direct requires [rdbms | schedd]\n" ); */
#else
	fprintf( stderr, 
		"Error: Quill feature set is not available.\n"
		"Error: Argument -direct may only take 'schedd' as an option.\n" );
#endif

	exit(EXIT_FAILURE);

	/* Here to make the compiler happy since there is an exit above */
	return DIRECT_UNKNOWN;
}


static char *
buffer_io_display( ClassAd *ad )
{
	int cluster=0, proc=0;
	float read_bytes=0, write_bytes=0, seek_count=0;
	int buffer_size=0, block_size=0;
	float wall_clock=-1;

	char owner[256];

	ad->EvalInteger(ATTR_CLUSTER_ID,NULL,cluster);
	ad->EvalInteger(ATTR_PROC_ID,NULL,proc);
	ad->EvalString(ATTR_OWNER,NULL,owner);

	ad->EvalFloat(ATTR_FILE_READ_BYTES,NULL,read_bytes);
	ad->EvalFloat(ATTR_FILE_WRITE_BYTES,NULL,write_bytes);
	ad->EvalFloat(ATTR_FILE_SEEK_COUNT,NULL,seek_count);
	ad->EvalFloat(ATTR_JOB_REMOTE_WALL_CLOCK,NULL,wall_clock);
	ad->EvalInteger(ATTR_BUFFER_SIZE,NULL,buffer_size);
	ad->EvalInteger(ATTR_BUFFER_BLOCK_SIZE,NULL,block_size);

	sprintf( return_buff, "%4d.%-3d %-14s", cluster, proc,
			 format_owner( owner, ad ) );

	/* If the jobAd values are not set, OR the values are all zero,
	   report no data collected.  This could be true for a vanilla
	   job, or for a standard job that has not checkpointed yet. */

	if(wall_clock<0 || ((read_bytes < 1.0) && (write_bytes < 1.0) && (seek_count < 1.0)) ) {
		strcat(return_buff, "   [ no i/o data collected ]\n");
	} else {
		if(wall_clock < 1.0) wall_clock=1;

		/*
		Note: metric_units() cannot be used twice in the same
		statement -- it returns a pointer to a static buffer.
		*/

		sprintf( return_buff,"%s %8s", return_buff, metric_units(read_bytes) );
		sprintf( return_buff,"%s %8s", return_buff, metric_units(write_bytes) );
		sprintf( return_buff,"%s %8.0f", return_buff, seek_count );
		sprintf( return_buff,"%s %8s/s", return_buff, metric_units((int)((read_bytes+write_bytes)/wall_clock)) );
		sprintf( return_buff,"%s %8s", return_buff, metric_units(buffer_size) );
		sprintf( return_buff,"%s %8s\n", return_buff, metric_units(block_size) );
	}
	return ( return_buff );
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

	MyString args_string;
	ArgList::GetArgsStringForDisplay(ad,&args_string);

	std::string description;

	ad->EvalString(ATTR_JOB_DESCRIPTION, NULL, description);
	if ( !description.empty() ){
		buffer.formatstr("%s", description.c_str());
	}
	else if (!args_string.IsEmpty()) {
		buffer.formatstr( "%s %s", condor_basename(cmd), args_string.Value() );
	} else {
		buffer.formatstr( "%s", condor_basename(cmd) );
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
			            : description.empty() ? "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f %-18.18s\n"
				                           : "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f (%-17.17s)\n",
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
	printf( " %-7s %-14s ", "ID", dag ? "OWNER/NODENAME" : "OWNER" );
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
		printf( "%8s %8s %8s %10s %8s %8s\n",
				"READ", "WRITE", "SEEK", "XPUT", "BUFSIZE", "BLKSIZE" );
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

static char *
format_remote_host (char *, AttrList *ad)
{
	static char host_result[MAXHOSTNAMELEN];
	static char unknownHost [] = "[????????????????]";
	condor_sockaddr addr;

	int universe = CONDOR_UNIVERSE_STANDARD;
	ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	if (((universe == CONDOR_UNIVERSE_SCHEDULER) || (universe == CONDOR_UNIVERSE_LOCAL)) &&
		addr.from_sinful(scheddAddr) == true) {
		MyString hostname = get_hostname(addr);
		if (hostname.Length() > 0) {
			strcpy( host_result, hostname.Value() );
			return host_result;
		} else {
			return unknownHost;
		}
	} else if (universe == CONDOR_UNIVERSE_GRID) {
		if (ad->LookupString(ATTR_EC2_REMOTE_VM_NAME,host_result,sizeof(host_result)) == 1)
			return host_result;
		else if (ad->LookupString(ATTR_GRID_RESOURCE,host_result, sizeof(host_result)) == 1 )
			return host_result;
		else
			return unknownHost;
	}

	if (ad->LookupString(ATTR_REMOTE_HOST, host_result, sizeof(host_result)) == 1) {
		if( is_valid_sinful(host_result) && 
			addr.from_sinful(host_result) == true ) {
			MyString hostname = get_hostname(addr);
			if (hostname.Length() > 0) {
				strcpy(host_result, hostname.Value());
			} else {
				return unknownHost;
			}
		}
		return host_result;
	}
	return unknownHost;
}

static const char *
format_cpu_time (double utime, AttrList *ad)
{
	return format_time( (int) job_time(utime,(ClassAd *)ad) );
}

static const char *
format_goodput (int job_status, AttrList *ad)
{
	static char put_result[9];
	int ckpt_time = 0, shadow_bday = 0, last_ckpt = 0;
	float wall_clock = 0.0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time );
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	ad->LookupInteger( ATTR_LAST_CKPT_TIME, last_ckpt );
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, wall_clock );
	if ((job_status == RUNNING || job_status == TRANSFERRING_OUTPUT || job_status == SUSPENDED) &&
		shadow_bday && last_ckpt > shadow_bday)
	{
		wall_clock += last_ckpt - shadow_bday;
	}
	if (wall_clock <= 0.0) return " [?????]";
	float goodput_time = ckpt_time/wall_clock*100.0;
	if (goodput_time > 100.0) goodput_time = 100.0;
	else if (goodput_time < 0.0) return " [?????]";
	sprintf(put_result, " %6.1f%%", goodput_time);
	return put_result;
}

static const char *
format_mbps (double bytes_sent, AttrList *ad)
{
	static char result_format[10];
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
	if (total_mbits <= 0) return " [????]";
	sprintf(result_format, " %6.2f", total_mbits/wall_clock);
	return result_format;
}

static const char *
format_cpu_util (double utime, AttrList *ad)
{
	static char result_format[10];
	int ckpt_time = 0;
	ad->LookupInteger( ATTR_JOB_COMMITTED_TIME, ckpt_time);
	if (ckpt_time == 0) return " [??????]";
	double util = utime/ckpt_time*100.0;
	if (util > 100.0) util = 100.0;
	else if (util < 0.0) return " [??????]";
	sprintf(result_format, "  %6.1f%%", util);
	return result_format;
}

static const char *
format_owner_common (char *owner, AttrList *ad)
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

	if ( dag && ad->LookupExpr( ATTR_DAGMAN_JOB_ID ) ) {
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
format_owner (char *owner, AttrList *ad)
{
	static char result_format[24];
	sprintf(result_format, "%-14.14s", format_owner_common(owner, ad));
	return result_format;
}

static const char *
format_owner_wide (char *owner, AttrList *ad, Formatter & /*fmt*/)
{
	return format_owner_common(owner, ad);
}

static const char *
format_globusStatus( int globusStatus, AttrList * /* ad */ )
{
	static char result_str[64];
#if defined(HAVE_EXT_GLOBUS)
	sprintf(result_str, " %7.7s", GlobusJobStatusName( globusStatus ) );
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
			return gram_states[ii].psz;
		}
	}
	sprintf(result_str, "%d", globusStatus);
#endif
	return result_str;
}

// The remote hostname may be in GlobusResource or GridResource.
// We want this function to be called if at least one is defined,
// but it will only be called if the one attribute it's registered
// with is defined. So we register it with an attribute we know will
// always be present and be a string. We then ignore that attribute
// and examine GlobusResource and GridResource.
static const char *
format_globusHostAndJM( char *, AttrList *ad )
{
	static char result_format[64];
	char	host[80] = "[?????]";
	char	jm[80] = "fork";
	char	*tmp;
	int	p;
	char *attr_value = NULL;
	char *resource_name = NULL;
	char *grid_type = NULL;

	if ( ad->LookupString( ATTR_GRID_RESOURCE, &attr_value ) ) {
			// If ATTR_GRID_RESOURCE exists, skip past the initial
			// '<job type> '.
		resource_name = strchr( attr_value, ' ' );
		if ( resource_name ) {
			*resource_name = '\0';
			grid_type = strdup( attr_value );
			resource_name++;
		}
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
	sprintf( result_format, " %-8.8s %-18.18s  ", jm, host );
	return( result_format );
}

static const char *
format_gridStatus( int jobStatus, AttrList * ad, Formatter & /*fmt*/ )
{
	static char result_str[32];
	if (ad->LookupString( ATTR_GRID_JOB_STATUS, result_str, COUNTOF(result_str) )) {
		return result_str;
	} 
	int globusStatus = 0;
	if (ad->LookupInteger( ATTR_GLOBUS_STATUS, globusStatus )) {
		return format_globusStatus(globusStatus, ad);
	} 
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
			return states[ii].psz;
		}
	}
	sprintf(result_str, "%d", jobStatus);
	return result_str;
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

static const char *
format_gridResource( char * grid_res, AttrList * ad, Formatter & /*fmt*/ )
{
	std::string grid_type;
	std::string str = grid_res;
	std::string mgr = "[?]";
	std::string host = "[???]";
	const bool fshow_host_port = false;
	const unsigned int width = 1+6+1+8+1+18+1;

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

	//if ((MATCH == grid_type.find("gt")) || (MATCH == grid_type.compare("globus"))){
	//	return format_globusHostAndJM( grid_res, ad );
	//}

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
	return result_str;
}

static const char *
format_gridJobId( char * grid_jobid, AttrList *ad, Formatter & /*fmt*/ )
{
	std::string str = grid_jobid;
	std::string jid;
	std::string host;

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
		jid += host;
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
		//jid = grid_type;
		//jid += " : ";
		jid += str.substr(ix4);
	}

	static char result_str[1024];
	sprintf(result_str, "%s", jid.c_str());
	return result_str;
}

static const char *
format_q_date (int d, AttrList *)
{
	return format_date(d);
}


static void
usage (const char *myName, int other)
{
	printf ("Usage: %s [general-opts] [restriction-list] [output-opts | analyze-opts]\n", myName);
	printf ("    [general-opts] are\n"
		"\t-global\t\t\tQuery all Schedulers in this pool\n"
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
		"    [restriction-list] each restriction may be one of\n"
		"\t<cluster>\t\tGet information about specific cluster\n"
		"\t<cluster>.<proc>\tGet information about specific job\n"
		"\t<owner>\t\t\tInformation about jobs owned by <owner>\n"
		"\t-constraint <expr>\tGet information about jobs that match <expr>\n"
		"    [output-opts] are\n"
		"\t-cputime\t\tDisplay CPU_TIME instead of RUN_TIME\n"
		"\t-currentrun\t\tDisplay times only for current run\n"
		"\t-debug\t\t\tDisplay debugging info to console\n"
		"\t-dag\t\t\tSort DAG jobs under their DAGMan\n"
		"\t-expert\t\t\tDisplay shorter error messages\n"
		"\t-globus\t\t\tGet information about jobs of type globus\n"
		"\t-goodput\t\tDisplay job goodput statistics\n"	
		"\t-help [Universe|State]\tDisplay this screen, JobUniverses, JobStates\n"
		"\t-hold\t\t\tGet information about jobs on hold\n"
		"\t-io\t\t\tShow information regarding I/O\n"
		"\t-run\t\t\tGet information about running jobs\n"
		"\t-stream-results \tProduce output as jobs are fetched\n"
		"\t-version\t\tPrint the HTCondor version and exit\n"
		"\t-wide\t\t\tWidescreen output\n"
		"\t-autoformat[:V,ntlh] <attr> [attr2 [attr3 ...]]\n"
		"\t\t\t\tPrint attr(s) with automatic formatting\n"
		"\t-format <fmt> <attr>\tPrint attribute attr using format fmt\n"
		"\t-long\t\t\tDisplay entire ClassAds\n"
		"\t-xml\t\t\tDisplay entire ClassAds, but in XML\n"
		"\t-attributes X,Y,...\tAttributes to show in -xml and -long\n"
		"    [analyze-opts] are\n"
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
	if (! customFormat && !dash_long ) {
		printf ("\n\n-- %s\n", source_label);
			// Print the output header
		if (usingPrintMask && mask_head.Length() > 0) {
			mask.display_Headings(stdout, mask_head);
		} else if ( ! better_analyze) {
			short_header();
		}
	}
	if (customFormat && mask_head.Length() > 0) {
		mask.display_Headings(stdout, mask_head);
	}
	if( use_xml ) {
			// keep this consistent with AttrListList::fPrintAttrListList()
		std::string xml;
		AddClassAdXMLFileHeader(xml);
		printf("%s\n", xml.c_str());
	}
}

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

static void init_output_mask()
{
	// just  in case we re-enter this function, only setup the mask once
	static bool	setup_mask = false;
	// TJ: before my 2012 refactoring setup_mask didn't protect summarize, so I'm preserving that.
	if ( dash_run || dash_goodput || dash_globus || dash_grid ) 
		summarize = false;
	else if (customFormat && ! show_held)
		summarize = false;

	if (setup_mask)
		return;
	setup_mask = true;

	int console_width = getDisplayWidth();

	const char * mask_headings = NULL;

	if (dash_run || dash_goodput) {
		//mask.SetAutoSep("<","{","},",">\n"); // for testing.
		mask.SetAutoSep(NULL," ",NULL,"\n");
		mask.registerFormat ("%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
		mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix, ATTR_PROC_ID);
		mask.registerFormat (NULL, -14, 0, format_owner_wide, ATTR_OWNER, "[????????????]" );
		//mask.registerFormat(" ", "*bogus*", " ");  // force space
		mask.registerFormat (NULL,  11, 0, (IntCustomFmt) format_q_date, ATTR_Q_DATE, "[????????????]");
		//mask.registerFormat(" ", "*bogus*", " ");  // force space
		mask.registerFormat (NULL,  12, 0, (FloatCustomFmt) format_cpu_time,
							  ATTR_JOB_REMOTE_USER_CPU,
							  "[??????????]");
		if (dash_run && ! dash_goodput) {
			mask_headings = (cputime) ? " ID\0 \0OWNER\0  SUBMITTED\0    CPU_TIME\0HOST(S)\0"
			                          : " ID\0 \0OWNER\0  SUBMITTED\0    RUN_TIME\0HOST(S)\0";
			//mask.registerFormat(" ", "*bogus*", " "); // force space
			// We send in ATTR_OWNER since we know it is always
			// defined, and we need to make sure
			// format_remote_host() is always called. We are
			// actually displaying ATTR_REMOTE_HOST if defined,
			// but we play some tricks if it isn't defined.
			mask.registerFormat ( (StringCustomFmt) format_remote_host,
								  ATTR_OWNER, "[????????????????]");
		} else {			// goodput
			mask_headings = (cputime) ? " ID\0 \0OWNER\0  SUBMITTED\0    CPU_TIME\0GOODPUT\0CPU_UTIL\0Mb/s\0"
			                          : " ID\0 \0OWNER\0  SUBMITTED\0    RUN_TIME\0GOODPUT\0CPU_UTIL\0Mb/s\0";
			mask.registerFormat (NULL, 8, 0, (IntCustomFmt) format_goodput,
								  ATTR_JOB_STATUS,
								  "[?????]");
			mask.registerFormat (NULL, 9, 0, (FloatCustomFmt) format_cpu_util,
								  ATTR_JOB_REMOTE_USER_CPU,
								  "[??????]");
			mask.registerFormat (NULL, 7, 0, (FloatCustomFmt) format_mbps,
								  ATTR_BYTES_SENT,
								  "[????]");
		}
		//mask.registerFormat("\n", "*bogus*", "\n");  // force newline
		usingPrintMask = true;
	} else if (dash_globus) {
		mask_headings = " ID\0 \0OWNER\0STATUS\0MANAGER     HOST\0EXECUTABLE\0";
		mask.SetAutoSep(NULL," ",NULL,"\n");
		mask.registerFormat ("%4d.", 5, FormatOptionAutoWidth | FormatOptionNoSuffix, ATTR_CLUSTER_ID);
		mask.registerFormat ("%-3d", 3, FormatOptionAutoWidth | FormatOptionNoPrefix,  ATTR_PROC_ID);
		mask.registerFormat (NULL, -14, 0, format_owner_wide, ATTR_OWNER, "[?]" );
		mask.registerFormat(NULL, -8, 0, (IntCustomFmt) format_globusStatus, ATTR_GLOBUS_STATUS, "[?]" );
		if (widescreen) {
			mask.registerFormat(NULL, -30, FormatOptionAutoWidth | FormatOptionNoTruncate, 
				(StringCustomFmt)format_globusHostAndJM, ATTR_GRID_RESOURCE, "fork    [?????]" );
			mask.registerFormat("%v", -18, FormatOptionAutoWidth | FormatOptionNoTruncate, ATTR_JOB_CMD );
		} else {
			mask.registerFormat(NULL, 30, 0, 
				(StringCustomFmt)format_globusHostAndJM, ATTR_JOB_CMD, "fork    [?????]" );
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
		mask.registerFormat (NULL, -14, 0, format_owner_wide, ATTR_OWNER, "[????????????]" );
		//mask.registerFormat(" ", "*bogus*", " ");  // force space
		mask.registerFormat (NULL, 11, 0, (IntCustomFmt) format_q_date,
							  ATTR_ENTERED_CURRENT_STATUS, "[????????????]");
		//mask.registerFormat(" ", "*bogus*", " ");  // force space
		if (widescreen) {
			mask.registerFormat("%v", -43, FormatOptionAutoWidth | FormatOptionNoTruncate, ATTR_HOLD_REASON );
		} else {
			mask.registerFormat("%-43.43s", ATTR_HOLD_REASON);
		}
		usingPrintMask = true;
	}

	if (mask_headings) {
		const char * pszz = mask_headings;
		size_t cch = strlen(pszz);
		while (cch > 0) {
			mask_head.Append(pszz);
			pszz += cch+1;
			cch = strlen(pszz);
		}
	}
}

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
AddToClassAdList(void * pv, ClassAd *ad) {
	ClassAdList * plist = (ClassAdList*)pv;
	plist->Insert(ad);
	// return false to indicate we took ownership of the ad 
	// and it should NOT be deleted by the caller.
	return false;
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
		if (pfnProcess || pvProcess) {}
#endif /* HAVE_EXT_POSTGRESQL */

	// if we streamed, then the results have already been printed out.
	// we just need to write the footer/summary
	if (g_stream_results) {
		print_full_footer();
		return true;
	}

	if (better_analyze) {
		print_full_header(source_label.c_str());
		return print_jobs_analysis(jobs, NULL);
	}

	// at this point we either have a populated jobs list, or a populated dag_map
	// depending on which processing function we used in the query above.
	// for -long -xml or -analyze, we should have jobs, but no dag_map

	// if outputing dag format, we want to build the parent child links in the dag map
	// and then edit the text for jobs that are dag nodes.
	if (dag && (dag_map.size() > 0)) {
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


#if 0
// tj: 2013 refactor---
// so we can compare the output of old vs. new code. keep both around for now.
static bool
process_buffer_line_old(void *, ClassAd *job)
{
	int status = 0;

	clusterProcString * tempCPS = new clusterProcString;

	job->LookupInteger( ATTR_CLUSTER_ID, tempCPS->cluster );
	job->LookupInteger( ATTR_PROC_ID, tempCPS->proc );
	job->LookupInteger( ATTR_JOB_STATUS, status );

	switch (status)
	{
		case IDLE:                idle++;      break;
		case TRANSFERRING_OUTPUT:
		case RUNNING:             running++;   break;
		case SUSPENDED:           suspended++; break;
		case COMPLETED:           completed++; break;
		case REMOVED:             removed++;   break;
		case HELD:		          held++;	   break;
	}

	// If it's not a DAGMan job (and this includes the DAGMan process
	// itself), then set the dagman_cluster_id equal to cluster so that
	// it sorts properly against dagman jobs.
	char dagman_job_string[32];
	bool dci_initialized = false;
	if (!job->LookupString(ATTR_DAGMAN_JOB_ID, dagman_job_string, sizeof(dagman_job_string))) {
			// we failed to find an DAGManJobId string attribute, 
			// let's see if we find one that is an integer.
		int temp_cluster = -1;
		if (!job->LookupInteger(ATTR_DAGMAN_JOB_ID,temp_cluster)) {
				// could not job DAGManJobId, so fall back on 
				// just the regular job id
			tempCPS->dagman_cluster_id = tempCPS->cluster;
			tempCPS->dagman_proc_id    = tempCPS->proc;
			dci_initialized = true;
		} else {
				// in this case, we found DAGManJobId set as
				// an integer, not a string --- this means it is
				// the cluster id.
			tempCPS->dagman_cluster_id = temp_cluster;
			tempCPS->dagman_proc_id    = 0;
			dci_initialized = true;
		}
	} else {
		// We've gotten a string, probably something like "201.0"
		// we want to convert it to the numbers 201 and 0. To be safe, we
		// use atoi on either side of the period. We could just use
		// sscanf, but I want to know each fail case, so I can set them
		// to reasonable values. 
		char *loc_period = strchr(dagman_job_string, '.');
		char *proc_string_start = NULL;
		if (loc_period != NULL) {
			*loc_period = 0;
			proc_string_start = loc_period+1;
		}
		if (isdigit(*dagman_job_string)) {
			tempCPS->dagman_cluster_id = atoi(dagman_job_string);
			dci_initialized = true;
		} else {
			// It must not be a cluster id, because it's not a number.
			tempCPS->dagman_cluster_id = tempCPS->cluster;
			dci_initialized = true;
		}

		if (proc_string_start != NULL && isdigit(*proc_string_start)) {
			tempCPS->dagman_proc_id = atoi(proc_string_start);
			dci_initialized = true;
		} else {
			tempCPS->dagman_proc_id = 0;
			dci_initialized = true;
		}
	}
	if( !g_stream_results && dci_initialized ) {
		clusterProcMapper cpm(*tempCPS);
		std::pair<dag_map_type::iterator,bool> pp = dag_map.insert(
			std::pair<clusterProcMapper,clusterProcString*>(
				cpm, tempCPS ) );
		if( !pp.second ) {
			fprintf( stderr, "Error: Two clusters with the same ID.\n" );
			fprintf( stderr, "tempCPS: %d %d %d %d\n", tempCPS->dagman_cluster_id,
				tempCPS->dagman_proc_id, tempCPS->cluster, tempCPS->proc );
			dag_map_type::iterator ppp = dag_map.find(cpm);
			fprintf( stderr, "Comparing against: %d %d %d %d\n",
				ppp->second->dagman_cluster_id, ppp->second->dagman_proc_id,
				ppp->second->cluster, ppp->second->proc );
			exit( 1 );
		}
		clusterIDProcIDMapper cipim(*tempCPS);
		std::pair<dag_cluster_map_type::iterator,bool> pq = dag_cluster_map.insert(
			std::pair<clusterIDProcIDMapper,clusterProcString*>(
				cipim,tempCPS ) );
		if( !pq.second ) {
			fprintf( stderr, "Error: Clusters have nonunique IDs.\n" );
			exit( 1 );
		}
	}

	if (use_xml) {
		std::string s;
		StringList *attr_white_list = attrs.isEmpty() ? NULL : &attrs;
		sPrintAdAsXML(s,*job,attr_white_list);
		tempCPS->string = strnewp( s.c_str() );
	} else if( dash_long ) {
		MyString s;
		StringList *attr_white_list = attrs.isEmpty() ? NULL : &attrs;
		sPrintAd(s, *job, attr_white_list);
		s += "\n";
		tempCPS->string = strnewp( s.Value() );
	} else if( better_analyze ) {
		ASSERT(0);
	} else if ( show_io ) {
		tempCPS->string = strnewp( buffer_io_display( job ) );
	} else if ( usingPrintMask ) {
		char * tempSTR = mask.display ( job );
		// strnewp the mask.display return, since its an 8k string.
		tempCPS->string = strnewp( tempSTR );
		delete [] tempSTR;
	} else {
		tempCPS->string = strnewp( bufferJobShort( job ) );
	}

	if( g_stream_results ) {
		printf("%s",tempCPS->string);
		delete[] tempCPS->string;
		delete tempCPS;
		tempCPS = NULL;
	}

	// process_buffer_line returns 1 so that the ad that is passed
	// to it should be deleted.
	return true;
}
#endif

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


static const char * render_job_text(ClassAd *job, std::string & result_text)
{
	if (use_xml) {
		sPrintAdAsXML(result_text, *job, attrs.isEmpty() ? NULL : &attrs);
	} else if (dash_long) {
		sPrintAd(result_text, *job, false, attrs.isEmpty() ? NULL : &attrs);
		result_text += "\n";
	} else if (better_analyze) {
		ASSERT(0); // should never get here.
	} else if (show_io) {
		result_text += buffer_io_display(job);
	} else if (usingPrintMask) {
		result_text += mask.display(job);
	} else {
		result_text += bufferJobShort(job);
	}
	return result_text.c_str();
}

static bool
process_and_print_job(void *, ClassAd *job)
{
	count_job(job);

	std::string result_text;
	render_job_text(job, result_text);
	printf("%s", result_text.c_str());

	// return true to free the job ad, since we didn't take ownership of it.
	return true;
}

// insert a line of formatted output into the dag_map, so we can print out
// sorted results.
//
static bool
insert_job_output_into_dag_map(ClassAd * job, const char * job_output)
{
	clusterProcString * tempCPS = new clusterProcString;
	job->LookupInteger( ATTR_CLUSTER_ID, tempCPS->cluster );
	job->LookupInteger( ATTR_PROC_ID, tempCPS->proc );

	// If it's not a DAGMan job (and this includes the DAGMan process
	// itself), then set the dagman_cluster_id equal to cluster so that
	// it sorts properly against dagman jobs.
	char dagman_job_string[32];
	bool dci_initialized = false;
	if (!job->LookupString(ATTR_DAGMAN_JOB_ID, dagman_job_string, sizeof(dagman_job_string))) {
			// we failed to find an DAGManJobId string attribute,
			// let's see if we find one that is an integer.
		int temp_cluster = -1;
		if (!job->LookupInteger(ATTR_DAGMAN_JOB_ID,temp_cluster)) {
				// could not job DAGManJobId, so fall back on
				// just the regular job id
			tempCPS->dagman_cluster_id = tempCPS->cluster;
			tempCPS->dagman_proc_id    = tempCPS->proc;
			dci_initialized = true;
		} else {
				// in this case, we found DAGManJobId set as
				// an integer, not a string --- this means it is
				// the cluster id.
			tempCPS->dagman_cluster_id = temp_cluster;
			tempCPS->dagman_proc_id    = 0;
			dci_initialized = true;
		}
	} else {
		// We've gotten a string, probably something like "201.0"
		// we want to convert it to the numbers 201 and 0. To be safe, we
		// use atoi on either side of the period. We could just use
		// sscanf, but I want to know each fail case, so I can set them
		// to reasonable values.
		char *loc_period = strchr(dagman_job_string, '.');
		char *proc_string_start = NULL;
		if (loc_period != NULL) {
			*loc_period = 0;
			proc_string_start = loc_period+1;
		}
		if (isdigit(*dagman_job_string)) {
			tempCPS->dagman_cluster_id = atoi(dagman_job_string);
			dci_initialized = true;
		} else {
			// It must not be a cluster id, because it's not a number.
			tempCPS->dagman_cluster_id = tempCPS->cluster;
			dci_initialized = true;
		}

		if (proc_string_start != NULL && isdigit(*proc_string_start)) {
			tempCPS->dagman_proc_id = atoi(proc_string_start);
			dci_initialized = true;
		} else {
			tempCPS->dagman_proc_id = 0;
			dci_initialized = true;
		}
	}

	if ( ! dci_initialized) {
		return false;
	}

	tempCPS->string = strnewp(job_output);
	PRAGMA_REMIND("tj: change tempCPS->string to a set of fields, so we can set column widths based on data.")

	// insert tempCPS (and job_output copy) into the dag_map.
	// this transfers ownership of both allocations, they will be freed in the code
	// that deletes the map.
	clusterProcMapper cpm(*tempCPS);
	std::pair<dag_map_type::iterator,bool> pp = dag_map.insert(
		std::pair<clusterProcMapper,clusterProcString*>(
			cpm, tempCPS ) );
	if( !pp.second ) {
		fprintf( stderr, "Error: Two clusters with the same ID.\n" );
		fprintf( stderr, "tempCPS: %d %d %d %d\n", tempCPS->dagman_cluster_id,
			tempCPS->dagman_proc_id, tempCPS->cluster, tempCPS->proc );
		dag_map_type::iterator ppp = dag_map.find(cpm);
		fprintf( stderr, "Comparing against: %d %d %d %d\n",
			ppp->second->dagman_cluster_id, ppp->second->dagman_proc_id,
			ppp->second->cluster, ppp->second->proc );
		//tj: 2013 -don't abort without printing jobs
		// exit( 1 );
		//maybe we should toss tempCPS into an orphan list rather than simply leaking it?
		return false;
	}

	// also insert into the dag_cluster_map
	//
	clusterIDProcIDMapper cipim(*tempCPS);
	std::pair<dag_cluster_map_type::iterator,bool> pq = dag_cluster_map.insert(
		std::pair<clusterIDProcIDMapper,clusterProcString*>(
			cipim,tempCPS ) );
	if( !pq.second ) {
		fprintf( stderr, "Error: Clusters have nonunique IDs.\n" );
		//tj: 2013 -don't abort without printing jobs
		//exit( 1 );
	}

	return true;
}

static bool
process_job_and_render_to_dag_map(void *,  ClassAd *job)
{
	count_job(job);

	ASSERT( ! g_stream_results);

	std::string result_text;
	render_job_text(job, result_text);
	insert_job_output_into_dag_map(job, result_text.c_str());

	// process_buffer_line returns 1 so that the ad that is passed
	// to it should be deleted.
	return true;
}

// query SCHEDD or QUILLD daemon for jobs. and then print out the desired job info.
// this function handles -analyze, -streaming, -dag and all normal condor_q output
// when the source is a SCHEDD or QUILLD.
// TJ isn't sure that the QUILL daemon can use fast path, prior to the 2013 refactor, 
// the old code didn't try.
//
static bool
show_schedd_queue(const char* scheddAddress, const char* scheddName, const char* scheddMachine, bool useFastPath)
{
	// initialize counters
	malformed = idle = running = held = completed = suspended = 0;

	std::string source_label;
	if (querySchedds) {
		formatstr(source_label, "Schedd: %s : %s", scheddName, scheddAddress);
	} else {
		formatstr(source_label, "Submitter: %s : %s : %s", scheddName, scheddAddress, scheddMachine);
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
	if (dash_long || better_analyze) {
		pfnProcess = AddToClassAdList;
		pvProcess = &jobs;
	} else if (g_stream_results) {
		pfnProcess = process_and_print_job;
		// we are about to print out the jobads, so print an output header now.
		print_full_header(source_label.c_str());
	} else {
		// tj: 2013 refactor---
		// so we can compare the output of old vs. new code. keep both around for now.
		pfnProcess = /*use_old_code ? process_buffer_line_old : */ process_job_and_render_to_dag_map;
		pvProcess = &dag_map;
	}

	int fetchResult = Q.fetchQueueFromHostAndProcess(scheddAddress, attrs, pfnProcess, pvProcess, useFastPath, &errstack);
	if (fetchResult != Q_OK) {
		// The parse + fetch failed, print out why
		switch(fetchResult) {
		case Q_PARSE_ERROR:
		case Q_INVALID_CATEGORY:
			fprintf(stderr, "\n-- Parse error in constraint expression \"%s\"\n", user_job_constraint);
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
		
		PRAGMA_REMIND("TJ: shouldn't this be using scheddAddress instead of scheddName?")
		Daemon schedd(DT_SCHEDD, scheddName, pool ? pool->addr() : NULL );

		return print_jobs_analysis(jobs, source_label.c_str(), &schedd);
	}

	// at this point we either have a populated jobs list, or a populated dag_map
	// depending on which processing function we used in the query above.
	// for -long -xml or -analyze, we should have jobs, but no dag_map

	// if outputing dag format, we want to build the parent child links in the dag map
	// and then edit the text for jobs that are dag nodes.
	if (dag && (dag_map.size() > 0)) {
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
			formatstr(source_label, "Schedd: %s : %s", scheddName, scheddAddress);
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

		// display the jobs from this submittor
	if( jobs.MyLength() != 0 || !global ) {
		print_full_header(source_label.c_str());

		jobs.Open();
		while(ClassAd *job = jobs.Next()) {
			process_and_print_job(NULL, job);
		}
		jobs.Close();

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
				PRAGMA_REMIND("TJ: don't exit here, just analyze without userprio information")
				exit (1);
			}
		} else {
			// fetch submittor prios
			cPrios = fetchSubmittorPriosFromNegotiator(prioTable);
			if (cPrios < 0) {
				PRAGMA_REMIND("TJ: don't exit here, just analyze without userprio information")
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
		while (const char * line = getline(file)) {
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

#include "classad/classadCache.h" // for CachedExprEnvelope

// AnalyzeThisSubExpr recursively walks an ExprTree and builds a vector of
// this class, one entry for each clause of the ExprTree that deserves to be
// separately evaluated. 
//
class AnalSubExpr {

public:
	// these members are set while parsing the SubExpr
	classad::ExprTree *  tree; // this pointer is NOT owned by this class, and should not be freeed from here!!
	int  depth;	   // nesting depth (parens only)
	int  logic_op; // 0 = non-logic, 1=!, 2=||, 3=&&, 4=?:
	int  ix_left;
	int  ix_right;
	int  ix_grip;
	int  ix_effective;  // when this clause reduces down to exactly the same as another clause.
	std::string label;

	// these are used during iteration of Target ads against each of the sub-expressions
	int  matches;
	bool constant;
	int  hard_value;  // if constant, this is set to the value
	bool dont_care;
	int  pruned_by;   // index of entry that set dont_care
	bool reported;
	std::string unparsed;

	AnalSubExpr(classad::ExprTree * expr, const char * lbl, int dep, int logic=0)
		: tree(expr)
		, depth(dep)
		, logic_op(logic)
		, ix_left(-1)
		, ix_right(-1)
		, ix_grip(-1)
		, ix_effective(-1)
		, label(lbl)
		, matches(0)
		, constant(false)
		, hard_value(-1)
		, dont_care(false)
		, pruned_by(-1)
		, reported(false)
		{
		};

	void CheckIfConstant(ClassAd & ad)
	{
		classad::ClassAdUnParser unparser;
		unparser.Unparse(unparsed, tree);

		StringList my;
		StringList target;
		ad.GetExprReferences(unparsed.c_str(), my, target);
		constant = target.isEmpty();
		if (constant) {
			hard_value = 0;
			bool bool_val = false;
			classad::Value eval_result;
			if (EvalExprTree(tree, &ad, NULL, eval_result)
				&& eval_result.IsBooleanValue(bool_val)
				&& bool_val) {
				hard_value = 1;
			}
		}
	};

	const char * Label()
	{
		if (label.empty()) {
			if (MakeLabel(label)) {
				// nothing to do.
			} else if ( ! unparsed.empty()) {
				return unparsed.c_str();
			} else {
				return "empty";
			}
		}
		return label.c_str();
	}

	bool MakeLabel(std::string & lbl)
	{
		if (logic_op) {
			if (logic_op < 2)
				formatstr(lbl, " ! [%d]", ix_left);
			else if (logic_op > 3)
				formatstr(lbl, "[%d] ? [%d] : [%d]", ix_left, ix_right, ix_grip);
			else
				formatstr(lbl, "[%d] %s [%d]", ix_left, (logic_op==2) ? "||" : "&&", ix_right);
			return true;
		}
		return false;
	}

	// helper function that returns width-adjusted step numbers
	const char * StepLabel(std::string & str, int index, int width=4) {
		formatstr(str, "[%d]      ", index);
		str.erase(width+2, string::npos);
		return str.c_str();
	}
};

// printf formatting helper for indenting
#if 0 // indent using counted spaces
static const char * GetIndentPrefix(int indent) {
  #if 0
	static std::string str = "                                            ";
	while (str.size()/1 < indent) { str += "  "; }
	return &str.c_str()[str.size() - 1*indent];
  #else  // indent using N> 
	static std::string str;
	formatstr(str, "%d>     ", indent);
	str.erase(5, string::npos);
	return str.c_str();
  #endif
}
static const bool analyze_show_work = true;
#else
static const char * GetIndentPrefix(int) { return ""; }
static const bool analyze_show_work = false;
#endif

static std::string s_strStep;
#define StepLbl(ii) subs[ii].StepLabel(s_strStep, ii, 3)

int AnalyzeThisSubExpr(ClassAd *myad, classad::ExprTree* expr, std::vector<AnalSubExpr> & clauses, bool must_store, int depth)
{
	classad::ExprTree::NodeKind kind = expr->GetKind( );
	classad::ClassAdUnParser unparser;

	bool show_work = analyze_show_work;
	bool evaluate_logical = false;
	int  child_depth = depth;
	int  logic_op = 0;
	bool push_it = must_store;
	bool chatty = (analyze_detail_level & detail_diagnostic) != 0;
	const char * pop = "";
	int ix_me = -1, ix_left = -1, ix_right = -1, ix_grip = -1;

	std::string strLabel;

	classad::ExprTree *left=NULL, *right=NULL, *gripping=NULL;
	switch(kind) {
		case classad::ExprTree::LITERAL_NODE: {
			classad::Value val;
			classad::Value::NumberFactor factor;
			((classad::Literal*)expr)->GetComponents(val, factor);
			unparser.UnparseAux(strLabel, val, factor);
			if (chatty) {
				printf("     %d:const : %s\n", kind, strLabel.c_str());
			}
			show_work = false;
			break;
		}

		case classad::ExprTree::ATTRREF_NODE: {
			bool absolute;
			std::string strAttr; // simple attr, need unparse to get TARGET. or MY. prefix.
			((classad::AttributeReference*)expr)->GetComponents(left, strAttr, absolute);
			if (chatty) {
				printf("     %d:attr  : %s %s at %p\n", kind, absolute ? "abs" : "ref", strAttr.c_str(), left);
			}
			if (absolute) {
				left = NULL;
			} else if ( ! left && strAttr == "START") {
				left = myad->LookupExpr(strAttr.c_str());
			}
			show_work = false;
			break;
		}

		case classad::ExprTree::OP_NODE: {
			classad::Operation::OpKind op = classad::Operation::__NO_OP__;
			((classad::Operation*)expr)->GetComponents(op, left, right, gripping);
			pop = "??";
			if (op <= classad::Operation::__LAST_OP__) 
				pop = unparser.opString[op];
			if (chatty) {
				printf("     %d:op    : %2d:%s %p %p %p\n", kind, op, pop, left, right, gripping);
			}
			if (op >= classad::Operation::__COMPARISON_START__ && op <= classad::Operation::__COMPARISON_END__) {
				//show_work = true;
				evaluate_logical = false;
				push_it = true;
			} else if (op >= classad::Operation::__LOGIC_START__ && op <= classad::Operation::__LOGIC_END__) {
				//show_work = true;
				evaluate_logical = true;
				logic_op = 1 + (int)(op - classad::Operation::__LOGIC_START__);
				push_it = true;
			} else if (op == classad::Operation::PARENTHESES_OP){
				push_it = false;
				evaluate_logical = true;
				child_depth += 1;
			} else {
				//show_work = false;
			}
			break;
		}

		case classad::ExprTree::FN_CALL_NODE: {
			std::vector<classad::ExprTree*> args;
			((classad::FunctionCall*)expr)->GetComponents(strLabel, args);
			strLabel += "()";
			if (chatty) {
				printf("     %d:call  : %s() %d args\n", kind, strLabel.c_str(), (int)args.size());
			}
			if (must_store) {
				std::string str;
				unparser.Unparse(str, expr);
				if ( ! str.empty()) strLabel = str;
			}
			break;
		}

		case classad::ExprTree::CLASSAD_NODE: {
			vector< std::pair<string, classad::ExprTree*> > attrsT;
			((classad::ClassAd*)expr)->GetComponents(attrsT);
			if (chatty) {
				printf("     %d:ad    : %d attrs\n", kind, (int)attrsT.size());
			}
			break;
		}

		case classad::ExprTree::EXPR_LIST_NODE: {
			vector<classad::ExprTree*> exprs;
			((classad::ExprList*)expr)->GetComponents(exprs);
			if (chatty) {
				printf("     %d:list  : %d items\n", kind, (int)exprs.size());
			}
			break;
		}
		
		case classad::ExprTree::EXPR_ENVELOPE: {
			// recurse b/c we indirect for this element.
			left = ((classad::CachedExprEnvelope*)expr)->get();
			if (chatty) {
				printf("     %d:env  :     %p \n", kind, left);
			}
			break;
		}
	}

	if (left) ix_left = AnalyzeThisSubExpr(myad, left, clauses, evaluate_logical, child_depth);
	if (right) ix_right = AnalyzeThisSubExpr(myad, right, clauses, evaluate_logical, child_depth);
	if (gripping) ix_grip = AnalyzeThisSubExpr(myad, gripping, clauses, evaluate_logical, child_depth);

	if (push_it) {
		if (left && ! right && ix_left >= 0) {
			ix_me = ix_left;
		} else {
			ix_me = (int)clauses.size();
			AnalSubExpr sub(expr, strLabel.c_str(), depth, logic_op);
			sub.ix_left = ix_left;
			sub.ix_right = ix_right;
			sub.ix_grip = ix_grip;
			clauses.push_back(sub);
		}
	} else if (left && ! right) {
		ix_me = ix_left;
	}

	if (show_work) {

		std::string strExpr;
		unparser.Unparse(strExpr, expr);
		if (push_it) {
			if (left && ! right && ix_left >= 0) {
				printf("(---):");
			} else {
				printf("(%3d):", (int)clauses.size()-1);
			}
		} else { printf("      "); }

		if (evaluate_logical) {
			printf("[%3d] %5s : [%3d] %s [%3d] %s\n", 
				   ix_me, "", ix_left, pop, ix_right,
				   chatty ? strExpr.c_str() : "");
		} else {
			printf("[%3d] %5s : %s\n", ix_me, "", strExpr.c_str());
		}
	}

	return ix_me;
}

// recursively mark subexpressions as irrelevant
//
static void MarkIrrelevant(std::vector<AnalSubExpr> & subs, int index, std::string & irr_path, int at_index)
{
	if (index >= 0) {
		subs[index].dont_care = true;
		subs[index].pruned_by = at_index;
		//printf("(%d:", index);
		formatstr_cat(irr_path,"(%d:", index);
		if (subs[index].ix_left >= 0)  MarkIrrelevant(subs, subs[index].ix_left, irr_path, at_index);
		if (subs[index].ix_right >= 0) MarkIrrelevant(subs, subs[index].ix_right, irr_path, at_index);
		if (subs[index].ix_grip >= 0)  MarkIrrelevant(subs, subs[index].ix_grip, irr_path, at_index);
		formatstr_cat(irr_path,")");
		//printf(")");
	}
}


static void AnalyzePropagateConstants(std::vector<AnalSubExpr> & subs, bool show_work)
{

	// propagate hard true-false
	for (int ix = 0; ix < (int)subs.size(); ++ix) {

		int ix_effective = -1;
		int ix_irrelevant = -1;

		const char * pindent = GetIndentPrefix(subs[ix].depth);

		// fixup logic_op entries, propagate hard true/false

		if (subs[ix].logic_op) {
			// unset, false, true, indeterminate, undefined, error, unset
			static const char * truthy[] = {"?", "F", "T", "", "u", "e"};
			int hard_left = 2, hard_right = 2, hard_grip = 2;
			if (subs[ix].ix_left >= 0) {
				int il = subs[ix].ix_left;
				if (subs[il].constant) {
					hard_left = subs[il].hard_value;
				}
			}
			if (subs[ix].ix_right >= 0) {
				int ir = subs[ix].ix_right;
				if (subs[ir].constant) {
					hard_right = subs[ir].hard_value;
				}
			}
			if (subs[ix].ix_grip >= 0) {
				int ig = subs[ix].ix_grip;
				if (subs[ig].constant) {
					hard_grip = subs[ig].hard_value;
				}
			}

			switch (subs[ix].logic_op) {
				case 1: { // ! 
					formatstr(subs[ix].label, " ! [%d]%s", subs[ix].ix_left, truthy[hard_left+1]);
					break;
				}
				case 2: { // || 
					// true || anything is true
					if (hard_left == 1 || hard_right == 1) {
						subs[ix].constant = true;
						subs[ix].hard_value = true;
						// mark the non-constant irrelevant clause
						if (hard_left == 1) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
							ix_irrelevant = subs[ix].ix_right;
						} else if (hard_right == 1) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
							ix_irrelevant = subs[ix].ix_left;
						}
					} else
					// false || false is false
					if (hard_left == 0 && hard_right == 0) {
						subs[ix].constant = true;
						subs[ix].hard_value = false;
					} else if (hard_left == 0) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
					} else if (hard_right == 0) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
					}
					formatstr(subs[ix].label, "[%d]%s || [%d]%s", 
					          subs[ix].ix_left, truthy[hard_left+1], 
					          subs[ix].ix_right, truthy[hard_right+1]);
					break;
				}
				case 3: { // &&
					// false && anything is false
					if (hard_left == 0 || hard_right == 0) {
						subs[ix].constant = true;
						subs[ix].hard_value = false;
						// mark the non-constant irrelevant clause
						if (hard_left == 0) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
							ix_irrelevant = subs[ix].ix_right;
						} else if (hard_right == 0) {
							subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
							ix_irrelevant = subs[ix].ix_left;
						}
					} else
					// true && true is true
					if (hard_left == 1 && hard_right == 1) {
						subs[ix].constant = true;
						subs[ix].hard_value = true;
					} else if (hard_left == 1) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_right;
					} else if (hard_right == 1) {
						subs[ix].ix_effective = ix_effective = subs[ix].ix_left;
					}
					formatstr(subs[ix].label, "[%d]%s && [%d]%s", 
					          subs[ix].ix_left, truthy[hard_left+1], 
					          subs[ix].ix_right, truthy[hard_right+1]);
					break;
				}
				case 4: { // ?:
					if (hard_left == 0 || hard_left == 1) {
						ix_effective = hard_left ? subs[ix].ix_right : subs[ix].ix_grip; 
						subs[ix].ix_effective = ix_effective;
						if ((ix_effective >= 0) && subs[ix_effective].constant) {
							subs[ix].constant = true;
							subs[ix].hard_value = subs[ix_effective].hard_value;
						}
						// mark the irrelevant clause
						ix_irrelevant = hard_left ? subs[ix].ix_grip : subs[ix].ix_right;
					}
					formatstr(subs[ix].label, "[%d]%s ? [%d]%s : [%d]%s", 
					          subs[ix].ix_left, truthy[hard_left+1], 
					          subs[ix].ix_right, truthy[hard_right+1], 
					          subs[ix].ix_grip, truthy[hard_grip+1]);
					break;
				}
			}
		}

		// now walk effective expressions backward
		std::string effective_path = "";
		if (ix_effective >= 0) {
			if (ix_irrelevant < 0) {
				if (ix_effective == subs[ix].ix_left)
					ix_irrelevant = subs[ix].ix_right;
				if (ix_effective == subs[ix].ix_right)
					ix_irrelevant = subs[ix].ix_left;
			}
			formatstr(effective_path, "%d->%d", ix, ix_effective);
			while (subs[ix_effective].ix_effective >= 0) {
				ix_effective = subs[ix_effective].ix_effective;
				subs[ix].ix_effective = ix_effective;
				formatstr_cat(effective_path, "->%d", ix_effective);
			}
		}

		std::string irr_path = "";
		if (ix_irrelevant >= 0) {
			//printf("\tMarkIrrelevant(%d) by %d = ", ix_irrelevant, ix);
			MarkIrrelevant(subs, ix_irrelevant, irr_path, ix);
			//printf("\n");
		}


		if (show_work) {

			const char * const_val = "";
			if (subs[ix].constant)
				const_val = subs[ix].hard_value ? "always" : "never";
			if (ix_effective >= 0) {
				printf("%s %5s\t%s%s\t is effectively %s e<%s>\n", 
					   StepLbl(ix), const_val, pindent, subs[ix].Label(), subs[ix_effective].Label(), effective_path.c_str());
			} else {
				printf("%s %5s\t%s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label());
			}
			if (ix_irrelevant >= 0) {
				printf("           \tpruning %s\n", irr_path.c_str());
			}
		}
	} // check-if-constant
}

// insert spaces and \n into temp_buffer so that it will print out neatly on a display with the given width
// spaces are added at the start of each newly created line for the given indet, 
// but spaces are NOT added to the start of the input buffer.
//
static const char * PrettyPrintExprTree(classad::ExprTree *tree, std::string & temp_buffer, int indent, int width)
{
	classad::ClassAdUnParser unparser;
	unparser.Unparse(temp_buffer, tree);

	if (indent > width)
		indent = width*2/3;

	std::string::iterator it, lastAnd, lineStart;
	it = lastAnd = lineStart = temp_buffer.begin();
	int indent_at_last_and = indent;
	int pos = indent;  // we presume that the initial indent has already been printed by the caller.
	bool was_and = false;

	char chPrev = 0;
	while (it != temp_buffer.end()) {

		// as we iterate characters, keep track of the indent level
		// and whether the current position is the 2nd character of && or ||
		//
		bool is_and = false;
		if ((*it == '&' || *it == '|') && chPrev == *it) {
			is_and = true;
		}
		else if (*it == '(') { indent += 2; }
		else if (*it == ')') { indent -= 2; }

		// if the current position exceeds the width, we want to replace
		// the character after the last && or || with a \n. We can count on
		// the call putting a space after each && or ||, so replacing is safe.
		// Note: this code may insert charaters into the stream, but will only do
		// so BEFORE the current position, and it gurantees that the iterator
		// will still point to the same character after the insert as it did before,
		// so we don't loose our place.
		//
		if (pos >= width) {
			if (lastAnd != lineStart) {
				temp_buffer.replace(lastAnd, lastAnd + 1, 1, '\n');
				lineStart = lastAnd + 1;
				lastAnd = lineStart;
				pos = 0;

				// if we have to indent, we do that by inserting spaces at the start of line
				// then we have to fixup iterators and position counters to account for
				// the insertion. we do the fixup to avoid problems caused by std:string realloc
				// as well as to avoid scanning any character in the input string more than once,
				// which in turn guarantees that this code will always get to the end.
				if (indent_at_last_and > 0) {
					size_t cch = distance(temp_buffer.begin(), it);
					size_t cchStart = distance(temp_buffer.begin(), lineStart);
					temp_buffer.insert(lineStart, indent_at_last_and, ' ');
					// it, lineStart & lastAnd can be invalid if the insert above reallocated the buffer.
					// so we have to recreat them.
					it = temp_buffer.begin() + cch + indent_at_last_and;
					lastAnd = lineStart = temp_buffer.begin() + cchStart;
					pos = (int) distance(lineStart, it);
				}
				indent_at_last_and = indent;
			}
		}
		// was_and will be true if this is the character AFTER && or ||
		// if it is, this is a potential place to put a \n, so remember it.
		if (was_and) {
			lastAnd = it;
			indent_at_last_and = indent;
		}

		chPrev = *it;
		++it;
		++pos;
		was_and = is_and;
	}
	return temp_buffer.c_str();
}

static void AnalyzeRequirementsForEachTarget(ClassAd *request, ClassAdList & targets, std::string & return_buf, int detail_mask)
{
	int console_width = widescreen ? getDisplayWidth() : 80;
	bool show_work = (detail_mask & detail_diagnostic) != 0;

	bool request_is_machine = false;
	const char * attrConstraint = ATTR_REQUIREMENTS;
	if (0 == strcmp(GetMyTypeName(*request),STARTD_ADTYPE)) {
		//attrConstraint = ATTR_START;
		attrConstraint = ATTR_REQUIREMENTS;
		request_is_machine = true;
	}

	classad::ExprTree* exprReq = request->LookupExpr(attrConstraint);
	if ( ! exprReq)
		return;

	std::vector<AnalSubExpr> subs;
	std::string strStep;
	#undef StepLbl
	#define StepLbl(ii) subs[ii].StepLabel(strStep, ii, 3)

	if (show_work) { printf("\nDump ExprTree in evaluation order\n"); }

	AnalyzeThisSubExpr(request, exprReq, subs, true, 0);

	if (detail_mask & 0x200) {
		printf("\nDump subs\n");
		classad::ClassAdUnParser unparser;
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			printf("%p %2d %2d [%3d %3d %3d] %3d %d %d ", subs[ix].tree, subs[ix].depth, subs[ix].logic_op,
				subs[ix].ix_left, subs[ix].ix_right, subs[ix].ix_grip,
				subs[ix].ix_effective, (int)subs[ix].label.size(), (int)subs[ix].unparsed.size());
			std::string lbl;
			if (subs[ix].MakeLabel(lbl)) {
			} else if (subs[ix].ix_left < 0) {
				unparser.Unparse(lbl, subs[ix].tree);
			} else {
				lbl = "";
			}
			printf("%s\n", lbl.c_str());
		}
	}

	// check of each sub-expression has any target references.
	if (show_work) { printf("\nEvaluate constants:\n"); }
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		subs[ix].CheckIfConstant(*request);
	}

	AnalyzePropagateConstants(subs, show_work);

	//
	// now print out the strength-reduced expression
	//
	if (show_work) {
		printf("\nReduced boolean expressions:\n");
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			const char * const_val = "";
			if (subs[ix].constant)
				const_val = subs[ix].hard_value ? "always" : "never";
			std::string pruned = "";
			if (subs[ix].dont_care) {
				const_val = (subs[ix].constant) ? (subs[ix].hard_value ? "n/aT" : "n/aF") : "n/a";
				formatstr(pruned, "\tpruned-by:%d", subs[ix].pruned_by);
			}
			if (subs[ix].ix_effective < 0) {
				const char * pindent = GetIndentPrefix(subs[ix].depth);
				printf("%s %5s\t%s%s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label(), pruned.c_str());
			}
		}
	}

		// propagate effectives back up the chain. 
		//

	if (show_work) {
		printf("\nPropagate effectives:\n");
	}
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		bool fchanged = false;
		int jj = subs[ix].ix_left;
		if ((jj >= 0) && (subs[jj].ix_effective >= 0)) {
			subs[ix].ix_left = subs[jj].ix_effective;
			fchanged = true;
		}

		jj = subs[ix].ix_right;
		if ((jj >= 0) && (subs[jj].ix_effective >= 0)) {
			subs[ix].ix_right = subs[jj].ix_effective;
			fchanged = true;
		}

		jj = subs[ix].ix_grip;
		if ((jj >= 0) && (subs[jj].ix_effective >= 0)) {
			subs[ix].ix_grip = subs[jj].ix_effective;
			fchanged = true;
		}

		if (fchanged) {
			// force the label to be rebuilt.
			std::string oldlbl = subs[ix].label;
			if (oldlbl.empty()) oldlbl = "";
			subs[ix].label.clear();
			if (show_work) {
				printf("%s   %s  is effectively  %s\n", StepLbl(ix), oldlbl.c_str(), subs[ix].Label());
			}
		}
	}

	if (detail_mask & 0x200) {
		printf("\nDump subs\n");
		classad::ClassAdUnParser unparser;
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			printf("[%3d] %2d %2d [%3d %3d %3d] %3d %d %d ", ix, subs[ix].depth, subs[ix].logic_op,
				subs[ix].ix_left, subs[ix].ix_right, subs[ix].ix_grip,
				subs[ix].ix_effective, (int)subs[ix].label.size(), (int)subs[ix].unparsed.size());
			std::string lbl;
			if (subs[ix].MakeLabel(lbl)) {
			} else if (subs[ix].ix_left < 0) {
				unparser.Unparse(lbl, subs[ix].tree);
			} else {
				lbl = "";
			}
			printf("%s\n", lbl.c_str());
		}
	}

	if (show_work) {
		printf("\nFinal expressions:\n");
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			const char * const_val = "";
			if (subs[ix].constant)
				const_val = subs[ix].hard_value ? "always" : "never";
			if (subs[ix].ix_effective < 0 && ! subs[ix].dont_care) {
				std::string altlbl;
				//if ( ! subs[ix].MakeLabel(altlbl)) altlbl = "";
				const char * pindent = GetIndentPrefix(subs[ix].depth);
				printf("%s %5s\t%s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label() /*, altlbl.c_str()*/);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////////

	//
	//
	// build counts of matching machines, notice which expressions match all or no machines
	//
	if (show_work) {
		if (request_is_machine) {
			printf("Evaluation against job ads:\n");
			printf(" Step  Jobs    Condition\n");
		} else {
			printf("Evaluation against machine ads:\n");
			printf(" Step  Slots   Condition\n");
		}
	}

	std::string linebuf;
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		if (subs[ix].ix_effective >= 0 || subs[ix].dont_care)
			continue;

		if (subs[ix].constant) {
			if (show_work) {
				const char * const_val = subs[ix].hard_value ? "always" : "never";
				formatstr(linebuf, "%s %9s  %s%s\n", StepLbl(ix), const_val, GetIndentPrefix(subs[ix].depth), subs[ix].Label());
				printf("%s", linebuf.c_str()); 
			}
			continue;
		}

		subs[ix].matches = 0;

		// do short-circuit evaluation of && operations
		int matches_all = targets.Length();
		int ix_left = subs[ix].ix_left;
		int ix_right = subs[ix].ix_right;
		if (ix_left >= 0 && ix_right >= 0 && subs[ix].logic_op) {
			int ix_effective = -1;
			int ix_prune = -1;
			const char * reason = "";
			if (subs[ix].logic_op == 3) { // &&
				reason = "&&";
				if (subs[ix_left].matches == 0) {
					ix_effective = ix_left;
				} else if (subs[ix_right].matches == 0) {
					ix_effective = ix_right;
				}
			} else if (subs[ix].logic_op == 2) { // || 
				if (subs[ix_left].matches == matches_all) {
					reason = "a||";
					ix_effective = ix_left;
					ix_prune = ix_right;
				} else if (subs[ix_right].matches == matches_all) {
					reason = "||a";
					ix_effective = ix_right;
					ix_prune = ix_left;
				} else if (subs[ix_left].matches == 0 && subs[ix_right].matches != 0) {
					reason = "0||";
					ix_effective = ix_right;
					ix_prune = ix_left;
				} else if (subs[ix_left].matches != 0 && subs[ix_right].matches == 0) {
					reason = "||0";
					ix_effective = ix_left;
					ix_prune = ix_right;
				}
			}

			if (ix_prune >= 0) {
				std::string irr_path = "";
				MarkIrrelevant(subs, ix_prune, irr_path, ix);
				if (show_work) { printf("pruning peer [%d] %s %s\n", ix_prune, reason, irr_path.c_str()); }
			}

			if (ix_effective >= 0) {
				while(subs[ix_effective].ix_effective >= 0)
					ix_effective = subs[ix_effective].ix_effective;
				subs[ix].ix_effective = ix_effective;
				if (show_work) { printf("pruning [%d] back to [%d] because %s\n", ix, ix_effective, reason); }
			} else {
				if (subs[ix_left].ix_effective >= 0) {
					subs[ix].ix_left = subs[ix_left].ix_effective;
					subs[ix].label.clear();
				}
				if (subs[ix_right].ix_effective >= 0) {
					subs[ix].ix_right = subs[ix_right].ix_effective;
					subs[ix].label.clear();
				}
			}
		}

		targets.Open();
		while (ClassAd *target = targets.Next()) {

			classad::Value eval_result;
			bool bool_val;
			if (EvalExprTree(subs[ix].tree, request, target, eval_result) && 
				eval_result.IsBooleanValue(bool_val) && 
				bool_val) {
				subs[ix].matches += 1;
			}
		}
		targets.Close();

		// did the left or right path of a logic op dominate the result?
		if (ix_left >= 0 && ix_right >= 0 && subs[ix].logic_op) {
			int ix_effective = -1;
			const char * reason = "";
			if (subs[ix].logic_op == 3) { // &&
				reason = "&&";
				if (subs[ix_left].matches == subs[ix].matches) {
					ix_effective = ix_left;
					if (subs[ix_right].matches > subs[ix].matches) {
						subs[ix_right].dont_care = true;
						subs[ix_right].pruned_by = ix_left;
						if (show_work) { printf("pruning peer [%d] because of &&>[%d]\n", ix_right, ix_left); }
					}
				} else if (subs[ix_right].matches == subs[ix].matches) {
					ix_effective = ix_right;
					if (subs[ix_left].matches > subs[ix].matches) {
						subs[ix_left].dont_care = true;
						subs[ix_left].pruned_by = ix_right;
						if (show_work) { printf("pruning peer [%d] because of &&>[%d]\n", ix_left, ix_right); }
					}
				}
			} else if (subs[ix].logic_op == 2) { // || 
			}

			if (ix_effective >= 0) {
				while(subs[ix_effective].ix_effective >= 0)
					ix_effective = subs[ix_effective].ix_effective;
				subs[ix].ix_effective = ix_effective;
				subs[ix].label.clear();
				if (show_work) { printf("pruning [%d] back to [%d] because %s\n", ix, ix_effective, reason); }
			} else {
				if (subs[ix_left].ix_effective >= 0) {
					subs[ix].ix_left = subs[ix_left].ix_effective;
					subs[ix].label.clear();
				}
				if (subs[ix_right].ix_effective >= 0) {
					subs[ix].ix_right = subs[ix_right].ix_effective;
					subs[ix].label.clear();
				}
			}
		}

		if (show_work) { 
			formatstr(linebuf, "%s %9d  %s%s\n", StepLbl(ix), subs[ix].matches, GetIndentPrefix(subs[ix].depth), subs[ix].Label());
			printf("%s", linebuf.c_str()); 
		}
	}

	if (detail_mask & 0x200) {
		printf("\nDump subs\n");
		classad::ClassAdUnParser unparser;
		for (int ix = 0; ix < (int)subs.size(); ++ix) {
			printf("[%3d] %2d %2d [%3d %3d %3d] %3d '%s' ", ix, subs[ix].depth, subs[ix].logic_op,
				subs[ix].ix_left, subs[ix].ix_right, subs[ix].ix_grip,
				subs[ix].ix_effective, subs[ix].label.empty() ? "" : subs[ix].label.c_str());
			std::string lbl;
			if (subs[ix].MakeLabel(lbl)) {
			} else if (subs[ix].ix_left < 0) {
				unparser.Unparse(lbl, subs[ix].tree);
			} else {
				lbl = "";
			}
			printf("%s\n", lbl.c_str());
		}
	}

	//
	// render final output
	//
	return_buf = request_is_machine ? 
	                "       Clusters" 
	              : "         Slots";
	return_buf += "\nStep    Matched  Condition"
	              "\n-----  --------  ---------\n";
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		bool skip_me = (subs[ix].ix_effective >= 0 || subs[ix].dont_care);
		const char * prefix = "";
		if (detail_mask & 0x40) {
			prefix = "  ";
			if (subs[ix].dont_care) prefix = "dk";
			else if (subs[ix].ix_effective >= 0) prefix = "x ";
		} else if (skip_me) {
			continue;
		}

		const char * pindent = GetIndentPrefix(subs[ix].depth);
		const char * const_val = "";
		if (subs[ix].constant) {
			const_val = subs[ix].hard_value ? "always" : "never";
			formatstr(linebuf, "%s %9s  %s%s\n", StepLbl(ix), const_val, pindent, subs[ix].Label());
			return_buf += prefix;
			return_buf += linebuf;
			if (show_work) { printf("%s", linebuf.c_str()); }
			continue;
		}

		formatstr(linebuf, "%s %9d  %s%s", StepLbl(ix), subs[ix].matches, pindent, subs[ix].Label());

		bool append_pretty = false;
		if (subs[ix].logic_op) {
			append_pretty = (detail_mask & detail_smart_unparse_expr) != 0;
			if (subs[ix].ix_left == ix-2 && subs[ix].ix_right == ix-1)
				append_pretty = false;
			if ((detail_mask & detail_always_unparse_expr) != 0)
				append_pretty = true;
		}
			
		if (append_pretty) { 
			std::string treebuf;
			linebuf += " ( "; 
			PrettyPrintExprTree(subs[ix].tree, treebuf, linebuf.size(), console_width); 
			linebuf += treebuf; 
			linebuf += " )";
		}

		linebuf += "\n";
		return_buf += prefix;
		return_buf += linebuf;
	}

	// chase zeros back up the expression tree
#if 0
	show_work = (detail_mask & 8); // temporary
	if (show_work) {
		printf("\nCurrent Table:\nStep  -> Effec Depth Leaf D/C Matches\n");
		for (int ix = 0; ix < (int)subs.size(); ++ix) {

			int leaf = subs[ix].logic_op == 0;
			printf("[%3d] -> [%3d] %5d %4d %3d %7d %s\n", ix, subs[ix].ix_effective, subs[ix].depth, leaf, subs[ix].dont_care, subs[ix].matches, subs[ix].Label());
		}
		printf("\n");
		printf("\nChase Zeros:\nStep  -> Effec Depth Leaf Matches\n");
	}
	for (int ix = 0; ix < (int)subs.size(); ++ix) {
		if (subs[ix].ix_effective >= 0 || subs[ix].dont_care)
			continue;

		int leaf = subs[ix].logic_op == 0;
		if (leaf && subs[ix].matches == 0) {
			if (show_work) {
				printf("[%3d] -> [%3d] %5d %4d %7d %s\n", ix, subs[ix].ix_effective, subs[ix].depth, leaf, subs[ix].matches, subs[ix].Label());
			}
			for (int jj = ix+1; jj < (int)subs.size(); ++jj) {
				if (subs[jj].matches != 0)
					continue;
				if (subs[jj].reported)
					continue;
				if ((subs[jj].ix_effective == ix) || subs[jj].ix_left == ix || subs[jj].ix_right == ix || subs[jj].ix_grip == ix) {
					const char * pszop = "  \0 !\0||\0&&\0?:\0  \0"+subs[jj].logic_op*3;
					printf("[%3d] -> [%3d] %5d %4s %7d %s\n", jj, subs[jj].ix_effective, subs[jj].depth, pszop, subs[jj].matches, subs[jj].Label());
					subs[jj].reported = true;
				}
			}
		}
	}
	if (show_work) {
		printf("\n");
	}
#endif

}

#if 0 // experimental code

int EvalThisSubExpr(int & index, classad::ExprTree* expr, ClassAd *request, ClassAd *offer, std::vector<ExprTree*> & clauses, bool must_store)
{
	++index;

	classad::ExprTree::NodeKind kind = expr->GetKind( );
	classad::ClassAdUnParser unp;

	bool evaluate = true;
	bool evaluate_logical = false;
	bool push_it = must_store;
	bool chatty = false;
	const char * pop = "??";
	int ix_me = -1, ix_left = -1, ix_right = -1, ix_grip = -1;

	classad::Operation::OpKind op = classad::Operation::__NO_OP__;
	classad::ExprTree *left=NULL, *right=NULL, *gripping=NULL;
	switch(kind) {
		case classad::ExprTree::LITERAL_NODE: {
			if (chatty) {
				classad::Value val;
				classad::Value::NumberFactor factor;
				((classad::Literal*)expr)->GetComponents(val, factor);
				std::string str;
				unp.UnparseAux(str, val, factor);
				printf("     %d:const : %s\n", kind, str.c_str());
			}
			evaluate = false;
			break;
		}

		case classad::ExprTree::ATTRREF_NODE: {
			bool	absolute;
			std::string attrref;
			((classad::AttributeReference*)expr)->GetComponents(left, attrref, absolute);
			if (chatty) {
				printf("     %d:attr  : %s %s at %p\n", kind, absolute ? "abs" : "ref", attrref.c_str(), left);
			}
			if (absolute) {
				left = NULL;
			}
			evaluate = false;
			break;
		}

		case classad::ExprTree::OP_NODE: {
			((classad::Operation*)expr)->GetComponents(op, left, right, gripping);
			pop = "??";
			if (op <= classad::Operation::__LAST_OP__) 
				pop = unp.opString[op];
			if (chatty) {
				printf("     %d:op    : %2d:%s %p %p %p\n", kind, op, pop, left, right, gripping);
			}
			if (op >= classad::Operation::__COMPARISON_START__ && op <= classad::Operation::__COMPARISON_END__) {
				evaluate = true;
				evaluate_logical = false;
				push_it = true;
			} else if (op >= classad::Operation::__LOGIC_START__ && op <= classad::Operation::__LOGIC_END__) {
				evaluate = true;
				evaluate_logical = true;
				push_it = true;
			} else {
				evaluate = false;
			}
			break;
		}

		case classad::ExprTree::FN_CALL_NODE: {
			std::string strName;
			std::vector<ExprTree*> args;
			((classad::FunctionCall*)expr)->GetComponents(strName, args);
			if (chatty) {
				printf("     %d:call  : %s() %d args\n", kind, strName.c_str(), args.size());
			}
			break;
		}

		case classad::ExprTree::CLASSAD_NODE: {
			vector< std::pair<string, classad::ExprTree*> > attrs;
			((classad::ClassAd*)expr)->GetComponents(attrs);
			if (chatty) {
				printf("     %d:ad    : %d attrs\n", kind, attrs.size());
			}
			//clauses.push_back(expr);
			break;
		}

		case classad::ExprTree::EXPR_LIST_NODE: {
			vector<classad::ExprTree*> exprs;
			((classad::ExprList*)expr)->GetComponents(exprs);
			if (chatty) {
				printf("     %d:list  : %d items\n", kind, exprs.size());
			}
			//clauses.push_back(expr);
			break;
		}
		
		case classad::ExprTree::EXPR_ENVELOPE: {
			// recurse b/c we indirect for this element.
			left = ((classad::CachedExprEnvelope*)expr)->get();
			if (chatty) {
				printf("     %d:env  :     %p\n", kind, left);
			}
			break;
		}
	}

	if (left) ix_left = EvalThisSubExpr(index, left, request, offer, clauses, evaluate_logical);
	if (right) ix_right = EvalThisSubExpr(index, right, request, offer, clauses, evaluate_logical);
	if (gripping) ix_grip = EvalThisSubExpr(index, gripping, request, offer, clauses, evaluate_logical);

	if (push_it) {
		if (left && ! right && ix_left >= 0) {
			ix_me = ix_left;
		} else {
			ix_me = (int)clauses.size();
			clauses.push_back(expr);
			// TJ: for now, so I can see what's happening.
			evaluate = true;
		}
	} else if (left && ! right) {
		ix_me = ix_left;
	}

	if (evaluate) {

		classad::Value eval_result;
		bool           bool_val;
		bool matches = false;
		if (EvalExprTree(expr, request, offer, eval_result) && eval_result.IsBooleanValue(bool_val) && bool_val) {
			matches = true;
		}

		std::string strExpr;
		unp.Unparse(strExpr, expr);
		if (evaluate_logical) {
			//if (ix_left < 0)  { ix_left = (int)clauses.size(); clauses.push_back(left); }
			//if (ix_right < 0) { ix_right = (int)clauses.size(); clauses.push_back(right); }
			printf("[%3d] %5s : [%3d] %s [%3d]\n", ix_me, matches ? "1" : "0", 
				   ix_left, pop, ix_right);
			if (chatty) {
				printf("\t%s\n", strExpr.c_str());
			}
		} else {
			printf("[%3d] %5s : %s\n", ix_me, matches ? "1" : "0", strExpr.c_str());
		}
	}

	return ix_me;
}

static void EvalRequirementsExpr(ClassAd *request, ClassAdList & offers, std::string & return_buf)
{
	classad::ExprTree* exprReq = request->LookupExpr(ATTR_REQUIREMENTS);
	if (exprReq) {

		std::vector<ExprTree*> clauses;

		offers.Open();
		while (ClassAd *offer = offers.Next()) {
			int counter = 0;
			EvalThisSubExpr(counter, exprReq, request, offer, clauses, true);

			static bool there_can_be_only_one = true;
			if (there_can_be_only_one)
				break; // for now only do the first offer.
		}
		offers.Close();
	}
}

#endif // experimental code above


static void AddReferencedAttribsToBuffer(
	ClassAd * request,
	StringList & trefs, // out, returns target refs
	const char * pindent,
	std::string & return_buf)
{
	StringList refs;
	trefs.clearAll();

	request->GetExprReferences(ATTR_REQUIREMENTS,refs,trefs);
	if (refs.isEmpty() && trefs.isEmpty())
		return;

	refs.rewind();

	if ( ! pindent) pindent = "";

	AttrListPrintMask pm;
	pm.SetAutoSep(NULL, "", "\n", "\n");
	while(const char *attr = refs.next()) {
		std::string label;
		formatstr(label, "%s%s = %%V", pindent, attr);
		if (0 == strcasecmp(attr, ATTR_REQUIREMENTS) || 0 == strcasecmp(attr, ATTR_START))
			continue;
		pm.registerFormat(label.c_str(), 0, FormatOptionNoTruncate, attr);
	}
	if ( ! pm.IsEmpty()) {
		char * temp = pm.display(request);
		if (temp) {
			return_buf += temp;
			delete[] temp;
		}
	}
}

static void AddTargetReferencedAttribsToBuffer(
	StringList & trefs, // out, returns target refs
	ClassAd * request,
	ClassAd * target,
	const char * pindent,
	std::string & return_buf)
{
	trefs.rewind();

	AttrListPrintMask pm;
	pm.SetAutoSep(NULL, "", "\n", "\n");
	while(const char *attr = trefs.next()) {
		std::string label;
		formatstr(label, "%sTARGET.%s = %%V", pindent, attr);
		if (target->LookupExpr(attr)) {
			pm.registerFormat(label.c_str(), 0, FormatOptionNoTruncate, attr);
		}
	}
	if (pm.IsEmpty())
		return;

	char * temp = pm.display(request, target);
	if (temp) {
		//return_buf += "\n";
		//return_buf += pindent;
		std::string name;
		if ( ! target->LookupString(ATTR_NAME, name)) {
			int cluster=0, proc=0;
			if (target->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
				target->LookupInteger(ATTR_PROC_ID, proc);
				formatstr(name, "Job %d.%d", cluster, proc);
			} else {
				name = "Target";
			}
		}
		return_buf += name;
		return_buf += " has the following attributes:\n\n";
		return_buf += temp;
		delete[] temp;
	}
}


static void
doJobRunAnalysis(ClassAd *request, Daemon *schedd, int details)
{
	PRAGMA_REMIND("TJ: move this code out of this function so it doesn't output once per job")
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
	string  user;
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
				PRAGMA_REMIND("TJ: move this out of the machine iteration loop?")
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
			AddReferencedAttribsToBuffer(request, trefs, "    ", attrib_values);
			strcat(return_buff, attrib_values.c_str());
			attrib_values = "";

			if (single_machine || startdAds.Length() == 1) { 
				startdAds.Open(); 
				while (ClassAd * ptarget = startdAds.Next()) {
					attrib_values = "\n";
					AddTargetReferencedAttribsToBuffer(trefs, request, ptarget, "    ", attrib_values);
					strcat(return_buff, attrib_values.c_str());
				}
				startdAds.Close();
			}
			strcat(return_buff, "\n");
		}

		// TJ's experimental analysis (now with more anal)
		if (analEachReqClause || requirements_is_false) {
			std::string subexpr_detail;
			AnalyzeRequirementsForEachTarget(request, startdAds, subexpr_detail, details);
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
doSlotRunAnalysis(ClassAd *slot, JobClusterMap & clusters, Daemon *schedd, int console_width)
{
	printf("%s", doSlotRunAnalysisToBuffer(slot, clusters, schedd, console_width));
}

static const char *
doSlotRunAnalysisToBuffer(ClassAd *slot, JobClusterMap & clusters, Daemon * /*schedd*/, int console_width)
{
	bool analStartExpr = /*(better_analyze == 2) ||*/ (analyze_detail_level > 0);
	bool showSlotAttrs = ! (analyze_detail_level & detail_dont_show_job_attrs);

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
				AddReferencedAttribsToBuffer(slot, trefs, "    ", attrib_values);
				strcat(return_buff, attrib_values.c_str());
				attrib_values = "";

				if (jobs.Length() == 1) {
					jobs.Open();
					while(ClassAd *job = jobs.Next()) {
						attrib_values = "\n";
						AddTargetReferencedAttribsToBuffer(trefs, slot, job, "    ", attrib_values);
						strcat(return_buff, attrib_values.c_str());
					}
					jobs.Close();
				}
				//strcat(return_buff, "\n");
				strcat(return_buff, "\nThe Requirements expression for this slot reduces to these conditions:\n\n");
			}
		}

		std::string subexpr_detail;
		AnalyzeRequirementsForEachTarget(slot, (ClassAdList&)jobs, subexpr_detail, analyze_detail_level);
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
static QueryResult getQuillAddrFromCollector(char *quill_name, char *&quill_addr) {
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
		ad->LookupString(ATTR_MY_ADDRESS, &quill_addr);
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

