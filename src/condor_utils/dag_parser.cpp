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

#include "dag_parser.h"
#include <algorithm>
#include <stdexcept>
#include <assert.h>

static const char* DELIMITERS = " \t";
static const char* ILLEGAL_CHARS = "+";
static bool TRIM_QUOTES = true;

//--------------------------------------------------------------------------------------------
std::string
DagLexer::next(bool trim_quotes) {
	std::string token;
	bool escaped = false;
	bool in_quotes = false;
	bool is_pair = false;
	bool pair_complete = false;
	char quote = '-';

	while (pos < len && str[pos]) {
		if (escaped) {
			token += str[pos];
			escaped = false;
		} else if (in_quotes) {
			assert(quote == '"' || quote == '\'');
			if (str[pos] == '\\') {
				escaped = true;
			} else if (str[pos] == quote) {
				if ( ! trim_quotes) { token += quote; }
				in_quotes = false;
				quote = '-';
			} else {
				token += str[pos];
			}
		} else if (strchr(DELIMITERS, str[pos])) {
			bool next_char_equals = false;
			while (pos+1 < len && str[pos+1]) {
				if (strchr(DELIMITERS, str[pos+1])) { pos++; }
				else {
					if (str[pos+1] == '=') { next_char_equals = true; }
					break;
				}
			}
			if ( ! token.empty()) {
				if ((!is_pair && !next_char_equals) || pair_complete) {
					is_pair = pair_complete = false;
					break;
				}
			}
		} else if (!is_pair && str[pos] == '=') {
			is_pair = true;
			token += str[pos];
		} else {
			if (str[pos] == '"' || str[pos] == '\'') {
				quote = str[pos];
				if ( ! trim_quotes) { token += quote; }
				in_quotes = true;
			} else {
				token += str[pos];
			}

			if (is_pair) { pair_complete = true; }
		}

		pos++;
	}

	if (in_quotes) {
		err = "Invalid quoting: no ending quote found";
		return "";
	} else if (is_pair && !pair_complete) {
		err = "Invalid key value pair: no value discovered";
		return "";
	}

	return token;
}

std::string
DagLexer::remain() {
	while (pos < len && str[pos] && isspace(str[pos])) { pos++; }
	std::string ret = (pos < str.size() && str[pos]) ? std::string(str.substr(pos)) : "";
	pos = len;
	return ret;
}

//--------------------------------------------------------------------------------------------
static bool
skip_line(const std::string& line) {
	return (line.empty() || line[0] == '#' || line.substr(0, 2) == "//");
}

//--------------------------------------------------------------------------------------------
bool
DagParser::get_inline_desc_end(const std::string& desc, std::string& end) {
	if (desc.empty()) { return false; }
	else if (desc[0] == '{') { end = "}"; return true; }

	if (starts_with(desc, "@=")) {
		end = desc.length() > 2 ? "@" + desc.substr(2) : "";
		return true;
	}

	return false;
}

std::string
DagParser::parse_inline_desc(std::ifstream& stream, const std::string& end, std::string& error, std::string& endline) {
	std::string desc;
	std::string line;
	bool found_end = false;

	if (end.empty()) {
		error = "No inline description closing token specified (@=TOKEN)";
		return desc;
	}

	while (std::getline(stream, line)) {
		line_no++;
		trim(line);
		if (skip_line(line)) { continue; }

		if (line == end || starts_with(line, end + " ")) {
			endline = (line.length() > end.length()) ? line.substr(end.length()) : "";
			found_end = true;
			break;
		}
		desc += line + "\n";
	}

	if ( ! found_end) {
		error = "Missing inline description closing token: " + end;
	}

	return desc;
}

