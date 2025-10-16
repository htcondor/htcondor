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

#include <set>
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


struct MockDag {
	MockDag(bool join_nodes) {
		use_join_nodes = join_nodes;
	}

	struct Node {
		Node(const std::string& n, const uint32_t i, DAG::CMD t) {
			name = n;
			num = i;
			type = t;
		}

		std::string name{};
		std::set<uint32_t> children{};
		uint32_t num{0};
		DAG::CMD type{DAG::CMD::JOB};
		bool has_parent{false};
		bool visited{false};
	};

	bool hasSplice(const std::string& name) const {
		return std::ranges::find(splices, name, &MockDag::scope) != splices.end();
	}

	bool hasNode(const std::string& name) const {
		return node_name_hash.contains(name);
	}

	bool contains(const std::string& name) const {
		return hasNode(name) || hasSplice(name);
	}

	void makeDependencies(const ParentChildCommand* pc, std::vector<DagParseError>& errors) {
		std::vector<uint32_t> parents;
		std::vector<uint32_t> children;

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
		}

		if (use_join_nodes && parents.size() > 1 && children.size() > 1) {
			std::string join_node_name = "_condor_join_node" + std::to_string(++join_node_index);
			auto& join_node = nodes.emplace_back(join_node_name, node_index++, DAG::CMD::JOB);
			for (const auto num : parents) {
				nodes[num - node_index_offset].children.insert(join_node.num);
				join_node.has_parent = true;
			}

			parents.clear();
			parents.push_back(join_node.num);
		}

		for (const auto p_num : parents) {
			auto& parent = nodes[p_num - node_index_offset];
			for (const auto c_num : children) {
				parent.children.insert(c_num);
				nodes[c_num - node_index_offset].has_parent = true;
			}
		}
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

		auto& node = nodes.emplace_back(name, node_index++, t);
		node_name_hash[name] = node.num;

		if (t == DAG::CMD::FINAL) {
			final_node = name;
		} else if (t == DAG::CMD::PROVISIONER) {
			provisioner = name;
		}

