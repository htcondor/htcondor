/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "dc_collector.h"
#include "dc_schedd.h"
//#include "internet.h"
#include "metric_units.h"
//#include "ad_printmask.h"
#include "directory.h"
#include "iso_dates.h"
#include "basename.h" // for condor_dirname
#include "match_prefix.h"
#include "subsystem_info.h"
#include "condor_id.h"
#include "setenv.h"
#include "condor_daemon_core.h" // for extractInheritedSocks
#include "file_transfer.h"
#include "condor_q.h"
#include <algorithm> //for std::reverse
#include <utility> // for std::move
#include <map>
#include <tuple>

#include "classad_helpers.h"
#include <fcntl.h>  // for O_BINARY

void Usage(const char* name, int iExitCode=1);

void Usage(const char* name, int iExitCode)
{
	printf ("Usage: %s [source] [job-list] [options]\n"
		"\n   where [source] is one of\n"
		"\t-jobads <file>\t\tUsing jobads from <file>, - is stdin\n"
		"\t-name <schedd-name>\tRemote schedd to read from\n"
		"\t-pool <collector-name>\tPool remote schedd lives in.\n"
		"   If -name is not specified, then the schedd identified by\n"
		"   SCHEDD_HOST or SCHEDD_NAME is located (as condor_submit does)\n"
		"   and queried for job classads of the [job-list]\n"
		"\n   and [job-list] is one or more of\n"
		"\t<cluster>\t\tAll jobs in the cluster\n"
		"\t<cluster>.<proc>\tSpecific job\n"
//		"\t<owner>\t\t\tInformation about jobs owned by <owner>\n"
//		"\t-constraint <expr>\tInformation about jobs matching <expr>\n"
		"\n   and [options] are one or more of\n"
		"\t-help\t\t\tDisplay this screen\n"
		"\t-debug[:<level>]\t\t\tDebug logging. <level> sets debug level\n"
		//	"\t-inherit[:<ver>]\t\tUse socket inherited from parent\n" // for use when acting as spool server for the schedd
		, name);
  exit(iExitCode);
}


//------------------------------------------------------------------------
static bool diagnostic = false;
static const char * g_pool = nullptr;
static const char * g_name = nullptr;
static bool spool_helper = false;
static Stream* socks[2] = { NULL, NULL };
static std::vector<ClassAd*> g_jobads;

int getInheritedSocks(Stream* socks[], size_t cMaxSocks, pid_t & ppid)
{
	const char *envName = ENV_CONDOR_INHERIT;
	const char *inherit = GetEnv(envName);

	dprintf(D_FULLDEBUG, "condor_history: getInheritedSocks from %s is '%s'\n", envName, inherit ? inherit : "NULL");

	if ( ! inherit || ! inherit[0]) {
		socks[0] = NULL;
		return 0;
	}

	std::string psinful;
	std::vector<std::string> remaining_items; // for the remainder which we expect to be empty.
	int cSocks = extractInheritedSocks(inherit, ppid, psinful, socks, (int)cMaxSocks, remaining_items);
	UnsetEnv(envName); // prevent this from being passed on to children.
	return cSocks;
}

int spool_job_files(DCSchedd & schedd, std::vector<ClassAd*> & jobads)
{
	if (jobads.size() > 0) {
		CondorError errstack;

		// perhaps check for proper schedd version here?
		bool result = schedd.spoolJobFiles((int)jobads.size(), jobads.data(), &errstack);
		if ( ! result) {
			fprintf(stderr, "\n%s\n", errstack.getFullText(true).c_str() );
			fprintf(stderr, "ERROR: Failed to spool job files.\n" );
			exit(1);
		}
	}
	return 0;
}

static void refuse(ReliSock *sock)
{
	sock->encode();
	sock->put( NOT_OK );
	sock->end_of_message();
}

static bool add_to_classad_list(void* pv, ClassAd* ad) {
	std::vector<ClassAd*> * plist = (std::vector<ClassAd*>*)pv;
	if (spool_helper) { dprintf(D_ALWAYS, "got ad\n"); }
	plist->push_back(ad);
	return false; // return false to indicate we took ownership of the ad.
}

