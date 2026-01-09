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

#ifndef DAG_COMMANDS_H
#define DAG_COMMANDS_H

#include <set>
#include <map>
#include <algorithm>
#include <string>
#include <memory>
#include <limits>
#include <assert.h>

#include "stl_string_utils.h"

#ifdef _MSC_VER
	#define strncasecmp _strnicmp
	#define strcasecmp _stricmp
#endif

//Class to make the option map key comparison case insensitive
struct NoCaseCmp {
	bool operator()(const std::string& left, const std::string& right) const noexcept {
		return strcasecmp(left.c_str(), right.c_str()) < 0;
	}
};

namespace DAG {
	// Enum of all DAG commands.
	// Note: Ordered in desired processing order (Lower # is better i.e top->bottom)
	enum class CMD {
		SUBMIT_DESCRIPTION = 0,
		JOB,
		FINAL,
		PROVISIONER,
		SERVICE,
		SUBDAG,
		SPLICE,
		CATEGORY,
		PARENT_CHILD,
		SCRIPT,
		RETRY,
		ABORT_DAG_ON,
		VARS,
		PRIORITY,
		PRE_SKIP,
		DONE,
		MAXJOBS,
		CONFIG,
		INCLUDE,
		DOT,
		NODE_STATUS_FILE,
		JOBSTATE_LOG,
		SAVE_POINT_FILE,
		SET_JOB_ATTR,
		ENV,
		REJECT,
		PIN_IN,
		PIN_OUT,
		CONNECT,
	};

	enum class SCRIPT {
		PRE,
		POST,
		HOLD,
	};

	enum class ScriptOutput {
		NONE = 0,
		STDOUT,
		STDERR,
		ALL,
	};

	enum class VarsPlacement {
		DEFAULT = -1,
		PREPEND,
		APPEND,
	};

	// Quick map of keyword strings to enum value
	extern const std::map<std::string, CMD, NoCaseCmp> KEYWORD_MAP;
	// Map of DAG Command to exampe syntax
	extern const std::map<CMD, const char*> COMMAND_SYNTAX;

	// Convert a DagCmd unique_ptr (see below) to specified derived class: DAG::DERIVE_CMD<JobCommand>(cmd)
	// Typename D is the Derived Command Type and Typename B is the base command Type (normally implicitly disccovered)
	template<typename D, typename B>
	D* DERIVE_CMD(const std::unique_ptr<B>& cmd) {
		D* derived = dynamic_cast<D*>(cmd.get());
		ASSERT(derived);
		return derived;
	}

	// DAG file constants
	extern const std::string ALL_NODES;
	// Reserved words that node names cannot use
	extern const std::set<std::string, NoCaseCmp> RESERVED;

	// Map of Script type string to script type enum
	extern const std::map<std::string, SCRIPT, NoCaseCmp> SCRIPT_TYPES_MAP;
	// Map of Script debug output stream capture string to enum
	extern const std::map<std::string, ScriptOutput, NoCaseCmp> SCRIPT_DEBUG_MAP;

	// Character to replace newlines (\n) in inline submit descriptions for display
	extern const char NEWLINE_RELACEMENT;

	// Get DAG Command Keyword from command
	extern const char* GET_KEYWORD_STRING(const CMD command);
	// Get Script type string from script type enum
	extern const char* GET_SCRIPT_TYPE_STRING(const SCRIPT type);
	// Get Script debug capture stream type string from enum
	extern const char* GET_SCRIPT_DEBUG_CAPTURE_TYPE(const ScriptOutput type);

	namespace STRING_SPACE {
		// Map of string to # references
		extern std::map<std::string, int> __string_space_map;

		// Deduplicate string from string space
		extern std::string_view DEDUP(const std::string_view str);
		extern std::string_view __DEDUP(const std::string_view str, bool internal = true);

		// Release reference to string in map (remove entry if no more references)
		extern void FREE(const std::string_view str);

		// Do general cleanup of string space map (remove non-referenced keys)
		// Note: This includes parse command references that haven't been handed off
		extern void GARBAGE_COLLECT();
	}

}

// Abstract base class for all derived DAG Commands
class BaseDagCommand {
public:
	// Rule of 5
	BaseDagCommand() = default;
	BaseDagCommand(const BaseDagCommand&) = default;
	BaseDagCommand(BaseDagCommand&&) = default;
	BaseDagCommand& operator=(const BaseDagCommand&) = default;
	BaseDagCommand& operator=(BaseDagCommand&&) = default;
	virtual ~BaseDagCommand() = default;

