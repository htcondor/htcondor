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
#include "condor_attributes.h"
#include "my_popen.h"
#include "submit_utils.h"
#include "submit_protocol.h"
#include "condor_version.h"
#include "my_username.h"
#include "../condor_utils/dagman_utils.h"

// Local DAGMan includes
#include "dagman_main.h"
#include "dag.h"
#include "dagman_submit.h"
#include "debug.h"
#include "tmp_dir.h"
#include "write_user_log.h"

namespace deep = DagmanDeepOptions;
namespace conf = DagmanConfigOptions;

static const char* getEventMask() {
	static std::string eventMask;

	if (eventMask.empty()) {
		// IMPORTANT NOTE:  see all events that we deal with in
		// Dag::ProcessOneEvent() -- all of those need to be in the
		// event mask!! (wenger 2012-11-16)
		const std::array<int, 18> desiredEvents = {
			ULOG_SUBMIT,
			ULOG_EXECUTE,
			ULOG_EXECUTABLE_ERROR,
			ULOG_JOB_EVICTED,
			ULOG_JOB_TERMINATED,
			ULOG_SHADOW_EXCEPTION,
			ULOG_GENERIC,
			ULOG_JOB_ABORTED,
			ULOG_JOB_SUSPENDED,
			ULOG_JOB_UNSUSPENDED,
			ULOG_JOB_HELD,
			ULOG_JOB_RELEASED,
			ULOG_POST_SCRIPT_TERMINATED,
			ULOG_GLOBUS_SUBMIT, // For Pegasus
			ULOG_JOB_RECONNECT_FAILED,
			ULOG_GRID_SUBMIT, // For Pegasus
			ULOG_CLUSTER_SUBMIT,
			ULOG_CLUSTER_REMOVE,
		};

		for (const auto& event : desiredEvents) {
			if ( ! eventMask.empty()) { eventMask += ","; }
			eventMask += std::to_string(event);
		}
	}

	return eventMask.c_str();
}

struct NodeVar {
	NodeVar(std::string k, std::string v, bool a) : key(k), value(v), append(a) {};
	std::string key{};
	std::string value{};
	bool append{false};
};

// Check if node var key is in specified list of items to defer (True add to other structure) return True for deferred
static bool check_defer_var(std::vector<NodeVar>& deferred, const NodeVar& var, const std::set<std::string>& key_filter) {
	if (key_filter.contains(var.key)) {
		deferred.push_back(std::move(var));
		return true;
	}
	return false;
}

