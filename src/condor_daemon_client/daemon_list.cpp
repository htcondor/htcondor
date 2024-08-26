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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "daemon_list.h"
#include "dc_collector.h"
#include "internet.h"
#include "ipv6_hostname.h"
#include "condor_daemon_core.h"

#include <vector>
#include <algorithm>


CollectorList::CollectorList(DCCollectorAdSequences * adseq /*=NULL*/)
	: adSeq(adseq) {
}

CollectorList::~CollectorList() {
	for (auto& col : m_list) {
		delete col;
	}
	if (adSeq) { delete adSeq; adSeq = NULL; }
}


CollectorList *
CollectorList::create(const char * pool, DCCollectorAdSequences * adseq)
{
	CollectorList * result = new CollectorList(adseq);

		// Read the new names from config file or use the given parameter
	char * collector_name_param = NULL;
	collector_name_param = (pool && *pool) ? strdup(pool) : getCmHostFromConfig( "COLLECTOR" );
	if( collector_name_param ) {
			// Create collector objects
		for (const auto& collector_name : StringTokenIterator(collector_name_param)) {
			result->m_list.emplace_back(new DCCollector(collector_name.c_str()));
		}
	} else {
			// Otherwise, just return an empty list
		dprintf( D_ALWAYS, "Warning: Collector information was not found in the configuration file. ClassAds will not be sent to the collector and this daemon will not join a larger Condor pool.\n");
	}
	if( collector_name_param ) {
		free( collector_name_param );
	}
	return result;
}


/***
 * 
 * Resort a collector list for locality;
 * prioritize the collector that is best suited for the negotiator
 * running on this machine. This minimizes the delay for fetching
 * ads from the Collector by the Negotiator, which some people
 * consider more critical.
 *
 ***/

int
CollectorList::resortLocal( const char *preferred_collector )
{
		// Find the collector in the list that is best suited for 
		// this host. This is determined either by
		// a) preferred_collector passed in
        // b) the collector that has the same hostname as this negotiator
	char * tmp_preferred_collector = NULL;

	if ( !preferred_collector ) {
        // figure out our hostname for plan b) above
		auto _hostname_str = get_local_fqdn();
		const char * _hostname = _hostname_str.c_str();
		if (!(*_hostname)) {
				// Can't get our hostname??? fuck off
			return -1;
		}

		tmp_preferred_collector = strdup(_hostname);
		preferred_collector = tmp_preferred_collector; // So we know to free later
	}


		// sort collector(s) that are on this host to be first
	std::sort(m_list.begin(), m_list.end(),
			[&](Daemon* a, Daemon* b) {
				return same_host(preferred_collector, a->fullHostname()) &&
				       !same_host(preferred_collector, b->fullHostname());
			});
	
	free(tmp_preferred_collector); // Warning, preferred_collector (may have) just became invalid, so do this just before returning.
	return 0;
}

// return a ref to the collection of ad sequence counters, creating it if necessary
DCCollectorAdSequences & CollectorList::getAdSeq()
{
	if ( ! adSeq) adSeq = new DCCollectorAdSequences();
	return *adSeq;
}

int
CollectorList::sendUpdates (int cmd, ClassAd * ad1, ClassAd* ad2, bool nonblocking,
	DCTokenRequester *token_requester, const std::string &identity,
	const std::string authz_name)
{
	int success_count = 0;

	if ( ! adSeq) {
		adSeq = new DCCollectorAdSequences();
	}

	// advance the sequence numbers for these ads
	//
	time_t now = time(NULL);
	adSeq->getAdSeq(*ad1).advance(now);

	size_t num_collectors = m_list.size();
	for (auto& daemon : m_list) {
		if (!daemon->addr()) {
			dprintf(D_ALWAYS, "Can't resolve collector %s; skipping update\n",
					daemon->name() ? daemon->name() : "without a name(?)");
			continue;
		}

		if ((num_collectors > 1) && daemon->isBlacklisted()) {
			dprintf(D_ALWAYS, "Skipping update to collector %s which has timed out in the past\n", daemon->addr());
			continue;
		}
		dprintf( D_FULLDEBUG, 
				 "Trying to update collector %s\n", 
				 daemon->addr() );
		void *data = nullptr;
		if (token_requester && daemon->name()) {
			data = token_requester->createCallbackData(daemon->name(),
				identity, authz_name);
		}

		if( num_collectors > 1 ) {
			daemon->blacklistMonitorQueryStarted();
		}

		bool success = daemon->sendUpdate(cmd, ad1, *adSeq, ad2, nonblocking,
			DCTokenRequester::daemonUpdateCallback, data);

		if( num_collectors > 1 ) {
			daemon->blacklistMonitorQueryFinished(success);
		}

		if (success)
		{
			success_count++;
		}
	}

	return success_count;
}

// pass flag down to the individual DCCollector objects
void CollectorList::checkVersionBeforeSendingUpdates(bool check) {
	for (auto * dcc : m_list) { if (dcc) dcc->checkVersionBeforeSendingUpdate(check); }
}

QueryResult
CollectorList::query (CondorQuery & cQuery, bool (*callback)(void*, ClassAd *), void* pv, CondorError * errstack) {

	size_t num_collectors = m_list.size();
	if (num_collectors < 1) {
		return Q_NO_COLLECTOR_HOST;
	}

	// Make a new list that we can erase items from
	std::vector<DCCollector *> vCollectors = m_list;
	DCCollector * daemon;
	QueryResult result = Q_COMMUNICATION_ERROR;

	bool problems_resolving = false;
	bool random_order = ! param_boolean("HAD_USE_PRIMARY", false);

	while ( vCollectors.size() ) {
		// choose a random collector in the list to query.
		unsigned int idx = random_order ? (get_random_int_insecure() % vCollectors.size()) : 0;
		daemon = vCollectors[idx];

		if ( ! daemon->addr() ) {
			if ( daemon->name() ) {
				dprintf( D_ALWAYS,
						 "Can't resolve collector %s; skipping\n",
						 daemon->name() );
			} else {
				dprintf( D_ALWAYS,
						 "Can't resolve nameless collector; skipping\n" );
			}
			problems_resolving = true;
		} else if ( daemon->isBlacklisted() && vCollectors.size() > 1 ) {
			dprintf( D_ALWAYS,"Collector %s blacklisted; skipping\n",
					 daemon->name() );
		} else {
			dprintf (D_FULLDEBUG,
					 "Trying to query collector %s\n",
					 daemon->addr());

			if( num_collectors > 1 ) {
				daemon->blacklistMonitorQueryStarted();
			}

			result = cQuery.processAds (callback, pv, daemon->addr(), errstack);

			if( num_collectors > 1 ) {
				daemon->blacklistMonitorQueryFinished( result == Q_OK );
			}

			if (result == Q_OK) {
				return result;
			}
		}

		// if you got here remove it from the list of potential candidates.
		vCollectors.erase( vCollectors.begin()+idx );
	}

	// only push an error if the error stack exists and is currently empty
	if(problems_resolving && errstack && !errstack->code(0)) {
		char* tmplist = getCmHostFromConfig( "COLLECTOR" );
		errstack->pushf("CONDOR_STATUS",1,"Unable to resolve COLLECTOR_HOST (%s).",tmplist?tmplist:"(null)");
	}

		// If we've gotten here, there are no good collectors
	return result;
}
