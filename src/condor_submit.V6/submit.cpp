/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "spooled_job_files.h"
#include "subsystem_info.h"
#include "env.h"
#include "basename.h"
#include "condor_getcwd.h"
#include <time.h>
#include "write_user_log.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_io.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif
#include "store_cred.h"
#include "internet.h"
#include "my_hostname.h"
#include "domain_tools.h"
#include "get_daemon_name.h"
#include "sig_install.h"
#include "access.h"
#include "daemon.h"
#include "match_prefix.h"

#include "HashTable.h"
#include "string_list.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "my_username.h"
#include "globus_utils.h"
#include "enum_utils.h"
#include "setenv.h"
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "condor_crontab.h"
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"
#include "NegotiationUtils.h"
#include <submit_utils.h>
#include <param_info.h> // for BinaryLookup

//uncomment this to have condor_submit use the new for 8.5 submit_utils classes
#define USE_SUBMIT_UTILS 1
#include "condor_qmgr.h"
#include "submit_internal.h"

#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "my_popen.h"
#include "zkm_base64.h"

#include <algorithm>
#include <string>
#include <set>

#ifndef USE_SUBMIT_UTILS
#error "condor submit must use submit utils"
#endif

std::set<std::string> CheckFilesRead;
std::set<std::string> CheckFilesWrite;

#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
#else
StringList NoClusterCheckAttrs;
#endif

time_t submit_time = 0;

// Explicit template instantiation


char	*ScheddName = NULL;
std::string ScheddAddr;
AbstractScheddQ * MyQ = NULL;
char	*PoolName = NULL;
DCSchedd* MySchedd = NULL;
MapFile *protectedUrlMap = nullptr;

char	*My_fs_domain;

int		ExtraLineNo;
int		GotQueueCommand = 0;
int		GotNonEmptyQueueCommand = 0;


const char	*MyName;
int 	dash_interactive = 0; /* true if job submitted with -interactive flag */
int 	InteractiveSubmitFile = 0; /* true if using INTERACTIVE_SUBMIT_FILE */
bool	verbose = false; // formerly: int Quiet = 1;
bool	terse = false; // generate parsable output
bool	debug = false;
SetAttributeFlags_t setattrflags = 0; // flags to SetAttribute()
bool	CmdFileIsStdin = false;
bool	NoCmdFileNeeded = false; // set if there is no need for a commmand file (i.e. -queue was specified on command line and at least 1 key=value pair)
bool	GotCmdlineKeys = false; // key=value or -append specifed on the command line
int		WarnOnUnusedMacros = 1;
int		DisableFileChecks = 0;
int     DashQueryCapabilities = 0; // get capabilites from schedd and print the
int		DashDryRun = 0;
int		DashMaxJobs = 0;	 // maximum number of jobs to create before generating an error
int		DashMaxClusters = 0; // maximum number of clusters to create before generating an error.
const char * DashDryRunOutName = NULL;
int     DashDryRunFullAds = 0;
int		DumpSubmitHash = 0;
int		DumpSubmitDigest = 0;
int		DumpJOBSETClassad = 0;
int		MaxProcsPerCluster;
int	  ClusterId = -1;
int	  ProcId = -1;
int		ClustersCreated = 0;
int		JobsCreated = 0;
int		ActiveQueueConnection = FALSE;
bool	NewExecutable = false;
int		dash_remote=0;
int		dash_factory=0;
int		default_to_factory=0;
const char *create_local_factory_file=NULL; // create a copy of the submit digest in the current directory
bool	sim_current_condor_version=false; // don't bother to locate the schedd before creating a SimSchedd
int		sim_starting_cluster=0;			// initial clusterId value for a SimSchedd
#if defined(WIN32)
char* RunAsOwnerCredD = NULL;
#endif
char * batch_name_line = NULL;
char * batch_id_line = NULL;
bool sent_credential_to_credd = false;
bool allow_crlf_script = false;

// For mpi universe testing
bool use_condor_mpi_universe = false;

std::string LastExecutable; // used to pass executable between the check_file callback and SendExecutableb
bool     SpoolLastExecutable;

time_t get_submit_time()
{
	if ( ! submit_time) {
		submit_time = time(0);
	}
	return submit_time;
}

#define time(a) poison_time(a)

//
// The default polling interval for the schedd
//
extern const int SCHEDD_INTERVAL_DEFAULT;
//
// The default job deferral prep time
//
extern const int JOB_DEFERRAL_PREP_DEFAULT;

char* LogNotesVal = NULL;
char* UserNotesVal = NULL;
char* StackSizeVal = NULL;
std::string queueCommandLine; // queue statement passed in via -q argument

SubmitHash submit_hash;

// these are used to keep track of the source of various macros in the table.
//const MACRO_SOURCE DefaultMacro = { true, false, 1, -2, -1, -2 }; // for macros set by default
//const MACRO_SOURCE ArgumentMacro = { true, false, 2, -2, -1, -2 }; // for macros set by command line
//const MACRO_SOURCE LiveMacro = { true, false, 3, -2, -1, -2 };    // for macros use as queue loop variables
MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };

//
// Dump ClassAd to file junk
//
const char * DumpFileName = NULL;
bool		DumpClassAdToFile = false;
FILE		*DumpFile = NULL;
bool		DumpFileIsStdout = 0;

void usage(FILE * out);
// these are in submit_help.cpp
void help_info(FILE* out, int num_topics, const char ** topics);
void schedd_capabilities_help(FILE * out, const ClassAd &ad, const std::string &helpex, DCSchedd* schedd, int options);

void init_params();
void reschedule();
int submit_jobs (
	FILE * fp,
	MACRO_SOURCE & source,
	int as_factory,                  // 0=not factory, 1=must be factory, 2=smart factory (max_materialize), 3=smart factory (all-but-single-proc)
	std::vector<std::string> & append_lines, // lines passed in via -a argument
	std::string & queue_cmd_line);   // queue statement passed in via -q argument
void check_umask();
void setupAuthentication();
const char * is_queue_statement(const char * line); // return ptr to queue args of this is a queue statement
int allocate_a_cluster();
void init_vars(SubmitHash & hash, int cluster_id, StringList & vars);
int set_vars(SubmitHash & hash, StringList & vars, char * item, int item_index, int options, const char * delims, const char * ws);
void cleanup_vars(SubmitHash & hash, StringList & vars);
bool IsNoClusterAttr(const char * name);
int  check_sub_file(void*pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags);
bool is_crlf_shebang(const char * path);
int  SendLastExecutable();
static int MySendJobAttributes(const JOB_ID_KEY & key, const classad::ClassAd & ad, SetAttributeFlags_t saflags);
int  DoUnitTests(int options);

char *username = NULL;

extern DLL_IMPORT_MAGIC char **environ;

extern "C" {
int DoCleanup(int,int,const char*);
}

struct SubmitErrContext ErrContext = { PHASE_INIT, -1, -1, NULL, NULL };

// this will get called when the condor EXCEPT() macro is invoked.
// for the most part, we want to report the message, but to use submit file and
// line numbers rather and the source file and line numbers that are passed in.
void ReportSubmitException(const char * msg, int /*src_line*/, const char * /*src_file*/)
{
	char loc[100];
	switch (ErrContext.phase) {
	case PHASE_READ_SUBMIT: snprintf(loc, sizeof(loc), " on Line %d of submit file", FileMacroSource.line); break;
	case PHASE_DASH_APPEND: snprintf(loc, sizeof(loc), " with -a argument #%d", ExtraLineNo); break;
	case PHASE_QUEUE:       snprintf(loc, sizeof(loc), " at Queue statement on Line %d", FileMacroSource.line); break;
	case PHASE_QUEUE_ARG:   snprintf(loc, sizeof(loc), " with -queue argument"); break;
	default: loc[0] = 0; break;
	}

	fprintf( stderr, "ERROR%s: %s\n", loc, msg );
	if ((ErrContext.phase == PHASE_QUEUE || ErrContext.phase == PHASE_QUEUE_ARG) && ErrContext.step >= 0) {
		if (ErrContext.raw_macro_val && ErrContext.raw_macro_val[0]) {
			fprintf(stderr, "\t at Step %d, ItemIndex %d while expanding %s=%s\n",
				ErrContext.step, ErrContext.item_index, ErrContext.macro_name, ErrContext.raw_macro_val);
		} else {
			fprintf(stderr, "\t at Step %d, ItemIndex %d\n", ErrContext.step, ErrContext.item_index);
		}
	}
}


struct SubmitRec {
	int cluster;
	int firstjob;
	int lastjob;
};

std::vector <SubmitRec> SubmitInfo;

std::vector <ClassAd*> JobAdsArray;
size_t JobAdsArrayLastClusterIndex = 0;

// called by the factory submit to fill out the data structures that
// we use to print out the standard messages on complection.
void set_factory_submit_info(int cluster, int num_procs)
{
	SubmitInfo.push_back(SubmitRec());
	SubmitInfo.back().cluster = cluster;
	SubmitInfo.back().firstjob = 0;
	SubmitInfo.back().lastjob = num_procs-1;
}

void TestFilePermissions( const char *scheddAddr = NULL )
{
#ifdef WIN32
	// this isn't going to happen on Windows since:
	// 1. this uid/gid stuff isn't portable to windows
	// 2. The shadow runs as the user now anyways, so there's no
	//    need for condor to waste time finding out if SYSTEM can
	//    write to some path. The one exception is if the user
	//    submits from AFS, but we're documenting that as unsupported.
	
#else
	gid_t gid = getgid();
	uid_t uid = getuid();

	int result;
	std::set<std::string>::iterator name;

	for ( name = CheckFilesRead.begin(); name != CheckFilesRead.end(); name++ )
	{
		result = attempt_access(name->c_str(), ACCESS_READ, uid, gid, scheddAddr);
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not readable by condor.\n", 
					name->c_str());
		}
	}

	for ( name = CheckFilesWrite.begin(); name != CheckFilesWrite.end(); name++ )
	{
		result = attempt_access(name->c_str(), ACCESS_WRITE, uid, gid, scheddAddr );
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not writable by condor.\n",
					name->c_str());
		}
	}
#endif
}

static bool fnStoreWarning(void * pv, int code, const char * /*subsys*/, const char * message) {
	std::vector<std::string> *warns = (std::vector<std::string> *)pv;
	if (message && ! code) {
		warns->insert(warns->begin(), message);
	}
	return true;
}

void print_submit_parse_warnings(FILE* out, CondorError *errstack)
{
	if (errstack && ! errstack->empty()) {
		// put the warnings into a vector so that we can print them int the order they were added (sigh)
		std::vector<std::string> warns;
		errstack->walk(fnStoreWarning, &warns);
		for (const auto &msg: warns) {
			fprintf(out, "WARNING: %s", msg.c_str());
		}
		errstack->clear();
	}
}

void print_errstack(FILE* out, CondorError *errstack)
{
	if ( ! errstack)
		return;

	for (/*nothing*/ ; ! errstack->empty(); errstack->pop()) {
		int code = errstack->code();
		std::string msg(errstack->message());
		if (msg.size() && msg[msg.size()-1] != '\n') { msg += '\n'; }
		if (code) {
			fprintf(out, "ERROR: %s", msg.c_str());
		} else {
			fprintf(out, "WARNING: %s", msg.c_str());
		}
	}
}

#if 0 // moved to submit_utils
struct SubmitStepFromQArgs {

	SubmitStepFromQArgs(SubmitHash & h)
		: m_hash(h)
		, m_jidInit(0,0)
		, m_nextProcId(0)
		, m_step_size(0)
		, m_done(false)
	{} // this needs to be cheap because Submit.queue() will always invoke it, even if there is no foreach data

	~SubmitStepFromQArgs() {
		// disconnnect the hashtable from our livevars pointers
		unset_live_vars();
	}

	bool has_items() { return m_fea.items.number() > 0; }
	bool done() { return m_done; }

	// returns < 0 on error
	// returns 0 if done iterating
	// returns 2 for first iteration
	// returns 1 for subsequent iterations
	int next(JOB_ID_KEY & jid, int & item_index, int & step)
	{
		if (m_done) return 0;

		int iter_index = (m_nextProcId - m_jidInit.proc);

		jid.cluster = m_jidInit.cluster;
		jid.proc = m_nextProcId;
		item_index = iter_index / m_step_size;
		step = iter_index % m_step_size;

		if (0 == step) { // have we started a new row?
			if (next_rowdata()) {
				set_live_vars();
			} else {
				// if no next row, then we are done iterating, unless it is the FIRST iteration
				// in which case we want to pretend there is a single empty item called "Item"
				if (0 == iter_index) {
					m_hash.set_live_submit_variable("Item", "", false);
				} else {
					m_done = true;
					return 0;
				}
			}
		}

		++m_nextProcId;
		return (0 == iter_index) ? 2 : 1;
	}

