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


#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "stork-lm.h"
#include "basename.h"
#include "condor_debug.h"
#include "condor_config.h"

// globus-url-copy requires any url specifying a directory must end with /.
#define IS_DIRECTORY(_url)  ( (_url)[(strlen(_url) - 1)] == '/' )

// Stork interface object to new "lease manager maker" for SC2005 demo.

// Instantiate some templates
#if defined( USING_ORDERED_SET )
  template class OrderedSet<StorkLeaseEntry*>;
#endif


StorkMatchStatsEntry::StorkMatchStatsEntry(void)
{
	curr_busy_matches = 0;
	curr_idle_matches = 0;
	total_busy_matches = 0;
	successes = 0;
	failures = 0;
}

void
StorkLeaseEntry::initialize(void)
{
	Url = NULL;
	Lease = NULL;
	Expiration_time = 0;
	IdleExpiration_time = 0;
	CompletePath = NULL;
	num_matched = 0;
}


StorkLeaseEntry::StorkLeaseEntry(void)
{
	initialize();
}

StorkLeaseEntry::StorkLeaseEntry(DCLeaseManagerLease * maker_lease)
{
	initialize();
	
	// init with lease ad
	ASSERT(maker_lease);
	Lease = maker_lease;

	// lookup Url here
	classad::ClassAd *ad = Lease->leaseAd();
	ASSERT(ad);
	string dest;
	char *attr_name = param("STORK_LM_DEST_ATTRNAME");
	if ( !attr_name ) {
		attr_name = strdup("URL");  // default
	}
	ad->EvaluateAttrString(attr_name, dest);
	free(attr_name);
	if ( dest.length() ) {
		Url = strdup(dest.c_str());
		if (! IS_DIRECTORY(Url) ) {
			dprintf(D_ALWAYS, "ERROR: URL %s is not a directory\n", Url);
		}
	}
	ASSERT(Url);

	// setup Expiration_time here
	Expiration_time = Lease->leaseExpiration();

}

StorkLeaseEntry::StorkLeaseEntry(const char* path)
{
	initialize();

	ASSERT(path);
	Url = condor_url_dirname( path );
	if (! IS_DIRECTORY(path) ) {
		CompletePath = new MyString( path );
	}
}


StorkLeaseEntry::~StorkLeaseEntry(void)
{
	if ( Url ) free(Url);
	if ( Lease ) delete Lease;
	if ( CompletePath ) delete CompletePath;
}


bool
StorkLeaseEntry::ReleaseLeaseWhenDone(void)
{
	ASSERT(Lease);
	return Lease->releaseLeaseWhenDone();
}


int
StorkLeaseEntry::operator< (const StorkLeaseEntry& E2)
{
	time_t comp1, comp2;
	static bool check_config = true;
	static bool want_rr = true;

	if ( Expiration_time==0 || E2.Expiration_time==0 ) {
		// must be searching just on Url,
		// return false so we don't stop search early
		return 0;
	}

	if ( check_config ) {
		want_rr = param_boolean("STORK_LM_WANT_RR",true);
		check_config = false;
	}

	if ( want_rr ) {
		if ( num_matched < E2.num_matched ) {
			return 1;
		} else {
			return 0;
		}
	}


	if ( IdleExpiration_time ) {
		comp1 = MIN( IdleExpiration_time, Expiration_time );
	} else {
		comp1 = Expiration_time;
	}

	if ( E2.IdleExpiration_time ) {
		comp2 = MIN( E2.IdleExpiration_time, E2.Expiration_time );
	} else {
		comp2 = E2.Expiration_time;
	}

	if ( comp1 < comp2 ) {
		return 1;
	} else {
		return 0;
	}
}


int
StorkLeaseEntry::operator== (const StorkLeaseEntry& E2)
{
	// If both entries have a lease id, that must be equal

	if ( Lease &&  E2.Lease ) {
		if ( Lease->idMatch(*E2.Lease) )  {
			return 1;
		} else {
			return 0;
		}
	}

	// Else entrires are equal if their dest file is equal
	if (CompletePath && E2.CompletePath) {
		if ( *(CompletePath) == *(E2.CompletePath) ) {
			return 1;
		} else {
			return 0;
		}
	}

	// Else on Url
	if (Url && E2.Url ) {
		if ( strcmp(Url,E2.Url)==0 ) {
			return 1;
		} else {
			return 0;
		}
	}

	// If made it here, something is wrong!
	EXCEPT("StorkLeaseEntry operator== : Internal inconsistancy!");
	return 0; // will never get here, just to make compiler happy
}

