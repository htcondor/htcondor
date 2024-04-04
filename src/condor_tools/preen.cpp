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




/*********************************************************************
* Find files which may have been left lying around on somebody's workstation
* by condor.  Depending on command line options we may remove the file
* and/or mail appropriate authorities about it.
*********************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_config.h"

#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "condor_uid.h"
#include "directory.h"
#include "condor_qmgr.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_state.h"
#include "sig_install.h"
#include "condor_email.h"
#include "daemon.h"
#include "condor_distribution.h"
#include "basename.h" // for condor_basename
#include "link.h"
#include "shared_port_endpoint.h"
#include "file_lock.h"
#include "filename_tools.h"
#include "ipv6_hostname.h"
#include "my_popen.h"
#include "stl_string_utils.h"
#include "checkpoint_cleanup_utils.h"
#include "format_time.h"

#include <array>
#include <memory>

#include <map>
#include <sstream>

#include <filesystem>

#define PREEN_EXIT_STATUS_SUCCESS       0
#define PREEN_EXIT_STATUS_FAILURE       1
#define PREEN_EXIT_STATUS_EMAIL_FAILURE 2

State get_machine_state();

extern void		_condor_set_debug_flags( const char *strflags, int flags );

// Define this to check for memory leaks

char		*Spool;				// dir for condor job queue
std::vector<std::string> ExecuteDirs;	// dirs for execution of condor jobs
char		*Log;				// dir for condor program logs
char		*DaemonSockDir;     // dir for daemon named sockets
char        *PublicFilesWebrootDir; // dir for public input file hash links
char		*PreenAdmin;		// who to send mail to in case of trouble
char		*MyName;			// name this program was invoked by
char        *ValidSpoolFiles;   // well known files in the spool dir
char        *UserValidSpoolFiles; // user defined files in the spool dir to preserve
char        *InvalidLogFiles;   // files we know we want to delete from log
bool		MailFlag;			// true if we should send mail about problems
bool		VerboseFlag;		// true if we should produce verbose output
bool		RmFlag;				// true if we should remove extraneous files
std::vector<std::string> BadFiles;	// list of files which don't belong

// prototypes of local interest
void usage();
void init_params();
bool check_cleanup_dir();
void check_spool_dir();
void check_execute_dir();
void check_log_dir();
void rec_lock_cleanup(const char *path, int depth, bool remove_self = false);
void check_tmp_dir();
void check_daemon_sock_dir();
void bad_file( const char *, const char *, Directory &, const char * extra = NULL );
void good_file( const char *, const char * );
int send_email();
bool is_valid_shared_exe( const char *name );
bool is_ckpt_file_or_submit_digest(const char *name, JOB_ID_KEY & jid);
bool is_ccb_file( const char *name );
bool is_gangliad_file( const char *name );
bool touched_recently(char const *fname,time_t delta);
bool linked_recently(char const *fname,time_t delta);
#ifdef HAVE_HTTP_PUBLIC_FILES
	void check_public_files_webroot_dir();
#endif
std::string get_corefile_program( const char* corefile, const char* dir );

/*
  Tell folks how to use this program.
*/
void
usage()
{
	fprintf( stderr, "Usage: %s [-mail] [-remove] [-verbose] [-debug] [-log <filename>]\n", MyName );
	DC_Exit( PREEN_EXIT_STATUS_FAILURE );
}


void
main_shutdown_fast() {
	DC_Exit( 0 );
}


void
main_shutdown_graceful() {
	DC_Exit( 0 );
}

void
main_pre_dc_init( int /* argc */, char ** /* argv */ ) { }


void
main_pre_command_sock_init() { }


void
main_config() { }


int _argc;
char ** _argv;


int preen_report();

int
preen_main( int, char ** ) {
	// argc = _argc;
	char ** argv = _argv;

#ifndef WIN32
		// Ignore SIGPIPE so if we cannot connect to a daemon we do
		// not blowup with a sig 13.
    install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	// initialize the config settings
	config_ex(CONFIG_OPT_NO_EXIT);

	// Initialize things
	MyName = argv[0];
	set_priv_initialize(); // allow uid switching if root
	config();

	VerboseFlag = false;
	MailFlag = false;
	RmFlag = false;

	// Parse command line arguments
	for( argv++; *argv; argv++ ) {
		if( (*argv)[0] == '-' ) {
			switch( (*argv)[1] ) {

			  case 'd':
                dprintf_set_tool_debug("TOOL", 0);
				break;

			  case 'l':
				  if (argv[1]) {
					  dprintf_set_tool_debug_log("TOOL", 0, argv[1]);
					  ++argv;
				  } else {
					  dprintf_set_tool_debug("TOOL", 0);
				  }
				  break;

			  case 'v':
				VerboseFlag = true;
				break;

			  case 'm':
				MailFlag = true;
				break;

			  case 'r':
				RmFlag = true;
				break;

			  default:
				usage();

			}
		} else {
			usage();
		}
	}

	init_params();

	if (VerboseFlag)
	{
		// always append D_FULLDEBUG locally when verbose.
		// shouldn't matter if it already exists.
		std::string szVerbose="D_ALWAYS:2 D_PID";
		char * pval = param("TOOL_DEBUG");
		if( pval ) {
			szVerbose+=" ";
			szVerbose+=pval;
			free( pval );
		}
		_condor_set_debug_flags( szVerbose.c_str(), D_FULLDEBUG );
	}

	dprintf( D_ALWAYS, "********************************\n");
	dprintf( D_ALWAYS, "STARTING: condor_preen PID: %d\n", getpid());
	dprintf( D_ALWAYS, "********************************\n");

	// Do the file checking
	check_spool_dir();
	check_execute_dir();
	check_log_dir();
	check_daemon_sock_dir();
	check_tmp_dir();
#ifdef HAVE_HTTP_PUBLIC_FILES
	check_public_files_webroot_dir();
#endif

	// Check to see if we need to clean up checkpoint destinations.
	if(! check_cleanup_dir()) {
		DC_Exit(preen_report());
	}

	// Don't exit until check_cleanup_dir_actual() has finished.
	return 0;
}


int
preen_report() {
	// Produce output, either on stdout or by mail
	int exit_status = PREEN_EXIT_STATUS_SUCCESS;
	if( !BadFiles.empty() ) {
		// write the files we deleted to the daemon log
		for (auto& str: BadFiles) {
			dprintf(D_ALWAYS, "%s\n", str.c_str());
		}

		dprintf(D_ALWAYS, "Results: %zu file%s preened\n", BadFiles.size(), (BadFiles.size()>1) ? "s" : "");

		exit_status = send_email();
	} else {
		dprintf(D_ALWAYS, "Results: No files preened\n");
	}

	dprintf( D_ALWAYS, "********************************\n");
	dprintf( D_ALWAYS, "ENDING: condor_preen PID: %d STATUS: %d\n", getpid(), exit_status);
	dprintf( D_ALWAYS, "********************************\n");

	return exit_status;
}