	StringList & vars() { return m_fea.vars; }

	// 
	void set_live_vars()
	{
		for (const char * key = vars().first(); key != NULL; key = vars().next()) {
			auto str = m_livevars.find(key);
			if (str != m_livevars.end()) {
				m_hash.set_live_submit_variable(key, str->second.c_str(), false);
			} else {
				m_hash.unset_live_submit_variable(key);
			}
		}
	}

	void unset_live_vars()
	{
		// set the pointers of the 'live' variables to the unset string (i.e. "")
		for (const char * key = vars().first(); key != NULL; key = vars().next()) {
			m_hash.unset_live_submit_variable(key);
		}
	}

	// load the next rowdata into livevars
	// but not into the SubmitHash
	int next_rowdata()
	{
		auto_free_ptr data(m_fea.items.pop());
		if ( ! data) {
			return 0;
		}

		// split the data in the reqired number of fields
		// then store that field data into the m_livevars set
		// NOTE: we don't use the SubmitForeachArgs::split_item method that takes a NOCASE_STRING_MAP
		// because it clears the map first, and that is only safe to do after we unset_live_vars()
		std::vector<const char*> splits;
		m_fea.split_item(data.ptr(), splits);
		int ix = 0;
		for (const char * key = vars().first(); key != NULL; key = vars().next()) {
			m_livevars[key] = splits[ix++];
		}
		return 1;
	}

	// return all of the live value data as a single 'line' using the given item separator and line terminator
	int get_rowdata(std::string & line, const char * sep, const char * eol)
	{
		// so that the separator and line terminators can be \0, we make the size strlen()
		// unless the first character is \0, then the size is 1
		int cchSep = sep ? (sep[0] ? strlen(sep) : 1) : 0;
		int cchEol = eol ? (eol[0] ? strlen(eol) : 1) : 0;
		line.clear();
		for (const char * key = vars().first(); key != NULL; key = vars().next()) {
			if ( ! line.empty() && sep) line.append(sep, cchSep);
			auto str = m_livevars.find(key);
			if (str != m_livevars.end() && ! str->second.empty()) {
				line += str->second;
			}
		}
		if (eol && ! line.empty()) line.append(eol, cchEol);
		return (int)line.size();
	}

	// this is called repeatedly when we are sending rowdata to the schedd
	static int send_row(void* pv, std::string & rowdata) {
		SubmitStepFromQArgs *sii = (SubmitStepFromQArgs*)pv;

		rowdata.clear();
		if (sii->done())
			return 0;

		// Split and write into the string using US (0x1f) a field separator and LF as record terminator
		if ( ! sii->get_rowdata(rowdata, "\x1F", "\n"))
			return 0;

		int rval = sii->next_rowdata();
		if (rval < 0) { return rval; }
		if (rval == 0) { sii->m_done = true; } // so subsequent iterations will return 0
		return 1;
	}


	SubmitHash & m_hash;         // the (externally owned) submit hash we are updating as we iterate
	JOB_ID_KEY m_jidInit;
	SubmitForeachArgs m_fea;
	NOCASE_STRING_MAP m_livevars; // holds live data for active vars
	int  m_nextProcId;
	int  m_step_size;
	bool m_done;
};
#endif

int
main( int argc, const char *argv[] )
{
	FILE	*fp;
	const char **ptr;
	const char *pcolon = NULL;
	const char *cmd_file = NULL;
	std::string method;
	std::vector<std::string> extraLines;  // lines passed in via -a argument

	setbuf( stdout, NULL );

	set_mySubSystem( "SUBMIT", false, SUBSYSTEM_TYPE_SUBMIT );

	MyName = condor_basename(argv[0]);
	set_priv_initialize(); // allow uid switching if root
	config();
	classad::ClassAdSetExpressionCaching(false);

	// We pass this in to submit_utils, but it isn't used to set the Owner attribute for remote submits
	// it's only used to set the default accounting user
	username = my_username();
	if( !username ) {
		username = strdup("unknown");
	}

	init_params();
	submit_hash.init(JSM_CONDOR_SUBMIT);

	default_to_factory = param_boolean("SUBMIT_FACTORY_JOBS_BY_DEFAULT", default_to_factory);

		// If our effective and real gids are different (b/c the
		// submit binary is setgid) set umask(002) so that stdout,
		// stderr and the user log files are created writable by group
		// condor.
	check_umask();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, (SIG_HANDLER)SIG_IGN );