std::string 
DagParser::ParseSubmitDesc(std::ifstream& stream, DagLexer& details) {
	
	std::string token = details.next();
	if (token.empty()) {
		return "No submit description name provided";
	}
	data.reset(new SubmitDescCommand(token));

	token = details.next();
	if (token.empty()) {
		return "No inline description provided";
	}

	std::string inline_end, final_line;
	if (get_inline_desc_end(token, inline_end)) {
		std::string error;
		std::string inline_desc = parse_inline_desc(stream, inline_end, error, final_line);
		if ( ! error.empty()) { return error; }
		((SubmitDescCommand*)data.get())->SetInlineDesc(inline_desc);
		return "";
	}

	return "No inline description provided";
}

std::string
DagParser::ParseNodeTypes(std::ifstream& stream, DagLexer& details, DAG::CMD type) {
	std::string node_name = details.next();
	if (node_name.empty()) {
		return "Missing node name";
	}

	if (DAG::RESERVED.contains(node_name)) {
		return "Node name is a reserved word";
	} else if ( ! allow_illegal_chars) {
		auto illegal = [](const char c){ return strchr(ILLEGAL_CHARS, c); };
		if (std::find_if(node_name.begin(), node_name.end(), illegal) != node_name.end()) {
			return "Node name contains illegal charater";
		}
	}

	switch (type) {
		case DAG::CMD::SUBDAG:
			data.reset(new SubdagCommand(node_name));
			break;
		case DAG::CMD::JOB:
			data.reset(new JobCommand(node_name));
			break;
		case DAG::CMD::FINAL:
			data.reset(new FinalCommand(node_name));
			break;
		case DAG::CMD::PROVISIONER:
			data.reset(new ProvisionerCommand(node_name));
			break;
		case DAG::CMD::SERVICE:
			data.reset(new ServiceCommand(node_name));
			break;
		default:
			// This is a developer error so throw exception
			throw std::invalid_argument("Invalid DAG Command: Not a node type");
	}

	NodeCommand* nodeCmd = (NodeCommand*)data.get();
	assert(nodeCmd != nullptr);

	std::string desc = details.next(TRIM_QUOTES);
	if (desc.empty()) {
		return "No submit description provided";
	}

	std::string inline_end, final_line;
	if (type != DAG::CMD::SUBDAG && get_inline_desc_end(desc, inline_end)) {
		std::string error;
		std::string inline_desc = parse_inline_desc(stream, inline_end, error, final_line);
		if ( ! error.empty()) { return error; }
		nodeCmd->SetInlineDesc(inline_desc);
		nodeCmd->SetSubmit("INLINE");
		details.parse(final_line);
	} else {
		nodeCmd->SetSubmit(desc);
	}

	std::string error = "";
	while (true) {
		std::string token = details.next();
		if (token.empty()) { break; }
		else if (strcasecmp(token.c_str(), "NOOP") == 0) {
			nodeCmd->SetNoop();
		} else if (strcasecmp(token.c_str(), "DONE") == 0) {
			nodeCmd->SetDone();
		} else if (strcasecmp(token.c_str(), "DIR") == 0) {
			std::string dir = details.next(TRIM_QUOTES);
			if (dir.empty()) {
				error = "No directory path provided for DIR subcommand";
				break;
			}
			nodeCmd->SetDir(dir);
		} else {
			error = "Unexpected token '" + token + "'";
			break;
		}
	}

	return error;
}

std::string
DagParser::ParseSplice(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) {
		return "Missing splice name";
	}
	data.reset(new SpliceCommand(token));
	SpliceCommand* cmd = (SpliceCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next(TRIM_QUOTES);
	if (token.empty()) {
		return "Missing DAG file";
	}
	cmd->SetDagFile(token);

	std::string error = "";
	token = details.next();
	if ( ! token.empty()) {
		if (strcasecmp(token.c_str(), "DIR") == 0) {
			std::string dir = details.next(TRIM_QUOTES);
			if (dir.empty()) {
				error = "No directory path provided for DIR subcommand";
			} else {
				cmd->SetDir(dir);
			}

			token = details.next();
			if ( ! token.empty()) { error = "Unexpected token '" + token + "'"; }
		} else {
			error = "Unexpected token '" + token + "'";
		}
	}

	return error;
}

