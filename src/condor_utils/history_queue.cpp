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

	std::string scanLimit;
	if (queryAd.EvaluateAttr("ScanLimit", value) && value.IsIntegerValue()) {
		unparser.Unparse(scanLimit, value);
	}

	std::string record_src;
	queryAd.EvaluateAttrString(ATTR_HISTORY_RECORD_SOURCE, record_src);

	std::string ad_type_filter;
	if ( ! queryAd.EvaluateAttrString(ATTR_HISTORY_AD_TYPE_FILTER, ad_type_filter)) {
		ad_type_filter.clear();
	}

	bool searchDir = false;
	if (!queryAd.EvaluateAttrBool("HistoryFromDir", searchDir)) {
		searchDir = false;
	}

	if (m_requests >= m_max_requests) {
		if (m_queue.size() > 1000) {
			return sendHistoryErrorAd(stream, 9, "Cowardly refusing to queue more than 1000 requests.");
		}
		classad_shared_ptr<Stream> stream_shared(stream);
		HistoryHelperState state(stream_shared, requirements_str, since_str, proj_str, match_limit, record_src);
		state.m_streamresults = streamresults;
		state.m_searchdir = searchDir;
		state.m_searchForwards = searchForwards;
		state.m_scanLimit = scanLimit;
		state.m_adTypeFilter = ad_type_filter;
		m_queue.push_back(state);
		return KEEP_STREAM;
	} else {
		HistoryHelperState state(*stream, requirements_str, since_str, proj_str, match_limit, record_src);
		state.m_streamresults = streamresults;
		state.m_searchdir = searchDir;
		state.m_searchForwards = searchForwards;
		state.m_scanLimit = scanLimit;
		state.m_adTypeFilter = ad_type_filter;
		return launcher(state);
	}
}

int HistoryHelperQueue::reaper(int, int) {
	m_requests--;
	while ((m_requests < m_max_requests) && m_queue.size() > 0) {
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
		args.AppendArg("condor_history_helper");
		args.AppendArg("-f");
		args.AppendArg("-t");
		// NOTE: before 8.4.8 and 8.5.6 the argument order was: requirements projection match max
		// starting with 8.4.8 and 8.5.6 the argument order was changed to: match max requirements projection
		// this change was made so that projection could be empty without causing problems on Windows.
		args.AppendArg(state.m_streamresults ? "true" : "false"); // new for 8.4.9
		args.AppendArg(state.MatchCount());
		args.AppendArg(std::to_string(param_integer("HISTORY_HELPER_MAX_HISTORY", 10000)));
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
		}
		if (strcasecmp(state.RecordSrc().c_str(),"JOB_EPOCH") == MATCH) {
			args.AppendArg("-epochs");
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
		args.AppendArg("-scanlimit");
		if (state.m_scanLimit.empty()) {
			args.AppendArg(std::to_string(param_integer("HISTORY_HELPER_MAX_HISTORY", 50000)));
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
		auto_free_ptr searchPath(param(searchKnob.c_str()));
		if (searchPath) {
			args.AppendArg("-search");
			args.AppendArg(searchPath);
		} else {
			std::string errmsg;
			formatstr(errmsg, "%s undefined in remote configuration. No such related history to be queried.", searchKnob.c_str());
			return sendHistoryErrorAd(state.GetStream(), 5, errmsg);
		}
		std::string myargs;
		args.GetArgsStringForLogging(myargs);
		dprintf(D_FULLDEBUG, "invoking %s %s\n", history_helper.ptr(), myargs.c_str());
	}

	Stream *inherit_list[] = {state.GetStream(), NULL};

	pid_t pid = daemonCore->Create_Process(history_helper.ptr(), args, PRIV_ROOT, m_rid,
		false, false, NULL, NULL, NULL, inherit_list);
	if (!pid) {
		return sendHistoryErrorAd(state.GetStream(), 4, "Failed to launch history helper process");
	}
	m_requests++;
	return true;
}
