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
#include "dag_parser.h"
#include "submit_utils.h"
#include "condor_version.h"
#include "my_username.h"
#include "edge.h"
#include "dag.hpp"

#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>
#include <filesystem>
#include <system_error>
#include <functional>

//      EXIT SUCCESS 0     All good
//      EXIT_FAILURE 1     Invalid DAG (parse failure)
#define EXIT_ERROR   2  // Tool setup/execution failure

// Attribute keys localized for DAG statistics
#define DAG_STAT_TOTAL_NODES       "TotalNodes"
#define DAG_STAT_NUM_NODES         "NumNodes"
#define DAG_STAT_NUM_SERVICE       "NumServiceNodes"
#define DAG_STAT_NUM_JOIN          "NumJoinNodes"
#define DAG_STAT_NUM_SUBDAGS       "NumSubDAGs"
#define DAG_STAT_NUM_ARCS          "NumArcs"
#define DAG_STAT_NUM_SPLICES       "NumSplices"
#define DAG_STAT_FINAL_NODE        "FinalNode"
#define DAG_STAT_PROVISIONER_NODE  "ProvisionerNode"
#define DAG_STAT_HAS_CYCLE         "IsCyclic"
#define DAG_STAT_DETECTED_CYCLE    "DetectedCycle"
#define DAG_STAT_HEIGHT            "GraphHeight"
#define DAG_STAT_WIDTH             "GraphWidth"
#define DAG_STAT_WIDTH_BY_DEPTH    "WidthByDepth"

constexpr short CHECK_SUB_DAGS = (1 << 0);
constexpr short CHECK_JDL      = (1 << 1);
constexpr short CHECK_SCRIPTS  = (1 << 2);
constexpr short CHECK_EXTERNAL = std::numeric_limits<short>::max();


class ChangeDir {
public:
	ChangeDir() = delete;
	ChangeDir(std::filesystem::path cd, bool do_cd = true) {
		if (do_cd) {
			original = std::filesystem::current_path(ec);
			if ( ! ec) {
				std::filesystem::current_path(cd, ec);
				if ( ! ec) { switched = true; }
			}
		}
	}

	~ChangeDir() {
		if (switched) {
			std::filesystem::current_path(original);
		}
	}

	bool failed() const { return (bool)ec; }
	std::string error() const { return ec.message(); }
private:
	std::filesystem::path original{};
	std::error_code ec{};
	bool switched{false};
};


void addCommandError(const BaseDagCommand* cmd, const std::string& err, std::vector<DagParseError>& errors) {
	auto [src, line] = cmd->GetSource();
	auto& ref = errors.emplace_back(src, line, err);
	ref.SetCommand(cmd->GetCommand());
}

// Helper macro function to store error and return the caller
#define FAIL_AND_RETURN(cmd, msg, errors) addCommandError(cmd, msg, errors); return;


// Config flags shared across every MockDag in this run -- not per-scope DAG data, so
// they live at file scope, same as `included_files` below.
static short check_external = 0; // Check all JDL simulation for job submission
static bool strict = false; // Check extra stuff with stricter rules e.g. JDL files exist
static bool use_join_nodes = false;

struct MockDagData; // forward declare -- needed by MockDagNode below
struct MockDagNode; // forward declare -- needed by the alias right below
using MockDag = Dag<MockDagData, MockDagNode>;

struct MockDagNode {
	std::string name;
	DAG::CMD type{DAG::CMD::JOB};
};

struct MockDagData {
	void SetParent(MockDag& dag) { self = std::addressof(dag); }

	bool hasSplice(const std::string& name) const {
		return std::ranges::find(splices, name, [](const MockDag& d) { return d.data.scope; }) != splices.end();
	}

	bool hasNode(const std::string& name) const {
		return node_name_hash.contains(name);
	}

	bool contains(const std::string& name) const {
		return hasNode(name) || hasSplice(name);
	}

	inline bool hasFinal() const { return ! final_node.empty(); }
	inline bool hasProvisioner() const { return ! provisioner.empty(); }

	void makeDependencies(const ParentChildCommand* pc, std::vector<DagParseError>& errors) {
		std::vector<node_id_t> parents;
		std::vector<node_id_t> children;

		std::string missing_parents;
		std::string missing_children;

		for (const auto& name : pc->GetParents())
			considerDependency(pc, name.data(), true, parents, missing_parents, errors);

		for (const auto& name : pc->GetChildren())
			considerDependency(pc, name.data(), false, children, missing_children, errors);

		if ( ! missing_parents.empty() || ! missing_children.empty()) {
			std::string err = "References to undefined nodes:";
			if ( ! missing_parents.empty())  { err += " Parents [" + missing_parents + "]"; }
			if ( ! missing_children.empty()) { err += " Children [" + missing_children + "]"; }
			addCommandError(pc, err, errors);
			return;
		}

		if (use_join_nodes && parents.size() > 1 && children.size() > 1) {
			std::string join_node_name = "_condor_join_node" + std::to_string(++join_node_index);
			node_id_t join_node = self->AddNode(join_node_name, DAG::CMD::JOB);

			std::ignore = self->Connect(parents, {join_node});

			parents.clear();
			parents.push_back(join_node);
		}

		std::ignore = self->Connect(parents, children);
	}