void
main_init( int argc, char ** argv ) {
	main_config();

	int rv = preen_main( argc, argv );
	if( rv != 0 ) { DC_Exit( rv ); }
}


int
main( int argc, char * argv[] ) {
	set_mySubSystem( "TOOL", false, SUBSYSTEM_TYPE_TOOL );

	// This is also dumb, but less dangerous than (a) reaching into daemon
	// core to set a flag and (b) hoping that my command-line arguments and
	// its command-line arguments don't conflict.
	char ** dcArgv = (char **)malloc( 5 * sizeof( char * ) );
	dcArgv[0] = argv[0];
	// Force daemon core to run in the foreground.
	dcArgv[1] = strdup( "-f" );
	// Disable the daemon core command socket.
	dcArgv[2] = strdup( "-p" );
	dcArgv[3] = strdup(  "0" );
	dcArgv[4] = NULL;


	// This is dumb, but easier than fighting daemon core about parsing.
	_argc = argc;
	_argv = (char **)malloc( (argc + 1) * sizeof( char * ) );
	for( int i = 0; i < argc; ++i ) {
		_argv[i] = strdup( argv[i] );

		if( argv[i][0] == '-' && argv[i][1] == 'd' ) {
		    free(dcArgv[1]);
		    dcArgv[1] = strdup( "-t" );
		}
	}
	_argv[argc] = NULL;


	argc = 4;
	argv = dcArgv;

	dc_main_init = & main_init;
	dc_main_config = & main_config;
	dc_main_shutdown_fast = & main_shutdown_fast;
	dc_main_shutdown_graceful = & main_shutdown_graceful;
	dc_main_pre_dc_init = & main_pre_dc_init;
	dc_main_pre_command_sock_init = & main_pre_command_sock_init;

	return dc_main( argc, argv );
}


/*
  As the program runs, we create a list of messages regarding the status
  of file in the list BadFiles.  Now we use that to produce all output.
  If MailFlag is set, we send the output to the condor administrators
  via mail, otherwise we just print it on stdout.
*/
int
send_email()
{
	FILE	*mailer;
	std::string subject,szTmp;
	formatstr(subject, "condor_preen results %s: %zu old file%s found",
		get_local_fqdn().c_str(), BadFiles.size(),
		(BadFiles.size() > 1)?"s":"");

	if( MailFlag ) {
		if( (mailer=email_nonjob_open(PreenAdmin, subject.c_str())) == NULL ) {
			dprintf(D_ERROR, "Can't do email_open(\"%s\", \"%s\")\n",PreenAdmin,subject.c_str());
		#ifdef WIN32
			if ( ! param_defined("SMTP_SERVER")) {
				dprintf(D_ALWAYS, "SMTP_SERVER not configured\n");
			}
		#endif
			return PREEN_EXIT_STATUS_EMAIL_FAILURE;
		}
	} else {
		mailer = stdout;
	}

	formatstr(szTmp, "The condor_preen process has found the following stale condor files on <%s>:\n\n",  get_local_hostname().c_str());
	dprintf(D_ALWAYS, "%s", szTmp.c_str());

	if( MailFlag ) {
		fprintf( mailer, "\n" );
		fprintf( mailer, "%s", szTmp.c_str());
	}

	for (auto& str: BadFiles) {
		formatstr(szTmp, "  %s\n", str.c_str());
		fprintf( mailer, "%s", szTmp.c_str() );
	}

	if( MailFlag ) {
		const char *explanation = "\n\nWhat is condor_preen?\n\n"
"The condor_preen tool examines the directories belonging to Condor, and\n"
"removes extraneous files and directories which may be left over from Condor\n"
"processes which terminated abnormally either due to internal errors or a\n"
"system crash.  The directories checked are the LOG, EXECUTE, and SPOOL\n"
"directories as defined in the Condor configuration files.  The condor_preen\n"
"tool is intended to be run as user root (or user condor) periodically as a\n"
"backup method to ensure reasonable file system cleanliness in the face of\n"
"errors. This is done automatically by default by the condor_master daemon.\n"
"It may also be explicitly invoked on an as needed basis.\n\n"
"See the Condor manual section on condor_preen for more details.\n";

		fprintf( mailer, "%s\n", explanation );
		email_close( mailer );
	}

	return PREEN_EXIT_STATUS_SUCCESS;
}

// return true if this directory name could be a clusterid mode 10000 or procid mod 10000
static bool is_job_subdir(const char * child) {
	char *end=NULL;
	long l = strtol(child,&end,10);
	if (l == LONG_MIN || !end || *end != '\0') {
		return false; // not part of the job spool hierarchy
	}
	return true;
}

// class used to hold/manage a list of 'maybe stale' files by jobid that are
// valid only when the schedd has active jobs with corresponding job ids
class JobIdSpoolFiles {
public:
	std::deque<std::string> files;
	std::map<JOB_ID_KEY, std::vector<int>> jid_to_files;

	// make it convenient to iterate the files collection
	std::deque<std::string>::iterator begin() { return files.begin(); }
	std::deque<std::string>::iterator end() { return files.end(); }

	bool empty() const { return jid_to_files.empty(); }
	int  size() const { return (int)jid_to_files.size(); }

	// append a filename, and add the index of that file to the jid_to_files map at the given key
	void add(const JOB_ID_KEY & jid, const std::string & fn) {
		int ix = (int)files.size();
		jid_to_files[jid].push_back(ix);
		files.push_back(fn);
	}
	int first_cluster_id() {
		if (jid_to_files.empty()) { return 0; }
		return jid_to_files.begin()->first.cluster;
	}
	int last_cluster_id() {
		if (jid_to_files.empty()) { return 0; }
		return jid_to_files.rbegin()->first.cluster;
	}
	// clear all files corresponding to a given job id, then remove the job id.
	void clear(const JOB_ID_KEY & jid) {
		auto found = jid_to_files.find(jid);
		if (found != jid_to_files.end()) {
			for (auto it = found->second.begin(); it != found->second.end(); ++it) {
				files[*it].clear(); // clear string, can't delete the entry because find items by index
			}
			jid_to_files.erase(found);
		}
		// if we have a prod id, also clear cluster specific jobs for that cluster
		if (jid.proc >= 0) {
			JOB_ID_KEY cid(jid.cluster, -1);
			clear(cid);
		}
	}
};