	// Print DAG command information
	virtual void PrintInfo() const {
		size_t len_pad = strlen(GetCommandStr()) >= 20 ? 0 : (20 - strlen(GetCommandStr()));
		std::string padding(len_pad, ' ');
		printf("[%02d] %s%s\n", (int)GetCommand(), padding.c_str(), GetDetails().c_str());
	}

	virtual std::string GetDetails() const {
		std::string details;
		formatstr(details, "%s > %s", GetCommandStr(), _getDetails().c_str());
		return details;
	}

	virtual const char* GetCommandStr() const { return DAG::GET_KEYWORD_STRING(GetCommand()); }

	// Get the commands enum value
	virtual DAG::CMD GetCommand() const = 0;
	virtual std::string _getDetails() const = 0;

	virtual std::string Location() const {
		return ToStr(source) + ":" + std::to_string(line);
	}

	virtual void SetSource(const std::string& src, const uint64_t line_no) {
		source = DAG::STRING_SPACE::__DEDUP(src);
		line = line_no;
	}

	virtual std::pair<std::string, uint64_t> GetSource() const {
		return std::make_pair(ToStr(source), line);
	}

protected:
	virtual std::string ToStr(const std::string_view& v) const { return v.data() ? v.data() : ""; }
	virtual const char* Disp(const std::string_view& v) const { return v.data() ? v.data() : ""; }

	std::string_view source{"Unknown"};
	uint64_t line{0};
};

// Create custom type to represent a unique pointer to BaseDagCommand
using DagCmd = std::unique_ptr<BaseDagCommand>;

// Abstract command for all node types
class NodeCommand : public BaseDagCommand {
public:
	NodeCommand() = default;

	virtual DAG::CMD GetCommand() const = 0;
	virtual std::string _getDetails() const {
		std::string ret;
		std::string desc = inline_desc.empty() ? "NONE" : ToStr(inline_desc);
		std::replace(desc.begin(), desc.end(), '\n', DAG::NEWLINE_RELACEMENT);
		formatstr(ret, "%s %s {%s} %s %s %s", Disp(name), Disp(submit),
		          desc.c_str(), Disp(dir), noop ? "T" : "F", done ? "T" : "F");
		return ret;
	}

	void SetDir(const std::string& s) { dir = DAG::STRING_SPACE::__DEDUP(s); }
	std::string GetDir() const { return ToStr(dir); }
	bool HasDir() const { return !dir.empty(); }

	std::string GetName() const { return ToStr(name); }

	void SetSubmit(const std::string& s) { submit = DAG::STRING_SPACE::__DEDUP(s); }
	std::string GetSubmit() const { return ToStr(submit); }

	void SetInlineDesc(const std::string& s) { inline_desc = DAG::STRING_SPACE::__DEDUP(s); }
	std::string GetInlineDesc() const { return ToStr(inline_desc); }
	bool HasInlineDesc() const { return !inline_desc.empty(); }

	void SetNoop() { noop = true; }
	bool IsNoop() const { return noop; }

	void SetDone() { done = true; }
	bool IsDone() const { return done; }
protected:
	std::string_view name{}; // Name of node (Note: No guarantee that this is unique during parse time)
	std::string_view submit{}; // Name of submit file/description or sub-DAG
	std::string_view inline_desc{}; // Inline description contents
	std::string_view dir{}; // Execution directory
	bool noop{false}; // Specify no-operation node
	bool done{false}; // Specify pre-done node
};

// JOB Command
class JobCommand : public NodeCommand {
public:
	JobCommand() = delete;
	JobCommand(const std::string& n) { name = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::JOB; }
};

// FINAL Command
class FinalCommand : public NodeCommand {
public:
	FinalCommand() = delete;
	FinalCommand(const std::string& n) { name = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::FINAL; }
};

// SERVICE Command
class ServiceCommand : public NodeCommand {
public:
	ServiceCommand() = delete;
	ServiceCommand(const std::string& n) { name = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::SERVICE; }
};

// PROVISIONER Command
class ProvisionerCommand : public NodeCommand {
public:
	ProvisionerCommand() = delete;
	ProvisionerCommand(const std::string& n) { name = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::PROVISIONER; }
};

// SUBDAG Command
class SubdagCommand : public NodeCommand {
public:
	SubdagCommand() = delete;
	SubdagCommand(const std::string& n) { name = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::SUBDAG; }
};

