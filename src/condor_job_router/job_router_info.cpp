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
//#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "condor_distribution.h"
#include "write_user_log.h"
#include "dprintf_internal.h" // for dprintf_set_outputs
#include "condor_version.h"

#include "Scheduler.h"
#include "JobRouter.h"
#include "submit_job.h"

#ifdef __GNUC__
#if __GNUC__ >= 4
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#endif


//JobRouter *job_router = NULL;

//-------------------------------------------------------------
const char * MyName = NULL;
classad::ClassAdCollection * g_jobs = NULL;
FILE * submitted_jobs_fh = NULL;

static bool read_classad_file(const char *filename, classad::ClassAdCollection &classads, const char * constr);

void PREFAST_NORETURN
my_exit( int status )
{
	fflush(stdout);
	fflush(stderr);
	exit(status);
}

void
usage(int retval = 1)
{
	fprintf(stderr, "Usage: %s [options]\n", MyName);
	fprintf(stderr,
		"    where [options] is one or more of:\n"
		"\t-help\t\tPrint this screen and exit\n"
		"\t-version\tPrint HTCondor version and exit\n"
		"\t-config\t\tPrint configured routes\n"
		"\t-match-jobs\tMatch jobs to routes and print the first match\n"
		"\t-route-jobs <file>\tMatch jobs to routes and print the routed jobs to the file\n"
		"\t-log-steps\tLog the routing steps as jobs are routed. Use with -route-jobs\n"
		"\t-ignore-prior-routing\tRemove routing attributes from the job ClassAd and set JobStatus to IDLE before matching\n"
		"\t-jobads <file>\tWhen operation requires job ClassAds, Read them from <file>\n\t\t\tIf <file> is -, read from stdin\n"
		"\n"
		);
	my_exit(retval);
}

static const char * use_next_arg(const char * arg, const char * argv[], int & i)
{
	if (argv[i+1]) {
		const char * p = argv[i + 1];
		if (*p != '-' || MATCH == strcmp(p, "-") || MATCH == strcmp(p, "-2")) {
			return argv[++i];
		}
	}

	fprintf(stderr, "-%s requires an argument\n", arg);
	//usage(1);
	my_exit(1);
	return NULL;
}

static StringList saved_dprintfs;
static void print_saved_dprintfs(FILE* hf)
{
	saved_dprintfs.rewind();
	const char * line;
	while ((line = saved_dprintfs.next())) {
		fprintf(hf, "%s", line);
	}
	saved_dprintfs.clearAll();
}


bool g_silence_dprintf = false;
bool g_save_dprintfs = false;
void _dprintf_intercept(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo)
{
	//if (cat_and_flags & D_FULLDEBUG) return;
	if (g_silence_dprintf) {
		if (g_save_dprintfs) {
			saved_dprintfs.append(message);
		}
		return;
	}
	if (is_arg_prefix("JobRouter", message, 9)) { message += 9; if (*message == ':') ++message; if (*message == ' ') ++message; }
	int cch = strlen(message);
	fprintf(stdout, &"\n%s"[(cch > 150) ? 0 : 1], message);
}

static void dprintf_set_output_intercept (
	int cat_and_flags,
	DebugOutputChoice choice,
	DprintfFuncPtr fn)
{

	dprintf_output_settings my_output;
	my_output.choice = choice;
	my_output.accepts_all = true;
	my_output.logPath = ">BUFFER";	// this is a special case of intercept
	my_output.HeaderOpts = (cat_and_flags & ~(D_CATEGORY_RESERVED_MASK | D_FULLDEBUG | D_VERBOSE_MASK));
	my_output.VerboseCats = (cat_and_flags & (D_FULLDEBUG | D_VERBOSE_MASK)) ? choice : 0;
	dprintf_set_outputs(&my_output, 1);

	// throw away any dprintf messages up to this point.
	bool was_silent = g_silence_dprintf;
	g_silence_dprintf = true;
	dprintf_WriteOnErrorBuffer(NULL, true);
	g_silence_dprintf = was_silent;

	// PRAGMA_REMIND("tj: fix this hack when the dprintf code has a proper way to register an intercept.")
	// HACK!!! there is no properly exposed way to set an intercept function, so for now, we reach into
	// the dprintf internal data structures and just set one. 
	extern std::vector<DebugFileInfo> * DebugLogs;
	if (DebugLogs) { (*DebugLogs)[0].dprintfFunc = fn; }
}