/*
  Check the condor spool directory for extraneous files.  Files found
  there should either be on a list of well known files, or should be
  checkpoint files belonging to jobs currently in the job queue.  We
  create a global list of all jobs in the queue for use by lower level
  routines which will check actual files aginst the job queue to see
  if they are legitimate checkpoints.
*/
void
check_spool_dir()
{
	const char  	*f;
	Directory  		dir(Spool, PRIV_ROOT);
	StringList 		well_known_list;
	JobIdSpoolFiles maybe_stale;
	std::string tmpstr;

	if ( ValidSpoolFiles == NULL ) {
		dprintf( D_ALWAYS, "Not cleaning spool directory: VALID_SPOOL_FILES not configured\n");
		return;
	}

	//List of known history like config knobs to not delete if in spool
	static const std::string valid_knobs[] = {
		"HISTORY",
		"JOB_EPOCH_HISTORY",
		"STARTD_HISTORY",
		"COLLECTOR_PERSISTENT_AD_LOG",
		"LVM_BACKING_FILE",
	};
	//Param the knobs for the file name and add to data structure
	std::deque<std::string> config_defined_files;
	for(const auto &knob : valid_knobs) {
		auto_free_ptr option(param(knob.c_str()));
		if (option) { config_defined_files.push_back(condor_basename(option)); }
	}

	well_known_list.initializeFromString (ValidSpoolFiles);
	if (UserValidSpoolFiles) {
		StringList tmp(UserValidSpoolFiles);
		well_known_list.create_union(tmp, false);
	}
		// add some reasonable defaults that we never want to remove
	static const char* valid_list[] = {
		"job_queue.log",
		"job_queue.log.tmp",
		"spool_version",
		"Accountant.log",
		"Accountantnew.log",
		"local_univ_execute",
		"EventdShutdownRate.log",
		"*OfflineLog*",
		"*OfflineAds*",
		// SCHEDD.lock: High availability lock file.  Current
		// manual recommends putting it in the spool, so avoid it.
		"SCHEDD.lock",
		"lost+found",
		};

	for (auto & ix : valid_list) {
		if ( ! well_known_list.contains(ix)) { 
			well_known_list.append(ix);
		}
	}

		// Step 1: Check each file in the directory. Look for files that
		// obviously should be here (job queue logs, shared exes, etc.) and
		// flag them as good files. Put everything else into stale_spool_files,
		// which we'll deal with later.
	while((f = dir.Next()) ) {
			// see if it's on the list
		if( well_known_list.contains_withwildcard(f) ) {
			good_file( Spool, f );
			continue;
		}
		if( !strncmp(f,"job_queue.log",13) ) {
			// Historical job queue log files have names like: job_queue.log.3
			// In theory, we could clean up any such files that are not
			// in the range allowed by SCHEDD_MAX_HISTORICAL_LOGS, but
			// the sequence number keeps increasing each time a new
			// one is added, so we would have to find out what the latest
			// sequence number is in order to know what is safe to delete.
			// Therefore, this issue is ignored.  We rely on the schedd
			// to clean up after itself.
			good_file( Spool, f );
			continue;
		}

		//Check to see if file is a valid config defined file
		bool isValidCondorFile = false;
		for (const auto &file : config_defined_files) {
			if (strncmp(f, file.c_str(), file.length()) == MATCH) {
				isValidCondorFile = true;
				break;
			}
		}
		//If we found a valid config defined file then mark this as good
		if (isValidCondorFile) {
			good_file( Spool, f );
			continue;
		}

			// see it it's an in-use shared executable
		if( is_valid_shared_exe(f) ) {
			good_file( Spool, f );
			continue;
		}

			// See if it's a CCB server file
		if ( is_ccb_file( f ) ) {
			good_file( Spool, f );
			continue;
		}

			// See if it's a GangliaD reset metrics file
		if ( is_gangliad_file( f ) ) {
			good_file( Spool, f );
			continue;
		}

		// if the file is a directory, look into it.
		if (dir.IsDirectory() && ! dir.IsSymlink()) {

			// we will set is_good to true if this is a directory that has a possibly valid file or subdir
			// we can't ever delete those that *might* be good because they can never become completely stale
			// so we mark them as definitely good so long as they are non-empty. this affects verbose logging
			// and mot much else
			bool is_good = false;

			// We expect directories of the form produced by SpooledJobFiles::getJobSpoolPath()
			// and GetSpooledExecutablePath().
			// e.g. $(SPOOL)/<cluster mod 10000>/<proc mod 10000>/cluster<cluster>.proc<proc>.subproc<proc>/<jobfiles>
			// or $(SPOOL)/<cluster mod 10000>/cluster<cluster>.ickpt.subproc<subproc>
			// or $(SPOOL)/<cluster mod 10000>/condor_submit.<cluster>.*   (late materialization digest and itemdata)
			if (is_job_subdir(f)) {
				const char * clusterdir = dir.GetFullPath();
				Directory dir2(clusterdir,PRIV_ROOT);
				const char *fn2;
				JOB_ID_KEY jid;

				while ((fn2 = dir2.Next())) {
					// does it match the pattern of a <proc mod 10000> directory?
					// if it does, then we need to check it for valid spool files
					if (is_job_subdir(fn2)) {
						// if it is a directory, check it for spooled job files
						if (dir2.IsDirectory() && ! dir2.IsSymlink())  {
							is_good = true;
							const char * procdir = dir2.GetFullPath();
							Directory dir3(procdir,PRIV_ROOT);
							const char *fn3;
							while ((fn3 = dir3.Next())) {
								if (is_ckpt_file_or_submit_digest(fn3, jid)) {
									// put it in the list of files/dirs needing jobid checks
									// directories for spooled job files will end up here
									formatstr(tmpstr,"%s%c%s%c%s",f,DIR_DELIM_CHAR,fn2,DIR_DELIM_CHAR,fn3);
									maybe_stale.add(jid, tmpstr);
								} else {
									// not a valid pattern, we can delete this one now
									bad_file(procdir, fn3, dir3);
								}
							}
						} else {
							// matches the pattern of a proc dir, but it is not a dir so it's invalid
							bad_file(clusterdir, fn2, dir2);
						}
					} else if (is_ckpt_file_or_submit_digest(fn2, jid)) {
						// put it in the list of files needing jobid checks
						formatstr(tmpstr,"%s%c%s",f,DIR_DELIM_CHAR,fn2);
						maybe_stale.add(jid, tmpstr);
						is_good = true;
					} else {
						// file is not a submit_digest, checkpoint, or proc subdir. it is invalid
						// we can delete it now.
						bad_file(clusterdir, fn2, dir2);
					}
				}
			} else {
				// file is not of a valid pattern, delete it now
				bad_file(Spool, f, dir);
			}

			// NOTE: anything that matches the pattern *might* be a valid future clusterdir even if it
			// is no longer a valid past clusterdir, so we can't ever delete it while the schedd is alive.
			if (is_good) {
				good_file(Spool, f);
			}

		} else {
			// file was not a directory, we can deal with it right now.
			JOB_ID_KEY jid;
			if (is_ckpt_file_or_submit_digest(f, jid)) {
				maybe_stale.add(jid, f);
			} else {
				// not a directory, and also clearly not a checkpoint or submit file
				// so we can delete it now
				bad_file(Spool, f, dir);
			}
		}
	}

	// We now (may) have a list of files that have valid naming patterns, but might be stale
	// if they refer to jobs no longer in the schedd.  So we need to contact the schedd to find out.
	if ( ! maybe_stale.empty()) {

		bool can_delete_stale_files = false; // assume we will not be able to contact the schedd
		dprintf(D_ALWAYS, "Found files for %d jobs. Asking schedd if the jobs are still present\n", maybe_stale.size());

		// Establish (or re-establish) schedd connection
		Daemon schedd(DT_SCHEDD, NULL);
		if ( ! schedd.locate()) {
			// schedd.locate() will dprintf an error if it does not succeed
			// dprintf(D_ALWAYS, "Can't find address of local Schedd\n");
		} else {
			CondorError  errstack;
			int cmd = QUERY_JOB_ADS;
			Sock *sock = schedd.startCommand(cmd, Stream::reli_sock, 0, &errstack);
			if ( ! sock) {
				dprintf(D_ALWAYS, "Can't connect to schedd: %s\n", errstack.getFullText().c_str() );
			} else {

				// build a query ad
				ClassAd ad;
				ad.Assign(ATTR_PROJECTION, ATTR_CLUSTER_ID "," ATTR_PROC_ID);
				int firstid = maybe_stale.first_cluster_id();
				int lastid = maybe_stale.last_cluster_id();
				if (firstid == lastid) {
					formatstr(tmpstr, ATTR_CLUSTER_ID "==%d", firstid);
				} else {
					formatstr(tmpstr, ATTR_CLUSTER_ID ">=%d && " ATTR_CLUSTER_ID "<=%d", firstid, lastid);
				}
				ad.AssignExpr(ATTR_REQUIREMENTS, tmpstr.c_str());
				ad.Assign(ATTR_QUERY_Q_INCLUDE_CLUSTER_AD, true);

				if ( ! putClassAd(sock, ad) || ! sock->end_of_message()) {
					dprintf(D_ALWAYS, "Error, schedd communication error\n");
				} else {
					ad.Clear();
					while (getClassAd(sock, ad) && sock->end_of_message()) {
						// we succeedd in contacting the schedd, and we got a reply
						// so we can safely delete anything left in the maybe_stale collection
						// once we are done with this loop.
						can_delete_stale_files = true;

						// the end of the jobs collection is indicated by a classad with Owner==0
						long long intVal;
						if (ad.EvaluateAttrInt(ATTR_OWNER, intVal) && (intVal == 0)) { // last ad is not a real job ad
							break;
						}

						// construct a job id key, and clear 'maybe stale' files that match the key
						// this also removes the key from the maybe_stale collection.
						JOB_ID_KEY jid;
						if (ad.EvaluateAttrInt(ATTR_CLUSTER_ID, jid.cluster)) {
							jid.proc = -1;
							ad.EvaluateAttrInt(ATTR_PROC_ID, jid.proc);
							maybe_stale.clear(jid);
						}

						ad.Clear(); // clear the ad for the next loop iteration.
					}
				}

				sock->close();
				delete sock;
			}
		}

		if (can_delete_stale_files) {
			if (maybe_stale.size() > 0) {
				dprintf(D_ALWAYS, "Deleting files for %d jobs no longer in the local schedd\n", maybe_stale.size());
				// we can now delete all of the non-empty filenames in the maybe_stale collection
				for (auto it = maybe_stale.begin(); it != maybe_stale.end(); ++it) {
					if (it->empty()) continue; // files that were discovered to be not-stale were cleared.
					bad_file(Spool, it->c_str(), dir);
				}
			} else {
				dprintf(D_ALWAYS, "All job id's are still current\n");
			}
		} else {
			dprintf(D_ALWAYS, "Assuming all job id's are still current\n");
		}
	}
}