#endif

	bool query_credential = true;

	for( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if (MATCH == strcmp(ptr[0], "-")) { // treat a bare - as a submit filename, it means read from stdin.
				cmd_file = ptr[0];
			} else if (is_dash_arg_prefix(ptr[0], "verbose", 1)) {
				verbose = true; terse = false;
			} else if (is_dash_arg_prefix(ptr[0], "terse", 3)) {
				terse = true; verbose = false;
			} else if (is_dash_arg_prefix(ptr[0], "disable", 1)) {
				DisableFileChecks = 1;
			} else if (is_dash_arg_colon_prefix(ptr[0], "debug", &pcolon, 2)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
				debug = true;
			} else if (is_dash_arg_colon_prefix(ptr[0], "dry-run", &pcolon, 3)) {
				DashDryRun = 1;
				bool needs_file_arg = true;
				if (pcolon) { 
					StringList opts(++pcolon);
					for (const char * opt = opts.first(); opt; opt = opts.next()) {
						if (YourString(opt) == "hash") {
							DumpSubmitHash |= 0x100 | HASHITER_NO_DEFAULTS;
						} else if (YourString(opt) == "def") {
							DumpSubmitHash &= ~HASHITER_NO_DEFAULTS;
						} else if (YourString(opt) == "full") {
							DashDryRunFullAds = 1;
						} else if (YourString(opt) == "digest") {
							DumpSubmitDigest = 1;
						} else if (YourString(opt) == "jobset") {
							DumpJOBSETClassad = 1;
						} else if (YourString(opt) == "tpl" || starts_with(opt, "template")) {
							DumpSubmitHash |= 0x80;
						} else if (starts_with(opt, "cluster=")) {
							sim_current_condor_version = true;
							sim_starting_cluster = atoi(strchr(opt, '=') + 1);
							sim_starting_cluster = MAX(sim_starting_cluster - 1, 0);
						} else if (starts_with(opt, "oauth=")) {
							// log oauth request, 4 = succeed, 2 = fail
							DashDryRun = atoi(strchr(opt,'=')+1) ? 4 : 2;
						} else {
							int optval = atoi(opt);
							// if the argument is -dry:<number> and number is > 0x10,
							// then what we are actually doing triggering the unit tests.
							if (optval > 1) {  DashDryRun = optval; needs_file_arg = optval < 0x10; }
							else {
								fprintf(stderr, "unknown option %s for -dry-run:<opts>\n", opt);
								exit(1);
							}
						}
					}
				}
				if (needs_file_arg) {
					if (DumpClassAdToFile) {
						fprintf(stderr, "%s: -dry-run <file> and -dump <file> cannot be used together\n", MyName);
						exit(1);
					}
					const char * pfilearg = ptr[1];
					if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
						fprintf( stderr, "%s: -dry-run requires another argument\n", MyName );
						exit(1);
					}
					DashDryRunOutName = pfilearg;
					--argc; ++ptr;
				}

			} else if (is_dash_arg_prefix(ptr[0], "spool", 1)) {
				dash_remote++;
				DisableFileChecks = 1;
			} else if (is_dash_arg_prefix(ptr[0], "file", 2)) {
				if (!(--argc) || !(*(++ptr))) {
					fprintf(stderr, "%s: -file requires another argument\n", MyName);
					exit(1);
				}
				cmd_file = *ptr;
			} else if (is_dash_arg_prefix(ptr[0], "digest", 6)) {
				if (!(--argc) || !(*(++ptr)) || ((*ptr)[0] == '-')) {
					fprintf(stderr, "%s: -digest requires a filename argument that is not -\n", MyName);
					exit(1);
				}
				create_local_factory_file = *ptr;
			} else if (is_dash_arg_prefix(ptr[0], "factory", 2)) {
				dash_factory++;
				DisableFileChecks = 1;
			} else if (is_dash_arg_prefix(ptr[0], "generator", 1)) {
				dash_factory++;
				DisableFileChecks = 1;
			} else if (is_dash_arg_prefix(ptr[0], "address", 2)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf(stderr, "%s: -address requires another argument\n", MyName);
					exit(1);
				}
                if (!is_valid_sinful(*ptr)) {
                    fprintf(stderr, "%s: \"%s\" is not a valid address\n", MyName, *ptr);
                    fprintf(stderr, "Should be of the form <ip:port>\n");
                    fprintf(stderr, "For example: <123.456.789.123:6789>\n");
                    exit(1);
                }
                ScheddAddr = *ptr;
			} else if (is_dash_arg_prefix(ptr[0], "remote", 1)) {
				dash_remote++;
				DisableFileChecks = 1;
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -remote requires another argument\n",
							 MyName );
					exit(1);
				}
				if( ScheddName ) {
					free(ScheddName);
				}
				if( !(ScheddName = get_daemon_name(*ptr)) ) {
					fprintf( stderr, "%s: unknown host %s\n",
							 MyName, get_host_part(*ptr) );
					exit(1);
				}
				query_credential = false;
			} else if (is_dash_arg_prefix(ptr[0], "name", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -name requires another argument\n",
							 MyName );
					exit(1);
				}
				if( ScheddName ) {
					free(ScheddName);
				}
				if( !(ScheddName = get_daemon_name(*ptr)) ) {
					fprintf( stderr, "%s: unknown host %s\n",
							 MyName, get_host_part(*ptr) );
					exit(1);
				}
				query_credential = false;
			} else if (is_dash_arg_prefix(ptr[0], "append", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -append requires another argument\n",
							 MyName );
					exit( 1 );
				}
				// if appended submit line looks like a queue statement, save it off for queue processing.
				if (is_queue_statement(ptr[0])) {
					if ( ! queueCommandLine.empty()) {
						fprintf(stderr, "%s: only one -queue command allowed\n", MyName);
						exit(1);
					}
					queueCommandLine = ptr[0];
				} else {
					extraLines.emplace_back( *ptr );
					GotCmdlineKeys = true;
				}
			} else if (is_dash_arg_prefix(ptr[0], "batch-name", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -batch-name requires another argument\n",
							 MyName );
					exit( 1 );
				}
				const char * bname = *ptr;
				std::string tmp; // if -batch-name was specified, this holds the string 'MY.JobBatchName = "name"'
				if (*bname == '"') {
					formatstr(tmp,"MY.%s = %s", ATTR_JOB_BATCH_NAME, bname);
				} else {
					formatstr(tmp,"MY.%s = \"%s\"", ATTR_JOB_BATCH_NAME, bname);
				}
				// if batch_name_line is not NULL,  we will leak a bit here, but that's better than
				// freeing something behind the back of the extraLines
				extraLines.emplace_back(tmp);
			} else if (is_dash_arg_prefix(ptr[0], "batch-id", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -batch-id requires another argument\n",
							 MyName );
					exit( 1 );
				}
				const char * bid = *ptr;
				std::string tmp; // if -batch-id was specified, this holds the string 'MY.JobBatchId = "id"'
				if (*bid == '"') {
					formatstr(tmp,"MY.%s = %s", ATTR_JOB_BATCH_ID, bid);
				} else {
					formatstr(tmp,"MY.%s = \"%s\"", ATTR_JOB_BATCH_ID, bid);
				}
				// if batch_id_line is not NULL,  we will leak a bit here, but that's better than
				// freeing something behind the back of the extraLines
				extraLines.emplace_back(tmp);
			} else if (is_dash_arg_prefix(ptr[0], "queue", 1)) {
				if( !(--argc) || (!(*ptr[1]) || *ptr[1] == '-')) {
					fprintf( stderr, "%s: -queue requires at least one argument\n",
							 MyName );
					exit( 1 );
				}
				if ( ! queueCommandLine.empty()) {
					fprintf(stderr, "%s: only one -queue command allowed\n", MyName);
					exit(1);
				}

				// The queue command uses arguments until it sees one starting with -
				// we do this so that shell glob expansion behaves like you would expect.
				++ptr;
				queueCommandLine = "queue "; queueCommandLine += ptr[0];
				while (argc > 1) {
					bool is_dash = (MATCH == strcmp(ptr[1], "-"));
					if (is_dash || ptr[1][0] != '-') {
						queueCommandLine += " ";
						queueCommandLine += ptr[1];
						++ptr;
						--argc;
						if ( ! is_dash) continue;
					}
					break;
				}
			} else if (is_dash_arg_prefix (ptr[0], "maxjobs", 3)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -maxjobs requires another argument\n",
							 MyName );
					exit(1);
				}
				// read the next argument to set DashMaxJobs as an integer and
				// set it to the job limit.
				char *endptr;
				DashMaxJobs = strtol(*ptr, &endptr, 10);
				if (*endptr != '\0') {
					fprintf(stderr, "Error: Unable to convert argument (%s) to a number for -maxjobs.\n", *ptr);
					exit(1);
				}
				if (DashMaxJobs < -1) {
					fprintf(stderr, "Error: %d is not a valid for -maxjobs.\n", DashMaxJobs);
					exit(1);
				}
			} else if (is_dash_arg_prefix (ptr[0], "single-cluster", 3)) {
				DashMaxClusters = 1;
			} else if (is_dash_arg_prefix(ptr[0], "pool", 2)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -pool requires another argument\n",
							 MyName );
					exit(1);
				}
				if( PoolName ) {
					free(PoolName);
				}
					// TODO We should try to resolve the name to a full
					//   hostname, but get_full_hostname() doesn't like
					//   seeing ":<port>" at the end, which is valid for a
					//   collector name.
				PoolName = strdup( *ptr );
			} else if (is_dash_arg_prefix(ptr[0], "unused", 1)) {
				WarnOnUnusedMacros = WarnOnUnusedMacros == 1 ? 0 : 1;
				// TOGGLE? 
				// -- why not? if you set it to warn on unused macros in the 
				// config file, there should be a way to turn it off
			} else if (is_dash_arg_prefix(ptr[0], "dump", 2)) {
				if (DashDryRunOutName) {
					fprintf(stderr, "%s: -dump <file> and -dry-run <file> cannot be used together\n", MyName);
					exit(1);
				}
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -dump requires another argument\n",
						MyName );
					exit(1);
				}
				DumpFileName = *ptr;
				DumpClassAdToFile = true;
				DumpFileIsStdout = (MATCH == strcmp(DumpFileName,"-"));
				// if we are dumping to a file, we never want to check file permissions
				// as this would initiate some schedd communication
				DisableFileChecks = 1;
				// we don't really want to do this because there is no real 
				// schedd to query the credentials from...
				query_credential = false;
			} else if (is_dash_arg_prefix(ptr[0], "force-mpi-universe", 7)) {
				use_condor_mpi_universe = true;
			} else if (is_dash_arg_prefix(ptr[0], "allow-crlf-script", 8)) {
				allow_crlf_script = true;
			} else if (is_dash_arg_prefix(ptr[0], "help")) {
				help_info(stdout, --argc, ++ptr);
				exit( 0 );
			} else if (is_dash_arg_colon_prefix(ptr[0], "capabilities", &pcolon, 3)) {
				DashQueryCapabilities = 1;
				if (pcolon && pcolon[0]) {
					int opt = atoi(++pcolon);
					if (opt) DashQueryCapabilities = opt;
				}
			} else if (is_dash_arg_prefix(ptr[0], "interactive", 1)) {
				// we don't currently support -interactive on Windows, but we parse for it anyway.
				dash_interactive = 1;
				extraLines.emplace_back( "MY.InteractiveJob=True" );
			} else {
				usage(stderr);
				exit( 1 );
			}
		} else if (strchr(ptr[0],'=')) {
			// loose arguments that contain '=' are attrib=value pairs.
			// so we split on the = into name and value and insert into the submit hashtable
			// we do this BEFORE parsing the submit file so that they can be referred to by submit.
			std::string name(ptr[0]);
			size_t ix = name.find('=');
			std::string value(name.substr(ix+1));
			name = name.substr(0,ix);
			trim(name); trim(value);
			if ( ! name.empty() && name[1] == '+') {
				name = "MY." + name.substr(1);
			}
			if (name.empty() || ! is_valid_param_name(name.c_str())) {
				fprintf( stderr, "%s: invalid attribute name '%s' for attrib=value assigment\n", MyName, name.c_str() );
				exit(1);
			}
			submit_hash.set_arg_variable(name.c_str(), value.c_str());
			GotCmdlineKeys = true; // there is no need for a command file.
		} else {
			cmd_file = *ptr;
		}
	}

	// Have reading the submit file from stdin imply -verbose. This is
	// for backward compatibility with HTCondor version 8.1.1 and earlier
	// -terse can be used to override the backward compatable behavior.
	CmdFileIsStdin = cmd_file && (MATCH == strcmp(cmd_file, "-"));
	if (CmdFileIsStdin && ! terse) {
		verbose = true;
	}

	// if we got both a queue statement and at least one key=value pair on the command line
	// we don't need a submit file at all.
	if (GotCmdlineKeys && ! queueCommandLine.empty()) {
		NoCmdFileNeeded = true;
	}

	if (!DisableFileChecks) {
		DisableFileChecks = param_boolean_crufty("SUBMIT_SKIP_FILECHECKS", true) ? 1 : 0;
	}

	MaxProcsPerCluster = param_integer("SUBMIT_MAX_PROCS_IN_CLUSTER", 0, 0);

	// the -dry argument takes a qualifier that I'm hijacking to do queue parsing unit tests for now the 8.3 series.
	if (DashDryRun > 0x10) { exit(DoUnitTests(DashDryRun)); }

	// we don't want a schedd instance if we are dumping to a file
	if ( !DumpClassAdToFile && !sim_current_condor_version) {
		// Instantiate our DCSchedd so we can locate and communicate
		// with our schedd.  
        if (!ScheddAddr.empty()) {
            MySchedd = new DCSchedd(ScheddAddr.c_str());
        } else {
            MySchedd = new DCSchedd(ScheddName, PoolName);
        }
		if( ! MySchedd->locate() ) {
			if (ScheddName) {
				fprintf(stderr, "\nERROR: Can't find address of schedd %s\n", ScheddName);
				exit(1);
			} else if ( ! DashDryRun) {
				fprintf(stderr, "\nERROR: Can't find address of local schedd\n");
				exit(1);
			} else {
				// delete the MySchedd so that -dry-run will assume the version of submit
				delete MySchedd; MySchedd = NULL;
			}
		}
	}

	if (DashQueryCapabilities) {
		queue_connect();
		if ( ! MyQ) {
			fprintf(stderr, "Could not connect to schedd to query capabilities");
			exit(1);
		}

		ClassAd ad;
		std::string helpex;
		MyQ->get_Capabilities(ad);
		if (MyQ->has_extended_help(helpex) && ! IsUrl(helpex.c_str())) {
			MyQ->get_ExtendedHelp(helpex);
		}
		schedd_capabilities_help(stdout, ad, helpex, MySchedd, DashQueryCapabilities);

		// TODO: report errors?? can this even fail?
		CondorError errstk;
		MyQ->disconnect(false, errstk);
		exit(0);
	}

	// make sure our shadow will have access to our credential
	// (check is disabled for "-n" and "-r" submits)
	if (query_credential) {

#ifdef WIN32
		// setup the username to query
		std::string userdom;
		auto_free_ptr the_username(my_username());
		auto_free_ptr the_domainname(my_domainname());
		userdom = the_username.ptr();
		userdom += "@";
		if (the_domainname) { userdom += the_domainname.ptr(); }

		// if we have a credd, query it first
		bool cred_is_stored = false;
		int store_cred_result;
		Daemon my_credd(DT_CREDD);
		if (my_credd.locate()) {
			store_cred_result = do_store_cred_passwd(userdom.c_str(), NULL, QUERY_PWD_MODE, &my_credd);
			if ( store_cred_result == SUCCESS ||
							store_cred_result == FAILURE_NO_IMPERSONATE) {
				cred_is_stored = true;
			}
		}

		if (!cred_is_stored) {
			// query the schedd
			store_cred_result = do_store_cred_passwd(userdom.c_str(), NULL, QUERY_PWD_MODE, MySchedd);
			if ( store_cred_result == SUCCESS ||
							store_cred_result == FAILURE_NO_IMPERSONATE) {
				cred_is_stored = true;
			} else if (DashDryRun && (store_cred_result == FAILURE)) {
				// if we can't contact the schedd, just keep going.
				cred_is_stored = true;
			}
		}
		if (!cred_is_stored) {
			fprintf( stderr, "\nERROR: No credential stored for %s\n"
					"\n\tCorrect this by running:\n"
					"\tcondor_store_cred add\n", userdom.c_str() );
			exit(1);
		}
#endif
	}

	// if this is an interactive job, and no cmd_file was specified on the
	// command line, use a default cmd file as specified in the config table.
	if ( dash_interactive ) {
		if ( !cmd_file ) {
			cmd_file = param("INTERACTIVE_SUBMIT_FILE");
			if (cmd_file) {
				InteractiveSubmitFile = 1;
			} 
		}
		// If the user specified their own submit file for an interactive
		// submit, "rewrite" the job to run /bin/sleep.
		// But, if there's an active ssh, stay alive, in case we are
		// running in a container.
		// This effectively means executable, transfer_executable,
		// arguments, universe, and queue X are ignored from the submit
		// file and instead we rewrite to the values below.
		if ( !InteractiveSubmitFile ) {
			auto_free_ptr tmp(param("INTERACTIVE_SUBMIT_CMD_OVERRIDE"));
			if (tmp) submit_hash.set_arg_variable(SUBMIT_KEY_INTERACTIVE_Executable, tmp);
			tmp.set(param("INTERACTIVE_SUBMIT_ARGS_OVERRIDE"));
			if (tmp) submit_hash.set_arg_variable(SUBMIT_KEY_INTERACTIVE_Args, tmp);
		}
	}

	if ( ! cmd_file && (DashDryRunOutName || DumpFileName)) {
		const char * dump_name = DumpFileName ? DumpFileName : DashDryRunOutName;
		fprintf( stderr,
			"\nERROR: No submit filename was provided, and '%s'\n"
			"  was given as the output filename for -dump or -dry-run. Was this intended?\n"
			"  To to avoid confusing the output file with the submit file when using these\n"
			"  commands, an explicit submit filename argument must be given. You can use -\n"
			"  as the submit filename argument to read from stdin.\n",
			dump_name);
		exit(1);
	}

	bool has_late_materialize = false;
	CondorVersionInfo cvi(MySchedd ? MySchedd->version() : CondorVersion());
	if (cvi.built_since_version(8,7,1)) {
		has_late_materialize = true;
		if ( ! DumpClassAdToFile && ! DashDryRun && ! dash_interactive && default_to_factory) { dash_factory = true; }
	}
	if (dash_factory && ! has_late_materialize) {
		fprintf(stderr, "\nERROR: Schedd does not support late materialization\n");
		exit(1);
	} else if (dash_factory && dash_interactive) {
		fprintf(stderr, "\nERROR: Interactive jobs cannot be materialized late\n");
		exit(1);
	}

	// open submit file
	if (CmdFileIsStdin || ( ! cmd_file && ! NoCmdFileNeeded)) {
		// no file specified, read from stdin
		fp = stdin;
		submit_hash.insert_source("<stdin>", FileMacroSource);
	} else {
		const char * submit_filename = cmd_file;
	#ifdef WIN32
		if ( ! cmd_file && NoCmdFileNeeded) { cmd_file = "NUL"; submit_filename = "null"; }
	#else
		if ( ! cmd_file && NoCmdFileNeeded) { cmd_file = "/dev/null"; submit_filename = "null"; }
	#endif
		if( (fp=safe_fopen_wrapper_follow(cmd_file,"r")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open command file (%s) (%s)\n",
						cmd_file, strerror(errno));
			exit(1);
		}
		// this does both insert_source, and also gives a values to the default $(SUBMIT_FILE) expansion
		submit_hash.insert_submit_filename(submit_filename, FileMacroSource);
	}

	// in case things go awry ...
	_EXCEPT_Reporter = ReportSubmitException;
	_EXCEPT_Cleanup = DoCleanup;

	submit_hash.setDisableFileChecks(DisableFileChecks);
	submit_hash.setFakeFileCreationChecks(DashDryRun);
	submit_hash.setScheddVersion(MySchedd ? MySchedd->version() : CondorVersion());
	submit_hash.init_base_ad(get_submit_time(), username);

	if ( !DumpClassAdToFile ) {
		if ( !CmdFileIsStdin && ! terse) {
			fprintf(stdout, DashDryRun ? "Dry-Run job(s)" : "Submitting job(s)");
		}
	} else if ( ! DumpFileIsStdout ) {
		// get the file we are to dump the ClassAds to...
		if ( ! terse) { fprintf(stdout, "Storing job ClassAd(s)"); }
		if( (DumpFile = safe_fopen_wrapper_follow(DumpFileName,"w")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open file to dump ClassAds into (%s)\n",
				strerror(errno));
			exit(1);
		}
	}

	//  Parse the file and queue the jobs
	int as_factory = 0;
	if (has_late_materialize) {
		// turn dash_factory command line option and default_to_factory flag as bits
		// so submit_jobs can be clever about defaults
		// 0=not factory, 1=always factory, 2=smart factory (max_materialize), 3=smart factory (all but single-proc)
		as_factory = (dash_factory ? 1 : 0) | (default_to_factory ? (1|2) : 0);
	}
	int rval = submit_jobs(fp, FileMacroSource, as_factory, extraLines, queueCommandLine);
	if (protectedUrlMap) { delete protectedUrlMap; protectedUrlMap = nullptr; }
	if( rval < 0 ) {
		if( ExtraLineNo == 0 ) {
			fprintf( stderr,
					 "\nERROR: Failed to parse command file (line %d).\n",
					 FileMacroSource.line);
		}
		else {
			fprintf( stderr,
					 "\nERROR: Failed to parse -a argument line (#%d).\n",
					 ExtraLineNo );
		}
		exit(1);
	}

	if( !GotQueueCommand ) {
		fprintf(stderr, "\nERROR: \"%s\" doesn't contain any \"queue\" commands -- no jobs queued\n",
				CmdFileIsStdin ? "(stdin)" : cmd_file);
		exit( 1 );
	} else if ( ! GotNonEmptyQueueCommand && ! terse) {
		fprintf(stderr, "\nWARNING: \"%s\" has only empty \"queue\" commands -- no jobs queued\n",
				CmdFileIsStdin ? "(stdin)" : cmd_file);
	}

	// we can't disconnect from something if we haven't connected to it: since
	// we are dumping to a file, we don't actually open a connection to the schedd
	if (MyQ) {
		CondorError errstack;
		if ( !MyQ->disconnect(true, errstack) ) {
			fprintf(stderr, "\nERROR: Failed to commit job submission into the queue.\n");
			if (errstack.subsys())
			{
				fprintf(stderr, "ERROR: %s\n", errstack.message());
			}
			exit(1);
		}
		if(! errstack.empty()) {
			const char * message = errstack.message();
			fprintf( stderr, "\nWARNING: Committed job submission into the queue with the following warning(s):\n" );
			fprintf( stderr, "WARNING: %s\n", message );
		}
	}

	if ( ! CmdFileIsStdin && ! terse) {
		fprintf(stdout, "\n");
	}

	ErrContext.phase = PHASE_COMMIT;

	if ( ! verbose && ! DumpFileIsStdout) {
		if (terse) {
			size_t ixFirst = 0;
			for (size_t ix = 0; ix < SubmitInfo.size(); ++ix) {
				// fprintf(stderr, "\t%d.%d - %d\n", SubmitInfo[ix].cluster, SubmitInfo[ix].firstjob, SubmitInfo[ix].lastjob);
				if ((ix == (SubmitInfo.size()-1)) || SubmitInfo[ix].cluster != SubmitInfo[ix+1].cluster) {
					if (SubmitInfo[ixFirst].cluster >= 0) {
						fprintf(stdout, "%d.%d - %d.%d\n", 
							SubmitInfo[ixFirst].cluster, SubmitInfo[ixFirst].firstjob,
							SubmitInfo[ix].cluster, SubmitInfo[ix].lastjob);
					}
					ixFirst = ix+1;
				}
			}
		} else {
			int this_cluster = -1, job_count=0;
			for (size_t ix=0; ix < SubmitInfo.size(); ix++) {
				if (SubmitInfo[ix].cluster != this_cluster) {
					if (this_cluster != -1) {
						fprintf(stdout, "%d job(s) %s to cluster %d.\n", job_count, DashDryRun ? "dry-run" : "submitted", this_cluster);
						job_count = 0;
					}
					this_cluster = SubmitInfo[ix].cluster;
				}
				job_count += SubmitInfo[ix].lastjob - SubmitInfo[ix].firstjob + 1;
			}
			if (this_cluster != -1) {
				fprintf(stdout, "%d job(s) %s to cluster %d.\n", job_count, DashDryRun ? "dry-run" : "submitted", this_cluster);
			}
		}
	}

	ActiveQueueConnection = FALSE; 

	if ( MySchedd && !DisableFileChecks) {
		TestFilePermissions( MySchedd->addr() );
	}

	// we don't want to spool jobs if we are simply writing the ClassAds to 
	// a file, so we just skip this block entirely if we are doing this...
	if ( MySchedd && !DumpClassAdToFile ) {
		if ( dash_remote && JobAdsArray.size() > 0 ) {
			bool result;
			CondorError errstack;

			// perhaps check for proper schedd version here?
			result = MySchedd->spoolJobFiles( (int)JobAdsArray.size(),
			                                  JobAdsArray.data(),
			                                  &errstack );
			if ( !result ) {
				fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
				fprintf( stderr, "ERROR: Failed to spool job files.\n" );
				exit(1);
			}
		}
	}

	// don't try to reschedule jobs if we are dumping to a file, because, again
	// there will be not schedd present to reschedule thru.
	if ( !DumpClassAdToFile) {
		if( ProcId != -1 ) {
			reschedule();
		}
	}

	if (DashDryRun && DashDryRunFullAds) {
		FILE* dryfile = NULL;
		bool  close_it = false;
		if (MATCH == strcmp(DashDryRunOutName, "-")) {
			dryfile = stdout; close_it = false;
			fputs("\n", stdout);
		} else {
			dryfile = safe_fopen_wrapper_follow(DashDryRunOutName,"wb");
			if ( ! dryfile) {
				fprintf( stderr, "\nERROR: Failed to open file -dry-run output file (%s)\n", strerror(errno));
				exit(1);
			}
			close_it = true;
		}
		std::string out;
		for (size_t idx=0;idx<JobAdsArray.size();idx++) {
			if (idx > 0) { out = "\n"; }
			// for testing...
			//formatstr_cat(out, "-- %d %p chainedto %p\n", (int)idx, JobAdsArray[idx], JobAdsArray[idx]->GetChainedParentAd());
			formatAd(out, *JobAdsArray[idx], nullptr, nullptr, false);
			fputs(out.c_str(), dryfile);
		}
		if (close_it) { fclose(dryfile); }
	}

	// Deallocate some memory just to keep Purify happy
	for (size_t idx=0;idx<JobAdsArray.size();idx++) {
		delete JobAdsArray[idx];
	}
	submit_hash.delete_job_ad();
	delete MySchedd;

	// look for warnings from parsing the submit file.
	print_submit_parse_warnings(stderr, submit_hash.error_stack());

	/*	print all of the parameters that were not actually expanded/used 
		in the submit file. but not if we never queued any jobs. since
		there would be a ton of false positives in that case.
	*/
	if (WarnOnUnusedMacros && GotNonEmptyQueueCommand) {
		if (verbose) { fprintf(stdout, "\n"); }
		submit_hash.warn_unused(stderr);
		// if there was an errorstack, then the above populates the errorstack rather than printing to stdout
		// so we now flush the errstack to stdout.
		print_errstack(stderr, submit_hash.error_stack());
	}

	// If this is an interactive job, spawn ssh_to_job -auto-retry -X, and also
	// with pool and schedd names if they were passed into condor_submit
