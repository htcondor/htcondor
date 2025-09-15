/***************************************************************
 *
 * Copyright (C) 2016, Condor Team, Computer Sciences Department,
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

// unit tests for the functions related to the MACRO_SET data structure
// used by condor's configuration and submit language.

#include "condor_common.h"
#include "condor_config.h"
#include "param_info.h"
#include "match_prefix.h"
#include "condor_random_num.h" // so we can force the random seed.
#include "MyString.h"
#include "xform_utils.h"
#include "condor_attributes.h"
#include "condor_holdcodes.h"
#include <map>

class YourString ystr;    // the result we are comparing
class YourString ycheck;  // the name of the check
class YourString wantstr; // the result we want
bool   grequire_failed;
double gstart_time; // start time for REQUIRE condition
double gelapsed_time;  // elapsed time for REQUIRE condition

bool dash_verbose = false;
bool dash_verbose2 = false;
int fail_count = 0;

template <class c>
bool within(YourString /*ys*/, c /*lb*/, c /*hb*/) { return false; }

template <> bool within(YourString ys, int lb, int hb) {
	int v = atoi(ys.Value());
	//fprintf(stderr, "within '%s' -> %d\n", ys.Value(), v);
	return v >= lb && v <= hb;
}

template <> bool within(YourString ys, const char* lb, const char* hb) {
	return strcmp(ys.Value(),lb) >= 0 && strcmp(ys.Value(),hb) <= 0;
}

#define REQUIRE( condition ) \
	gstart_time = _condor_debug_get_time_double(); \
	grequire_failed = ! ( condition ); \
	gelapsed_time = _condor_debug_get_time_double() - gstart_time; \
	if (grequire_failed) { \
		fprintf( stderr, "%s Failed %5d: %s", ycheck.c_str(), __LINE__, #condition ); \
		if ( ! ystr.empty()) fprintf( stderr,    "\n             :   actual value: %s", ystr.c_str()); \
		if ( ! wantstr.empty()) fprintf( stderr, "\n             : expected value: %s", wantstr.c_str()); \
		fprintf(stderr, "\n"); \
		++fail_count; \
	} else if( verbose ) { \
		fprintf( stdout, "    OK %s %5d: %s \t# %s\n", ycheck.c_str(), __LINE__, #condition, ystr.c_str()); \
	}

// returns true if ads are the (shallowly) the same
bool ads_are_same(const ClassAd & aa, const ClassAd & bb)
{
	std::string buf;
	int differences = 0;
	for (auto & [k,ea] : aa) {
		auto found = bb.find(k);
		if (found == bb.end()) {
			++ differences;
			if (dash_verbose) { fprintf(stderr, "%s=%s MISSING KEY\n", k.c_str(), ExprTreeToString(ea)); }
		}
		else if (*found->second != *ea) {
			++ differences;
			if (dash_verbose) { 
				fprintf(stderr, "%s=%s  UNEQUAL %s\n", k.c_str(), ExprTreeToString(ea), ExprTreeToString(found->second, buf));
			}
		}
	}

	// check for keys in bb that are not in aa
	for (auto & [k,eb] : bb) {
		auto found = aa.find(k);
		if (found == aa.end()) {
			++ differences;
			if (dash_verbose) { fprintf(stderr, "MISSING KEY %s=%s\n", k.c_str(), ExprTreeToString(eb)); }
		}
	}

	return differences == 0;
}

static const char basic_jobdata[] =R"(
Args = "hello"
ClusterId = 241
Cmd = "/bin/echo"
DiskUsage = 200
DiskUsage_RAW = 180
Environment = ""
Err = "echo.err"
ExecutableSize = 175
ExecutableSize_RAW = 162
FirstJobMatchDate = 1756503379
ImageSize = 175000
ImageSize_RAW = 162000
In = "/dev/null"
Iwd = "/home/bob/test"
JobPrio = 0
JobStatus = 2
JobSubmitFile = "echo.sub"
JobSubmitMethod = 0
JobUniverse = 5
LeaveJobInQueue = false
MaxHosts = 1
MemoryUsage = ((ResidentSetSize + 1023) / 1024)
MinHosts = 1
MyType = "Job"
NTDomain = "CRANE"
OsUser = "bob@crane"
Out = "echo.out"
Owner = "bob"
QDate = 1756503379
Rank = 0.0
RemoteSysCpu = 0.0
RemoteUserCpu = 0.0
RequestCpus = 1
RequestMemory = 50
RequestDisk = MAX({ 1024,(TransferInputSizeMB + 1) * 1.25 }) * 1024
Requirements = (Arch != "ARM") && (TARGET.OpSys == "WINDOWS") && (TARGET.Disk >= RequestDisk) && (TARGET.Memory >= RequestMemory) && (TARGET.HasFileTransfer)
ShouldTransferFiles = "YES"
SpooledOutputFiles = ""
StreamErr = false
StreamOut = false
TargetType = "Machine"
TotalSubmitProcs = 1
TransferIn = false
TransferInputSizeMB = 1
User = "bob@crane"
UserLog = "/home/bob/test/echo.log"
WhenToTransferOutput = "ON_EXIT"

ProcId = 0
GlobalJobId = "ap@crane#241.0#1756503379"
EnteredCurrentStatus = 1756504405
ImageSize = 250000
ImageSize_RAW = 245052
InitialWaitDuration = 5
JobRunCount = 1
JobStartDate = 1756503384
JobStatus = 2
LastJobStatus = 1
LastMatchTime = 1756503769
LastPublicClaimId = "<192.168.122.62:7107?addrs=192.168.122.62-7107&alias=crane&noUDP&sock=startd_27800_5bc6>#1756501661#26#..."
LastRemoteHost = "slot1_6@crane"
LastRemoteWallClockTime = 636.0
LastSuspensionTime = 0
LastVacateTime = 1756503719
MachineAttrCpus0 = 1
MachineAttrSlotWeight0 = 1
MemoryProvisioned = 384
NumCkpts = 0
NumCkpts_RAW = 0
NumJobCompletions = 1
NumJobMatches = 1
NumJobStarts = 1
NumRestarts = 0
NumShadowStarts = 1
NumSystemHolds = 0
NumVacates = 1
NumVacatesByReason = [ StartdHeldJob = 1 ]
OrigMaxHosts = 1
RemoteWallClockTime = 852.0
ResidentSetSize = 250000
ResidentSetSize_RAW = 245052
ScratchDirFileCount = 10
StartdPrincipal = "execute-side@matchsession/192.168.100.1"
StatsLifetimeStarter = 630
StderrMtime = 1756503775
StdoutMtime = 1756504405
TerminationPending = true
TotalSuspensions = 0
TransferInFinished = 1756503774
TransferInputFileCounts = [ CEDAR = 1 ]
TransferInputStats = [ CedarSizeBytesTotal = 663552; CedarFilesCountTotal = 4; CedarSizeBytesLastRun = 165888; CedarFilesCountLastRun = 1 ]
TransferOutFinished = 1756504405
TransferOutputStats = [ CedarSizeBytesTotal = 657; CedarFilesCountTotal = 2; CedarSizeBytesLastRun = 657; CedarFilesCountLastRun = 2 ]
VacateReasonCode=21
VacateReasonSubCode=102
)"
"\0"  // null to terminate cluster+proc
"\0\0"; // empty record at the end

// changes to apply to the basic_jobdata for nextmax test
static const char nextmax_jobdata[] =R"(
ClusterId = 241
NextRequestMemory = RequestMemory + 200
MaxRequestMemory = 300
#MaxRequestMemory = 1024
)"
"\0"  // null to terminate cluster+proc
"\0\0"; // empty record at the end