/*
*/
bool
is_valid_shared_exe( const char *name )
{
	if ((strlen(name) < 4) || (strncmp(name, "exe-", 4) != 0)) {
		return false;
	}
	std::string path;
	formatstr(path, "%s/%s", Spool, name);
	int count = link_count(path.c_str());
	if (count == 1) {
		return false;
	}
	if (count == -1) {
		dprintf(D_ALWAYS, "link_count error on %s; not deleting\n", name);
	}
	return true;
}


/*
Grab an integer value which has been embedded in a file name.  We're given
a name to examine, and a pattern to search for.  If the pattern exists,
then the number we want follows it immediately.  If the pattern doesn't
exist, we return -1.
*/
inline int
grab_val( const char *str, const char *pattern )
{
	char const *ptr;

	if( (ptr = strstr(str,pattern)) ) {
		return atoi(ptr + strlen(pattern) );
	}
	return -1;
}

/*
Given the name of a file in the spool directory, return true if has the pattern
of a legitimate checkpoint file or submit digest, and false otherwise.  If the name starts
with "cluster", it should be a V3 style checkpoint. If it starts with condor_submit
it's a submit digest, Otherwise it is either a V2 style checkpoint, or not a checkpoint at all.
if true is returned, then the ID of the job (or of the cluster) that must exist for
the file to be still valid is also returned.
*/
bool
is_ckpt_file_or_submit_digest(const char *name, JOB_ID_KEY & jid)
{
	if (name[0] == 'c') { // might start with 'cluster' or 'condor_submit'
		if( name[1] == 'l' ) { // might start with 'cluster'
			// V3 checkpoint file formats are:
			// cluster<#>.ickpt.subproc<#>		- initial checkpoint file
			// 	cluster<#>.proc<#>.subproc<#>		- specific checkpoint file
			jid.cluster = grab_val(name, "cluster");
			jid.proc = grab_val(name, ".proc");
			return jid.cluster > 0;
		} else if ( name[1] == 'o' ) { // might start with 'condor_submit'
			jid.cluster = grab_val( name, "condor_submit." );
			jid.proc = -1;
			return jid.cluster > 0;
		}
	} else if ( name[0] == 'j') { // might start with 'job'
		// V2 checkpoint files formats are:
		// job<#>.ickpt		- initial checkpoint file
		// job<#>.ckpt.<#>	- specific checkpoint file

		jid.cluster = grab_val(name, "job");
		jid.proc = grab_val(name, ".ckpt.");
		return jid.cluster > 0;
	} else if (name[0] == '_') { // might start with '_condor_creds'
		jid.cluster = grab_val(name, "_condor_creds.cluster");
		jid.proc = grab_val(name, ".proc");
		return jid.cluster > 0;
	}
	return false;
}


/*
  Check whether the given file is a CCB reconnect file
*/
bool
is_ccb_file( const char *name )
{
	if( strstr(name,".ccb_reconnect") ) {
		return true;
	}
	return false;
}

