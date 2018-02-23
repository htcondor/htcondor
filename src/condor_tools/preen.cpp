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
#include "../condor_privsep/condor_privsep.h"
#include "filename_tools.h"
#include "ipv6_hostname.h"
#include "subsystem_info.h"

#include <map>

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
void send_mail_file();
void init_params();
void check_spool_dir();
void check_execute_dir();
void check_log_dir();
void rec_lock_cleanup(const char *path, int depth, bool remove_self = false);
void check_tmp_dir();
void check_daemon_sock_dir();
void bad_file( const char *, const char *, Directory & );
void good_file( const char *, const char * );
void produce_output();
bool is_valid_shared_exe( const char *name );
bool is_ckpt_file_or_submit_digest( const char *name );
bool is_v2_ckpt( const char *name );
bool is_v3_ckpt( const char *name );
bool is_submit_digest( const char *name );
bool cluster_exists( int );
bool proc_exists( int, int );
bool is_myproxy_file( const char *name );
bool is_ccb_file( const char *name );
bool touched_recently(char const *fname,time_t delta);
bool linked_recently(char const *fname,time_t delta);

#ifdef HAVE_HTTP_PUBLIC_FILES
void check_public_files_webroot_dir();
#endif

/*
  Tell folks how to use this program.
*/
void
usage()
{
	fprintf( stderr, "Usage: %s [-mail] [-remove] [-verbose] [-debug]\n", MyName );
	exit( 1 );
}


int
main( int argc, char *argv[] )
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
	myDistro->Init( argc, argv );
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
		std::string szVerbose="D_FULLDEBUG";
		char * pval = param("TOOL_DEBUG");
		if( pval ) {
			szVerbose+="|";
			szVerbose+=pval;
			free( pval );
		}
		_condor_set_debug_flags( szVerbose.c_str(), D_FULLDEBUG );
		
	}
	dprintf( D_ALWAYS, "********************************\n");
	dprintf( D_ALWAYS, "STARTING: condor_preen\n");
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
	if( !BadFiles->isEmpty() ) {
		produce_output();
	}

		// Clean up
	delete BadFiles;

	dprintf( D_ALWAYS, "********************************\n");
	dprintf( D_ALWAYS, "ENDING: condor_preen\n");
	dprintf( D_ALWAYS, "********************************\n");
	
	return 0;
}