#ifdef WIN32
	if (dash_interactive &&  ClusterId != -1) {
		fprintf( stderr, "ERROR: -interactive not supported on this platform.\n" );
		exit(1);
	}
#else
	if (dash_interactive &&  ClusterId != -1) {
		char jobid[40];
		int i,j,rval;
		const char* sshargs[15];

		for (i=0;i<15;i++) sshargs[i]=NULL; // init all points to NULL
		i=0;
		sshargs[i++] = "condor_ssh_to_job"; // note: this must be in the PATH
		sshargs[i++] = "-auto-retry";
		sshargs[i++] = "-remove-on-interrupt";
		sshargs[i++] = "-X";
		if (debug) {
			sshargs[i++] = "-debug";
		}
		if (PoolName) {
			sshargs[i++] = "-pool";
			sshargs[i++] = PoolName;
		}
		if (ScheddName) {
			sshargs[i++] = "-name";
			sshargs[i++] = ScheddName;
		}
		snprintf(jobid, sizeof(jobid), "%d.0", ClusterId);
		sshargs[i++] = jobid;
		sleep(3);	// with luck, schedd will start the job while we sleep
		// We cannot fork before calling ssh_to_job, since ssh will want
		// direct access to the pseudo terminal. Since util functions like
		// my_spawn, my_popen, my_system and friends all fork, we cannot use
		// them. Since this is all specific to Unix anyhow, call exec directly.
		rval = execvp("condor_ssh_to_job", const_cast<char *const*>(sshargs));
		if (rval == -1 ) {
			int savederr = errno;
			fprintf( stderr, "ERROR: Failed to spawn condor_ssh_to_job" );
			// Display all args to ssh_to_job to stderr
			for (j=0;j<i;j++) {
				fprintf( stderr, " %s",sshargs[j] );
			}
			fprintf( stderr, " : %s\n",strerror(savederr));
			exit(1);
		}
	}
#endif

	return 0;
}

// check if path is a (broken) interpreter script with dos line endings
bool is_crlf_shebang(const char *path)
{
	char buf[128];     // BINPRM_BUF_SIZE from <linux/binfmts.h>; also:
	bool ret = false;  // execve(2) says the max #! line length is 127
	FILE *fp = safe_fopen_no_create(path, "rb");

	if (!fp) {
		// can't open, don't worry about it
		return false;
	}

	// check first line for CRLF ending if readable and starts with #!
	if (fgets(buf, sizeof buf, fp) && buf[0] == '#' && buf[1] == '!') {
		size_t len = strlen(buf);
		ret = (len > 2) && (buf[len-1] == '\n' && buf[len-2] == '\r');
	}

	fclose(fp);
	return ret;
}

// callback passed to make_job_ad on the submit_hash that gets passed each input or output file
// so we can choose to do file checks.
int check_sub_file(void* /*pv*/, SubmitHash * sub, _submit_file_role role, const char * pathname, int flags)
{
	if ((pathname == NULL) && (role != SFR_PSEUDO_EXECUTABLE)) {
		fprintf(stderr, "\nERROR: NULL filename\n");
		return 1;
	}
	if (role == SFR_LOG) {
		if ( !DumpClassAdToFile && !DashDryRun ) {
			// check that the log is a valid path
			if ( !DisableFileChecks ) {
				FILE* test = safe_fopen_wrapper_follow(pathname, "a+", 0664);
				if (!test) {
					fprintf(stderr,
							"\nERROR: Invalid log file: \"%s\" (%s)\n", pathname,
							strerror(errno));
					return 1;
				} else {
					fclose(test);
				}
			}

			// Check that the log file isn't on NFS
			bool nfs_is_error = param_boolean("LOG_ON_NFS_IS_ERROR", false);
			bool nfs = false;

			if ( nfs_is_error ) {
				if ( fs_detect_nfs( pathname, &nfs ) != 0 ) {
					fprintf(stderr,
							"\nWARNING: Can't determine whether log file %s is on NFS\n",
							pathname );
				} else if ( nfs ) {

					fprintf(stderr,
							"\nERROR: Log file %s is on NFS.\nThis could cause"
							" log file corruption. Condor has been configured to"
							" prohibit log files on NFS.\n",
							pathname );

					return 1;

				}
			}
		}
		return 0;

	} else if (role == SFR_EXECUTABLE || role == SFR_PSEUDO_EXECUTABLE) {
		const char * ename = pathname;
		bool transfer_it = (flags & 1) != 0;

		if (!ename) transfer_it = false;

        LastExecutable = empty_if_null(ename);
		SpoolLastExecutable = false;

		// ensure the executables exist and spool them only if no
		// $$(arch).$$(opsys) are specified  (note that if we are simply
		// dumping the class-ad to a file, we won't actually transfer
		// or do anything [nothing that follows will affect the ad])
		if ( transfer_it && !DumpClassAdToFile && !strstr(ename,"$$") ) {

			StatInfo si(ename);
			if ( SINoFile == si.Error () ) {
				fprintf ( stderr, "\nERROR: Executable file %s does not exist\n", ename );
				return 1; // abort
			}

			if (!si.Error() && (si.GetFileSize() == 0)) {
				fprintf( stderr, "\nERROR: Executable file %s has zero length\n", ename );
				return 1; // abort
			}

			if (is_crlf_shebang(ename)) {
				fprintf( stderr, "\n%s: Executable file %s is a script with "
					"CRLF (DOS/Windows) line endings.\n",
					allow_crlf_script ? "WARNING" : "ERROR", ename );
				if (!allow_crlf_script) {
					fprintf( stderr, "This generally doesn't work, and you "
						"should probably run 'dos2unix %s' -- or a similar "
						"tool -- before you resubmit.\n", ename );
					return 1; // abort
				}
			}

			if (role == SFR_EXECUTABLE) {
				bool param_exists;
				SpoolLastExecutable = sub->submit_param_bool( SUBMIT_KEY_CopyToSpool, "CopyToSpool", false, &param_exists );
				if ( ! param_exists) {
						// In so many cases, copy_to_spool=true would just add
						// needless overhead.  Therefore, (as of 6.9.3), the
						// default is false.
					SpoolLastExecutable = false;
				}
			}
		}
		return 0;
	}

	// Queue files for testing access if not already queued
	if( flags & O_WRONLY )
	{
		// add this file to our list; no-op if already there
		CheckFilesWrite.insert(pathname);
	}
	else
	{
		// add this file to our list; no-op if already there
		CheckFilesRead.insert(pathname);
	}
	return 0; // of the check is ok,  nonzero to abort
}