// SUBMIT-DESCRIPTION Command (Shared inline job submit description)
class SubmitDescCommand : public BaseDagCommand {
public:
	SubmitDescCommand() = delete;
	SubmitDescCommand(const std::string& n) : name(DAG::STRING_SPACE::__DEDUP(n)) {}

	virtual DAG::CMD GetCommand() const { return DAG::CMD::SUBMIT_DESCRIPTION; }
	virtual std::string _getDetails() const {
		std::string ret;
		std::string desc = ToStr(inline_desc);
		std::replace(desc.begin(), desc.end(), '\n', DAG::NEWLINE_RELACEMENT);
		formatstr(ret, "%s {%s}", Disp(name), desc.c_str());
		return ret;
	}

	std::string GetName() const { return ToStr(name); }

	void SetInlineDesc(const std::string& s) { inline_desc = DAG::STRING_SPACE::__DEDUP(s); }
	std::string GetInlineDesc() const { return ToStr(inline_desc); }
	bool HasInlineDesc() const { return !inline_desc.empty(); }

private:
	std::string_view name{}; // Name to refer to inline description
	std::string_view inline_desc{}; // Inline description contents
};

// SPLICE Command
class SpliceCommand : public BaseDagCommand {
public:
	SpliceCommand() = delete;
	SpliceCommand(const std::string& n) : name(DAG::STRING_SPACE::__DEDUP(n)) {}

	virtual DAG::CMD GetCommand() const { return DAG::CMD::SPLICE; }
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %s %s", Disp(name), Disp(file), Disp(dir));
		return ret;
	}

	std::string GetName() const { return ToStr(name); }

	void SetDir(const std::string& s) { dir = DAG::STRING_SPACE::__DEDUP(s); }
	std::string GetDir() const { return ToStr(dir); }
	bool HasDir() const { return !dir.empty(); }

	void SetDagFile(const std::string& f) { file = DAG::STRING_SPACE::__DEDUP(f); }
	std::string GetDagFile() const { return ToStr(file); }
private:
	std::string_view name{}; // Splice name for references
	std::string_view file{}; // DAG file to splice into DAG
	std::string_view dir{}; // Directory to do splicing from
};

// PARENT(CHILD) Command
class ParentChildCommand : public BaseDagCommand {
public:
	ParentChildCommand() = default;

	virtual DAG::CMD GetCommand() const { return DAG::CMD::PARENT_CHILD; }
	virtual std::string _getDetails() const {
		std::string ret = "[ ";
		for (const auto& p : parents) { ret += ToStr(p) + " "; }
		ret += "] --> [ ";
		for (const auto& c : children) { ret += ToStr(c) + " "; }
		return ret + "]";
	}

	void AddParent(const std::string& p) {
		parents.insert(DAG::STRING_SPACE::__DEDUP(p));
	}

	void AddChild(const std::string& c) {
		children.insert(DAG::STRING_SPACE::__DEDUP(c));
	}

	const std::set<std::string_view>& GetParents() const { return parents; }
	const std::set<std::string_view>& GetChildren() const { return children; }

private:
	std::set<std::string_view> parents{}; // List of parent nodes
	std::set<std::string_view> children{}; // List of child nodes
};

// Abstract class to modify some behavior of a node type
class NodeModifierCommand : public BaseDagCommand {
public:
	NodeModifierCommand() = default;

	virtual DAG::CMD GetCommand() const = 0;
	virtual std::string _getDetails() const = 0;

	virtual std::string GetNodeName() const { return ToStr(node); };
protected:
	std::string_view node{}; // Node name to apply command
};

// SCRIPT Command
class ScriptCommand : public NodeModifierCommand {
public:
	ScriptCommand() = default;

