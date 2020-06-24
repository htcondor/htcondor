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

#include "condor_common.h"
#include <map>
#include <set>
#include "proc.h"
#include "scheduler.h"
#include "pccc.h"

#include <algorithm>
#include "dc_startd.h"
#include "qmgmt.h"

extern Scheduler scheduler;
extern void send_vacate( match_rec *, int );

// Globals --------------------------------------------------------------------

// Arguably, we should have one map from PROC_ID to a structure, and that
// map should be in the Scheduler singleton.
ProcIDToMatchRecMap pcccWantsMap;
ProcIDToMatchRecMap pcccGotMap;
ProcIDToTimerMap pcccTimerMap;

// This is stupid, but less troublesome than adding the plumbing to expose
// the Service pointer the TimerManager is already holding.
ProcIDToServiceMap pcccTimerSelfMap;

// Forward declarations -------------------------------------------------------

void pcccStopCoalescing( PROC_ID nowJob );

class pcccDoneCallback : public Service {
	public:
		pcccDoneCallback( PROC_ID nj ) : nowJob(nj) { }
		void callback();

	private:
		PROC_ID nowJob;
};

class pcccStopCallback : public Service {
	public:
		pcccStopCallback( PROC_ID nj, classy_counted_ptr<TwoClassAdMsg> tcam, const char * n, const char * a, int rr ) : nowJob(nj), message(tcam), name(n), addr(a), retriesRemaining(rr) { }

		void callback();
		static void failed( PROC_ID nowJob );
		void dcMessageCallback( DCMsgCallback * cb );

	private:
		PROC_ID nowJob;
		classy_counted_ptr<TwoClassAdMsg> message;
		const char * name;
		const char * addr;
		int retriesRemaining;
};

// API functions --------------------------------------------------------------

bool
pcccNew( PROC_ID nowJob ) {
	dprintf( D_FULLDEBUG, "pcccNew( %d.%d )\n", nowJob.cluster, nowJob.proc );

	if( pcccWantsMap.find( nowJob ) != pcccWantsMap.end() ) { return false; }

	return true;
}

void
pcccWants( PROC_ID nowJob, match_rec * match ) {
	dprintf( D_FULLDEBUG, "pcccWants( %d.%d, %p )\n", nowJob.cluster, nowJob.proc, match );

	if( pcccTimerMap.find( nowJob ) == pcccTimerMap.end() ) {
		pcccDoneCallback * pcd = new pcccDoneCallback( nowJob );
		pcccTimerSelfMap[ nowJob ] = pcd;
		pcccTimerMap[ nowJob ] = daemonCore->Register_Timer(
			20 /* years of carefuly research */,
			(TimerHandlercpp) & pcccDoneCallback::callback,
			"pcccDoneCallback", pcd );
		dprintf( D_FULLDEBUG, "pcccWants( %d.%d, %p ): started timer %d (data %p)\n", nowJob.cluster, nowJob.proc, match, pcccTimerMap[ nowJob ], pcd );
	} else {
		// Reschedule the timer for 20 seconds from now, instead?
	}

	pcccWantsMap[ nowJob ].insert( match );
}

void
pcccGot( PROC_ID nowJob, match_rec * match ) {
	dprintf( D_FULLDEBUG, "pcccGot( %d.%d, %p )\n", nowJob.cluster, nowJob.proc, match );

	// We can't invalidate the match record's now job until later, because we
	// need to keep it marked as special so it's not deleted.  However, if we
	// just leave it alone, we could blow an assert later, because the job we
	// just vacated could be rescheduled.  So just unlink the match record
	// instead.  (Don't release the claim yet -- we need it for the
	// coalesce command later.)
	bool later = match->needs_release_claim;
	match->needs_release_claim = false;
	scheduler.unlinkMrec( match );
	match->needs_release_claim = later;

	pcccGotMap[ nowJob ].insert( match );
}