//Create a vector of variable keys and values to be added to the node job(s).
static std::vector<NodeVar> init_vars(const Dagman& dm, const Node& node) {
	std::vector<NodeVar> vars;

	const char* nodeName = node.GetNodeName();
	int retry = node.GetRetries();
	std::string parents, batchName, batchId;

	if ( ! node.NoParents()) {
		parents.reserve(2048);
		node.PrintParents(parents, 2000, dm.dag, ",");
	}

	if ( ! node.GetDagFile() && dm.options[deep::str::BatchName] == " ") {
		batchName = "";
	} else {
		batchName = dm.options[deep::str::BatchName];
	}
	if ( ! node.GetDagFile() && dm.options[deep::str::BatchId] == " ") {
		batchId = "";
	} else {
		batchId = dm.options[deep::str::BatchId];
	}

	vars.emplace_back("JOB", nodeName, false);
	vars.emplace_back("RETRY", std::to_string(retry), false);

	vars.emplace_back("DAG_STATUS", std::to_string((int)dm.dag->_dagStatus), false);
	vars.emplace_back("FAILED_COUNT", std::to_string(dm.dag->NumNodesFailed()), false);

	// Only Add Parents Macro if not empty. Custom Attr will resolve to ""
	if ( ! parents.empty()) { vars.emplace_back("DAG_PARENT_NAMES", parents, false); }
	vars.emplace_back("MY.DAGParentNodeNames", "\"$(DAG_PARENT_NAMES)\"", false);

	if ( ! batchName.empty()) {
		vars.emplace_back(SUBMIT_KEY_BatchName, batchName, false);
	}
	if ( ! batchId.empty()) {
		vars.emplace_back(SUBMIT_KEY_BatchId, batchId, false);
	}

	if (dm.config[conf::b::JobInsertRetry] && retry > 0) {
		vars.emplace_back("MY.DAGManNodeRetry", std::to_string(retry), false);
	}

	if (node._effectivePriority != 0) {
		vars.emplace_back(SUBMIT_KEY_Priority, std::to_string(node._effectivePriority), false);
	}

	if (node.GetHold()) {
		debug_printf(DEBUG_VERBOSE, "Submitting node %s job(s) on hold\n", nodeName);
		vars.emplace_back(SUBMIT_KEY_Hold, "true", false);
	}

	if ( ! node.NoChildren() && dm.config[conf::i::HoldClaimTime] > 0) {
		vars.emplace_back(SUBMIT_KEY_KeepClaimIdle, std::to_string(dm.config[conf::i::HoldClaimTime]), false);
	}

	if ( ! dm.options[deep::str::AcctGroup].empty()) {
		vars.emplace_back(SUBMIT_KEY_AcctGroup, dm.options[deep::str::AcctGroup], false);
	}

	if ( ! dm.options[deep::str::AcctGroupUser].empty()) {
		vars.emplace_back(SUBMIT_KEY_AcctGroupUser, dm.options[deep::str::AcctGroupUser], false);
	}

	if ( ! dm.config[conf::str::MachineAttrs].empty()) {
		vars.emplace_back(SUBMIT_KEY_JobAdInformationAttrs, dm.config[conf::str::UlogMachineAttrs], false);
		vars.emplace_back(SUBMIT_KEY_JobMachineAttrs, dm.config[conf::str::MachineAttrs], false);
	}

	for (const auto& [key, val] : dm.inheritAttrs) {
		vars.emplace_back(std::string("My.") + key, val, false);
	}

	vars.emplace_back(ATTR_DAG_NODE_NAME_ALT, nodeName, true);
	vars.emplace_back(SUBMIT_KEY_LogNotesCommand, std::string("DAG Node: ") + nodeName, true);
	vars.emplace_back(SUBMIT_KEY_DagmanLogFile, dm.config[conf::str::NodesLog], true);
	vars.emplace_back("My." ATTR_DAGMAN_WORKFLOW_MASK, std::string("\"") + getEventMask() + "\"", true);

	// NOTE: we specify the job ID of DAGMan using only its cluster ID
	// so that it may be referenced by jobs in their priority
	// attribute (which needs an int, not a string).  Doing so allows
	// users to effectively "batch" jobs by DAG so that when they
	// submit many DAGs to the same schedd, all the ready jobs from
	// one DAG complete before any jobs from another begin.
	if (dm.DAGManJobId._cluster > 0) {
		vars.emplace_back("My." ATTR_DAGMAN_JOB_ID, std::to_string(dm.DAGManJobId._cluster), true);
		vars.emplace_back(ATTR_DAGMAN_JOB_ID, std::to_string(dm.DAGManJobId._cluster), true);
	}

	if (dm.config[conf::b::SuppressJobLogs]) {
		vars.emplace_back(SUBMIT_KEY_UserLogFile, "", true);
	}

	if (dm.options[deep::b::SuppressNotification]) {
		vars.emplace_back(SUBMIT_KEY_Notification, "NEVER", true);
	}

	for (auto &dagVar : node.varsFromDag) {
		vars.emplace_back(dagVar._name, dagVar._value, !dagVar._prepend);
	}

	return vars;
}