	std::string addNode(const std::string& name, DAG::CMD t) {
		if (contains(name)) {
			return "Node " + name + " is already defined in the DAG";
		} else if (t == DAG::CMD::FINAL) {
			if (hasFinal()) { return "Final node is already defined in the DAG"; }
			else if ( ! scope.empty()) { return "Spliced DAGs can not contain a final node"; }
		} else if (t == DAG::CMD::PROVISIONER && hasProvisioner()) {
			return "Provisioner node is already defined in the DAG";
		}

		node_id_t id = self->AddNode(name, t);
		node_name_hash[name] = id;

		if (t == DAG::CMD::FINAL) {
			final_node = name;
		} else if (t == DAG::CMD::PROVISIONER) {
			provisioner = name;
		}

		return "";
	}

	MockDag* addSplice(const std::string& name, std::string& error) {
		if (contains(name)) {
			error = "Splice name " + name + " is already defined in the DAG";
			return nullptr;
		}

		auto& splice = splices.emplace_back();
		splice.data.scope = name;

		return &splice;
	}

	// Folds every splice's own independent graph into this one: re-AddNode()s each of
	// its nodes (name-prefixed with the splice's scope, same as production/real
	// DAGMan splice semantics) and re-Connect()s each of its edges, translating ids
	// through a per-splice old-id -> new-id map. This is what lets a PARENT_CHILD
	// reference to a splice by name later resolve to that splice's boundary
	// (leaf/root) nodes exactly as if they'd always lived directly in this DAG --
	// unlike a SUBDAG, which stays a fully separate, independently-checked DAG (see
	// checkSubdag()).
	void inheritSpliceNodes() {
		for (auto& splice : splices) {
			num_splices += 1 + splice.data.num_splices;

			std::vector<node_id_t> id_map(splice.NumNodes());

			for (auto& node : splice) {
				std::string name = splice.data.scope + "+" + node.data.name;
				node_id_t new_id = self->AddNode(name, node.data.type);
				node_name_hash[name] = new_id;
				id_map[node.GetID()] = new_id;
			}

			for (auto& node : splice) {
				std::vector<node_id_t> child_ids;
				splice.VisitChildren(node, [&child_ids](MockDag&, Node<MockDagNode>&, Node<MockDagNode>& child) -> int {
					child_ids.push_back(child.GetID());
					return 0;
				});

				if ( ! child_ids.empty()) {
					std::vector<node_id_t> new_children;
					new_children.reserve(child_ids.size());
					for (auto cid : child_ids) { new_children.push_back(id_map[cid]); }
					std::ignore = self->Connect({id_map[node.GetID()]}, new_children);
				}
			}
		}
	}