bool
pcccSatisfied( PROC_ID nowJob ) {
	dprintf( D_FULLDEBUG, "pcccSatisfied( %d.%d )\n", nowJob.cluster, nowJob.proc );

#if defined(SOME_GOOD_REASON_THIS_IS_CPP14_ONLY)
	return std::equal(	pcccWantsMap[ nowJob ].begin(),
						pcccWantsMap[ nowJob ].end(),
						pcccGotMap[ nowJob ].begin()
						pcccGotMap[ nowJob ].end() );
#else
	return pcccWantsMap[ nowJob ].size() == pcccGotMap[ nowJob ].size() &&
		std::equal(  pcccWantsMap[ nowJob ].begin(),
			pcccWantsMap[ nowJob ].end(), pcccGotMap[ nowJob ].begin() );
#endif
}

void
pcccStartCoalescing( PROC_ID nowJob, int retriesRemaining ) {
	dprintf( D_FULLDEBUG, "pcccStartCoalescing( %d.%d )\n", nowJob.cluster, nowJob.proc );

	if( pcccTimerMap.find( nowJob ) != pcccTimerMap.end() ) {
		dprintf( D_FULLDEBUG, "pcccStartCoalescing( %d.%d ): delete( %p )\n", nowJob.cluster, nowJob.proc, pcccTimerSelfMap[ nowJob ] );
		delete( pcccTimerSelfMap[ nowJob ] );
		pcccTimerSelfMap.erase( nowJob );

		dprintf( D_FULLDEBUG, "pcccStartCoalescing( %d.%d ): Cancel_Timer( %d )\n", nowJob.cluster, nowJob.proc, pcccTimerMap[ nowJob ] );
		daemonCore->Cancel_Timer( pcccTimerMap[ nowJob ] );
		pcccTimerMap.erase( nowJob );
	}


	// Issue coalesce command.
	std::set< match_rec * > matches = pcccGotMap[ nowJob ];
	ASSERT(! matches.empty());

	auto i = matches.begin();
	match_rec * match = * i;
	classy_counted_ptr<DCStartd> startd = new DCStartd( match->description(),
		NULL, match->peer, NULL );

	ClassAd commandAd;
	std::string claimIDList;
	formatstr( claimIDList, "%s", match->claimId() );
	for( ++i; i != matches.end(); ++i ) {
		formatstr( claimIDList, "%s, %s", claimIDList.c_str(), (* i)->claimId() );
	}
	// ATTR_CLAIM_ID_LIST is one of the magic attributes that we automatically
	// encrypt/decrypt whenever we're about to put/get it on/from the wire.
	commandAd.InsertAttr( ATTR_CLAIM_ID_LIST, claimIDList.c_str() );

	ClassAd * jobAd = GetJobAd( nowJob.cluster, nowJob.proc );
	if(! jobAd) {
		// We checked that the now job existed when we accepted the coalesce
		// request, so this is rather unexpected.  However, we should note it
		// in the log whenever it happens, so that the admin has something to
		// find if a user asks why their now job failed to run after their
		// condor_now command succeeded.
		dprintf( D_ALWAYS, "[now job %d.%d]: unable to find now job ad, failing\n", nowJob.cluster, nowJob.proc );
		pcccStopCallback::failed( nowJob );
		return;
	}

	classy_counted_ptr<TwoClassAdMsg> cMsg = new TwoClassAdMsg( COALESCE_SLOTS, commandAd, * jobAd );
	cMsg->setStreamType( Stream::reli_sock );
	cMsg->setSuccessDebugLevel( D_FULLDEBUG );
	pcccStopCallback * pcs = new pcccStopCallback( nowJob, cMsg, match->description(), match->peer, retriesRemaining );
	// Annoyingly, the deadline only applies to /sending/ the message.
	pcccTimerMap[ nowJob ] = daemonCore->Register_Timer(
		20 /* years of careful research */,
		(TimerHandlercpp) & pcccStopCallback::callback, "pcccStop", pcs );
	pcccTimerSelfMap[ nowJob ] = pcs;
	cMsg->setCallback( new DCMsgCallback( (DCMsgCallback::CppFunction) & pcccStopCallback::dcMessageCallback, pcs ) );
	cMsg->setDeadlineTimeout( 20 /* years of careful research */ );
	startd->sendMsg( cMsg.get() );
}

