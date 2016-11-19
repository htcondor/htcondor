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

#ifndef _CLERK_H_
#define _CLERK_H_

#include "condor_classad.h"
#include "condor_commands.h"
//#include "totals.h"
#include "forkwork.h"
#include "clerk_stats.h"
#include "clerk_utils.h"
#include "clerk_collect.h"

// these are set based on whether we have been started as the main collector or not.
extern bool ImTheCollector;

#define MY_SUBSYSTEM lookup_macro("SUBSYS", ClerkVars, ClerkEvalCtx)

class PopulateAdsInfo {
public:
	PopulateAdsInfo(int op, AdTypes adt, const char * _source, bool _is_command=false, const char * _filter=NULL)
		: prev(this)
		, next(this)
		, source(_source)
		, filter(_filter)
		, is_command(_is_command)
		, is_waiting(true)
		, is_complete(false)
		, collect_op(op)
		, adtype(adt)
		, iter(NULL)
	{}

	~PopulateAdsInfo() { Close(0); }
	void Close(int err) { 
		if (iter) { if (err) is_failed = true; else is_complete = true; }
		delete iter; iter = NULL;
	}

	CollectionIterator<ClassAd*>* Open() {
		Close(0);
		iter = clerk_get_file_iter(source.c_str(), is_command, filter.empty() ? NULL : filter.StrDup());
		if ( ! iter) { is_failed = true; is_waiting = false; }
		else { is_failed = is_complete = is_waiting = false; }
		return iter;
	}

	ClassAd * Next() {
		if ( ! iter && is_waiting) Open();
		if ( ! iter) return NULL;
		ClassAd * ad = iter->Next();
		if ( ! ad) { Close(0); }
		return ad;
	}

	int collect(ClassAd *ad) {
		return ::collect(collect_op, adtype, ad, NULL);
	}

	bool Idle() { return is_waiting; }
	bool Running() { return ! is_waiting && ! is_complete; }
	bool Done() { return is_complete || is_failed; }
	bool Failed() { return is_failed; }
	const char * Source() { return source.c_str(); }
	AdTypes WhichAds() { return adtype; }

	void push_back(PopulateAdsInfo * that) {
		if ( ! this->next) { this->next = this->prev = this; }
		that->next = this;
		that->prev = this->prev;
		that->prev->next = that;
		this->prev = that;
	}

	static PopulateAdsInfo * remove_head(PopulateAdsInfo * & head) {
		if ( ! head) return NULL;
		PopulateAdsInfo * aa = head;
		PopulateAdsInfo * bb = aa->next;
		if ( ! bb || bb == aa) {
			bb = NULL;
		} else {
			bb->prev = aa->prev;
			aa->prev->next = bb;
		}
		head = bb;
		return aa;
	}

protected:
	PopulateAdsInfo *prev, *next; // so we can link these into a list
	std::string source; // the filename we would be using. ptr is not owned here.
	MyString filter;
	bool is_command;
	bool is_waiting;
	bool is_complete;
	bool is_failed;
	int collect_op;
	AdTypes adtype;
	CollectionIterator<ClassAd*>* iter;
};

class CmDaemon : public Service
{
public:
	CmDaemon();
	~CmDaemon();

	// methods needed by dc_main
	void Init();   // called on startup
	void Config(); // called on startup and on reconfig
	void Shutdown(bool fast);

	// command handlers
	int receive_update(int, Stream*);
	int receive_update_expect_ack(int, Stream*);
	int receive_invalidation(int, Stream*);
	int receive_query(int, Stream*);

	// timer handlers
	int update_collector(); // send normal daemon updates to the collector.  called when ! ImTheCollector
	int send_collector_ad(); // send collector ad to upstream collectors, called when ImTheCollector
	int populate_collector(); // populate the collectors tables from persist storage

private:
	bool m_initialized;
	MyString m_daemonName;
	ClassAd m_daemonAd;
	CmStatistics stats;
	CollectorList* m_collectorsToUpdate;
	int m_idTimerSendUpdates;  // the id of the timer for sending updates upstream.
	int m_idTimerPopulate;     // the id of the timer for populating the collector from persist

	// fields used by populate-from-file code
	PopulateAdsInfo * m_popList;
	void append_populate(PopulateAdsInfo * popAds) { if (m_popList) m_popList->push_back(popAds); else m_popList = popAds; }

	// cached param values
	int m_UPDATE_INTERVAL;      // the rate at which we sent daemon ad updates upstream
	int m_CLIENT_TIMEOUT;
	int m_QUERY_TIMEOUT;
	int m_CLASSAD_LIFETIME;
	bool m_LOG_UPDATES;
	bool m_IGNORE_INVALIDATE;
	bool m_EXPIRE_INVALIDATED_ADS;

	int FetchAds (AdTypes whichAds, ClassAd & query, List<ClassAd>& ads, int * pcTotalAds);
	int put_ad_v1(ClassAd *curr_ad, Stream* sock, ClassAd & query, bool v0);
	int put_ad_v2(ClassAd &ad, Stream* sock, ClassAd & query);
	int put_ad_v3(ClassAd &ad, Stream* sock, const classad::References * projection);

		// register the socket to use with subsequent updates.
	int StashSocket(ReliSock* sock);

};

// lookup ad type from a command ad
AdTypes CollectorCmdToAdType(int cmd);

// this holds variables used by the clerk, that are not params
extern MACRO_SET ClerkVars;

#endif