		return "";
	}

	void inheritSpliceNodes() {
		for (auto& splice : splices) {
			++num_splices += splice.num_splices;
			for (auto& node : splice.nodes) {
				std::string name = splice.scope + "+" + node.name;
				node_name_hash[name] = node.num;
				node.name = name;
				nodes.push_back(node);
			}
			splice.nodes.clear();
		}
	}

	MockDag* addSplice(const std::string& name, std::string& error) {
		if (contains(name)) {
			error = "Splice name " + name + " is already defined in the DAG";
			return nullptr;
		}

		auto& splice = splices.emplace_back(use_join_nodes);
		splice.node_index_offset = node_index;

		return &splice;
	}

	void getStats(ClassAd& stats) {
		std::map<std::string, uint32_t> node_counts;
		size_t num_arcs = 0;
		bool has_graph_nodes = false;

		for (const auto& node : nodes) {
			switch (node.type) {
				case DAG::CMD::JOB:
					has_graph_nodes = true;
					if (node.name.starts_with("_condor_join_node"))
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
			num_arcs += node.children.size();
		}

		stats.InsertAttr(DAG_STAT_TOTAL_NODES, (int)nodes.size());

		for (const auto& [key, count]: node_counts) {
			stats.InsertAttr(key, (int)count);
		}

		if (num_arcs > 0) { stats.InsertAttr(DAG_STAT_NUM_ARCS, (int)num_arcs); }
		if (num_splices > 0) { stats.InsertAttr(DAG_STAT_NUM_SPLICES, (int)num_splices); }

		if (hasFinal()) {
			stats.InsertAttr(DAG_STAT_FINAL_NODE, final_node);
		}

		if (hasProvisioner()) {
			stats.InsertAttr(DAG_STAT_PROVISIONER_NODE, provisioner);
		}

		if (has_graph_nodes) {
			uint32_t height = 1;
			std::vector<uint32_t> widths;
			std::vector<uint32_t> ancestors;
			bool cycle = false;
			bool found_start_nodes = false;

			widths.emplace_back(0);

			for (auto& node : nodes) {
				switch (node.type) {
					case DAG::CMD::FINAL:
					case DAG::CMD::SERVICE:
					case DAG::CMD::PROVISIONER:
						node.visited = true;
						continue;
						break;
					default:
						break;
				}

				if ( ! node.has_parent) {
					found_start_nodes = true;
					if (walkDFS(node, 0, height, widths, ancestors)) {
						cycle = true;
						break;
					}
				}
			}

			std::erase_if(widths, [](uint32_t n) { return n == 0; });

			std::string not_visited;
			std::ranges::for_each(nodes, [&not_visited](const auto& node){
				if ( ! node.visited) {
					not_visited += (not_visited.empty() ? "" : ",") + node.name;
				}
			});

			if ( ! found_start_nodes || ! not_visited.empty()) { cycle = true; }

			if (cycle) {
				stats.InsertAttr(DAG_STAT_HAS_CYCLE, true);
				std::string cycle_msg = "No initial nodes discovered";

				if (found_start_nodes) {
					if ( ! ancestors.empty()) {
						cycle_msg.clear();
						for (auto num : ancestors) {
							cycle_msg += (cycle_msg.empty() ? "" : " -> ") + nodes[num - node_index_offset].name;
						}
					} else if ( ! not_visited.empty()) {
						cycle_msg = "Failed to visit nodes " + not_visited;
					}
				}

				stats.InsertAttr(DAG_STAT_DETECTED_CYCLE, cycle_msg);
			} else if ( ! widths.empty()) {
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

	inline bool hasFinal() const { return ! final_node.empty(); }
	inline bool hasProvisioner() const { return ! provisioner.empty(); }

	std::map<std::string, uint32_t> node_name_hash{};
	std::vector<Node> nodes{};
	std::vector<MockDag> splices{};
	std::string scope{};

	std::string final_node{};
	std::string provisioner{};

	size_t num_splices{0};
	uint32_t node_index_offset{0};

	bool use_join_nodes{true};

	static uint32_t node_index;
	static uint32_t join_node_index;

private:
	bool walkDFS(Node& node, size_t depth, uint32_t& height, std::vector<uint32_t>& widths, std::vector<uint32_t>& ancestors) {
		if (node.visited) {
			bool cycle = false;
			if (std::ranges::find(ancestors, node.num) != ancestors.end()) {
				ancestors.emplace_back(node.num);
				cycle = true;
			}
			return cycle;
		}

		node.visited = true;

		widths[depth]++;
		height = MAX(depth + 1, height);

		if ( ! node.children.empty()) {
			if (widths.size() - 1 <= depth) { widths.emplace_back(0); }

			ancestors.emplace_back(node.num);

			for (const auto num : node.children) {
				if (walkDFS(nodes[num - node_index_offset], depth + 1, height, widths, ancestors)) { return true; }
			}

			ancestors.pop_back();
		}

		return false;
	}

	void considerDependency(const ParentChildCommand* pc, const std::string& name, const bool is_parent, std::vector<uint32_t>& list, std::string& missing, std::vector<DagParseError>& errors) {
		static std::set<DAG::CMD> invalid = {
			DAG::CMD::FINAL,
			DAG::CMD::SERVICE,
			DAG::CMD::PROVISIONER,
		};

		if (hasNode(name)) {
			auto& node = nodes[node_name_hash[name] - node_index_offset];
			if (invalid.contains(node.type)) {
				std::string type(DAG::GET_KEYWORD_STRING(node.type));
				addCommandError(pc, type + " node " + name + " can not have dependencies", errors);
			} else {
				list.push_back(node.num);
			}
		} else if (hasSplice(name)) {
			auto it = std::ranges::find(splices, name, &MockDag::scope);
			std::string prefix = it->scope + "+";
			for (auto& node : nodes) {
				bool is_splice_end = is_parent ? node.children.empty() : ( ! node.has_parent);
				if (node.name.starts_with(prefix) && is_splice_end) {
					if (invalid.contains(node.type)) {
						std::string type(DAG::GET_KEYWORD_STRING(node.type));
						addCommandError(pc, type + " node " + name + " can not have dependencies", errors);
					} else {
						list.push_back(node.num);
					}
				}
			}
		} else {
			if ( ! missing.empty()) { missing += ","; }
			missing += name;
		}
	}
};


uint32_t MockDag::node_index = 0;
uint32_t MockDag::join_node_index = 0;
static std::set<std::string> included_files;


void parseDAG(DagParser& parser, MockDag& dag, std::vector<DagParseError>& errors) {
	static istring_view all_nodes_keyword(DAG::ALL_NODES.c_str());

	std::vector<DagCmd> commands{};

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
						const NodeCommand* node = (NodeCommand*)(cmd.get());

						std::string err = dag.addNode(node->GetName(), cmd_val);
						if ( ! err.empty()) {
							addCommandError(node, err, errors);
						}
					}
					break;
				case DAG::CMD::SPLICE:
					{
						const SpliceCommand* splice = (SpliceCommand*)(cmd.get());

						std::string err;
						MockDag* splice_dag = dag.addSplice(splice->GetName(), err);
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

							splice_dag->scope = splice->GetName();

							included_files.insert(file);
							parseDAG(sp, *splice_dag, errors);
							included_files.erase(file);
						}
					}
					break;
				case DAG::CMD::INCLUDE:
					{
						const IncludeCommand* include = (IncludeCommand*)(cmd.get());
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

	dag.inheritSpliceNodes();

	std::ranges::sort(commands, [](DagCmd& l, DagCmd& r) { return l->GetCommand() < r->GetCommand(); });
	std::ranges::sort(dag.nodes, {}, &MockDag::Node::num);

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
			case DAG::CMD::DONE:
				{
					const NodeModifierCommand* mod = (NodeModifierCommand*)(cmd.get());
					const std::string node_name = mod->GetNodeName();

					std::string error;

					if (node_name.c_str() == all_nodes_keyword) {
						// Allow ALL_NODES (case insensitive)
						if (cmd_val == DAG::CMD::DONE) { error = "ALL_NODES can not be used with DONE command"; }
					} else if (dag.hasSplice(node_name)) {
						error = "Cannot be applied to splice " + node_name;
					} else if ( ! dag.hasNode(node_name)) {
						error = "References undefined node " + node_name;
					}

					if ( ! error.empty()) {
						addCommandError(mod, error, errors);
					}
				}
				break;
			case DAG::CMD::PARENT_CHILD:
				dag.makeDependencies((ParentChildCommand*)(cmd.get()), errors);
				break;
			case DAG::CMD::CATEGORY:
				{
					const CategoryCommand* cat = (CategoryCommand*)(cmd.get());
					std::string missing;

					for (const auto& node : cat->GetNodes()) {
						if ( ! dag.hasNode(node.data()) && node.data() != all_nodes_keyword) {
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
					const ConfigCommand* config = (ConfigCommand*)(cmd.get());
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
			default:
				break;
		} // End switch(cmd)
	} // End command process for loop
}


bool json_printer(const std::string& file, MockDag& dag, std::vector<DagParseError>& errors) {
	ClassAd result;

	result.InsertAttr("DagFile", file);

	dag.getStats(result);

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
	dag.getStats(stats);

	bool cyclic = false;
	stats.LookupBool(DAG_STAT_HAS_CYCLE, cyclic);

	printf("=== %s statistics ===\n", file.c_str());
	fPrintAd(stdout, stats);
	printf("\n");

	return cyclic;
}


bool default_printer(const std::string& file, MockDag& dag, std::vector<DagParseError>& errors) {
	ClassAd stats;
	dag.getStats(stats);

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
	printf("\t-json                   Print Results in JSON Format\n");
	printf("\t-[No]JoinNodes          Enable/Disable creating join nodes (Default enabled)\n");
	printf("\t-Statistics             Print statistics about parsed DAG\n");
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
	bool use_join_nodes = param_boolean("DAGMAN_USE_JOIN_NODES", true);
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

		MockDag dag(use_join_nodes);
		dag.node_index = 0;
		dag.join_node_index = 0;

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

		if (print_json) {
			if (++i < dag_files.size()) { printf(","); }
			printf("\n");
		}
	}

	if (print_json) { printf("]\n"); }

	return exit_code;
}

