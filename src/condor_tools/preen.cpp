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
#include "condor_uid.h"
#include "string_list.h"
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
#include "extArray.h"
#include "link.h"
#include "shared_port_endpoint.h"
#include "file_lock.h"
#include "filename_tools.h"
#include "ipv6_hostname.h"
#include "subsystem_info.h"
#include "my_popen.h"

#include <array>
#include <memory>

#include <map>
#include <sstream>

#define PREEN_EXIT_STATUS_SUCCESS       0
#define PREEN_EXIT_STATUS_FAILURE       1
#define PREEN_EXIT_STATUS_EMAIL_FAILURE 2

using namespace std;

State get_machine_state();

extern void		_condor_set_debug_flags( const char *strflags, int flags );

// Define this to check for memory leaks

char		*Spool;				// dir for condor job queue
StringList   ExecuteDirs;		// dirs for execution of condor jobs
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
StringList	*BadFiles;			// list of files which don't belong

// prototypes of local interest
void usage();
void init_params();
void check_spool_dir();
void check_execute_dir();
void check_log_dir();
void rec_lock_cleanup(const char *path, int depth, bool remove_self = false);
void check_tmp_dir();
void check_daemon_sock_dir();
void bad_file( const char *, const char *, Directory & );
void good_file( const char *, const char * );
int send_email();
bool is_valid_shared_exe( const char *name );
bool is_ckpt_file_or_submit_digest(const char *name, JOB_ID_KEY & jid);
bool is_myproxy_file( const char *name, JOB_ID_KEY & jid );
bool is_ccb_file( const char *name );
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
	exit( PREEN_EXIT_STATUS_FAILURE );
}