// changes to apply to the basic_jobdata for seq test
static const char seq_jobdata[] =R"(
ClusterId = 241
RetryRequestMemory = { 250, 500 }
)"
"\0"  // null to terminate cluster+proc
"\0\0"; // empty record at the end

// changes to apply to the basic_jobdata for seq test
static const char env_jobdata[] =R"(
ClusterId = 241
Environment = "FOO=BOO INFO=banner"
)"
"\0"  // null to terminate cluster+proc
"\0\0"; // empty record at the end

typedef ClassAd JobQueueJob;

#define JOB_SHOULD_REQUEUE 107
#define JOB_SHOULD_HOLD    112

int TransformOnEvict(JobQueueJob * job, const char * name, const char * xform_body, int shadow_code, std::string & job_id, XFormAdUndo & undo)
{
	XFormHash mset;
	mset.init();

	MacroStreamXFormSource xfm(name);
	std::string errmsg;
	int rval = xfm.set_simple_transform(xform_body); // returns -1 for bad requirements and -2 for empty transform
	if (rval < 0) {
		if (rval == -1) {
			dprintf(D_ALWAYS, "TransformOnEvict %s invalid requirements in %s\n", job_id.c_str(), name);
		}
		return rval;
	}
	errmsg.clear();
	if ( ! xfm.matches(job)) {
		dprintf(D_ALWAYS, "TransformOnEvict %s does not match %s requirements\n", job_id.c_str(), name);
		return shadow_code;
	} else {
		if (xfm.getRequirements()) {
			dprintf(D_ALWAYS, "TransformOnEvict %s MATCHES %s requirements\n", job_id.c_str(), name);
		}
	}

	// XFormAdUndo undo;

	int flags = XFORM_UTILS_LOG_ERRORS | XFORM_UTILS_LOG_STEPS | XFORM_UTILS_LOG_TO_DPRINTF;
	rval = TransformClassAd(job, xfm, mset, errmsg, flags, &undo);
	//if (rval < 0) undo.Revert(*job);
	if (rval < 0) {
		// Transformation failed; errmsg should say why.
		// TODO errorStack ?
		dprintf(D_ALWAYS,
			"TransformOnEvict %s error %d applying transform %s : %s\n",
			job_id.c_str(), rval, xfm.getName(), errmsg.c_str());
	}

	return rval;
}