/*****************************************************************************/


// Constructor
StorkLeaseManager::StorkLeaseManager(void)
{
	tid = -1;
	tid_interval = -1;

	lm_name = param("STORK_LM_NAME");
	lm_pool = param("STORK_LM_POOL");

	matchStats = new HashTable<MyString,StorkMatchStatsEntry*>(200,MyStringHash,rejectDuplicateKeys);
	SetTimers();
}


// Destructor.
StorkLeaseManager::~StorkLeaseManager(void)
{
	StorkLeaseEntry* match;

	idleMatches.StartIterations();
	while ( idleMatches.Iterate(match) ) {
		ASSERT(match->Lease);
		//dprintf(D_FULLDEBUG,
		//		"TODD - idle remove %p ptr=%p cp=%p\n",
		//		match->Lease,match,match->CompletePath);
		delete match;
	}

	busyMatches.StartIterations();
	while ( busyMatches.Iterate(match) ) {
		ASSERT(match->Lease);
		//dprintf(D_FULLDEBUG,
		//		"TODD - busy remove %p ptr=%p cp=%p\n",
		//		match->Lease,match,match->CompletePath);
		delete match;
	}

	if (lm_name) free(lm_name);
	if (lm_pool) free(lm_pool);
}


bool
StorkLeaseManager::areMatchesAvailable(void)
{
	getMatchesFromLeaseManager();

	if ( idleMatches.Count() == 0 )  {
		return false;
	} else {
		return true;
	}
}


// Grab matches from the lease manager
bool
StorkLeaseManager::getMatchesFromLeaseManager(void)
{
	StorkLeaseEntry* match;

		// Grab some matches from the lease manager if we don't have
		// any locally cached.
	if ( idleMatches.Count() == 0 )
	{
		list<DCLeaseManagerLease *> leases;

		// Match ClassAd
		classad::ClassAd	match_ad;

		int num = param_integer("STORK_LM_MATCHES_PER_REQUEST",10,1);
		match_ad.InsertAttr( "RequestCount", num );

		int duration = param_integer("STORM_LM_MATCH_DURATION",1800,1);
		match_ad.InsertAttr( "LeaseDuration", duration );

		char *req = param("STORK_LM_REQUIREMENTS");
		classad::ExprTree	*req_expr = NULL;
		if ( req ) {
			string					req_str = req;
			classad::ClassAdParser	parser;
			req_expr = parser.ParseExpression( req_str );
			if ( !req_expr ) {
				dprintf( D_ALWAYS,
						 "ERROR: Unable to parse requirements '%s'\n", req );
			} else {
				match_ad.Insert( "Requirements", req_expr );
			}
			free( req );
		}
		char *name = param("STORK_NAME");
		if ( !name ) {
			// TODO - need a unique name here
			name = strdup("whatever");
		}
		match_ad.InsertAttr( "Name", name );
		free( name );

		char	*tmp = param( "STORK_LM_EXPRS" );
		if( tmp ) {
			StringList	reqdExprs;
			reqdExprs.initializeFromString (tmp);	
			free (tmp);

			if( !reqdExprs.isEmpty() ) {
				reqdExprs.rewind();
				while( ( tmp = reqdExprs.next()) ) {
					char	pname[64];
					snprintf( pname, sizeof( pname ), "STORK_LM_%s", tmp );
					char	*expr;
					expr = param( pname );
					if ( !expr ) {
						expr = param( tmp );
					}
					if( expr ) {
						dprintf( D_FULLDEBUG, "%s = %s\n", tmp, expr );
						match_ad.InsertAttr( tmp, expr );
						free( expr );
					}
				}
			}	
		}

		classad::PrettyPrint u;
		std::string adbuffer;
		u.Unparse( adbuffer, &match_ad );
		dprintf( D_FULLDEBUG, "MatchAd=%s\n", adbuffer.c_str() );

			// After all that, go get the stinkin' matches
		DCLeaseManager		dclm( lm_name,lm_pool );
		bool result = dclm.getLeases( match_ad, leases );

		if ( !result ) {
			dprintf(D_ALWAYS,"ERROR getLeases() failed, num=%d\n", num);
			return false;
		}

			// For all matches we get back, add to our idle set
		int count = 0;
		for ( list<DCLeaseManagerLease *>::iterator iter=leases.begin();
			  iter != leases.end();
			  iter++ )
		{
			count++;
			match = new StorkLeaseEntry( *iter );
			addToIdleSet(match);
		// dprintf(D_FULLDEBUG,"TODD - idle add %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);
		}
		dprintf(D_ALWAYS,
			"LM: Requested %d matches from LeaseManager, got %d back\n",
			num, count);
	}

	return true;
}



