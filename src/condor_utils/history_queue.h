/***************************************************************
 *
 * Copyright (C) 1990-2020, Condor Team, Computer Sciences Department,
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


#ifndef _HISTORY_QUEUE_H_
#define _HISTORY_QUEUE_H_

#include <deque>

class HistoryHelperState
{
public:
	HistoryHelperState(Stream &stream, const std::string &reqs, const std::string &since, const std::string &proj, const std::string &match, const std::string &source)
		: m_stream_ptr(&stream), m_reqs(reqs), m_since(since), m_proj(proj), m_match(match), m_recordSrc(source)
	{}

	HistoryHelperState(classad_shared_ptr<Stream> stream, const std::string &reqs, const std::string &since, const std::string &proj, const std::string &match, const std::string &source)
		: m_stream_ptr(nullptr), m_reqs(reqs), m_since(since), m_proj(proj), m_match(match), m_recordSrc(source), m_stream(stream)
	{}

	~HistoryHelperState() { if (m_stream.get() && (m_stream.use_count() == 1)) daemonCore->Cancel_Socket(m_stream.get()); }

	Stream * GetStream() const { return m_stream_ptr ? m_stream_ptr : m_stream.get(); }

	const std::string & Requirements() const { return m_reqs; }
	const std::string & Since() const { return m_since; }
	const std::string & Projection() const { return m_proj; }
	const std::string & MatchCount() const { return m_match; }
	const std::string & RecordSrc() const { return m_recordSrc; }
	std::string m_adTypeFilter{};
	std::string m_scanLimit{};
	bool m_streamresults{false};
	bool m_searchdir{false};
	bool m_searchForwards{false};

private:
	Stream *m_stream_ptr;
	std::string m_reqs;
	std::string m_since;
	std::string m_proj;
	std::string m_match;
	std::string m_recordSrc;
	classad_shared_ptr<Stream> m_stream;
};

class HistoryHelperQueue : public Service
{
public:
	HistoryHelperQueue(bool legacy=false) 
		: m_requests(0)
		, m_max_requests(0)       // 0 disables the queue
		, m_max_concurrency(0) // 0 disables the queue
		, m_rid(-1)            // reaper id
		, m_allow_legacy_helper(legacy) // when true, the Schedd's old history helper is allowed
		, m_want_startd(false)
		{
	}
	~HistoryHelperQueue() {};

	// establish limits and register the reaper
	void setup(int request_max, int concurrency_max);
	void want_startd_history(bool want) { m_want_startd = want; }

	int command_handler(int cmd, Stream* stream);

protected:
	int launcher(const HistoryHelperState &state);
	int reaper(int, int);

	std::deque<HistoryHelperState> m_queue;
	int m_requests;         // number of incomplete requests (queued + concurrent)
	int m_max_requests;     // max number of incomplete requests at any time, set to 0 to disable this command
	int m_max_concurrency;  // max number in flight at any one time
	int m_rid;              // reaper id
	bool m_allow_legacy_helper;
	bool m_want_startd;
};


#endif 	// _GET_HISTORY_CMD_H_