int main(int argc, const char *argv[])
{
	MyName = argv[0];
	set_priv_initialize();
	set_mySubSystem("TOOL", true, SUBSYSTEM_TYPE_TOOL);
	config();

	StringList bare_args;
	StringList job_files;
	bool dash_config = false;
	bool dash_match_jobs = false;
	bool dash_route_jobs = false;
	bool dash_log_route_steps = false;
	int  dash_diagnostic = 0;
	bool dash_ignore_prior_routing = false;
	//bool dash_d_fulldebug = false;
	const char * route_jobs_filename = NULL;

	g_jobs = new classad::ClassAdCollection();

	for (int i = 1; i < argc; ++i) {

		const char * pcolon = NULL;
		if (is_dash_arg_prefix(argv[i], "help", 1)) {
			usage(0);
		} else if (is_dash_arg_prefix(argv[i], "version", 1)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			my_exit(0);
		} else if (is_dash_arg_colon_prefix(argv[i], "debug", &pcolon, 1)) {
			if (pcolon && (is_arg_prefix(pcolon+1, "verbose", 1) || is_arg_prefix(pcolon+1, "full", 1))) {
				//dash_d_fulldebug = true;
			}
		} else if (is_dash_arg_colon_prefix(argv[i], "diagnostic", &pcolon, 4)) {
			dash_diagnostic = JOB_ROUTER_TOOL_FLAG_DIAGNOSTIC;
			if (pcolon) {
				StringTokenIterator it(++pcolon,40,",");
				for (const char * opt = it.first(); opt; opt = it.next()) {
					if (is_arg_prefix(opt, "match")) {
						dash_diagnostic |= JOB_ROUTER_TOOL_FLAG_DEBUG_UMBRELLA;
					}
				}
			}
		} else if (is_dash_arg_prefix(argv[i], "config", 2)) {
			dash_config = true;
		} else if (is_dash_arg_prefix(argv[i], "match-jobs", 2)) {
			dash_match_jobs = true;
		} else if (is_dash_arg_prefix(argv[i], "route-jobs", 5)) {
			dash_match_jobs = true;
			dash_route_jobs = true;
			route_jobs_filename = use_next_arg("route-jobs", argv, i);
		} else if (is_dash_arg_prefix(argv[i], "log-steps", 2)) {
			dash_log_route_steps = true;
		} else if (is_dash_arg_prefix(argv[i], "ignore-prior-routing", 2)) {
			dash_ignore_prior_routing = true;
		} else if (is_dash_arg_prefix(argv[i], "jobads", 1)) {
			const char * filename = use_next_arg("jobads", argv, i);
			job_files.append(filename);
		} else if (*argv[i] != '-') {
			// arguments that don't begin with "-" are bare arguments
			bare_args.append(argv[i]);
			continue;
		} else {
			fprintf(stderr, "ERROR: %s is not a valid argument\n", argv[i]);
			usage(1);
		}
	}


	// tell the dprintf code to had messages to our callback function.
	unsigned int cat_and_flags = D_FULLDEBUG | D_CAT;
	//if (dash_d_fulldebug) { cat_and_flags |= D_FULLDEBUG; }
	DebugOutputChoice choice=1<<D_ERROR;
	if (dash_diagnostic || dash_log_route_steps) { choice |= (1<<D_ALWAYS)|(1<<D_STATUS); }
	dprintf_set_output_intercept(cat_and_flags, choice, _dprintf_intercept);


	// before we call init() for the router, we need to install a pseudo-schedd object
	// so that init() doesn't install a real schedd object.
	Scheduler* schedd = new Scheduler(0);

	g_silence_dprintf = dash_diagnostic ? false : true;
	g_save_dprintfs = true;
	unsigned int tool_flags = JOB_ROUTER_TOOL_FLAG_AS_TOOL | JOB_ROUTER_TOOL_FLAG_LOG_XFORM_ERRORS;
	// assume job router will be running as root even if we (the tool) are not.
	tool_flags |= JOB_ROUTER_TOOL_FLAG_CAN_SWITCH_IDS;
	if (dash_diagnostic) {
		tool_flags |= dash_diagnostic;
	}
	if (dash_log_route_steps) {
		tool_flags |= JOB_ROUTER_TOOL_FLAG_LOG_XFORM_STEPS;
	}
	JobRouter job_router(tool_flags);
	job_router.set_schedds(schedd, nullptr);
	job_router.init();
	g_silence_dprintf = false;
	g_save_dprintfs = false;

	// if the job router is not enabled at this point, say so, and print out saved dprintfs.
	if ( ! job_router.isEnabled()) {
		print_saved_dprintfs(stderr);
		fprintf(stderr, "JobRouter is disabled.\n");
	}

	if (dash_config) {
		fprintf (stdout, "\n\n");
		job_router.dump_routes(stdout);
	}

	if ( ! job_files.isEmpty()) {
		job_files.rewind();
		const char * filename;
		while ((filename = job_files.next())) {
			read_classad_file(filename, *g_jobs, NULL);
		}
	}

	if (dash_ignore_prior_routing) {
		// strip attributes that indicate that the job has already been routed
		std::string key;
		classad::LocalCollectionQuery query;
		query.Bind(g_jobs);
		query.Query("root");
		query.ToFirst();
		if (query.Current(key)) do {
			classad::ClassAd *ad = g_jobs->GetClassAd(key);
			ad->Delete("Managed");
			ad->Delete("RoutedBy");
			ad->Delete("StageInStart");
			ad->Delete("StageInFinish");
			ad->InsertAttr("JobStatus", 1); // pretend job is idle so that it will route.
		} while (query.Next(key));
	}

	if (dash_match_jobs) {
		fprintf(stdout, "\nMatching jobs against routes to find candidate jobs.\n");
		const classad::View *root_view = g_jobs->GetView("root");
		if (root_view && (root_view->begin() != root_view->end())) {
			job_router.GetCandidateJobs();
			if (dash_route_jobs) {
				bool close_file = true;
				if (*route_jobs_filename == '-') {
					close_file = false;
					if (route_jobs_filename[1]=='2') {
						submitted_jobs_fh = stderr;
					} else {
						submitted_jobs_fh = stdout;
					}
				} else {
					submitted_jobs_fh = safe_fopen_wrapper_follow(route_jobs_filename, "wb");
					if ( ! submitted_jobs_fh) {
						fprintf(stderr, "could not open %s\n", route_jobs_filename);
						my_exit(1);
					}
				}
				// now claim and route the jobs
				job_router.SimulateRouting();
				// close the 
				if (close_file) {
					fclose(submitted_jobs_fh);
				}
				submitted_jobs_fh = NULL;
			}
		} else {
			fprintf(stdout, "There are no jobs to match\n");
		}
	}

	return 0;
}