	void getStats(ClassAd& stats) {
		std::map<std::string, uint32_t> node_counts;
		bool has_graph_nodes = false;

		for (auto& node : *self) {
			switch (node.data.type) {
				case DAG::CMD::JOB:
					has_graph_nodes = true;
					if (node.data.name.starts_with("_condor_join_node"))
						node_counts[DAG_STAT_NUM_JOIN]++;
					else
						node_counts[DAG_STAT_NUM_NODES]++;
					break;
				case DAG::CMD::SERVICE:
					node_counts[DAG_STAT_NUM_SERVICE]++;
					break;
				case DAG::CMD::SUBDAG:
					has_graph_nodes = true;
					node_counts[DAG_STAT_NUM_SUBDAGS]++;
					break;
				case DAG::CMD::FINAL:
				case DAG::CMD::PROVISIONER:
					// Already accounted for
					break;
				default:
					// TODO: We should never get here!!!
					break;
			}
		}

		stats.InsertAttr(DAG_STAT_TOTAL_NODES, (int)self->NumNodes());

		for (const auto& [key, count]: node_counts) {
			stats.InsertAttr(key, (int)count);
		}

		size_t num_arcs = self->GetEdgeTable().ArcCount();
		if (num_arcs > 0) { stats.InsertAttr(DAG_STAT_NUM_ARCS, (int)num_arcs); }
		if (num_splices > 0) { stats.InsertAttr(DAG_STAT_NUM_SPLICES, (int)num_splices); }

		if (hasFinal()) {
			stats.InsertAttr(DAG_STAT_FINAL_NODE, final_node);
		}

		if (hasProvisioner()) {
			stats.InsertAttr(DAG_STAT_PROVISIONER_NODE, provisioner);
		}

		if (has_graph_nodes) {
			std::vector<node_id_t> ancestors;

			if (self->Cycle(&ancestors)) {
				// Only non-special nodes count as a real traversal root -- FINAL/
				// SERVICE/PROVISIONER can't have dependencies (considerDependency()),
				// so they're always isolated and must not count toward "found a root".
				bool found_start_nodes = std::ranges::any_of(*self, [](const auto& node) {
					switch (node.data.type) {
						case DAG::CMD::FINAL:
						case DAG::CMD::SERVICE:
						case DAG::CMD::PROVISIONER:
							return false;
						default:
							return node.NoParents();
					}
				});

				std::string not_visited;
				std::ranges::for_each(*self, [&not_visited](const auto& node){
					if ( ! node.WasVisited()) {
						not_visited += (not_visited.empty() ? "" : ",") + node.data.name;
					}
				});

				stats.InsertAttr(DAG_STAT_HAS_CYCLE, true);
				std::string cycle_msg = "No initial nodes discovered";

				if (found_start_nodes) {
					if ( ! ancestors.empty()) {
						cycle_msg.clear();
						for (auto id : ancestors) {
							cycle_msg += (cycle_msg.empty() ? "" : " -> ") + (*self)[id].data.name;
						}
					} else if ( ! not_visited.empty()) {
						cycle_msg = "Failed to visit nodes " + not_visited;
					}
				}

				stats.InsertAttr(DAG_STAT_DETECTED_CYCLE, cycle_msg);
			} else {
				uint32_t height = 1;
				std::vector<uint32_t> widths{0};

				self->Walk([&height, &widths](MockDag&, Node<MockDagNode>& node, size_t depth) {
					switch (node.data.type) {
						case DAG::CMD::FINAL:
						case DAG::CMD::SERVICE:
						case DAG::CMD::PROVISIONER:
							return; // trivial isolated roots -- excluded from height/width stats
						default:
							break;
					}

					if (widths.size() <= depth) { widths.resize(depth + 1, 0); }
					widths[depth]++;
					height = MAX((uint32_t)depth + 1, height);
				}, WalkOrder::DFS);

				std::erase_if(widths, [](uint32_t n) { return n == 0; });

				stats.InsertAttr(DAG_STAT_HEIGHT, (int)height);

				uint32_t max = *(std::ranges::max_element(widths));
				stats.InsertAttr(DAG_STAT_WIDTH, (int)max);

				std::string list;
				for (const auto& n : widths) { list += (list.empty() ? "" : ",") + std::to_string(n); }
				list = "{" + list + "}";

				stats.AssignExpr(DAG_STAT_WIDTH_BY_DEPTH, list.c_str());
			}
		}
	}

private:
	void considerDependency(const ParentChildCommand* pc, const std::string& name, const bool is_parent, std::vector<node_id_t>& list, std::string& missing, std::vector<DagParseError>& errors) {
		static std::set<DAG::CMD> invalid = {
			DAG::CMD::FINAL,
			DAG::CMD::SERVICE,
			DAG::CMD::PROVISIONER,
		};

		if (hasNode(name)) {
			node_id_t id = node_name_hash[name];
			DAG::CMD type = (*self)[id].data.type;
			if (invalid.contains(type)) {
				std::string type_str(DAG::GET_KEYWORD_STRING(type));
				addCommandError(pc, type_str + " node " + name + " can not have dependencies", errors);
			} else {
				list.push_back(id);
			}
		} else if (hasSplice(name)) {
			auto it = std::ranges::find(splices, name, [](const MockDag& d) { return d.data.scope; });
			std::string prefix = it->data.scope + "+";
			for (auto& node : *self) {
				bool is_splice_end = is_parent ? node.NoChildren() : node.NoParents();
				if (node.data.name.starts_with(prefix) && is_splice_end) {
					if (invalid.contains(node.data.type)) {
						std::string type_str(DAG::GET_KEYWORD_STRING(node.data.type));
						addCommandError(pc, type_str + " node " + name + " can not have dependencies", errors);
					} else {
						list.push_back(node.GetID());
					}
				}
			}
		} else {
			if ( ! missing.empty()) { missing += ","; }
			missing += name;
		}
	}

	std::map<std::string, node_id_t> node_name_hash{};
	std::vector<MockDag> splices{};

	// "" for the top-level DAG (and every independent SUBDAG), or this splice's own
	// declared name otherwise (unprefixed -- just what it's called at its own scope).
	std::string scope{};

	std::string final_node{};
	std::string provisioner{};

