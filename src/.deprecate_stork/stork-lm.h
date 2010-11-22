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

#ifndef _STORK_LM_
#define _STORK_LM_

/* #define TEST_VERSION 1 */

#ifndef TEST_VERSION
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#endif

#include "dc_lease_manager.h"
#include "MyString.h"
#include "HashTable.h"

// Turn on/off the use of ordered sets
#define USING_ORDERED_SET
#if defined( USING_ORDERED_SET )
# include "OrderedSet.h"
#else
# include "Set.h"
#endif

class StorkLeaseManager; // forward reference

class StorkLeaseEntry {
	friend class StorkLeaseManager;
	public:
		StorkLeaseEntry();
		StorkLeaseEntry(DCLeaseManagerLease * maker_lease);
		StorkLeaseEntry(const char* url);
		~StorkLeaseEntry();

		const char* GetUrl() { return Url; };

		bool ReleaseLeaseWhenDone();
		time_t GetExpirationTime() {return Expiration_time; };
		
		int operator< (const StorkLeaseEntry& E2);
		int operator== (const StorkLeaseEntry& E2);

	protected:
		void initialize();
		time_t Expiration_time;
		time_t IdleExpiration_time;
		char *Url;
		DCLeaseManagerLease* Lease;
		MyString *CompletePath;
		unsigned int num_matched;

		
};

class StorkMatchStatsEntry {
	public:
		StorkMatchStatsEntry();
		unsigned int curr_busy_matches;
		unsigned int total_busy_matches;
		unsigned int successes;
		unsigned int failures;

		unsigned int curr_idle_matches;   // only in global
};


// Stork interface object to new "leasemanager maker" for SC2005 demo.
class StorkLeaseManager 
#ifndef TEST_VERSION
	: public Service
#endif
{
	public:
		// Constructor.  For now, I'll assume there are no args to the
		// constructor.  Perhaps later we'll decide to specify a leasemanager to
		// connect to.  Stork calls the constructor at daemon startup time.
		StorkLeaseManager(void);

		// Destructor.  Stork calls the destructor at daemon shutdown time.
		~StorkLeaseManager(void);

		// Get a dynamic transfer destination from the leasemanager by protocol,
		// e.g. "gsiftp", "http", "file", etc.  This method returns a
		// destination URL of the format "proto://host/dir/".  This means a
		// transfer directory has been created on the destination host.
		// Function returns a pointer to dynamically allocated string, which
		// must be free()'ed by the caller after use.
		// Return NULL if there are no destinations available.
		const char * getTransferDirectory(const char *protocol);

		// WARNING:  This method not yet tested!
		// Get a dynamic transfer destination from the leasemanager by protocol,
		// e.g. "gsiftp", "http", "file", etc.  This method returns a
		// destination URL of the format "proto://host/dir/file".  This means a
		// transfer directory has been created on the destination host.
		// Function returns a pointer to dynamically allocated string, which
		// must be free()'ed by the caller after use.
		// Return NULL if there are no destinations available.
		const char * getTransferFile(const char *protocol); // NOT TESTED!

		// Return a dynamic transfer destination to the leasemanager.  Stork
		// calls this method when it is no longer using the transfer
		// destination.  LeaseManager returns false upon error.
		bool returnTransferDestination(const char * url, bool successful_transfer = true);

		// Inform the leasemanager that a dynamic transfer destination has
		// failed.  LeaseManager returns false upon error.
		bool failTransferDestination(const char * url);

		// Returns false if no matches are avail, else true
		bool areMatchesAvailable();

		bool getStats(const char *url);
		bool getStatsTotal();

	protected:
		bool getMatchesFromLeaseManager();
		StorkLeaseEntry * getTransferDestination(const char *protocol);
		bool destroyFromBusy(StorkLeaseEntry * match);
		bool destroyFromIdle(StorkLeaseEntry * match);
		bool addToBusySet(StorkLeaseEntry * match);
		bool addToIdleSet(StorkLeaseEntry * match);
		bool fromBusyToIdle(StorkLeaseEntry * match);
		void SetTimers();
		void timeout();

	private:
#if defined( USING_ORDERED_SET )
		OrderedSet<StorkLeaseEntry*> busyMatches;
		OrderedSet<StorkLeaseEntry*> idleMatches;
#else
		Set<StorkLeaseEntry*> busyMatches;
		Set<StorkLeaseEntry*> idleMatches;
#endif
		char *lm_name;
		char *lm_pool;
		int tid, tid_interval;
		//HashTable<MyString, StorkMatchStatsEntry*> matchStats(200,MyStringHash,rejectDuplicateKeys);
		HashTable<MyString, StorkMatchStatsEntry*> *matchStats;
		StorkMatchStatsEntry totalStats;
		bool want_rr;	// order the set for round-robin matches

}; // class StorkLeaseManager

#endif // _STORK_LM_