int
main( int /*argc*/, char *argv[] )
{
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
	BadFiles = new StringList;

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

		// Produce output, either on stdout or by mail
	int exit_status = PREEN_EXIT_STATUS_SUCCESS;
	if( !BadFiles->isEmpty() ) {
		// write the files we deleted to the daemon log
		for (const char * str = BadFiles->first(); str; str = BadFiles->next()) {
			dprintf(D_ALWAYS, "%s\n", str);
		}

		dprintf(D_ALWAYS, "Results: %d file%s preened\n", BadFiles->number(), (BadFiles->number()>1) ? "s" : "");

		exit_status = send_email();
	} else {
		dprintf(D_ALWAYS, "Results: No files preened\n");
	}

		// Clean up
	delete BadFiles;

	dprintf( D_ALWAYS, "********************************\n");
	dprintf( D_ALWAYS, "ENDING: condor_preen PID: %d STATUS: %d\n", getpid(), exit_status);
	dprintf( D_ALWAYS, "********************************\n");
	
	return exit_status;
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
	char	*str;
	FILE	*mailer;
	std::string subject,szTmp;
	formatstr(subject, "condor_preen results %s: %d old file%s found", 
		get_local_fqdn().c_str(), BadFiles->number(), 
		(BadFiles->number() > 1)?"s":"");

	if( MailFlag ) {
		if( (mailer=email_nonjob_open(PreenAdmin, subject.c_str())) == NULL ) {
			dprintf(D_ALWAYS|D_FAILURE, "Can't do email_open(\"%s\", \"%s\")\n",PreenAdmin,subject.c_str());
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

	for( BadFiles->rewind(); (str = BadFiles->next()); ) {
		formatstr(szTmp, "  %s\n", str);
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
    size_t	history_length, startd_history_length;
	const char  	*f;
    const char      *history, *startd_history;
	Directory  		dir(Spool, PRIV_ROOT);
	StringList 		well_known_list;
	JobIdSpoolFiles maybe_stale;
	std::string tmpstr;

	if ( ValidSpoolFiles == NULL ) {
		dprintf( D_ALWAYS, "Not cleaning spool directory: VALID_SPOOL_FILES not configured\n");
		return;
	}

    history = param("HISTORY");
    history = condor_basename(history); // condor_basename never returns NULL
    history_length = strlen(history);

    startd_history = param("STARTD_HISTORY");
   	startd_history = condor_basename(startd_history);
	startd_history_length = strlen(startd_history);

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
		"OfflineLog",
		// SCHEDD.lock: High availability lock file.  Current
		// manual recommends putting it in the spool, so avoid it.
		"SCHEDD.lock",
		};
	for (int ix = 0; ix < (int)(sizeof(valid_list)/sizeof(valid_list[0])); ++ix) {
		if ( ! well_known_list.contains(valid_list[ix])) well_known_list.append(valid_list[ix]);
	}
	
		// Step 1: Check each file in the directory. Look for files that
		// obviously should be here (job queue logs, shared exes, etc.) and
		// flag them as good files. Put everything else into stale_spool_files,
		// which we'll deal with later.
	while( (f = dir.Next()) ) {
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
            // see if it's a rotated history file. 
        if (   strlen(f) >= history_length 
            && strncmp(f, history, history_length) == 0) {
            good_file( Spool, f );
            continue;
        }

			// if startd_history is defined, so if it's one of those
		if ( startd_history_length > 0 &&
			strlen(f) >= startd_history_length &&
			strncmp(f, startd_history, startd_history_length) == 0) {

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
			if (is_ckpt_file_or_submit_digest(f, jid) || is_myproxy_file(f, jid)) {
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
				ad.Assign("IncludeClusterAd", true);

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
	}
	return false;
}

bool
is_myproxy_file(const char *name, JOB_ID_KEY & jid)
{
	// TODO This will accept files that look like a valid MyProxy
	//   password file with extra characters on the end of the name.
	int rc = sscanf( name, "mpp.%d.%d", &jid.cluster, &jid.proc );
	if ( rc != 2 ) {
		return false;
	}
	return jid.cluster > 0 && jid.proc >= 0;
}


/*
  Check whether the given file could be a valid MyProxy password file
  for a queued job.
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
  Scan the execute directory looking for bogus files.
*/ 
void
check_execute_dir()
{
	time_t now = time(NULL);
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

	ExecuteDirs.rewind();
	char const *Execute;
	while( (Execute=ExecuteDirs.next()) ) {
		Directory dir( Execute, PRIV_ROOT );
		while( (f = dir.Next()) ) {
			if( busy ) {
				good_file( Execute, f );	// let anything go here
			} else {
				if( dir.GetCreateTime() < now ) {
					bad_file( Execute, f, dir ); // junk it
				}
				else {
						// In case the startd just started up a job, we
						// avoid removing very recently created files.
						// (So any files forward dated in time will have
						// to wait for the startd to restart before they
						// are cleaned up.)
					dprintf(D_FULLDEBUG, "In %s, found %s with recent "
					        "creation time.  Not removing.\n", Execute, f );
					good_file( Execute, f ); // too young to kill
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
	int coreFileMaxSize = param_integer("PREEN_COREFILE_MAX_SIZE", 50000000);
	int coreFileStaleAge = param_integer("PREEN_COREFILE_STAGE_AGE", 5184000);
	unsigned int coreFilesPerProgram = param_integer("PREEN_COREFILES_PER_PROCESS", 10);
	StringList invalid;
	std::map<std::string, std::map<int, std::string>> programCoreFiles;

	invalid.initializeFromString (InvalidLogFiles ? InvalidLogFiles : "");

	while( (f = dir.Next()) ) {
		if( invalid.contains(f) ) {
			bad_file( Log, f, dir );
		}
		#ifndef WIN32
			else {
				// Check if this is a core file
				const char* coreFile = strstr( f, "core." );
				if ( coreFile ) {
					StatInfo statinfo( Log, f );
					if( statinfo.Error() == 0 ) {
						// If this core file is stal	e, flag it for removal
						if( abs((int)( time(NULL) - statinfo.GetModifyTime() )) > coreFileStaleAge ) {
							bad_file( Log, f, dir );
							continue;
						}
						// If this core file exceeds a certain size, flag for removal
						if( statinfo.GetFileSize() > coreFileMaxSize ) {
							bad_file( Log, f, dir );
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
		ExecuteDirs.append(Execute);
		free(Execute);
		Execute = NULL;
	}
		// In addition to EXECUTE, there may be SLOT1_EXECUTE, ...
#if 1
	ExtArray<const char *> params;
	Regex re; int errnumber = 0, erroffset = 0;
	ASSERT(re.compile("slot([0-9]*)_execute", &errnumber, &erroffset, PCRE2_CASELESS));
	if (param_names_matching(re, params)) {
		for (int ii = 0; ii < params.length(); ++ii) {
			Execute = param(params[ii]);
			if (Execute) {
				if ( ! ExecuteDirs.contains(Execute)) {
					ExecuteDirs.append(Execute);
				}
				free(Execute);
			}
		}
	}
	params.truncate(0);
#else
	ExtArray<ParamValue> *params = param_all();
	for( int p=params->length(); p--; ) {
		char const *name = (*params)[p].name.Value();
		char *tail = NULL;
		if( strncasecmp( name, "SLOT", 4 ) != 0 ) continue;
		long l = strtol( name+4, &tail, 10 );
		if( l == LONG_MIN || tail <= name || strcasecmp( tail, "_EXECUTE" ) != 0 ) continue;

		Execute = param(name);
		if( Execute ) {
			if( !ExecuteDirs.contains( Execute ) ) {
				ExecuteDirs.append( Execute );
			}
			free( Execute );
		}
	}
	delete params;
#endif

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
bad_file( const char *dirpath, const char *name, Directory & dir )
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
	BadFiles->append( buf.c_str() );
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
		std::unique_ptr<FILE, decltype(&my_pclose)> process_pipe( my_popen( args, "r", 0 ), my_pclose );

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