	size_t num_splices{0};
	uint32_t join_node_index{0};

	MockDag* self{nullptr};
};


static std::set<std::string> included_files;


enum class JDL {
	PATH = 0,
	INLINE = 1,
};


// Forward declaration
void parseDAG(DagParser& parser, MockDag& dag, std::vector<DagParseError>& errors);


void checkSubdag(const SubdagCommand* cmd, const bool strict, std::vector<DagParseError>& errors) {
	ChangeDir cd(cmd->GetDir(), cmd->HasDir());
	if (cd.failed()) {
		if (strict) { addCommandError(cmd, cd.error(), errors); }
		return;
	}

	std::string source = cmd->GetSubmit();
	if (std::filesystem::exists(source)) {
		DagParser parser(source);
		if (parser.failed()) {
			FAIL_AND_RETURN(cmd, parser.error(), errors);
		}

		const std::string file = parser.GetAbsolutePath();
		if (included_files.contains(file)) {
			FAIL_AND_RETURN(cmd, "Recursive sub-DAG detected", errors);
		}

		std::vector<DagParseError> subdag_errors;
		MockDag subdag;

		included_files.insert(file);
		parseDAG(parser, subdag, subdag_errors);
		included_files.erase(file);

		if ( ! subdag_errors.empty()) {
			for (const auto& err : subdag_errors) {
				addCommandError(cmd, err.GetLocation() + ">" + err.GetError(), errors);
			}
		}
	} else if (strict) {
		addCommandError(cmd, source + " does not exist", errors);
	}
}


void checkJDL(const BaseDagCommand* cmd, const std::string& jdl, const JDL src,
              std::vector<DagParseError>& errors, std::map<std::string, DagParseError>* jdl_dne = nullptr) {
	std::string queue_args;
	auto_free_ptr owner(my_username());
	char* tmp_qline = nullptr;

	MacroStreamFile msf;
	MACRO_SOURCE msm_source;
	MacroStreamMemoryFile msm(nullptr, 0, msm_source);
	MacroStream* ms = nullptr;

	SubmitHash submitHash;
	SubmitStepFromQArgs ssi(submitHash);

	submitHash.init(JSM_DAGMAN);
	submitHash.setDisableFileChecks(true);
	submitHash.setScheddVersion(CondorVersion());
	submitHash.init_base_ad(time(nullptr), owner);

	std::string errmsg;

	switch (src) {
		case JDL::PATH:
			{
				// Only node commands can provide a directory
				const NodeCommand* node = dynamic_cast<const NodeCommand*>(cmd);
				ASSERT(node);
				ChangeDir cd(node->GetDir(), node->HasDir());
				if (cd.failed()) {
					FAIL_AND_RETURN(cmd, cd.error(), errors);
				}

				if (std::filesystem::exists(jdl)) {
					if ( ! msf.open(jdl.c_str(), false, submitHash.macros(), errmsg)) {
						FAIL_AND_RETURN(cmd, errmsg, errors);
					}

					ms = &msf;
					// set submit filename into the submit hash so that $(SUBMIT_FILE) works
					submitHash.insert_submit_filename(jdl.c_str(), msf.source());
				} else {
					// Defer to higher powers to ensure this is not an inline submit description reference
					if (jdl_dne) {
						auto [src, line] = cmd->GetSource();
						auto [ref, _] = jdl_dne->insert({jdl, DagParseError(src, line, "Submit JDL does not exist")});
						ref->second.SetCommand(cmd->GetCommand());
					}

					return;
				}
			}
			break;
		case JDL::INLINE:
			msm.set(jdl.c_str(), jdl.size(), 0, msm_source);
			ms = &msm;
			submitHash.insert_submit_filename("internal-jdl", msm_source);
			break;
	}

	ASSERT(ms);

	if (submitHash.parse_up_to_q_line(*ms, errmsg, &tmp_qline)) {
		FAIL_AND_RETURN(cmd, "Failed to parse to queue line: " + errmsg, errors);
	}

	if (tmp_qline) {
		const char* qargs = submitHash.is_queue_statement(tmp_qline);
		if (qargs) { queue_args = qargs; }
	}

	if (ssi.init(queue_args.c_str(), errmsg) != 0) {
		FAIL_AND_RETURN(cmd, "Invalid queue args: " + queue_args, errors);
	}

	if (ssi.load_items(*ms, false, errmsg) < 0) {
		FAIL_AND_RETURN(cmd, "Failed to load submit step items: " + errmsg, errors);
	}

	// Load extended submit commands from config (like a schedd would provide)
	std::string extended_cmds;
	param(extended_cmds, "EXTENDED_SUBMIT_COMMANDS");
	if ( ! extended_cmds.empty()) {
		ClassAd ext_cmd_ad;
		initAdFromString(extended_cmds.c_str(), ext_cmd_ad);
		submitHash.addExtendedCommands(ext_cmd_ad);
	}

	// Simulate job submission: ignore late materialization, iterate item data directly
	constexpr int cluster_id = 1;
	int item_index = 0, step = 0, rval = 0;
	JOB_ID_KEY jid(cluster_id, 0);
	ssi.begin(jid, true);

	bool iter_selected = true;
	while ((rval = ssi.next_impl(iter_selected, jid, item_index, step, iter_selected)) > 0) {
		ClassAd* proc_ad = submitHash.make_job_ad(jid, item_index, step, false, false, nullptr, nullptr);
		if ( ! proc_ad) {
			CondorError* err = submitHash.error_stack();
			FAIL_AND_RETURN(cmd, err ? err->getFullText() : "Submit description produced invalid job ad", errors);
		}

		// Break after first successful job ad unless strict checking is enabled
		if ( ! strict) { break; }
	}

	if (rval < 0) {
		FAIL_AND_RETURN(cmd, "Failed to iterate submit description items: " + errmsg, errors);
	} else if (submitHash.error_stack()) {
		submitHash.warn_unused(stderr, "DAGMAN");
		std::string errstk(submitHash.error_stack()->getFullText());
		if ( ! errstk.empty()) {
			FAIL_AND_RETURN(cmd, "Submit warning: " + errstk, errors);
		}
	}
}