/*
  Check whether the given file is a GandliaD metrics file
*/
bool
is_gangliad_file( const char *name )
{
	if( strstr(name,".ganglia_metrics") ) {
		return true;
	}
	return false;
}



/*
  Scan the execute directory looking for bogus files.
*/
void
check_execute_dir()
{
		// The plugin tests create a temporary directory to hold the
		// test file outputs.  Since there's no easily predictable pattern,
		// as to whether it's in use, we assume that anything less than 30
		// minutes is in use.
	time_t now = time(NULL) - 30 * 60;
	const char	*f;
	bool	busy;
	State	s = get_machine_state();

		// If we know the state of the machine, we can use a simple
		// algorithm.  If we are hosting a job - anything goes,
		// otherwise the execute directory should be empty.  If we
		// can't find the state, just leave the execute directory
		// alone.  -Derek Wright 4/2/99
	switch( s ) {
	case owner_state:
	case unclaimed_state:
	case matched_state:
		busy = false;
		break;
	case claimed_state:
	case preempting_state:
		busy = true;
		break;
	default:
		dprintf( D_ALWAYS,
				 "Error getting startd state, not cleaning execute directory.\n" );
		return;
	}

	for (const auto& Execute: ExecuteDirs) {
		Directory dir( Execute.c_str(), PRIV_ROOT );
		while( (f = dir.Next()) ) {
			if( busy ) {
				good_file( Execute.c_str(), f );	// let anything go here
			} else {
				if( dir.GetCreateTime() < now ) {
					bad_file( Execute.c_str(), f, dir ); // junk it
				}
				else {
						// In case the startd just started up a job, we
						// avoid removing very recently created files.
						// (So any files forward dated in time will have
						// to wait for the startd to restart before they
						// are cleaned up.)
					dprintf(D_FULLDEBUG, "In %s, found %s with recent "
					        "creation time.  Not removing.\n", Execute.c_str(), f );
					good_file( Execute.c_str(), f ); // too young to kill
				}
			}
		}
	}
}

/*
  Scan the log directory for bogus files.  All files in the log directory
  should have well known names.
*/
void
check_log_dir()
{
	const char	*f;
	Directory dir(Log, PRIV_ROOT);
	long long coreFileMaxSize;
	param_longlong("PREEN_COREFILE_MAX_SIZE", coreFileMaxSize, true, 50'000'000);
	int coreFileStaleAge = param_integer("PREEN_COREFILE_STALE_AGE", 5'184'000);
	unsigned int coreFilesPerProgram = param_integer("PREEN_COREFILES_PER_PROCESS", 10);
	//Max Disk space daemon type core files can take up (schedd:5GB can have files 1GB 1GB 3GB)
	long long scheddCoresMaxSum, negotiatorCoresMaxSum, collectorCoresMaxSum;
	param_longlong("PREEN_SCHEDD_COREFILES_TOTAL_DISK", scheddCoresMaxSum, true, 4 * coreFileMaxSize);
	param_longlong("PREEN_NEGOTIATOR_COREFILES_TOTAL_DISK", negotiatorCoresMaxSum, true, 4 * coreFileMaxSize);
	param_longlong("PREEN_COLLECTOR_COREFILES_TOTAL_DISK", collectorCoresMaxSum, true, 4 * coreFileMaxSize);
	std::vector<std::string> invalid;
	std::map<std::string, std::map<int, std::string>> programCoreFiles;
	//Corefiles for daemons with large base sizes (schedd, negotiator, collector)
	std::map<std::string, std::map<time_t, std::pair<std::string, filesize_t>>> largeCoreFiles;

	invalid = split(InvalidLogFiles ? InvalidLogFiles : "");

	while( (f = dir.Next()) ) {
		if( contains(invalid, f) ) {
			bad_file( Log, f, dir );
		}
		#ifndef WIN32
			else {
				// Check if this is a core file
				const char* coreFile = strstr( f, "core." );
				if ( coreFile ) {
					StatInfo statinfo( Log, f );
					if( statinfo.Error() == 0 ) {
						std::string daemonExe = get_corefile_program( f, dir.GetDirectoryPath() );
						// If this core file is stale, flag it for removal
						if( abs((int)( time(NULL) - statinfo.GetModifyTime() )) > coreFileStaleAge ) {
							std::string coreFileDetails;
							formatstr( coreFileDetails, "file: %s, modify time: %s, size: %zd",
								daemonExe.c_str(), format_date_year(statinfo.GetModifyTime()), (ssize_t)statinfo.GetFileSize()
							);
							bad_file( Log, f, dir, coreFileDetails.c_str() );
							continue;
						}
						// If this core file exceeds a certain size, flag for removal
						if( statinfo.GetFileSize() > coreFileMaxSize ) {
							//If core file belongs to schedd, negotiator, or collector daemon then
							//add to data struct for later processing else flag for removal
							if (daemonExe.find("condor_schedd") != std::string::npos ||
								daemonExe.find("condor_negotiator") != std::string::npos ||
								daemonExe.find("condor_collector") != std::string::npos) {
									largeCoreFiles[condor_basename(daemonExe.c_str())].insert(std::make_pair(statinfo.GetModifyTime(),
															std::pair<std::string,filesize_t>(std::string(f),statinfo.GetFileSize())));
							} else {
								std::string coreFileDetails;
								formatstr( coreFileDetails, "file: %s, modify time: %s, size: %zd",
									daemonExe.c_str(), format_date_year(statinfo.GetModifyTime()), (ssize_t)statinfo.GetFileSize()
								);
								bad_file( Log, f, dir, coreFileDetails.c_str() );
							}
							continue;
						}
					}
					// If we couldn't stat the file, ignore it and move on
					else {
						continue;
					}

					// Add this core file plus its timestamp to a data structure linking it to its process
					std::string program = get_corefile_program( f, dir.GetDirectoryPath() );
					if( program != "" ) {
						programCoreFiles[program].insert( std::make_pair( statinfo.GetModifyTime(), std::string( f ) ) );
					}
				}
				// If not a core file, assume it's good
				else {
					good_file( Log, f );
				}
			}
		#else
			else {
				good_file( Log, f );
			}
		#endif
	}

	/*
	*	Now iterate over each daemons core files. Remove oldests files if
	*	needed until under alotted disk disk space/max sum limit.
	*	Because std::map sorts alphabetically by key, and the timestamp
	*	is the key so the inner map goes from oldest to newest.
	*/
	for (auto& d : largeCoreFiles) {
		//Set maximum core file sizes sum to daemons limit
		long long maxSum = 0;
		if (d.first.find("condor_schedd") != std::string::npos) { maxSum = scheddCoresMaxSum; }
		else if (d.first.find("condor_negotiator") != std::string::npos) { maxSum = negotiatorCoresMaxSum; }
		else if (d.first.find("condor_collector") != std::string::npos) { maxSum = collectorCoresMaxSum; }

		//Reverse through inner map contents to see how many files from newest to
		//oldest we can sum up before we pass the maxiumum sum. We will attempt
		//to fit as many core files as we can. Mark good/bad file as we go.
		filesize_t fileSum = 0;
		for (auto it = d.second.rbegin(); it != d.second.rend(); it++) {
			if (fileSum + it->second.second > maxSum) {
				bad_file(Log, it->second.first.c_str(), dir);
				double fileSize = static_cast<double>(it->second.second)/1024/1024/1024;
				double availSpace = static_cast<double>(maxSum-fileSum)/1024/1024/1024;
				double maxSpace = static_cast<double>(maxSum)/1024/1024/1024;
				dprintf(D_ALWAYS, "Marking %s as bad file due to size %.2f GB exceeding the remaining available space of %.2f GB out of a %.2f GB max.\n",
							it->second.first.c_str(), fileSize, availSpace, maxSpace);
			} else {
				fileSum += it->second.second;
				good_file(Log, it->second.first.c_str());
			}
		}
	}

	// Now iterate over the processes we tracked core files for.
	// Keep the 10 most recent core files for each, remove anything older.
	// Because std::map sorts alphabetically by key, and the timestamp is the
	// key, the last 10 entries in the map are the most recent.
	for( auto ps = programCoreFiles.begin(); ps != programCoreFiles.end(); ++ps ) {
		unsigned int index = 0;
		for( auto core = ps->second.begin(); core != ps->second.end(); ++core ) {
			if( ( ps->second.size() > coreFilesPerProgram ) && ( index < ( ps->second.size() - coreFilesPerProgram ) ) ) {
				bad_file( Log, core->second.c_str(), dir );
			}
			else {
				good_file( Log, core->second.c_str() );
			}
			index++;
		}
	}
}