std::string
DagParser::ParseParentChild(DagLexer& details) {
	data.reset(new ParentChildCommand);
	ParentChildCommand* cmd = (ParentChildCommand*)data.get();
	assert(cmd != nullptr);

	bool parsing_children = false;
	std::string token = details.next();
	if (token.empty() || strcasecmp(token.c_str(), "CHILD") == 0) {
		return "No parent node(s) specified";
	}

	std::string error = "Missing CHILD specifier";
	do {
		if (strcasecmp(token.c_str(), "CHILD") == 0) {
			if ( ! details.peek().empty()) {
				parsing_children = true;
				error.clear();
			} else { error = "No children node(s) specified"; }
		} else if (parsing_children) {
			cmd->children.emplace_back(token);
		} else {
			cmd->parents.emplace_back(token);
		}
		token = details.next();
	} while ( ! token.empty());

	return error;
}

std::string
DagParser::ParseScript(DagLexer& details) {
	data.reset(new ScriptCommand);
	ScriptCommand* cmd = (ScriptCommand*)data.get();
	assert(cmd != nullptr);

	while (true) {
		std::string token = details.next();
		if (DAG::SCRIPT_TYPES_MAP.contains(token)) {
			cmd->SetType(DAG::SCRIPT_TYPES_MAP.at(token));
			break;
		} else if (strcasecmp(token.c_str(), "DEFER") == 0) {
			std::string value = details.next();
			int status = 0;
			time_t interval = 0;

			if (value.empty()) { return "DEFER missing status value"; }
			try {
				status = std::stoi(value);
			} catch (...) { return "Invalid DEFER status value '" + value +"'"; }

			value = details.next();
			if (value.empty()) { return "DEFER missing time value"; }
			try {
				interval = (time_t)std::stoi(value);
			} catch (...) { return "Invalid DEFER time value '" + value + "'"; }
			cmd->SetDeferal(status, interval);
		} else if (strcasecmp(token.c_str(), "DEBUG") == 0) {
			std::string file = details.next();
			if (file.empty()) { return "DEBUG missing filename"; }
			std::string stream = details.next();

			if (stream.empty()) { return "DEBUG missing output stream type (STDOUT, STDERR, ALL)"; }
			else if ( ! DAG::SCRIPT_DEBUG_MAP.contains(stream)) {
				return "Unknown DEBUG output stream type '" + stream + "'";
			}

			DAG::ScriptOutput type = DAG::SCRIPT_DEBUG_MAP.at(stream);
			cmd->SetDebugInfo(file, type);
		} else {
			return "Missing script type (PRE, POST, HOLD) and optional sub-commands (DEFER, DEBUG)";
		}
	}

	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }
	cmd->SetNodeName(token);

	token = details.remain();
	trim(token);
	if (token.empty()) { return "No script specified"; }
	cmd->SetScript(token);

	return "";
}

std::string
DagParser::ParseRetry(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }

	data.reset(new RetryCommand(token));
	RetryCommand* cmd = (RetryCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next();
	if (token.empty()) { return "Missing max retry value"; }

	try {
		int max = std::stoi(token);
		if (max < 0) { throw std::invalid_argument("Negative number of retries specified"); }
		cmd->SetMaxRetries(max);
	} catch (...) {
		return "Invalid max retry value '" + token + "' (Must be positive integer)";
	}

	token = details.next();
	if ( ! token.empty()) {
		if (strcasecmp(token.c_str(), "UNLESS-EXIT") != 0) {
			return "Unexpected token '" + token + "'";
		}

		token = details.next();
		if (token.empty()) { return "UNLESS-EXIT missing exit code"; }

		try {
			int code = std::stoi(token);
			cmd->SetBreakCode(code);
		} catch (...) {
			return "Invalid UNLESS-EXIT code '" + token + "' specified";
		}

		token = details.next();
		if ( ! token.empty()) { return "Unexpected token '" + token + "'"; }
	}

	return "";
}