/*
  As the program runs, we create a list of messages regarding the status
  of file in the list BadFiles.  Now we use that to produce all output.
  If MailFlag is set, we send the output to the condor administrators
  via mail, otherwise we just print it on stdout.
*/
void
produce_output()
{
	char	*str;
	FILE	*mailer;
	MyString subject,szTmp;
	subject.formatstr("condor_preen results %s: %d old file%s found", 
		get_local_fqdn().Value(), BadFiles->number(), 
		(BadFiles->number() > 1)?"s":"");

	if( MailFlag ) {
		if( (mailer=email_nonjob_open(PreenAdmin, subject.Value())) == NULL ) {
			EXCEPT( "Can't do email_open(\"%s\", \"%s\")",PreenAdmin,subject.Value());
		}
	} else {
		mailer = stdout;
	}

	szTmp.formatstr("The condor_preen process has found the following stale condor files on <%s>:\n\n",  get_local_hostname().Value());
	dprintf(D_ALWAYS, "%s", szTmp.Value()); 
		
	if( MailFlag ) {
		fprintf( mailer, "\n" );
		fprintf( mailer, "%s", szTmp.Value());
	}

	for( BadFiles->rewind(); (str = BadFiles->next()); ) {
		szTmp.formatstr("  %s\n", str);
		dprintf(D_ALWAYS, "%s", szTmp.Value() );
		fprintf( mailer, "%s", szTmp.Value() );
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
}

bool
check_job_spool_hierarchy( char const *parent, char const *child, std::set< std::string > &stale_spool_files )
{
	ASSERT( parent );
	ASSERT( child );

		// We expect directories of the form produced by SpooledJobFiles::getJobSpoolPath()
		// and GetSpooledExecutablePath().
		// e.g. $(SPOOL)/<cluster mod 10000>/<proc mod 10000>/cluster<cluster>.proc<proc>.subproc<proc>
		// or $(SPOOL)/<cluster mod 10000>/cluster<cluster>.ickpt.subproc<subproc>

	char *end=NULL;
	long l = strtol(child,&end,10);
	if( l == LONG_MIN || !end || *end != '\0' ) {
		return false; // not part of the job spool hierarchy
	}

	std::string topdir;
	formatstr(topdir,"%s%c%s",parent,DIR_DELIM_CHAR,child);
	Directory dir(topdir.c_str(),PRIV_ROOT);
	char const *f;
	while( (f=dir.Next()) ) {

			// see if it's a legitimate job spool file/directory
		if( is_ckpt_file_or_submit_digest(f) ) {
			good_file( topdir.c_str(), f );
			continue;
		}

		if( IsDirectory(dir.GetFullPath()) && !IsSymlink(dir.GetFullPath()) ) {
			if( check_job_spool_hierarchy( topdir.c_str(), f, stale_spool_files ) ) {
				good_file( topdir.c_str(), f );
				continue;
			}
		}

		const char* dirPath = dir.GetFullPath();
		stale_spool_files.insert( dirPath );
	}

		// By returning true here, we indicate that this directory is
		// part of the spool directory hierarchy and should not be
		// deleted.  We do return true even if the directory is empty.
		// If we wanted to remove empty directories of this type, we
		// would need to do so with rmdir(), not a recursive remove,
		// because new job directories might show up inside this
		// directory between the time we examine the directory
		// contents and the time we remove the directory.  We don't
		// bother dealing with that case, because if something ever
		// causes the self-cleaning of these directories to fail, the
		// self-cleaning will be tried again the next time the
		// directory is used.  In the worst case, the spool hierarchy
		// is self-limited in how many entries will be made anyway.

	return true;
}

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
    unsigned int	history_length, startd_history_length;
	const char  	*f;
    const char      *history, *startd_history;
	Directory  		dir(Spool, PRIV_ROOT);
	StringList 		well_known_list;
	std::set<std::string> maybe_stale_spool_files;
	std::set<std::string> stale_spool_files;
	Qmgr_connection *qmgr = NULL;
	double			last_connection_time;
	double			max_connection_time;

	if ( ValidSpoolFiles == NULL ) {
		dprintf( D_ALWAYS, "Not cleaning spool directory: No VALID_SPOOL_FILES defined\n");
		return;
	}

    history = param("HISTORY");
    history = condor_basename(history); // condor_basename never returns NULL
    history_length = strlen(history);

    startd_history = param("STARTD_HISTORY");
   	startd_history = condor_basename(startd_history);
	startd_history_length = strlen(startd_history);
	   
	last_connection_time = _condor_debug_get_time_double();
	max_connection_time = param_integer("PREEN_MAX_SCHEDD_CONNECTION_TIME");

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

			// We still don't know if this file is good or bad, and we can't
			// tell without asking the schedd. Add it to our potentially stale
			// files list, we'll deal with it later.
		maybe_stale_spool_files.insert( f );
	}

		// Step 2: Connect to the schedd, and check if the files in 
		// stage_spool_files are truly stale. If we find any that are not, 
		// remove from the list. Also if we stay connected to the schedd for
		// too long, force a temporary disconnect so it can recover.
	for( auto i = maybe_stale_spool_files.begin(); i != maybe_stale_spool_files.end(); ++i ) {
		
		std::string spool_file = *i;

		// Establish (or re-establish) schedd connection
		if ( !qmgr ) {
			if ( !( qmgr = ConnectQ (0, 0, false) ) ) {
				dprintf( D_ALWAYS, "Unknown error connecting to job queue. "
					"Aborting without deleting files.\n" );
				return;
			}
			last_connection_time = _condor_debug_get_time_double();
		}

			// See if it's a legitimate checkpoint.
		if( is_ckpt_file_or_submit_digest( spool_file.c_str() ) ) {
			good_file( Spool, spool_file.c_str() );
			continue;
		}

			// See if it's a legimate MyProxy password file.
		if ( is_myproxy_file( spool_file.c_str() ) ) {
			good_file( Spool, spool_file.c_str() );
			continue;
		}

			// If a directory, check through subdirectories. Because we are not
			// in the dir.Next() loop anymore, need to create full path manually.
		MyString spool_file_full_path;
		dircat( dir.GetDirectoryPath(), spool_file.c_str(), spool_file_full_path );
		if( IsDirectory( spool_file_full_path.Value() ) && !IsSymlink( spool_file_full_path.Value() ) ) {
			if( check_job_spool_hierarchy( Spool, spool_file.c_str(), stale_spool_files ) ) {
				good_file( Spool, spool_file.c_str() );
				continue;
			}
		}

			// At this point we know for sure the file is stale
		stale_spool_files.insert( spool_file );

			// If the schedd is connected, check how long the connection has
			// been active. If it has exceeded a maximum connection time 
			// (defined by PREEN_MAX_SCHEDD_CONNECTION_TIME) then disconnect.
			// This will give the schedd some time to deal with everything that
			// happened while it was blocked.
		if ( qmgr ) {
			if ( _condor_debug_get_time_double() > 
							( last_connection_time + max_connection_time ) ) {
				if ( DisconnectQ( qmgr ) ) {
					qmgr = NULL;
				} 
				else {
					dprintf( D_ALWAYS, "Error disconnecting from job queue.\n" );
				}
			}
		}
	}

		// Now disconnect from the schedd for good.
	if ( qmgr ) {
		if ( DisconnectQ( qmgr ) ) {
			qmgr = NULL;
		} 
		else {
			dprintf( D_ALWAYS, "Error disconnecting from job queue.\n" );
		}
	}

		// Step 3: Now that we have a final list of stale files, it's time to 
		// go through and actually flag them all for deletion.
	for( auto i = stale_spool_files.begin(); i != stale_spool_files.end(); ++i ) {
		std::string spool_file = *i;
		bad_file( Spool, spool_file.c_str(), dir );
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
	MyString path;
	path.formatstr("%s/%s", Spool, name);
	int count = link_count(path.Value());
	if (count == 1) {
		return false;
	}
	if (count == -1) {
		dprintf(D_ALWAYS, "link_count error on %s; not deleting\n", name);
	}
	return true;
}

