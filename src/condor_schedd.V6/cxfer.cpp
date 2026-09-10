#include "condor_common.h"
#include "condor_debug.h"

#include "scheduler.h"
#include "catalog_utils.h"
#include "dc_coroutines.h"
#include "cxfer.h"

#include "qmgmt.h"
#include "condor_qmgr.h"

#include "condor_daemon_client.h"

extern Scheduler scheduler;


std::tuple<
	CXFER_TYPE,
	std::optional<ListOfCatalogs>
>
determine_cxfer_type( match_rec * m_rec, const PROC_ID & jobID ) {
	//
	// We'd like to start looking at the job _after_ we check the FORBIDDN
	// and CANT conditions, but until I can prove that falling back works
	// properly when re-using a shadow, we have to check the NONE condition
	// first to avoid not recycling a shadow when we otherwise could.
	//

	// Now we start looking at the job ad.
	ClassAd * jobAd = GetJobAd(jobID);
	if( jobAd == NULL ) {
		dprintf( D_ERROR, "cxfer: Failed to obtain job ad, falling back to uncommon transfer.\n" );
		return {CXFER_TYPE::CANT, {}};
	}

	auto common_file_catalogs = computeCommonInputFileCatalogs( jobAd, m_rec->peer );
	if(! common_file_catalogs) {
		dprintf( D_ERROR, "cxfer: Failed to construct unique name(s) for catalog(s), falling back to uncommon transfer.\n" );
		return {CXFER_TYPE::CANT, {}};
	}
	// Even though we don't (presently) care if the common files could be
	// transferred by a slightly older version of the starter, we still need
	// call this function to add `MY.CommonFiles` to the list of catalogs.
	int required_version = 2;
	if(! computeCommonInputFiles( jobAd, m_rec->peer, * common_file_catalogs, required_version )) {
		dprintf( D_ERROR, "cxfer: Failed to construct unique name for " ATTR_COMMON_INPUT_FILES " catalog, falling back to uncommon transfer.\n" );
		return {CXFER_TYPE::CANT, {}};
	}
	// If we don't find any common file catalogs, return CXFER_TYPE::None.
	if( common_file_catalogs->size() == 0 ) {
		return {CXFER_TYPE::NONE, {}};
	}


	// Admins can disable all common file transfers.
	bool disallowed = param_boolean("FORBID_COMMON_FILE_TRANSFER", false);
	if( disallowed ) {
		return {CXFER_TYPE::FORBIDDEN, {}};
	}


	//
	// We don't coalesce OCUs, so we shouldn't split them, either.
	//
	if( m_rec->is_ocu ) {
		return {CXFER_TYPE::CANT, {}};
	}


	// The match ad (`m_rec->my_match_ad`) already has the `CondorVersion`
	// and `HasCommonFilesTransfer` attributes, so we can check capability;
	// we're assuming that the starter has the same version as the startd.
	std::string starter_version;
	m_rec->my_match_ad->LookupString( ATTR_VERSION, starter_version );
	CondorVersionInfo cvi( starter_version.c_str() );
	if(! cvi.built_since_version(25, 2, 0)) {
		return {CXFER_TYPE::CANT, {}};
	}

	int hasCommonFilesTransfer = 0;
	m_rec->my_match_ad->LookupInteger(
		ATTR_HAS_COMMON_FILES_TRANSFER, hasCommonFilesTransfer
	);
	if( hasCommonFilesTransfer < 2 ) {
		return {CXFER_TYPE::CANT, {}};
	}

	//
	// Only in 25.10 and later could we map from staging directories on LVs.
	//
	std::string lvmBackingStore;
	m_rec->my_match_ad->LookupString(
		ATTR_LVM_BACKING_STORE, lvmBackingStore
	);
	if(! lvmBackingStore.empty()) {
		if(! cvi.built_since_version(25, 10, 0)) {
			return {CXFER_TYPE::CANT, {}};
		}
	}

	//
	// We can't split static slots, so don't even try.
	//
	std::string slotType;
	m_rec->my_match_ad->LookupString(
		ATTR_SLOT_TYPE, slotType
	);
	if( MATCH == strcasecmp(slotType.c_str(), "Static") ) {
		return {CXFER_TYPE::CANT, {}};
	}


	//
	// (HTCONDOR-3641)  See the note in schedd.cpp.
	//
	int clusterID = jobID.cluster;
	GetAttributeInt( jobID.cluster, jobID.proc, ATTR_DAGMAN_JOB_ID, & clusterID );

	int commonTransferFailed = FALSE;
	GetAttributeInt( clusterID, -1, "CommonTransferFailed", & commonTransferFailed );
	if( commonTransferFailed ) {
		return {CXFER_TYPE::CANT, {}};
	}


	//
	// We now have a list of all common the file catalogs for this job.
	//
	// (a)  If any catalog has not started staging, return CXFER_TYPE::STAGING.
	// (b)  If each catalog has been staged, return CXFER_TYPE::READY.
	// (c)  Otherwise, at least one catalog has started staging but not
	//      finished; return CXFER_TYPE::MAPPING.
	//

	size_t staged = 0;
	size_t staging = 0;

	for( const auto & [cifName, commonInputFiles] : * common_file_catalogs ) {
		// The cifName is unique in this AP's LOCK directory, so a
		// single-level lookup table is sufficient.
		auto entry = scheduler.getShadowForCatalog( cifName );
		if( entry ) {
			switch((*entry)->cxfer_state) {
				case CXFER_STATE::INVALID:
					dprintf( D_ERROR, "cxfer: Common transfer state unset in transfer shadow record, falling back to uncommon transfer.\n" );
					return {CXFER_TYPE::CANT, common_file_catalogs};
				case CXFER_STATE::STAGING:
					++staging;
					continue;
				case CXFER_STATE::STAGED:
					++staged;
					continue;
				case CXFER_STATE::MAPPING:
					// Then we have become terribly confused somehow.
					dprintf( D_ERROR, "cxfer: Common transfer state set to waiting in transfer shadow record, falling back to uncommon transfer.\n" );
					return {CXFER_TYPE::CANT, common_file_catalogs};
				case CXFER_STATE::RETIRING:
					// This transfer shadow's lease expired and we have asked
					// it to retire, but we haven't reaped it yet.
					return {CXFER_TYPE::RETIRING, common_file_catalogs};
			}
		} else {
			return {CXFER_TYPE::STAGING, common_file_catalogs};
		}
	}

	if( staged == common_file_catalogs->size() ) {
		return {CXFER_TYPE::READY, common_file_catalogs};
	} else if( staging != 0 ) {
		return {CXFER_TYPE::MAPPING, common_file_catalogs};
	} else {
		// Then we have become terribly confused somehow.
		dprintf( D_ERROR, "cxfer: Inconsistency in common file catalog: all entries were either staging or staged, but the sum of those two states is not the total size.  Falling back to uncommon transfer.\n" );
		return {CXFER_TYPE::CANT, common_file_catalogs};
	}
}