//-------------------------------------------------------------------------
static bool shell_condor_submit(const Dagman &dm, Node* node, CondorID& condorID) {
	static DagmanUtils utils;
	std::string cmdFile = node->GetCmdFile();
	auto vars = init_vars(dm, *node);

	if (node->HasInlineDesc()) {
		formatstr(cmdFile, "%s-inline.%d.temp", node->GetNodeName(), daemonCore->getpid());
		if (utils.fileExists(cmdFile)) {
			debug_printf(DEBUG_QUIET, "Warning: Temporary submit file '%s' already exists. Overwriting...\n",
			             cmdFile.c_str());
		}
		FILE *temp_fp = safe_fopen_wrapper_follow(cmdFile.c_str(), "w");
		if ( ! temp_fp) {
			debug_printf(DEBUG_QUIET, "Error: Failed to create temporary submit file '%s'\n",
			             cmdFile.c_str());
			return false;
		}

		std::string_view& desc = node->inline_desc;
		if (fwrite(desc.data(), sizeof(char), desc.size(), temp_fp) != desc.size()) {
			debug_printf(DEBUG_QUIET, "Error: Failed to write temporary submit file '%s':\n%s### END DESC ###\n",
			             cmdFile.c_str(), desc.data());
			if (dm.config[conf::b::RemoveTempSubFiles]) { utils.tolerant_unlink(cmdFile); }
			fclose(temp_fp);
			return false;
		}

		fclose(temp_fp);
	}

	// Construct condor_submit command to execute
	ArgList args;
	args.AppendArg(dm.config[conf::str::SubmitExe]);

	static const std::set<std::string> defer_list = {"DAG_PARENT_NAMES", "MY.DAGParentNodeNames"};
	std::vector<NodeVar> deferred;
	for (const auto& var : vars) {
		if (check_defer_var(deferred, var, defer_list)) { continue; }
		if (var.append) { args.AppendArg("-a"); }
		std::string cmd = var.key + "=" + var.value;
		args.AppendArg(cmd);
	}

	// Hold on adding parent nodes list incase command exceeds max size
	ArgList extraArgs;
	for (const auto& var : deferred) {
		if (var.append) { extraArgs.AppendArg("-a"); }
		std::string cmd = var.key + "=" + var.value;
		extraArgs.AppendArg(cmd);
	}

	// Get size of parts of the command line we are about to run
	std::string display;
	args.GetArgsStringForDisplay(display);
	int cmdLineSize = display.length();
	display.clear();

	extraArgs.GetArgsStringForDisplay(display);
	int DAGParentNodeNamesLen = display.length();
	display.clear();

	int reserveNeeded = (int)cmdFile.length();

	// if we don't have room for DAGParentNodeNames, leave it unset
	if ((cmdLineSize + reserveNeeded + DAGParentNodeNamesLen) > _POSIX_ARG_MAX) {
		debug_printf(DEBUG_NORMAL,
		             "Warning: node %s has too many parents to list in its classad; leaving its DAGParentNodeNames attribute undefined\n",
		             node->GetNodeName());
		check_warning_strictness(DAG_STRICT_3);
	} else {
		args.AppendArgsFromArgList(extraArgs);
	}

	args.AppendArg(cmdFile);
	// End command construction

	args.GetArgsStringForDisplay(display);
	debug_printf(DEBUG_VERBOSE, "Submitting: %s\n", display.c_str());

	// Execute condor_submit command
	Env myEnv;
	myEnv.Import();

	int jobProcCount;
	int exit_status;
	auto_free_ptr output = run_command(180, args, MY_POPEN_OPT_WANT_STDERR, &myEnv, &exit_status);

	if (dm.config[conf::b::RemoveTempSubFiles] && node->HasInlineDesc()) {
		utils.tolerant_unlink(cmdFile);
	}

	if ( ! output) {
		if (exit_status != 0) {
			debug_printf(DEBUG_QUIET, "ERROR: Failed to run condor_submit for node %s with status %d\n",
			             node->GetNodeName(), exit_status);
		} else {
			debug_printf(DEBUG_QUIET, "ERROR (%d): Failed to run condor_submit for node %s: %s\n",
			             errno, node->GetNodeName(), strerror(errno));
		}
		return false;
	}

	//----------------------------------------------------------------------
	// Parse submit command output for a HTCondor job ID.
	// Typical condor_submit output looks like:
	//    Submitting job(s).
	//    Logging submit event(s).
	//    1 job(s) submitted to cluster 2267.
	//----------------------------------------------------------------------

	bool successful_submit = false;
	for (const auto& line : StringTokenIterator(output.ptr(), "\n")) {
		debug_printf(DEBUG_VERBOSE, "From submit: %s\n", line.c_str());
		if ( ! successful_submit && line.find(" job(s) submitted to cluster ") != std::string::npos) {
			if (2 != sscanf(line.c_str(), " %d job(s) submitted to cluster %d", &jobProcCount, &condorID._cluster)) {
				debug_printf(DEBUG_QUIET, "ERROR: parse_condor_submit failed:\n\t%s\n", line.c_str());
				// Return true so higher level code handles failure correctly rather than
				// retrying the submit however many times DAGMan is configured to.
				return true;
			}
			successful_submit = true;
		}
	}

	if ( ! successful_submit) {
		debug_printf(DEBUG_QUIET, "ERROR: Failed to run condor_submit for node %s:\n%s\n",
		             node->GetNodeName(), output.ptr());
		return false;
	}

	// Check for multiple job procs if configured to disallow that.
	if (jobProcCount > 1) {
		if (dm.config[conf::b::ProhibitMultiJobs]) {
			// Other nodes may be single proc so fail and make forward progress
			debug_printf(DEBUG_NORMAL, "Submit generated %d job procs; disallowed by DAGMAN_PROHIBIT_MULTI_JOBS setting\n",
			             jobProcCount);
			return false;
		} else if (node->GetType() == NodeType::PROVISIONER) {
			// Required first node so abort (note: debug_error calls DC_EXIT)
			debug_error(EXIT_ERROR, DEBUG_NORMAL, "ERROR: Provisioner node %s submitted more than one job\n",
			             node->GetNodeName());
		}

	}

	node->SetNumSubmitted(jobProcCount);

	return true;
}

