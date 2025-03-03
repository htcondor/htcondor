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
	virtual void PrintInfo() {
		size_t len_pad = strlen(GetCommandStr()) >= 20 ? 0 : (20 - strlen(GetCommandStr()));
		std::string padding(len_pad, ' ');
		printf("[%02d] %s%s\n", (int)GetCommand(), padding.c_str(), GetDetails().c_str());
	}

	virtual std::string GetDetails() {
		std::string details;
		formatstr(details, "%s > %s", GetCommandStr(), _getDetails().c_str());
		return details;
	}

	virtual const char* GetCommandStr() { return DAG::GET_KEYWORD_STRING(GetCommand()); }

	// Get the commands enum value
	virtual DAG::CMD GetCommand() = 0;
	virtual std::string _getDetails() = 0;
};

// Create custom type to represent a unique pointer to BaseDagCommand
using DagCmd = std::unique_ptr<BaseDagCommand>;

// Abstract command for all node types
class NodeCommand : public BaseDagCommand {
public:
	NodeCommand() = default;

	virtual DAG::CMD GetCommand() = 0;
	virtual std::string _getDetails() {
		std::string ret;
		std::string desc = inline_desc.empty() ? "NONE" : inline_desc;
		std::replace(desc.begin(), desc.end(), '\n', DAG::NEWLINE_RELACEMENT);
		formatstr(ret, "%s %s {%s} %s %s %s", name.c_str(), submit.c_str(),
		          desc.c_str(), dir.c_str(), noop ? "T" : "F", done ? "T" : "F");
		return ret;
	}

	void SetDir(const std::string& s) { dir = s; }
	std::string GetDir() const { return dir; }
	bool HasDir() const { return !dir.empty(); }

	std::string GetName() const { return name; }

	void SetSubmit(const std::string& s) { submit = s; }
	std::string GetSubmit() const { return submit; }

	void SetInlineDesc(const std::string& s) { inline_desc = s; }
	std::string GetInlineDesc() const { return inline_desc; }
	bool HasInlineDesc() const { return !inline_desc.empty(); }

	void SetNoop() { noop = true; }
	bool IsNoop() const { return noop; }

	void SetDone() { done = true; }
	bool IsDone() const { return done; }
protected:
	std::string name{}; // Name of node
	std::string submit{}; // Name of submit file/description or sub-DAG
	std::string inline_desc{}; // Inline description contents
	std::string dir{}; // Execution directory
	bool noop{false}; // Specify no-operation node
	bool done{false}; // Specify pre-done node
};

// JOB Command
class JobCommand : public NodeCommand {
public:
	JobCommand() = delete;
	JobCommand(const std::string& n) { name = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::JOB; }
};

// FINAL Command
class FinalCommand : public NodeCommand {
public:
	FinalCommand() = delete;
	FinalCommand(const std::string& n) { name = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::FINAL; }
};

// SERVICE Command
class ServiceCommand : public NodeCommand {
public:
	ServiceCommand() = delete;
	ServiceCommand(const std::string& n) { name = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::SERVICE; }
};

// PROVISIONER Command
class ProvisionerCommand : public NodeCommand {
public:
	ProvisionerCommand() = delete;
	ProvisionerCommand(const std::string& n) { name = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::PROVISIONER; }
};

// SUBDAG Command
class SubdagCommand : public NodeCommand {
public:
	SubdagCommand() = delete;
	SubdagCommand(const std::string& n) { name = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::SUBDAG; }
};

// SUBMIT-DESCRIPTION Command (Shared inline job submit description)
class SubmitDescCommand : public BaseDagCommand {
public:
	SubmitDescCommand() = delete;
	SubmitDescCommand(const std::string& n) : name(n) {}

	virtual DAG::CMD GetCommand() { return DAG::CMD::SUBMIT_DESCRIPTION; }
	virtual std::string _getDetails() {
		std::string ret;
		std::string desc(inline_desc);
		std::replace(desc.begin(), desc.end(), '\n', DAG::NEWLINE_RELACEMENT);
		formatstr(ret, "%s {%s}", name.c_str(), desc.c_str());
		return ret;
	}