void parseDAG(DagParser& parser, MockDag& dag, std::vector<DagParseError>& errors) {
	static istring_view all_nodes_keyword(DAG::ALL_NODES.c_str());

	std::vector<DagCmd> commands{};
	std::map<std::string, DagParseError> node_jdl_dne;

	for (auto cmd : parser) {
		if (cmd) {
			DAG::CMD cmd_val = cmd->GetCommand();
			switch (cmd_val) {
				case DAG::CMD::SUBDAG:
				case DAG::CMD::JOB:
				case DAG::CMD::FINAL:
				case DAG::CMD::PROVISIONER:
				case DAG::CMD::SERVICE:
					{
						const NodeCommand* node = DAG::DERIVE_CMD<NodeCommand>(cmd);

						std::string err = dag.data.addNode(node->GetName(), cmd_val);
						if ( ! err.empty()) {
							addCommandError(node, err, errors);
						}

						if (cmd_val == DAG::CMD::SUBDAG && check_external & CHECK_SUB_DAGS) {
							std::string parent_dag = parser.GetAbsolutePath();
							included_files.insert(parent_dag);
							checkSubdag(DAG::DERIVE_CMD<SubdagCommand>(cmd), strict, errors);
							included_files.erase(parent_dag);
						} else if (check_external & CHECK_JDL){
							std::string src = node->GetSubmit();
							JDL type = JDL::PATH;
							if (node->HasInlineDesc()) {
								src = node->GetInlineDesc();
								type = JDL::INLINE;
							}

							checkJDL(cmd.get(), src, type, errors, &node_jdl_dne);
						}
					}
					break;
				case DAG::CMD::SPLICE:
					{
						const SpliceCommand* splice = DAG::DERIVE_CMD<SpliceCommand>(cmd);

						std::string err;
						MockDag* splice_dag = dag.data.addSplice(splice->GetName(), err);
						if ( ! err.empty()) {
							addCommandError(splice, err, errors);
							continue;
						}

						ChangeDir cd(splice->GetDir(), splice->HasDir());
						if (cd.failed()) {
							addCommandError(splice, "Failed to read splice " + splice->GetName() + ": " + cd.error(), errors);
							continue;
						}

						DagParser sp(splice->GetDagFile());
						if (sp.failed()) {
							addCommandError(splice, "Failed to read splice " + splice->GetName() + ": " + sp.error(), errors);
						} else {
							sp.InheritOptions(parser);

							const std::string file = sp.GetAbsolutePath();
							if (included_files.contains(file)) {
								addCommandError(splice, "Recursive splicing detected", errors);
								continue;
							}

							included_files.insert(file);
							parseDAG(sp, *splice_dag, errors);
							included_files.erase(file);
						}
					}
					break;
				case DAG::CMD::INCLUDE:
					{
						const IncludeCommand* include = DAG::DERIVE_CMD<IncludeCommand>(cmd);
						std::string file = include->GetFile();

						DagParser ip(file);
						if (ip.failed()) {
							addCommandError(include, "Failed to read include file: " + ip.error(), errors);
						} else {
							ip.InheritOptions(parser);

							file = ip.GetAbsolutePath();
							if (included_files.contains(file)) {
								addCommandError(include, "Recursive file inclusion detected", errors);
								continue;
							}

							included_files.insert(file);
							parseDAG(ip, dag, errors);
							included_files.erase(file);
						}
					}
					break;
				default:
					// Relinquish pointer ownership to commands vector
					commands.emplace_back(cmd.release());
					break;
			} // End Switch(cmd)
		} // End if(cmd)
	} // End parser for loop

	const auto& new_errors = parser.GetParseErrorList();
	errors.insert(errors.end(), new_errors.begin(), new_errors.end());

	dag.data.inheritSpliceNodes();

	std::ranges::sort(commands, [](DagCmd& l, DagCmd& r) { return l->GetCommand() < r->GetCommand(); });

	bool has_config = false;

	for (const auto& cmd : commands) {
		DAG::CMD cmd_val = cmd->GetCommand();
		switch (cmd_val) {
			case DAG::CMD::SCRIPT:
			case DAG::CMD::RETRY:
			case DAG::CMD::ABORT_DAG_ON:
			case DAG::CMD::VARS:
			case DAG::CMD::PRIORITY:
			case DAG::CMD::PRE_SKIP:
			case DAG::CMD::TOLERANCE:
			case DAG::CMD::DONE:
				{
					const NodeModifierCommand* mod = DAG::DERIVE_CMD<NodeModifierCommand>(cmd);
					const std::string node_name = mod->GetNodeName();

					std::string error;

					if (node_name.c_str() == all_nodes_keyword) {
						// Allow ALL_NODES (case insensitive)
						if (cmd_val == DAG::CMD::DONE) { error = "ALL_NODES can not be used with DONE command"; }
					} else if (dag.data.hasSplice(node_name)) {
						error = "Cannot be applied to splice " + node_name;
					} else if ( ! dag.data.hasNode(node_name)) {
						error = "References undefined node " + node_name;
					}

					if (check_external & CHECK_SCRIPTS && cmd_val == DAG::CMD::SCRIPT) {
						const ScriptCommand* script = DAG::DERIVE_CMD<ScriptCommand>(cmd);
						if ( ! std::filesystem::exists(script->GetScript())) {
							addCommandError(script, "Script file does not exist", errors);
						}
					}

					if ( ! error.empty()) {
						addCommandError(mod, error, errors);
					}
				}
				break;
			case DAG::CMD::PARENT_CHILD:
				dag.data.makeDependencies(DAG::DERIVE_CMD<ParentChildCommand>(cmd), errors);
				break;
			case DAG::CMD::CATEGORY:
				{
					const CategoryCommand* cat = DAG::DERIVE_CMD<CategoryCommand>(cmd);
					std::string missing;

					for (const auto& node : cat->GetNodes()) {
						if ( ! dag.data.hasNode(node.data()) && node.data() != all_nodes_keyword) {
							if ( ! missing.empty()) { missing += ","; }
							missing += node.data();
						}
					}

					if ( ! missing.empty()) {
						addCommandError(cat, "References to undefined nodes: " + missing, errors);
					}
				}
				break;
			case DAG::CMD::CONFIG:
				{
					const ConfigCommand* config = DAG::DERIVE_CMD<ConfigCommand>(cmd);
					std::filesystem::path conf(config->GetFile());
					std::string error;

					if (has_config) {
						error = "DAG configuration file is already defined";
					} else if ( ! std::filesystem::exists(conf)) {
						error = "Configuration file " + config->GetFile() + " does not exist";
					}

					has_config = true;

					if ( ! error.empty()) {
						addCommandError(config, error, errors);
					}
				}
				break;
			case DAG::CMD::REJECT:
				{
					auto [src, line] = cmd->GetSource();
					errors.emplace_back(src, line, "DAG marked with REJECT command");
				}
				break;
			case DAG::CMD::SUBMIT_DESCRIPTION:
				if (check_external & CHECK_JDL) {
					const SubmitDescCommand* desc = DAG::DERIVE_CMD<SubmitDescCommand>(cmd);
					std::ignore = node_jdl_dne.erase(desc->GetName());
					checkJDL(desc, desc->GetInlineDesc(), JDL::INLINE, errors);
				}
			default:
				break;
		} // End switch(cmd)
	} // End command process for loop

	// Any missing JDL files can now be added since we
	// removed any submit description references
	if (strict) {
		for (auto& [_, err] : node_jdl_dne) {
			errors.push_back(std::move(err));
		}
	}
}