static bool send_jobset_if_allowed(SubmitHash& submitHash, int cluster) {
	if ( ! param_boolean("USE_JOBSETS", false)) { return false; }

	const ClassAd * jobsetAd = submitHash.getJOBSET();
	if (jobsetAd && SendJobsetAd(cluster, *jobsetAd, 0) >= 0) { return true; }
	return false;
}

//-------------------------------------------------------------------------
// TJ's new direct submit w/ late-materialization.
static bool direct_condor_submitV2(const Dagman &dm, Node* node, CondorID& condorID) {
	int rval = 0;
	int cred_result = 0;
	bool is_factory = param_boolean("SUBMIT_FACTORY_JOBS_BY_DEFAULT", false);
	long long max_materialize = INT_MAX;
	int selected_job_count = 0; // number of jobs we will be submitting (including unmaterialized jobs)
	bool success = false;
	std::string errmsg;
	std::string URL;
	auto_free_ptr owner(my_username());

	MacroStreamFile msf;
	MACRO_SOURCE msm_source;
	MacroStreamMemoryFile msm(node->inline_desc.data(), node->inline_desc.size(), msm_source);
	MacroStream* ms = &msm;
	const char* cmdFile = node->GetCmdFile(); // used when submit source is an actual file

	char* tmp_qline = nullptr;
	std::string queue_args;
	SubmitHash submitHash;
	SubmitStepFromQArgs ssi(submitHash);

	DCSchedd schedd;
	CondorError errstack; // errstack for general qmgr commands
	AbstractScheddQ* MyQ = nullptr;

	submitHash.init(JSM_DAGMAN);
	submitHash.setDisableFileChecks(true);
	submitHash.setScheddVersion(CondorVersion());
	submitHash.init_base_ad(time(nullptr), owner);

	auto vars = init_vars(dm, *node);
	const auto partition = std::ranges::stable_partition(vars, [](NodeVar v) -> bool { return !v.append; });

	struct AddVar {
		AddVar(SubmitHash& h) : hash(h) {};
		void operator()(NodeVar v) {
			hash.set_arg_variable(v.key.c_str(), v.value.c_str());
		}
		SubmitHash& hash;
	};

	AddVar setVar(submitHash);
	std::for_each(vars.begin(), partition.begin(), setVar); // Add node vars (prepend)

	// read in the submit file
	if (node->HasInlineDesc()) {
		ms = &msm;
		// Note: cmdFile is set to inline description name for inline descriptions
		submitHash.insert_submit_filename(cmdFile, msm_source);
		debug_printf(DEBUG_NORMAL, "Submitting node %s from inline description using direct job submission\n", node->GetNodeName());
	} else {
		debug_printf(DEBUG_NORMAL, "Submitting node %s from file %s using direct job submission\n", node->GetNodeName(), node->GetCmdFile());
		if ( ! msf.open(cmdFile, false, submitHash.macros(), errmsg)) {
			debug_printf(DEBUG_QUIET, "ERROR: submit attempt failed, errno=%d %s\n", errno, strerror(errno));
			debug_printf(DEBUG_QUIET, "could not open submit file : %s - %s\n", cmdFile, errmsg.c_str());
			goto finis;
		}
		ms = &msf;
		// set submit filename into the submit hash so that $(SUBMIT_FILE) works
		submitHash.insert_submit_filename(cmdFile, msf.source());
	}

	// read the submit file until we get to the queue statement or end of file
	rval = submitHash.parse_up_to_q_line(*ms, errmsg, &tmp_qline);
	if (rval) { goto finis; }
	// capture queue line permanantly. tmp_qline is a pointer to global line buffer
	if (tmp_qline) {
		const char* qargs = submitHash.is_queue_statement(tmp_qline);
		if (qargs) { queue_args = qargs; }
	}

	// Add node vars (append)
	std::for_each(partition.begin(), partition.end(), setVar);

	// Now we can parse the queue arguments and initialize the iterator
	// This where we can finally check for invalid queue statements
	if (ssi.init(queue_args.c_str(), errmsg) != 0) {
		errmsg = "Invalid queue statement (" + queue_args + ")";
		rval = -1;
		goto finis;
	}

	// TODO: Make this a verfication of credentials existing and produce earlier
	// (DAGMan parse or condor_submit_dag). Perhaps double check here and produce if desired?
	if (dm.config[conf::b::ProduceJobCreds]) {
		// Produce credentials needed for job(s)
		cred_result = process_job_credentials(submitHash, 0, URL, errmsg);
		if (cred_result != 0) {
			errmsg = "Failed to produce job credentials (" + std::to_string(cred_result) + "): " + errmsg;
			rval = -1;
			goto finis;
		} else if ( ! URL.empty()) {
			errmsg = "Failed to submit job(s) due to credential setup. Please visit: " + URL;
			rval = -1;
			goto finis;
		}
	}

	// load the itemdata, in some cases we have to do this in order to know the QUEUE variables
	rval = ssi.load_items(*ms, false, errmsg);
	if (rval < 0) { goto finis; }

	//if (dry_run) {
	//	const int sim_starting_cluster = 1;
	//	auto * SimQ = new SimScheddQ(sim_starting_cluster);
	//	FILE * outfile = nullptr;
	//	SimQ->Connect(outfile, false, false);
	//	MyQ = SimQ;
	//} else
	{
		auto * ScheddQ = new ActualScheddQ();
		if (ScheddQ->Connect(schedd, errstack) == 0) {
			delete ScheddQ;
			// TODO: report connection failure?
			// "\nERROR: Failed to connect to local queue manager\n%s\n",
			// errstack.getFullText(true).c_str() );
			goto finis;
		}
		MyQ = ScheddQ;
	}

	// add extended submit commands for this Schedd to the submitHash
	// add the transfer map
	{
		ClassAd extended_submit_commands;
		if (MyQ->has_extended_submit_commands(extended_submit_commands)) {
			submitHash.addExtendedCommands(extended_submit_commands);
		}

		submitHash.attachTransferMap(dm._protectedUrlMap);
	}

	selected_job_count = ssi.selected_job_count();
	if (submitHash.want_factory_submit(max_materialize)) {
		int late_ver = 0;
		if (MyQ->allows_late_materialize() &&
			MyQ->has_late_materialize(late_ver) && late_ver >= 2) {
			is_factory = true;
		} else if (selected_job_count > 1) {
			// TODO: fail the submit here??
		}
	}

	if (selected_job_count > 1) {
		if (dm.config[conf::b::ProhibitMultiJobs]) {
			// Other nodes may be single proc so fail and attempt forward progress
			errmsg = "Submit generated multiple job procs; disallowed by DAGMAN_PROHIBIT_MULTI_JOBS setting";
			rval = -1;
			goto finis;
		} else if (node->GetType() == NodeType::PROVISIONER) {
			// Required first node so abort (note: debug_error calls DC_EXIT)
			debug_error(EXIT_ERROR, DEBUG_NORMAL, "ERROR: Provisioner node %s submitted more than one job\n",
				node->GetNodeName());
		}
	} else {
		// TODO: ignore factory submit request if number of jobs is 1 ??
	}

	// submit transaction starts here
	if (MyQ) {
		int cluster_id = MyQ->get_NewCluster(*submitHash.error_stack());
		if (cluster_id < 0) {
			rval = cluster_id;
			goto finis;
		}

		int proc_id = 0, item_index = 0, step = 0;

		JOB_ID_KEY jid(cluster_id, proc_id);
		ssi.begin(jid, ! is_factory);

		// for late-mat we want to iter all items, for regular submit we iter only selected ones
		bool iter_selected = ! is_factory;

		while ((rval = ssi.next_impl(iter_selected, jid, item_index, step, iter_selected)) > 0) {
			bool send_cluster = (rval == 2); // rval tells us when we need to send the cluster ad

			if ( ! is_factory) {
				proc_id = MyQ->get_NewProc(cluster_id);
				if (proc_id != jid.proc) {
					formatstr(errmsg, "expected next ProcId to be %d, but Schedd says %d", jid.proc, proc_id);
					rval = -1;
					goto finis;
				}

				// If this job has >1 procs, check if multi-proc jobs are prohibited
				// TODO: is this redundant? what about PROVISIONER nodes? can they be late-mat?
				if (proc_id >= 1) {
					if (dm.config[conf::b::ProhibitMultiJobs]) {
						// Other nodes may be single proc so fail and attempt forward progress
						errmsg = "Submit generated multiple job procs; disallowed by DAGMAN_PROHIBIT_MULTI_JOBS setting";
						rval = -1;
						goto finis;
					} else if (node->GetType() == NodeType::PROVISIONER) {
						// Required first node so abort (note: debug_error calls DC_EXIT)
						debug_error(EXIT_ERROR, DEBUG_NORMAL, "ERROR: Provisioner node %s submitted more than one job\n",
							node->GetNodeName());
					}
				}

			} else {
				// for late-mat we need to build the submit digest
				std::string submit_digest;
				submitHash.make_digest(submit_digest, cluster_id, ssi.vars(), 0);
				if (submit_digest.empty()) {
					rval = -1;
					goto finis;
				}

				// send submit itemdata (if any)
				rval = MyQ->send_Itemdata(cluster_id, ssi.m_fea, errmsg);
				if (rval < 0) { goto finis; }

				// append the revised queue statement to the submit digest
				rval = append_queue_statement(submit_digest, ssi.m_fea);
				if (rval < 0) { goto finis; }

				int total_procs = ssi.selected_job_count();
				if (max_materialize <= 0) { max_materialize = INT_MAX; }
				max_materialize = MIN(max_materialize, total_procs);
				max_materialize = MAX(max_materialize, 1);

				// send the submit digest
				rval = MyQ->set_Factory(cluster_id, (int)max_materialize, "", submit_digest.c_str());
				if (rval < 0) { goto finis; }

				// we can now set the live vars from the ssqa.next_impl() call above
				// and fall down to the common code below that sends the cluster ad
				ssi.set_live_vars();
			}

			ClassAd *proc_ad = submitHash.make_job_ad(jid, item_index, step, false, false, nullptr, nullptr);
			if ( ! proc_ad) {
				errmsg = "failed to create job classad";
				rval = -1;
				goto finis;
			}

			if (send_cluster) { // we need to send the cluster ad
				classad::ClassAd * clusterad = proc_ad->GetChainedParentAd();
				if (clusterad) {

					// If there is also a jobset ad, send it before the cluster ad.
					const ClassAd * jobsetAd = submitHash.getJOBSET();
					if (jobsetAd) {
						int jobset_version = 0;
						if (MyQ->has_send_jobset(jobset_version)) {
							rval = MyQ->send_Jobset(cluster_id, jobsetAd);
							if (rval == 0 || rval == 1) {
								rval = 0;
							} else {
								errmsg = "failed to submit jobset.";
								goto finis;
							}
						}
					}

					// send the cluster ad
					errmsg = MyQ->send_JobAttributes(JOB_ID_KEY(cluster_id, -1), *clusterad, SetAttribute_NoAck);
					if ( ! errmsg.empty()) {
						// TODO: is convering errmsg to error_stack() desirable here?
						submitHash.error_stack()->pushf("Submit", SCHEDD_ERR_SET_ATTRIBUTE_FAILED, "%s", errmsg.c_str());
						errmsg.clear();
						rval = -1;
						goto finis;
					}
					rval = 0;
				}
				condorID._cluster = jid.cluster;
				condorID._proc = jid.proc;
				condorID._subproc = 0;
			}

			if (is_factory) {
				// break out of the loop, we are done.
				break;
			}

			// send procad attributes
			errmsg = MyQ->send_JobAttributes(jid, *proc_ad, SetAttribute_NoAck);
			if ( ! errmsg.empty()) {
				// TODO: is convering errmsg to error_stack() desirable here?
				submitHash.error_stack()->pushf("Submit", SCHEDD_ERR_SET_ATTRIBUTE_FAILED, "%s", errmsg.c_str());
				errmsg.clear();
				rval = -1;
				goto finis;
			}
			rval = 0;

		} // end while

		// commit transaction and disconnect queue
		success = MyQ->disconnect(true, errstack);
		if ( ! success) {
			debug_printf(DEBUG_NORMAL, "Failed to submit job %s: %s\n", node->GetNodeName(), errstack.getFullText().c_str());
		} else { node->SetNumSubmitted(proc_id+1); }
	}

finis:
	submitHash.detachTransferMap();
	if (MyQ) {
		// if qmanager object is still open, cancel any pending transaction and disconnnect it.
		MyQ->disconnect(false, errstack);
		delete MyQ; MyQ = nullptr;
	}
	// report errors from submit
	if (rval < 0) {
		debug_printf(DEBUG_QUIET, "ERROR: on Line %d of submit file: %s\n", ms->source().line, errmsg.c_str());
		if (submitHash.error_stack()) {
			std::string errstk(submitHash.error_stack()->getFullText());
			if ( ! errstk.empty()) {
				debug_printf(DEBUG_QUIET, "submit error: %s", errstk.c_str());
			}
			submitHash.error_stack()->clear();
		}
	} else {
		// If submit succeeded, we still need to log any warning messages
		if (submitHash.error_stack()) {
			submitHash.warn_unused(stderr, "DAGMAN");
			std::string errstk(submitHash.error_stack()->getFullText());
			if ( ! errstk.empty()) {
				debug_printf(DEBUG_QUIET, "Submit warning: %s", errstk.c_str());
			}
			submitHash.error_stack()->clear();
		}
	}

	return success;
}


