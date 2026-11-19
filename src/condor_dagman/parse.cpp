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

#include "node.h"
#include "parse.h"
#include "debug.h"
#include "dag_parser.h"
#include "tmp_dir.h"
#include "basename.h"
#include "condor_getcwd.h"

namespace conf = DagmanConfigOptions;

bool DagProcessor::process(const Dagman& dm, Dag& dag, const std::string& file, int dag_munge_id) {
	// DAG Commands only parsed at DAG submit time (in DAGManUtils)
	static const std::set<DAG::CMD> ignore_commands{
		DAG::CMD::CONFIG,
		DAG::CMD::SET_JOB_ATTR,
		DAG::CMD::ENV,
	};

	// Commands to parse on the first pass (i.e. node types and splices)
	static const std::set<DAG::CMD> pre_parse_commands{
		DAG::CMD::SUBMIT_DESCRIPTION,
		DAG::CMD::JOB,
		DAG::CMD::FINAL,
		DAG::CMD::PROVISIONER,
		DAG::CMD::SERVICE,
		DAG::CMD::SUBDAG,
		DAG::CMD::SPLICE,
		DAG::CMD::REJECT,
		DAG::CMD::DOT,
		DAG::CMD::NODE_STATUS_FILE,
		DAG::CMD::JOBSTATE_LOG,
	};

	// Change into DAG directory if UseDagDir is specified
	const char* file_to_parse = file.c_str();
	TmpDir dagDir;
	if (useDagDir) {
		std::string tmpDir = condor_dirname(file.c_str());
		std::string error;
		if (!dagDir.Cd2TmpDir(tmpDir.c_str(), error)) {
			debug_printf(DEBUG_QUIET, "ERROR: Failed to change into DAG directory %s: %s\n", tmpDir.c_str(), error.c_str());
			return false;
		}

		// The DAG file to parse is now relative to use so just use the file name
		file_to_parse = condor_basename(file.c_str());
	}

	DagParser parser(file_to_parse);

	parser.SearchFor(pre_parse_commands).Ignore(ignore_commands);
	if (config[conf::b::AllowIllegalChars]) { parser.AllowIllegalChars(); }

	if (parser.failed()) {
		debug_printf(DEBUG_QUIET, "ERROR: Failed to open %s: %s\n", file.c_str(), parser.c_error());
		return false;
	}

	// Verify that we are not recursively parsing a DAG file (i.e. INCLUDE/SPLICE)
	const std::string full_file_path = parser.GetAbsolutePath();
	auto [_, added] = parsed_file_check.insert(full_file_path);
	if (!added) {
		debug_printf(DEBUG_QUIET, "ERROR: Recursive DAG file parsing detected with %s\n", full_file_path.c_str());
		return false;
	}

	bool success = false;
	std::string location; // Save 'file:line#' of command in case processing error occurs

	// Pre Parse Loop
	for (const auto cmd : parser) {
		if (cmd && !ProcessCommand(dm, cmd, dag, dag_munge_id)) {
			location = cmd->Location();
			goto processing_failed;
		}
	}

	if (parser.failed()) {
		const auto error = parser.ParseError();
		if (error) {
			debug_printf(DEBUG_QUIET, "ERROR: %s\n", error->c_str());
			debug_printf(DEBUG_QUIET, "       Example sytanx: %s\n", error->syntax().c_str());
		} else {
			debug_printf(DEBUG_QUIET, "ERROR: Unknown file parse failure for %s\n", file.c_str());
		}

		goto processing_failed;
	}

	// Only reparse the file if we skipped lines in pre parse
	// (i.e. speed up for bag of nodes with no modifiers)
	if (parser.SkippedCommands()) {
		parser.reset().ClearSearch().Ignore(pre_parse_commands);

		// Second pass parse loop
		for (const auto cmd : parser) {
			if (cmd && !ProcessCommand(dm, cmd, dag, dag_munge_id)) {
				location = cmd->Location();
				goto processing_failed;
			}
		}

		if (parser.failed()) {
			const auto error = parser.ParseError();
			if (error) {
				debug_printf(DEBUG_QUIET, "ERROR: %s\n", error->c_str());
				debug_printf(DEBUG_QUIET, "       Example sytanx: %s\n", error->syntax().c_str());
			} else {
				debug_printf(DEBUG_QUIET, "ERROR: Unknown file parse failure for %s\n", file.c_str());
			}

			goto processing_failed;
		}
	}

	// If we were successful then inherit splice information
	dag.LiftSplices(SELF);
	// Also track initial and terminal nodes of graph
	dag.RecordInitialAndTerminalNodes();

	success = true;

processing_failed:
	if (!success && location.size()) { debug_printf(DEBUG_QUIET, "Processing error at %s\n", location.c_str()); }

	// Once we are done parsing the file remove it from recursive checking set
	parsed_file_check.erase(full_file_path);

	if (useDagDir) {
		std::string error;
		if (!dagDir.Cd2MainDir(error)) {
			debug_printf(DEBUG_QUIET, "ERROR: Failed to change back to original directory: %s\n", error.c_str());
			success = false;
		}
	}

	return success;
}