bool iter_ads_from_file(const char *filename,
	bool (*pfnProcess)(void* pv, ClassAd* ad), void* pvProcess,
	CondorClassAdFileParseHelper & parse_helper, ExprTree * constraint)
{
	FILE* fh;
	bool close_file = false;
	if (MATCH == strcmp(filename, "-")) {
		fh = stdin;
		close_file = false;
	} else {
		fh = safe_fopen_wrapper_follow(filename, "r");
		if (fh == NULL) {
			fprintf(stderr, "Can't open file of ClassAds: %s\n", filename);
			return false;
		}
		close_file = true;
	}

	CondorClassAdFileIterator adIter;
	if ( ! adIter.begin(fh, close_file, parse_helper)) {
		if (close_file) { fclose(fh); fh = NULL; }
		return false;
	} else {
		ClassAd * ad;
		while ((ad = adIter.next(constraint))) {
			if (pfnProcess(pvProcess, ad)) {
				delete ad; ad = NULL;
			}
		}
	}
	fh = NULL;
	return true;
}

// If original value exists as a renamed attr orig_attr (e.g SUBMIT_Iwd)
// Save the current value as save_attr (e.g. SPOOL_Iwd)
// and restore the original value to the given attr (e.g. Iwd)
//
void restore_submit_value(ClassAd & ad, const char * attr, const char * orig_attr, const char * save_attr)
{
	ExprTree * submit_val;
	ExprTree * orig_val;
	submit_val = ad.Remove(orig_attr);
	if (submit_val) {
		orig_val = ad.Remove(attr);
		if (orig_val) ad.Insert(save_attr, orig_val);
		ad.Insert(attr, submit_val);
	}
}
// Macro to help calling restore_submit_value(ad, "Iwd", "SUBMIT_Iwd", "SPOOL_Iwd") etc.
#define RESTORE(ad, attr) restore_submit_value(ad, attr, "SUBMIT_" attr, "SPOOL_" attr)