int TransformApplyChanges(bool verbose, JobQueueJob * job, int shadow_code, std::string & job_id, XFormAdUndo & undo, std::map<std::string,std::string> & transaction)
{
	bool autocluster_changing = false;

	for (auto &[key,oldval] : undo.list) {
		ExprTree * tree = job->Lookup(key);

		// ignore changes that are no change
		if (tree && oldval && *tree == *oldval) continue;

		const char * attr = key.c_str();
		const char *rhstr = nullptr;
	
		if (tree) {
			rhstr = ExprTreeToString(tree);
		} else {
			// If an attribute is marked as dirty but Lookup() on this
			// attribute fails, it means that attribute was deleted.
			// We handle this by inserting into the transaction log a
			// SetAttribute to UNDEFINED, which will work properly even if we
			// are dealing with a chained job ad that has the attribute set differently
			// in the cluster ad.
			rhstr = "undefined";
		}
		if ( ! rhstr) { 
			dprintf(D_ALWAYS,"TransformOnEvict %s: Problem with transformed classad attribute %s aborting transform\n",
				job_id.c_str(), attr);
			return shadow_code;
		}

		if (verbose) { fprintf(stdout, "\tSetAttribute %s %s\n", attr, rhstr); }
		transaction[attr] = rhstr;

		if (shadow_code == JOB_SHOULD_HOLD) {
			// This trick assumes that SetAttribute above will have set the autocluster_id to -1 if an attribute
			// in the autocluster was changed, or that the autocluster was dirty already.
			// We further check to see if it appears to be a resource request attr, this should insure that if we
			// go back to idle the autocluster would be recalculated at a minimum.
			// TODO: better way of recognizing that it is safe to skip the hold?
			if (starts_with_ignore_case(key, "Request")) {
				autocluster_changing = true;
			}
		}
	}

	// now revert the direct changes to the job classad
	// since they are now part of the transaction
	undo.Revert(*job);

	if (shadow_code == JOB_SHOULD_HOLD && autocluster_changing) {
		shadow_code = JOB_SHOULD_REQUEUE;
	}

	return shadow_code;
}

// specialize the XFormAdUndo class so we can dump its internal data
//
class XFormAdUndoEx : public XFormAdUndo
{
public:
	void clear() {
		for (auto & [k,v] : list) { if (v) delete v; }
		list.clear();
		deletions.clear();
	}

	const char * dump(std::string & out, const char * indent="") {
		classad::ClassAdUnParser unparser;
		unparser.SetOldClassAd( true, true );
		for (auto & [k,v] : list) {
			formatstr_cat(out, "%s%s=", indent, k.c_str());
			if (v) { unparser.Unparse(out, v); } else { out += "<null>"; }
			out += "\n";
		}
		return out.c_str();
	}

	const char * dump_keys(std::string & out) {
		for (auto & [k,v] : list) { out += k; out += ","; }
		if ( ! list.empty()) out.pop_back(); // remove trailing comma
		return out.c_str();
	}

	const char * dump_ad_values(std::string & out, ClassAd & ad, const char * indent, classad::References * addl=nullptr) {
		classad::ClassAdUnParser unparser;
		unparser.SetOldClassAd( true, true );
		ClassAd * parent = ad.GetChainedParentAd();
		if (addl) {
			// iterate the interesting keys
			for (auto & k : *addl) {
				if (list.find(k) != list.end()) continue; // skip keys that are in the undo
				formatstr_cat(out, "%s%s=", indent, k.c_str());
				if (parent) {
					auto * cluster_tree = parent->Lookup(k);
					auto * proc_tree = ad.LookupIgnoreChain(k);
					if (proc_tree) { unparser.Unparse(out, proc_tree); out += " hiding "; } else { out += " -> "; }
					if (cluster_tree) { unparser.Unparse(out, cluster_tree); } else {
						if (parent->find(k) != parent->end()) {
							out += "<null>";
						} else {
							out += "<nothing>";
						}
					}
				} else {
					auto * tree = ad.Lookup(k);
					if (tree) { unparser.Unparse(out, tree); } else { out += "<null>"; }
				}
				out += "\n";
			}
		}
		for (auto & [k,_] : list) {
			formatstr_cat(out, "%s%s=", indent, k.c_str());
			if (parent) {
				auto * cluster_tree = parent->Lookup(k);
				auto * proc_tree = ad.LookupIgnoreChain(k);
				if (proc_tree) { unparser.Unparse(out, proc_tree); out += " hiding "; } else { out += " -> "; }
				if (cluster_tree) { unparser.Unparse(out, cluster_tree); } else {
					if (parent->find(k) != parent->end()) {
						out += "<null>";
					} else {
						out += "<nothing>";
					}
				}
			} else {
				auto * tree = ad.Lookup(k);
				if (tree) { unparser.Unparse(out, tree); } else { out += "<null>"; }
			}
			out += "\n";
		}
		return out.c_str();

	}
};

