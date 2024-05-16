/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


#ifndef _CONDOR_DAEMON_LIST_H
#define _CONDOR_DAEMON_LIST_H

#include "condor_common.h"
#include "daemon.h"
#include "condor_classad.h"
#include "condor_query.h"


class DCTokenRequester;
class DCCollector;
class CondorQuery;

class DCCollectorAdSequences;

class CollectorList {
 public:
	CollectorList(DCCollectorAdSequences * adseq=NULL);
	virtual ~CollectorList();

		// Create the list of collectors for the pool
		// based on configruation settings
	static CollectorList * create(const char * pool = NULL, DCCollectorAdSequences * adseq = NULL);

		// Resort a collector list for locality (for negotiator)
	int resortLocal( const char *preferred_collector );

		// Send updates to all the collectors
		// return - number of successfull updates
	int sendUpdates (int cmd, ClassAd* ad1, ClassAd* ad2, bool nonblocking,
		DCTokenRequester *token_requester = nullptr, const std::string &identity = "",
		const std::string authz_name = "");

	void checkVersionBeforeSendingUpdates(bool check);

	std::vector<DCCollector*>& getList() { return m_list; }

		// use this to detach the ad sequence counters before destroying the collector list
		// we do this when we want to move the sequence counters to a new list
	DCCollectorAdSequences * detachAdSequences() { DCCollectorAdSequences * p = adSeq; adSeq = NULL; return p; }

	bool hasAdSeq() { return adSeq != NULL; }
	DCCollectorAdSequences & getAdSeq();
	
		// Try querying all the collectors until you get a good one
	QueryResult query (CondorQuery & cQuery, bool (*callback)(void*, ClassAd *), void* pv, CondorError * errstack = 0);

		// a common case is just wanting a list of ads back, so provide a ready-made callback that does that...
	static bool fetchAds_callback(void* pv, ClassAd * ad) {
		ClassAdList * padList = (ClassAdList *)pv;
		padList->Insert (ad);
		return false;
	}
	QueryResult query (CondorQuery & cQuery, ClassAdList & adList, CondorError *errstack = 0) {
		return query(cQuery, fetchAds_callback, &adList, errstack);
	}

private:
	std::vector<DCCollector*> m_list;
	DCCollectorAdSequences * adSeq;
};



#endif /* _CONDOR_DAEMON_LIST_H */