bool json_printer(const std::string& file, MockDag& dag, std::vector<DagParseError>& errors) {
	ClassAd result;

	result.InsertAttr("DagFile", file);

	dag.data.getStats(result);

	bool cyclic = false;
	result.LookupBool(DAG_STAT_HAS_CYCLE, cyclic);

	if ( ! errors.empty()) {
		result.InsertAttr("NumErrors", (int)errors.size());
		classad::ExprList* err_list = new classad::ExprList();
		if ( ! err_list) {
			fprintf(stderr, "ERROR: Failed to create ClassAd Expression List (Out of memory)\n");
			exit(EXIT_ERROR);
		}

		for (const auto& err : errors) {
			ClassAd* ad = new ClassAd();
			if ( ! ad) {
				fprintf(stderr, "ERROR: Failed to create ClassAd (Out of memory)\n");
				exit(EXIT_ERROR);
			}

			auto [src, line] = err.GetSource();
			auto [cmd, known] = err.GetCommand();

			bool rejected = (cmd == DAG::CMD::REJECT && err.GetError().empty());

			ad->InsertAttr("Reason", rejected ? "DAG marked as rejected" : err.GetError());
			ad->InsertAttr("SourceFile", src);
			ad->InsertAttr("SourceLine", (long long)line);

			if (known) {
				ad->InsertAttr("DagCommand", DAG::GET_KEYWORD_STRING(cmd));
			}

			if (rejected) { result.InsertAttr("Rejected", true); }

			err_list->push_back(ad);
		}
		result.Insert("Errors", err_list);
	}

	fPrintAdAsJson(stdout, result);

	return cyclic;
}


