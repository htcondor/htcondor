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

DaemonList::DaemonList()
{
}


DaemonList::~DaemonList( void )
{
	Daemon* tmp;
	list.Rewind();
	while( list.Next(tmp) ) {
		delete tmp;
	}
}


void
DaemonList::init( daemon_t type, const char* host_list, const char* pool_list )
{
	Daemon* tmp;
	char* host;
	char const *pool = NULL;
	StringList foo;
	StringList pools;
	if( host_list ) {
		foo.initializeFromString( host_list );
		foo.rewind();
	}
	if( pool_list ) {
		pools.initializeFromString( pool_list );
		pools.rewind();
	}
	while( true ) {
		host = foo.next();
		pool = pools.next();
		if( !host && !pool ) {
			break;
		}
		tmp = buildDaemon( type, host, pool );
		append( tmp );
	}
}


bool
DaemonList::shouldTryTokenRequest()
{
	list.Rewind();
	Daemon *daemon = nullptr;
	bool try_token_request = false;
	while( list.Next(daemon) ) {
		try_token_request |= daemon->shouldTryTokenRequest();
	}
	return try_token_request;
}


Daemon*
DaemonList::buildDaemon( daemon_t type, const char* host, char const *pool )
{
	Daemon* tmp;
	switch( type ) {
	case DT_COLLECTOR:
		tmp = new DCCollector( host );
		break;
	default:
		tmp = new Daemon( type, host, pool );
		break;
	}
	return tmp;
}


/*************************************************************
 ** SimpleList API
 ************************************************************/


bool
DaemonList::append( Daemon* d) { return list.Append(d); }

bool
DaemonList::Append( Daemon* d) { return list.Append(d); }

bool
DaemonList::isEmpty( void ) { return list.IsEmpty(); } 

bool
DaemonList::IsEmpty( void ) { return list.IsEmpty(); } 

int
DaemonList::number( void ) { return list.Number(); } 

int
DaemonList::Number( void ) { return list.Number(); } 

void
DaemonList::rewind( void ) { list.Rewind(); } 

void
DaemonList::Rewind( void ) { list.Rewind(); } 

bool
DaemonList::current( Daemon* & d ) { return list.Current(d); } 

bool
DaemonList::Current( Daemon* & d ) { return list.Current(d); } 

bool
DaemonList::next( Daemon* & d ) { return list.Next(d); } 

bool
DaemonList::Next( Daemon* & d ) { return list.Next(d); } 

bool
DaemonList::atEnd() { return list.AtEnd(); } 

bool
DaemonList::AtEnd() { return list.AtEnd(); } 

void
DaemonList::deleteCurrent() { this->DeleteCurrent(); }

/*
  NOTE: SimpleList does NOT delete the Daemon objects themselves,
  DeleteCurrent() is only going to delete the pointer itself.  Since
  we're responsible for all this memory (we create the Daemon objects 
  in DaemonList::init() and DaemonList::buildDaemon()), we have to be
  responsbile to deallocate it when we're done.  We're already doing
  this correctly in the DaemonList destructor, and we have to do it
  here in the DeleteCurrent() interface, too.
*/
void
DaemonList::DeleteCurrent() {
	Daemon* cur = NULL;
	if( list.Current(cur) && cur ) {
		delete cur;
	}
	list.DeleteCurrent();
}


CollectorList::CollectorList(DCCollectorAdSequences * adseq /*=NULL*/)
	: adSeq(adseq) {
}

CollectorList::~CollectorList() {
	if (adSeq) { delete adSeq; adSeq = NULL; }
}


CollectorList *
CollectorList::create(const char * pool, DCCollectorAdSequences * adseq)
{
	CollectorList * result = new CollectorList(adseq);
	DCCollector * collector = NULL;

		// Read the new names from config file or use the given parameter
	StringList collector_name_list;
	char * collector_name_param = NULL;
	collector_name_param = pool ? strdup(pool) : getCmHostFromConfig( "COLLECTOR" );
	if( collector_name_param ) {
		collector_name_list.initializeFromString(collector_name_param);
	
			// Create collector objects
		collector_name_list.rewind();
		char * collector_name = NULL;
		while ((collector_name = collector_name_list.next()) != NULL) {
			collector = new DCCollector (collector_name);
			result->append (collector);
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


		// First, pick out collector(s) that is on this host
	Daemon *daemon;
	SimpleList<Daemon*> prefer_list;
	this->list.Rewind();
	while ( this->list.Next(daemon) ) {
		if ( same_host (preferred_collector, daemon->fullHostname()) ) {
			this->list.DeleteCurrent();
			prefer_list.Prepend( daemon );
		}
	}

		// Walk through the list of preferred collectors,
		// stuff 'em in the main "list"
	this->list.Rewind();
	prefer_list.Rewind();
	while ( prefer_list.Next(daemon) ) {
		this->list.Prepend( daemon );
	}
	
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
	DCCollectorAdSeq * seqgen = adSeq->getAdSeq(*ad1);
	if (seqgen) { seqgen->advance(now); }

	this->rewind();
	DCCollector * daemon;
	while (this->next(daemon)) {
		dprintf( D_FULLDEBUG, 
				 "Trying to update collector %s\n", 
				 daemon->addr() );
		void *data = nullptr;
		if (token_requester && daemon->name()) {
			data = token_requester->createCallbackData(daemon->name(),
				identity, authz_name);
		}
		if( daemon->sendUpdate(cmd, ad1, *adSeq, ad2, nonblocking,
			DCTokenRequester::daemonUpdateCallback, data) )
		{
			success_count++;
		} 
	}

	return success_count;
}

QueryResult
CollectorList::query (CondorQuery & cQuery, bool (*callback)(void*, ClassAd *), void* pv, CondorError * errstack) {

	int num_collectors = this->number();
	if (num_collectors < 1) {
		return Q_NO_COLLECTOR_HOST;
	}

	std::vector<DCCollector *> vCollectors;
	DCCollector * daemon;
	QueryResult result = Q_COMMUNICATION_ERROR;

	bool problems_resolving = false;
	bool random_order = ! param_boolean("HAD_USE_PRIMARY", false);

	// switch containers for easier random access.
	this->rewind();
	while (this->next(daemon)) {
		vCollectors.push_back(daemon);
	}

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


bool
CollectorList::next( DCCollector* & d )
{
	return DaemonList::Next( (Daemon*&)d );
}


bool
CollectorList::Next( DCCollector* & d )
{
	return next( d );
}


bool
CollectorList::next( Daemon* & d )
{
	return DaemonList::Next( d );
}


bool
CollectorList::Next( Daemon* & d )
{
	return next( d );
}
