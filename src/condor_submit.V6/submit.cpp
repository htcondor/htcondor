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
#include "condor_network.h"
#include "condor_string.h"
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
//#include "condor_qmgr.h"  // only submit_protocol.cpp is allowed to access the schedd's Qmgt functions
#include "sig_install.h"
#include "access.h"
#include "daemon.h"
#include "match_prefix.h"

#include "extArray.h"
#include "HashTable.h"
#include "MyString.h"
#include "string_list.h"
#include "which.h"
#include "sig_name.h"
#include "print_wrapped_text.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "my_username.h"
#include "globus_utils.h"
#include "enum_utils.h"
#include "setenv.h"
#include "classad_hashtable.h"
#include "directory.h"
#include "filename_tools.h"
#include "fs_util.h"
#include "dc_transferd.h"
#include "condor_ftp.h"
#include "condor_crontab.h"
#include <scheduler.h>
#include "condor_holdcodes.h"
#include "condor_url.h"
#include "condor_version.h"
#include "NegotiationUtils.h"
#include <submit_utils.h>
//uncomment this to have condor_submit use the new for 8.5 submit_utils classes
#define USE_SUBMIT_UTILS 1
#include "submit_internal.h"

#include "list.h"
#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "condor_md.h"
#include "my_popen.h"
#include "zkm_base64.h"

#include <algorithm>
#include <string>
#include <set>

#ifndef USE_SUBMIT_UTILS
#error "condor submit must use submit utils"
#endif
// the halting parser doesn't work when Queue statements are in an submit file include : statement
//#define USE_HALTING_PARSER 1


// TODO: hashFunction() is case-insenstive, but when a MyString is the
//   hash key, the comparison in HashTable is case-sensitive. Therefore,
//   the case-insensitivity of hashFunction() doesn't complish anything.
//   CheckFilesRead and CheckFilesWrite should be
//   either completely case-sensitive (and use MyStringHash()) or
//   completely case-insensitive (and use AttrKey and AttrKeyHashFunction).
static unsigned int hashFunction( const MyString& );
#include "file_sql.h"

HashTable<MyString,int> CheckFilesRead( 577, hashFunction ); 
HashTable<MyString,int> CheckFilesWrite( 577, hashFunction ); 

#ifdef PLUS_ATTRIBS_IN_CLUSTER_AD
#else
StringList NoClusterCheckAttrs;
#endif
ClassAd *gClusterAd = NULL;

time_t submit_time = 0;

// Explicit template instantiation


char	*ScheddName = NULL;
std::string ScheddAddr;
AbstractScheddQ * MyQ = NULL;
char	*PoolName = NULL;
DCSchedd* MySchedd = NULL;

char	*My_fs_domain;

int		ExtraLineNo;
int		GotQueueCommand = 0;
int		GotNonEmptyQueueCommand = 0;
SandboxTransferMethod	STMethod = STM_USE_SCHEDD_ONLY;


const char	*MyName;
int 	dash_interactive = 0; /* true if job submitted with -interactive flag */
int 	InteractiveSubmitFile = 0; /* true if using INTERACTIVE_SUBMIT_FILE */
bool	verbose = false; // formerly: int Quiet = 1;
bool	terse = false; // generate parsable output
SetAttributeFlags_t setattrflags = 0; // flags to SetAttribute()
bool	SubmitFromStdin = false;
int		WarnOnUnusedMacros = 1;
int		DisableFileChecks = 0;
int		DashDryRun = 0;
int		DashMaxJobs = 0;	 // maximum number of jobs to create before generating an error
int		DashMaxClusters = 0; // maximum number of clusters to create before generating an error.
const char * DashDryRunOutName = NULL;
int		DumpSubmitHash = 0;
int		MaxProcsPerCluster;
int	  ClusterId = -1;
int	  ProcId = -1;
int		ClustersCreated = 0;
int		JobsCreated = 0;
int		ActiveQueueConnection = FALSE;
bool    nice_user_setting = false;
bool	NewExecutable = false;
int		dash_remote=0;
int		dash_factory=0;
int		default_to_factory=0;
unsigned int submit_unique_id=1; // hack to make default_to_factory work in test suite for milestone 1 (8.7.1)
#if defined(WIN32)
char* RunAsOwnerCredD = NULL;
#endif
char * batch_name_line = NULL;
bool sent_credential_to_credd = false;

// For mpi universe testing
bool use_condor_mpi_universe = false;

MyString LastExecutable; // used to pass executable between the check_file callback and SendExecutableb
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
List<const char> extraLines;  // lines passed in via -a argument
std::string queueCommandLine; // queue statement passed in via -q argument

SubmitHash submit_hash;

// these are used to keep track of the source of various macros in the table.
const MACRO_SOURCE DefaultMacro = { true, false, 1, -2, -1, -2 }; // for macros set by default
const MACRO_SOURCE ArgumentMacro = { true, false, 2, -2, -1, -2 }; // for macros set by command line
const MACRO_SOURCE LiveMacro = { true, false, 3, -2, -1, -2 };    // for macros use as queue loop variables
MACRO_SOURCE FileMacroSource = { false, false, 0, 0, -1, -2 };

#define MEG	(1<<20)


//
// Dump ClassAd to file junk
//
const char * DumpFileName = NULL;
bool		DumpClassAdToFile = false;
FILE		*DumpFile = NULL;
bool		DumpFileIsStdout = 0;

void usage();
void init_params();
void reschedule();
int  read_submit_file( FILE *fp );
void check_umask();
void setupAuthentication();
const char * is_queue_statement(const char * line); // return ptr to queue args of this is a queue statement
bool IsNoClusterAttr(const char * name);
int  check_sub_file(void*pv, SubmitHash * sub, _submit_file_role role, const char * name, int flags);
int  SendLastExecutable();
int  SendJobAd (ClassAd * job, ClassAd * ClusterAd);
int  DoUnitTests(int options);

char *owner = NULL;
char *myproxy_password = NULL;

int  SendJobCredential();
void SetSendCredentialInAd( ClassAd *job_ad );

extern DLL_IMPORT_MAGIC char **environ;

extern "C" {
int SetSyscalls( int foo );
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
	case PHASE_READ_SUBMIT: sprintf(loc, " on Line %d of submit file", FileMacroSource.line); break;
	case PHASE_DASH_APPEND: sprintf(loc, " with -a argument #%d", ExtraLineNo); break;
	case PHASE_QUEUE:       sprintf(loc, " at Queue statement on Line %d", FileMacroSource.line); break;
	case PHASE_QUEUE_ARG:   sprintf(loc, " with -queue argument"); break;
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

ExtArray <SubmitRec> SubmitInfo(10);
int CurrentSubmitInfo = -1;

ExtArray <ClassAd*> JobAdsArray(100);
int JobAdsArrayLen = 0;
int JobAdsArrayLastClusterIndex = 0;

// called by the factory submit to fill out the data structures that
// we use to print out the standard messages on complection.
void set_factory_submit_info(int cluster, int num_procs)
{
	CurrentSubmitInfo++;
	SubmitInfo[CurrentSubmitInfo].cluster = cluster;
	SubmitInfo[CurrentSubmitInfo].firstjob = 0;
	SubmitInfo[CurrentSubmitInfo].lastjob = num_procs-1;
}

void TestFilePermissions( char *scheddAddr = NULL )
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

	int result, junk;
	MyString name;

	CheckFilesRead.startIterations();
	while( ( CheckFilesRead.iterate( name, junk ) ) )
	{
		result = attempt_access(name.Value(), ACCESS_READ, uid, gid, scheddAddr);
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not readable by condor.\n", 
					name.Value());
		}
	}

	CheckFilesWrite.startIterations();
	while( ( CheckFilesWrite.iterate( name, junk ) ) )
	{
		result = attempt_access(name.Value(), ACCESS_WRITE, uid, gid, scheddAddr );
		if( result == FALSE ) {
			fprintf(stderr, "\nWARNING: File %s is not writable by condor.\n",
					name.Value());
		}
	}