// Get a dynamic transfer destination from the lease manager by protocol.
StorkLeaseEntry *
StorkLeaseManager::getTransferDestination(const char *protocol)
{
	(void) protocol;
	StorkLeaseEntry* match;

	getMatchesFromLeaseManager();

		// Now if we have any idle matches, just give the first one.
	idleMatches.StartIterations();
	if ( idleMatches.Iterate(match) ) {
		// found one.

		// remove match from idle set.
		idleMatches.RemoveLast();

		// add it into the busy set.
		addToBusySet(match);

		// dprintf(D_FULLDEBUG,"TODD1 - idle add %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);

		// return url + filename
		return match;
	}
	return NULL;	// failed!
}

const char *
StorkLeaseManager::
getTransferDirectory(const char *protocol)
{
	StorkLeaseEntry* match = getTransferDestination(protocol);
	if (match == NULL) {
		return NULL;
	}

		// update some stats
	MyString key( match->GetUrl() );
	StorkMatchStatsEntry *value = NULL;
	matchStats->lookup(key,value);
	if ( !value ) {
		value = new StorkMatchStatsEntry;
		matchStats->insert(key,value);
	}
	(value->curr_busy_matches)++;
	(value->total_busy_matches)++;

	return strdup( match->GetUrl() );
}



// WARNING:  This method not yet tested!
const char *
StorkLeaseManager::
getTransferFile(const char *protocol)
{
	StorkLeaseEntry* match = getTransferDestination(protocol);
	if (match == NULL) {
		return NULL;
	}

	static int unique_num = 0;
	if ( unique_num == 0 ) {
		unique_num = (int) time(NULL);
	}
	if (!match->CompletePath) {
		match->CompletePath = new MyString;
		ASSERT(match->CompletePath);
	}
	match->CompletePath->sprintf("%s/file%d",match->GetUrl(),++unique_num);
	// dprintf(D_FULLDEBUG,"TODD2 - idle add %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);
	return strdup(match->CompletePath->Value());
}


// Return a dynamic transfer destination to the lease manager.
bool
StorkLeaseManager::returnTransferDestination(const char * path,
										   bool successful_transfer)
{
	bool ret_val;
	StorkLeaseEntry match(path);

		// just move it from the busy set to the idle set
	ret_val =  fromBusyToIdle( &match );

	if ( ret_val ) {
			// update some stats
		MyString key( path );
		StorkMatchStatsEntry *value = NULL;
		matchStats->lookup(key,value);
		if ( value ) {
			(value->curr_busy_matches)--;
			if ( successful_transfer ) {
				(value->successes)++;
			} else {
				(value->failures)++;
			}
		}
	}

	return ret_val;
}


// Inform the lease manager that a dynamic transfer destination has
// failed.
bool
StorkLeaseManager::failTransferDestination(const char * path)
{
		// Remove this destination from BOTH busy and idle lists.
		// By doing so, we let the lease on this destination simply
		// expire, and ensure we do not give out this url again until that happens.

	ASSERT(path);

	// Only fail transfer destinations if enabled.
	if (! param_boolean("STORK_LM_XFER_FAIL_ENABLE", true) ) {
		return returnTransferDestination(path,false);
	}
		// Create entry on *dirname* since we assume that all
		// transfers to this destination will fail if this one did.
	StorkLeaseEntry match;
	match.Url = condor_url_dirname( path );
	
		// Call in a while loop, since we have multiple matches to this destination
	//while ( destroyFromBusy( &match ) ) ;
	//while ( destroyFromIdle( &match ) ) ;
	
	destroyFromBusy( &match ) ;

	return true;	
}

