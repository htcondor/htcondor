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
#include "condor_ver_info.h"
#if !defined(WIN32)
#include <pwd.h>
#include <sys/stat.h>
#endif
#include "store_cred.h"
#include "internet.h"
#include "my_hostname.h"
#include "domain_tools.h"
#include "sig_install.h"
#include "access.h"
#include "daemon.h"
#include "match_prefix.h"

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
//uncomment this to have condor_submit use the new for 8.5 submit_utils classes
#define USE_SUBMIT_UTILS 1
#include "condor_qmgr.h"
#include "submit_internal.h"

#include "condor_vm_universe_types.h"
#include "vm_univ_utils.h"
#include "my_popen.h"
#include "condor_base64.h"
#include "zkm_base64.h"

#include <algorithm>
#include <string>
#include <set>

const char * is_queue_statement(const char * line); // return ptr to queue args of this is a queue statement
void SetSendCredentialInAd( ClassAd *job_ad ); 
void set_factory_submit_info(int cluster, int num_procs);
void init_vars(SubmitHash & hash, int cluster_id, StringList & vars);

extern AbstractScheddQ * MyQ;
extern SubmitHash submit_hash;

extern int ExtraLineNo;
extern int GotQueueCommand;
extern int GotNonEmptyQueueCommand;
extern int WarnOnUnusedMacros;

extern ClassAd *gClusterAd;
extern int ClustersCreated;
extern int ClusterId;
//extern int ProcId;

extern int DashDryRun;
extern int DumpSubmitHash;
extern int default_to_factory;