bool condor_submit(const Dagman &dm, Node* node, CondorID& condorID) {
	bool success = false;
	const char* directory = node->GetDirectory();
	TmpDir tmpDir;
	std::string errMsg;

	if ( ! tmpDir.Cd2TmpDir(directory, errMsg)) {
		debug_printf(DEBUG_QUIET, "Could not change to node directory %s: %s\n", directory, errMsg.c_str());
		return success;
	}

	DagSubmitMethod method = static_cast<DagSubmitMethod>(dm.options[deep::i::SubmitMethod]);
	switch (method) {
		case DagSubmitMethod::CONDOR_SUBMIT: // run condor_submit
			success = shell_condor_submit(dm, node, condorID);
			break;
		case DagSubmitMethod::DIRECT: // direct submit
			success = direct_condor_submitV2(dm, node, condorID);
			break;
		default:
			// We have unknown submission method requested so jobs will never be submitted abort
			debug_printf(DEBUG_NORMAL, "Error: Unknown submit method (%d)\n", (int)method);
			main_shutdown_rescue(EXIT_ERROR, DAG_STATUS_ERROR);
			break;
	}

	if ( ! tmpDir.Cd2MainDir(errMsg)) {
		debug_printf(DEBUG_QUIET, "Could not change to original directory: %s\n", errMsg.c_str());
		success = false;
	}

	return success;
}