std::string
DagParser::ParseAbortDagOn(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }

	data.reset(new AbortDagCommand(token));
	AbortDagCommand* cmd = (AbortDagCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next();
	if (token.empty()) { return "Missing exit status to abort on"; }

	try {
		int status = std::stoi(token);
		cmd->SetCondition(status);
	} catch (...) {
		return "Invalid exit status '" + token + "' specified";
	}

	token = details.next();
	if ( ! token.empty()) {
		if (strcasecmp(token.c_str(), "RETURN") != 0) {
			return "Unexpected token '" + token + "'";
		}

		token = details.next();
		if (token.empty()) { return "RETURN is missing value"; }

		try {
			int code = std::stoi(token);
			if (code < 0 || code > 255) { throw std::invalid_argument("Value out of range 0-255"); }
			cmd->SetExitValue(code);
		} catch (...) {
			return "Invalid RETURN code '" + token +"' (Must be between 0 and 255)";
		}

		token = details.next();
		if ( ! token.empty()) { return "Unexpected token '" + token + "'"; }
	}

	return "";
}

std::string
DagParser::ParseVars(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }

	data.reset(new VarsCommand(token));
	VarsCommand* cmd = (VarsCommand*)data.get();
	assert(cmd != nullptr);

	// Possibly APPEND or PREPEND keyword
	token = details.next();

	if (strcasecmp(token.c_str(), "PREPEND") == 0) {
		cmd->Prepend();
		token = details.next();
	} else if (strcasecmp(token.c_str(), "APPEND") == 0) {
		cmd->Append();
		token = details.next();
	}

	int num_pairs = 0;
	while ( ! token.empty()) {
		size_t equals = token.find_first_of("=");
		if (equals == std::string::npos) { return "Non key=value token specified: " + token; }

		std::string key = token.substr(0, equals);
		std::string val = token.substr(equals + 1);

		if (key.empty()) { return "Invalid key=value pair: Missing key"; }
		if (val.empty()) { return "Invalid key=value pair: Missing value"; }

		if (key == "+") { return "Variable name must contain at least one alphanumeric character"; }
		else if (key[0] == '+') { key = "My." + key.substr(1); }

		std::string tmp(key);
		lower_case(tmp);
		if (tmp.starts_with("queue")) { return "Illegal variable name '" + key + "': name can not begin with 'queue'"; }

		num_pairs++;
		cmd->AddPair(key, val);

		token = details.next();
	}

	return (num_pairs == 0) ? "No key=value pairs specified" : "";
}

std::string
DagParser::ParsePriority(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }

	data.reset(new PriorityCommand(token));
	PriorityCommand* cmd = (PriorityCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next();
	if (token.empty()) { return "Missing priority value"; }

	try {
		int prio = std::stoi(token);
		cmd->SetPriority(prio);
	} catch (...) {
		return "Invalid priority value '" + token + "'";
	}

	token = details.next();
	return ( ! token.empty()) ? "Unexpected token '" + token + "'" : "";
}

std::string
DagParser::ParsePreSkip(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }

	data.reset(new PreSkipCommand(token));
	PreSkipCommand* cmd = (PreSkipCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next();
	if (token.empty()) { return "Missing exit code"; }

	try {
		int code = std::stoi(token);
		cmd->SetExitCode(code);
	} catch (...) {
		return "Invalid exit code '" + token + "'";
	}

	token = details.next();
	return ( ! token.empty()) ? "Unexpected token '" + token + "'" : "";
}

std::string
DagParser::ParseSavePoint(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }

	data.reset(new SavePointCommand(token));
	SavePointCommand* cmd = (SavePointCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next(TRIM_QUOTES);
	if (token.empty()) {
		// Default <node name>-<dag file>.save
		std::string file = cmd->GetNodeName() + "-" + path.filename().string() + ".save";
		cmd->SetFilename(file);
	} else {
		cmd->SetFilename(token);
		token = details.next();
		if ( ! token.empty()) { return "Unexpected token '" + token + "'"; }
	}

	return "";
}