condor::cr::void_coroutine
command_data_slot_callback(
	Sock * sock,
	std::string originaClaimID,
	ClassAd requestAd,
	shadow_rec * srec
);


void
call_StartJobFailure( const std::string & claimID, shadow_rec * srec ) {
    // We shouldn't have to do this separately now that we're explicitly
    // tracking (by passing around) the transfer shadow's record, but
    // unregistering the shadow catalogs as soon as possible results in
    // fewer jobs blocking for a transfer shadow that will never start.
	if( srec ) {
		// Since this shadow failed to start, its PID field should still be 0.
		scheduler.unregister_shadow_catalogs( srec, 0 );
	}

	//
	// There's a race condition here.  StartJobFailed() call del_mrec(),
	// which calls unlink_mrec(), which calls send_vacate().  This can
	// return resources to the startd before it replies to (or is contacted
	// by?) a REQUEST_CLAIM command issued against its partitionable slot
	// left-overs... which can put the schedd into a busy loop of requesting
	// a d-slot, vacating it, and then requesting it again.
	//
	// Instead, let's wait a few seconds before vacating the claim.
	//

	auto lambda = [claimID, srec](int /* timerID */) -> void {
	    //
	    // We (try) to delete the match record first, because there are a
	    // few things that are done differently in unlinkMrec() if there's
	    // a registered shadow record.
	    //
		match_rec * mrec = scheduler.FindMrecByClaimID( claimID.c_str() );
		if( mrec != nullptr ) {
			PROC_ID id( mrec->jid.cluster, transferToPromptingProcID(mrec->jid.proc) );
			dprintf( D_VERBOSE, "call_StartJobFailure(): deleting match record after failure to create data slot.\n" );
			scheduler.StartJobFailed( mrec, id );
		} else {
			dprintf( D_VERBOSE, "call_StartJobFailure(): did not find match record for claim ID '%s'\n", claimID.c_str() );
		}

		// The way shadow record lifetimes are managed, any code which runs
		// before a shadow gets a PID is responsible for cleaning the shadow
		// record up.  (After, the shadow's reaper must clean up its record.)
		//
		// This should be safe -- this shadow record hasn't been added to the
		// queue of shadows to start yet, and as such nobody else will ever
		// try to delete it -- but there's extra magic there now JIC.
		if( srec ) {
			scheduler.delete_shadow_rec( srec );
		}
	};

	std::ignore = daemonCore->Register_Timer(
		3,              // delay
		TIMER_NEVER,    // repeat
		lambda,
		"call_startJobFailure"
	);

}