bool
StorkLeaseManager::destroyFromBusy(StorkLeaseEntry*  match)
{
	StorkLeaseEntry* temp;

	busyMatches.StartIterations();
	while (busyMatches.Iterate(temp)) {
		if ( *temp == *match ) {
			const char	*s;
			if ( temp->CompletePath ) {
				s = temp->CompletePath->Value();
			} else if ( temp->Lease ) {
				s = temp->Lease->leaseId().c_str();
			} else {
				s = temp->GetUrl();
			}
			dprintf( D_FULLDEBUG, "Destroying busy match %s\n", s );
			busyMatches.RemoveLast();
			delete temp;
			return true;
		}
	}
	return false;
}

bool
StorkLeaseManager::destroyFromIdle(StorkLeaseEntry*  match)
{
	StorkLeaseEntry* temp;

	idleMatches.StartIterations();
	while (idleMatches.Iterate(temp)) {
		if ( *temp == *match ) {
			const char	*s;
			if ( temp->CompletePath ) {
				s = temp->CompletePath->Value();
			} else if ( temp->Lease ) {
				s = temp->Lease->leaseId().c_str();
			} else {
				s = temp->GetUrl();
			}
			dprintf( D_FULLDEBUG, "Destroying idle match %s\n", s );
			idleMatches.RemoveLast();
			delete temp;
			return true;
		}
	}
	return false;
}

bool
StorkLeaseManager::addToBusySet(StorkLeaseEntry* match)
{
	match->IdleExpiration_time = 0;
	ASSERT(match->Lease);
	busyMatches.Add(match);
	return true;
}

bool
StorkLeaseManager::addToIdleSet(StorkLeaseEntry* match)
{
	int n = param_integer("STORK_LM_MATCH_IDLE_TIME", 5 * 60, 0);

	match->IdleExpiration_time = time(NULL) + n;
	ASSERT(match->Lease);
	idleMatches.Add(match);
	return true;
}

bool
StorkLeaseManager::fromBusyToIdle(StorkLeaseEntry* match)
{
	StorkLeaseEntry* full_match = NULL;
	StorkLeaseEntry* temp;

	if (match->Lease) {
		full_match = match;
		busyMatches.Remove(match);
	} else {
	  	// We need to find the entry
		busyMatches.StartIterations();
		while ( busyMatches.Iterate(temp) ) {
			if ( *temp==*match ) {
				// found it
				full_match = temp;
				busyMatches.RemoveLast();
				break;
			}
		}
		if ( !full_match ) {
			dprintf( D_FULLDEBUG,
					 "StorkLeaseEntry fromBusyToIdle: Can't find match %s!\n",
					 match->GetUrl() );
			return false;
		}
	}

	if ( full_match->Lease ) {
		if ( full_match->ReleaseLeaseWhenDone() ) {
			// drop this match on the floor, since the lease manager asked us
			// to stop using it as soon as the current transfer completed
			delete full_match;
			full_match = NULL;
		} else {
			addToIdleSet(full_match);
		}
		return true;
	} else {
		return false;
	}
}

void
StorkLeaseManager::SetTimers(void)
{
#ifndef TEST_VERSION
	int interval = param_integer("STORK_LM_INTERVAL",30);

	if ( interval == tid_interval  && tid != -1 ) {
		// we are already done, since we already
		// have a timer set with the desired interval
		return;
	}

	// If we made it here, the timer needs to be re-created
	// either because (a) it never existed, or (b) the interval changed

	if ( tid != -1 ) {
		// destroy pre-existing timer
		daemonCore->Cancel_Timer(tid);
	}

	// Create a new timer the correct interval
	tid = daemonCore->Register_Timer(interval,interval,
		(TimerHandlercpp)&StorkLeaseManager::timeout,
		"StorkLeaseManager::timeout", this);
	tid_interval = interval;
#endif
}