int write_factory_file(const char * filename, const void* data, int cb, mode_t access)
{
	int fd = safe_open_wrapper_follow(filename, O_WRONLY|_O_BINARY|O_CREAT|O_TRUNC|O_APPEND, access);
	if (fd == -1) {
		dprintf(D_ALWAYS, "ERROR: write_factory_file(%s): open() failed: %s (%d)\n", filename, strerror(errno), errno);
		return -1;
	}

	int cbwrote = write(fd, data, cb);
	if (cbwrote != cb) {
		dprintf(D_ALWAYS, "ERROR: write_factory_file(%s): write() failed: %s (%d)\n", filename, strerror(errno), errno);
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

// callback passed to make_job_ad on the submit_hash that gets passed each input or output file
// so we can choose to do file checks. 
#if 1
int check_cluster_sub_file(void* /*pv*/, SubmitHash * /*sub*/, _submit_file_role role, const char * pathname, int flags)
{
	if (DashDryRun) {
		fprintf(stdout, "check_file: role=%d, flags=%d, file=%s\n", role, flags, pathname);
	}
	return 0;
}
#else
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
#endif

int SendClusterAd (ClassAd * ad)
{
	int  setattrflags = 0;
	if (param_boolean("SUBMIT_NOACK_ON_SETATTRIBUTE",true)) {
		setattrflags |= SetAttribute_NoAck;
	}

	if (MyQ->set_AttributeInt (ClusterId, -1, ATTR_CLUSTER_ID, ClusterId, setattrflags) == -1) {
		if (setattrflags & SetAttribute_NoAck) {
			fprintf( stderr, "\nERROR: Failed submission for job %d - aborting entire submit\n", ClusterId);
		} else {
			fprintf( stderr, "\nERROR: Failed submission for job %d (%d)\n", ClusterId, errno);
		}
		return -1;
	}

	const char *lhstr;
	ExprTree *tree = NULL;
	std::string rhs;
	rhs.reserve(120);
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );

	for ( auto itr = ad->begin(); itr != ad->end(); itr++ ) {
		lhstr = itr->first.c_str();
		tree = itr->second;
		if ( ! lhstr || ! tree) {
			fprintf( stderr, "\nERROR: Null attribute name or value for cluster %d\n", ClusterId );
			return -1;
		} else {
			rhs.clear();
			unparser.Unparse(rhs, tree);
			if (YourStringNoCase("undefined") == rhs.c_str()) {
				// don't write undefined values into the cluster ad.
				continue;
			}
			if (MyQ->set_Attribute(ClusterId, -1, lhstr, rhs.c_str(), setattrflags) == -1) {
				if (setattrflags & SetAttribute_NoAck) {
					fprintf(stderr, "\nERROR: Failed set %s=%s cluster %d - aborting entire submit\n", lhstr, rhs.c_str(), ClusterId);
				} else {
					fprintf(stderr, "\nERROR: Failed to set %s=%s for cluster %d (%d)\n", lhstr, rhs.c_str(), ClusterId, errno);
				}
				return -1;
			}
		}
	}

	return 0;
}


// convert a populated foreach item set to a "from <file>" type of foreach, creating the file if needed
// if the foreach mode is foreach_not, this function does nothing.
// if there is already an items file and spill_items is false, the items file is converted to a full path
//
int convert_to_foreach_file(SubmitHash & hash, SubmitForeachArgs & o, int ClusterId, bool spill_items)
{
	int rval = 0;

	// if submit foreach data is not already a disk file, turn it into one.
	bool make_foreach_file = false;
	if (spill_items) {
		// PRAGMA_REMIND("TODO: only spill foreach data to a file if it is larger than a certain size.")
		if (o.items.isEmpty()) {
			o.foreach_mode = foreach_not;
		} else {
			make_foreach_file = true;
		}
	} else if (o.foreach_mode == foreach_from) {
		// if it is a file, make sure the path is fully qualified.
		std::string foreach_fn = o.items_filename;
		o.items_filename = hash.full_path(foreach_fn.c_str(), false);
	}

	// if we are makeing the foreach file, we had to wait until we got the cluster id to do that
	// so make it now.
	if (make_foreach_file) {
		std::string foreach_fn;
		formatstr(foreach_fn,"condor_submit.%d.items", ClusterId);
		o.items_filename = hash.full_path(foreach_fn.c_str(), false);

		int fd = safe_open_wrapper_follow(o.items_filename.c_str(), O_WRONLY|_O_BINARY|O_CREAT|O_TRUNC|O_APPEND, 0644);
		if (fd == -1) {
			dprintf(D_ALWAYS, "ERROR: write_items_file(%s): open() failed: %s (%d)\n", o.items_filename.c_str(), strerror(errno), errno);
			return -1;
		}

		std::string line;
		for (const char * item = o.items.first(); item != NULL; item = o.items.next()) {
			line = item; line += "\n";
			int cbwrote = write(fd, line.data(), (int)line.size());
			if (cbwrote != (int)line.size()) {
				dprintf(D_ALWAYS, "ERROR: write_items_file(%s): write() failed: %s (%d)\n", o.items_filename.c_str(), strerror(errno), errno);
				rval = -1;
				break;
			}
		}

		close(fd);
		if (rval < 0)
			return rval;

		o.foreach_mode = foreach_from;
	}
	//PRAGMA_REMIND("add code to check for unused hash entries and fail/warn.")
	//PRAGMA_REMIND("add code to do partial expansion of the hash values.")

	return rval;
}

int append_queue_statement(std::string & submit_digest, SubmitForeachArgs & o)
{
	int rval = 0;

	// append the digest of the queue statement to the submit digest.
	//
	submit_digest += "\n";
	submit_digest += "Queue ";
	if (o.queue_num) { formatstr_cat(submit_digest, "%d ", o.queue_num); }
	auto_free_ptr submit_vars(o.vars.print_to_delimed_string(","));
	if (submit_vars.ptr()) { submit_digest += submit_vars.ptr(); submit_digest += " "; }
	if ( ! o.items_filename.empty()) {
		submit_digest += "from ";
		char slice_str[16*3+1];
		if (o.slice.to_string(slice_str, COUNTOF(slice_str))) { submit_digest += slice_str; submit_digest += " "; }
		submit_digest += o.items_filename.c_str();
	}
	submit_digest += "\n";

	return rval;
}