bool DagProcessor::ProcessCommand(const Dagman& dm, const DagCmd& cmd, Dag& dag, int dag_munge_id) {
	bool all_good = true;

	switch (cmd->GetCommand()) {
	case DAG::CMD::SUBMIT_DESCRIPTION: {
		const SubmitDescCommand* desc = DAG::DERIVE_CMD<SubmitDescCommand>(cmd);
		std::ignore = dag.add_inline_desc(desc->GetName(), desc->GetInlineDesc());
	} break;
	case DAG::CMD::SUBDAG:
	case DAG::CMD::JOB:
	case DAG::CMD::FINAL:
	case DAG::CMD::PROVISIONER:
	case DAG::CMD::SERVICE:
		all_good = ProcessNode(DAG::DERIVE_CMD<NodeCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::SPLICE:
		all_good = ProcessSplice(dm, dag, DAG::DERIVE_CMD<SpliceCommand>(cmd), dag_munge_id);
		break;
	case DAG::CMD::DOT: {
		const DotCommand* dot = DAG::DERIVE_CMD<DotCommand>(cmd);
		dag.SetDotFileUpdate(dot->Update());
		dag.SetDotFileOverwrite(dot->Overwrite());
		if (dot->HasInclude()) { dag.SetDotIncludeFileName(dot->GetInclude().c_str()); }
		dag.SetDotFileName(dot->GetFile().c_str());
	} break;
	case DAG::CMD::INCLUDE:
		all_good = process(dm, dag, (DAG::DERIVE_CMD<FileCommand>(cmd))->GetFile(), dag_munge_id);
		break;
	case DAG::CMD::NODE_STATUS_FILE: {
		const NodeStatusCommand* status = DAG::DERIVE_CMD<NodeStatusCommand>(cmd);
		dag.SetNodeStatusFileName(status->GetFile().c_str(), status->GetMinUpdateTime(), status->AlwaysUpdate());
		dag.SetNodeStatusFormat(status->GetFmt(), status->Compact());
	} break;
	case DAG::CMD::JOBSTATE_LOG:
		dag.SetJobstateLogFileName((DAG::DERIVE_CMD<FileCommand>(cmd))->GetFile().c_str());
		break;
	case DAG::CMD::REJECT:
		debug_printf(DEBUG_NORMAL, "DAG marked as rejected at %s\n", cmd->Location().c_str());
		all_good = false;
		break;
	case DAG::CMD::CATEGORY:
		all_good = ProcessCategory(DAG::DERIVE_CMD<CategoryCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::PARENT_CHILD:
		all_good = ProcessDependencies(DAG::DERIVE_CMD<ParentChildCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::SCRIPT:
		all_good = ProcessScript(DAG::DERIVE_CMD<ScriptCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::RETRY:
		all_good = ProcessRetry(DAG::DERIVE_CMD<RetryCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::ABORT_DAG_ON:
		all_good = ProcessAbortDagOn(DAG::DERIVE_CMD<AbortDagCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::VARS:
		all_good = ProcessVars(DAG::DERIVE_CMD<VarsCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::PRIORITY:
		all_good = ProcessPriority(DAG::DERIVE_CMD<PriorityCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::PRE_SKIP:
		all_good = ProcessPreSkip(DAG::DERIVE_CMD<PreSkipCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::TOLERANCE:
		all_good = ProcessTolerance(DAG::DERIVE_CMD<ToleranceCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::DONE:
		all_good = ProcessDone(DAG::DERIVE_CMD<DoneCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::SAVE_POINT_FILE:
		all_good = ProcessSaveFile(DAG::DERIVE_CMD<SavePointCommand>(cmd), dag, dag_munge_id);
		break;
	case DAG::CMD::MAXJOBS: {
		const MaxJobsCommand* mjc = DAG::DERIVE_CMD<MaxJobsCommand>(cmd);
		std::string category = mjc->GetCategory();
		dag._catThrottles.SetThrottle(&category, mjc->GetLimit());
	} break;
	case DAG::CMD::CONNECT: {
		const auto& [s1, s2] = (DAG::DERIVE_CMD<ConnectCommand>(cmd))->GetSplices();
		Dag* parent = dag.LookupSplice(MakeFullName(s1, dag_munge_id));
		Dag* child = dag.LookupSplice(MakeFullName(s2, dag_munge_id));

		if (!parent) { debug_printf(DEBUG_QUIET, "ERROR: Connect parent splice %s does not exist\n", s1.c_str()); }
		if (!child) { debug_printf(DEBUG_QUIET, "ERROR: Connect child splice %s does not exist\n", s2.c_str()); }

		all_good = (parent && child) ? Dag::ConnectSplices(parent, child) : false;
	} break;
	case DAG::CMD::PIN_IN:
	case DAG::CMD::PIN_OUT: {
		const PinCommand* pin = DAG::DERIVE_CMD<PinCommand>(cmd);
		std::string node = MakeFullName(pin->GetNode(), dag_munge_id);
		all_good = dag.SetPinInOut(!pin->IsPinOut(), node.c_str(), pin->GetPinNum());
	} break;
	default:
		// TODO: Fail or ignore?
		debug_printf(DEBUG_NORMAL, "DAGMan does not know how to process %s command yet...\n",
		             DAG::GET_KEYWORD_STRING(cmd->GetCommand()));
		break;
	} // End switch statement

	return all_good;
}

bool DagProcessor::ProcessNode(const NodeCommand* cmd, Dag& dag, int dag_munge_id) {
	// clang-format off
	static const std::map<DAG::CMD, NodeType> CMD_TO_NODE_TYPE = {
		{DAG::CMD::SUBDAG, NodeType::JOB},
		{DAG::CMD::JOB, NodeType::JOB},
		{DAG::CMD::FINAL, NodeType::FINAL},
		{DAG::CMD::PROVISIONER, NodeType::PROVISIONER},
		{DAG::CMD::SERVICE, NodeType::SERVICE}
	};
	// clang-format on

	ASSERT(CMD_TO_NODE_TYPE.contains(cmd->GetCommand()));

	std::string name = MakeFullName(cmd->GetName(), dag_munge_id);
	if (dag.NodeExists(name.c_str())) {
		debug_printf(DEBUG_QUIET, "ERROR: Node %s already exists in DAG.\n", name.c_str());
		return false;
	}

	if (dag.LookupSplice(name)) {
		debug_printf(DEBUG_QUIET, "ERROR: Node name %s already associated with a splice in DAG\n", name.c_str());
		return false;
	}

	std::string desc = cmd->GetSubmit();
	std::string final_node_dag;
	if (cmd->GetCommand() == DAG::CMD::SUBDAG) {
		desc += DAG_SUBMIT_FILE_SUFFIX;
	} else if (ends_with(desc, DAG_SUBMIT_FILE_SUFFIX)) {
		if (cmd->GetCommand() == DAG::CMD::FINAL) { // Allow this for final nodes (for LSST)
			final_node_dag = desc.substr(0, desc.rfind(DAG_SUBMIT_FILE_SUFFIX));
		} else {
			debug_printf(DEBUG_NORMAL, "Error: The use of the JOB keyword for nested DAGs is prohibited.\n");
			return false;
		}
	} else if (cmd->HasInlineDesc()) {
		static uint32_t inline_count = 0;
		desc = name + "-InlineDesc" + std::to_string(inline_count++);
		std::ignore = dag.add_inline_desc(desc, cmd->GetInlineDesc());
	}

	std::string directory;
	if (useDagDir) {
		if (cmd->HasDir()) {
			debug_printf(DEBUG_QUIET, "ERROR: Node DIR <directory> sub-command can not be used with -UseDagDir\n");
			return false;
		}
		condor_getcwd(directory);
	} else if (cmd->HasDir()) {
		directory = cmd->GetDir();
	}

	// TODO: hold nodes by value
	std::unique_ptr<Node> node(new Node(name.c_str(), directory.c_str(), desc.c_str()));
	ASSERT(node);

	node->SetType(CMD_TO_NODE_TYPE.at(cmd->GetCommand()));

	node->SetNoop(cmd->IsNoop());

	if (cmd->IsDone()) {
		if (node->GetType() != NodeType::JOB) {
			debug_printf(DEBUG_QUIET, "WARNING: %s node %s can not be marked as DONE\n", DAG::GET_KEYWORD_STRING(cmd->GetCommand()),
						 name.c_str());
			std::ignore = check_warning_strictness(DAG_STRICT_1, false);
		} else {
			dag.AddPreDoneNode(node.get());
		}
	}

	if (cmd->GetCommand() == DAG::CMD::SUBDAG) {
		node->SetDagFile(cmd->GetSubmit().c_str());
	} else if (!final_node_dag.empty()) {
		node->SetDagFile(final_node_dag.c_str());
	}

	if (!dag.Add(node.release())) {
		debug_printf(DEBUG_QUIET, "ERROR: Failed to add node %s to DAG.\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessSplice(const Dagman& dm, Dag& dag, const SpliceCommand* cmd, int dag_munge_id) {
	std::string name = MakeFullName(cmd->GetName(), dag_munge_id);

	if (dag.NodeExists(name.c_str())) {
		debug_printf(DEBUG_QUIET, "ERROR: Splice name %s already associated with a node in DAG.\n", cmd->GetName().c_str());
		return false;
	}

	if (dag.LookupSplice(name)) {
		debug_printf(DEBUG_QUIET, "ERROR: Splice named %s already exists in DAG.\n", cmd->GetName().c_str());
		return false;
	}

	// TODO: Have DAG hold splice DAG's by value and return reference to act upon
	std::string scope = name + "+";
	std::unique_ptr<Dag> splice_dag(new Dag(dm, true, scope));
	ASSERT(splice_dag);

	if (cmd->HasDir()) {
		splice_dag->SetDirectory(cmd->GetDir());
	}

	std::string error;
	TmpDir spliceDir;
	if (cmd->HasDir() && !spliceDir.Cd2TmpDir(cmd->GetDir().c_str(), error)) {
		debug_printf(DEBUG_QUIET, "ERROR: Failed to change to directory %s: %s\n", cmd->GetDir().c_str(), error.c_str());
		return false;
	}

	debug_printf(DEBUG_VERBOSE, "Parsing Splice %s in directory %s with file %s\n", cmd->GetName().c_str(),
				 cmd->HasDir() ? cmd->GetDir().c_str() : ".", cmd->GetDagFile().c_str());

	if (!process(dm, *splice_dag, cmd->GetDagFile(), dag_munge_id)) {
		debug_printf(DEBUG_QUIET, "ERROR: Failed to parse splice %s's DAG file %s\n", cmd->GetName().c_str(),
					 cmd->GetDagFile().c_str());
		return false;
	}

	if (cmd->HasDir() && !spliceDir.Cd2MainDir(error)) {
		debug_printf(DEBUG_QUIET, "ERROR: Failed to return to original directory: %s\n", error.c_str());
		return false;
	}

	if (splice_dag->HasFinalNode()) {
		debug_printf(DEBUG_QUIET, "ERROR: Splice %s has a FINAL node. This is not permitted.\n", cmd->GetName().c_str());
		return false;
	}

	splice_dag->PrefixAllNodeNames(scope);
	splice_dag->_catThrottles.PrefixAllCategoryNames(scope);

	// Print out a useful piece of debugging...
	if (DEBUG_LEVEL(DEBUG_DEBUG_1)) {
		splice_dag->PrintNodeList();
	}

	// Add splice to current DAG
	if (!dag.InsertSplice(name, splice_dag.release())) {
		debug_printf(DEBUG_QUIET, "ERROR: Failed to insert splice %s into DAG\n", name.c_str());
		return false;
	}

	return true;
}

size_t DagProcessor::join_node_id = 0;

bool DagProcessor::ProcessDependencies(const ParentChildCommand* cmd, Dag& dag, int dag_munge_id) {
	std::vector<Node*> parents;
	std::vector<Node*> children;

	for (const auto& n : cmd->GetParents()) {
		std::string name = MakeFullName(n.data(), dag_munge_id);
		Dag* splice = dag.LookupSplice(name);
		if (splice) {
			for (auto node : *(splice->FinalRecordedNodes())) {
				parents.push_back(node);
			}
		} else {
			Node* node = dag.FindNodeByName(name.c_str());
			if (!node) {
				debug_printf(DEBUG_QUIET, "ERROR: Unknown parent node %s specified\n", n.data());
				return false;
			}
			parents.push_back(node);
		}
	}

	for (const auto& n : cmd->GetChildren()) {
		std::string name = MakeFullName(n.data(), dag_munge_id);
		Dag* splice = dag.LookupSplice(name);
		if (splice) {
			for (auto node : *(splice->InitialRecordedNodes())) {
				children.push_back(node);
			}
		} else {
			Node* node = dag.FindNodeByName(name.c_str());
			if (!node) {
				debug_printf(DEBUG_QUIET, "ERROR: Unknown child node %s specified\n", n.data());
				return false;
			}
			children.push_back(node);
		}
	}

	const auto GetID = [](const Node* n) -> node_id_t { return n->GetNodeID(); };

	std::ranges::sort(parents, std::less{}, GetID);
	const auto duplicate_parents = std::ranges::unique(parents, std::equal_to{}, GetID);
	parents.erase(duplicate_parents.begin(), duplicate_parents.end());

	std::ranges::sort(children, std::less{}, GetID);
	const auto duplicate_children = std::ranges::unique(children, std::equal_to{}, GetID);
	children.erase(duplicate_children.begin(), duplicate_children.end());

	// DagArc metadata for the requested dependency strength, passed straight through
	// to Dag::Connect() -- ARC_WEAK is only meaningful on children-edge arcs.
	unsigned int meta = cmd->IsWeak() ? ARC_WEAK : 0;

	// If this statement has multiple parent nodes and multiple child nodes, we
	// can optimize the dag structure by creating an intermediate "join node"
	// connecting the two sets.
	if (config[conf::b::UseJoinNodes] && parents.size() > 1 && children.size() > 1) {
		std::string name = "_condor_join_node" + std::to_string(join_node_id++);
		if (dag.NodeExists(name.c_str()) || dag.LookupSplice(name)) {
			debug_printf(DEBUG_QUIET, "ERROR: Join node name %s already in use in DAG!!!\n", name.c_str());
			return false;
		}

		std::unique_ptr<Node> node(new Node(name.c_str(), "", "DNE.sub"));
		ASSERT(node);

		node->SetNoop(true);

		if (!dag.Add(node.release())) {
			debug_printf(DEBUG_QUIET, "ERROR: Failed to add join node %s to DAG.\n", name.c_str());
			return false;
		}

		Node* join = dag.FindNodeByName(name.c_str());

		// Connect parents to the join node -- this is where the requested dependency
		// strength actually applies. The join node is a NOOP that can't itself fail
		// once unblocked, so join -> children below always stays strong.
		std::vector<Node*> lst = {join};
		if (!dag.Connect(parents, lst, meta)) {
			debug_printf(DEBUG_QUIET, "ERROR: failed to add dependency to join node %s\n", join ? join->GetNodeName() : "unknown");
			return false;
		}

		// reset parent list to the join node and fall through to build the child edges
		parents.clear();
		parents.push_back(join);
		meta = 0;
	}

	if (!dag.Connect(parents, children, meta)) {
		debug_printf(DEBUG_QUIET, "ERROR: failed to add dependency between nodes\n");
		return false;
	}

	return true;
}

bool DagProcessor::ProcessDone(const DoneCommand* cmd, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(cmd->GetNodeName(), dag_munge_id);

	Node* node = dag.FindNodeByName(name.c_str());
	if (node) {
		if (node->GetType() != NodeType::JOB) {
			debug_printf(DEBUG_QUIET, "WARNING: %s node(s) can not be referenced by the DONE command\n",
						 DAG::GET_KEYWORD_STRING(cmd->GetCommand()));
			return !check_warning_strictness(DAG_STRICT_1, false);
		}
		node->SetStatus(Node::STATUS_DONE);
	} else {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessSaveFile(const SavePointCommand* sp, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(sp->GetNodeName(), dag_munge_id);
	bool all_good = false;

	Node* node = dag.FindNodeByName(name.c_str());
	if (!node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced\n", name.c_str());
	} else if (node->GetType() != NodeType::JOB) {
		debug_printf(DEBUG_QUIET, "ERROR: Save file can not be applied to %s node(s)\n", DAG::GET_KEYWORD_STRING(sp->GetCommand()));
	} else {
		node->SetSaveFile(sp->GetFilename());
		all_good = true;
	}

	return all_good;
}

bool DagProcessor::ProcessVars(const VarsCommand* vars, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(vars->GetNodeName(), dag_munge_id);

	bool prepend = !config[conf::b::AppendVars];
	if (vars->ExplicitPlacement()) {
		prepend = vars->WantPrepend();
	}

	bool found_a_node = false;
	for (Node* node : dag.FindAllNodes(name)) {
		found_a_node = true;
		for (const auto& [key, value] : vars->GetPairs()) {
			std::ignore = node->AddVar(key.data(), value.data(), prepend);
			debug_printf(DEBUG_DEBUG_1, "Adding %s='%s' to %s\n", key.data(), value.data(), node->GetNodeName());
		}
	}

	if (!found_a_node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced.\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessScript(const ScriptCommand* cmd, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(cmd->GetNodeName(), dag_munge_id);

	bool found_a_node = false;
	for (Node* node : dag.FindAllNodes(name)) {
		found_a_node = true;

		// TODO: Just use the dag_command.h enum class for script type every where in DAGMan
		//       I am deferring this work for now since that will touch a lot of code
		ScriptType type = static_cast<ScriptType>((int)(cmd->GetType()));

		std::unique_ptr<Script> script(new Script(type, cmd->GetScript()));
		ASSERT(script);

		if (cmd->HasDeferal()) {
			const auto [status, defer] = cmd->GetDeferal();
			script->SetDeferal(status, defer);
		}

		if (cmd->WantsDebug()) {
			const auto [file, when] = cmd->GetDebugInfo();
			script->SetDebug(file, when);
		}

		if (!node->AddScript(script.release())) {
			debug_printf(DEBUG_QUIET, "ERROR: Failed to add %s script to node %s\n", cmd->GetTypeStr(), node->GetNodeName());
			return false;
		}
	}

	if (!found_a_node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced.\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessRetry(const RetryCommand* retry, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(retry->GetNodeName(), dag_munge_id);

	bool found_a_node = false;
	for (Node* node : dag.FindAllNodes(name)) {
		found_a_node = true;

		if (node->GetType() != NodeType::JOB) {
			debug_printf(DEBUG_QUIET, "ERROR: %s node %s can not have a retry specification\n",
						 DAG::GET_KEYWORD_STRING(retry->GetCommand()), node->GetNodeName());
			return false;
		}

		node->SetMaxRetries(retry->GetMaxRetries(), retry->GetBreakCode());
	}

	if (!found_a_node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced.\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessAbortDagOn(const AbortDagCommand* ado, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(ado->GetNodeName(), dag_munge_id);

	bool found_a_node = false;
	for (Node* node : dag.FindAllNodes(name)) {
		found_a_node = true;

		if (node->GetType() == NodeType::FINAL) {
			debug_printf(DEBUG_QUIET, "ERROR: FINAL node %s cannot have an ABORT-DAG-ON specification.\n", node->GetNodeName());
			return false;
		}

		node->SetAbortDagOn(ado->GetCondition(), ado->GetExitValue());
	}

	if (!found_a_node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced.\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessPreSkip(const PreSkipCommand* skip, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(skip->GetNodeName(), dag_munge_id);

	bool found_a_node = false;
	for (Node* node : dag.FindAllNodes(name)) {
		found_a_node = true;

		std::string error;
		if (!node->AddPreSkip(skip->GetExitCode(), error)) {
			debug_printf(DEBUG_QUIET, "ERROR: Failed to add pre skip to %s: %s\n", node->GetNodeName(), error.c_str());
			return false;
		}
	}

	if (!found_a_node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessTolerance(const ToleranceCommand* cmd, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(cmd->GetNodeName(), dag_munge_id);

	bool found_a_node = false;
	for (Node* node : dag.FindAllNodes(name)) {
		found_a_node = true;

		node->SetTolerance(cmd->GetTolerance(), cmd->GetMode(), cmd->IsPercentage());
	}

	if (!found_a_node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessPriority(const PriorityCommand* prio, Dag& dag, int dag_munge_id) {
	std::string name = MakeFullName(prio->GetNodeName(), dag_munge_id);

	bool found_a_node = false;
	for (Node* node : dag.FindAllNodes(name)) {
		found_a_node = true;

		if (node->GetType() != NodeType::JOB) {
			debug_printf(DEBUG_QUIET, "ERROR: %s node %s can not have a priority\n", DAG::GET_KEYWORD_STRING(prio->GetCommand()),
						 node->GetNodeName());
			return false;
		}

		int current = node->GetExplicitPrio();
		if (current && current != prio->GetPriority()) {
			debug_printf(DEBUG_NORMAL, "WARNING: New priority %d for node %s overrides old value %d\n", prio->GetPriority(),
						 node->GetNodeName(), current);
			check_warning_strictness(DAG_STRICT_2);
		}

		node->SetPrio(prio->GetPriority());
	}

	if (!found_a_node) {
		debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced\n", name.c_str());
		return false;
	}

	return true;
}

bool DagProcessor::ProcessCategory(const CategoryCommand* cat, Dag& dag, int dag_munge_id) {
	for (const auto& n : cat->GetNodes()) {
		std::string name = MakeFullName(n.data(), dag_munge_id);

		bool found_a_node = false;
		for (Node* node : dag.FindAllNodes(name)) {
			found_a_node = true;

			if (node->GetType() != NodeType::JOB) {
				debug_printf(DEBUG_QUIET, "ERROR: %s node %s can not be added to a category\n",
							 DAG::GET_KEYWORD_STRING(cat->GetCommand()), node->GetNodeName());
				return false;
			}

			node->SetCategory(cat->GetCategory().c_str(), dag._catThrottles);
		}

		if (!found_a_node) {
			debug_printf(DEBUG_QUIET, "ERROR: Unknown node %s referenced\n", name.c_str());
			return false;
		}
	}

	return true;
}