	std::string GetName() const { return name; }

	void SetInlineDesc(const std::string& s) { inline_desc = s; }
	std::string GetInlineDesc() const { return inline_desc; }
	bool HasInlineDesc() const { return !inline_desc.empty(); }

private:
	std::string name{}; // Name to refer to inline description
	std::string inline_desc{}; // Inline description contents
};

// SPLICE Command
class SpliceCommand : public BaseDagCommand {
public:
	SpliceCommand() = delete;
	SpliceCommand(const std::string& n) : name(n) {}

	virtual DAG::CMD GetCommand() { return DAG::CMD::SPLICE; }
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %s %s", name.c_str(), file.c_str(), dir.c_str());
		return ret;
	}

	std::string GetName() const { return name; }

	void SetDir(const std::string& s) { dir = s; }
	std::string GetDir() const { return dir; }
	bool HasDir() const { return !dir.empty(); }

	void SetDagFile(const std::string& f) { file = f; }
	std::string GetDagFile() const { return file; }
private:
	std::string name{}; // Splice name for references
	std::string file{}; // DAG file to splice into DAG
	std::string dir{}; // Directory to do splicing from
};

// PARENT(CHILD) Command
class ParentChildCommand : public BaseDagCommand {
public:
	ParentChildCommand() = default;

	virtual DAG::CMD GetCommand() { return DAG::CMD::PARENT_CHILD; }
	virtual std::string _getDetails() {
		std::string ret = "[ ";
		for (const auto& p : parents) { ret += p + " "; }
		ret += "] --> [ ";
		for (const auto& c : children) { ret += c + " "; }
		return ret + "]";
	}

	std::vector<std::string> parents{}; // List of parent nodes
	std::vector<std::string> children{}; // List of child nodes
};

// Abstract class to modify some behavior of a node type
class NodeModifierCommand : public BaseDagCommand {
public:
	NodeModifierCommand() = default;

	virtual DAG::CMD GetCommand() = 0;
	virtual std::string _getDetails() = 0;

	virtual std::string GetNodeName() const { return node; };
protected:
	std::string node{}; // Node name to apply command
};

// SCRIPT Command
class ScriptCommand : public NodeModifierCommand {
public:
	ScriptCommand() = default;

	virtual DAG::CMD GetCommand() { return DAG::CMD::SCRIPT; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %s '%s' %lld %d %s %s", node.c_str(), DAG::GET_SCRIPT_TYPE_STRING(type),
		          script.c_str(), (long long)defer_time, defer_status, debug_file.c_str(),
		          DAG::GET_SCRIPT_DEBUG_CAPTURE_TYPE(capture));
		return ret;
	}

	void SetNodeName(const std::string& n) { node = n; }

	std::string GetScript() const { return script; }
	void SetScript(const std::string& s) { script = s; }

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
		debug_file = f;
		capture = c;
	}
	std::tuple<std::string, DAG::ScriptOutput> GetDebugInfo() const { return std::make_tuple(debug_file, capture); }
private:
	std::string script{}; // Script + args to execute
	std::string debug_file{}; // Debug file to write script stdout/stderr to
	time_t defer_time{0}; // Deferal time
	int defer_status{-1}; // Exit code to trigger deferal
	DAG::SCRIPT type{}; // Script type
	DAG::ScriptOutput capture{DAG::ScriptOutput::NONE}; // Output streams to capture for debug file
};