void
send_matchless_vacate( const char * name, const char * pool, const char * addr, const char * claimID, int cmd ) {
	classy_counted_ptr<DCStartd> startd = new DCStartd( name, pool, addr, claimID );
	classy_counted_ptr<DCClaimIdMsg> msg = new DCClaimIdMsg( cmd, claimID );

	msg->setSuccessDebugLevel( D_FULLDEBUG );
	msg->setTimeout( STARTD_CONTACT_TIMEOUT );
	msg->setStreamType( Stream::reli_sock );

	startd->sendMsg( msg.get() );
}

// Internal functions ---------------------------------------------------------

void
pcccStopCoalescing( PROC_ID nowJob ) {
	dprintf( D_FULLDEBUG, "pcccStopCoalescing( %d.%d )\n", nowJob.cluster, nowJob.proc );

	if( pcccTimerMap.find( nowJob ) != pcccTimerMap.end() ) {
		dprintf( D_FULLDEBUG, "pcccStopCoalescing( %d.%d ): Cancel_Timer( %d )\n", nowJob.cluster, nowJob.proc, pcccTimerMap[ nowJob ] );
		daemonCore->Cancel_Timer( pcccTimerMap[ nowJob ] );

		dprintf( D_FULLDEBUG, "pcccStopCoalescing( %d.%d ): delete( %p )\n", nowJob.cluster, nowJob.proc, pcccTimerSelfMap[ nowJob ] );
		delete( pcccTimerSelfMap[ nowJob ] );
	}

	// If the coalesce command succeeds, don't release the coalesced
	// claims -- they've all already been invalidated.  Also don't call
	// DelMrec(), since we already unlink()ed all of the matches.
	std::set< match_rec * > & gotList = pcccGotMap[ nowJob ];
	for( auto i = gotList.begin(); i != gotList.end(); ++i ) {
		dprintf( D_FULLDEBUG, "pcccStopCoalescing( %d.%d ): DelMrec( %p )\n", nowJob.cluster, nowJob.proc, *i );
		delete( *i );
	}

	pcccWantsMap.erase( nowJob );
	pcccGotMap.erase( nowJob );
	pcccTimerMap.erase( nowJob );
	pcccTimerSelfMap.erase( nowJob );

	pcccDumpTable();
}

void
pcccDumpTable( int flags ) {
	dprintf( flags, "pcccDumpTable(): dumping table...\n" );
	for( auto i = pcccWantsMap.begin(); i != pcccWantsMap.end(); ++i ) {
		PROC_ID nowJob = i->first;
		dprintf( flags, "%d.%d = [%p, %p, %d, %p]\n",
			nowJob.cluster, nowJob.proc,
			& pcccWantsMap[ nowJob ], & pcccGotMap[ nowJob ],
			pcccTimerMap[ nowJob ], & pcccTimerSelfMap[ nowJob ] );
	}
	dprintf( flags, "pcccDumpTable(): ... done dumping PCCC table.\n" );
}

void
pcccDoneCallback::callback() {
	dprintf( D_ALWAYS, "[now job %d.%d]: targeted job(s) did not vacate quickly enough, failing\n", nowJob.cluster, nowJob.proc );

	// Prevent outstanding deactivations for claims we haven't got() yet
	// from confusing us later.  Instead, we'll just schedule them.
	std::set< match_rec * > & wantsList = pcccWantsMap[ nowJob ];
	for( auto i = wantsList.begin(); i != wantsList.end(); ++i ) {
		(*i)->m_now_job.invalidate();
	}

	std::set< match_rec * > & gotList = pcccGotMap[ nowJob ];
	for( auto i = gotList.begin(); i != gotList.end(); ++i ) {
		dprintf( D_FULLDEBUG, "pcccDoneCallback( %d.%d ): DelMrec( %p )\n", nowJob.cluster, nowJob.proc, *i );
		scheduler.DelMrec( *i );
	}

	pcccWantsMap.erase( nowJob );
	pcccGotMap.erase( nowJob );

	// We shouldn't have to cancel the timer (it was a one-shot),
	// but since we do need to delete something in pcccTimerSelfMap,
	// we might as well use the same code as we do elsewhere.
	//
	// Deletes this.
	if( pcccTimerMap.find( nowJob ) != pcccTimerMap.end() ) {
		dprintf( D_FULLDEBUG, "pcccDoneCallback::callback( %d.%d ): Cancel_Timer( %d )\n", nowJob.cluster, nowJob.proc, pcccTimerMap[ nowJob ] );
		daemonCore->Cancel_Timer( pcccTimerMap[ nowJob ] );
		pcccTimerMap.erase( nowJob );

		auto * self = pcccTimerSelfMap[ this->nowJob ];
		dprintf( D_FULLDEBUG, "pcccDoneCallback::callback( %d.%d ): delete( %p )\n", nowJob.cluster, nowJob.proc, self );
		pcccTimerSelfMap.erase( this->nowJob );
		delete( self );
	}

	pcccDumpTable();
}