	virtual DAG::CMD GetCommand() const { return DAG::CMD::SCRIPT; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %s '%s' %lld %d %s %s", Disp(node), DAG::GET_SCRIPT_TYPE_STRING(type),
		          Disp(script), (long long)defer_time, defer_status, Disp(debug_file),
		          DAG::GET_SCRIPT_DEBUG_CAPTURE_TYPE(capture));
		return ret;
	}

	void SetNodeName(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	std::string GetScript() const { return ToStr(script); }
	void SetScript(const std::string& s) { script = DAG::STRING_SPACE::__DEDUP(s); }

	DAG::SCRIPT GetType() const { return type; }
	void SetType(const DAG::SCRIPT t) { type = t; }
	const char* GetTypeStr() const { return DAG::GET_SCRIPT_TYPE_STRING(type); }

	bool HasDeferal() const { return defer_status != -1; }
	void SetDeferal(const int status, const time_t t) {
		defer_status = status;
		defer_time = t;
	}
	std::tuple<int, time_t> GetDeferal() const { return std::make_tuple(defer_status, defer_time); }

	bool WantsDebug() const { return !debug_file.empty(); }
	void SetDebugInfo(const std::string& f, DAG::ScriptOutput c) {
		debug_file = DAG::STRING_SPACE::__DEDUP(f);
		capture = c;
	}
	std::tuple<std::string, DAG::ScriptOutput> GetDebugInfo() const { return std::make_tuple(ToStr(debug_file), capture); }
private:
	std::string_view script{}; // Script + args to execute
	std::string_view debug_file{}; // Debug file to write script stdout/stderr to
	time_t defer_time{0}; // Deferal time
	int defer_status{-1}; // Exit code to trigger deferal
	DAG::SCRIPT type{}; // Script type
	DAG::ScriptOutput capture{DAG::ScriptOutput::NONE}; // Output streams to capture for debug file
};

// RETRY Command
class RetryCommand : public NodeModifierCommand {
public:
	RetryCommand() = delete;
	RetryCommand(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::RETRY; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %d %d", Disp(node), max, code);
		return ret;
	}

	void SetMaxRetries(const int num) { max = num; }
	int GetMaxRetries() const { return max; }

	void SetBreakCode(const int c) { code = c; }
	int GetBreakCode() const { return code; }
private:
	int max{0}; // Max retry attempts
	int code{0}; // Unless exit code to prevent retry
};

// ABORT_DAG_ON Command
class AbortDagCommand : public NodeModifierCommand {
public:
	AbortDagCommand() = delete;
	AbortDagCommand(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::ABORT_DAG_ON; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %d %d", Disp(node), status, code);
		return ret;
	}

	void SetCondition(const int s) { status = s; }
	int GetCondition() const { return status; }

	void SetExitValue(const int c) { code = c; }
	int GetExitValue() const { return code; }
private:
	int status{0}; // Exit code to trigger abort
	int code{std::numeric_limits<int>::max()}; // Code DAGMan should exit with
};

// VARS Command
class VarsCommand : public NodeModifierCommand {
public:
	VarsCommand() = delete;
	VarsCommand(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::VARS; };
	virtual std::string _getDetails() const {
		std::string ret = ToStr(node);
		if (when == DAG::VarsPlacement::PREPEND) { ret += " PREPEND"; }
		else if (when == DAG::VarsPlacement::APPEND) { ret += " APPEND"; }

		for (const auto& [k,v] : kv_pairs) { formatstr_cat(ret, " [%s=%s]", Disp(k), Disp(v)); }
		return ret;
	}

	void AddPair(const std::string& k, const std::string& v) {
		kv_pairs[DAG::STRING_SPACE::__DEDUP(k)] = DAG::STRING_SPACE::__DEDUP(v);
	}
	const std::map<std::string_view, std::string_view>& GetPairs() const { return kv_pairs; }

	void Prepend() { when = DAG::VarsPlacement::PREPEND; }
	void Append() { when = DAG::VarsPlacement::APPEND; }
	DAG::VarsPlacement GetPlacement() const { return when; }

	bool ExplicitPlacement() const { return when != DAG::VarsPlacement::DEFAULT; }
	bool WantPrepend() const { return when == DAG::VarsPlacement::PREPEND; }
private:
	std::map<std::string_view, std::string_view> kv_pairs{}; // Map of key -> value pairs to add
	DAG::VarsPlacement when{DAG::VarsPlacement::DEFAULT}; // Specify when to add info (before/after parsing submit description)
};

// PRIORITY Command
class PriorityCommand : public NodeModifierCommand {
public:
	PriorityCommand() = delete;
	PriorityCommand(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::PRIORITY; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %d", Disp(node), prio);
		return ret;
	}

	void SetPriority(const int p) { prio = p; }
	int GetPriority() const { return prio; }
private:
	int prio{0}; // Node priority
};

// PRE_SKIP Command (Skip job(s) and post script execution)
class PreSkipCommand : public NodeModifierCommand {
public:
	PreSkipCommand() = delete;
	PreSkipCommand(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::PRE_SKIP; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %d", Disp(node), code);
		return ret;
	}

	void SetExitCode(const int c) { code = c; }
	int GetExitCode() const { return code; }
private:
	int code{0}; // PRE Script exit code that triggers skip
};