#endif
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

int
main( int argc, const char *argv[] )
{
	FILE	*fp;
	const char **ptr;
	const char *pcolon = NULL;
	const char *cmd_file = NULL;
	int i;
	MyString method;

	setbuf( stdout, NULL );

	set_mySubSystem( "SUBMIT", SUBSYSTEM_TYPE_SUBMIT );

#if !defined(WIN32)
		// Make sure root isn't trying to submit.
	if( getuid() == 0 || getgid() == 0 ) {
		fprintf( stderr, "\nERROR: Submitting jobs as user/group 0 (root) is not "
				 "allowed for security reasons.\n" );
		exit( 1 );
	}
#endif /* not WIN32 */

	MyName = condor_basename(argv[0]);
	myDistro->Init( argc, argv );
	config();

	//TODO:this should go away, and the owner name be placed in ad by schedd!
	owner = my_username();
	if( !owner ) {
		owner = strdup("unknown");
	}

	init_params();
	submit_hash.init();

	default_to_factory = param_boolean("SUBMIT_FACTORY_JOBS_BY_DEFAULT", default_to_factory);

		// If our effective and real gids are different (b/c the
		// submit binary is setgid) set umask(002) so that stdout,
		// stderr and the user log files are created writable by group
		// condor.
	check_umask();

	set_debug_flags(NULL, D_EXPR);

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
			} else if (is_dash_arg_prefix(ptr[0], "debug", 2)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", 0);
			} else if (is_dash_arg_colon_prefix(ptr[0], "dry-run", &pcolon, 3)) {
				DashDryRun = 1;
				bool needs_file_arg = true;
				if (pcolon) { 
					needs_file_arg = false;
					int opt = atoi(pcolon+1);
					if (opt > 1) {  DashDryRun =  opt; needs_file_arg = opt < 0x10; }
					else if (MATCH == strcmp(pcolon+1, "hash")) {
						DumpSubmitHash = 1;
						needs_file_arg = true;
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
			} else if (is_dash_arg_prefix(ptr[0], "factory", 1)) {
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
					delete [] ScheddName;
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
					delete [] ScheddName;
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
					extraLines.Append( *ptr );
				}
			} else if (is_dash_arg_prefix(ptr[0], "batch-name", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -batch-name requires another argument\n",
							 MyName );
					exit( 1 );
				}
				const char * bname = *ptr;
				MyString tmp; // if -batch-name was specified, this holds the string 'MY.JobBatchName = "name"'
				if (*bname == '"') {
					tmp.formatstr("MY.%s = %s", ATTR_JOB_BATCH_NAME, bname);
				} else {
					tmp.formatstr("MY.%s = \"%s\"", ATTR_JOB_BATCH_NAME, bname);
				}
				// if batch_name_line is not NULL,  we will leak a bit here, but that's better than
				// freeing something behind the back of the extraLines
				batch_name_line = strdup(tmp.c_str());
				extraLines.Append(const_cast<const char*>(batch_name_line));
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
			} else if (is_dash_arg_prefix( ptr[0], "password", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -password requires another argument\n",
							 MyName );
				}
				myproxy_password = strdup (*ptr);
			} else if (is_dash_arg_prefix(ptr[0], "pool", 2)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -pool requires another argument\n",
							 MyName );
					exit(1);
				}
				if( PoolName ) {
					delete [] PoolName;
				}
					// TODO We should try to resolve the name to a full
					//   hostname, but get_full_hostname() doesn't like
					//   seeing ":<port>" at the end, which is valid for a
					//   collector name.
				PoolName = strnewp( *ptr );
			} else if (is_dash_arg_prefix(ptr[0], "stm", 1)) {
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -stm requires another argument\n",
							 MyName );
					exit(1);
				}
				method = *ptr;
				string_to_stm(method, STMethod);
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
			} else if (is_dash_arg_prefix(ptr[0], "help")) {
				usage();
				exit( 0 );
			} else if (is_dash_arg_prefix(ptr[0], "interactive", 1)) {
				// we don't currently support -interactive on Windows, but we parse for it anyway.
				dash_interactive = 1;
				extraLines.Append( "+InteractiveJob=True" );
			} else {
				usage();
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
		} else {
			cmd_file = *ptr;
		}
	}

	// Have reading the submit file from stdin imply -verbose. This is
	// for backward compatibility with HTCondor version 8.1.1 and earlier
	// -terse can be used to override the backward compatable behavior.
	SubmitFromStdin = cmd_file && ! strcmp(cmd_file, "-");
	if (SubmitFromStdin && ! terse) {
		verbose = true;
	}

	// ensure I have a known transfer method
	if (STMethod == STM_UNKNOWN) {
		fprintf( stderr, 
			"%s: Unknown sandbox transfer method: %s\n", MyName, method.Value());
		usage();
		exit(1);
	}

	// we only want communication with the schedd to take place... because
	// that's the only type of communication we are interested in
	if ( DumpClassAdToFile && STMethod != STM_USE_SCHEDD_ONLY ) {
		fprintf( stderr, 
			"%s: Dumping ClassAds to a file is not compatible with sandbox "
			"transfer method: %s\n", MyName, method.Value());
		usage();
		exit(1);
	}

	if (!DisableFileChecks) {
		DisableFileChecks = param_boolean_crufty("SUBMIT_SKIP_FILECHECKS", true) ? 1 : 0;
	}

	MaxProcsPerCluster = param_integer("SUBMIT_MAX_PROCS_IN_CLUSTER", 0, 0);

	// the -dry argument takes a qualifier that I'm hijacking to do queue parsing unit tests for now the 8.3 series.
	if (DashDryRun > 0x10) { exit(DoUnitTests(DashDryRun)); }

	// we don't want a schedd instance if we are dumping to a file
	if ( !DumpClassAdToFile ) {
		// Instantiate our DCSchedd so we can locate and communicate
		// with our schedd.  
        if (!ScheddAddr.empty()) {
            MySchedd = new DCSchedd(ScheddAddr.c_str());
        } else {
            MySchedd = new DCSchedd(ScheddName, PoolName);
        }
		if( ! MySchedd->locate() ) {
			if( ScheddName ) {
				fprintf( stderr, "\nERROR: Can't find address of schedd %s\n",
						 ScheddName );
			} else {
				fprintf( stderr, "\nERROR: Can't find address of local schedd\n" );
			}
			exit(1);
		}
	}


	// make sure our shadow will have access to our credential
	// (check is disabled for "-n" and "-r" submits)
	if (query_credential) {

#ifdef WIN32
		// setup the username to query
		char userdom[256];
		char* the_username = my_username();
		char* the_domainname = my_domainname();
		sprintf(userdom, "%s@%s", the_username, the_domainname);
		free(the_username);
		free(the_domainname);

		// if we have a credd, query it first
		bool cred_is_stored = false;
		int store_cred_result;
		Daemon my_credd(DT_CREDD);
		if (my_credd.locate()) {
			store_cred_result = store_cred(userdom, NULL, QUERY_MODE, &my_credd);
			if ( store_cred_result == SUCCESS ||
							store_cred_result == FAILURE_NOT_SUPPORTED) {
				cred_is_stored = true;
			}
		}

		if (!cred_is_stored) {
			// query the schedd
			store_cred_result = store_cred(userdom, NULL, QUERY_MODE, MySchedd);
			if ( store_cred_result == SUCCESS ||
							store_cred_result == FAILURE_NOT_SUPPORTED) {
				cred_is_stored = true;
			}
		}
		if (!cred_is_stored) {
			fprintf( stderr, "\nERROR: No credential stored for %s\n"
					"\n\tCorrect this by running:\n"
					"\tcondor_store_cred add\n", userdom );
			exit(1);
		}
#else
		SendJobCredential();
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
		// This effectively means executable, transfer_executable,
		// arguments, universe, and queue X are ignored from the submit
		// file and instead we rewrite to the values below.
		if ( !InteractiveSubmitFile ) {
			extraLines.Append( "executable=/bin/sleep" );
			extraLines.Append( "transfer_executable=false" );
			extraLines.Append( "arguments=180" );
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
	if ( ! cmd_file || SubmitFromStdin) {
		// no file specified, read from stdin
		fp = stdin;
		submit_hash.insert_source("<stdin>", FileMacroSource);
	} else {
		if( (fp=safe_fopen_wrapper_follow(cmd_file,"r")) == NULL ) {
			fprintf( stderr, "\nERROR: Failed to open command file (%s)\n",
						strerror(errno));
			exit(1);
		}
		// this does both insert_source, and also gives a values to the default $(SUBMIT_FILE) expansion
		submit_hash.insert_submit_filename(cmd_file, FileMacroSource);
		submit_unique_id = hashFuncChars(cmd_file);
	}

	// in case things go awry ...
	_EXCEPT_Reporter = ReportSubmitException;
	_EXCEPT_Cleanup = DoCleanup;

	submit_hash.setDisableFileChecks(DisableFileChecks);
	submit_hash.setFakeFileCreationChecks(DashDryRun);
	submit_hash.setScheddVersion(MySchedd ? MySchedd->version() : CondorVersion());
	if (myproxy_password) submit_hash.setMyProxyPassword(myproxy_password);
	submit_hash.init_cluster_ad(get_submit_time(), owner);

	if ( !DumpClassAdToFile ) {
		if ( ! SubmitFromStdin && ! terse) {
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

	int rval = 0;
	//  Parse the file and queue the jobs
	if (dash_factory && has_late_materialize) {
		rval = submit_factory_job(fp, FileMacroSource, extraLines, queueCommandLine);
	} else {
		rval = read_submit_file(fp);
	}
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
				SubmitFromStdin ? "(stdin)" : cmd_file);
		exit( 1 );
	} else if ( ! GotNonEmptyQueueCommand && ! terse) {
		fprintf(stderr, "\nWARNING: \"%s\" has only empty \"queue\" commands -- no jobs queued\n",
				SubmitFromStdin ? "(stdin)" : cmd_file);
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
	}

	if ( ! SubmitFromStdin && ! terse) {
		fprintf(stdout, "\n");
	}

	ErrContext.phase = PHASE_COMMIT;

	if ( ! verbose && ! DumpFileIsStdout) {
		if (terse) {
			int ixFirst = 0;
			for (int ix = 0; ix <= CurrentSubmitInfo; ++ix) {
				// fprintf(stderr, "\t%d.%d - %d\n", SubmitInfo[ix].cluster, SubmitInfo[ix].firstjob, SubmitInfo[ix].lastjob);
				if ((ix == CurrentSubmitInfo) || SubmitInfo[ix].cluster != SubmitInfo[ix+1].cluster) {
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
			for (i=0; i <= CurrentSubmitInfo; i++) {
				if (SubmitInfo[i].cluster != this_cluster) {
					if (this_cluster != -1) {
						fprintf(stdout, "%d job(s) %s to cluster %d.\n", job_count, DashDryRun ? "dry-run" : "submitted", this_cluster);
						job_count = 0;
					}
					this_cluster = SubmitInfo[i].cluster;
				}
				job_count += SubmitInfo[i].lastjob - SubmitInfo[i].firstjob + 1;
			}
			if (this_cluster != -1) {
				fprintf(stdout, "%d job(s) %s to cluster %d.\n", job_count, DashDryRun ? "dry-run" : "submitted", this_cluster);
			}
		}
	}

	ActiveQueueConnection = FALSE; 

    bool isStandardUni = false;
	isStandardUni = submit_hash.getUniverse() == CONDOR_UNIVERSE_STANDARD;

	if ( !DisableFileChecks || isStandardUni) {
		TestFilePermissions( MySchedd->addr() );
	}

	// we don't want to spool jobs if we are simply writing the ClassAds to 
	// a file, so we just skip this block entirely if we are doing this...
	if ( !DumpClassAdToFile ) {
		if ( dash_remote && JobAdsArrayLen > 0 ) {
			bool result;
			CondorError errstack;

			switch(STMethod) {
				case STM_USE_SCHEDD_ONLY:
					// perhaps check for proper schedd version here?
					result = MySchedd->spoolJobFiles( JobAdsArrayLen,
											  JobAdsArray.getarray(),
											  &errstack );
					if ( !result ) {
						fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
						fprintf( stderr, "ERROR: Failed to spool job files.\n" );
						exit(1);
					}
					break;

				case STM_USE_TRANSFERD:
					{ // start block

					fprintf(stdout,
						"Locating a Sandbox for %d jobs.\n",JobAdsArrayLen);
					MyString td_sinful;
					MyString td_capability;
					ClassAd respad;
					int invalid;
					MyString reason;

					result = MySchedd->requestSandboxLocation( FTPD_UPLOAD, 
												JobAdsArrayLen,
												JobAdsArray.getarray(), FTP_CFTP,
												&respad, &errstack );
					if ( !result ) {
						fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
						fprintf( stderr, 
							"ERROR: Failed to get a sandbox location.\n" );
						exit(1);
					}

					respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
					if (invalid == TRUE) {
						fprintf( stderr, 
							"Schedd rejected sand box location request:\n");
						respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
						fprintf( stderr, "\t%s\n", reason.Value());
						return false;
					}

					respad.LookupString(ATTR_TREQ_TD_SINFUL, td_sinful);
					respad.LookupString(ATTR_TREQ_CAPABILITY, td_capability);

					dprintf(D_ALWAYS, "Got td: %s, cap: %s\n", td_sinful.Value(),
						td_capability.Value());

					fprintf(stdout,"Spooling data files for %d jobs.\n",
						JobAdsArrayLen);

					DCTransferD dctd(td_sinful.Value());

					result = dctd.upload_job_files( JobAdsArrayLen,
											  JobAdsArray.getarray(),
											  &respad, &errstack );
					if ( !result ) {
						fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
						fprintf( stderr, "ERROR: Failed to spool job files.\n" );
						exit(1);
					}

					} // end block

					break;

				default:
					EXCEPT("PROGRAMMER ERROR: Unknown sandbox transfer method");
					break;
			}
		}
	}

	// don't try to reschedule jobs if we are dumping to a file, because, again
	// there will be not schedd present to reschedule thru.
	if ( !DumpClassAdToFile ) {
		if( ProcId != -1 ) {
			reschedule();
		}
	}

	// Deallocate some memory just to keep Purify happy
	for (i=0;i<JobAdsArrayLen;i++) {
		delete JobAdsArray[i];
	}
	submit_hash.delete_job_ad();
	delete MySchedd;

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
		if (PoolName) {
			sshargs[i++] = "-pool";
			sshargs[i++] = PoolName;
		}
		if (ScheddName) {
			sshargs[i++] = "-name";
			sshargs[i++] = ScheddName;
		}
		sprintf(jobid,"%d.0",ClusterId);
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

// callback passed to make_job_ad on the submit_hash that gets passed each input or output file
// so we can choose to do file checks. 
int check_sub_file(void* /*pv*/, SubmitHash * sub, _submit_file_role role, const char * pathname, int flags)
{
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

		LastExecutable = ename;
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

			if (role == SFR_EXECUTABLE) {
				bool param_exists;
				SpoolLastExecutable = sub->submit_param_bool( SUBMIT_KEY_CopyToSpool, "CopyToSpool", false, &param_exists );
				if ( ! param_exists) {
					if ( submit_hash.getUniverse() == CONDOR_UNIVERSE_STANDARD ) {
							// Standard universe jobs can't restore from a checkpoint
							// if the executable changes.  Therefore, it is deemed
							// too risky to have copy_to_spool=false by default
							// for standard universe.
						SpoolLastExecutable = true;
					}
					else {
							// In so many cases, copy_to_spool=true would just add
							// needless overhead.  Therefore, (as of 6.9.3), the
							// default is false.
						SpoolLastExecutable = false;
					}
				}
			}
		}
		return 0;
	}

	// Queue files for testing access if not already queued
	int junk;
	if( flags & O_WRONLY )
	{
		if ( CheckFilesWrite.lookup(pathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesWrite.insert(pathname,junk);
		}
	}
	else
	{
		if ( CheckFilesRead.lookup(pathname,junk) < 0 ) {
			// this file not found in our list; add it
			CheckFilesRead.insert(pathname,junk);
		}
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
	if ( param_boolean("SUBMIT_SEND_RESCHEDULE",true) ) {
		Stream::stream_type st = MySchedd->hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
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

		queue_end(vars, false);
		if (rval < 0)
			return rval;
	}

	return 0;
}


MyString last_submit_executable;
MyString last_submit_cmd;

int SpecialSubmitPreQueue(const char* queue_args, bool from_file, MACRO_SOURCE& source, MACRO_SET& macro_set, std::string & errmsg)
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");
	int rval = 0;

	GotQueueCommand = true;

	// parse the extra lines before doing $ expansion on the queue line
	ErrContext.phase = PHASE_DASH_APPEND;
	ExtraLineNo = 0;
	extraLines.Rewind();
	const char * exline;
	while (extraLines.Next(exline)) {
		++ExtraLineNo;
		rval = Parse_config_string(source, 1, exline, macro_set, ctx);
		if (rval < 0)
			return rval;
		rval = 0;
	}
	ExtraLineNo = 0;
	ErrContext.phase = PHASE_QUEUE;
	ErrContext.step = -1;

	if (DashDryRun && DumpSubmitHash) {
		auto_free_ptr expanded_queue_args(expand_macro(queue_args, macro_set, ctx));
		char * pqargs = expanded_queue_args.ptr();
		ASSERT(pqargs);
		fprintf(stdout, "\n----- Queue arguments -----\nSpecified: %s\nExpanded : %s", queue_args, pqargs);
	}

	if ( ! queueCommandLine.empty() && from_file) {
		errmsg = "-queue argument conflicts with queue statement in submit file";
		return -1;
	}

	// HACK! the queue function uses a global flag to know whether or not to ask for a new cluster
	// In 8.2 this flag is set whenever the "executable" or "cmd" value is set in the submit file.
	// As of 8.3, we don't believe this is still necessary, but we are afraid to change it. This
	// code is *mostly* the same, but will differ in cases where the users is setting cmd or executble
	// to the *same* value between queue statements. - hopefully this is close enough.
	MyString cur_submit_executable(lookup_macro_exact_no_default(SUBMIT_KEY_Executable, macro_set, 0));
	if (last_submit_executable != cur_submit_executable) {
		NewExecutable = true;
		last_submit_executable = cur_submit_executable;
	}
	MyString cur_submit_cmd(lookup_macro_exact_no_default("cmd", macro_set, 0));
	if (last_submit_cmd != cur_submit_cmd) {
		NewExecutable = true;
		last_submit_cmd = cur_submit_cmd;
	}

	return rval;
}

int SpecialSubmitPostQueue()
{
	if (dash_interactive && !InteractiveSubmitFile) {
		// for interactive jobs, just one queue statement
		// a return of 1 tells it to stop scanning, but is not an error.
		return 1;
	}
	ErrContext.phase = PHASE_READ_SUBMIT;
	return 0;
}

// this gets called while parsing the submit file to process lines
// that don't look like valid key=value pairs.  That *should* only be queue lines for now.
// return 0 to keep scanning the file, ! 0 to stop scanning. non-zero return values will
// be passed back out of Parse_macros
//
int SpecialSubmitParse(void* pv, MACRO_SOURCE& source, MACRO_SET& macro_set, char * line, std::string & errmsg)
{
	FILE* fp_submit = (FILE*)pv;
	int rval = 0;
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");

	// Check to see if this is a queue statement.
	//
	const char * queue_args = is_queue_statement(line);
	if (queue_args) {

		rval = SpecialSubmitPreQueue(queue_args, fp_submit!=NULL, source, macro_set, errmsg);
		if (rval) {
			return rval;
		}

		auto_free_ptr expanded_queue_args(expand_macro(queue_args, macro_set, ctx));
		char * pqargs = expanded_queue_args.ptr();
		ASSERT(pqargs);

		// set glob expansion options from submit statements.
		int expand_options = 0;
		if (submit_hash.submit_param_bool("SubmitWarnEmptyMatches", "submit_warn_empty_matches", true)) {
			expand_options |= EXPAND_GLOBS_WARN_EMPTY;
		}
		if (submit_hash.submit_param_bool("SubmitFailEmptyMatches", "submit_fail_empty_matches", false)) {
			expand_options |= EXPAND_GLOBS_FAIL_EMPTY;
		}
		if (submit_hash.submit_param_bool("SubmitWarnDuplicateMatches", "submit_warn_duplicate_matches", true)) {
			expand_options |= EXPAND_GLOBS_WARN_DUPS;
		}
		if (submit_hash.submit_param_bool("SubmitAllowDuplicateMatches", "submit_allow_duplicate_matches", false)) {
			expand_options |= EXPAND_GLOBS_ALLOW_DUPS;
		}
		char* parm = submit_hash.submit_param("SubmitMatchDirectories", "submit_match_directories");
		if (parm) {
			if (MATCH == strcasecmp(parm, "never") || MATCH == strcasecmp(parm, "no") || MATCH == strcasecmp(parm, "false")) {
				expand_options |= EXPAND_GLOBS_TO_FILES;
			} else if (MATCH == strcasecmp(parm, "only")) {
				expand_options |= EXPAND_GLOBS_TO_DIRS;
			} else if (MATCH == strcasecmp(parm, "yes") || MATCH == strcasecmp(parm, "true")) {
				// nothing to do.
			} else {
				errmsg = parm;
				errmsg += " is not a valid value for SubmitMatchDirectories";
				return -1;
			}
			free(parm); parm = NULL;
		}


		// skip whitespace before queue arguments (if any)
		while (isspace(*pqargs)) ++pqargs;

		SubmitForeachArgs o;
		int queue_modifier = 1;
		int citems = -1;
		// parse the queue arguments, handling the count and finding the in,from & matching keywords
		// on success pqargs will point to to \0 or to just after the keyword.
		rval = o.parse_queue_args(pqargs);
		queue_modifier = o.queue_num;
		if (rval < 0) {
			errmsg = "invalid Queue statement";
			return rval;
		}

		// if no loop variable specified, but a foreach mode is used. use "Item" for the loop variable.
		if (o.vars.isEmpty() && (o.foreach_mode != foreach_not)) { o.vars.append("Item"); }

		if ( ! o.items_filename.empty()) {
			if (o.items_filename == "<") {
				if ( ! fp_submit) {
					errmsg = "unexpected error while attempting to read queue items from submit file.";
					return -1;
				}
				// read items from submit file until we see the closing brace on a line by itself.
				bool saw_close_brace = false;
				int item_list_begin_line = source.line;
				for(char * line=NULL; ; ) {
					line = getline_trim(fp_submit, source.line);
					if ( ! line) break; // null indicates end of file
					if (line[0] == '#') continue; // skip comments.
					if (line[0] == ')') { saw_close_brace = true; break; }
					if (o.foreach_mode == foreach_from) {
						o.items.append(line);
					} else {
						o.items.initializeFromString(line);
					}
				}
				if ( ! saw_close_brace) {
					formatstr(errmsg, "Reached end of file without finding closing brace ')'"
						" for Queue command on line %d", item_list_begin_line);
					return -1;
				}
			} else if (o.items_filename == "-") {
				int lineno = 0;
				for (char* line=NULL;;) {
					line = getline_trim(stdin, lineno);
					if ( ! line) break;
					if (o.foreach_mode == foreach_from) {
						o.items.append(line);
					} else {
						o.items.initializeFromString(line);
					}
				}
			} else {
				MACRO_SOURCE ItemsSource;
				FILE * fp = Open_macro_source(ItemsSource, o.items_filename.Value(), false, macro_set, errmsg);
				if ( ! fp) {
					return -1;
				}
				for (char* line=NULL;;) {
					line = getline_trim(fp, ItemsSource.line);
					if ( ! line) break;
					o.items.append(line);
				}
				rval = Close_macro_source(fp, ItemsSource, macro_set, 0);
			}
		}

		switch (o.foreach_mode) {
		case foreach_in:
		case foreach_from:
			// itemlist is already correct
			// PRAGMA_REMIND("do argument validation here?")
			citems = o.items.number();
			break;

		case foreach_matching:
		case foreach_matching_files:
		case foreach_matching_dirs:
		case foreach_matching_any:
			if (o.foreach_mode == foreach_matching_files) {
				expand_options &= ~EXPAND_GLOBS_TO_DIRS;
				expand_options |= EXPAND_GLOBS_TO_FILES;
			} else if (o.foreach_mode == foreach_matching_dirs) {
				expand_options &= ~EXPAND_GLOBS_TO_FILES;
				expand_options |= EXPAND_GLOBS_TO_DIRS;
			} else if (o.foreach_mode == foreach_matching_any) {
				expand_options &= ~(EXPAND_GLOBS_TO_FILES|EXPAND_GLOBS_TO_DIRS);
			}
			citems = submit_expand_globs(o.items, expand_options, errmsg);
			if ( ! errmsg.empty()) {
				fprintf(stderr, "\n%s: %s", citems >= 0 ? "WARNING" : "ERROR", errmsg.c_str());
				errmsg.clear();
			}
			if (citems < 0) return citems;
			break;

		default:
		case foreach_not:
			// to simplify the loop below, set a single empty item into the itemlist.
			citems = 1;
			break;
		}

		if (queue_modifier > 0 && citems > 0) {
			iterate_queue_foreach(queue_modifier, o.vars, o.items, o.slice);
		}

		rval = SpecialSubmitPostQueue();
		if (rval)
			return rval;
		return 0; // keep scanning
	}
	return -1;
}

int read_submit_file(FILE * fp)
{
	ErrContext.phase = PHASE_READ_SUBMIT;

	std::string errmsg;
  #ifdef USE_HALTING_PARSER
	int rval = 0;
	while (rval == 0) {
		char * qline = NULL;
		rval = submit_hash.parse_file_up_to_q_line(fp, FileMacroSource, errmsg, &qline);
		if (rval || ! qline)
			break;

		const char * queue_args = is_queue_statement(qline);
		if ( ! queue_args)
			break;

		// Do last minute things that happen just before the queue statement
		// NOTE: it is by design that the ClusterId can only change once per queue statement
		// even if the queue iteration changes the "cmd". It's important for the job factory
		// that the cluster does not change.
		rval = SpecialSubmitPreQueue(queue_args, true, FileMacroSource, submit_hash.macros(), errmsg);
		if (rval)
			break;

		SubmitForeachArgs o;
		rval = submit_hash.parse_q_args(queue_args, o, errmsg);
		if (rval)
			break;

		rval = submit_hash.load_q_foreach_items(fp, FileMacroSource, o, errmsg);
		if (rval)
			break;

		rval = iterate_queue_foreach(o.queue_num, o.vars, o.items, o.slice);
		if (rval)
			break;

		rval = SpecialSubmitPostQueue();
		if (rval) {
			// a return of 1 means stop scanning, but i
			if (rval == 1) rval = 0;
			break;
		}
	}
  #else
	int rval = submit_hash.parse_file(fp, FileMacroSource, errmsg, SpecialSubmitParse, fp);
  #endif

	if( rval < 0 ) {
		fprintf (stderr, "\nERROR: on Line %d of submit file: %s\n", FileMacroSource.line, errmsg.c_str());
		if (submit_hash.error_stack()) {
			std::string errstk(submit_hash.error_stack()->getFullText());
			if ( ! errstk.empty()) {
				fprintf(stderr, "%s", errstk.c_str());
			}
			submit_hash.error_stack()->clear();
		}
	} else {
		ErrContext.phase = PHASE_QUEUE_ARG;

		// if a -queue command line option was specified, and the submit file did not have a queue statement
		// then parse the command line argument now (as if it was appended to the submit file)
		if ( rval == 0 && ! GotQueueCommand && ! queueCommandLine.empty()) {
			char * line = strdup(queueCommandLine.c_str()); // copy cmdline so we can destructively parse it.
			ASSERT(line);
			rval = submit_hash.process_q_line(FileMacroSource, line, errmsg, SpecialSubmitParse, NULL);
			free(line);
		}
		if (rval < 0) {
			fprintf (stderr, "\nERROR: while processing -queue command line option: %s\n", errmsg.c_str());
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
			SimScheddQ* SimQ = new SimScheddQ();
			if (DumpFileIsStdout) {
				SimQ->Connect(stdout, false, false);
			} else if (DashDryRun) {
				FILE* dryfile = NULL;
				bool  free_it = false;
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
MACRO_ITEM* find_submit_item(const char * name) { return submit_hash.lookup_exact(name); }
#define set_live_submit_variable submit_hash.set_live_submit_variable

int queue_begin(StringList & vars, bool new_cluster)
{
	if ( ! MyQ)
		return -1;

	// establish live buffers for $(Cluster) and $(Process), and other loop variables
	// Because the user might already be using these variables, we can only set the explicit ones
	// unconditionally, the others we must set only when not already set by the user.
	set_live_submit_variable(SUBMIT_KEY_Cluster, ClusterString);
	set_live_submit_variable(SUBMIT_KEY_Process, ProcessString);
	
	vars.rewind();
	char * var;
	while ((var = vars.next())) { set_live_submit_variable(var, EmptyItemString, false); }

	// optimize the macro set for lookups if we inserted anything.  we expect this to happen only once.
	submit_hash.optimize();

	if (new_cluster) {
		// if we have already created the maximum number of clusters, error out now.
		if (DashMaxClusters > 0 && ClustersCreated >= DashMaxClusters) {
			fprintf(stderr, "\nERROR: Number of submitted clusters would exceed %d and -single-cluster was specified\n", DashMaxClusters);
			exit(1);
		}

		if ((ClusterId = MyQ->get_NewCluster()) < 0) {
			fprintf(stderr, "\nERROR: Failed to create cluster\n");
			if ( ClusterId == -2 ) {
				fprintf(stderr,
					"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
			}
			exit(1);
		}

		++ClustersCreated;

		delete gClusterAd; gClusterAd = NULL;
	}

	if (DashDryRun && DumpSubmitHash) {
		fprintf(stdout, "\n----- submit hash at queue begin -----\n");
		int flags = (DumpSubmitHash & 8) ? HASHITER_SHOW_DUPS : 0;
		submit_hash.dump(stdout, flags);
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

	int rval = 0; // default to success (if num == 0 we will return this)
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
		set_live_submit_variable(var, data, false);

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
				set_live_submit_variable(var, data, false);
			}
		}

		if (check_empty) {
			vars.rewind();
			std::string empties;
			while ((var = vars.next())) {
				MACRO_ITEM* pitem = find_submit_item(var);
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

	if (DashDryRun && DumpSubmitHash) {
		fprintf(stdout, "----- submit hash changes for ItemIndex = %d -----\n", item_index);
		char * var;
		vars.rewind();
		while ((var = vars.next())) {
			MACRO_ITEM* pitem = find_submit_item(var);
			fprintf (stdout, "  %s = %s\n", var, pitem ? pitem->raw_value : "");
		}
		fprintf(stdout, "-----\n");
	}

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
			if ( ProcId == -2 ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
			} else if( ProcId == -3 ) {
				fprintf(stderr,
				"Number of submitted jobs would exceed MAX_JOBS_PER_OWNER\n");
			} else if( ProcId == -4 ) {
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
		(void)sprintf(ClusterString, "%d", ClusterId);
		(void)sprintf(ProcessString, "%d", ProcId);

		// we move this outside the above, otherwise it appears that we have 
		// received no queue command (or several, if there were multiple ones)
		GotNonEmptyQueueCommand = 1;

		ClassAd * job = submit_hash.make_job_ad(
			JOB_ID_KEY(ClusterId,ProcId),
			item_index, ii,
			dash_interactive, dash_remote,
			check_sub_file, NULL);
		if ( ! job) {
			print_errstack(stderr, submit_hash.error_stack());
			DoCleanup(0,0,NULL);
			exit(1);
		}

		int JobUniverse = submit_hash.getUniverse();
		SendLastExecutable(); // if spooling the exe, send it now.
		SetSendCredentialInAd( job );
		NewExecutable = false;
		// write job ad to schedd or dump to file, depending on what type MyQ is
		rval = SendJobAd(job, gClusterAd);
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

		if (CurrentSubmitInfo == -1 || SubmitInfo[CurrentSubmitInfo].cluster != ClusterId) {
			CurrentSubmitInfo++;
			SubmitInfo[CurrentSubmitInfo].cluster = ClusterId;
			SubmitInfo[CurrentSubmitInfo].firstjob = ProcId;
		}
		SubmitInfo[CurrentSubmitInfo].lastjob = ProcId;

		// SubmitInfo[x].lastjob controls how many submit events we
		// see in the user log.  For parallel jobs, we only want
		// one "job" submit event per cluster, no matter how many 
		// Procs are in that it.  Setting lastjob to zero makes this so.

		if (JobUniverse == CONDOR_UNIVERSE_PARALLEL) {
			SubmitInfo[CurrentSubmitInfo].lastjob = 0;
		}

		if ( ! gClusterAd) {
			gClusterAd = new ClassAd(*job);
			for (size_t ii = 0; ii < COUNTOF(no_cluster_attrs); ++ii) {
				gClusterAd->Delete(no_cluster_attrs[ii]);
			}
		}

		// If spooling entire job "sandbox" to the schedd, then we need to keep
		// the classads in an array to later feed into the filetransfer object.
		if ( dash_remote ) {
			ClassAd * tmp = new ClassAd(*job);
			tmp->Assign(ATTR_CLUSTER_ID, ClusterId);
			tmp->Assign(ATTR_PROC_ID, ProcId);
			if (0 == ProcId) {
				JobAdsArrayLastClusterIndex = JobAdsArrayLen;
			} else {
				// proc ad to cluster ad (if there is one)
				tmp->ChainToAd(JobAdsArray[JobAdsArrayLastClusterIndex]);
			}
			JobAdsArray[ JobAdsArrayLen++ ] = tmp;
			return true;
		}

		submit_hash.delete_job_ad();
		job = NULL;
	}

	ErrContext.step = -1;

	return rval;
}

void queue_end(StringList & vars, bool /*fEof*/)   // called when done processing each Queue statement and at end of submit file
{
	// set live submit variables to generate reasonable unused-item messages.
	if ( ! vars.isEmpty())  {
		vars.rewind();
		char * var = vars.next();
		while (var) {
			set_live_submit_variable(var, "<Queue_item>", false);
			var = vars.next();
		}
	}
}

#undef set_live_submit_variable


void
usage()
{
	fprintf( stderr, "Usage: %s [options] [<attrib>=<value>] [- | <submit-file>]\n", MyName );
	fprintf( stderr, "    [options] are\n" );
	fprintf( stderr, "\t-terse  \t\tDisplay terse output, jobid ranges only\n" );
	fprintf( stderr, "\t-verbose\t\tDisplay verbose output, jobid and full job ClassAd\n" );
	fprintf( stderr, "\t-debug  \t\tDisplay debugging output\n" );
	fprintf( stderr, "\t-append <line>\t\tadd line to submit file before processing\n"
					 "\t              \t\t(overrides submit file; multiple -a lines ok)\n" );
	fprintf( stderr, "\t-queue <queue-opts>\tappend Queue statement to submit file before processing\n"
					 "\t                   \t(submit file must not already have a Queue statement)\n" );
	fprintf( stderr, "\t-batch-name <name>\tappend a line to submit file that sets the batch name\n"
					/* "\t                  \t(overrides batch_name in submit file)\n" */);
	fprintf( stderr, "\t-disable\t\tdisable file permission checks\n" );
	fprintf( stderr, "\t-dry-run <filename>\tprocess submit file and write ClassAd attributes to <filename>\n"
					 "\t        \t\tbut do not actually submit the job(s) to the SCHEDD\n" );
	fprintf( stderr, "\t-maxjobs <maxjobs>\tDo not submit if number of jobs would exceed <maxjobs>.\n" );
	fprintf( stderr, "\t-single-cluster\t\tDo not submit if more than one ClusterId is needed.\n" );
	fprintf( stderr, "\t-unused\t\t\ttoggles unused or unexpanded macro warnings\n"
					 "\t       \t\t\t(overrides config file; multiple -u flags ok)\n" );
	//fprintf( stderr, "\t-force-mpi-universe\tAllow submission of obsolete MPI universe\n );
	fprintf( stderr, "\t-dump <filename>\tWrite job ClassAds to <filename> instead of\n"
					 "\t                \tsubmitting to a schedd.\n" );
#if !defined(WIN32)
	fprintf( stderr, "\t-interactive\t\tsubmit an interactive session job\n" );
#endif
	fprintf( stderr, "\t-name <name>\t\tsubmit to the specified schedd\n" );
	fprintf( stderr, "\t-remote <name>\t\tsubmit to the specified remote schedd\n"
					 "\t              \t\t(implies -spool)\n" );
    fprintf( stderr, "\t-addr <ip:port>\t\tsubmit to schedd at given \"sinful string\"\n" );
	fprintf( stderr, "\t-spool\t\t\tspool all files to the schedd\n" );
	fprintf( stderr, "\t-password <password>\tspecify password to MyProxy server\n" );
	fprintf( stderr, "\t-pool <host>\t\tUse host as the central manager to query\n" );
	fprintf( stderr, "\t-stm <method>\t\tHow to move a sandbox into HTCondor\n" );
	fprintf( stderr, "\t             \t\t<methods> is one of: stm_use_schedd_only\n" );
	fprintf( stderr, "\t             \t\t                     stm_use_transferd\n" );

	fprintf( stderr, "\t<attrib>=<value>\tSet <attrib>=<value> before reading the submit file.\n" );

	fprintf( stderr, "\n    If <submit-file> is omitted or is -, input is read from stdin.\n"
					 "    Use of - implies verbose output unless -terse is specified\n");
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

	if (owner) {
		free(owner);
	}
	if (myproxy_password) {
		free (myproxy_password);
	}

	return 0;		// For historical reasons...
}
} /* extern "C" */


void
init_params()
{
	char *tmp = NULL;
	MyString method;

	const char * err = init_submit_default_macros();
	if (err) {
		fprintf(stderr, "%s\n", err);
		DoCleanup(0,0,NULL);
		exit( 1 );
	}

	My_fs_domain = param( "FILESYSTEM_DOMAIN" );
		// Will always return something, since config() will put in a
		// value (full hostname) if it's not in the config file.  
	

	// The default is set as the global initializer for STMethod
	tmp = param( "SANDBOX_TRANSFER_METHOD" );
	if ( tmp != NULL ) {
		method = tmp;
		free( tmp );
		string_to_stm( method, STMethod );
	}

	WarnOnUnusedMacros =
		param_boolean_crufty("WARN_ON_UNUSED_SUBMIT_FILE_MACROS",
							 WarnOnUnusedMacros ? true : false) ? 1 : 0;

	if ( param_boolean("SUBMIT_NOACK_ON_SETATTRIBUTE",true) ) {
		setattrflags |= SetAttribute_NoAck;  // set noack flag
	} else {
		setattrflags &= ~SetAttribute_NoAck; // clear noack flag
	}

}


extern "C" {
int SetSyscalls( int foo ) { return foo; }
}


int SendJobCredential()
{
	// however, if we do need to send it for any job, we only need to do that once.
	if (sent_credential_to_credd) {
		return 0;
	}

	MyString producer;
	if(!param(producer, "SEC_CREDENTIAL_PRODUCER")) {
		// nothing to do
		return 0;
	}

	// If SEC_CREDENTIAL_PRODUCER is set to magic value CREDENTIAL_ALREADY_STORED,
	// this means that condor_submit should NOT bother spending time to send the
	// credential to the credd (because it is already there), but it SHOULD do
	// all the other work as if it did send it (such as setting the SendCredential
	// attribute so the starter will fetch the credential at job launch).
	// If SEC_CREDENTIAL_PRODUCER is anything else, then consider it to be the
	// name of a script we should spawn to create the credential.

	if ( strcasecmp(producer.Value(),"CREDENTIAL_ALREADY_STORED") != MATCH ) {
		// If we made it here, we need to spawn a credential producer process.
		dprintf(D_ALWAYS, "CREDMON: invoking %s\n", producer.c_str());
		ArgList args;
		args.AppendArg(producer);
		FILE* uber_file = my_popen(args, "r", 0);
		unsigned char *uber_ticket = NULL;
		if (!uber_file) {
			fprintf(stderr, "\nERROR: (%i) invoking %s\n", errno, producer.c_str());
			exit( 1 );
		} else {
			uber_ticket = (unsigned char*)malloc(65536);
			ASSERT(uber_ticket);
			int bytes_read = fread(uber_ticket, 1, 65536, uber_file);
			// what constitutes failure?
			my_pclose(uber_file);

			if(bytes_read == 0) {
				fprintf(stderr, "\nERROR: failed to read any data from %s!\n", producer.c_str());
				exit( 1 );
			}

			// immediately convert to base64
			char* ut64 = zkm_base64_encode(uber_ticket, bytes_read);

			// sanity check:  convert it back.
			//unsigned char *zkmbuf = 0;
			int zkmlen = -1;
			unsigned char* zkmbuf = NULL;
			zkm_base64_decode(ut64, &zkmbuf, &zkmlen);

			dprintf(D_FULLDEBUG, "CREDMON: b64: %i %i\n", bytes_read, zkmlen);
			dprintf(D_FULLDEBUG, "CREDMON: b64: %s %s\n", (char*)uber_ticket, (char*)zkmbuf);

			char preview[64];
			strncpy(preview,ut64, 63);
			preview[63]=0;

			dprintf(D_FULLDEBUG | D_SECURITY, "CREDMON: read %i bytes {%s...}\n", bytes_read, preview);

			// setup the username to query
			char userdom[256];
			char* the_username = my_username();
			char* the_domainname = my_domainname();
			sprintf(userdom, "%s@%s", the_username, the_domainname);
			free(the_username);
			free(the_domainname);

			dprintf(D_ALWAYS, "CREDMON: storing cred for user %s\n", userdom);
			Daemon my_credd(DT_CREDD);
			int store_cred_result;
			if (my_credd.locate()) {
				store_cred_result = store_cred(userdom, ut64, ADD_MODE, &my_credd);
				if ( store_cred_result != SUCCESS ) {
					fprintf( stderr, "\nERROR: store_cred failed!\n");
					exit( 1 );
				}
			} else {
				fprintf( stderr, "\nERROR: locate(credd) failed!\n");
				exit( 1 );
			}
		}
	}  // end of block to run a credential producer

	// If we made it here, we either successufully ran a credential producer, or
	// we've been told a credential has already been stored.  Either way we want
	// to set a flag that tells the rest of condor_submit that there is a stored
	// credential associated with this job.

	sent_credential_to_credd = true;

	return 0;
}

void SetSendCredentialInAd( ClassAd *job_ad )
{
	if (!sent_credential_to_credd) {
		return;
	}

	// add it to the job ad (starter needs to know this value)
	job_ad->Assign( ATTR_JOB_SEND_CREDENTIAL, true );
}


int SendLastExecutable()
{
	const char * ename = LastExecutable.empty() ? NULL : LastExecutable.Value();
	bool copy_to_spool = SpoolLastExecutable;
	MyString SpoolEname(ename);

	// spool executable if necessary
	if ( ename && copy_to_spool ) {

		bool try_ickpt_sharing = false;
		CondorVersionInfo cvi(MySchedd->version());
		if (cvi.built_since_version(7, 3, 0)) {
			try_ickpt_sharing = param_boolean("SHARE_SPOOLED_EXECUTABLES",
												true);
		}

		MyString md5;
		if (try_ickpt_sharing) {
			Condor_MD_MAC cmm;
			unsigned char* md5_raw;
			if (!cmm.addMDFile(ename)) {
				dprintf(D_ALWAYS,
						"SHARE_SPOOLED_EXECUTABLES will not be used: "
							"MD5 of file %s failed\n",
						ename);
			}
			else if ((md5_raw = cmm.computeMD()) == NULL) {
				dprintf(D_ALWAYS,
						"SHARE_SPOOLED_EXECUTABLES will not be used: "
							"no MD5 support in this Condor build\n");
			}
			else {
				for (int i = 0; i < MAC_SIZE; i++) {
					md5.formatstr_cat("%02x", static_cast<int>(md5_raw[i]));
				}
				free(md5_raw);
			}
		}
		int ret;
		if ( ! md5.IsEmpty()) {
			ClassAd tmp_ad;
			tmp_ad.Assign(ATTR_OWNER, owner);
			tmp_ad.Assign(ATTR_JOB_CMD_MD5, md5.Value());
			ret = MyQ->send_SpoolFileIfNeeded(tmp_ad);
		}
		else {
			char * chkptname = GetSpooledExecutablePath(submit_hash.getClusterId(), "");
			SpoolEname = chkptname;
			if (chkptname) free(chkptname);
			ret = MyQ->send_SpoolFile(SpoolEname.Value());
		}

		if (ret < 0) {
			fprintf( stderr,
						"\nERROR: Request to transfer executable %s failed\n",
						SpoolEname.Value() );
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


int SendJobAd (ClassAd * job, ClassAd * ClusterAd)
{
	ExprTree *tree = NULL;
	const char *lhstr, *rhstr;
	int  retval = 0;
	int myprocid = ProcId;

	if ( ProcId > 0 ) {
		MyQ->set_AttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId, setattrflags);
	} else {
		myprocid = -1;		// means this is a cluster ad
		if( MyQ->set_AttributeInt (ClusterId, myprocid, ATTR_CLUSTER_ID, ClusterId, setattrflags) == -1 ) {
			if( setattrflags & SetAttribute_NoAck ) {
				fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
					ClusterId, ProcId);
			} else {
				fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n",
					ATTR_CLUSTER_ID, ClusterId, ClusterId, ProcId, errno);
			}
			return -1;
		}
	}

#if defined(ADD_TARGET_SCOPING)
	if ( JobUniverse == CONDOR_UNIVERSE_SCHEDULER ||
		 JobUniverse == CONDOR_UNIVERSE_LOCAL ) {
		job->AddTargetRefs( TargetScheddAttrs, false );
	} else {
		job->AddTargetRefs( TargetMachineAttrs, false );
	}
#endif

	job->ResetExpr();
	while( job->NextExpr(lhstr, tree) ) {
		rhstr = ExprTreeToString( tree );
		if( !lhstr || !rhstr ) { 
			fprintf( stderr, "\nERROR: Null attribute name or value for job %d.%d\n",
					 ClusterId, ProcId );
			retval = -1;
		} else {
			int tmpProcId = myprocid;
			// IsNoClusterAttr tells us that an attribute should always be in the proc ad
			bool force_proc = IsNoClusterAttr(lhstr);
			if ( ProcId > 0 ) {
				// For all but the first proc, check each attribute against the value in the cluster ad.
				// If the values match, don't add the attribute to this proc ad
				if ( ! force_proc) {
					ExprTree *cluster_tree = ClusterAd->LookupExpr( lhstr );
					if ( cluster_tree && *tree == *cluster_tree ) {
						continue;
					}
				}
			} else {
				if (force_proc) {
					myprocid = ProcId;
				}
			}

			if( MyQ->set_Attribute(ClusterId, myprocid, lhstr, rhstr, setattrflags) == -1 ) {
				if( setattrflags & SetAttribute_NoAck ) {
					fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
						ClusterId, ProcId);
				} else {
					fprintf( stderr, "\nERROR: Failed to set %s=%s for job %d.%d (%d)\n",
						 lhstr, rhstr, ClusterId, ProcId, errno );
				}
				retval = -1;
			}
			myprocid = tmpProcId;
		}
		if(retval == -1) {
			return -1;
		}
	}

	if ( ProcId == 0 ) {
		if( MyQ->set_AttributeInt (ClusterId, ProcId, ATTR_PROC_ID, ProcId, setattrflags) == -1 ) {
			if( setattrflags & SetAttribute_NoAck ) {
				fprintf( stderr, "\nERROR: Failed submission for job %d.%d - aborting entire submit\n",
					ClusterId, ProcId);
			} else {
				fprintf( stderr, "\nERROR: Failed to set %s=%d for job %d.%d (%d)\n",
					 ATTR_PROC_ID, ProcId, ClusterId, ProcId, errno );
			}
			return -1;
		}
	}

	return retval;
}


// hash function used by the CheckFiles read/write tables
static unsigned int 
hashFunction (const MyString &str)
{
	 int i = str.Length() - 1;
	 unsigned int hashVal = 0;
	 while (i >= 0) 
	 {
		  hashVal += (unsigned int)tolower(str[i]);
		  i--;
	 }
	 return hashVal;
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
		SetEnv( "RENDEZVOUS_DIRECTORY", Rendezvous );
		free( Rendezvous );
	}
}


#if 1
//PRAGMA_REMIND("make a proper queue args unit test.")
int DoUnitTests(int options)
{
	return (options > 1) ? 1 : 0;
}
#else
// old pre submit_utils unit tests. for reference.
int DoUnitTests(int options)
{
	static const struct {
		int          rval;
		const char * args;
		int          num;
		int          mode;
		int          cvars;
		int          citems;
	} trials[] = {
	// rval  args          num    mode      vars items
		{0,  "in (a b)",     1, foreach_in,   0,  2},
		{0,  "ARG in (a,b)", 1, foreach_in,   1,  2},
		{0,  "ARG in(a)",    1, foreach_in,   1,  1},
		{0,  "ARG\tin\t(a)", 1, foreach_in,   1,  1},
		{0,  "arg IN ()",    1, foreach_in,   1,  0},
		{0,  "2 ARG in ( a, b, cd )",  2, foreach_in,   1,  3},
		{0,  "100 ARG In a, b, cd",  100, foreach_in,   1,  3},
		{0,  "  ARG In a b cd e",  1, foreach_in,   1,  4},

		{0,  "matching *.dat",               1, foreach_matching,   0,  1},
		{0,  "FILE matching *.dat",          1, foreach_matching,   1,  1},
		{0,  "glob MATCHING (*.dat *.foo)",  1, foreach_matching,   1,  2},
		{0,  "glob MATCHING files (*.foo)",  1, foreach_matching_files,   1,  1},
		{0,  " dir matching */",             1, foreach_matching,   1,  1},
		{0,  " dir matching dirs */",        1, foreach_matching_dirs,   1,  1},

		{0,  "Executable,Arguments From args.lst",  1, foreach_from,   2,  0},
		{0,  "Executable,Arguments From (sleep.exe 10)",  1, foreach_from,   2,  1},
		{0,  "9 Executable Arguments From (sleep.exe 10)",  9, foreach_from,   2,  1},

		{0,  "arg from [1]  args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [:1] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [::] args.lst",     1, foreach_from,   1,  0},

		{0,  "arg from [100::] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [:100:] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [::100] args.lst",     1, foreach_from,   1,  0},

		{0,  "arg from [100:10:5] args.lst",     1, foreach_from,   1,  0},
		{0,  "arg from [10:100:5] args.lst",     1, foreach_from,   1,  0},

		{0,  "",             1, 0, 0, 0},
		{0,  "2",            2, 0, 0, 0},
		{0,  "9 - 2",        7, 0, 0, 0},
		{0,  "1 -1",         0, 0, 0, 0},
		{0,  "2*2",          4, 0, 0, 0},
		{0,  "(2+3)",        5, 0, 0, 0},
		{0,  "2 * 2 + 5",    9, 0, 0, 0},
		{0,  "2 * (2 + 5)", 14, 0, 0, 0},
		{-2, "max(2,3)",    -1, 0, 0, 0},
		{0,  "max({2,3})",   3, 0, 0, 0},

	};

	for (int ii = 0; ii < (int)COUNTOF(trials); ++ii) {
		int foreach_mode=-1;
		int queue_num=-1;
		StringList vars;
		StringList items;
		qslice     slice;
		MyString   items_filename;
		char * pqargs = strdup(trials[ii].args);
		int rval = parse_queue_args(pqargs, foreach_mode, queue_num, vars, items, slice, items_filename);

		int cvars = vars.number();
		int citems = items.number();
		bool ok = (rval == trials[ii].rval && foreach_mode == trials[ii].mode && queue_num == trials[ii].num && cvars == trials[ii].cvars && citems == trials[ii].citems);

		char * vars_list = vars.print_to_string();
		if ( ! vars_list) vars_list = strdup("");

		char * items_list = items.print_to_delimed_string("|");
		if ( ! items_list) items_list = strdup("");
		else if (strlen(items_list) > 50) { items_list[50] = 0; items_list[49] = '.'; items_list[48] = '.'; items_list[46] = '.'; }

		fprintf(stderr, "%s num:   %d/%d  mode:  %d/%d  vars:  %d/%d {%s} ", ok ? " " : "!",
				queue_num, trials[ii].num, foreach_mode, trials[ii].mode, cvars, trials[ii].cvars, vars_list);
		fprintf(stderr, "  items: %d/%d {%s}", citems, trials[ii].citems, items_list);
		if (slice.initialized()) { char sz[16*3]; slice.to_string(sz, sizeof(sz)); fprintf(stderr, " slice: %s", sz); }
		if ( ! items_filename.empty()) { fprintf(stderr, " file:'%s'\n", items_filename.Value()); }
		fprintf(stderr, "\tqargs: '%s' -> '%s'\trval: %d/%d\n", trials[ii].args, pqargs, rval, trials[ii].rval);

		free(pqargs);
		free(vars_list);
		free(items_list);
	}

	/// slice tests
	static const struct {
		const char * val;
		int start; int end;
	} slices[] = {
		{ "[:]",  0, 10 },
		{ "[0]",  0, 10 },
		{ "[0:3]",  0, 10 },
		{ "[:1]",  0, 10 },
		{ "[-1]", 0, 10 },
		{ "[:-1]", 0, 10 },
		{ "[::2]", 0, 10 },
		{ "[1::2]", 0, 10 },
	};

	for (int ii = 0; ii < (int)COUNTOF(slices); ++ii) {
		qslice slice;
		slice.set(const_cast<char*>(slices[ii].val));
		fprintf(stderr, "%s : %s\t", slices[ii].val, slice.initialized() ? "init" : " no ");
		for (int ix = slices[ii].start; ix < slices[ii].end; ++ix) {
			if (slice.selected(ix, slices[ii].end)) {
				fprintf(stderr, " %d", ix);
			}
		}
		fprintf(stderr, "\n");
	}

	return (options > 1) ? 1 : 0;
}
#endif