class CondorQClassAdFileParseHelper : public ClassAdFileParseHelper
{
 public:
	virtual int PreParse(std::string & line, classad::ClassAd & ad, FILE* file);
	virtual int OnParseError(std::string & line, classad::ClassAd & ad, FILE* file);
	// return non-zero if new parser, o if old (line oriented) parser, non-zero is returned the above functions will never be called.
	virtual int NewParser(classad::ClassAd & /*ad*/, FILE* /*file*/, bool & detected_long, std::string & /*errmsg*/) { detected_long = false; return 0; }
	std::string schedd_name;
	std::string schedd_addr;
};

// this method is called before each line is parsed. 
// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
int CondorQClassAdFileParseHelper::PreParse(std::string & line, classad::ClassAd & /*ad*/, FILE* /*file*/)
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
		if (line[ix] == '#' || line[ix] == '\n')
			return 0; // skip this line, but don't stop parsing.
		if (line[ix] != ' ' && line[ix] != '\t')
			break;
	}
	return 1; // parse this line
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
		ee = this->PreParse(line, ad, file);
	}
	return ee;
}

// -----------------------------
static bool read_classad_file(const char *filename, classad::ClassAdCollection &classads, const char * constr)
{
	bool success = false;

	FILE* file = NULL;
	bool  read_from_stdin = false;
	if (MATCH == strcmp(filename, "-")) {
		read_from_stdin = true;
		file = stdin;
	} else {
		file = safe_fopen_wrapper_follow(filename, "r");
	}
	if (file == NULL) {
		fprintf(stderr, "Can't open file of job ads: %s\n", filename);
		return false;
	} else {
		// this helps us parse the output of condor_q -long
		CondorQClassAdFileParseHelper parse_helper;

		for (;;) {
			ClassAd* classad = new ClassAd();

			int error;
			bool is_eof;
			int cAttrs = InsertFromFile(file, *classad, is_eof, error, &parse_helper);

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
				int cluster, proc = -1;
				if (classad->LookupInteger(ATTR_CLUSTER_ID, cluster) && classad->LookupInteger(ATTR_PROC_ID, proc)) {
					std::string key;
					formatstr(key, "%d.%d", cluster, proc);
					if (classads.AddClassAd(key, classad)) {
						classad = NULL; // this is now owned by the collection.
					}
				} else {
					fprintf(stderr, "Skipping ad because it doesn't have a ClusterId and/or ProcId attribute\n");
				}
			}
			if (classad) {
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

		if ( ! read_from_stdin) { fclose(file); }
	}
	return success;
}


// this is how we will feed job ads into the router
//
class JobLogMirror {
public:
	JobLogMirror(char const *job_queue=NULL) {}
	~JobLogMirror() {}

	void init() {}
	void config() {}
	void stop() {}

private:
};

Scheduler::Scheduler(int id /*=0*/)
	: m_consumer(NULL)
	, m_mirror(NULL)
	, m_follow_log(id ? NULL : "job_router_info")
	, m_id(id)
{
}

Scheduler::~Scheduler()
{
	delete m_mirror;
	m_mirror = NULL;
	m_consumer = NULL;
}

classad::ClassAdCollection *Scheduler::GetClassAds() const
{
	if (m_id == 0) {
		return g_jobs;
	}
	return NULL;
}

void Scheduler::init() {  if (m_mirror) m_mirror->init(); }
void Scheduler::config() { if (m_mirror) m_mirror->config(); }
void Scheduler::stop()  { if (m_mirror) m_mirror->stop(); }
void Scheduler::poll()  { }


// 
JobRouterHookMgr::JobRouterHookMgr() : HookClientMgr(), m_warn_cleanup(false), m_warn_update(false), m_warn_translate(false), m_warn_exit(false),NUM_HOOKS(0), UNDEFINED("UNDEFINED"), m_default_hook_keyword(NULL), m_hook_paths(hashFunction) {}
JobRouterHookMgr::~JobRouterHookMgr() {};
bool JobRouterHookMgr::initialize() { reconfig(); return true; /*HookClientMgr::initialize()*/; }
bool JobRouterHookMgr::reconfig() { m_default_hook_keyword = param("JOB_ROUTER_HOOK_KEYWORD"); return true; }

int JobRouterHookMgr::hookTranslateJob(RoutedJob* r_job, std::string &route_info) { return 0; }
int JobRouterHookMgr::hookUpdateJobInfo(RoutedJob* r_job) { return 0; }
int JobRouterHookMgr::hookJobExit(RoutedJob* r_job) { return 0; }
int JobRouterHookMgr::hookJobCleanup(RoutedJob* r_job) { return 0; }

std::string
JobRouterHookMgr::getHookKeyword(const classad::ClassAd &ad)
{
	std::string hook_keyword;

	if (false == ad.EvaluateAttrString(ATTR_HOOK_KEYWORD, hook_keyword))
	{
		if ( m_default_hook_keyword ) {
			hook_keyword = m_default_hook_keyword;
		}
	}
	return hook_keyword;
}




ClaimJobResult claim_job(int cluster, int proc, std::string * error_details, const char * my_identity)
{
	return CJR_OK;
}



ClaimJobResult claim_job(classad::ClassAd const &ad, const char * pool_name, const char * schedd_name, int cluster, int proc, std::string * error_details, const char * my_identity, bool target_is_sandboxed)
{
	classad::ClassAd * job = const_cast<classad::ClassAd*>(&ad);
	job->InsertAttr(ATTR_JOB_MANAGED, MANAGED_EXTERNAL);
	if (my_identity) {
		job->InsertAttr(ATTR_JOB_MANAGED_MANAGER, my_identity);
	}
	return CJR_OK;
}

bool yield_job(bool done, int cluster, int proc, classad::ClassAd const &job_ad, std::string * error_details, const char * my_identity, bool target_is_sandboxed, bool release_on_hold, bool *keep_trying) {
	return true;
}


bool yield_job(classad::ClassAd const &ad,const char * pool_name,
	const char * schedd_name, bool done, int cluster, int proc,
	std::string * error_details, const char * my_identity, bool target_is_sandboxed,
        bool release_on_hold, bool *keep_trying)
{
	return true;
}

bool submit_job( const std::string & owner, const std::string & domain, ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	fprintf(stdout, "submit_job as %s@%s to %s pool:%s%s:\n", owner.c_str(), domain.c_str(),
		schedd_name ? schedd_name : "local",
		pool_name ? pool_name : "local",
		is_sandboxed ? " (sandboxed)" : "");
	if (submitted_jobs_fh) {
		fPrintAd(submitted_jobs_fh, src);
		fprintf(submitted_jobs_fh, "\n");
	}
	return true;
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_dirty_attributes(classad::ClassAd & src)
{
	return true;
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Establishes (and tears down) a qmgr connection.
*/
bool push_dirty_attributes(classad::ClassAd & src, const char * schedd_name, const char * pool_name)
{
	return true;
}

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_classad_diff(classad::ClassAd & src,classad::ClassAd & dest)
{
	return true;
}

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
	Establishes (and tears down) a qmgr connection.
*/
bool push_classad_diff(classad::ClassAd & src, classad::ClassAd & dest, const char * schedd_name, const char * pool_name)
{
	return true;
}

bool finalize_job(const std::string &owner, const std::string &domain, classad::ClassAd const &ad,int cluster, int proc, const char * schedd_name, const char * pool_name, bool is_sandboxed)
{
	return true;
}

bool remove_job(classad::ClassAd const &ad, int cluster, int proc, char const *reason, const char * schedd_name, const char * pool_name, std::string &error_desc)
{
	return true;
}

bool InitializeAbortedEvent( JobAbortedEvent *event, classad::ClassAd const &job_ad )
{
	return true;
}

bool InitializeTerminateEvent( TerminatedEvent *event, classad::ClassAd const &job_ad )
{
	return true;
}

bool InitializeHoldEvent( JobHeldEvent *event, classad::ClassAd const &job_ad )
{
	return true;
}

bool WriteEventToUserLog( ULogEvent const &event, classad::ClassAd const &ad )
{
	return true;
}

bool WriteTerminateEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}

bool WriteAbortEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}

bool WriteHoldEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}

bool WriteExecuteEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}

bool WriteEvictEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}

// The following is copied from gridmanager/basejob.C
// TODO: put the code into a shared file.

void
EmailTerminateEvent(ClassAd * job_ad, bool   /*exit_status_known*/)
{
}

bool EmailTerminateEvent( classad::ClassAd const &ad )
{
	return true;
}