/*
** Send the reschedule command to the local schedd to get the jobs running
** as soon as possible.
*/
void
reschedule()
{
	if ( ! MySchedd)
		return;

	if ( param_boolean("SUBMIT_SEND_RESCHEDULE",true) ) {
		Stream::stream_type st = MySchedd->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
		if (DashDryRun) {
			if (DashDryRun & 2) {
				fprintf(stdout, "::sendCommand(RESCHEDULE, %s, 0)\n", 
					(st == Stream::safe_sock) ? "UDP" : "TCP");
			}
			return;
		}
		if ( ! MySchedd->sendCommand(RESCHEDULE, st, 0) ) {
			fprintf( stderr,
					 "Can't send RESCHEDULE command to condor scheduler\n" );
			DoCleanup(0,0,NULL);
		}
	}
}


// Check to see if this is a queue statement, if it is, return a pointer to the queue arguments.
// 
const char * is_queue_statement(const char * line)
{
	const int cchQueue = sizeof("queue")-1;
	if (starts_with_ignore_case(line, "queue") && (0 == line[cchQueue] || isspace(line[cchQueue]))) {
		const char * pqargs = line+cchQueue;
		while (*pqargs && isspace(*pqargs)) ++pqargs;
		return pqargs;
	}
	return NULL;
}


int iterate_queue_foreach(int queue_num, StringList & vars, StringList & items, qslice & slice)
{
	int rval;
	const char* token_seps = ", \t";
	const char* token_ws = " \t";

	int queue_item_opts = 0;
	if (submit_hash.submit_param_bool("SubmitWarnEmptyFields", "submit_warn_empty_fields", true)) {
		queue_item_opts |= QUEUE_OPT_WARN_EMPTY_FIELDS;
	}
	if (submit_hash.submit_param_bool("SubmitFailEmptyFields", "submit_fail_empty_fields", false)) {
		queue_item_opts |= QUEUE_OPT_FAIL_EMPTY_FIELDS;
	}

	if (queue_num > 0) {
		if ( ! MyQ) {
			int rval = queue_connect();
			if (rval < 0)
				return rval;
		}

		rval = queue_begin(vars, NewExecutable || (ClusterId < 0)); // called before iterating items
		if (rval < 0)
			return rval;

		char * item = NULL;
		if (items.isEmpty()) {
			rval = queue_item(queue_num, vars, item, 0, queue_item_opts, token_seps, token_ws);
		} else {
			int citems = items.number();
			items.rewind();
			int item_index = 0;
			while ((item = items.next())) {
				if (slice.selected(item_index, citems)) {
					rval = queue_item(queue_num, vars, item, item_index, queue_item_opts, token_seps, token_ws);
					if (rval < 0)
						break;
				}
				++item_index;
			}
		}

		// make sure vars don't continue to reference the o.items data that we are about to free.
		cleanup_vars(submit_hash, vars);
		if (rval < 0)
			return rval;
	}

	return 0;
}

//PRAGMA_REMIND("TODO: move this into submit_hash?")
int ParseDashAppendLines(std::vector<std::string> &exlines, MACRO_SOURCE& source, MACRO_SET& macro_set)
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");

	ErrContext.phase = PHASE_DASH_APPEND;
	ExtraLineNo = 0;
	for (const auto &exline: exlines) {
		++ExtraLineNo;
		int rval = Parse_config_string(source, 1, exline.c_str(), macro_set, ctx);
		if (rval < 0)
			return rval;
		rval = 0;
	}
	ExtraLineNo = 0;
	return 0;
}

bool CheckForNewExecutable(MACRO_SET& macro_set) {
    static std::string last_submit_executable;
    static std::string last_submit_cmd;

	bool new_exe = false;

	// HACK! the queue function uses a global flag to know whether or not to ask for a new cluster
	// In 8.2 this flag is set whenever the "executable" or "cmd" value is set in the submit file.
	// As of 8.3, we don't believe this is still necessary, but we are afraid to change it. This
	// code is *mostly* the same, but will differ in cases where the users is setting cmd or executble
	// to the *same* value between queue statements. - hopefully this is close enough.

	std::string cur_submit_executable(lookup_macro_exact_no_default(SUBMIT_KEY_Executable, macro_set, 0));
	if (last_submit_executable != cur_submit_executable) {
		new_exe = true;
		last_submit_executable = cur_submit_executable;
	}
	std::string cur_submit_cmd(lookup_macro_exact_no_default("cmd", macro_set, 0));
	if (last_submit_cmd != cur_submit_cmd) {
		new_exe = true;
		last_submit_cmd = cur_submit_cmd;
	}

	return new_exe;
}

static bool jobset_ad_is_trivial(const ClassAd *ad)
{
	if ( ! ad) return true;
	for (auto it : *ad) {
		if (YourStringNoCase(ATTR_JOB_SET_NAME) == it.first) continue;
		return false;
	}
	return true;
}

int send_jobset_ad(SubmitHash & hash, int ClusterId)
{
	int rval = 0;
	const ClassAd * jobsetAd = hash.getJOBSET();
	if (jobsetAd) {
		int jobset_version = 0;
		if (MyQ->has_send_jobset(jobset_version)) {
			rval = MyQ->send_Jobset(ClusterId, jobsetAd);
			if (rval == 0 || rval == 1) {
				rval = 0;
			} else {
				fprintf( stderr, "\nERROR: Failed to submit jobset.\n" );
				return rval;
			}
		} else {
			fprintf(stderr, "\nWARNING: schedd does not support jobsets.%s\n",
				jobset_ad_is_trivial(jobsetAd) ? "" : "  Ignoring JOBSET.* attributes");
		}
	}
	return rval;
}

int send_cluster_ad(SubmitHash & hash, int ClusterId, bool is_interactive, bool is_remote)
{
	int rval = 0;

	// make the cluster ad.
	//
	// use make_job_ad for job 0 to populate the cluster ad.
	void * pv_check_arg = NULL; // future?
	ClassAd * job = hash.make_job_ad(JOB_ID_KEY(ClusterId,0),
	                                 0, 0, is_interactive, is_remote,
	                                 check_sub_file, pv_check_arg);
	if ( ! job) {
		return -1;
	}

	classad::ClassAd * clusterAd = job->GetChainedParentAd();
	if ( ! clusterAd) {
		fprintf( stderr, "\nERROR: no cluster ad.\n" );
		rval = -1;
		goto bail;
	}

	if ( ! hash.getUniverse()) {
		fprintf( stderr, "\nERROR: job has no universe.\n" );
		rval = -1;
		goto bail;
	}

	// if this submission is using jobsets, send the jobset ad first
	rval = send_jobset_ad(hash, ClusterId);
	if (rval >= 0) {
		SendLastExecutable(); // if spooling the exe, send it now.
		rval = MySendJobAttributes(JOB_ID_KEY(ClusterId,-1), *clusterAd, setattrflags);
		if (rval < 0) {
			fprintf( stderr, "\nERROR: Failed to queue job.\n" );
		}
	}

bail:
	if (rval == 0 || rval == 1) {
		rval = 0;
	}
	hash.delete_job_ad();
	job = NULL;

	return rval;
}