const char * print_ad_value(std::string & out, const std::string & attr, ClassAd & ad, const char * indent) {
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );
	out += indent;
	out += attr;
	out += "=";
	ClassAd * parent = ad.GetChainedParentAd();
	if (parent) {
		auto * cluster_tree = parent->Lookup(attr);
		auto * proc_tree = ad.LookupIgnoreChain(attr);
		if (proc_tree) { unparser.Unparse(out, proc_tree); out += " hiding "; } else { out += " -> "; }
		if (cluster_tree) { unparser.Unparse(out, cluster_tree); } else { out += "<null>"; }
	} else {
		auto * tree = ad.Lookup(attr);
		if (tree) { unparser.Unparse(out, tree); } else { out += "<null>"; }
	}
	return out.c_str();
}

static const char * retry_mem_basic = 
	"REQUIREMENTS (VacateReasonCode==21 && VacateReasonSubCode==102) || (VacateReasonCode == 34 && VacateReasonSubCode == 102)\x1e"
	"SET RequestMemory 2048"
;

static const char basic_result1[] =R"(JOB_SHOULD_REQUEUE
RequestMemory = 2048
)";
static const char basic_result2[] =R"(JOB_SHOULD_HOLD
<empty>
)";

static const char * retry_mem_nextmax = 
	"REQUIREMENTS (VacateReasonCode==21 && VacateReasonSubCode==102) || (VacateReasonCode == 34 && VacateReasonSubCode == 102)\x1e"
	"EVALSET RequestMemory MIN({NextRequestMemory, MaxRequestMemory})"
;

static const char nextmax_result1[] =R"(JOB_SHOULD_REQUEUE
RequestMemory = 250
)";
static const char nextmax_result2[] =R"(JOB_SHOULD_REQUEUE
RequestMemory = 300
)";
static const char nextmax_result3[] =R"(JOB_SHOULD_HOLD
<empty>
)";

// test for retry_request_memory iterating
static const char * retry_mem_seq = 
	"REQUIREMENTS (VacateReasonCode==21 && VacateReasonSubCode==102) || (VacateReasonCode == 34 && VacateReasonSubCode == 102)\x1e"
	"EVALSET RequestMemory RetryRequestMemory[NextRequestMemory?:0]\x1e"
	"EVALSET NextRequestMemory MIN({size(RetryRequestMemory)-1, (NextRequestMemory?:0)+1})"
;
static const char seq_result1[] =R"(JOB_SHOULD_REQUEUE
NextRequestMemory = 1
RequestMemory = 250
)";
static const char seq_result2[] =R"(JOB_SHOULD_REQUEUE
RequestMemory = 500
)";
static const char seq_result3[] =R"(JOB_SHOULD_HOLD
<empty>
)";
static const char seq_result4[] =R"(JOB_SHOULD_HOLD
<empty>
)";

// test for COPY and mergeEnvironment iterating
static const char * retry_set_env = 
"REQUIREMENTS true\x1e"
"COPY Environment OldEnvironment\x1e"
"EVALSET Environment mergeEnvironment(Environment, \"FOO=BAR\")"
;
static const char set_env_result1[] =R"(JOB_SHOULD_HOLD
Environment = "FOO=BAR INFO=banner"
OldEnvironment = "FOO=BOO INFO=banner"
)";
static const char set_env_result2[] =R"(JOB_SHOULD_HOLD
OldEnvironment = "FOO=BAR INFO=banner"
)";
static const char set_env_result3[] =R"(JOB_SHOULD_HOLD
<empty>
)";

// test for DELETE and RENAME attributes
static const char * retry_rename_attr = 
"REQUIREMENTS true\x1e"
"DELETE LeaveJobInQueue\x1e"
"SET    LeaveIt true\x1e"
"RENAME LeaveIt LeaveJob"
;
static const char rename_attr_result1[] =R"(JOB_SHOULD_HOLD
LeaveIt = undefined
LeaveJob = true
LeaveJobInQueue = undefined
)";
// TODO: this is kinda wrong, should be <empty>, but it is the way undo currently behaves
static const char rename_attr_result2[] =R"(JOB_SHOULD_HOLD
LeaveIt = undefined
)";