bool stats_printer(const std::string& file, MockDag& dag, std::vector<DagParseError>& /*errors*/) {
	ClassAd stats;
	dag.data.getStats(stats);

	bool cyclic = false;
	stats.LookupBool(DAG_STAT_HAS_CYCLE, cyclic);

	printf("=== %s statistics ===\n", file.c_str());
	fPrintAd(stdout, stats);
	printf("\n");

	return cyclic;
}


bool default_printer(const std::string& file, MockDag& dag, std::vector<DagParseError>& errors) {
	ClassAd stats;
	dag.data.getStats(stats);

	bool cyclic = false;
	stats.LookupBool(DAG_STAT_HAS_CYCLE, cyclic);
	if (cyclic) {
		std::string cycle = "?????";
		stats.LookupString(DAG_STAT_DETECTED_CYCLE, cycle);
		errors.emplace_back(file, 0, "Cycle detected: " + cycle);
	}

	if ( ! errors.empty()) {
		size_t count = errors.size();
		printf("Detected %zu issue%s in %s:\n", count, (count > 1) ? "s" : "", file.c_str());

		for (const auto& err: errors) { printf("\t* %s\n", err.c_str()); }

		printf("\n");
	}

	return cyclic;
}


void usage(int code = EXIT_SUCCESS) {
	printf("Usage: condor_dag_checker [options] <DAG File> [<DAG File> ...]\n");
	printf("       Verify provided DAG file(s) will be parsed/processed\n");
	printf("       successfully, and display any issues discovered in\n");
	printf("       the provided DAG file(s).\n\n");

	printf("Options:\n");
	printf("\t-h/-help                Print Tool Usage\n\n");
	printf("\t-AllowIllegalChars      Allow node names to contain illegal characters [+]\n");
	printf("\t-CheckExternalFiles     Best effort process external files\n");
	printf("\t-CheckJDL               Best effort process node submit descriptions\n");
	printf("\t-CheckScripts           Verify scripts exist\n");
	printf("\t-CheckSubDags           Best effort process Sub-DAG files\n");
	printf("\t-json                   Print Results in JSON Format\n");
	printf("\t-[No]JoinNodes          Enable/Disable creating join nodes (Default enabled)\n");
	printf("\t-Statistics             Print statistics about parsed DAG\n");
	printf("\t-Strict                 Enable strict checking: Error if missing external files\n");
	printf("\t-UseDagDir              Change into DAG file directory prior to parsing\n");

	exit(code);
}


bool matchOption(const istring_view option, const char* check, size_t n) {
	size_t start = option.find_first_not_of("-");

	if (start == std::string_view::npos) {
		fprintf(stderr, "ERROR: Invalid option '%s' provided\n", option.data());
		usage(EXIT_ERROR);
	}

	bool match = true;

	if (option.compare(start, n, check, n) != MATCH) { // Minimum character match
		match = false;
	} else if (option.substr(start).compare(check) > MATCH) { // Passed option is less than or equal to check
		match = false;
	}

	return match;
}


enum class DagPrinter {
	DEFAULT = 0,
	JSON,
	STATISTICS,
};

void setPrinterOption(const DagPrinter printer, istring_view option) {
	static istring_view ref;
	static DagPrinter current = DagPrinter::DEFAULT;

	if (current != DagPrinter::DEFAULT && current != printer) {
		fprintf(stderr, "ERROR: Multiple print styles specified: %s and %s\n\n",
		        ref.data(), option.data());
		usage(EXIT_ERROR);
	}

	if (ref.empty()) { ref = option; }
	current = printer;
}