class SlowRetryCallback : public Service {
	public:
		SlowRetryCallback( PROC_ID nj, int rr ) : nowJob(nj), retriesRemaining(rr) { }

		void callback() {
			dprintf( D_FULLDEBUG, "SlowRetryCallback::callback( %d, %d )\n", nowJob.cluster, nowJob.proc );
			pcccStartCoalescing( nowJob, retriesRemaining - 1 );
			delete( this );
		}

	private:
		PROC_ID nowJob;
		int retriesRemaining;
};


void
pcccStopCallback::callback() {
	dprintf( D_ALWAYS, "[now job %d.%d]: coalesce command timed out, failing\n", nowJob.cluster, nowJob.proc );

	// This calls dcMessageCallback(), which turns around and
	// calls failed(), which delete()s this.  This sequence
	// seems a little fragile to me, but there's no way to
	// unregister a callback.
	message.get()->cancelMessage( "coalesce command timed out" );
}

void
pcccStopCallback::failed( PROC_ID nowJob ) {
	// If the coalesce command times out, delete -- and try to
	// release -- all the claims we got.  Don't call DelMrec(),
	// because we already unlink()ed the match record.
	std::set< match_rec * > & gotList = pcccGotMap[ nowJob ];
	for( auto i = gotList.begin(); i != gotList.end(); ++i ) {
		dprintf( D_FULLDEBUG, "pcccStopCallback::failed( %d.%d ): DelMrec( %p )\n", nowJob.cluster, nowJob.proc, *i );
		if( (*i)->needs_release_claim ) {
			send_vacate( *i, RELEASE_CLAIM );
		}
		delete( *i );
	}

	pcccGotMap.erase( nowJob );
	pcccWantsMap.erase( nowJob );

	// pcccStopCallback::failed() can be called from a timer firing
	// or from the message callback, so it has to explicitly cancel
	// the timer.
	if( pcccTimerMap.find( nowJob ) != pcccTimerMap.end() ) {
		dprintf( D_FULLDEBUG, "pcccStopCallback::failed( %d.%d ): delete( %p )\n", nowJob.cluster, nowJob.proc, pcccTimerSelfMap[ nowJob ] );
		delete( pcccTimerSelfMap[ nowJob ] );
		pcccTimerSelfMap.erase( nowJob );

		dprintf( D_FULLDEBUG, "pcccStopCallback::failed( %d.%d ): Cancel_Timer( %d )\n", nowJob.cluster, nowJob.proc, pcccTimerMap[ nowJob ] );
		daemonCore->Cancel_Timer( pcccTimerMap[ nowJob ] );
		pcccTimerMap.erase( nowJob );
	}

	pcccDumpTable();
}

