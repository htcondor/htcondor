/***************************************************************
 *
 * Copyright (C) 1990-2019, Condor Team, Computer Sciences Department,
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

#define FACTORY_TEST 1
#define SetAttributeInt FakeSetAttrInt
#define GetAttributeInt FakeGetAttrInt
#define SetSecureAttributeInt FakeSetAttrInt
int FakeSetAttrInt(int cluster_id, int proc_id, const char *attr, int value, unsigned int flags);
int FakeGetAttrInt(int cluster_id, int proc_id, char const *attr, int *value);
#include "../condor_schedd.V6/qmgmt_factory.cpp"

#include "subsystem_info.h"
#include "env.h"
#include "basename.h"
#include "condor_getcwd.h"
#include "condor_config.h"
#include "match_prefix.h"
#include "MapFile.h"
#include "directory.h"
#include "filename_tools.h"
#include "condor_holdcodes.h"
#include "condor_version.h"
#include <my_async_fread.h>
#include <submit_utils.h>
#include "classad_helpers.h"
#include <param_info.h> // for BinaryLookup


class JobQueueCluster * ClusterObj = nullptr;

int FakeSetAttrInt(int cluster_id, int /*proc_id*/, const char *attr, int value, unsigned int /*flags*/)
{
	if (ClusterObj && (cluster_id == ClusterObj->jid.cluster)) {
		ClusterObj->Assign(attr, value);
		return 0;
	}
	return -1;
}

int FakeGetAttrInt(int cluster_id, int /*proc_id*/, char const *attr, int *value)
{
	if (ClusterObj && (cluster_id == ClusterObj->jid.cluster)) {
		ClusterObj->LookupInteger(attr, *value);
		return 0;
	}
	return -1;
}

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
// force value of 1 is proc ad, -1 is cluster ad.
// NOTE: this table MUST be sorted by case-insensitive attribute name
#define FILL(attr,force) { attr, force }
static const ATTR_FORCE_PAIR aForcedSetAttrs[] = {
	FILL(ATTR_CLUSTER_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_NAME,       -1), // forced into cluster ad
	FILL(ATTR_JOB_STATUS,         1),  // forced into proc ad
	FILL(ATTR_JOB_UNIVERSE,       -1), // forced into cluster ad
	FILL(ATTR_NT_DOMAIN,          -1), // forced into cluster ad
	FILL(ATTR_OWNER,              -1), // forced into cluster ad
	FILL(ATTR_PROC_ID,            1),  // forced into proc ad
	FILL(ATTR_USER,              -1), // forced into cluster ad
};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
static int IsForcedProcAttribute(const char *attr)
{
	const ATTR_FORCE_PAIR* found = nullptr;
	found = BinaryLookup<ATTR_FORCE_PAIR>(
		aForcedSetAttrs,
		COUNTOF(aForcedSetAttrs),
		attr, strcasecmp);
	if (found) {
		return found->forced;
	}
	return 0;
}

void usage(FILE* out);

const char	*MyName = "job_factory";
static FILE * NewProcOutFp = nullptr;
static bool verbose = false;
static bool output_full_ads = false;
static bool output_proc_ads_only = false;

