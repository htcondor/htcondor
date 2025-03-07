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

#include "dag_commands.h"

const std::map<std::string, DAG::CMD, NoCaseCmp> DAG::KEYWORD_MAP = {
	{"JOB", DAG::CMD::JOB},
	{"FINAL", DAG::CMD::FINAL},
	{"PROVISIONER", DAG::CMD::PROVISIONER},
	{"SERVICE", DAG::CMD::SERVICE},
	{"SUBDAG", DAG::CMD::SUBDAG},
	{"SPLICE", DAG::CMD::SPLICE},
	{"INCLUDE", DAG::CMD::INCLUDE},
	{"SUBMIT_DESCRIPTION", DAG::CMD::SUBMIT_DESCRIPTION},
	{"CATEGORY", DAG::CMD::CATEGORY},
	{"PARENT", DAG::CMD::PARENT_CHILD},
	{"SCRIPT", DAG::CMD::SCRIPT},
	{"RETRY", DAG::CMD::RETRY},
	{"ABORT_DAG_ON", DAG::CMD::ABORT_DAG_ON},
	{"VARS", DAG::CMD::VARS},
	{"PRIORITY", DAG::CMD::PRIORITY},
	{"PRE_SKIP", DAG::CMD::PRE_SKIP},
	{"DONE", DAG::CMD::DONE},
	{"MAXJOBS", DAG::CMD::MAXJOBS},
	{"CONFIG", DAG::CMD::CONFIG},
	{"DOT", DAG::CMD::DOT},
	{"NODE_STATUS_FILE", DAG::CMD::NODE_STATUS_FILE},
	{"JOBSTATE_LOG", DAG::CMD::JOBSTATE_LOG},
	{"SAVE_POINT_FILE", DAG::CMD::SAVE_POINT_FILE},
	{"SET_JOB_ATTR", DAG::CMD::SET_JOB_ATTR},
	{"ENV", DAG::CMD::ENV},
	{"REJECT", DAG::CMD::REJECT},
	{"CONNECT", DAG::CMD::CONNECT},
	{"PIN_IN", DAG::CMD::PIN_IN},
	{"PIN_OUT", DAG::CMD::PIN_OUT},
};

const std::map<DAG::CMD, const char*> DAG::COMMAND_SYNTAX = {
	{DAG::CMD::JOB, "JOB <name> <submit description> [DIR <directory>] [NOOP] [DONE]"},
	{DAG::CMD::FINAL, "FINAL <name> <submit description> [DIR <directory>] [NOOP] [DONE]"},
	{DAG::CMD::PROVISIONER, "PROVISIONER <name> <submit description> [DIR <directory>] [NOOP] [DONE]"},
	{DAG::CMD::SERVICE, "SERVICE <name> <submit description> [DIR <directory>] [NOOP] [DONE]"},
	{DAG::CMD::SUBDAG, "SUBDAG EXTERNAL <name> <dag file> [DIR <directory>] [NOOP] [DONE]"},
	{DAG::CMD::SPLICE, "SPLICE <name> <dag file> [DIR <directory>]"},
	{DAG::CMD::INCLUDE, "INCLUDE <dag file>"},
	{DAG::CMD::SUBMIT_DESCRIPTION, "SUBMIT_DESCRIPTION <name> {\\n<inline description>\\n}"},
	{DAG::CMD::CATEGORY, "CATEGORY <node> <name>"},
	{DAG::CMD::PARENT_CHILD, "PARENT p1 [p2 p3 ...] CHILD c1 [c2 c3 ...]"},
	{DAG::CMD::SCRIPT, "SCRIPT [DEFER <status> <time>] [DEBUG <file> (STDOUT|STDERR|ALL)] (PRE|POST|HOLD) <node> <script> <arguments ...>"},
	{DAG::CMD::RETRY, "RETRY <node> <max retries> [UNLESS-EXIT <code>]"},
	{DAG::CMD::ABORT_DAG_ON, "ABORT_DAG_ON <node> <status> [RETURN <code>]"},
	{DAG::CMD::VARS, "VARS <node> [PREPEND|APPEND] key1=value1 [key2=value2 key3=value3 ...]"},
	{DAG::CMD::PRIORITY, "PRIORITY <node> <value>"},
	{DAG::CMD::PRE_SKIP, "PRE_SKIP <node> <status>"},
	{DAG::CMD::DONE, "DONE <node>"},
	{DAG::CMD::MAXJOBS, "MAXJOBS <category> <value>"},
	{DAG::CMD::CONFIG, "CONFIG <file>"},
	{DAG::CMD::DOT, "DOT <file> [UPDATE|DONT-UPDATE] [OVERWRITE|DONT-OVERWRITE] [INCLUDE <header file>]"},
	{DAG::CMD::NODE_STATUS_FILE, "NODE_STATUS_FILE <file> [<min update time>] [ALWAYS-UPDATE]"},
	{DAG::CMD::JOBSTATE_LOG, "JOBSTATE_LOG <file>"},
	{DAG::CMD::SAVE_POINT_FILE, "SAVE_POINT_FILE <node> [<file>]"},
	{DAG::CMD::SET_JOB_ATTR, "SET_JOB_ATTR <key> = <value>"},
	{DAG::CMD::ENV, "ENV (SET|GET) <environment variables>"},
	{DAG::CMD::REJECT, "REJECT"},
	{DAG::CMD::CONNECT, "CONNECT <splice 1> <splice 2>"},
	{DAG::CMD::PIN_IN, "PIN_IN <node> <pin number>"},
	{DAG::CMD::PIN_OUT, "PIN_OUT <node> <pin number>"},
};

const std::string DAG::ALL_NODES = "ALL_NODES";

const std::set<std::string, NoCaseCmp> DAG::RESERVED = {
	"PARENT",
	"CHILD",
	DAG::ALL_NODES,
};

const std::map<std::string, DAG::SCRIPT, NoCaseCmp> DAG::SCRIPT_TYPES_MAP {
	{"PRE", DAG::SCRIPT::PRE},
	{"POST", DAG::SCRIPT::POST},
	{"HOLD", DAG::SCRIPT::HOLD},
};

const std::map<std::string, DAG::ScriptOutput, NoCaseCmp> DAG::SCRIPT_DEBUG_MAP {
	{"STDOUT", DAG::ScriptOutput::STDOUT},
	{"STDERR", DAG::ScriptOutput::STDERR},
	{"ALL", DAG::ScriptOutput::ALL},
};

const char DAG::NEWLINE_RELACEMENT = '\x1F';

const char* DAG::GET_KEYWORD_STRING(const CMD command) {
	auto it = std::ranges::find_if(KEYWORD_MAP, [&command](const auto& pair) { return pair.second == command; });
	return (it == KEYWORD_MAP.end()) ? "UNKNOWN" : it->first.c_str();
}

const char* DAG::GET_SCRIPT_TYPE_STRING(const SCRIPT type) {
	auto it = std::ranges::find_if(SCRIPT_TYPES_MAP, [&type](const auto& pair) { return pair.second == type; });
	return (it == SCRIPT_TYPES_MAP.end()) ? "UNKNOWN" : it->first.c_str();
}

const char* DAG::GET_SCRIPT_DEBUG_CAPTURE_TYPE(const ScriptOutput type) {
	auto it = std::ranges::find_if(SCRIPT_DEBUG_MAP, [&type](const auto& pair) { return pair.second == type; });
	return (it == SCRIPT_DEBUG_MAP.end()) ? "NONE" : it->first.c_str();
}