// table of actions, when the name is "" body is the jobs to load
// when the name is not "" or null, the body is the transform to apply
static const struct { const char * name; const char * body; const char * result; } checks[] = {
	{ "", basic_jobdata, nullptr },
		{ "basic",   retry_mem_basic, basic_result1 },
		{ "basic",   retry_mem_basic, basic_result2 },
	{ "", basic_jobdata, nextmax_jobdata },
		{ "nextmax", retry_mem_nextmax, nextmax_result1 },
		{ "nextmax", retry_mem_nextmax, nextmax_result2 },
		{ "nextmax", retry_mem_nextmax, nextmax_result3 },
	{ "", basic_jobdata, seq_jobdata },
		{ "seq1", retry_mem_seq, seq_result1 },
		{ "seq2", retry_mem_seq, seq_result2 },
		{ "seq3", retry_mem_seq, seq_result3 },
		{ "seq4", retry_mem_seq, seq_result4 },
	{ "", basic_jobdata, env_jobdata },
		{ "env1", retry_set_env, set_env_result1 },
		{ "env2", retry_set_env, set_env_result2 },
		{ "env3", retry_set_env, set_env_result3 },
	{ "", basic_jobdata, env_jobdata },
		{ "rename1", retry_rename_attr, rename_attr_result1 },
		{ "rename2", retry_rename_attr, rename_attr_result2 },
};

static const struct { const char * name; const char * body; } xforms[] = {
	{ "basic",   retry_mem_basic },
	{ "nextmax", retry_mem_nextmax },
	{ "seq1", retry_mem_seq },
};

void load_test_jobs(std::vector<ClassAd> & holder, std::vector<ClassAd*> & jobs, const char * szzjobdata, const char * szzDeltas)
{
	size_t begin_index = holder.size();

	const char * ptr = szzjobdata;
	size_t cch = strlen(ptr);
	while (cch) {
		// parse from string
		std::string_view instr(ptr, cch);

		while ( ! instr.empty() && isspace(instr.front())) {
			instr.remove_prefix(1);
		}
		CondorClassAdFileIterator ccfi;
		CompatStringViewLexerSource lexsrc(instr,0);
		if ( ! ccfi.begin(&lexsrc, false)) {
			fprintf(stderr, "ERROR: Could not parse jobad string\n");
			return;
		}

		// load a job or chained cluster/proc 
		ClassAd & ad = holder.emplace_back();
		int numAttrs = ccfi.next(ad);
		if (numAttrs > 0) {
			while ( ! lexsrc.AtEnd()) {
				ClassAd & procAd = holder.emplace_back();
				numAttrs = ccfi.next(procAd);
				if (numAttrs < 0) {
					holder.pop_back();
				}
			}
		} else {
			holder.pop_back();
		}

		// advance to the next
		ptr += cch+1; cch = strlen(ptr);
	}

	if (szzDeltas && (szzDeltas != szzjobdata)) {
		ptr = szzDeltas;
		cch = strlen(ptr);
		while (cch) {

			// parse from string
			std::string_view instr(ptr, cch);

			while ( ! instr.empty() && isspace(instr.front())) {
				instr.remove_prefix(1);
			}
			CondorClassAdFileIterator ccfi;
			CompatStringViewLexerSource lexsrc(instr,0);
			if ( ! ccfi.begin(&lexsrc, false)) {
				fprintf(stderr, "ERROR: Could not parse deltas ad string\n");
				break;
			}

			ClassAd ad;
			int numAttrs = ccfi.next(ad);
			if (numAttrs > 0) {
				// make a constraint expression to match the deltas ad to the original ad
				int id = -1;
				ConstraintHolder constr;
				if (ad.LookupInteger(ATTR_CLUSTER_ID, id)) {
					std::string tmp("ClusterId==" + std::to_string(id));
					constr.parse(tmp.c_str());
				} else if (ad.LookupInteger(ATTR_PROC_ID, id)) {
					std::string tmp("ProcId==" + std::to_string(id));
					constr.parse(tmp.c_str());
				} else {
					fprintf(stderr, "ERROR: Could not determine deltas ad constraint\n");
					break;
				}
				
				// now merge the parsed ad into the corresponding ads
				for (size_t ix = begin_index; ix < holder.size(); ++ix) {
					if (EvalExprBool(&holder[ix], constr.Expr())) {
						holder[ix].Update(ad);
						break;
					}
				}
			}

			// advance to the next
			ptr += cch+1; cch = strlen(ptr);
		}


	}

	// now chain proc ads to cluster ads
	ClassAd * cluster = nullptr;
	for (auto & ad : holder) {
		bool has_cluster_id = ad.Lookup(ATTR_CLUSTER_ID);
		bool has_proc_id = ad.Lookup(ATTR_PROC_ID);
		if (has_cluster_id) {
			if (has_proc_id) { cluster = nullptr; } else { cluster = &ad; }
		} else if (has_proc_id) {
			ad.ChainToAd(cluster);
		}
		if (has_proc_id) { jobs.push_back(&ad); }
	}
}