void
pcccStopCallback::dcMessageCallback( DCMsgCallback * cb ) {
	dprintf( D_FULLDEBUG, "pcccStopCallback::dcMessageCallback( %d.%d )\n", nowJob.cluster, nowJob.proc );

	// Not sure why this one isn't also a classy_counted_ptr.
	TwoClassAdMsg * msg = reinterpret_cast<TwoClassAdMsg *>( cb->getMessage() );

	switch( msg->deliveryStatus() ) {
		case DCMsg::DELIVERY_SUCCEEDED: {
			ClassAd & reply = msg->getFirstClassAd();
			ClassAd & slotAd = msg->getSecondClassAd();

			std::string resultString;
			reply.LookupString( ATTR_RESULT, resultString );
			CAResult result = getCAResultNum( resultString.c_str() );
			switch( result ) {
				default:
				case CA_FAILURE:
				case CA_INVALID_REQUEST: {
					std::string errorString;
					reply.LookupString( ATTR_ERROR_STRING, errorString );
					dprintf( D_ALWAYS, "[now job %d.%d]: coalesce failed: %s\n", nowJob.cluster, nowJob.proc, errorString.c_str() );

					// Deletes this.
					failed( nowJob );
					} return;

				case CA_SUCCESS:
					break;

				case CA_INVALID_STATE:
					if( retriesRemaining == 0 ) {
						dprintf( D_ALWAYS, "[now job %d.%d]: coalesce failed last retry, giving up.\n", nowJob.cluster, nowJob.proc );

						// Deletes this.
						failed( nowJob );
						return;
					}

					dprintf( D_FULLDEBUG, "pcccStopCallback::dcMessageCallback( %d.%d ): will retry in one second (%d retries remaining)\n", nowJob.cluster, nowJob.proc, retriesRemaining );

					// Retry one second from now.
					SlowRetryCallback * srcb = new SlowRetryCallback( nowJob, retriesRemaining );
					daemonCore->Register_Timer( 1,
						(TimerHandlercpp) & SlowRetryCallback::callback,
						"SlowRetryCallBack", srcb );

					// Kill the timer from this attempt.  Deletes this.
					if( pcccTimerMap.find( nowJob ) != pcccTimerMap.end() ) {
						dprintf( D_FULLDEBUG, "pcccStopCallback::dcMessageCallback( %d.%d ): Cancel_Timer( %d )\n", nowJob.cluster, nowJob.proc, pcccTimerMap[ nowJob ] );
						daemonCore->Cancel_Timer( pcccTimerMap[ nowJob ] );
						pcccTimerMap.erase( nowJob );

						auto * self = pcccTimerSelfMap[ this->nowJob ];
						dprintf( D_FULLDEBUG, "pcccStopCallback::dcMessageCallback( %d.%d ): delete( %p )\n", nowJob.cluster, nowJob.proc, self );
						pcccTimerSelfMap.erase( this->nowJob );
						delete( self );
					}
					return;
			}

			// dprintf( D_FULLDEBUG, "pcccStopCallback::dcMessageCallback( %d.%d ): coalesce command returned the following slot ad:\n", nowJob.cluster, nowJob.proc );
			// dPrintAd( D_FULLDEBUG, slotAd );

			std::string claimID;
			if((! reply.LookupString( ATTR_CLAIM_ID, claimID )) || claimID.empty() ) {
				dprintf( D_ALWAYS, "[now job %d.%d]: coalesce did not return a claim ID, failing\n", nowJob.cluster, nowJob.proc );

				// Deletes this.
				failed( nowJob );
				return;
			}
			// dprintf( D_FULLDEBUG, "ATTR_CLAIM_ID = %s\n", claimID.c_str() );

			// Generate a new match record.
			ClassAd * jobAd = GetJobAd( nowJob.cluster, nowJob.proc );
			if(! jobAd) {
				dprintf( D_ALWAYS, "[now job %d.%d]: unable to find now job ad, failing\n", nowJob.cluster, nowJob.proc );

				// Once we've received a claim ID for a coalesced slot,
				// we don't want to waste time trying to release the
				// old and now-invalidated claims which formed it.
				//
				// Further, since we can't split the slot on our own,
				// (even if CLAIM_PARTIONABLE_LEFTOVERS is on, the slot
				// isn't a p-slot), release our new claim.
				send_matchless_vacate( name, NULL, addr,
					claimID.c_str(), RELEASE_CLAIM );
				pcccStopCoalescing( nowJob );
				return;
			}

			// Make sure the job is still idle.
			int status;
			jobAd->LookupInteger( ATTR_JOB_STATUS, status );
			if( status != IDLE ) {
				dprintf( D_ALWAYS, "[now job %d.%d]: now job is no longer idle, failing\n", nowJob.cluster, nowJob.proc );

				send_matchless_vacate( name, NULL, addr,
					claimID.c_str(), RELEASE_CLAIM );
				// Deletes this.
				pcccStopCoalescing( nowJob );
				return;
			}

			std::string owner;
			jobAd->LookupString( ATTR_OWNER, owner );
			ASSERT(! owner.empty());

			Daemon startd( & slotAd, DT_STARTD, NULL );
			if( (! startd.locate()) || startd.error() ) {
				dprintf( D_ALWAYS, "[now job %d.%d]: can't find address of startd in coalesced ad (%d: %s), failing\n", nowJob.cluster, nowJob.proc, startd.errorCode(), startd.error() );
				dprintf( D_FULLDEBUG, "[now job %d.%d]: printing slot ad...\n", nowJob.cluster, nowJob.proc );
				dPrintAd( D_FULLDEBUG, slotAd );
				dprintf( D_FULLDEBUG, "[now job %d.%d]: ... slot ad complete\n", nowJob.cluster, nowJob.proc );

				send_matchless_vacate( name, NULL, addr,
					claimID.c_str(), RELEASE_CLAIM );
				// Deletes this.
				pcccStopCoalescing( nowJob );
				return;
			}

			// We ignore the remote pool attribute because we've
			// already checked if the job is running.
			match_rec * coalescedMatch = scheduler.AddMrec(
				claimID.c_str(), startd.addr(), & nowJob,
				& slotAd, owner.c_str(), NULL
			);
			if(! coalescedMatch) {
				dprintf( D_ALWAYS, "[now job %d.%d]: failed to construct match record\n", nowJob.cluster, nowJob.proc );

				send_matchless_vacate( name, NULL, addr,
					claimID.c_str(), RELEASE_CLAIM );
				pcccStopCoalescing( nowJob );
				return;
			}
			// See Scheduler::claimedStartd() for the things we're
			// skipping.  We're ignoring the auth hole (we're already
			// talking with the startd); we didn't ask for claim
			// leftovers, so we'll let the startd deal with them.
			coalescedMatch->setStatus( M_CLAIMED );

			// Start the now job.
			scheduler.StartJob( coalescedMatch );
			// If we didn't, delete the mrec so the user can try again
			// without crashing the schedd.
			if( coalescedMatch->status != M_ACTIVE ) {
				dprintf( D_ALWAYS, "[now job %d.%d]: failed to start job on match\n", nowJob.cluster, nowJob.proc );
				scheduler.DelMrec( coalescedMatch );
			}

			// Deletes this.
			pcccStopCoalescing( nowJob );
			} break;

		default:
			// Deletes this.
			failed( nowJob );
			break;
	}
}