// RETRY Command
class RetryCommand : public NodeModifierCommand {
public:
	RetryCommand() = delete;
	RetryCommand(const std::string& n) { node = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::RETRY; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %d %d", node.c_str(), max, code);
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
	AbortDagCommand(const std::string& n) { node = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::ABORT_DAG_ON; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %d %d", node.c_str(), status, code);
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
	VarsCommand(const std::string& n) { node = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::VARS; };
	virtual std::string _getDetails() {
		std::string ret = node;
		if (when == DAG::VarsPlacement::PREPEND) { ret += " PREPEND"; }
		else if (when == DAG::VarsPlacement::APPEND) { ret += " APPEND"; }

		for (const auto& [k,v] : kv_pairs) { ret += " [" + k + "=" + v + "]"; }
		return ret;
	}

	void AddPair(const std::string& k, const std::string& v) { kv_pairs[k] = v; }
	const std::map<std::string, std::string>& GetPairs() const { return kv_pairs; }

	void Prepend() { when = DAG::VarsPlacement::PREPEND; }
	void Append() { when = DAG::VarsPlacement::APPEND; }
	DAG::VarsPlacement GetPlacement() const { return when; }
private:
	std::map<std::string, std::string> kv_pairs{}; // Map of key -> value pairs to add
	DAG::VarsPlacement when{DAG::VarsPlacement::DEFAULT}; // Specify when to add info (before/after parsing submit description)
};

// PRIORITY Command
class PriorityCommand : public NodeModifierCommand {
public:
	PriorityCommand() = delete;
	PriorityCommand(const std::string& n) { node = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::PRIORITY; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %d", node.c_str(), prio);
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
	PreSkipCommand(const std::string& n) { node = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::PRE_SKIP; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %d", node.c_str(), code);
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
	DoneCommand(const std::string& n) { node = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::DONE; };
	virtual std::string _getDetails() { return node; }
};

// SAVE_POINT_FILE Command
class SavePointCommand : public NodeModifierCommand {
public:
	SavePointCommand() = delete;
	SavePointCommand(const std::string& n) { node = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::SAVE_POINT_FILE; };
	virtual std::string _getDetails() { return node + " " + file; }

	void SetFilename(const std::string& f) { file = f; }
	std::string GetFilename() const { return file; }
private:
	std::string file{}; // File to save state
};

// CATEGORY Command
class CategoryCommand : public BaseDagCommand {
public:
	CategoryCommand() = delete;
	CategoryCommand(const std::string& n) { name = n; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::CATEGORY; };
	virtual std::string _getDetails() {
		std::string ret = name;
		for (const auto& n : nodes) { ret += " " + n; }
		return ret;
	}

	std::string GetCategory() const { return name; }

	std::vector<std::string> nodes{}; // List of nodes to add to category
private:
	std::string name{}; // Category name
};

// MAXJOBS Command
class MaxJobsCommand : public BaseDagCommand {
public:
	MaxJobsCommand() = delete;
	MaxJobsCommand(const std::string& cat) : category(cat) {}

	virtual DAG::CMD GetCommand() { return DAG::CMD::MAXJOBS; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %d", category.c_str(), limit);
		return ret;
	}

	std::string GetCategory() const { return category; }

	void SetLimit(const int m) { limit = m; }
	int GetLimit() const { return limit; }
private:
	std::string category{}; // Category to apply throttle
	int limit{1}; // Maximum number of nodes with placed jobs
};

// Abstract class to handle an external file used by DAGMan
class FileCommand : public BaseDagCommand {
public:
	FileCommand() = default;

	virtual DAG::CMD GetCommand() = 0;
	virtual std::string _getDetails() = 0;

	virtual std::string GetFile() const { return file; };
protected:
	std::string file{}; // File to act on
};

// CONFIG Command
class ConfigCommand : public FileCommand {
public:
	ConfigCommand() = delete;
	ConfigCommand(const std::string& f) { file = f; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::CONFIG; };
	virtual std::string _getDetails() { return file; }
};

// INCLUDE Command
class IncludeCommand : public FileCommand {
public:
	IncludeCommand() = delete;
	IncludeCommand(const std::string& f) { file = f; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::INCLUDE; };
	virtual std::string _getDetails() { return file; }
};

// DOT Command
class DotCommand : public FileCommand {
public:
	DotCommand() = delete;
	DotCommand(const std::string& f) { file = f; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::DOT; };
	virtual std::string _getDetails() {
		return file + " " + include + " " + (update ? "T" : "F") + " " + (overwrite ? "T" : "F");
	}

	void SetInclude(const std::string& i) { include = i; }
	std::string GetInclude() const { return include; }
	bool HasInclude() const { return !include.empty(); }

	void SetUpdate(const bool u) { update = u; }
	bool Update() const { return update; }

	void SetOverwrite(const bool o) { overwrite = o; }
	bool Overwrite() const { return overwrite; }
private:
	std::string include{}; // Extra DOT file header to include
	bool update{false}; // Whether or not to update the file periodically
	bool overwrite{false}; // Whether or not to overwrite old dot file
};

// NODE_STATUS_FILE Command
class NodeStatusCommand : public FileCommand {
public:
	NodeStatusCommand() = delete;
	NodeStatusCommand(const std::string& f) { file = f; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::NODE_STATUS_FILE; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %d %s", file.c_str(), min_update, always_update ? "T" : "F");
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
	JobStateLogCommand(const std::string& f) { file = f; }

	virtual DAG::CMD GetCommand() { return DAG::CMD::JOBSTATE_LOG; };
	virtual std::string _getDetails() { return file; }
};

// REJECT Command
class RejectCommand : public BaseDagCommand {
public:
	RejectCommand() = delete;
	RejectCommand(const std::string& f, const int n) : file(f), line_no(n) {}

	virtual DAG::CMD GetCommand() { return DAG::CMD::REJECT; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s:%d", file.c_str(), line_no);
		return ret;
	}

	std::string GetFile() const { return file; }
	int GetLine() const { return line_no; }
private:
	std::string file{};
	int line_no{0};
};

// SET_JOB_ATTR Command
class SetAttrCommand : public BaseDagCommand {
public:
	SetAttrCommand() = delete;
	SetAttrCommand(const std::string& a) : attr_line(a) {}

	virtual DAG::CMD GetCommand() { return DAG::CMD::SET_JOB_ATTR; };
	virtual std::string _getDetails() { return attr_line; }

	std::string GetAttrLine() const { return attr_line; }
private:
	std::string attr_line{}; // Attribute line to append (key = value)
};

// ENV Command
class EnvCommand : public BaseDagCommand {
public:
	EnvCommand() = delete;
	EnvCommand(const std::string& v, bool set) : vars(v), is_set(set) {}

	virtual DAG::CMD GetCommand() { return DAG::CMD::ENV; };
	virtual std::string _getDetails() { return (is_set ? "SET " : "GET ") + vars; }

	std::string GetEnvVariables() const { return vars; }
	bool IsSet() const { return is_set; }
private:
	std::string vars{}; // Environment variables to do something with
	bool is_set{false}; // SET or GET behavior
};

// CONNECT Command (connect 2 splices with PINs)
class ConnectCommand : public BaseDagCommand {
public:
	ConnectCommand() = delete;
	ConnectCommand(const std::string& s1, const std::string& s2) : splice1(s1), splice2(s2) {}

	virtual DAG::CMD GetCommand() { return DAG::CMD::CONNECT; };
	virtual std::string _getDetails() { return "[" + splice1 + "]--[" + splice2 + "]"; }

	std::tuple<std::string, std::string> GetSplices() const { return std::make_tuple(splice1, splice2); }
private:
	std::string splice1{}; // Splice 1
	std::string splice2{}; // Splice 2
};

// PIN_[IN/OUT] Commands
class PinCommand : public BaseDagCommand {
public:
	PinCommand() = delete;
	PinCommand(const std::string& n, DAG::CMD c) : node(n) {
		assert(c == DAG::CMD::PIN_IN || c == DAG::CMD::PIN_OUT);
		cmd = c;
	}

	virtual DAG::CMD GetCommand() { return cmd; };
	virtual std::string _getDetails() {
		std::string ret;
		formatstr(ret, "%s %d %s", node.c_str(), pin, (cmd == DAG::CMD::PIN_IN) ? "IN" : "OUT");
		return ret;
	}

	std::string GetNode() const { return node; }
	bool IsPinOut() const { return cmd == DAG::CMD::PIN_OUT; }

	void SetPinNum(const int n) { pin = n; }
	int GetPinNum() const { return pin; }
private:
	std::string node{}; // Node to apply pin
	int pin{1}; // Pin number to apply to node
	DAG::CMD cmd{DAG::CMD::PIN_IN}; // Specific PIN_IN/PIN_OUT Command
};

#endif // End DAG_COMMANDS_H
