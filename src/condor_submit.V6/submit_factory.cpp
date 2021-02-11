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

#include "extArray.h"
#include "MyString.h"
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
#include "condor_qmgr.h"
#include "submit_internal.h"

#include "list.h"
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
int ParseDashAppendLines(List<const char> &exlines, MACRO_SOURCE& source, MACRO_SET& macro_set);
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
		MyString foreach_fn = o.items_filename;
		o.items_filename = hash.full_path(foreach_fn.c_str(), false);
	}

	// if we are makeing the foreach file, we had to wait until we got the cluster id to do that
	// so make it now.
	if (make_foreach_file) {
		MyString foreach_fn;
		foreach_fn.formatstr("condor_submit.%d.items", ClusterId);
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


#if 0 // no longer used.

int submit_factory_job (
	FILE * fp,
	MACRO_SOURCE & source,
	List<const char> & extraLines,   // lines passed in via -a argument
	std::string & queueCommandLine)  // queue statement passed in via -q argument
{
	MACRO_EVAL_CONTEXT ctx; ctx.init("SUBMIT");
	std::string errmsg;

	ErrContext.phase = PHASE_READ_SUBMIT;

	// if there is a queue statement from the command line, get ready to parse it.
	auto_free_ptr qcmd;
	if ( ! queueCommandLine.empty()) { qcmd.set(strdup(queueCommandLine.c_str())); }

	char * qline = NULL;
	int rval = submit_hash.parse_file_up_to_q_line(fp, source, errmsg, &qline);
	do { // So we can use break for flow control
		if (rval)
			break;

		// parse the extra lines before doing $ expansion on the queue line
		rval = ParseDashAppendLines(extraLines, source, submit_hash.macros());
		if (rval < 0)
			break;

		if (qline && qcmd) {
			errmsg = "-queue argument conflicts with queue statement in submit file";
			rval = -1;
			break;
		}

		// the schedd will need to know what the current working directory was when we parsed the submit file.
		// so stuff it into the submit hash
		MyString FactoryIwd;
		condor_getcwd(FactoryIwd);
		insert_macro("FACTORY.Iwd", FactoryIwd.c_str(), submit_hash.macros(), DetectedMacro, ctx);

		submit_hash.optimize();
		int max_materialize = submit_hash.submit_param_int("max_materialize",ATTR_JOB_MATERIALIZE_LIMIT,INT_MAX);

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

		GotQueueCommand = true;
		GotNonEmptyQueueCommand = true;

		SubmitForeachArgs o;
		rval = submit_hash.parse_q_args(queue_args, o, errmsg);
		if (rval)
			break;

		bool spill_foreach_data = false; // set to true when we need to convert foreach into foreach from
		if (o.foreach_mode != foreach_not) {
			spill_foreach_data = (
				o.items_filename.empty() ||
				o.items_filename.FindChar('|') > 0 || // is items from a command (or invalid filename)
				o.items_filename == "<" ||            // is remainder of file. aka 'inline' items
				o.items_filename == "-"               // is items from stdin
				);

			// In order to get a correct item count, we have to always spill the foreach data
			// even when the submit file has a "queue from <file>" command already.
			if (param_boolean("SUBMIT_ALWAYS_COUNT_ITEMS", true)) {
				spill_foreach_data = true;
			}

			MacroStreamYourFile ms(fp, source);
			rval = submit_hash.load_inline_q_foreach_items(ms, o, errmsg);
			if (rval == 1) {
				rval = 0; // having external foreach data is not an error (yet)
				if (spill_foreach_data) {
					rval = submit_hash.load_external_q_foreach_items(o, false, errmsg);
				}
			}
			if (rval)
				break;

			//PRAGMA_REMIND("add code to check for empty fields and fail/warn ?")
		}

		// figure out how many items we will be making jobs from.
		int selected_item_count = 1;
		if (o.foreach_mode != foreach_not) {
			selected_item_count = 0;
			int num_items = o.items.number();
			for (int row = 0; row < num_items; ++row) {
				if (o.slice.selected(row, num_items)) { ++selected_item_count; }
			}
		}

		// if submit foreach data is not already a disk file, turn it into one.
		bool make_foreach_file = false;
		if (spill_foreach_data) {
			//PRAGMA_REMIND("TODO: only spill foreach data to a file if it is larger than a certain size.")
			if (o.items.isEmpty()) {
				o.foreach_mode = foreach_not;
			} else {
				make_foreach_file = true;
			}
		} else if (o.foreach_mode == foreach_from) {
			// if it is a file, make sure the path is fully qualified.
			MyString foreach_fn = o.items_filename;
			o.items_filename = submit_hash.full_path(foreach_fn.c_str(), false);
		}

		if (o.queue_num > 0) {

			if ( ! MyQ) {
				int rval = queue_connect();
				if (rval < 0)
					break;

				if ( ! MyQ->allows_late_materialize()) {
					if (MyQ->has_late_materialize()) {
						fprintf(stderr, "\nERROR: Late materialization is not allowed by this SCHEDD\n");
					} else {
						fprintf(stderr, "\nERROR: The SCHEDD is too old to support late materialization\n");
					}
					WarnOnUnusedMacros = false; // no point on checking for unused macros.
					rval = -1;
					break;
				}
			}

			if (DashDryRun && DumpSubmitHash) {
				fprintf(stdout, "\n----- submit hash at queue begin -----\n");
				int flags = (DumpSubmitHash & 8) ? HASHITER_SHOW_DUPS : 0;
				submit_hash.dump(stdout, flags);
				fprintf(stdout, "-----\n");
			}


			if ((ClusterId = MyQ->get_NewCluster()) < 0) {
				fprintf(stderr, "\nERROR: Failed to create cluster\n");
				if ( ClusterId == -2 ) {
					fprintf(stderr, "Number of submitted jobs would exceed MAX_JOBS_SUBMITTED\n");
				}
				rval = -2;
				break;
			}

			++ClustersCreated;
			delete gClusterAd; gClusterAd = NULL;

			// if we are makeing the foreach file, we had to wait until we got the cluster id to do that
			// so make it now.
			if (make_foreach_file) {
				MyString foreach_fn;
				if (default_to_factory) { // hack to make the test suite work. 
					foreach_fn.formatstr("condor_submit.%d.%u.items", ClusterId, submit_unique_id);
				} else {
					foreach_fn.formatstr("condor_submit.%d.items", ClusterId);
				}
				o.items_filename = submit_hash.full_path(foreach_fn.c_str(), false);

				int fd = safe_open_wrapper_follow(o.items_filename.c_str(), O_WRONLY|_O_BINARY|O_CREAT|O_TRUNC|O_APPEND, 0644);
				if (fd == -1) {
					dprintf(D_ALWAYS, "ERROR: write_items_file(%s): open() failed: %s (%d)\n", o.items_filename.c_str(), strerror(errno), errno);
					rval = -1;
					break;
				}

				std::string line;
				for (const char * item = o.items.first(); item != NULL; item = o.items.next()) {
					line = item; line += "\n";
					size_t cbwrote = write(fd, line.data(), line.size());
					if (cbwrote != line.size()) {
						dprintf(D_ALWAYS, "ERROR: write_items_file(%s): write() failed: %s (%d)\n", o.items_filename.c_str(), strerror(errno), errno);
						rval = -1;
						break; // break out of this loop, but don't return because we still need to close the file.
					}
				}

				close(fd);
				if (rval < 0)
					break;

				o.foreach_mode = foreach_from;
			}
			//PRAGMA_REMIND("add code to check for unused hash entries and fail/warn.")
			//PRAGMA_REMIND("add code to do partial expansion of the hash values.")

			std::string submit_digest;
			submit_hash.make_digest(submit_digest, ClusterId, o.vars, 0);

			//PRAGMA_REMIND("add code to check for unused hash entries and fail/warn.")

			// append the digest of the queue statement to the submit digest.
			//
			submit_digest += "\n";
			submit_digest += "Queue ";
			if (o.queue_num) { formatstr_cat(submit_digest, "%d ", o.queue_num); }
			auto_free_ptr submit_vars(o.vars.print_to_delimed_string(","));
			if (submit_vars.ptr()) { submit_digest += submit_vars.ptr(); submit_digest += " "; }
			char slice_str[16*3+1];
			if (o.slice.to_string(slice_str, COUNTOF(slice_str))) { submit_digest += slice_str; submit_digest += " "; }
			if ( ! o.items_filename.empty()) { submit_digest += "from "; submit_digest += o.items_filename.c_str(); }
			submit_digest += "\n";

			MyString factory_fn;
			if (default_to_factory) { // hack to make the test suite work. 
				factory_fn.formatstr("condor_submit.%d.%u.digest", ClusterId, submit_unique_id);
			} else {
				factory_fn.formatstr("condor_submit.%d.digest", ClusterId);
			}
			MyString factory_path(submit_hash.full_path(factory_fn.c_str(), false));
			rval = write_factory_file(factory_path.c_str(), submit_digest.data(), submit_digest.size(), 0644);
			if (rval < 0)
				break;

			// materialize all of the jobs unless the user requests otherwise.
			// (the admin can also set a limit which is applied at the schedd)
			max_materialize = MIN(max_materialize, selected_item_count * (o.queue_num?o.queue_num:1));

			// send the submit digest to the schedd. the schedd will parse the digest at this point
			// and return success or failure.
			rval = MyQ->set_Factory(ClusterId, max_materialize, factory_path.c_str(), submit_digest.data());
			if (rval < 0)
				break;

			// make the cluster ad.
			//
			init_vars(submit_hash, ClusterId, o.vars);

			// use make_job_ad for job 0 to populate the cluster ad.
			const bool is_interactive = false; // for now, interactive jobs don't work
			const bool dash_remote = false; // for now, remote jobs don't work.
			void * pv_check_arg = NULL; // future?
			ClassAd * job = submit_hash.make_job_ad(JOB_ID_KEY(ClusterId,0), 0, 0,
				is_interactive, dash_remote,
				check_cluster_sub_file, pv_check_arg);
			if ( ! job) {
				rval = -1;
				break;
			}

			int JobUniverse = submit_hash.getUniverse();
			if ( ! JobUniverse) {
				break;
			}
		#ifdef ALLOW_SPOOLED_FACTORY_JOBS
			SendLastExecutable(); // if spooling the exe, send it now.
		#endif
			SetSendCredentialInAd( job );

			gClusterAd = new ClassAd(*job);
			//PRAGMA_REMIND("are there any attributes we should strip from the cluster ad before sending it?")
			rval = SendClusterAd(gClusterAd);
			if (rval == 0 || rval == 1) {
				rval = 0; 
			} else {
				fprintf( stderr, "\nERROR: Failed to queue job.\n" );
				break;
			}

			// tell condor_q what cluster was submitted and how many jobs.
			set_factory_submit_info(ClusterId, (o.queue_num?o.queue_num:1) * selected_item_count);
			submit_hash.delete_job_ad();
			job = NULL;
		}
		if (rval)
			break;

	#if 0 // if we support multiple queue statements, we go back to parsing submit here.
		ErrContext.phase = PHASE_READ_SUBMIT;
	#endif
	} while (false);

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

#endif
