/***************************************************************
 *
 * Copyright (C) 2018, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_PCCC_H
#define _CONDOR_PCCC_H

// #include "condor_common.h"
// #include <map>
// #include <set>
// #include "proc.h"
// #include "scheduler.h"
// #include "pccc.h"

//
// Pending Claims for Coalesce Command
//
// In order to coalesce a set of claims (e.g., for condor_now), we must
// deactivate each one before we (ask the startd to) coalesce them.  We
// deactivate claims asynchronously, so we need keep track of which claims
// we've queued for deactivation and which claims have actually been
// deactivated.
//

struct ProcIDComparison {
	bool operator() ( const PROC_ID & a, const PROC_ID & b ) const {
		if( a.cluster < b.cluster ) { return true; }
		if( a.cluster == b.cluster ) { return a.proc < b.proc; }
		return false;
	}
};

typedef std::map< PROC_ID, std::set< match_rec * >, ProcIDComparison >
	ProcIDToMatchRecMap;
typedef std::map< PROC_ID, int, ProcIDComparison >
	ProcIDToTimerMap;
typedef std::map< PROC_ID, Service *, ProcIDComparison >
	ProcIDToServiceMap;

//
// Arguably, this should each pending coalesce should be its own object,
// but this interface seemed simpler to actually use.
//

bool pcccNew( PROC_ID nowJob );
void pcccWants( PROC_ID nowJob, match_rec * match );
void pcccGot( PROC_ID nowJob, match_rec * match );
bool pcccSatisfied( PROC_ID nowJob );
void pcccStartCoalescing( PROC_ID nowJob, int retriesRemaining );

// Never call this.
bool pcccTest();

// For testing purposes only.
void pcccDumpTable( int flags = D_FULLDEBUG );

// Utility function.
void send_matchless_vacate( const char * name, const char * pool, const char * addr, const char * claimID, int cmd );

#endif /* _CONDOR_PCCC_H */
