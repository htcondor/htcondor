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


#ifndef PARSE_H
#define PARSE_H

#include "vector"
#include "string"

#include "dag.h"
#include "config.hpp"
#include "dagman_main.h"
#include "dag_commands.h"
#include "stl_string_utils.h"

/**
 * Set whether we should munge the node names (only applies to multi-DAG
 * runs).
 * @param Whether to munge the node names.
 */
void parseSetDoNameMunge(bool doit);

/**
 * Parse a DAG file.
 * @param The Dag object we'll be adding nodes to.
 * @param The name of the DAG file.
 * @param Run DAGs in directories from DAG file paths if true
 * @param Whether to increment the DAG number (should be true for
 *     "normal" DAG files (on the command line), false for splices
 *     and includes)
 */
bool parse(const Dagman& dm, Dag *dag, const char * filename, bool incrementDagNum = true);

/**
 * Determine whether the given token is a DAGMan reserved word.
 * @param The token we're testing.
 * @return True iff the token is a reserved word.
 */
bool isReservedWord( const char *token );
//void DFSVisit (Job * job);

class DagProcessor {
public:
	DagProcessor() = delete;
	DagProcessor(const Dagman& dm) : config(dm.config), useDagDir(dm.options[DagmanDeepOptions::b::UseDagDir]) {}

	// Note: dag_munge_id is the number to use for name munging. Negative #'s = no munge
	bool process(const Dagman& dm, Dag& dag, const std::string& file, int dag_munge_id = -1);
private:
	bool ProcessCommand(const Dagman& dm, const DagCmd& cmd, Dag& dag, int dag_munge_id);
	bool ProcessNode(const NodeCommand* cmd, Dag& dag, int dag_munge_id);
	bool ProcessSplice(const Dagman& dm, Dag& dag, const SpliceCommand* cmd, int dag_munge_id);
	bool ProcessCategory(const CategoryCommand* cat, Dag& dag, int dag_munge_id);
	bool ProcessDependencies(const ParentChildCommand* cmd, Dag& dag, int dag_munge_id);
	bool ProcessPreSkip(const PreSkipCommand* skip, Dag& dag, int dag_munge_id);
	bool ProcessPriority(const PriorityCommand* prio, Dag& dag, int dag_munge_id);
	bool ProcessVars(const VarsCommand* vars, Dag& dag, int dag_munge_id);
	bool ProcessDone(const DoneCommand* cmd, Dag& dag, int dag_munge_id);
	bool ProcessSaveFile(const SavePointCommand* sp, Dag& dag, int dag_munge_id);
	bool ProcessAbortDagOn(const AbortDagCommand* ado, Dag& dag, int dag_munge_id);
	bool ProcessRetry(const RetryCommand* retry, Dag& dag, int dag_munge_id);
	bool ProcessScript(const ScriptCommand* cmd, Dag& dag, int dag_munge_id);

	// Note: Copy input variable as to not muck up original variable
	std::string MakeFullName(std::string name, int dag_munge_id) {
		static const istring_view all_nodes_check(DAG::ALL_NODES.c_str());
		if (name.c_str() == all_nodes_check) {
			return name;
		}

		if (config[DagmanConfigOptions::b::MungeNodeNames] && dag_munge_id >= 0) {
			name = std::to_string(dag_munge_id) + "." + name;
		}

		return name;
	}

	const DagmanConfig& config;
	std::set<std::string> parsed_file_check{}; // Used to make sure file parse recursion is explicity checked
	bool useDagDir{false};

	static size_t join_node_id;
};


#endif