bool send_reschedule(const Dagman & dm) {
	DagSubmitMethod method = static_cast<DagSubmitMethod>(dm.options[deep::i::SubmitMethod]);
	switch (method) {
		case DagSubmitMethod::CONDOR_SUBMIT: // condor_submit already rescheduled
			return true;
		default:
			break;
	}

	DCSchedd schedd;
	Stream::stream_type st = schedd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
	return schedd.sendCommand(RESCHEDULE, st, 0);
}

//-------------------------------------------------------------------------
// Subproc ID for "fake" events (for NOOP jobs).
static int _subprocID = 0;
void set_fake_condorID(int subprocID) { _subprocID = subprocID; }
int get_fake_condorID() { return _subprocID; }
//-------------------------------------------------------------------------
bool fake_condor_submit(CondorID& condorID, Node* node, const char* DAGNodeName, const char* directory, const char *logFile) {
	TmpDir tmpDir;
	std::string errMsg;
	if ( ! tmpDir.Cd2TmpDir(directory, errMsg)) {
		debug_printf(DEBUG_QUIET, "Could not change to node directory %s: %s\n",
		             directory, errMsg.c_str());
		return false;
	}

	_subprocID++;
	// Special HTCondorID for NOOP nodes -- actually indexed by
	// otherwise-unused subprocID.
	condorID._cluster = 0;
	condorID._proc = Node::NOOP_NODE_PROCID;
	condorID._subproc = _subprocID;

	// Make sure that this node gets marked as a NOOP
	if (node) { node->SetCondorID( condorID ); }

	WriteUserLog ulog;
	ulog.setUseCLASSAD(0);
	ulog.initialize(logFile, condorID._cluster, condorID._proc, condorID._subproc);

	SubmitEvent subEvent;
	subEvent.cluster = condorID._cluster;
	subEvent.proc = condorID._proc;
	subEvent.subproc = condorID._subproc;

	// We need some value for submitHost for the event to be read correctly.
	subEvent.setSubmitHost("<dummy-submit-for-noop-job>");

	subEvent.submitEventLogNotes = "DAG Node: ";
	subEvent.submitEventLogNotes += DAGNodeName;

	if ( ! ulog.writeEvent(&subEvent)) {
		EXCEPT("Error: writing dummy submit event for NOOP node failed!");
		return false;
	}


	JobTerminatedEvent termEvent;
	termEvent.cluster = condorID._cluster;
	termEvent.proc = condorID._proc;
	termEvent.subproc = condorID._subproc;
	termEvent.normal = true;
	termEvent.returnValue = 0;
	termEvent.signalNumber = 0;

	if ( ! ulog.writeEvent(&termEvent)) {
		EXCEPT("Error: writing dummy terminated event for NOOP node failed!");
		return false;
	}

	return true;
}