//
// If this were the coroutine it should be, we'd need to worry about the
// lifetime of the `mrec` pointer; but see what we're already doing to avoid
// having to do so.  Likewise, we'd need a copy of the `requestAd`.
//
void
start_command_data_slot( match_rec * mrec, const ClassAd & requestAd ) {
	// dprintf( D_ALWAYS, "start_command_data_slot(): begin.\n" );

	// We're responsible for deleting this shadow record right up until
	// we call scheduler.addRunnableJob(), and we can't depend on the match
	// record being there or remembering the shadow record later.
	shadow_rec * srec = mrec->shadowRec;

	CondorError errorStack;
	DCStartd startd( mrec->peer, nullptr );

	ClaimIdParser cidp( mrec->claimId() );
	char const * sessionID = cidp.secSessionId();
	// dprintf( D_ALWAYS, "start_command_data_slot(): using sessionID %s\n", sessionID );

	std::string originalClaimID = mrec->claimId();
	auto result = startd.startCommand_nonblocking(
		COMMAND_DATA_SLOT,
		Sock::reli_sock,
		20 /* seconds of careful research */,
		/* & errorStack, */ // We'll figure out the lifetime of this later.
		nullptr,
		[originalClaimID, requestAd, srec](
			bool success, Sock * sock, CondorError * errorStack,
			const std::string & /* trust_domain */,
			bool /* should_try_token_request */
		) -> void {
			if( success ) {
				command_data_slot_callback( sock, originalClaimID, requestAd, srec );
			} else {
				dprintf( D_ALWAYS,
					"start_command_data_slot(): startCommand(COMMAND_DATA_SLOT): failed: %s.\n",
					errorStack == nullptr ? "no error stack" : errorStack->getFullText().c_str()
				);

				call_StartJobFailure( originalClaimID, srec );
				return;
			}
		},
		"description: COMMAND_DATA_SLOT",
		false /* not raw protocol */,
		sessionID
	);

	switch (result) {
		case StartCommandFailed: {
			dprintf( D_ALWAYS, "start_command_data_slot(): startCommand(COMMAND_DATA_SLOT) failed.\n" );
			call_StartJobFailure( originalClaimID, srec );
			} return;
		case StartCommandSucceeded:  /* that was quick */
			break;
		case StartCommandInProgress: /* as prophesied */
			break;
		case StartCommandWouldBlock: /* impossible */
			break;
		case StartCommandContinue:   /* impossible */
			break;
	}

	// dprintf( D_ALWAYS, "start_command_data_slot(): end.\n" );
}