int submit_jobs (
	FILE * fp,
	MACRO_SOURCE & source,
	int as_factory,                  // 0=not factory, 1=dash_factory, 2=smart factory (max_materialize), 3=smart factory (all but single-proc)
	std::vector<std::string> & append_lines, // lines passed in via -a argument
	std::string & queue_cmd_line)    // queue statement passed in via -q argument
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");
	std::string errmsg;
	int rval = -1;
	const char* token_seps = ", \t";
	const char* token_ws = " \t";

	// if there is a queue statement from the command line, get ready to parse it.
	// we will end up actually parsing it only if there is no queue statement in the file.
	// we make a copy here because parsing the queue line is destructive.
	auto_free_ptr qcmd;
	if ( ! queue_cmd_line.empty()) { qcmd.set(strdup(queue_cmd_line.c_str())); }

	MacroStreamYourFile ms(fp, source);

	// set want_factory to 0 or 1 when we know for sure whether it is a factory submit or not.
	// set need_factory to 1 if we should fail if we cannot submit a factory job.
	int want_factory = as_factory ? 1 : -1;
	int need_factory = (as_factory & 1) != 0;
	long long max_materialize = INT_MAX;
	long long max_idle = INT_MAX;

	// there can be multiple queue statements in the file, we need to process them all.
	for (;;) {
		if (feof(fp)) { break; }

		// Error if any data (non-comment/blank lines) exist post queue for late materialization
		// and if AP admin is preventing multi-queue statement submits
		if (GotQueueCommand && (want_factory || param_boolean("SUBMIT_PREVENT_MULTI_QUEUE", false))) {
			if ( ! CmdFileIsStdin) {
				bool extra_data = false;
				const char* line;
				while ((line = getline_trim(ms)) != nullptr) {
					if (*line == '#' || blankline(line)) { continue; }
					errmsg = "Extra data found after queue statement";
					rval = -1;
					extra_data = true;
					break;
				}
				if (extra_data) { break; }
			} else{
				const char* line = getline_trim(ms);
				if (line && !blankline(line)) {
					errmsg = "Extra data was provided to STDIN after queue statement";
					rval = -1;
					break;
				}
			}
		}

		ErrContext.phase = PHASE_READ_SUBMIT;

		char * qline = NULL;
		rval = submit_hash.parse_up_to_q_line(ms, errmsg, &qline);
		if (rval)
			break;

		if ( ! qline && GotQueueCommand) {
			// after we have seen a queue command, if we parse and get no queue line
			// then we have hit the end of file. so just break out of the loop.
			// this is a common and normal way to exit the loop.
			break;
		}

		// parse the extra lines before doing $ expansion on the queue line
		rval = ParseDashAppendLines(append_lines, source, submit_hash.macros());
		if (rval < 0)
			break;

		// we'll use the GotQueueCommand flag as a convenient flag for 'this is the first iteration of the loop'
		if ( ! GotQueueCommand) {
			// If this turns out to be late materialization, the schedd will need to know what the
			// current working directory was when we parsed the submit file. so stuff it into the submit hash
			std::string FactoryIwd;
			condor_getcwd(FactoryIwd);
			insert_macro("FACTORY.Iwd", FactoryIwd.c_str(), submit_hash.macros(), DetectedMacro, ctx);

			// the hash should be fully populated now, so we can optimize it for lookups.
			submit_hash.optimize();

			// Prime the globals we use to detect a change in executable, and force NewExecutable to be true
			CheckForNewExecutable(submit_hash.macros());
			NewExecutable = true;
		} else {
			NewExecutable = CheckForNewExecutable(submit_hash.macros());
		}

		// do the dry-run logging of queue arguments
		if (DashDryRun && DumpSubmitHash) {
			if ( ! queue_cmd_line.empty()) {
				auto_free_ptr expanded_queue_args(expand_macro(queue_cmd_line.c_str(), submit_hash.macros(), ctx));
				char * pqargs = expanded_queue_args.ptr();
				ASSERT(pqargs);
				fprintf(stdout, "\n----- -queue command line -----\nSpecified: %s\nExpanded : %s", queue_cmd_line.c_str(), pqargs);
			}
			if (qline) {
				auto_free_ptr expanded_queue_args(expand_macro(qline, submit_hash.macros(), ctx));
				char * pqargs = expanded_queue_args.ptr();
				ASSERT(pqargs);
				fprintf(stdout, "\n----- Queue arguments -----\nSpecified: %s\nExpanded : %s", qline, pqargs);
			}
		}

		// setup for parsing the queue args, this can come from either the command line,
		// or from the submit file, but NOT BOTH.
		const char * queue_args = NULL;
		if (qline) {
			queue_args = is_queue_statement(qline);
			ErrContext.phase = PHASE_QUEUE;
			ErrContext.step = -1;
		} else if (qcmd) {
			queue_args = is_queue_statement(qcmd);
			ErrContext.phase = PHASE_QUEUE_ARG;
			ErrContext.step = -1;
		}
		if ( ! queue_args) {
			errmsg = "no queue statement";
			rval = -1;
			break;
		}
		GotQueueCommand += 1;

		// check for conflict between queue line and -queue command line
		if (qline && qcmd) {
			errmsg = "-queue argument conflicts with queue statement in submit file";
			rval = -1;
			break;
		}

		// expand and parse the queue statement
		SubmitForeachArgs o;
		rval = submit_hash.parse_q_args(queue_args, o, errmsg);
		if (rval)
			break;


		if(! sent_credential_to_credd) {
			std::string URL;
			std::string error_string;
			int cred_result = process_job_credentials(
				submit_hash,
				DashDryRun,
				URL,
				error_string
			);
			if( cred_result != 0 ) {
				// what is the best way to bail out / abort the submit process?
				printf( "Failed to process job credential requests (%d): '%s'; BAILING OUT.\n", cred_result, error_string.c_str() );
				exit(1);
			}
			if(! URL.empty()) {
				char * user_name = my_username();
				fprintf( stdout, "\nHellow, %s.\nPlease visit: %s\n\n", user_name, URL.c_str() );
				free( user_name );
				exit(1);
			}
			sent_credential_to_credd = true;
		}


		// load the foreach data
		rval = submit_hash.load_inline_q_foreach_items(ms, o, errmsg);
		if (rval == 1) { // 1 means forech data is external
			rval = submit_hash.load_external_q_foreach_items(o, true, errmsg);
		}
		if (rval)
			break;

		// figure out how many items we will be making jobs from taking the slice [::] into account
		int selected_item_count = o.item_len();

		// if this is the first queue command, and we don't yet know if this is a job factory, decide now.
		// we do this after we parse the first queue command so that we can use the size of the queue statement
		// to decide whether this is a factory or not.
		if (as_factory == 3) {
			// if as_factory == 3, then we want 'smart' factory, where we use late materialize
			// for multi-proc and non-factory for single proc
			want_factory = (selected_item_count > 1 || o.queue_num > 1) ? 1 : 0;
			need_factory = want_factory;
		}
		if (GotQueueCommand == 1) {
			if (submit_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeLimit, ATTR_JOB_MATERIALIZE_LIMIT, max_materialize, true)) {
				want_factory = 1;
			} else if (submit_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeMaxIdle, ATTR_JOB_MATERIALIZE_MAX_IDLE, max_idle, true) ||
				       submit_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeMaxIdleAlt, ATTR_JOB_MATERIALIZE_MAX_IDLE, max_idle, true)) {
				max_materialize = INT_MAX; // no max materialize specified, so set it to number of jobs
				want_factory = 1;
			} else if (want_factory < 0) {
				//PRAGMA_REMIND("are there other conditions were we promote to factory?")
				want_factory = 0;
			}
		}

		// if this is an empty queue statement, there is nothing to queue...
		if (selected_item_count == 0 || o.queue_num < 0) {
			if (feof(fp)) {
				break;
			} else {
				continue;
			}
			//PRAGMA_REMIND("check if this properly handles empty submit item lists and/or multiple queue lines")
		}

		int queue_item_opts = 0;
		if (submit_hash.submit_param_bool("SubmitWarnEmptyFields", "submit_warn_empty_fields", true)) {
			queue_item_opts |= QUEUE_OPT_WARN_EMPTY_FIELDS;
		}
		if (submit_hash.submit_param_bool("SubmitFailEmptyFields", "submit_fail_empty_fields", false)) {
			queue_item_opts |= QUEUE_OPT_FAIL_EMPTY_FIELDS;
		}

		// ===== begin talking to schedd here ===
		if ( ! MyQ) {
			int rval = queue_connect();
			if (rval < 0)
				break;

			if (want_factory && ! MyQ->allows_late_materialize()) {
				// if factory was required, not just preferred. then we fail the submit
				if (need_factory) {
					int late_ver = 0;
					if (MyQ->has_late_materialize(late_ver)) {
						if (late_ver < 2) {
							fprintf(stderr, "\nERROR: This SCHEDD allows only an older Late materialization protocol\n");
						} else {
							fprintf(stderr, "\nERROR: Late materialization is not allowed by this SCHEDD\n");
						}
					} else {
						fprintf(stderr, "\nERROR: The SCHEDD is too old to support late materialization\n");
					}
					WarnOnUnusedMacros = false; // no point on checking for unused macros.
					rval = -1;
					break;
				}
				// otherwise we just fall back to non-factory submit
				want_factory = 0;
			}

			ClassAd extended_submit_commands;
			if (MyQ->has_extended_submit_commands(extended_submit_commands)) {
				submit_hash.addExtendedCommands(extended_submit_commands);
			}
			submit_hash.attachTransferMap(protectedUrlMap);
		}

		// At this point we really expect to  have a working queue connection (possibly simulated)
		if ( ! MyQ) { rval = -1; break; }

		// allocate a cluster object if this is the first time through the loop, or if the executable changed.
		if (NewExecutable || (ClusterId < 0)) {
			rval = allocate_a_cluster();
			if (rval < 0)
				break;
		}

		if (want_factory) { // factory submit

			// turn the submit hash into a submit digest
			std::string submit_digest;
			submit_hash.make_digest(submit_digest, ClusterId, o.vars, 0);
			if (submit_digest.empty()) {
				rval = -1;
				break;
			}

			// if generating a dry-run digest file with a generic name, also generate a dry-run itemdata file
			if (create_local_factory_file && (MyQ->get_type() == AbstractQ_TYPE_SIM)) {
				std::string items_fn = create_local_factory_file;
				items_fn += ".items";
				static_cast<SimScheddQ*>(MyQ)->echo_Itemdata(submit_hash.full_path(items_fn.c_str(), false));
			}
			rval = MyQ->send_Itemdata(ClusterId, o);
			if (rval < 0)
				break;

			// append the revised queue statement to the submit digest
			rval = append_queue_statement(submit_digest, o);
			if (rval < 0)
				break;

			// write the submit digest to the current working directory.
			//PRAGMA_REMIND("todo: force creation of local factory file if schedd is version < 8.7.3?")
			if (create_local_factory_file) {
				std::string factory_path = submit_hash.full_path(create_local_factory_file, false);
				rval = write_factory_file(factory_path.c_str(), submit_digest.data(), (int)submit_digest.size(), 0644);
				if (rval < 0)
					break;
			}

			// materialize all of the jobs unless the user requests otherwise.
			// (the admin can also set a limit which is applied at the schedd)
			int total_procs = (o.queue_num?o.queue_num:1) * o.item_len();
			if (total_procs > 0) GotNonEmptyQueueCommand = 1;
			if (max_materialize <= 0) max_materialize = INT_MAX;
			max_materialize = MIN(max_materialize, total_procs);
			max_materialize = MAX(max_materialize, 1);

			if (DashDryRun && DumpSubmitDigest) {
				fprintf(stdout, "\n----- submit digest -----\n%s-----\n", submit_digest.c_str());
			}

			// send the submit digest to the schedd. the schedd will parse the digest at this point
			// and return success or failure.
			rval = MyQ->set_Factory(ClusterId, (int)max_materialize, "", submit_digest.c_str());
			if (rval < 0)
				break;

			init_vars(submit_hash, ClusterId, o.vars);

			if (DashDryRun && DumpSubmitHash) {
				fprintf(stdout, "\n----- submit hash at queue begin -----\n");
				submit_hash.dump(stdout, DumpSubmitHash & 0xF);
				if (DumpSubmitHash & 0x80) {
					fprintf(stdout, "\n----- templates -----\n");
					submit_hash.dump_templates(stdout, "TEMPLATE", DumpSubmitHash & 0xFF);
				}
				fprintf(stdout, "-----\n");
			}

			// stuff foreach data for the first item before we make the cluster ad.
			char * item = o.items.first();
			rval = set_vars(submit_hash, o.vars, item, 0, queue_item_opts, token_seps, token_ws);
			if (rval < 0)
				break;

			// submit the cluster ad
			//
			rval = send_cluster_ad(submit_hash, ClusterId, dash_interactive, dash_remote);
			if (rval < 0)
				break;

			cleanup_vars(submit_hash, o.vars);

			// tell main what cluster was submitted and how many jobs.
			set_factory_submit_info(ClusterId, total_procs);

		} else { // non-factory submit

			init_vars(submit_hash, ClusterId, o.vars);

			if (DashDryRun && DumpSubmitHash) {
				fprintf(stdout, "\n----- submit hash at queue begin -----\n");
				submit_hash.dump(stdout, DumpSubmitHash & 0xF);
				if (DumpSubmitHash & 0x80) {
					fprintf(stdout, "\n----- templates -----\n");
					submit_hash.dump_templates(stdout, "TEMPLATE", DumpSubmitHash & 0xFF);
				}
				fprintf(stdout, "-----\n");
			}

			char * item = NULL;
			if (o.items.isEmpty()) {
				rval = queue_item(o.queue_num, o.vars, item, 0, queue_item_opts, token_seps, token_ws);
			} else {
				int citems = o.items.number();
				o.items.rewind();
				int item_index = 0;
				while ((item = o.items.next())) {
					if (o.slice.selected(item_index, citems)) {
						rval = queue_item(o.queue_num, o.vars, item, item_index, queue_item_opts, token_seps, token_ws);
						if (rval < 0)
							break;
					}
					++item_index;
				}
			}

			// make sure vars don't continue to reference the o.items data that we are about to free.
			cleanup_vars(submit_hash, o.vars);
		}

		if (DashDryRun && DumpJOBSETClassad) {
			fprintf(stdout, "\n----- jobset -----\n");
			if (submit_hash.getJOBSET()) {
				fPrintAd(stdout, *submit_hash.getJOBSET());
			}
			fprintf(stdout, "-----\n");
		}

		// if there was a failure, quit out of this loop.
		if (rval < 0)
			break;

	} // end for(;;)

	submit_hash.detachTransferMap();

	if (GotQueueCommand > 1) {
		fprintf(stderr, "\nWarning: Use of multiple queue statements in a single submit file is deprecated."
		                "\n         This functionality will be removed in the V24 feature series. To see"
		                "\n         how multiple queue statements can be converted into one visit:"
		                "\nhttps://htcondor.readthedocs.io/en/latest/auto-redirect.html?category=example&tag=convert-multi-queue-statements\n");
	}

	// report errors from submit
	//
	if( rval < 0 ) {
		fprintf (stderr, "\nERROR: on Line %d of submit file: %s\n", source.line, errmsg.c_str());
		if (submit_hash.error_stack()) {
			std::string errstk(submit_hash.error_stack()->getFullText());
			if ( ! errstk.empty()) {
				fprintf(stderr, "%s", errstk.c_str());
			}
			submit_hash.error_stack()->clear();
		}
	}

	return rval;
}

#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
  // To facilitate processing of job status from the
  // job_queue.log, the ATTR_JOB_STATUS attribute should not
  // be stored within the cluster ad. Instead, it should be
  // directly part of each job ad. This change represents an
  // increase in size for the job_queue.log initially, but
  // the ATTR_JOB_STATUS is guaranteed to be specialized for
  // each job so the two approaches converge. -mattf 1 June 09
  // tj 2015 - it also wastes space in the schedd, so we should probably stop doign this. 
static const char * no_cluster_attrs[] = {
	ATTR_JOB_STATUS,
};
bool IsNoClusterAttr(const char * name) {
	for (size_t ii = 0; ii < COUNTOF(no_cluster_attrs); ++ii) {
		if (MATCH == strcasecmp(name, no_cluster_attrs[ii]))
			return true;
	}
	return false;
}
#else
void SetNoClusterAttr(const char * name)
{
	NoClusterCheckAttrs.append( name );
}
#endif


void
connect_to_the_schedd()
{
	if ( ActiveQueueConnection ) {
		// we are already connected; do not connect again
		return;
	}

	setupAuthentication();

	CondorError errstack;
	ActualScheddQ *ScheddQ = new ActualScheddQ();
	if( ScheddQ->Connect(*MySchedd, errstack) == 0 ) {
		delete ScheddQ;
		if( ScheddName ) {
			fprintf( stderr, 
					"\nERROR: Failed to connect to queue manager %s\n%s\n",
					 ScheddName, errstack.getFullText(true).c_str() );
		} else {
			fprintf( stderr, 
				"\nERROR: Failed to connect to local queue manager\n%s\n",
				errstack.getFullText(true).c_str() );
		}
		exit(1);
	}

	MyQ = ScheddQ;
	ActiveQueueConnection = TRUE;
}


int queue_connect()
{
	if ( ! ActiveQueueConnection)
	{
		if (DumpClassAdToFile || DashDryRun) {
			SimScheddQ* SimQ = new SimScheddQ(sim_starting_cluster);
			if (DumpFileIsStdout) {
				SimQ->Connect(DashDryRunFullAds ? nullptr : stdout, false, false);
			} else if (DashDryRun) {
				FILE* dryfile = NULL;
				bool  free_it = false;
				if ( ! DashDryRunFullAds) {
					if (MATCH == strcmp(DashDryRunOutName, "-")) {
						dryfile = stdout; free_it = false;
					} else {
						dryfile = safe_fopen_wrapper_follow(DashDryRunOutName,"w");
						if ( ! dryfile) {
							fprintf( stderr, "\nERROR: Failed to open file -dry-run output file (%s)\n", strerror(errno));
							exit(1);
						}
						free_it = true;
					}
				}
				SimQ->Connect(dryfile, free_it, (DashDryRun&2)!=0);
			} else {
				SimQ->Connect(DumpFile, true, false);
				DumpFile = NULL;
			}
			MyQ = SimQ;
		} else {
			connect_to_the_schedd();
		}
		ASSERT(MyQ);
	}
	ActiveQueueConnection = MyQ != NULL;
	return MyQ ? 0 : -1;
}