/*
  Given the name of a file in the spool directory, return true if it's a
  legitimate checkpoint file or submit digest, and false otherwise.  If the name starts
  with "cluster", it should be a V3 style checkpoint. If it starts with condor_submit
  it's a submit digest, Otherwise it is either a V2 style checkpoint, or not a checkpoint at all.
*/
bool
is_ckpt_file_or_submit_digest( const char *name )
{
	if (name[0] == 'c') { // might start with 'cluster' or 'condor_submit'
		if( name[1] == 'l' ) { // might start with 'cluster'
			return is_v3_ckpt( name );
		} else if ( name[1] == 'o' ) { // might start with 'condor_submit'
			return is_submit_digest( name );
		}
	} else if ( name[0] == 'j') { // might start with 'job'
		return is_v2_ckpt( name );
	}
	return false;
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
  We're given the name of a file which appears to be a V2 checkpoint file.
  We try to dig out the cluster/proc ids, and search the job queue for
  a corresponding process.  We return true if we find it, and false
  otherwise.

  V2 checkpoint files formats are:
	  job<#>.ickpt		- initial checkpoint file
	  job<#>.ckpt.<#>	- specific checkpoint file
*/
bool
is_v2_ckpt( const char *name )
{
	int		cluster;
	int		proc;

	cluster = grab_val( name, "job" );
	proc = grab_val( name, ".ckpt." );

	if( proc < 0 ) {
		return cluster_exists( cluster );
	} else {
		return proc_exists( cluster, proc );
	}
}

/*
  We're given the name of a file which appears to be a V3 checkpoint file.
  We try to dig out the cluster/proc ids, and search the job queue for
  a corresponding process.  We return true if we find it, and false
  otherwise.

  V3 checkpoint file formats are:
	  cluster<#>.ickpt.subproc<#>		- initial checkpoint file
	  cluster<#>.proc<#>.subproc<#>		- specific checkpoint file
*/
bool
is_v3_ckpt( const char *name )
{
	int		cluster;
	int		proc;

	cluster = grab_val( name, "cluster" );
	proc = grab_val( name, ".proc" );
	grab_val( name, ".subproc" );
		
	if( proc < 0 ) {
		return cluster_exists( cluster );
	} else {
		return proc_exists( cluster, proc );
	}
}

/*
  Check whether the given file could be a valid submit digest
  for a queued late materialization job factory
*/
bool
is_submit_digest( const char *name )
{
	int cluster = grab_val( name, "condor_submit." );
	return cluster_exists( cluster );
}


/*
  Check whether the given file could be a valid MyProxy password file
  for a queued job.
*/
bool
is_myproxy_file( const char *name )
{
	int cluster, proc;

		// TODO This will accept files that look like a valid MyProxy
		//   password file with extra characters on the end of the name.
	int rc = sscanf( name, "mpp.%d.%d", &cluster, &proc );
	if ( rc != 2 ) {
		return false;
	}
	return proc_exists( cluster, proc );
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
  Check to see whether a given cluster number exists in the job queue.
*/
bool
cluster_exists( int cluster )
{
	return proc_exists( cluster, -1 );
}

/*
  Check to see whether a given cluster and process number exist in the
  job queue.
*/
bool
proc_exists( int cluster, int proc )
{
	if (cluster <= 0)
		return false;

	int id = 0;
	if (GetAttributeInt(cluster, proc, ATTR_CLUSTER_ID, &id) < 0) {
		return false;
	}

	return true;
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
	StringList invalid;

	invalid.initializeFromString (InvalidLogFiles ? InvalidLogFiles : "");

	while( (f = dir.Next()) ) {
		if( invalid.contains(f) ) {
			bad_file( Log, f, dir );
		} else {
			good_file( Log, f );
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
		MyString full_path;
		full_path.formatstr("%s%c%s",DaemonSockDir,DIR_DELIM_CHAR,f);

			// daemon sockets are touched periodically to mark them as
			// still in use
		if( touched_recently( full_path.Value(), stale_age ) ) {
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
		MyString tmpDir;
		FileLock::getTempPath(tmpDir);
		rec_lock_cleanup(tmpDir.Value(), 3);
	}
  
#endif	
}


extern "C" int
SetSyscalls( int foo ) { return foo; }

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
	Regex re; int err = 0; const char * pszMsg = 0;
	ASSERT(re.compile("slot([0-9]*)_execute", &pszMsg, &err, PCRE_CASELESS));
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
	MyString	pathname;
	MyString	buf;

	if( !fullpath( name ) ) {
		pathname.formatstr( "%s%c%s", dirpath, DIR_DELIM_CHAR, name );
	}
	else {
		pathname = name;
	}

	if( VerboseFlag ) {
		printf( "%s - BAD\n", pathname.Value() );
		dprintf( D_ALWAYS, "%s - BAD\n", pathname.Value() );
	}

	if( RmFlag ) {
		bool removed = dir.Remove_Full_Path( pathname.Value() );
		if( !removed && privsep_enabled() ) {
			removed = privsep_remove_dir( pathname.Value() );
			if( VerboseFlag ) {
				if( removed ) {
					dprintf( D_ALWAYS, "%s - failed to remove directly, but succeeded via privsep switchboard\n", pathname.Value() );
					printf( "%s - failed to remove directly, but succeeded via privsep switchboard\n", pathname.Value() );
				}
			}
		}
		if( removed ) {
			buf.formatstr( "%s - Removed", pathname.Value() );
		} else {
			buf.formatstr( "%s - Can't Remove", pathname.Value() );
		}
	} else {
		buf.formatstr( "%s - Not Removed", pathname.Value() );
	}
	BadFiles->append( buf.Value() );
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