//
// The `sock`et needs to live on the heap, and this function needs to control
// its lifetime.  These requriements appear to be guaranteed by the
// startCommand_nonblocking() callback API.
//
// The `originalClaimID` and `requestAd` are copies so that we don't have
// to think about lifetime and ownership.
//
// The shadow rec pointer is dangerous, but the schedd doesn't presently
// have an indirect way of doing the look-up and avoiding the risk of it
// having been deleted by the time we get called.  (This is the same way
// that the schedd manages shadow records while they're in the start
// queue, but that's also perilous.)  Because the match record might vanish
// or be altered (removing the shadow pointer), we can't depend on it to
// make sure we clean up the shadow record.  (Leaking a shadow record is
// bad; the actual problem is incorrectly blocking jobs because the catalog
// to shadow map is outdated.)
//
condor::cr::void_coroutine
command_data_slot_callback(
	Sock * sock,
	std::string originalClaimID,
	ClassAd requestAd,
	shadow_rec * srec
) {
	auto scope_guard = std::unique_ptr<Sock>(sock);

	ClassAd commandAd;
	commandAd.InsertAttr( ATTR_CLAIM_ID, originalClaimID );
	commandAd.InsertAttr( "DesiredSlotPrefix", "data" );

	if(! putClassAd( sock, commandAd )) {
		dprintf( D_ALWAYS, "start_command_data_slot(): could not putClassAd(commandAd).\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}

	if(! putClassAd( sock, requestAd )) {
		dprintf( D_ALWAYS, "start_command_data_slot(): could not putClassAd(requestAd).\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}

	if(! sock->end_of_message()) {
		dprintf( D_ALWAYS, "start_command_data_slot(): could not end message (put).\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}


	//
	// This is where the magic happens.  We assume that writes don't block
	// (not sure about how that works with EWOULDBLOCK on connect), so we
	// only need to return to the event loop before waiting for the reply.
	//

	//
	// The whole point of the AwaitableDeadlineSocket is that it returns
	// back to the event loop, which means any pointer that we carry past
	// co_await() call may no longer be valid.  For match records, we work
	// around this by looking them up by their claim ID; if someone's
	// deleted it, too bad, so sad, that means they did the clean-up.
	//
	// There's no equivalent map of all shadow records, and changing
	// shadowsByProcID (which seems like it ought to be that, given that
	// there's shadowsByPID for shadow records which have PIDs) to be that
	// map founders in a mess of weird semantics.  We _believe_ that nobody
	// else will clean up our shadow record but for now we've added some
	// memory-intensive magic to the shadow_rec constructor and destructor
	// so that we don't ever actually try to clean one up twice.
	//

	// dprintf( D_ALWAYS, "start_command_data_slot(): waiting for reply.\n" );
	condor::dc::AwaitableDeadlineSocket ads;
	const int reply_timeout = 20;
	ads.deadline( sock, reply_timeout );
	auto [_, timed_out] = co_await(ads);
	ASSERT(_ == sock || timed_out);

	if( timed_out ) {
		dprintf( D_ALWAYS, "start_command_data_slot(): timed out.\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}


	ClassAd replyAd;
	if(! getClassAd( sock, replyAd )) {
		dprintf( D_ALWAYS, "start_command_data_slot(): could not getClassAd(replyAd).\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}

	ClassAd newSlotAd;
	if(! getClassAd( sock, newSlotAd )) {
		dprintf( D_ALWAYS, "start_command_data_slot(): could not getClassAd(newSlotAd).\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}
	if(! sock->end_of_message()) {
		dprintf( D_ALWAYS, "start_command_data_slot(): could not end message (get).\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}


	std::string resultString;
	replyAd.LookupString( ATTR_RESULT, resultString );
	CAResult result = getCAResultNum( resultString.c_str() );
	if( result != CA_SUCCESS ) {
		dprintf( D_ALWAYS, "start_command_data_slot(): result was not success\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}

	// We shouldn't ever need this in anger.
	std::string claimIDString;
	if(! replyAd.LookupString( ATTR_CLAIM_ID, claimIDString )) {
		dprintf( D_ALWAYS, "start_command_data_slot(): result did not contain claim ID.\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}
	if( claimIDString != originalClaimID ) {
		dprintf( D_ALWAYS, "start_command_data_slot(): startd erroneously returned new claim ID.\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}


	//
	// The slot is now a data slot; find the corresponding match record,
	// update it, and start a transfer shadow on it.
	//
	match_rec * mrec = scheduler.FindMrecByClaimID( originalClaimID.c_str() );
	if( mrec == nullptr ) {
		dprintf( D_ALWAYS, "command_data_slot(): startCommand(COMMAND_DATA_SLOT, ...) returned but corresponding match record no longer exists.\n" );
		call_StartJobFailure( originalClaimID, srec );
		co_return;
	}

	// Stolen from Scheduler::claimedStartd(); we should probably refactor.
	mrec->my_match_ad->CopyFrom(newSlotAd);
	mrec->my_match_ad->Update(mrec->m_added_attrs);
	mrec->makeDescription();

	// And -- finally -- schedule the transfer shadow to to be spawned.
	scheduler.addRunnableJob( mrec->shadowRec );


	// dprintf( D_ALWAYS, "start_command_data_slot(): success!\n" );
	co_return;
}