// buffers used while processing the queue statement to inject $(Cluster) and $(Process) into the submit hash table.
static char ClusterString[20]="1", ProcessString[20]="0", EmptyItemString[] = "";
void init_vars(SubmitHash & hash, int cluster_id, StringList & vars)
{
	snprintf(ClusterString, sizeof(ClusterString), "%d", cluster_id);
	strcpy(ProcessString, "0");

	// establish live buffers for $(Cluster) and $(Process), and other loop variables
	// Because the user might already be using these variables, we can only set the explicit ones
	// unconditionally, the others we must set only when not already set by the user.
	hash.set_live_submit_variable(SUBMIT_KEY_Cluster, ClusterString);
	hash.set_live_submit_variable(SUBMIT_KEY_Process, ProcessString);
	
	vars.rewind();
	char * var;
	while ((var = vars.next())) { hash.set_live_submit_variable(var, EmptyItemString, false); }

	// optimize the macro set for lookups if we inserted anything.  we expect this to happen only once.
	hash.optimize();
}

// DESTRUCTIVELY! parse 'item' and store the fields into the submit hash as 'live' variables.
//
int set_vars(SubmitHash & hash, StringList & vars, char * item, int item_index, int options, const char * delims, const char * ws)
{
	int rval = 0;

	bool check_empty = options & (QUEUE_OPT_WARN_EMPTY_FIELDS|QUEUE_OPT_FAIL_EMPTY_FIELDS);
	bool fail_empty = options & QUEUE_OPT_FAIL_EMPTY_FIELDS;

	// UseStrict = condor_param_bool("Submit.IsStrict", "Submit_IsStrict", UseStrict);

	// If there are loop variables, destructively tokenize item and stuff the tokens into the submit hashtable.
	if ( ! vars.isEmpty())  {

		if ( ! item) {
			item = EmptyItemString;
			EmptyItemString[0] = '\0';
		}

		// set the first loop variable unconditionally, we set it initially to the whole item
		// we may later truncate that item when we assign fields to other loop variables.
		vars.rewind();
		char * var = vars.next();
		char * data = item;
		hash.set_live_submit_variable(var, data, false);

		// if there is more than a single loop variable, then assign them as well
		// we do this by destructively null terminating the item for each var
		// the last var gets all of the remaining item text (if any)
		while ((var = vars.next())) {
			// scan for next token separator
			while (*data && ! strchr(delims, *data)) ++data;
			// null terminate the previous token and advance to the start of the next token.
			if (*data) {
				*data++ = 0;
				// skip leading separators and whitespace
				while (*data && strchr(ws, *data)) ++data;
				hash.set_live_submit_variable(var, data, false);
			}
		}

		if (DashDryRun && DumpSubmitHash) {
			fprintf(stdout, "----- submit hash changes for ItemIndex = %d -----\n", item_index);
			char * var;
			vars.rewind();
			while ((var = vars.next())) {
				MACRO_ITEM* pitem = hash.lookup_exact(var);
				fprintf (stdout, "  %s = %s\n", var, pitem ? pitem->raw_value : "");
			}
			fprintf(stdout, "-----\n");
		}

		if (check_empty) {
			vars.rewind();
			std::string empties;
			while ((var = vars.next())) {
				MACRO_ITEM* pitem = hash.lookup_exact(var);
				if ( ! pitem || (pitem->raw_value != EmptyItemString && 0 == strlen(pitem->raw_value))) {
					if ( ! empties.empty()) empties += ",";
					empties += var;
				}
			}
			if ( ! empties.empty()) {
				fprintf (stderr, "\n%s: Empty value(s) for %s at ItemIndex=%d!", fail_empty ? "ERROR" : "WARNING", empties.c_str(), item_index);
				if (fail_empty) {
					return -4;
				}
			}
		}

	}

	return rval;
}

void cleanup_vars(SubmitHash & hash, StringList & vars)
{
	// set live submit variables to generate reasonable unused-item messages.
	if ( ! vars.isEmpty())  {
		vars.rewind();
		char * var;
		while ((var = vars.next())) { hash.set_live_submit_variable(var, "<Queue_item>", false); }
	}
}

MACRO_ITEM* find_submit_item(const char * name) { return submit_hash.lookup_exact(name); }
#define set_live_submit_variable submit_hash.set_live_submit_variable

int allocate_a_cluster()
{
	// if we have already created the maximum number of clusters, error out now.
	if (DashMaxClusters > 0 && ClustersCreated >= DashMaxClusters) {
		fprintf(stderr, "\nERROR: Number of submitted clusters would exceed %d and -single-cluster was specified\n", DashMaxClusters);
		exit(1);
	}

	CondorError errstack;
	if ((ClusterId = MyQ->get_NewCluster(errstack)) < 0) {
		const char * msg = "Failed to create cluster";
		if (errstack.message()) { msg = errstack.message(); }
		fprintf(stderr, "\nERROR: %s\n", msg);
		if ( ClusterId == NEWJOB_ERR_MAX_JOBS_SUBMITTED ) {
			fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
		} else if ( ClusterId == NEWJOB_ERR_MAX_JOBS_PER_OWNER ) {
			fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_PER_OWNER\n");
		} else if ( ClusterId == NEWJOB_ERR_DISABLED_USER ) {
			fprintf(stderr,
				"User is disabled\n");
		}
		exit(1);
	}

	++ClustersCreated;

	return 0;
}


int queue_begin(StringList & vars, bool new_cluster)
{
	if ( ! MyQ)
		return -1;

	if (new_cluster) {
		allocate_a_cluster();
	}

	init_vars(submit_hash, ClusterId, vars);

	if (DashDryRun && DumpSubmitHash) {
		fprintf(stdout, "\n----- submit hash at queue begin -----\n");
		submit_hash.dump(stdout, DumpSubmitHash & 0xF);
		fprintf(stdout, "-----\n");
	}

	return 0;
}

// queue N for a single item from the foreach itemlist.
// if there is no item list (i.e the old Queue N syntax) then item will be NULL.
//
int queue_item(int num, StringList & vars, char * item, int item_index, int options, const char * delims, const char * ws)
{
	ErrContext.item_index = item_index;

	if ( ! MyQ)
		return -1;

	int rval = set_vars(submit_hash, vars, item, item_index, options, delims, ws);
	if (rval < 0)
		return rval;

	/* queue num jobs */
	for (int ii = 0; ii < num; ++ii) {
	#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
	#else
		NoClusterCheckAttrs.clearAll();
	#endif
		ErrContext.step = ii;

		if ( ClusterId == -1 ) {
			fprintf( stderr, "\nERROR: Used queue command without "
						"specifying an executable\n" );
			exit(1);
		}

		// if we have already created the maximum number of jobs, error out now.
		if (DashMaxJobs > 0 && JobsCreated >= DashMaxJobs) {
			fprintf(stderr, "\nERROR: Number of submitted jobs would exceed -maxjobs (%d)\n", DashMaxJobs);
			DoCleanup(0,0,NULL);
			exit(1);
		}

		ProcId = MyQ->get_NewProc (ClusterId);

		if ( ProcId < 0 ) {
			fprintf(stderr, "\nERROR: Failed to create proc\n");
			if ( ProcId == NEWJOB_ERR_MAX_JOBS_SUBMITTED ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
			} else if( ProcId == NEWJOB_ERR_MAX_JOBS_PER_OWNER ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_PER_OWNER\n");
			} else if( ProcId == NEWJOB_ERR_MAX_JOBS_PER_SUBMISSION ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_PER_SUBMISSION\n");
			}
			DoCleanup(0,0,NULL);
			exit(1);
		}

		if (MaxProcsPerCluster && ProcId >= MaxProcsPerCluster) {
			fprintf(stderr, "\nERROR: number of procs exceeds SUBMIT_MAX_PROCS_IN_CLUSTER\n");
			DoCleanup(0,0,NULL);
			exit(-1);
		}

		// update the live $(Cluster), $(Process) and $(Step) string buffers
		(void)snprintf(ClusterString, sizeof(ClusterString), "%d", ClusterId);
		(void)snprintf(ProcessString, sizeof(ProcessString), "%d", ProcId);

		// we move this outside the above, otherwise it appears that we have 
		// received no queue command (or several, if there were multiple ones)
		GotNonEmptyQueueCommand = 1;
		JOB_ID_KEY jid(ClusterId,ProcId);

		ClassAd * job = submit_hash.make_job_ad(jid, item_index, ii, dash_interactive, dash_remote, check_sub_file, NULL);
		if ( ! job) {
			print_errstack(stderr, submit_hash.error_stack());
			DoCleanup(0,0,NULL);
			exit(1);
		}

		int JobUniverse = submit_hash.getUniverse();
		rval = 0;
		if ( ProcId == 0 ) {
			// if there are custom jobset attributes, and the schedd supports jobsets
			// send the jobset attributes now.
			rval = send_jobset_ad(submit_hash, jid.cluster);
			if (rval < 0) {
				DoCleanup(0,0,NULL);
				exit(1);
			}

			SendLastExecutable(); // if spooling the exe, send it now.

			// before sending proc0 ad, send the cluster ad
			classad::ClassAd * cad = job->GetChainedParentAd();
			if (cad) {
				rval = MySendJobAttributes(JOB_ID_KEY(jid.cluster, -1), *cad, setattrflags);
			}
		}
		NewExecutable = false;

		// now send the proc ad
		if (rval >= 0) {
			rval = MySendJobAttributes(jid, *job, setattrflags);
		}
		switch( rval ) {
		case 0:			/* Success */
		case 1:
			break;
		default:		/* Failed for some other reason... */
			fprintf( stderr, "\nERROR: Failed to queue job.\n" );
			exit(1);
		}

		++JobsCreated;
	
		if (verbose && ! DumpFileIsStdout) {
			fprintf(stdout, "\n** Proc %d.%d:\n", ClusterId, ProcId);
			fPrintAd (stdout, *job);
		} else if ( ! terse && ! DumpFileIsStdout && ! DumpSubmitHash) {
			fprintf(stdout, ".");
		}

		if (SubmitInfo.size() == 0 || SubmitInfo.back().cluster != ClusterId) {
			SubmitInfo.push_back(SubmitRec());
			SubmitInfo.back().cluster = ClusterId;
			SubmitInfo.back().firstjob = ProcId;
		}
		SubmitInfo.back().lastjob = ProcId;

		// SubmitInfo[x].lastjob controls how many submit events we
		// see in the user log.  For parallel jobs, we only want
		// one "job" submit event per cluster, no matter how many 
		// Procs are in that it.  Setting lastjob to zero makes this so.

		if (JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
			SubmitInfo.back().lastjob = 0;
		}

		// If spooling entire job "sandbox" to the schedd, then we need to keep
		// the classads in an array to later feed into the filetransfer object.
		if (dash_remote || (DashDryRun && DashDryRunFullAds)) {
			ClassAd * tmp = new ClassAd(*job);
			tmp->Assign(ATTR_CLUSTER_ID, ClusterId);
			tmp->Assign(ATTR_PROC_ID, ProcId);
			if (0 == ProcId) {
				JobAdsArrayLastClusterIndex = JobAdsArray.size();
				tmp->Unchain();
				tmp->UpdateFromChain(*job);
			} else {
				// proc ad to cluster ad (if there is one)
				tmp->ChainToAd(JobAdsArray[JobAdsArrayLastClusterIndex]);
			}
			JobAdsArray.push_back(tmp);
		}

		submit_hash.delete_job_ad();
		job = NULL;
	}

	ErrContext.step = -1;

	return rval;
}


#undef set_live_submit_variable