void
StorkLeaseManager::timeout(void)
{
	time_t now = time(NULL);
	int near_future = param_integer("STORK_LM_LEASE_REFRESH_PADDING",5*60,30);
	StorkLeaseEntry* match;
	Set<StorkLeaseEntry*> to_release, to_renew;
	bool result;



	// =====================================================================
	// First, deal with old idle matches.  For an old idle match,
	// we either need to simply remove it if it expired, or give it
	// back to the negotiator.
	idleMatches.StartIterations();
	while ( idleMatches.Iterate(match) ) {
		ASSERT(match->Expiration_time);
		ASSERT(match->IdleExpiration_time);
		if ( match->Expiration_time <= now ) {
			// This match expired, just delete it.
			idleMatches.RemoveLast();
			delete match;
			continue;
		}
		if ( match->IdleExpiration_time <= now ) {
			// This match has not expired, but has been sitting on our
			// idle list for too long.  Add it to the list of leases
			// to release, and remove from the list.
			to_release.Add(match);
			idleMatches.RemoveLast();
			continue;
		}
#ifdef USING_ORDERED_SET
		// If we made it here, we know everything else on the list is
		// for the future, since the list is ordered.  So break out,
		// we're done.
		break;
#endif
	}
	// Now actually connect to the lease manager if we have leases to release.
	if ( to_release.Count() ) {
			// create list of lease ads to release
		list<DCLeaseManagerLease*> leases;
		to_release.StartIterations();
		while ( to_release.Iterate(match) ) {
			ASSERT(match->Lease);
			leases.push_back(match->Lease);
		}
			// send our list to the lease manager
		DCLeaseManager		dclm( lm_name, lm_pool );
		result = dclm.releaseLeases(leases);
		dprintf(D_ALWAYS,"LM: %s release %d leases\n",
						result ? "Successful" : "Failed to ",
						to_release.Count());
			// deallocate memory
		to_release.StartIterations();
		while ( to_release.Iterate(match) ) {
			delete match;
		}
	}

	// =====================================================================
	// Now deal with busy matches that need to be refreshed.
	busyMatches.StartIterations();
	while ( busyMatches.Iterate(match) ) {
		ASSERT(match->Expiration_time);
		if ( match->Expiration_time <= now ) {
			// This match _already_ expired, just delete it.
			busyMatches.RemoveLast();
			delete match;
			continue;
		}
		if ( match->Expiration_time <= now + near_future ) {
			// This match is about to expire, move it to renew list.
			busyMatches.RemoveLast();
			to_renew.Add(match);
			continue;
		}
#ifdef USING_ORDERED_SET
		// If we made it here, we know everything else on the list is
		// for the future, since the list is ordered.  So break out, we're done.
		break;
#endif
	}
	// Now actually connect to the lease manager if we have leases to renew.
	if ( to_renew.Count() ) {
			// create list of lease ads
		list<const DCLeaseManagerLease*> input_leases;
		list<DCLeaseManagerLease*> output_leases;
		to_renew.StartIterations();
		while ( to_renew.Iterate(match) ) {
			ASSERT(match->Lease);
			input_leases.push_back(match->Lease);
		}
			// send our list to the lease manager
		DCLeaseManager		dclm( lm_name,lm_pool );
		result = dclm.renewLeases(input_leases,output_leases);
		dprintf(D_ALWAYS,"LM: %s renew %d leases\n",
						result ? "Successful" : "Failed to ",
						to_renew.Count());
			// update the expiration counters for all renewed leases
		if ( result ) {
			list<DCLeaseManagerLease *>::iterator iter;
			for ( iter  = output_leases.begin();
				  iter != output_leases.end();
				  iter++ )
			{
				bool found = false;
				DCLeaseManagerLease *lease = *iter;
				to_renew.StartIterations();
				while ( to_renew.Iterate(match) ) {
					ASSERT(match->Lease);
					if ( match->Lease->idMatch(*lease) ) {
						match->Lease->copyUpdates( *lease );
						found = true;
						break;
					}
				}
				ASSERT(found);	// an Id in our output list better be found in our input list!
			}
			DCLeaseManagerLease_freeList( output_leases );
		}
			// now put em all back onto the busy set, refreshed or not.
		to_renew.StartIterations();
		while ( to_renew.Iterate(match) ) {
			addToBusySet(match);
		}
	}

		// reset timers to go off again as appropriate
	SetTimers();
}