// Test functions -------------------------------------------------------------

int accounting_single( PROC_ID nowJob ) {
	static int clusterID = 99;

	PROC_ID jobID;
	ClassAd matchAd;

	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchA = scheduler.AddMrec( "A", "peerA", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchB = scheduler.AddMrec( "B", "peerB", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchC = scheduler.AddMrec( "C", "peerC", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchD = scheduler.AddMrec( "D", "peerD", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchE = scheduler.AddMrec( "E", "peerE", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchF = scheduler.AddMrec( "F", "peerF", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchG = scheduler.AddMrec( "G", "peerG", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchH = scheduler.AddMrec( "H", "peerH", & jobID, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchI = scheduler.AddMrec( "I", "peerI", & jobID, & matchAd, "tlmiller", NULL );

	if(! pcccNew( nowJob ) ) { return __LINE__; }
	if(! pcccSatisfied( nowJob )) { return __LINE__; }

	pcccWants( nowJob, matchA );
	if( pcccSatisfied( nowJob ) ) { return __LINE__; }

	pcccGot( nowJob, matchA );
	if(! pcccSatisfied( nowJob ) ) { return __LINE__; }

	pcccWants( nowJob, matchB );
	pcccGot( nowJob, matchC );
	if( pcccSatisfied( nowJob ) ) { return __LINE__; }

	pcccGot( nowJob, matchB );
	if( pcccSatisfied( nowJob ) ) { return __LINE__; }

	pcccWants( nowJob, matchC );
	if(! pcccSatisfied( nowJob ) ) { return __LINE__; }

	pcccWants( nowJob, matchD );
	pcccWants( nowJob, matchE );
	pcccWants( nowJob, matchF );
	if( pcccSatisfied( nowJob ) ) { return __LINE__; }

	pcccGot( nowJob, matchD );
	pcccGot( nowJob, matchE );
	pcccGot( nowJob, matchF );
	if(! pcccSatisfied( nowJob )) { return __LINE__; }

	// Add a few things to the want list for testing purposes later.
	pcccWants( nowJob, matchG );
	pcccWants( nowJob, matchH );
	pcccWants( nowJob, matchI );

	return 0;
}

int accounting_multiple( PROC_ID jobA, PROC_ID jobB, PROC_ID jobC ) {
	static int clusterID = 199;

	PROC_ID jobID;
	ClassAd matchAd;

	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchA = scheduler.AddMrec( "A", "peerA", & jobA, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchB = scheduler.AddMrec( "B", "peerB", & jobB, & matchAd, "tlmiller", NULL );
	jobID = PROC_ID( ++clusterID, 0 );
	match_rec * matchC = scheduler.AddMrec( "C", "peerC", & jobC, & matchAd, "tlmiller", NULL );

	if(! pcccNew( jobA )) { return __LINE__; }
	if(! pcccNew( jobB )) { return __LINE__; }

	pcccWants( jobA, matchA );
	pcccWants( jobB, matchB );

	if(! pcccNew( jobC )) { return __LINE__; }

	pcccGot( jobB, matchA );
	pcccGot( jobA, matchC );

	if( pcccSatisfied( jobA ) ) { return __LINE__; }
	if( pcccSatisfied( jobB ) ) { return __LINE__; }

	pcccWants( jobC, matchC );
	pcccGot( jobA, matchA );

	if( pcccSatisfied( jobC ) ) { return __LINE__; }

	pcccWants( jobA, matchC );
	if(! pcccSatisfied( jobA )) { return __LINE__; }

	pcccWants( jobB, matchA );
	pcccGot( jobB, matchB );
	if(! pcccSatisfied( jobB )) { return __LINE__; }

	pcccGot( jobC, matchC );
	if(! pcccSatisfied( jobC )) { return __LINE__; }

	return 0;
}

bool pcccTest() {
	// Basic accounting test.
	int line = accounting_single( PROC_ID( 1, 0 ) );
	if( line != 0 ) {
		dprintf( D_ALWAYS, "pcccTest(): Failed accounting_single( 1.0 ) at line %d.\n", line );
		return false;
	}
	dprintf( D_ALWAYS, "pcccTest(): Passed accounting_single( 1.0 ).\n" );

	// Repeat it for another cluster.
	line = accounting_single( PROC_ID( 2, 0 ) );
	if( line != 0 ) {
		dprintf( D_ALWAYS, "pcccTest(): Failed accounting_single( 2.0 ) at line %d.\n", line );
		return false;
	}
	dprintf( D_ALWAYS, "pcccTest(): Passed accounting_single( 2.0 ).\n" );

	// Repeat it for another proc ID.
	line = accounting_single( PROC_ID( 2, 1 ) );
	if( line != 0 ) {
		dprintf( D_ALWAYS, "pcccTest(): Failed accounting_single( 2.1 ) at line %d.\n", line );
		return false;
	}
	dprintf( D_ALWAYS, "pcccTest(): Passed accounting_single( 2.1 ).\n" );

	// Interleave three different now jobs.
	line = accounting_multiple( PROC_ID( 3, 0 ), PROC_ID( 3, 1 ), PROC_ID( 4, 0 ) );
	if( line != 0 ) {
		dprintf( D_ALWAYS, "pcccTest(): Failed accounting_multiple( 3.0, 3.1, 4.0 ) at line %d.\n", line );
		return false;
	}
	dprintf( D_ALWAYS, "pcccTest(): Passed accounting_multiple( 3.0, 3.1, 3.2 ).\n" );

	// We can't really test coalescing except functionally (or by writing
	// a DCStartd mock object, which seems like more work than its worth it,
	// given that we want a functional test anyway).

	// We could test the clean-up of a failed coalesce by registering a timer
	// for 21 seconds from now, but faking all of those match records well
	// enough to prevent the schedd from falling over in the interim seems
	// like it would be rather tricky.  The test would have to check that:
	//		* the now jobs' entries in all four maps have been erased;
	//		* bone of the match records we called pcccGot() on exist;
	//		* and the other match records we created have an invalid m_now_job.
	// We'd have to look up match[G,H,I] up in the schedd's usual data
	// structures (and verify that match[A-F] aren't in there).

	dprintf( D_ALWAYS, "pcccTest(): Succeeded.\n" );
	return true;
}