// This is a modified copy-pasta of NewProcFromAd in qmgmt.cpp
// this function is called by MaterializeNextFactoryJob
int NewProcFromAd (const classad::ClassAd * ad, int ProcId, JobQueueCluster * ClusterAd, SetAttributeFlags_t /*flags*/)
{
	int ClusterId = ClusterAd->jid.cluster;
	ClassAd effective_proc_ad;
	const bool full_ad = output_full_ads || ((ProcId == 0) && ! output_proc_ads_only);

	//int num_procs = IncrementClusterSize(ClusterId);
	//ClusterAd->SetClusterSize(num_procs);
	//NewProcInternal(ClusterId, ProcId);

	//int rval = SetAttributeInt(ClusterId, ProcId, ATTR_PROC_ID, ProcId, flags | SetAttribute_LateMaterialization);
	//if (rval < 0) {
	//	dprintf(D_ALWAYS, "ERROR: Failed to set ProcId=%d for job %d.%d (%d)\n", ProcId, ClusterId, ProcId, errno );
	//	return rval;
	//}

	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );
	std::string buffer;

	// the EditedClusterAttrs attribute of the cluster ad is the list of attibutes where the value in the cluster ad
	// should win over the value in the proc ad because the cluster ad was edited after submit time.
	// we do this so that "condor_qedit <clusterid>" will affect ALL jobs in that cluster, not just those that have
	// already been materialized.
	classad::References clusterAttrs;
	if (ClusterAd->LookupString(ATTR_EDITED_CLUSTER_ATTRS, buffer)) {
		add_attrs_from_string_tokens(clusterAttrs, buffer);
	}

	for (const auto & it : *ad) {
		const std::string & attr = it.first;
		const ExprTree * tree = it.second;
		if (attr.empty() || ! tree) {
			dprintf(D_ALWAYS, "ERROR: Null attribute name or value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		}

		// check if attribute is forced to either cluster ad (-1) or proc ad (1)
		bool send_it = false;
		int forced = IsForcedProcAttribute(attr.c_str());
		if (forced) {
			send_it = forced > 0;
		} else if (clusterAttrs.find(attr) != clusterAttrs.end()) {
			send_it = false; // this is a forced cluster attribute
		} else {
			// If we aren't going to unconditionally send it, when we check if the value
			// matches the value in the cluster ad.
			// If the values match, don't add the attribute to this proc ad
			ExprTree *cluster_tree = ClusterAd->Lookup(attr);
			send_it = ! cluster_tree || ! (*tree == *cluster_tree || *tree == *SkipExprEnvelope(cluster_tree));
		}
		if ( ! send_it)
			continue;

		buffer.clear();
		unparser.Unparse(buffer, tree);
		if (buffer.empty()) {
			dprintf(D_ALWAYS, "ERROR: Null value for job %d.%d\n", ClusterId, ProcId );
			return -1;
		} else if (full_ad) {
			effective_proc_ad.Insert(attr, tree->Copy());
		} else {
			fprintf(NewProcOutFp, "%s=%s\n", attr.c_str(), buffer.c_str());
		}
	}

	if (full_ad) {
		buffer.clear();
		effective_proc_ad.ChainToAd(ClusterAd);
		formatAd(buffer, effective_proc_ad, nullptr, nullptr, false);
		fputs(buffer.c_str(), NewProcOutFp);
		effective_proc_ad.Unchain();
	}

	return 0;
}

// start a transaction, or continue one if we already started it
int TransactionWatcher::BeginOrContinue(int id) {
	ASSERT(! completed);
	if (started) {
		lastid = id;
		return 0;
	} else {
		//ASSERT( ! JobQueue->InTransaction());
	}
	started = true;
	firstid = lastid = id;
	return 0;
}

// commit if we started a transaction
int TransactionWatcher::CommitIfAny(SetAttributeFlags_t /*flags*/, CondorError * /*errorStack*/)
{

	int rval = 0;
	if (started && ! completed) {
		//rval = CommitTransactionAndLive(flags, errorStack);
		if (rval == 0) {
			// Only set the completed flag if our CommitTransaction succeeded... if
			// it failed (perhaps due to SUBMIT_REQUIREMENTS not met), we cannot mark it
			// as completed because that means we will not abort it in the 
			// Transactionwatcher::AbortIfAny() method below and will likely ASSERT.
			completed = true;
		}
	}
	return rval;
}

// cancel if we started a transaction
int TransactionWatcher::AbortIfAny()
{
	int rval = 0;
	if (started && ! completed) {
		//ASSERT(JobQueue->InTransaction());
		//rval = AbortTransaction();
		completed = true;
	}
	return rval;
}

void JobQueueJob::PopulateFromAd() {}
void JobQueueBase::PopulateFromAd() {}
JobQueueCluster::~JobQueueCluster() {
	if (this->factory) {
		DestroyJobFactory(this->factory);
	}
	this->factory = nullptr;
}


bool setJobFactoryPauseAndLog(JobQueueCluster * cad, int pause_mode, int /*hold_code*/, const std::string& reason)
{
	if ( ! cad) return false;

	cad->Assign(ATTR_JOB_MATERIALIZE_PAUSED, pause_mode);
	if (reason.empty() || pause_mode == mmRunning) {
		cad->Delete(ATTR_JOB_MATERIALIZE_PAUSE_REASON);
	} else {
		cad->Assign(ATTR_JOB_MATERIALIZE_PAUSE_REASON, reason);
	}

	if (cad->factory) {
		// make sure that the factory state is in sync with the pause mode
		CheckJobFactoryPause(cad->factory, pause_mode);
	}

	// log the change in pause state
	//const char * reason_ptr = reason.empty() ? NULL : reason.c_str();
	//scheduler.WriteFactoryPauseToUserLog(cad, hold_code, reason_ptr);
	return true;
}

int
main( int argc, const char *argv[] )
{
	const char **ptr;
	const char *pcolon = nullptr;
	const char *digest_file = nullptr;
	const char *clusterad_file = nullptr;
	const char *items_file = nullptr;
	const char *output_file = nullptr;
	ClassAdFileParseType::ParseType clusterad_format = ClassAdFileParseType::Parse_long;
	std::string errmsg;

	set_mySubSystem( "SUBMIT", false, SUBSYSTEM_TYPE_SUBMIT );

	MyName = condor_basename(argv[0]);
	config();


	for( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
		if( ptr[0][0] == '-' ) {
			if (MATCH == strcmp(ptr[0], "-")) { // treat a bare - as a submit filename, it means read from stdin.
				digest_file = ptr[0];
			} else if (is_dash_arg_prefix(ptr[0], "verbose", 1)) {
				verbose = true;
			} else if (is_dash_arg_colon_prefix(ptr[0], "debug", &pcolon, 3)) {
				// dprintf to console
				dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
			} else if (is_dash_arg_colon_prefix(ptr[0], "clusterad", &pcolon, 1)) {
				if (!(--argc) || !(*(++ptr))) {
					fprintf(stderr, "%s: -clusterad requires another argument\n", MyName);
					exit(1);
				}
				clusterad_file = *ptr;
				if (pcolon) {
					clusterad_format = parseAdsFileFormat(++pcolon, ClassAdFileParseType::Parse_long);
				}
			} else if (is_dash_arg_prefix(ptr[0], "digest", 1)) {
				if (!(--argc) || !(*(++ptr))) {
					fprintf(stderr, "%s: -digest requires another argument\n", MyName);
					exit(1);
				}
				digest_file = *ptr;
			} else if (is_dash_arg_prefix(ptr[0], "items", 1)) {
				if (!(--argc) || !(*(++ptr))) {
					fprintf(stderr, "%s: -items requires another argument\n", MyName);
					exit(1);
				}
				items_file = *ptr;
			} else if (is_dash_arg_colon_prefix(ptr[0], "out", &pcolon, 1)) {
				bool needs_file_arg = true;
				if (pcolon) { 
					for (const auto& opt: StringTokenIterator(++pcolon)) {
						if (is_arg_prefix(opt.c_str(), "full", -1)) {
							output_full_ads = true;
						} else if (is_arg_prefix(opt.c_str(), "proc-only", 4)) {
							output_proc_ads_only = true;
						}
					}
				}
				if (needs_file_arg) {
					const char * pfilearg = ptr[1];
					if ( ! pfilearg || (*pfilearg == '-' && (MATCH != strcmp(pfilearg,"-"))) ) {
						fprintf( stderr, "%s: -out requires a filename argument\n", MyName );
						exit(1);
					}
					output_file = pfilearg;
					--argc; ++ptr;
				}
			} else if (is_dash_arg_prefix(ptr[0], "help")) {
				usage(stdout);
				exit( 0 );
			} else {
				usage(stderr);
				exit( 1 );
			}
		} else {
			digest_file = *ptr;
		}
	}

	ClassAd extendedSubmitCmds;
	auto_free_ptr extended_cmds(param("EXTENDED_SUBMIT_COMMANDS"));
	if (extended_cmds) {
		initAdFromString(extended_cmds, extendedSubmitCmds);
		extended_cmds.clear();
	}

	if ( ! clusterad_file) {
		fprintf(stderr, "\nERROR: no clusterad filename provided\n");
		exit(1);
	}

	FILE* file = NULL;
	bool close_file = false;
	if (MATCH == strcmp(clusterad_file, "-")) {
		file = stdin;
		close_file = false;
	} else {
		file = safe_fopen_wrapper_follow(clusterad_file, "rb");
		if (! file) {
			fprintf(stderr, "could not open %s: error %d - %s\n", clusterad_file, errno, strerror(errno));
			exit(1);
		}
		close_file = true;
	}

	CondorClassAdFileIterator adIter;
	if (! adIter.begin(file, close_file, clusterad_format)) {
		fprintf(stderr, "Unexpected error reading clusterad: %s\n", clusterad_file);
		if (close_file) { fclose(file); file = NULL; }
		exit(1);
	}
	file = NULL; // the adIter will close the clusterAd file handle when it is destroyed.
	close_file = false;

	int cluster_id = -1;
	ClassAd * ad = adIter.next(NULL);
	if ( ! ad || !ad->LookupInteger(ATTR_CLUSTER_ID, cluster_id)) {
		fprintf(stderr, "ERROR: %s did not contain a job Cluster classad", clusterad_file);
		exit(1);
	}

	JOB_ID_KEY cluster_jid(cluster_id, -1);
	ClusterObj = new JobQueueCluster(cluster_jid);
	ClusterObj->Update(*ad); delete ad; ad = nullptr;

	if (! digest_file) {
		fprintf(stderr, "\nERROR: no digest filename provided\n");
		exit(1);
	}

	MapFile* protected_url_map = getProtectedURLMap();
	JobFactory * factory = new JobFactory(digest_file, cluster_id, &extendedSubmitCmds, protected_url_map);

	if (items_file) {
		file = safe_fopen_wrapper_follow(items_file, "rb");
		if (! file) {
			fprintf(stderr, "could not open %s: error %d - %s\n", items_file, errno, strerror(errno));
			exit(1);
		}

		// read the items and append them in 64k (ish) chunks to replicate the way the data is sent to the schedd by submit
		const size_t cbAlloc = 0x10000;
		char buf[cbAlloc];
		std::string remainder;
		size_t cbread;
		size_t off = 0;
		while ((cbread = fread(buf, 1, sizeof(buf), file)) > 0) {
			if (AppendRowsToJobFactory(factory, buf, cbread, remainder) < 0) {
				fprintf(stderr, "ERROR: could not append %d bytes of itemdata to factory at offset %d\n", (int)cbread, (int)off);
				exit(1);
			}
			off += cbread;
		}

		fclose(file);
	}

	file = safe_fopen_wrapper_follow(digest_file, "rb");
	if (! file) {
		fprintf(stderr, "could not open %s: error %d - %s\n", digest_file, errno, strerror(errno));
		exit(1);
	}
	close_file = true;

	// the SetJobFactory RPC does this
	MacroStreamYourFile ms(file, factory->source);
	if (factory->LoadDigest(ms, nullptr, cluster_id, errmsg) != 0) {
		fprintf(stderr, "\nERROR: failed to parse submit digest for factory %d : %s\n", factory->getClusterId(), errmsg.c_str());
		exit(1);
	}
	// It also does this where first_proc_id is an argument passed through the RPC and always 0 (for now)
	if ( ! ClusterObj->Lookup(ATTR_JOB_MATERIALIZE_NEXT_PROC_ID)) {
		const int first_proc_id = 0;
		ClusterObj->Assign(ATTR_JOB_MATERIALIZE_NEXT_PROC_ID, first_proc_id);
	}

	// now that the digest is loaded, attach the cluster ad to the factory
	// this will populate some stuff in the factory from the cluster ad
	// and recompute the IDW from the digest and the cluster ad.
	AttachJobFactoryToCluster(factory, ClusterObj);

	FILE* out = stdout;
	bool free_out = false;
	if (output_file) {
		if (MATCH == strcmp(output_file, "-")) {
			out = stdout; free_out = false;
		} else {
			out = safe_fopen_wrapper_follow(output_file, "wb");
			if (! out) {
				fprintf(stderr, "\nERROR: Failed to open file -dry-run output file (%s)\n", strerror(errno));
				exit(1);
			}
			free_out = true;
		}
	}

	TransactionWatcher txn;
	NewProcOutFp = out;

	factory->attachTransferMap(protected_url_map);
	// ok, factory initialized, now materialize them jobs
	int num_jobs = 0;
	for (;;) {
		int retry_delay = 0; // will be set to non-zero when we should try again later.
		int rv = MaterializeNextFactoryJob(factory, ClusterObj, txn, retry_delay);
		if (rv >= 1) {
			// made a job.
			if (verbose) {
				int first,last;
				std::string range;
				if (txn.GetIdRange(first, last)) {
					formatstr(range, " %d.%d", cluster_id, last);
				}
				fprintf(stdout, "# %d job(s) : %s\n", rv, range.c_str());
			} else {
				fputs(".", stdout); // duplicate the behavior of condor_submit
			}
			num_jobs += rv;
		} else if (retry_delay > 0) {
			// we are delayed by itemdata loading, try again later
			if (verbose) fprintf(stdout, "# retry in %d seconds\n", retry_delay);
			if (retry_delay > 100) {
				fprintf(stdout, "# delay > 100 indicates factory paused, quitting\n");
				break;
			}
			sleep(retry_delay);
			continue;
		} else {
			if (verbose) { fprintf(stdout, "# no more jobs to materialize\n"); }
			break;
		}
		if (out) fputs("\n", out);
	}

	fprintf(stdout, "%d job(s) materialized to cluster %d.\n", num_jobs, cluster_id);

	NewProcOutFp = nullptr;
	if (free_out && out) {
		fclose(out); out = NULL;
	}
	factory->detachTransferMap();

	if (protected_url_map) {
		delete protected_url_map;
		protected_url_map = nullptr;
	}

	// this also deletes the factory object
	delete ClusterObj;
	return 0;
}

void usage(FILE* out)
{
	fprintf(out, "Usage: %s [options] -clusterad <adfile> -digest <digest-file>\n", MyName);
	fprintf(out, "  -clusterad[:<form>] <adfile>\tRead The ClusterAd <classad-file> as format <form>\n");
	fprintf(out, "  -digest <subfile>\t\tRead Submit digest commands from <subfile>\n");
	fprintf(out, "    [options] are\n");
	fprintf(out, "  -items <items-file>\tRead Submit itemdata from <items-file>\n");
	fprintf(out, "  -out[:full] <outfile>\t\tWrite materialized job attributes to <outfile>\n");
	fprintf(out, "  -verbose\t\tDisplay verbose output, jobid and materialization steps\n");
	fprintf(out, "  -debug  \t\tDisplay debugging output\n");
	fprintf(out,"\n"
		"%s is a tool for simulating late-materialization outside the schedd.  To use it\n"
		"first create a cluster ad file and submit digest file by running condor_submit -dry:\n"
		"\n\tcondor_submit -dry-run:cluster=100 100.ad -factory -digest 100.digest <submit-file>\n\n"
		"Then pass the cluster ad and digest files to this tool like this:\n"
		"\n\t%s -clusterad 100.ad -digest 100.digest -out 100.ads\n",
		MyName, MyName
	);

}