std::string
DagParser::ParseCategory(DagLexer& details) {
	std::string node = details.next();
	if (node.empty()) { return "No node name specified"; }

	std::string name = details.next();
	if (name.empty()) { return "No category name specified"; }

	std::string junk = details.next();
	if ( ! junk.empty()) { return "Unexpected token '" + junk + "'"; }

	data.reset(new CategoryCommand(name));
	((CategoryCommand*)data.get())->nodes.emplace_back(node);

	return "";
}

std::string
DagParser::ParseMaxJobs(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "No category name specified"; }

	data.reset(new MaxJobsCommand(token));
	MaxJobsCommand* cmd = (MaxJobsCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next();
	if (token.empty()) { return "No throttle limit specified"; }

	try {
		int limit = std::stoi(token);
		if (limit < 0) { throw std::invalid_argument("MAXJOBS throttle limit must be a positive integer"); }
		cmd->SetLimit(limit);
	} catch (...) {
		return "Invalid throttle limit '" + token + "'";
	}

	token = details.next();
	return ( ! token.empty()) ? "Unexpected token '" + token + "'" : "";
}

std::string
DagParser::ParseConfig(DagLexer& details) {
	std::string file = details.next(TRIM_QUOTES);
	if (file.empty()) { return "No configuration file specified"; }

	std::string junk = details.next();
	if ( ! junk.empty()) { return "Unexpected token '" + junk + "'"; }

	std::filesystem::path path(file);
	if ( ! path.is_absolute()) {
		path = std::filesystem::absolute(path);
	}

	file = path.string();

	data.reset(new ConfigCommand(file));
	return "";
}