void
usage(FILE* out)
{
	fprintf( out, "Usage: %s [options] [<attrib>=<value>] [- | <submit-file>]\n", MyName );
	fprintf( out, "    [options] are\n" );
	fprintf( out, "\t-file <submit-file>\tRead Submit commands from <submit-file>\n");
	fprintf( out, "\t-terse  \t\tDisplay terse output, jobid ranges only\n" );
	fprintf( out, "\t-verbose\t\tDisplay verbose output, jobid and full job ClassAd\n" );
	fprintf( out, "\t-capabilities\t\tDisplay configured capabilities of the schedd\n" );
	fprintf( out, "\t-debug  \t\tDisplay debugging output\n" );
	fprintf( out, "\t-append <line>\t\tadd line to submit file before processing\n"
				  "\t              \t\t(overrides submit file; multiple -a lines ok)\n" );
	fprintf( out, "\t-queue <queue-opts>\tappend Queue statement to submit file before processing\n"
				  "\t                   \t(submit file must not already have a Queue statement)\n" );
	fprintf( out, "\t-batch-name <name>\tappend a line to submit file that sets the batch name\n"
					/* "\t                  \t(overrides batch_name in submit file)\n" */);
	fprintf( out, "\t-disable\t\tdisable file permission checks\n" );
	fprintf( out, "\t-dry-run <filename>\tprocess submit file and write ClassAd attributes to <filename>\n"
				  "\t        \t\tbut do not actually submit the job(s) to the SCHEDD\n" );
	fprintf( out, "\t-maxjobs <maxjobs>\tDo not submit if number of jobs would exceed <maxjobs>.\n" );
	fprintf( out, "\t-single-cluster\t\tDo not submit if more than one ClusterId is needed.\n" );
	fprintf( out, "\t-unused\t\t\ttoggles unused or unexpanded macro warnings\n"
					 "\t       \t\t\t(overrides config file; multiple -u flags ok)\n" );
	//fprintf( stderr, "\t-force-mpi-universe\tAllow submission of obsolete MPI universe\n );
	fprintf( out, "\t-allow-crlf-script\tAllow submitting #! executable script with DOS/CRLF line endings\n" );
	fprintf( out, "\t-dump <filename>\tWrite job ClassAds to <filename> instead of\n"
					 "\t                \tsubmitting to a schedd.\n" );
#if !defined(WIN32)
	fprintf( out, "\t-interactive\t\tsubmit an interactive session job\n" );
#endif
	fprintf( out, "\t-factory\t\tSubmit a late materialization job factory\n");
	fprintf( out, "\t-name <name>\t\tsubmit to the specified schedd\n" );
	fprintf( out, "\t-remote <name>\t\tsubmit to the specified remote schedd\n"
					 "\t              \t\t(implies -spool)\n" );
    fprintf( out, "\t-addr <ip:port>\t\tsubmit to schedd at given \"sinful string\"\n" );
	fprintf( out, "\t-spool\t\t\tspool all files to the schedd\n" );
	fprintf( out, "\t-pool <host>\t\tUse host as the central manager to query\n" );
	fprintf( out, "\t<attrib>=<value>\tSet <attrib>=<value> before reading the submit file.\n" );

	fprintf( out, "\n    If <submit-file> is omitted or is -, and a -queue is not provided, submit commands\n"
				  "     are read from stdin. Use of - implies verbose output unless -terse is specified\n");
}


extern "C" {
int
DoCleanup(int,int,const char*)
{
	if( JobsCreated ) 
	{
		// TerminateCluster() may call EXCEPT() which in turn calls 
		// DoCleanup().  This lead to infinite recursion which is bad.
		JobsCreated = 0;
		if ( ! ActiveQueueConnection) {
			if (DumpClassAdToFile) {
			} else {
				CondorError errstack;
				ActualScheddQ *ScheddQ = (ActualScheddQ *)MyQ;
				if ( ! ScheddQ) { MyQ = ScheddQ = new ActualScheddQ(); }
				ActiveQueueConnection = ScheddQ->Connect(*MySchedd, errstack) != 0;
			}
		}
		if (ActiveQueueConnection && MyQ) {
			// Call DestroyCluster() now in an attempt to get the schedd
			// to unlink the initial checkpoint file (i.e. remove the 
			// executable we sent earlier from the SPOOL directory).
			MyQ->destroy_Cluster( ClusterId );

			// We purposefully do _not_ call DisconnectQ() here, since 
			// we do not want the current transaction to be committed.
		}
	}

	if (username) {
		free(username);
		username = NULL;
	}

	return 0;		// For historical reasons...
}
} /* extern "C" */


void
init_params()
{
	const char * err = init_submit_default_macros();
	if (err) {
		fprintf(stderr, "%s\n", err);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	My_fs_domain = param( "FILESYSTEM_DOMAIN" );
		// Will always return something, since config() will put in a
		// value (full hostname) if it's not in the config file.


	WarnOnUnusedMacros =
		param_boolean_crufty("WARN_ON_UNUSED_SUBMIT_FILE_MACROS",
							 WarnOnUnusedMacros ? true : false) ? 1 : 0;

	if ( param_boolean("SUBMIT_NOACK_ON_SETATTRIBUTE",true) ) {
		setattrflags |= SetAttribute_NoAck;  // set noack flag
	} else {
		setattrflags &= ~SetAttribute_NoAck; // clear noack flag
	}

	protectedUrlMap = getProtectedURLMap();
}


int SendLastExecutable()
{
	const char * ename = LastExecutable.empty() ? NULL : LastExecutable.c_str();
	bool copy_to_spool = SpoolLastExecutable;
	std::string SpoolEname(ename ? ename : "");

	// spool executable if necessary
	if ( ename && copy_to_spool ) {

		char * chkptname = GetSpooledExecutablePath(submit_hash.getClusterId(), "");
		SpoolEname = chkptname;
		free(chkptname);
		int ret = MyQ->send_SpoolFile(SpoolEname.c_str());

		if (ret < 0) {
			fprintf( stderr,
						"\nERROR: Request to transfer executable %s failed\n",
						SpoolEname.c_str() );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}

		// ret will be 0 if the SchedD gave us the go-ahead to send
		// the file. if it's not, the SchedD is using ickpt sharing
		// and already has a copy, so no need
		//
		if ((ret == 0) && MyQ->send_SpoolFileBytes(submit_hash.full_path(ename,false)) < 0) {
			fprintf( stderr,
						"\nERROR: failed to transfer executable file %s\n", 
						ename );
			DoCleanup(0,0,NULL);
			exit( 1 );
		}
	}
	return 0;
}


// hack! for 8.7.8 testing.
int attr_chain_depth = 0;

// struct for a table mapping attribute names to a flag indicating that the attribute
// must only be in cluster ad or only in proc ad, or can be either.
//
typedef struct attr_force_pair {
	const char * key;
	int          forced; // -1=forced cluster, 0=not forced, 1=forced proc
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const struct attr_force_pair& rhs) const {
		return strcasecmp(this->key, rhs.key) < 0;
	}
} ATTR_FORCE_PAIR;

// table defineing attributes that must be in either cluster ad or proc ad
// force value of 1 is proc ad, -1 is cluster ad, 2 is proc ad, and sent first, -2 is cluster ad and sent first
// NOTE: this table MUST be sorted by case-insensitive attribute name
#define FILL(attr,force) { attr, force }
static const ATTR_FORCE_PAIR aForcedSetAttrs[] = {
	FILL(ATTR_CLUSTER_ID,         -2), // forced into cluster ad
	FILL(ATTR_JOB_SET_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_NAME,       -1), // forced into cluster ad
	FILL(ATTR_JOB_STATUS,         2),  // forced into proc ad (because job counters don't work unless this is set to IDLE/HELD on startup)
	FILL(ATTR_JOB_UNIVERSE,       -1), // forced into cluster ad
	FILL(ATTR_OWNER,              -1), // forced into cluster ad
	FILL(ATTR_PROC_ID,            2),  // forced into proc ad
};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
static int IsForcedProcAttribute(const char *attr)
{
	const ATTR_FORCE_PAIR* found = NULL;
	found = BinaryLookup<ATTR_FORCE_PAIR>(
		aForcedSetAttrs,
		COUNTOF(aForcedSetAttrs),
		attr, strcasecmp);
	if (found) {
		return found->forced;
	}
	return 0;
}


// we have our own private implementation of SendAdAttributes because we use the abstract schedd queue (for -dry and -dump)
static int MySendJobAttributes(const JOB_ID_KEY & key, const classad::ClassAd & ad, SetAttributeFlags_t saflags)
{
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );
	std::string rhs; rhs.reserve(120);

	std::string keybuf;
	key.sprint(keybuf);
	const char * keystr = keybuf.c_str();

	int retval = 0;
	bool is_cluster = key.proc < 0;

	// first try and send the cluster id or proc id
	if (is_cluster) {
		if (MyQ->set_AttributeInt (key.cluster, -1, ATTR_CLUSTER_ID, key.cluster, saflags) == -1) {
			if (saflags & SetAttribute_NoAck) {
				fprintf( stderr, "\nERROR: Failed submission for job %s - aborting entire submit\n", keystr);
			} else {
				fprintf( stderr, "\nERROR: Failed to set " ATTR_CLUSTER_ID "=%d for job %s (%d)\n", key.cluster, keystr, errno);
			}
			return -1;
		}
	} else {
		if (MyQ->set_AttributeInt (key.cluster, key.proc, ATTR_PROC_ID, key.proc, saflags) == -1) {
			if (saflags & SetAttribute_NoAck) {
				fprintf( stderr, "\nERROR: Failed submission for job %s - aborting entire submit\n", keystr);
			} else {
				fprintf( stderr, "\nERROR: Failed to set " ATTR_PROC_ID "=%d for job %s (%d)\n", key.proc, keystr, errno);
			}
			return -1;
		}

		// For now, we make sure to set the JobStatus attribute in the proc ad, note that we may actually be
		// fetching this from a chained parent ad.  this is the ONLY attribute that we want to pick up
		// out of the chained parent and push into the child.
		// we force this into the proc ad because the code in the schedd that calculates per-cluster
		// and per-owner totals by state doesn't work if this attribute is missing in the proc ads.
		int status = IDLE;
		if ( ! ad.EvaluateAttrInt(ATTR_JOB_STATUS, status)) { status = IDLE; }
		if (MyQ->set_AttributeInt (key.cluster, key.proc, ATTR_JOB_STATUS, status, saflags) == -1) {
			if (saflags & SetAttribute_NoAck) {
				fprintf( stderr, "\nERROR: Failed submission for job %s - aborting entire submit\n", keystr);
			} else {
				fprintf( stderr, "\nERROR: Failed to set " ATTR_JOB_STATUS "=%d for job %s (%d)\n", status, keystr, errno);
			}
			return -1;
		}
	}

	// (shallow) iterate the attributes in this ad and send them to the schedd
	//
	for (auto it = ad.begin(); it != ad.end(); ++it) {
		const char * attr = it->first.c_str();

		// skip attributes that are forced into the other sort of ad, or have already been sent.
		int forced = IsForcedProcAttribute(attr);
		if (forced) {
			// skip attributes not forced into the cluster ad and not already sent
			if (is_cluster && (forced != -1)) continue;
			// skip attributes not forced into the proc ad and not already sent
			if ( ! is_cluster && (forced != 1)) continue;
		}

		if ( ! it->second) {
			fprintf(stderr, "\nERROR: Null attribute name or value for job %s\n", keystr);
			retval = -1;
			break;
		}
		rhs.clear();
		unparser.Unparse(rhs, it->second);

		if (MyQ->set_Attribute(key.cluster, key.proc, attr, rhs.c_str(), saflags) == -1) {
			if (saflags & SetAttribute_NoAck) {
				fprintf( stderr, "\nERROR: Failed submission for job %s - aborting entire submit\n", keystr);
			} else {
				fprintf( stderr, "\nERROR: Failed to set %s=%s for job %s (%d)\n", attr, rhs.c_str(), keystr, errno );
			}
			retval = -1;
			break;
		}
	}

	return retval;
}


// If our effective and real gids are different (b/c the submit binary
// is setgid) set umask(002) so that stdout, stderr and the user log
// files are created writable by group condor.  This way, people who
// run Condor as condor can still have the right permissions on their
// files for the default case of their job doesn't open its own files.
void
check_umask()
{
#ifndef WIN32
	if( getgid() != getegid() ) {
		umask( 002 );
	}
#endif
}

void 
setupAuthentication()
{
		//RendezvousDir for remote FS auth can be specified in submit file.
	char *Rendezvous = NULL;
	Rendezvous = submit_hash.submit_param( SUBMIT_KEY_RendezvousDir, "rendezvous_dir" );
	if( ! Rendezvous ) {
			// If those didn't work, try a few other variations, just
			// to be safe:
		Rendezvous = submit_hash.submit_param( "rendezvousdirectory", "rendezvous_directory" );
	}
	if( Rendezvous ) {
		dprintf( D_FULLDEBUG,"setting RENDEZVOUS_DIRECTORY=%s\n", Rendezvous );
			//SetEnv because Authentication::authenticate() expects them there.
		SetEnv(ENV_RENDEZVOUS_DIRECTORY, Rendezvous);
		free( Rendezvous );
	}
}


// PRAGMA_REMIND("make a proper queue args unit test.")
//
// The old pre-submit_utils unit tests should be a good refernce,
// and can be pulled out of git's history of this file from the hash
// d7f3ffdd8fac1316344cdf30eeb65b95b355da99 and earlier.
int DoUnitTests(int options)
{
	return (options > 1) ? 1 : 0;
}