void
check_daemon_sock_dir()
{
	const char	*f;
	Directory dir(DaemonSockDir, PRIV_ROOT);

	time_t stale_age = SharedPortEndpoint::TouchSocketInterval()*100;

	while( (f = dir.Next()) ) {
		std::string full_path;
		formatstr(full_path,"%s%c%s",DaemonSockDir,DIR_DELIM_CHAR,f);

			// daemon sockets are touched periodically to mark them as
			// still in use
		if( touched_recently( full_path.c_str(), stale_age ) ) {
			good_file( DaemonSockDir, f );
		}
		else {
			bad_file( DaemonSockDir, f, dir );
		}
	}
}

/*
  Scan the webroot directory used for public input files. Look for .access files
  more than a week old, and remove their respective hard links (same filename
  minus the .access extension)
*/
#ifdef HAVE_HTTP_PUBLIC_FILES
void
check_public_files_webroot_dir()
{
	// Make sure that PublicFilesWebrootDir is actually set before proceeding!
	// If not set, just ignore it and bail out here.
	if( !PublicFilesWebrootDir ) {
		return;
	}

	const char *filename;
	Directory dir(PublicFilesWebrootDir, PRIV_ROOT);
	FileLock *accessFileLock;
	std::string accessFilePath;
	std::string hardLinkName;
	std::string hardLinkPath;

	// Set the stale age for a file to be one week
	time_t stale_age = param_integer( "HTTP_PUBLIC_FILES_STALE_AGE", 604800 );

	while( ( filename = dir.Next() ) ) {
		if( strstr( filename, ".access" ) ) {
			accessFilePath = PublicFilesWebrootDir;
			accessFilePath += DIR_DELIM_CHAR;
			accessFilePath += filename;

			// Try to obtain a lock for the access file. If this fails for any
			// reason, just bail out and move on.
			accessFileLock = new FileLock( accessFilePath.c_str(), true, false );
			if( !accessFileLock->obtain( WRITE_LOCK ) ) {
				dprintf( D_ALWAYS, "check_public_files_webroot_dir: Failed to "
					"obtain lock on %s, ignoring file\n", accessFilePath.c_str() );
				continue;
			}

			hardLinkPath = accessFilePath.substr( 0, accessFilePath.length()-7 );
			hardLinkName = hardLinkPath.substr( hardLinkPath.find_last_of( DIR_DELIM_CHAR ) );

			// If the access file is stale, unlink both that and the hard link.
			if( !linked_recently( accessFilePath.c_str(), stale_age ) ) {
				// Something is weird here. I'm sending only the filename
				// of the access file, but the full path of the hard link, and
				// it works correctly??
				bad_file( PublicFilesWebrootDir, filename, dir );
				bad_file( PublicFilesWebrootDir, hardLinkPath.c_str(), dir );
			}
			else {
				good_file( PublicFilesWebrootDir, filename );
				good_file( PublicFilesWebrootDir, hardLinkPath.c_str() );
			}

			// Release the lock before moving on
			accessFileLock->release();
		}
	}
}
#endif

void rec_lock_cleanup(const char *path, int depth, bool remove_self) {
#if !defined(WIN32)
	FileLock *lock = NULL;
	if (depth == 0) {
		lock = new FileLock(path, true, true);
		delete lock;
		return ;
	}
	Directory *dir = new Directory(path);
	if (dir == NULL) {
		// that may be ok as the path could already be cleaned up.
		return;
	}
	const char *entry;
	while ((entry = dir->Next()) != 0) {
		if (!dir->IsDirectory() && depth > 1) { // clean up files floating around randomly -- maybe from older releases
			lock = new FileLock(path, false, true);
			bool result = lock->obtain(WRITE_LOCK);
			if (!result) {
					dprintf(D_FULLDEBUG, "Cannot lock %s\n", path);
			}
			int res = unlink(dir->GetFullPath());
			if (res != 0) {
				dprintf(D_FULLDEBUG, "Cannot delete %s (%s) \n", path, strerror(errno));
			}
			delete lock;
		} else {
			rec_lock_cleanup(dir->GetFullPath(), depth-1, true);
		}
	}
	// make sure, orphaned directories will be deleted as well.
	if (remove_self) {
		int res = rmdir(path);
		if (res != 0) {
			dprintf(D_FULLDEBUG, "Directory %s could not be removed.\n", path);
		}
	}

	delete dir;
#endif
}

void check_tmp_dir(){
#if !defined(WIN32)
	if (!RmFlag) return;

	bool newLock = param_boolean("CREATE_LOCKS_ON_LOCAL_DISK", true);
	if (newLock) {
		// get temp path for file locking from the FileLock class
		std::string tmpDir;
		FileLock::getTempPath(tmpDir);
		rec_lock_cleanup(tmpDir.c_str(), 3);
	}

#endif
}