std::string
DagParser::ParseDot(DagLexer& details) {
	std::string token = details.next(TRIM_QUOTES);
	if (token.empty()) { return "No file specified"; }

	data.reset(new DotCommand(token));
	DotCommand* cmd = (DotCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next();
	while ( ! token.empty()) {
		if (strcasecmp(token.c_str(), "UPDATE") == 0) {
			cmd->SetUpdate(true);
		} else if (strcasecmp(token.c_str(), "DONT-UPDATE") == 0) {
			cmd->SetUpdate(false);
		} else if (strcasecmp(token.c_str(), "OVERWRITE") == 0) {
			cmd->SetOverwrite(true);
		} else if (strcasecmp(token.c_str(), "DONT-OVERWRITE") == 0) {
			cmd->SetOverwrite(false);
		} else if (strcasecmp(token.c_str(), "INCLUDE") == 0) {
			token = details.next();
			if (token.empty()) { return "Missing INCLUDE header file"; }
			cmd->SetInclude(token);
		} else {
			return "Unexpected token '" + token + "'";
		}

		token = details.next();
	}

	return "";
}

std::string
DagParser::ParseNodeStatus(DagLexer& details) {
	std::string token = details.next(TRIM_QUOTES);
	if (token.empty()) { return "No file specified"; }

	data.reset(new NodeStatusCommand(token));
	NodeStatusCommand* cmd = (NodeStatusCommand*)data.get();
	assert(cmd != nullptr);

	token = details.next();
	while ( ! token.empty()) {
		if (strcasecmp(token.c_str(), "ALWAYS-UPDATE") == 0) {
			cmd->SetAlwaysUpdate();
		} else {
			try {
				int min = std::stoi(token);
				cmd->SetMinUpdateTime(min);
			} catch (...) {
				return "Unexpected token '" + token + "'";
			}
		}

		token = details.next();
	}

	return "";
}

std::string
DagParser::ParseEnv(DagLexer& details) {
	std::string token = details.next();
	if (token.empty()) { return "Missing action (SET or GET) and variables"; }

	bool set = false;
	if (strcasecmp(token.c_str(), "SET") == 0) {
		set = true;
	} else if (strcasecmp(token.c_str(), "GET") != 0) {
		return "Unexpected token '" + token + "'";
	}

	std::string vars = details.remain();
	trim(vars);
	if (vars.empty()) { return "No environment variables provided"; }

	data.reset(new EnvCommand(vars, set));
	return "";
}

std::string
DagParser::ParseConnect(DagLexer& details) {
	std::string s1 = details.next();
	std::string s2 = details.next();

	if (s1.empty() || s2.empty()) { return "Missing splice(s) to connect"; }

	std::string junk = details.next();
	if ( ! junk.empty()) { return "Unexpected token '" + junk + "'"; }

	data.reset(new ConnectCommand(s1, s2));
	return "";
}

std::string
DagParser::ParsePin(DagLexer& details, DAG::CMD type) {
	assert(type == DAG::CMD::PIN_IN || type == DAG::CMD::PIN_OUT);

	std::string token = details.next();
	if (token.empty()) { return "No node name specified"; }

	data.reset(new PinCommand(token, type));

	token = details.next();
	if (token.empty()) { return "No pin number specified"; }

	try {
		int pin = std::stoi(token);
		if (pin < 1) { throw std::invalid_argument("Pin number must be greater than or equal to 1"); }
		((PinCommand*)data.get())->SetPinNum(pin);
	} catch (...) {
		return "Invalid pin number '" + token + "'";
	}

	token = details.next();
	return ( ! token.empty()) ? "Unexpected token '" + token + "'" : "";
}

//--------------------------------------------------------------------------------------------
bool
DagParser::next() {
	std::string line;

	while (std::getline(fs, line)) {
		int command_line_no = ++line_no;
		trim(line);
		if (skip_line(line)) { continue; }

		bool parse_success = true;

		DagLexer details(line);
		std::string cmd_str = details.next();
		std::replace(cmd_str.begin(), cmd_str.end(), '-', '_'); // Make PRE_SKIP == PRE-SKIP

		if ( ! DAG::KEYWORD_MAP.contains(cmd_str)) {
			parse_success = false;
			formatstr(err, "%s:%d '%s' is not a valid DAG command",
			                GetFile().c_str(), command_line_no, cmd_str.c_str());
			all_errors.emplace_back(err);
		} else {
			DAG::CMD cmd = DAG::KEYWORD_MAP.at(cmd_str);

			std::string parse_error, check;

			if (filter_ignore.contains(cmd) || (! filter_only.empty() && ! filter_only.contains(cmd))) {
				// If ignored command is potentially multilined then parse the command
				if (cmd >= DAG::CMD::JOB && cmd <= DAG::CMD::SERVICE) {
					parse_error = ParseNodeTypes(fs, details, cmd);
				} else if (cmd == DAG::CMD::SUBMIT_DESCRIPTION) {
					parse_error = ParseSubmitDesc(fs, details);
				}

				if ( ! parse_error.empty()) {
					formatstr(err, "%s:%d Failed to parse %s command: %s",
					          GetFile().c_str(), command_line_no, cmd_str.c_str(), parse_error.c_str());
					all_errors.emplace_back(err);
					return false;
				}

				continue;
			}

			switch (cmd) {
				case DAG::CMD::SUBDAG:
					check = details.next(); // TODO: Make External keyword optional
					if (strcasecmp(check.c_str(), "EXTERNAL") != 0) {
						parse_error = "Missing EXTERNAL keyword";
						break;
					}
					[[fallthrough]];
				case DAG::CMD::JOB:
				case DAG::CMD::FINAL:
				case DAG::CMD::PROVISIONER:
				case DAG::CMD::SERVICE:
					parse_error = ParseNodeTypes(fs, details, cmd);
					break;
				case DAG::CMD::SPLICE:
					parse_error = ParseSplice(details); // TODO: Support inline DAG splices
					break;
				case DAG::CMD::SUBMIT_DESCRIPTION:
					parse_error = ParseSubmitDesc(fs, details);
					break;
				case DAG::CMD::PARENT_CHILD:
					parse_error = ParseParentChild(details);
					break;
				case DAG::CMD::SCRIPT:
					parse_error = ParseScript(details);
					break;
				case DAG::CMD::RETRY:
					parse_error = ParseRetry(details);
					break;
				case DAG::CMD::ABORT_DAG_ON:
					parse_error = ParseAbortDagOn(details);
					break;
				case DAG::CMD::VARS:
					parse_error = ParseVars(details);
					break;
				case DAG::CMD::PRIORITY:
					parse_error = ParsePriority(details);
					break;
				case DAG::CMD::PRE_SKIP:
					parse_error = ParsePreSkip(details);
					break;
				case DAG::CMD::DONE:
					check = details.next();
					if (check.empty()) {
						parse_error = "No node name specified";
					} else {
						data.reset(new DoneCommand(check));
						check = details.next();
						if ( ! check.empty()) {
							parse_error = "Unexpected token '" + check + "'";
						}
					}
					break;
				case DAG::CMD::CATEGORY:
					parse_error = ParseCategory(details);
					break;
				case DAG::CMD::MAXJOBS:
					parse_error = ParseMaxJobs(details);
					break;
				case DAG::CMD::CONFIG:
					parse_error = ParseConfig(details);
					break;
				case DAG::CMD::INCLUDE:
					check = details.next(TRIM_QUOTES);
					if (check.empty()) {
						parse_error = "No include file specified";
					} else {
						data.reset(new IncludeCommand(check));
						check = details.next();
						if ( ! check.empty()) {
							parse_error = "Unexpected token '" + check + "'";
						}
					}
					break;
				case DAG::CMD::DOT:
					parse_error = ParseDot(details);
					break;
				case DAG::CMD::NODE_STATUS_FILE:
					parse_error = ParseNodeStatus(details);
					break;
				case DAG::CMD::JOBSTATE_LOG:
					check = details.next(TRIM_QUOTES);
					if (check.empty()) {
						parse_error = "No include file specified";
					} else {
						data.reset(new JobStateLogCommand(check));
						check = details.next();
						if ( ! check.empty()) {
							parse_error = "Unexpected token '" + check + "'";
						}
					}
					break;
				case DAG::CMD::SAVE_POINT_FILE:
					parse_error = ParseSavePoint(details);
					break;
				case DAG::CMD::SET_JOB_ATTR:
					check = details.remain();
					trim(check);
					if (check.empty()) {
						parse_error = "No attribute line (key = value) provided";
					} else { data.reset(new SetAttrCommand(check)); }
					break;
				case DAG::CMD::ENV:
					parse_error = ParseEnv(details);
					break;
				case DAG::CMD::REJECT:
					check = details.next();
					if ( ! check.empty()) {
						parse_error = "Unexpected token '" + check + "'";
					} else { data.reset(new RejectCommand(GetFile(), line_no)); }
					break;
				case DAG::CMD::CONNECT:
					parse_error = ParseConnect(details);
					break;
				case DAG::CMD::PIN_IN:
				case DAG::CMD::PIN_OUT:
					parse_error = ParsePin(details, cmd);
					break;
				default:
					parse_error = "Parser not implemented";
			} // End switch statement on DAG commands

			// Handle line lexer error (overrides specific parse failure because this was likely the cause)
			if (details.failed()) {
				parse_error = details.error();
			}

			// Handle any errors encountered while parsing
			if ( ! parse_error.empty()) {
				data.reset(nullptr);
				parse_success = false;
				formatstr(err, "%s:%d Failed to parse %s command: %s",
				          GetFile().c_str(), command_line_no, cmd_str.c_str(), parse_error.c_str());

				all_errors.emplace_back(err);

				if (DAG::COMMAND_SYNTAX.contains(cmd)) {
					example_syntax = DAG::COMMAND_SYNTAX.at(cmd);
				} else { example_syntax = "No syntax provided"; }
			}
		} // End if/else valid DAG command

		return parse_success;
	} // End while loop

	// End of file
	return false;
}