int main(int argc, const char** argv) {
	set_priv_initialize(); // allow uid switching if root
	config();

	std::vector<std::filesystem::path> dag_files;
	std::function<bool(const std::string&, MockDag&, std::vector<DagParseError>&)> printer = default_printer;

	bool use_dag_dir = false;
	bool allow_illegal_chars = param_boolean("DAGMAN_ALLOW_ANY_NODE_NAME_CHARACTERS", false);
	use_join_nodes = param_boolean("DAGMAN_USE_JOIN_NODES", true);
	bool print_json = false;

	// Process command line arugments
	for (int i = 1; i < argc; i++) {
		istring_view option(argv[i]);
		if (matchOption(option, "help", 1)) {
			usage();
		} else if (matchOption(option, "UseDagDir", 3)) {
			use_dag_dir = true;
		} else if (matchOption(option, "AllowIllegalChars", 5)) {
			allow_illegal_chars = true;
		} else if (matchOption(option, "NoJoinNodes", 6)) {
			use_join_nodes = false;
		} else if (matchOption(option, "JoinNodes", 4)) {
			use_join_nodes = true;
		} else if (matchOption(option, "JSON", 4)) {
			setPrinterOption(DagPrinter::JSON, option);
			print_json = true;
			printer = json_printer;
		} else if (matchOption(option, "statistics", 4)) {
			setPrinterOption(DagPrinter::STATISTICS, option);
			printer = stats_printer;
		} else if (matchOption(option, "CheckExternalFiles", 8)) {
			check_external |= CHECK_EXTERNAL;
		} else if (matchOption(option, "CheckSubDags", 8)) {
			check_external |= CHECK_SUB_DAGS;
		} else if (matchOption(option, "CheckJDL", 8)) {
			check_external |= CHECK_JDL;
		} else if (matchOption(option, "CheckScripts", 8)) {
			check_external |= CHECK_SCRIPTS;
		} else if (matchOption(option, "Strict", 6)) {
			strict = true;
		} else if (option.starts_with("-")) {
			fprintf(stderr, "ERROR: Unknown command line option '%s'\n", option.data());
			usage(EXIT_ERROR);
		} else {
			dag_files.emplace_back(option.data());
		}
	}

	if (dag_files.empty()) {
		fprintf(stderr, "ERROR: No DAG file(s) provided for linting.\n");
		usage(EXIT_ERROR);
	}

	int exit_code = EXIT_SUCCESS;

	if (print_json) { printf("[\n"); }

	size_t i = 0;
	for (const auto& file : dag_files) {
		DagParser parser(file);
		std::vector<DagParseError> errors;

		MockDag dag;

		// Dag<D, N>/EdgeTable (edge.h, dag.hpp) throw rather than abort on an
		// internal error -- catch each known type here so a bug in one DAG file's
		// graph construction can't take down the whole tool run uncaught.
		try {
			if (parser.failed()) {
				exit_code = EXIT_FAILURE;
				errors.emplace_back(
				       file.string(), // Failed source file
				       0,             // Never parsed anything so set error line to 0
				       "Failed to open DAG " + file.filename().string() + ": " + parser.error()
				);
			} else {
				parser.AllowIllegalChars(allow_illegal_chars)
				      .ContOnParseFailure();

				ChangeDir cd(file.parent_path(), use_dag_dir);
				if (cd.failed()) {
					errors.emplace_back(
					       file.string(), // Failed source file
					       0,             // Never parsed anything so set error line to 0
					       "Failed to change directories: " + cd.error()
					);
				} else {
					included_files.clear();
					included_files.insert(parser.GetFile());

					parseDAG(parser, dag, errors);
				}
			}

			if ( ! errors.empty()) {
				exit_code = EXIT_FAILURE;
			}

			if (printer(file.string(), dag, errors)) {
				// Cycle was detected while printing output
				exit_code = EXIT_FAILURE;
			}
		} catch (const std::out_of_range& e) {
			fprintf(stderr, "ERROR: Invalid graph reference while processing '%s': %s\n", file.string().c_str(), e.what());
			exit_code = EXIT_ERROR;
		} catch (const std::logic_error& e) {
			fprintf(stderr, "ERROR: Internal graph invariant violation while processing '%s': %s\n", file.string().c_str(), e.what());
			exit_code = EXIT_ERROR;
		} catch (const std::exception& e) {
			fprintf(stderr, "ERROR: Unexpected error while processing '%s': %s\n", file.string().c_str(), e.what());
			exit_code = EXIT_ERROR;
		}

		if (print_json) {
			if (++i < dag_files.size()) { printf(","); }
			printf("\n");
		}
	}

	if (print_json) { printf("]\n"); }

	return exit_code;
}