// DONE Command
class DoneCommand : public NodeModifierCommand {
public:
	DoneCommand() = delete;
	DoneCommand(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::DONE; };
	virtual std::string _getDetails() const { return ToStr(node); }
};

// SAVE_POINT_FILE Command
class SavePointCommand : public NodeModifierCommand {
public:
	SavePointCommand() = delete;
	SavePointCommand(const std::string& n) { node = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::SAVE_POINT_FILE; };
	virtual std::string _getDetails() const { return ToStr(node) + " " + ToStr(file); }

	void SetFilename(const std::string& f) { file = DAG::STRING_SPACE::__DEDUP(f); }
	std::string GetFilename() const { return ToStr(file); }
private:
	std::string_view file{}; // File to save state
};

// CATEGORY Command
class CategoryCommand : public BaseDagCommand {
public:
	CategoryCommand() = delete;
	CategoryCommand(const std::string& n) { name = DAG::STRING_SPACE::__DEDUP(n); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::CATEGORY; };
	virtual std::string _getDetails() const {
		std::string ret = ToStr(name);
		for (const auto& n : nodes) { ret += " " + ToStr(n); }
		return ret;
	}

	std::string GetCategory() const { return ToStr(name); }

	void AddNode(const std::string& n) { nodes.emplace_back(DAG::STRING_SPACE::__DEDUP(n)); }
	const std::vector<std::string_view>& GetNodes() const { return nodes; }

private:
	std::vector<std::string_view> nodes{}; // List of nodes to add to category
	std::string_view name{}; // Category name
};

// MAXJOBS Command
class MaxJobsCommand : public BaseDagCommand {
public:
	MaxJobsCommand() = delete;
	MaxJobsCommand(const std::string& cat) : category(DAG::STRING_SPACE::__DEDUP(cat)) {}

	virtual DAG::CMD GetCommand() const { return DAG::CMD::MAXJOBS; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %d", Disp(category), limit);
		return ret;
	}

	std::string GetCategory() const { return ToStr(category); }

	void SetLimit(const int m) { limit = m; }
	int GetLimit() const { return limit; }
private:
	std::string_view category{}; // Category to apply throttle
	int limit{1}; // Maximum number of nodes with placed jobs
};

// Abstract class to handle an external file used by DAGMan
class FileCommand : public BaseDagCommand {
public:
	FileCommand() = default;

	virtual DAG::CMD GetCommand() const = 0;
	virtual std::string _getDetails() const = 0;

	virtual std::string GetFile() const { return ToStr(file); };
protected:
	std::string_view file{}; // File to act on
};

// CONFIG Command
class ConfigCommand : public FileCommand {
public:
	ConfigCommand() = delete;
	ConfigCommand(const std::string& f) { file = DAG::STRING_SPACE::__DEDUP(f); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::CONFIG; };
	virtual std::string _getDetails() const { return ToStr(file); }
};

// INCLUDE Command
class IncludeCommand : public FileCommand {
public:
	IncludeCommand() = delete;
	IncludeCommand(const std::string& f) { file = DAG::STRING_SPACE::__DEDUP(f); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::INCLUDE; };
	virtual std::string _getDetails() const { return ToStr(file); }
};

// DOT Command
class DotCommand : public FileCommand {
public:
	DotCommand() = delete;
	DotCommand(const std::string& f) { file = DAG::STRING_SPACE::__DEDUP(f); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::DOT; };
	virtual std::string _getDetails() const {
		return ToStr(file) + " " + ToStr(include) + " " + (update ? "T" : "F") + " " + (overwrite ? "T" : "F");
	}

	void SetInclude(const std::string& i) { include = DAG::STRING_SPACE::__DEDUP(i); }
	std::string GetInclude() const { return ToStr(include); }
	bool HasInclude() const { return !include.empty(); }

	void SetUpdate(const bool u) { update = u; }
	bool Update() const { return update; }

	void SetOverwrite(const bool o) { overwrite = o; }
	bool Overwrite() const { return overwrite; }
private:
	std::string_view include{}; // Extra DOT file header to include
	bool update{false}; // Whether or not to update the file periodically
	bool overwrite{false}; // Whether or not to overwrite old dot file
};

// NODE_STATUS_FILE Command
class NodeStatusCommand : public FileCommand {
public:
	NodeStatusCommand() = delete;
	NodeStatusCommand(const std::string& f) { file = DAG::STRING_SPACE::__DEDUP(f); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::NODE_STATUS_FILE; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %d %s", Disp(file), min_update, always_update ? "T" : "F");
		return ret;
	}

