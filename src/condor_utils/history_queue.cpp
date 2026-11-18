/***************************************************************
 *
 * Copyright (C) 2020, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "classad_helpers.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "condor_io.h"
#include "history_queue.h"

#include <algorithm>
#include <functional>

static bool
sendHistoryErrorAd(Stream *stream, int errorCode, std::string errorString)
{
	ClassAd ad;
	ad.InsertAttr(ATTR_OWNER, 0);
	ad.InsertAttr(ATTR_ERROR_STRING, errorString);
	ad.InsertAttr(ATTR_ERROR_CODE, errorCode);

	stream->encode();
	if (!putClassAd(stream, ad) || !stream->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to send error ad for remote history query\n");
	}
	return false;
}


void HistoryHelperQueue::setup(int request_max, int concurrency_max) {
	m_max_requests = request_max;
	m_max_concurrency = concurrency_max;
	if (m_rid < 0) {
		m_rid = daemonCore->Register_Reaper("history_reaper",
			(ReaperHandlercpp)&HistoryHelperQueue::reaper,
			"HistoryHelperQueue::reaper", this);
	}
}


// Filter values from the query ad are passed to condor_history as
// command line arguments, so every value must be checked to ensure it
// can never be interpreted as a command line option.

static bool isDigits(const std::string &str) {
	return ! str.empty() && std::ranges::all_of(str, [](unsigned char c) { return isdigit(c); });
}

// Non-flag job spec: N, N.M, or a username (user or user@domain)
static bool isValidJobSpecToken(const std::string &token) {
	if (token.empty()) { return false; }

	size_t dot = token.find('.');
	if (dot != std::string::npos && isDigits(token.substr(0, dot)) && isDigits(token.substr(dot + 1))) {
		return true;
	}
	if (isDigits(token)) { return true; }

	// Usernames may contain any printable ASCII character except '"' and
	// '\\': condor_history puts the name unescaped into an Owner == "name"
	// constraint.
	if (token[0] == '-') { return false; }
	return std::ranges::all_of(token, [](unsigned char c) {
		return c >= 0x20 && c < 0x7f && c != '"' && c != '\\';
	});
}

// Batch ids are free form, but must not look like an option
static bool isValidBatchId(const std::string &token) {
	return ! token.empty() && token[0] != '-';
}

// Batch names are any non-empty string. condor_history always takes the
// argument following -batchname as its value, so it can never be read as an option.
static bool isValidBatchName(const std::string &token) {
	return ! token.empty();
}

// Extract a ClassAd list of strings from the query ad, validating each element.
// A missing attribute is an empty filter. Returns false and sets err on failure.
static bool getFilterList(ClassAd &queryAd, const char *attr, const std::function<bool(const std::string &)> &valid, std::vector<std::string> &out, std::string &err) {
	classad::ExprTree *expr = queryAd.Lookup(attr);
	if ( ! expr) { return true; }

	classad::Value value;
	classad::ExprList *list = nullptr;
	if ( ! queryAd.EvaluateExpr(expr, value) || ! value.IsListValue(list)) {
		formatstr(err, "%s must be a list of strings", attr);
		return false;
	}

	for (auto *item : *list) {
		std::string token;
		if ( ! ExprTreeIsLiteralString(item, token)) {
			formatstr(err, "%s must be a list of strings", attr);
			return false;
		}
		if ( ! valid(token)) {
			formatstr(err, "Invalid %s value '%s'", attr, token.c_str());
			return false;
		}
		out.push_back(std::move(token));
	}
	return true;
}

int HistoryHelperQueue::command_handler(int cmd, Stream* stream)
{
	bool is_startd = (cmd == GET_HISTORY);

	ClassAd queryAd;
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd(true);

	stream->decode();
	stream->timeout(15);
	if( !getClassAd(stream, queryAd) || !stream->end_of_message()) {
		dprintf( D_ALWAYS, "Failed to receive query on TCP: aborting\n" );
		return FALSE;
	}

	if (m_max_requests == 0 || m_max_concurrency == 0) {
		return sendHistoryErrorAd(stream, 10,
			is_startd 
				? "Remote history has been disabled on this startd"
				: "Remote history has been disabled on this schedd"
		);
	}

	std::string requirements_str;
	classad::ExprTree *requirements = queryAd.Lookup(ATTR_REQUIREMENTS);
	if (requirements) {
		unparser.Unparse(requirements_str, requirements);
	}

	classad::ExprTree * since_expr = queryAd.Lookup("Since");
	std::string since_str;
	if (since_expr) { unparser.Unparse(since_str, since_expr); }

	classad::Value value;
	classad::References projection;
	int proj_err = mergeProjectionFromQueryAd(queryAd, ATTR_PROJECTION, projection, true);
	if (proj_err < 0) {
		if (proj_err == -1) {
			return sendHistoryErrorAd(stream, 2, "Unable to evaluate projection list");
		}
		return sendHistoryErrorAd(stream, 3, "Unable to convert projection list to string list");
	}
	std::string proj_str;
	print_attrs(proj_str, false, projection, ",");

	std::string match_limit;
	if (queryAd.EvaluateAttr(ATTR_NUM_MATCHES, value) && value.IsIntegerValue()) {
		unparser.Unparse(match_limit, value);
	}

	bool streamresults = false;
	if (!queryAd.EvaluateAttrBool("StreamResults", streamresults)) {
		streamresults = false;
	}

	bool searchForwards;
	if ( ! queryAd.EvaluateAttrBool("HistoryReadForwards", searchForwards)) { searchForwards = false; }

	bool noLibrarian = false;
	if ( ! queryAd.EvaluateAttrBool(ATTR_HISTORY_NO_LIBRARIAN, noLibrarian)) { noLibrarian = false; }

	std::string scanLimit;
	if (queryAd.EvaluateAttr("ScanLimit", value) && value.IsIntegerValue()) {
		unparser.Unparse(scanLimit, value);
	}

	std::string record_src;
	queryAd.EvaluateAttrString(ATTR_HISTORY_RECORD_SOURCE, record_src);

	std::string daemon_subsys;
	queryAd.EvaluateAttrString(ATTR_DAEMON_HISTORY_SUBSYS, daemon_subsys);

	// Remote history does no assumptions for daemon history... expect explicit subsystem
	if (istring_view("daemon") == record_src.c_str() && daemon_subsys.empty()) {
		return sendHistoryErrorAd(stream, 6, "Specified daemon history source without subsystem.");
	}

	std::string ad_type_filter;
	if ( ! queryAd.EvaluateAttrString(ATTR_HISTORY_AD_TYPE_FILTER, ad_type_filter)) {
		ad_type_filter.clear();
	}

	bool searchDir = false;
	if (!queryAd.EvaluateAttrBool("HistoryFromDir", searchDir)) {
		searchDir = false;
	}

	// Verify user specified filters are valid to prevent command injection
	// i.e. user passes "-search" "path/to/secret/condor/ad/file" that is read and returned
	std::vector<std::string> jobspecs;
	std::vector<std::pair<std::string, std::string>> workflow_filters;
	std::string filter_err;
	if ( ! getFilterList(queryAd, ATTR_HISTORY_JOB_SPEC_FILTER, isValidJobSpecToken, jobspecs, filter_err)) {
		return sendHistoryErrorAd(stream, 12, filter_err);
	}

	const struct { const char *attr; const char *flag; std::function<bool(const std::string &)> valid; } workflow_attrs[] = {
		{ ATTR_HISTORY_DAG_ID_FILTER, "-dagid", isDigits },
		{ ATTR_HISTORY_BATCH_ID_FILTER, "-batchid", isValidBatchId },
		{ ATTR_HISTORY_BATCH_NAME_FILTER, "-batchname", isValidBatchName },
	};
	for (const auto &wf : workflow_attrs) {
		std::vector<std::string> values;
		if ( ! getFilterList(queryAd, wf.attr, wf.valid, values, filter_err)) {
			return sendHistoryErrorAd(stream, 12, filter_err);
		}
		for (auto &v : values) { workflow_filters.emplace_back(wf.flag, std::move(v)); }
	}

	if (m_requests >= m_max_concurrency) {
		if (m_queue.size() >= static_cast<size_t>(m_max_requests)) {
			return sendHistoryErrorAd(stream, 9, "Cowardly refusing to queue more than " + std::to_string(m_max_requests) + " requests.");
		}
		classad_shared_ptr<Stream> stream_shared(stream);
		HistoryHelperState state(stream_shared, requirements_str, since_str, proj_str, match_limit, record_src);
		state.m_streamresults = streamresults;
		state.m_searchdir = searchDir;
		state.m_searchForwards = searchForwards;
		state.m_noLibrarian = noLibrarian;
		state.m_scanLimit = scanLimit;
		state.m_adTypeFilter = ad_type_filter;
		state.m_daemonSubsys = daemon_subsys;
		state.m_jobspecs = std::move(jobspecs);
		state.m_workflowFilters = std::move(workflow_filters);
		m_queue.push_back(state);
		return KEEP_STREAM;
	} else {
		HistoryHelperState state(*stream, requirements_str, since_str, proj_str, match_limit, record_src);
		state.m_streamresults = streamresults;
		state.m_searchdir = searchDir;
		state.m_searchForwards = searchForwards;
		state.m_noLibrarian = noLibrarian;
		state.m_scanLimit = scanLimit;
		state.m_adTypeFilter = ad_type_filter;
		state.m_daemonSubsys = daemon_subsys;
		state.m_jobspecs = std::move(jobspecs);
		state.m_workflowFilters = std::move(workflow_filters);
		return launcher(state);
	}
}

int HistoryHelperQueue::reaper(int, int) {
	m_requests--;
	while ((m_requests < m_max_concurrency) && m_queue.size() > 0) {
		auto it = m_queue.begin();
		launcher(*it);
		m_queue.erase(it);
	}
	return TRUE;
}

int HistoryHelperQueue::launcher(const HistoryHelperState &state) {

	auto_free_ptr history_helper(param("HISTORY_HELPER"));
	if ( ! history_helper) {
	#ifdef WIN32
		history_helper.set(expand_param("$(BIN)\\condor_history.exe"));
	#else
		history_helper.set(expand_param("$(BIN)/condor_history"));
	#endif
	}
	ArgList args;
	if (m_allow_legacy_helper && strstr(history_helper.ptr(), "_helper")) {
		dprintf(D_ALWAYS, "Using obsolete condor_history_helper arguments\n");
		if (state.HasJobFilters()) {
			// The legacy helper has no way to apply these, so they are ignored
			dprintf(D_ALWAYS, "Ignoring job/user and DAG/batch filters not supported by condor_history_helper\n");
		}
		args.AppendArg("condor_history_helper");
		args.AppendArg("-f");
		args.AppendArg("-t");
		// NOTE: before 8.4.8 and 8.5.6 the argument order was: requirements projection match max
		// starting with 8.4.8 and 8.5.6 the argument order was changed to: match max requirements projection
		// this change was made so that projection could be empty without causing problems on Windows.
		args.AppendArg(state.m_streamresults ? "true" : "false"); // new for 8.4.9
		args.AppendArg(state.MatchCount());
		args.AppendArg(std::to_string(param_integer("HISTORY_HELPER_MAX_HISTORY", 2000000000)));
		args.AppendArg(state.Requirements());
		args.AppendArg(state.Projection());
		std::string myargs;
		args.GetArgsStringForLogging(myargs);
		dprintf(D_FULLDEBUG, "invoking %s %s\n", history_helper.ptr(), myargs.c_str());
	} else {
		// pass arguments in the format that condor_history wants
		args.AppendArg("condor_history");
		args.AppendArg("-inherit"); // tell it to write to an inherited socket
		// Specify history source for default Ad type filtering despite specifying files to search
		if (m_want_startd) {
			args.AppendArg("-startd");
		} else if (strcasecmp(state.RecordSrc().c_str(),"JOB_EPOCH") == MATCH) {
			args.AppendArg("-epochs");
		} else if (strcasecmp(state.RecordSrc().c_str(),"DAEMON") == MATCH) {
			std::string flag;
			formatstr(flag, "-daemon:%s", state.m_daemonSubsys.c_str());
			args.AppendArg(flag);
		}

		// End history source specification
		if (state.m_streamresults) { args.AppendArg("-stream-results"); }
		if ( ! state.MatchCount().empty()) {
			args.AppendArg("-match");
			args.AppendArg(state.MatchCount());
		}
		if (state.m_searchForwards) {
			args.AppendArg("-forwards");
		}
		if (state.m_noLibrarian) {
			args.AppendArg("-no-librarian");
		}
		args.AppendArg("-scanlimit");
		if (state.m_scanLimit.empty()) {
			args.AppendArg(std::to_string(param_integer("HISTORY_HELPER_MAX_HISTORY", 2000000000)));
		} else {
			args.AppendArg(state.m_scanLimit);
		}
		if ( ! state.Since().empty()) {
			args.AppendArg("-since");
			args.AppendArg(state.Since());
		}
		if ( ! state.Requirements().empty()) {
			args.AppendArg("-constraint");
			args.AppendArg(state.Requirements());
		}
		if ( ! state.Projection().empty()) {
			args.AppendArg("-attributes");
			args.AppendArg(state.Projection());
		}
		if ( ! state.m_adTypeFilter.empty()) {
			args.AppendArg("-type");
			args.AppendArg(state.m_adTypeFilter);
		}
		//Here we tell condor_history where to search for history files/directories
		std::string searchKnob = "HISTORY";
		if (state.m_searchdir) {
			searchKnob += "_DIR";
			args.AppendArg("-dir");
		}
		if ( ! state.RecordSrc().empty()) {
			searchKnob = state.RecordSrc() + "_" + searchKnob;
		}
		if ( ! state.m_daemonSubsys.empty()) {
			searchKnob = state.m_daemonSubsys + "_" + searchKnob;
		}
		auto_free_ptr searchPath(param(searchKnob.c_str()));
		if (searchPath) {
			args.AppendArg("-search");
			args.AppendArg(searchPath);
		} else {
			std::string errmsg;
			formatstr(errmsg, "%s undefined in remote configuration. No such related history to be queried.", searchKnob.c_str());
			return sendHistoryErrorAd(state.GetStream(), 5, errmsg);
		}

		for (const auto& jobspec : state.m_jobspecs) {
			args.AppendArg(jobspec);
		}

		for (const auto& [flag, value] : state.m_workflowFilters) {
			args.AppendArg(flag);
			args.AppendArg(value);
		}

		std::string myargs;
		args.GetArgsStringForLogging(myargs);
		if (myargs.size() > 100000) {
			std::string errmsg;
			formatstr(errmsg, "Arguments to remote history query are too long (%ld), try with shorter constraint and/or projection", myargs.size());
			dprintf(D_ALWAYS, "%s\n", errmsg.c_str());
			return sendHistoryErrorAd(state.GetStream(), 6, errmsg);
		}
		dprintf(D_FULLDEBUG, "invoking %s %s\n", history_helper.ptr(), myargs.c_str());
	}

	Stream *inherit_list[] = {state.GetStream(), NULL};

	OptionalCreateProcessArgs cpArgs;
	pid_t pid = daemonCore->CreateProcessNew(history_helper.ptr(), args,
		cpArgs.priv(PRIV_CONDOR_FINAL)
			.reaperID(m_rid)
			.socketInheritList(inherit_list));
	if (!pid) {
		return sendHistoryErrorAd(state.GetStream(), 4, "Failed to launch history helper process");
	}
	m_requests++;
	return true;
}