/*
  Initialize information from the configuration files.
*/
void
init_params()
{
	Spool = param("SPOOL");
    if( Spool == NULL ) {
        EXCEPT( "SPOOL not specified in config file" );
    }

	Log = param("LOG");
    if( Log == NULL ) {
        EXCEPT( "LOG not specified in config file" );
    }

	DaemonSockDir = param("DAEMON_SOCKET_DIR");
	if( DaemonSockDir == NULL ) {
		EXCEPT("DAEMON_SOCKET_DIR not defined");
	}

	// PublicFilesWebrootDir is optional, may not be set
	PublicFilesWebrootDir = param("HTTP_PUBLIC_FILES_ROOT_DIR");

	char *Execute = param("EXECUTE");
	if( Execute ) {
		ExecuteDirs.emplace_back(Execute);
		free(Execute);
		Execute = NULL;
	}
		// In addition to EXECUTE, there may be SLOT1_EXECUTE, ...
	std::vector<std::string> params;
	Regex re; int errnumber = 0, erroffset = 0;
	ASSERT(re.compile("slot([0-9]*)_execute", &errnumber, &erroffset, PCRE2_CASELESS));
	if (param_names_matching(re, params)) {
		for (size_t ii = 0; ii < params.size(); ++ii) {
			Execute = param(params[ii].c_str());
			if (Execute) {
				if ( ! contains(ExecuteDirs, Execute)) {
					ExecuteDirs.emplace_back(Execute);
				}
				free(Execute);
			}
		}
	}

	if ( MailFlag ) {
		if( (PreenAdmin = param("PREEN_ADMIN")) == NULL ) {
			if( (PreenAdmin = param("CONDOR_ADMIN")) == NULL ) {
				EXCEPT( "CONDOR_ADMIN not specified in config file" );
			}
		}
	}

	// in 8.1.5, the param VALID_SPOOL_FILES was redefied to be only the user additions to the list of valid files
	UserValidSpoolFiles = param("VALID_SPOOL_FILES");
	// SYSTEM_VALID_SPOOL_FILES is the set of files known by HTCondor at compile time. It should not be overidden by the user.
	ValidSpoolFiles = param("SYSTEM_VALID_SPOOL_FILES");

	InvalidLogFiles = param("INVALID_LOG_FILES");
}


/*
  Produce output for a file that has been determined to be legitimate.
*/
void
good_file( const char *dir, const char *name )
{
	if( VerboseFlag ) {
		printf( "%s%c%s - OK\n", dir, DIR_DELIM_CHAR, name );
		dprintf( D_ALWAYS, "%s%c%s - OK\n", dir, DIR_DELIM_CHAR, name );
	}
}

/*
  Produce output for a file that has been determined to be bogus.  We also
  remove the file here if that has been called for in our command line
  arguments.
*/
void
bad_file( const char *dirpath, const char *name, Directory & dir, const char * extra )
{
	std::string pathname;
	std::string buf;

	if( !fullpath( name ) ) {
		formatstr( pathname, "%s%c%s", dirpath, DIR_DELIM_CHAR, name );
	}
	else {
		pathname = name;
	}

	if( VerboseFlag ) {
		printf( "%s - BAD\n", pathname.c_str() );
		dprintf( D_ALWAYS, "%s - BAD\n", pathname.c_str() );
	}

	if( RmFlag ) {
		bool removed = dir.Remove_Full_Path( pathname.c_str() );
		if( removed ) {
			formatstr( buf, "%s - Removed", pathname.c_str() );
		} else {
			formatstr( buf, "%s - Can't Remove", pathname.c_str() );
		}
	} else {
		formatstr( buf, "%s - Not Removed", pathname.c_str() );
	}

	if( extra != NULL ) {
		formatstr_cat( buf, " - %s", extra );
	}
	BadFiles.emplace_back( buf );
}


// Returns the state of the local startd
State
get_machine_state()
{
	char* state_str = NULL;
	State s;

	ReliSock* sock;
	Daemon my_startd( DT_STARTD );
	if( ! my_startd.locate() ) {
		dprintf( D_ALWAYS, "Can't find local startd address.\n" );
		return _error_state_;
	}

	if( !(sock = (ReliSock*)
		  my_startd.startCommand(GIVE_STATE, Stream::reli_sock, 0)) ) {
		dprintf( D_ALWAYS, "Can't connect to startd at %s\n",
				 my_startd.addr() );
		return _error_state_;
	}

	sock->end_of_message();
	sock->decode();
	if( !sock->code( state_str ) || !sock->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't read state/eom from startd.\n" );
		free(state_str);
		return _error_state_;
	}

	sock->close();
	delete sock;

	s = string_to_state( state_str );
	free(state_str);
	return s;
}

bool
touched_recently(char const *fname,time_t delta)
{
	StatInfo statinfo(fname);
	if( statinfo.Error() != 0 ) {
		return false;
	}
		// extend the window of what it means to have been touched "recently"
		// both forwards and backwards in time to handle system clock jumps.
	if( abs((int)(time(NULL)-statinfo.GetModifyTime())) > delta ) {
		return false;
	}
	return true;
}

/*
	Similar to touched_recently, but uses lstat instead of regular stat to look
	at the link inodes directly (instead of the inodes they target)
*/
#ifdef HAVE_HTTP_PUBLIC_FILES
bool
linked_recently(char const *fname, time_t delta)
{
	struct stat fileStat;
	if( lstat( fname, &fileStat ) != 0) {
		dprintf(D_ALWAYS, "preen.cpp: Failed to open link %s (errno %d)\n", fname, errno);
		return false;
	}
		// extend the window of what it means to have been touched "recently"
		// both forwards and backwards in time to handle system clock jumps.
	if( abs((int)(time(NULL) - fileStat.st_mtime)) > delta ) {
		return false;
	}
	return true;
}
#endif

// Use the 'file' command to determine which program created a core file.
// If anything goes wrong in here, return an empty string.
// Only supported for Linux. Windows will always return an empty string.
std::string
get_corefile_program( const char* corefile, const char* dir ) {

	std::string program = "";

	#ifndef WIN32
		// Assemble the "file /path/to/corefile" system command and call it
		std::string corepath = dir;
		corepath += DIR_DELIM_CHAR;
		corepath += corefile;

		// Prepare arguments
		ArgList args;
		args.AppendArg("file");
		args.AppendArg(corepath);

		std::array<char, 128> buffer;
		std::string cmd_output;
		std::unique_ptr<FILE, int (*)(FILE *)> process_pipe( my_popen( args, "r", 0 ), my_pclose );

		// Run the file command and capture output.
		// On any error, return an empty string.
		if ( !process_pipe )
			return "";
		while ( !feof( process_pipe.get() ) ) {
			if ( fgets( buffer.data(), 128, process_pipe.get() ) != NULL ) {
				cmd_output += buffer.data();
			}
		}

		// Parse the output, look for the "execfn:" token
		std::istringstream is( cmd_output );
		std::string token;
		while( getline( is, token, ' ' ) ) {
			if( token == "execfn:" ) {
				// Next token is the program binary that we want
				// If the output here is malformed in any way, just bail out
				// and return an empty string.
				getline( is, token, ' ' );
				if( token.find_last_of( "'" ) != std::string::npos ) {
					program = token.substr( 1, token.find_last_of( "'" )-1 );
				}
				break;
			}
		}
	#endif

	return program;
}