	void SetMinUpdateTime(const int t) { min_update = t; }
	int GetMinUpdateTime() const { return min_update; }

	void SetAlwaysUpdate() { always_update = true; }
	bool AlwaysUpdate() const { return always_update; }
private:
	int min_update{60}; // Minimal time between file updates
	bool always_update{false}; // Whether or not to update file regardless of state change
};

// JOBSTATE_LOG Command
class JobStateLogCommand : public FileCommand {
public:
	JobStateLogCommand() = delete;
	JobStateLogCommand(const std::string& f) { file = DAG::STRING_SPACE::__DEDUP(f); }

	virtual DAG::CMD GetCommand() const { return DAG::CMD::JOBSTATE_LOG; };
	virtual std::string _getDetails() const { return ToStr(file); }
};

// REJECT Command
class RejectCommand : public BaseDagCommand {
public:
	RejectCommand() = default;

	virtual DAG::CMD GetCommand() const { return DAG::CMD::REJECT; };
	virtual std::string _getDetails() const {
		return this->Location();
	}
};

// SET_JOB_ATTR Command
class SetAttrCommand : public BaseDagCommand {
public:
	SetAttrCommand() = delete;
	SetAttrCommand(const std::string& a) : attr_line(DAG::STRING_SPACE::__DEDUP(a)) {}

	virtual DAG::CMD GetCommand() const { return DAG::CMD::SET_JOB_ATTR; };
	virtual std::string _getDetails() const { return ToStr(attr_line); }

	std::string GetAttrLine() const { return ToStr(attr_line); }
private:
	std::string_view attr_line{}; // Attribute line to append (key = value)
};

// ENV Command
class EnvCommand : public BaseDagCommand {
public:
	EnvCommand() = delete;
	EnvCommand(const std::string& v, bool set) : vars(DAG::STRING_SPACE::__DEDUP(v)), is_set(set) {}

	virtual DAG::CMD GetCommand() const { return DAG::CMD::ENV; };
	virtual std::string _getDetails() const { return std::string((is_set ? "SET " : "GET ")) + ToStr(vars); }

	std::string GetEnvVariables() const { return ToStr(vars); }
	bool IsSet() const { return is_set; }
private:
	std::string_view vars{}; // Environment variables to do something with
	bool is_set{false}; // SET or GET behavior
};

// CONNECT Command (connect 2 splices with PINs)
class ConnectCommand : public BaseDagCommand {
public:
	ConnectCommand() = delete;
	ConnectCommand(const std::string& s1, const std::string& s2) : splice1(DAG::STRING_SPACE::__DEDUP(s1)), splice2(DAG::STRING_SPACE::__DEDUP(s2)) {}

	virtual DAG::CMD GetCommand() const { return DAG::CMD::CONNECT; };
	virtual std::string _getDetails() const { return std::string("[") + ToStr(splice1) + "]--[" + ToStr(splice2) + "]"; }

	std::tuple<std::string, std::string> GetSplices() const { return std::make_tuple(ToStr(splice1), ToStr(splice2)); }
private:
	std::string_view splice1{}; // Splice 1
	std::string_view splice2{}; // Splice 2
};

// PIN_[IN/OUT] Commands
class PinCommand : public BaseDagCommand {
public:
	PinCommand() = delete;
	PinCommand(const std::string& n, DAG::CMD c) : node(DAG::STRING_SPACE::__DEDUP(n)) {
		assert(c == DAG::CMD::PIN_IN || c == DAG::CMD::PIN_OUT);
		cmd = c;
	}

	virtual DAG::CMD GetCommand() const { return cmd; };
	virtual std::string _getDetails() const {
		std::string ret;
		formatstr(ret, "%s %d %s", Disp(node), pin, (cmd == DAG::CMD::PIN_IN) ? "IN" : "OUT");
		return ret;
	}

	std::string GetNode() const { return ToStr(node); }
	bool IsPinOut() const { return cmd == DAG::CMD::PIN_OUT; }

	void SetPinNum(const int n) { pin = n; }
	int GetPinNum() const { return pin; }
private:
	std::string_view node{}; // Node to apply pin
	int pin{1}; // Pin number to apply to node
	DAG::CMD cmd{DAG::CMD::PIN_IN}; // Specific PIN_IN/PIN_OUT Command
};

#endif // End DAG_COMMANDS_H