bool writePreSkipEvent(CondorID& condorID, Node* node, const char* DAGNodeName, const char* directory, const char *logFile) {
	TmpDir tmpDir;
	std::string errMsg;
	if ( ! tmpDir.Cd2TmpDir(directory, errMsg)) {
		debug_printf(DEBUG_QUIET, "Could not change to node directory %s: %s\n",
		             directory, errMsg.c_str());
		return false;
	}

	// Special HTCondorID for NOOP nodes -- actually indexed by
	// otherwise-unused subprocID.
	condorID._cluster = 0;
	condorID._proc = Node::NOOP_NODE_PROCID;

	condorID._subproc = 1 + get_fake_condorID();
	// Increment this value
	set_fake_condorID(condorID._subproc);

	if (node) { node->SetCondorID(condorID); }

	WriteUserLog ulog;
	ulog.setUseCLASSAD(0);
	ulog.initialize(logFile, condorID._cluster, condorID._proc, condorID._subproc);

	PreSkipEvent pEvent;
	pEvent.cluster = condorID._cluster;
	pEvent.proc = condorID._proc;
	pEvent.subproc = condorID._subproc;

	pEvent.skipEventLogNotes = "DAG Node: ";
	pEvent.skipEventLogNotes += DAGNodeName;

	if ( ! ulog.writeEvent(&pEvent)) {
		EXCEPT("Error: writing PRESKIP event failed!");
		return false;
	}
	return true;
}