//
// Checkpoint clean-up.
//

#include "dc_coroutines.h"
using namespace condor;


dc::void_coroutine
check_cleanup_dir_actual( const std::filesystem::path & checkpointCleanup ) {
	int CLEANUP_TIMEOUT = param_integer( "PREEN_CHECKPOINT_CLEANUP_TIMEOUT", 300 );
	size_t MAX_CHECKPOINT_CLEANUP_PROCS = param_integer( "MAX_CHECKPOINT_CLEANUP_PROCS", 100 );

	std::string message;
	dc::AwaitableDeadlineReaper logansRun;
	std::map<int, std::filesystem::path> pidToPathMap;
	std::map< std::filesystem::path, std::string > badPathMap;

	auto checkpointCleanupDir = std::filesystem::directory_iterator( checkpointCleanup );
	for( const auto & entry : checkpointCleanupDir ) {
		if(! entry.is_directory()) { continue; }
		// dprintf( D_ZKM, "Found directory %s\n", entry.path().string().c_str() );

		auto userSpecificDir = std::filesystem::directory_iterator(entry);
		for( const auto & jobDir : userSpecificDir ) {
			if(! jobDir.is_directory()) { continue; }
			// dprintf( D_ZKM, "Found directory %s\n", jobDir.path().string().c_str() );

			auto pathToJobAd = jobDir.path() / ".job.ad";
			FILE * fp = NULL;
			{
				TemporaryPrivSentry sentry(PRIV_ROOT);
				fp = safe_fopen_wrapper( pathToJobAd.string().c_str(), "r" );
			}
			if(! fp) {
				formatstr( message, "No .job.ad file found in %s, ignoring.", jobDir.path().string().c_str() );
				dprintf( D_ALWAYS, "%s\n", message.c_str() );
				badPathMap[jobDir.path()] = message;
				continue;
			}

			int err;
			bool is_eof;
			ClassAd jobAd;
			int attributesRead = InsertFromFile( fp, jobAd, is_eof, err );
			if( attributesRead <= 0 ) {
				formatstr( message, "No ClassAd attributes found in file %s, ignoring.", pathToJobAd.string().c_str() );
				dprintf( D_ALWAYS, "%s\n", message.c_str() );
				badPathMap[jobDir.path()] = message;
				continue;
			}

			int cluster = -1;
			if(! jobAd.LookupInteger(ATTR_CLUSTER_ID, cluster)) {
				formatstr( message, "Failed to find cluster ID in job ad (%s), ignoring.", pathToJobAd.string().c_str() );
				dprintf( D_ALWAYS, "%s\n", message.c_str() );
				badPathMap[jobDir.path()] = message;
				continue;
			}

			int proc = -1;
			if(! jobAd.LookupInteger(ATTR_PROC_ID, proc)) {
				formatstr( message, "Failed to find proc ID in job ad (%s), ignoring.", pathToJobAd.string().c_str() );
				dprintf( D_ALWAYS, "%s\n", message.c_str() );
				badPathMap[jobDir.path()] = message;
				continue;
			}


			std::string error;
			int spawned_pid = -1;
			bool rv = spawnCheckpointCleanupProcess(
				cluster, proc, & jobAd, logansRun.reaper_id(),
				spawned_pid, error
			);

			if(! rv) {
				formatstr( message, "Failed to spawn cleanup process for %d.%d, ignoring.", cluster, proc );
				dprintf( D_ALWAYS, "%s\n", message.c_str() );
				badPathMap[jobDir.path()] = message;
				continue;
			}

			pidToPathMap[spawned_pid] = jobDir.path();
			logansRun.born( spawned_pid, CLEANUP_TIMEOUT );
			if( logansRun.living() >= MAX_CHECKPOINT_CLEANUP_PROCS ) {
				// dprintf( D_ZKM, "Hit MAX_CHECKPOINT_CLEANUP_PROCS, pausing.\n" );
				auto [pid, timed_out, status] = co_await( logansRun );
				if( timed_out ) {
					badPathMap[pidToPathMap[pid]] = "Timed out.";
					// We can't call Kill_Family() because we don't register
					// any process families via Create_Process() because we
					// can't talk to the procd.  At some point, GregT will
					// fix this and daemon core will be able to track families
					// by cgroups without using the procd.
					//
					// Until then, we'll assume the plug-in is well-behaved.
					// daemonCore->Kill_Family( pid );
					// kill( pid, SIGTERM );
					daemonCore->Shutdown_Graceful( pid );
				} else if( status != 0 ) {
					formatstr( message, "checkpoint clean-up proc %d returned %d", pid, status );
					// dprintf( D_ZKM, "%s\n", message.c_str() );
					// This could be the event from us killing the timed-out
					// process, so don't overwrite the bad path map.
					if(! badPathMap.contains(pidToPathMap[pid])) {
						badPathMap[pidToPathMap[pid]] = message;
					}
				}
			}
		}
	}

	while( logansRun.living() ) {
		// dprintf( D_ZKM, "co_await logansRun.await()\n" );
		auto [pid, timed_out, status] = co_await( logansRun );
		// dprintf( D_ZKM, "co_await() = %d, %d, %d\n", pid, timed_out, status );
		if( timed_out ) {
			badPathMap[pidToPathMap[pid]] = "Timed out.";
			// FIXME: see note in copy of this code below, refactor this
			// code into a function.
			// daemonCore->Kill_Family( pid );
			// kill( pid, SIGTERM );
			daemonCore->Shutdown_Graceful( pid );
		} else if( status != 0 ) {
			formatstr( message, "checkpoint clean-up proc %d returned %d", pid, status );
			// dprintf( D_ZKM, "%s\n", message.c_str() );
			badPathMap[pidToPathMap[pid]] = message;
		}
	}

	std::string buffer;
	for( const auto & [path, message] : badPathMap ) {
		formatstr( buffer, "%s - %s\n", path.string().c_str(), message.c_str() );
		BadFiles.emplace_back( buffer );
	}

	DC_Exit(preen_report());
}


bool
check_cleanup_dir() {
	std::filesystem::path SPOOL(Spool);
	std::filesystem::path checkpointCleanup = SPOOL / "checkpoint-cleanup";

	if(! std::filesystem::exists( checkpointCleanup )) {
		// dprintf( D_ZKM, "Directory '%s' does not exist, ignoring.\n", checkpointCleanup.string().c_str() );
		return false;
	}

	check_cleanup_dir_actual( checkpointCleanup );
	return true;
}
