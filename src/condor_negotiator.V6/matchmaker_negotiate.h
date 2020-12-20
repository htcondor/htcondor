/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#ifndef _MATCHMAKER_NEGOTIATE_H
#define _MATCHMAKER_NEGOTIATE_H

#include <deque>

class ResourceRequestList {

 public:
	ResourceRequestList(int protocol_version);
	~ResourceRequestList();

		// returns true if a request was found, else false.
		// pass in a sock to use, function then returns request,
		// cluster, proc, and autocluster.
	bool getRequest(ClassAd &request, int &cluster, int &proc, 
			int &autocluster, ReliSock* const sock, int skipJobs = 1);

		//
	bool hadError() const { return errcode > 0; }

		//
	int getErrorCode() const { return errcode; }

		// 
	void noMatchFound();

	bool needsEndNegotiate() const;
	bool needsEndNegotiateNow() const;

		//
	void clearRejectedAutoclusters() { m_clear_rejected_autoclusters = true; }

	enum TryStates {
		RRL_DONE,
		RRL_NO_MORE_JOBS,
		RRL_ERROR,
		RRL_CONTINUE
	};
	TryStates tryRetrieve(ReliSock* const sock);

 private:

	TryStates fetchRequestsFromSchedd(ReliSock* const sock, bool blocking);

	int m_protocol_version;
	bool m_send_end_negotiate;
	bool m_send_end_negotiate_now;
	bool m_use_resource_request_counts;
	bool m_clear_rejected_autoclusters;
	int m_requests_to_fetch;
	int m_num_to_fetch;
	int errcode;
	int current_autocluster;
	ClassAd cached_resource_request;
	int resource_request_count;
	int resource_request_offers;
	std::deque<ClassAd *> m_ads;
	std::set<int> m_rejected_auto_clusters;
};

#endif