void testing_transforms(bool verbose)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_transforms ----\n\n");
	}

	classad::References interesting_keys{"RequestMemory", "NextRequestMemory", "MaxRequestMemory", "RetryRequestMemory"};

	std::vector<ClassAd>  ads_holder;
	std::vector<ClassAd*> jobs;

	for (auto [name, body, result] : checks) {

		if ( ! name) {
			ads_holder.clear(); jobs.clear();
			continue;
		}
		if ( ! *name) {
			ads_holder.clear(); jobs.clear();
			if (dash_verbose2) {
				fprintf(stdout, "\t-- reloading jobs\n%s\n", result ? result : body);
			}
			load_test_jobs(ads_holder, jobs, body, result);
			continue;
		}

		ycheck = name;
		ystr = "";

		// transform the jobs
		for (ClassAd * job : jobs) {
			JOB_ID_KEY jid(1,0);
			job->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
			job->LookupInteger(ATTR_PROC_ID, jid.proc);
			std::string jobid = jid;
			std::string dumpbuf;

			XFormAdUndoEx undo;
			std::map<std::string, std::string> transaction;

			// make a copy of the raw ads so we can check to see if the undos changes them at all
			std::vector<ClassAd>  ads_holder_backup(ads_holder);
			wantstr = result;

			if (dash_verbose2) {
				dumpbuf.clear();
				fprintf(stdout, "-- %s before xform %s :\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}

			int rval = TransformOnEvict(job, name, body, JOB_SHOULD_HOLD, jobid, undo);
			if (dash_verbose2) {
				dumpbuf.clear();
				fprintf(stdout, "result=%d xform %s undo:\n%s\n", rval, name, undo.dump(dumpbuf, "\t"));
				dumpbuf.clear();
				if (dash_verbose2) fprintf(stdout, "-- %s modified by xform %s:\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}
			int code = JOB_SHOULD_HOLD;
			if (rval < 0) {
				undo.Revert(*job);
			} else {
				if (dash_verbose2) fprintf(stdout, "-- %s log of changes:\n", name);
				code = TransformApplyChanges(dash_verbose2, job, code, jobid, undo, transaction);
				if (dash_verbose2 && transaction.empty()) { fprintf(stdout, "\t<no changes>\n"); }
				// apply changes does the revert
				// undo.Revert(*job);
			}

			if (dash_verbose2) {
				dumpbuf.clear();
				if (dash_verbose2) fprintf(stdout, "-- %s after revert of xform %s :\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}

			// check to see if the ads were reverted correctly by the undo
			for (size_t ii = 0; ii < ads_holder.size(); ++ii) {
				JOB_ID_KEY jid2(1,-1);
				ads_holder[ii].LookupInteger(ATTR_CLUSTER_ID, jid2.cluster);
				ads_holder[ii].LookupInteger(ATTR_PROC_ID, jid2.proc);
				std::string jobid2 = jid2;
				ystr = jobid2.c_str();
				REQUIRE(ads_are_same(ads_holder[ii], ads_holder_backup[ii]));
			}

			if (rval >= 0) {
				if (dash_verbose2) { fprintf(stdout, "-- commit transaction :\n"); }
				if (dash_verbose2 && transaction.empty()) { fprintf(stdout, "\t<empty>\n"); }
				for (auto & [k,v] : transaction) {
					if (dash_verbose2) { fprintf(stdout, "\t AssignExpr( %s, %s )\n", k.c_str(), v.c_str()); }
					job->AssignExpr(k, v.c_str());
				}
			}

			// check transaction and result
			std::string resbuf(code == JOB_SHOULD_REQUEUE ? "JOB_SHOULD_REQUEUE\n" : "JOB_SHOULD_HOLD\n");
			if (transaction.empty()) { resbuf += "<empty>\n"; }
			else { for (auto & [k,v] : transaction) { resbuf += k + " = " + v + "\n"; } }
			ystr = resbuf; // for REQUIRE logging.
			REQUIRE(resbuf == result);

			if (dash_verbose2) {
				dumpbuf.clear();
				if (dash_verbose2) fprintf(stdout, "-- %s after transaction of xform %s :\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}

			if (dash_verbose2) {
				fprintf(stdout, "RESULT %d : %s\n", code, code == JOB_SHOULD_REQUEUE ? "JOB_SHOULD_REQUEUE" : "JOB_SHOULD_HOLD");
			}

			if (dash_verbose2) fprintf(stdout, "\n");

		}

	}
}

void testing_transforms_on_a_job(bool verbose, ClassAd * test_job)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_transforms ----\n\n");
	}

	std::vector<ClassAd>  ads_holder;
	std::vector<ClassAd*> jobs;
	classad::References interesting_keys{"RequestMemory", "NextRequestMemory", "MaxRequestMemory", "RetryRequestMemory"};
	if (test_job) jobs.push_back(test_job);
	else {
		load_test_jobs(ads_holder, jobs, basic_jobdata, nullptr);
	}

	for (ClassAd * job : jobs) {
		JOB_ID_KEY jid(1,0);
		job->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
		job->LookupInteger(ATTR_PROC_ID, jid.proc);
		std::string jobid = jid;
		std::string dumpbuf;

		XFormAdUndoEx undo;
		std::map<std::string, std::string> transaction;

		for (auto [name, body] : xforms) {
			undo.clear(); dumpbuf.clear(); transaction.clear();

			if (verbose) {
				dumpbuf.clear();
				fprintf(stdout, "-- %s before xform %s :\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}

			int rval = TransformOnEvict(job, name, body, JOB_SHOULD_HOLD, jobid, undo);
			if (verbose) {
				dumpbuf.clear();
				fprintf(stdout, "result=%d xform %s undo:\n%s\n", rval, name, undo.dump(dumpbuf, "\t"));
				dumpbuf.clear();
				if (dash_verbose2) fprintf(stdout, "-- %s modified by xform %s:\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}
			int code = JOB_SHOULD_HOLD;
			if (rval < 0) {
				undo.Revert(*job);
			} else {
				if (verbose) fprintf(stdout, "-- %s log of changes:\n", name);
				code = TransformApplyChanges(verbose, job, code, jobid, undo, transaction);
				if (verbose && transaction.empty()) { fprintf(stdout, "\t<no changes>\n"); }
				// apply changes does this.
				// undo.Revert(*job);
			}

			if (verbose) {
				dumpbuf.clear();
				if (dash_verbose2) fprintf(stdout, "-- %s after revert of xform %s :\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}

			if (rval >= 0) {
				if (verbose) { fprintf(stdout, "-- commit transaction :\n"); }
				if (verbose && transaction.empty()) { fprintf(stdout, "\t<empty>\n"); }
				for (auto & [k,v] : transaction) {
					if (verbose) { fprintf(stdout, "\t AssignExpr( %s, %s )\n", k.c_str(), v.c_str()); }
					job->AssignExpr(k, v.c_str());
				}
			}

			if (verbose) {
				dumpbuf.clear();
				if (dash_verbose2) fprintf(stdout, "-- %s after transaction of xform %s :\n%s\n", jobid.c_str(), name,
					undo.dump_ad_values(dumpbuf, *job, "\t", &interesting_keys));
			}

			if (verbose) {
				fprintf(stdout, "RESULT %d : %s\n", code, code == JOB_SHOULD_REQUEUE ? "JOB_SHOULD_REQUEUE" : "JOB_SHOULD_HOLD");
			}

			if (verbose) fprintf(stdout, "\n");
		}
	}
}

void testing_undos(bool verbose, ClassAd *)
{
	if (verbose) {
		fprintf( stdout, "\n----- testing_undos ----\n\n");
	}

}


void Usage(const char * appname, FILE * out)
{
	const char * p = appname;
	while (*p) {
		if (*p == '/' || *p == '\\') appname = p+1;
		++p;
	}
	fprintf(out, "Usage: %s [<opts>] [-test[:<tests>] | <queryopts>]\n"
		"  Where <opts> is one of\n"
		"    -help\tPrint this message\n"
		"    -verbose[:2}\tMore verbose output\n\n"
		"  <tests> is one or more letters choosing specific subtests\n"
		"    x  transform tests\n"
		"    u  undo tests\n"
		"  If no arguments are given (or just -verbose) all tests are run.\n"
		"  Unless -verbose is specified, only failed tests produce output.\n"
		"  This program returns 0 if all tests succeed, 1 if any tests fails.\n\n"
		"  Instead of running tests, this program can apply a transform to a job\n"
		"  and report the updated job and/or the undo\n\n"
		"    -job <file>\t Parse <file> as a Job classad, or as chained Cluster and Proc ads\n"
		"    -debug[:<flags>]\t Send log messages to stdout. default flags is D_FULLDEBUG\n"
		, appname);
}

// runs all of the tests in non-verbose mode by default (i.e. printing only failures)
// individual groups of tests can be run by using the -t:<tests> option where <tests>
// is one or more of 
//   x   testing_transforms
//   u   testing_undos
//
int main( int /*argc*/, const char ** argv) {

	int test_flags = 0;
	const char * pcolon;
	bool other_arg = false;
	const char * jobfile = nullptr;
	const char * jobad_string = nullptr;

	// if we don't init dprintf, calls to it will be will be malloc'ed and stored
	// for later. this form of init will match the fewest possible issues.
	dprintf_config_tool_on_error("D_ERROR");
	dprintf_OnExitDumpOnErrorBuffer(stderr);

	for (int ii = 1; argv[ii]; ++ii) {
		const char *arg = argv[ii];
		if (is_dash_arg_colon_prefix(arg, "verbose", &pcolon, 1)) {
			dash_verbose = 1;
			if (pcolon && pcolon[1]=='2') { dash_verbose2 = true; }
		} else if (is_dash_arg_prefix(arg, "help", 1)) {
			Usage(argv[0], stdout);
			return 0;
		} else if (is_dash_arg_colon_prefix(arg, "test", &pcolon, 1)) {
			if (pcolon) {
				while (*++pcolon) {
					switch (*pcolon) {
					case 'x': test_flags |= 0x0111; break; // transform
					case 'u': test_flags |= 0x0222; break; // undo
					}
				}
			} else {
				test_flags = 3;
			}
		} else if (is_dash_arg_prefix(arg, "job", 1)) {
			jobfile = argv[ii+1];
			if (jobfile && (jobfile[0] != '-' || jobfile[1] != 0)) {
				++ii;
			} else {
				fprintf(stderr, "-job requires a filename argument\n");
				return 1;
			}
			other_arg = true;
		} else if (is_dash_arg_colon_prefix(arg, "debug", &pcolon, 3)) {
			// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else {
			fprintf(stderr, "unknown argument %s\n", arg);
			Usage(argv[0], stderr);
			return 1;
		}
	}
	if ( ! test_flags && ! other_arg) test_flags = -1;

	ClassAdReconfig(); // register the compat classad functions. 

	std::unique_ptr<ClassAd> clusterad;
	CondorClassAdFileIterator ccfi;
	classad::LexerSource * lexsrc = nullptr;
	if (jobfile) {
		FILE *file = safe_fopen_wrapper_follow(jobfile, "rb");
		if (NULL == file) {
			fprintf(stderr, "ERROR: Could not open job file '%s' : %s\n", jobfile, strerror(errno));
			return 1;
		} else {
			lexsrc = new CompatFileLexerSource (file, true);
			if ( ! ccfi.begin(lexsrc, true)) {
				fprintf(stderr, "ERROR: Could not parse job file '%s' : %s\n", jobfile, "");
				return 1;
			}
		}
	} else if (jobad_string) {

		// parse from string
		std::string_view instr(jobad_string);
		while ( ! instr.empty() && isspace(instr.front())) {
			instr.remove_prefix(1);
		}
		lexsrc = new CompatStringViewLexerSource(instr,0);
		if ( ! ccfi.begin(lexsrc, true)) {
			fprintf(stderr, "ERROR: Could not parse jobad string\n");
			return 1;
		}
	}

	// load a job or chained cluster/proc 
	ClassAd * job = nullptr; 
	if (lexsrc) {
		job= new ClassAd();
		int numAttrs = ccfi.next(*job);
		if (numAttrs > 0 &&  ! lexsrc->AtEnd()) {
			ClassAd * procAd = new ClassAd();
			numAttrs = ccfi.next(*procAd);
			if (numAttrs > 0) {
				procAd->ChainToAd(job);
				clusterad.reset(job);
				job = procAd;
			}
		}
	}

	if (test_flags & 0x0001) testing_transforms(dash_verbose);
	if (test_flags & 0x0002) testing_undos(dash_verbose, job);


#if 0
	if (userfile) {
		if (gridfile) {
			if (lookup_method) {
				fprintf(stderr, "WARNING: -method argument is ignored when doing lookups against a -grid file\n");
			}
		} else if (mapfile) {
			if ( ! lookup_method || (lookup_method[0] == '#')) lookup_method = "*";
		} else {
			fprintf(stderr, "ERROR: -userfile requires either -file -or -grid to do lookups against\n");
			return 1;
		}
		FILE *file = safe_fopen_wrapper_follow(userfile, "rb");
		if (NULL == file) {
			fprintf(stderr, "ERROR: Could not open user lookups file '%s' : %s\n", userfile, strerror(errno));
			return 1;
		}

		MyStringFpSource src(file, true);
		std::vector<std::string> lines;
		while ( ! src.isEof()) {
			std::string line;
			readLine(line, src); // Result ignored, we already monitor EOF
			chomp(line);
			if (line.empty()) continue;
			lines.emplace_back(line);
		}
	}

	delete gmf; gmf = nullptr; // so we can valgrind.
#endif

	dprintf_SetExitCode(fail_count > 0);
	return fail_count > 0;
}