int
main(int argc, const char* argv[])
{
	bool server = false;
	bool dash_local = false; // set if -local is passed
	bool dash_inherit = false;
	const char * inherit_ver = nullptr;
	std::unique_ptr<CondorVersionInfo> peer_version;
	const char* jobads_file=nullptr;
	ClassAdFileParseType::ParseType jobads_file_format = ClassAdFileParseType::Parse_auto;
	const char * pcolon=NULL;
	std::vector<const char *> jobid_list;

	GenericQuery Query; // used to build a complex constraint.

	if (argc <= 1) {
		Usage(argv[0], 0);
		exit(0);
	}

	for(int i=1; i<argc; i++) {

		if (is_dash_arg_colon_prefix(argv[i], "jobads", &pcolon, 1)) {
			if (argc <= i+1) {
				fprintf( stderr, "Error: -jobads requires a filename\n");
				exit(1);
			} else {
				i++;
				jobads_file = argv[i];
			}
			if (pcolon) {
				jobads_file_format = parseAdsFileFormat(++pcolon, jobads_file_format);
			}
		}
		else if (is_dash_arg_colon_prefix(argv[i],"inherit", &pcolon, -1)) { // Purposefully undocumented (used by spool_helper)

			if (pcolon) {
				inherit_ver = ++pcolon;
				std::string ver("$CondorVersion: "); ver += inherit_ver; ver += "  $";
				peer_version.reset(new CondorVersionInfo(ver.c_str(), "peer"));
				if ( ! peer_version->is_valid(ver.c_str())) {
					// if peer version doesn't look like a valid version, just ignore it.
					peer_version.reset();
				}
			}

			dash_inherit = true;
			server = true;
			spool_helper = true;
		}
		else if (is_dash_arg_prefix(argv[i],"help",1)) {
			Usage(argv[0],0);
			exit(0);
		}
		else if (is_dash_arg_prefix(argv[i],"constraint",1)) {
			// make sure we have at least one more argument
			if (argc <= i+1) {
				fprintf( stderr, "Error: Argument %s requires another parameter\n", argv[i]);
				exit(1);
			}
			i++;
			Query.addCustomAND(argv[i]);
		}
		else if (is_dash_arg_colon_prefix(argv[i],"debug",&pcolon,3)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		}
		else if (is_dash_arg_prefix(argv[i],"diagnostic",4)) { // Purposefully undocumented (Intended internal use)
				// dprintf to console
				diagnostic = true;
		}
		else if (is_dash_arg_prefix(argv[i],"local",2)) {
			// -local overrides the existance of SCHEDD_HOST/SCHEDD_NAME and forces a local query
			if (g_name) {
				fprintf(stderr, "Error: Arguments -local and -name cannot be used together\n");
				exit(1);
			}
			dash_local = true;
		}
		else if (is_dash_arg_prefix(argv[i], "name", 1)) {
			i++;
			if (argc <= i)
			{
				fprintf(stderr,
					"Error: Argument -name requires name of a remote schedd\n");
				fprintf(stderr,
					"\t\te.g. condor_history -name submit.example.com \n");
				exit(1);
			}
			if (dash_local) {
				fprintf(stderr, "Error: -local and -name cannot be used together\n");
				exit(1);
			}
			g_name = argv[i];
		}
		else if (is_dash_arg_prefix(argv[i], "pool", 1)) {
			i++;
			if (argc <= i)
			{
				fprintf(stderr,
					"Error: Argument -name requires name of a remote schedd\n");
				fprintf(stderr,
					"\t\te.g. condor_history -name submit.example.com \n");
				exit(1);
			}
			g_pool = argv[i];
		}
		else if (argv[i][0] == '-') {
			fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
			Usage(argv[0]); // quitting now will print usage and exit with an error below.
			break;
		} else {
			jobid_list.push_back(argv[i]);
		}
	}

	// process the args that are probably job ids or cluster ids
	for (auto * idstr : jobid_list) {
		JOB_ID_KEY jid(0,0);
		if (StrIsProcId(idstr, jid.cluster, jid.proc)) {
			std::string jobmatch;
			if (jid.cluster > 0) {
				jobmatch = ATTR_CLUSTER_ID "==" + std::to_string(jid.cluster);
				if (jid.proc >= 0) jobmatch += "&& " ATTR_PROC_ID "==" + std::to_string(jid.proc);
				Query.addCustomOR(jobmatch.c_str());
			}
		} else {
			// treat as owner?
			fprintf(stderr, "Error: unknown argument %s\n", idstr);
			exit(1);
		}
	}

	set_priv_initialize(); // allow uid switching if root
	config();

	if (dash_inherit) {
		// Start writing to the ToolLog
		dprintf_config("Tool");

		if (IsFulldebug(D_ALWAYS)) {
			std::string myargs;
			for (int ii = 0; ii < argc; ++ii) { formatstr_cat(myargs, "[%d]%s ", ii, argv[ii]); }
			dprintf(D_FULLDEBUG, "spool helper args: %s\n", myargs.c_str());
		}

		pid_t ppid;
		if ( ! getInheritedSocks(socks, COUNTOF(socks), ppid)) {
			dprintf(D_ERROR, "could not parse inherited sockets from environment, aborting\n");
			exit(1);
		}
		if ( ! socks[0] || socks[0]->type() != Stream::reli_sock) {
			dprintf(D_ERROR, "first inherited socket is not a ReliSock, aborting\n");
			exit(1);
		}
	}

	int result = 0; // exit code

	ConstraintHolder constr;
	std::string my_constraint;
	Query.makeQuery(my_constraint);
	if ( ! my_constraint.empty()) {
		if (diagnostic) { fprintf(stderr, "Using effective constraint: %s\n", my_constraint.c_str()); }
		if ( ! constr.parse(my_constraint.c_str())) {
			fprintf(stderr, "Error: Could not parse constraint %s\n", my_constraint.c_str());
			exit(1);
		}
	}

	DCSchedd schedd(g_name, g_pool);

	if (jobads_file) {
		if (spool_helper) { dprintf(D_ALWAYS, "reading ads from %s\n", jobads_file); }
		CondorClassAdFileParseHelper jobads_file_parse_helper("\n", jobads_file_format);
		if ( ! iter_ads_from_file(jobads_file, add_to_classad_list, &g_jobads, jobads_file_parse_helper, constr.Expr())) {
			if (spool_helper) { dprintf(D_ERROR, "failed to read ads, aborting\n"); }
			exit(1);
		}
	} else {
		// get ads by querying the schedd
		if (dash_local) {
			schedd.locate_local();
		} else {
			schedd.locate(Daemon::LOCATE_FOR_LOOKUP);
		}

		int cmd = QUERY_JOB_ADS;
		if (schedd.canUseQueryWithAuth()) {
			cmd = QUERY_JOB_ADS_WITH_AUTH;
		}

		ClassAd request_ad;
		const int fetch_opts = 0; //QueryFetchOpts::fetch_ClusterBeforeJob | QueryFetchOpts::fetch_IncludeClusterAd;
		const char * projection = nullptr;
		int match_limit = -1;
		const char * my_jobs_owner = nullptr;
		schedd.makeJobsQueryAd(request_ad, my_constraint.c_str(), projection, fetch_opts, match_limit, my_jobs_owner);

		int q_query_timeout = param_integer("Q_QUERY_TIMEOUT", 20);
		CondorError errstack;
		ClassAd *summary_ad = nullptr;
		result = schedd.queryJobs(cmd, request_ad, add_to_classad_list, &g_jobads, q_query_timeout, &errstack, &summary_ad);
		if (result != Q_OK) {
			const char * addr = schedd.addr();
			const char * name = schedd.name();
			if ( ! addr) addr = "";
			if ( ! name) name = "schedd";

			// The parse + fetch failed, print out why
			switch(result) {
			case Q_UNSUPPORTED_OPTION_ERROR:
				fprintf(stderr, "\n-- Unsupported query option at: %s : %s\n", addr ? addr : "", name ? name : "");
				break;
			default:
				fprintf(stderr,
					"\n-- Failed to fetch ads from: %s : %s\n\n%s\n",
					addr, name, errstack.getFullText(true).c_str() );
			}
			exit (1);
		}
	}

	if (server) {
		// handle sever side of spooling (spool_helper on behalf of the schedd)

		ReliSock* sock = (ReliSock*)socks[0];
		time_t old_timeout = sock->timeout(60 * 60 * 8);

		// priv state - copied without understanding from Scheduler::generalJobFilesWorkerThread
		priv_state xfer_priv = PRIV_UNKNOWN;
	#if !defined(WIN32)
		xfer_priv = PRIV_USER;
	#endif

		for (auto & jobad : g_jobads) {
			JOB_ID_KEY jid{-1,-1};
			jobad->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
			jobad->LookupInteger(ATTR_PROC_ID, jid.proc);

			FileTransfer ftrans;
			const bool want_check_perms = true;
			const bool is_server = true;
			const bool use_catalog = false;
			//const bool is_spool = true; // note Scheduler::generalJobFilesWorkerThread defaults this to false??
			int success = ftrans.SimpleInit(jobad, want_check_perms, is_server,
				sock, xfer_priv, use_catalog /*, is_spool*/);
			if (success) {
				if (peer_version.get()) {
					ftrans.setPeerVersion(*peer_version.get());
				} else if (inherit_ver) {
					dprintf(D_ERROR, "Ignoring invalid version in -inherit:%s\n", inherit_ver);
				}

				// DownloadFiles returns 1 on success, 0 on failure
				success = ftrans.DownloadFiles();
				if (success) {
					/*
					TemporaryPrivSentry old_priv;
					if ( xfer_priv == PRIV_USER ) {
						set_user_priv();
					}
					AuditLogJobProxy( *sock, ad );
					*/
				}
			}
			if ( ! success) {
				dprintf(D_AUDIT | D_ERROR_ALSO, *sock, "generalJobFilesWorkerThread(): "
					"failed to transfer files for job %d.%d \n",
					jid.cluster, jid.proc);
				sock->timeout( 10 ); // avoid hanging due to huge timeout
				refuse(sock);
				sock->timeout(old_timeout);
				break;
			}
		} // while

		sock->end_of_message();
		int answer = OK;
		sock->encode();
		if ( ! sock->code(answer)) {
			dprintf(D_ERROR,"generalJobFilesWorkerThread(): cannot send answer to client\n");
		}
		sock->end_of_message();
		sock->timeout(old_timeout);

		dprintf( D_AUDIT, *sock, (answer==OK) ? "Transfer completed\n" : "Error received from client\n" );
		dprintf( D_ALWAYS, (answer==OK) ? "Transfer completed\n" : "Error received from client\n" );

		result = (answer == OK) ? 0 : 1;

		sock->close();

	} else {

		// we (may) have to fixup the fetched ads since they may have been changed by the schedd
		// to refer to spool. We need the original attribute values for the client side of the conversation
		// which sould have been copied to SUBMIT_<attr> (e.g. SUBMIT_Iwd)
		// OTOH, -jobads may be reading the output of condor_submit -verbose, in which case the ad
		// will not have been rewritten by the schedd (although the schedd's copy will have been)
		// the RESTORE macro works in either case as long as the SUBMIT_* attributes are not present
		// in jobs ads that have not been re-written, or are present but contain the original submit value.
		for (auto * ad : g_jobads) {
			RESTORE(*ad, ATTR_JOB_IWD);
			RESTORE(*ad, ATTR_JOB_CMD);
			RESTORE(*ad, ATTR_JOB_INPUT);
			RESTORE(*ad, ATTR_TRANSFER_INPUT_FILES);
			RESTORE(*ad, ATTR_ULOG_FILE);
			RESTORE(*ad, ATTR_X509_USER_PROXY);
		}

		fprintf(stderr, "Spooling input for %d jobs\n", (int)g_jobads.size());
		result = spool_job_files(schedd, g_jobads);
	}

	return result;
}

