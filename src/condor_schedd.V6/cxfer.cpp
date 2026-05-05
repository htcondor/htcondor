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


//
// FIXME: This can't actually take a (bare) pointer, for lifetime-management
// reasons.  Instead, it needs to take a copy and then look for the original
// (using FindMrecByClaimID()) after it get backs from the event loop.
//
condor::cr::void_coroutine
start_conversion_to_data_slot( match_rec * mrec, ClassAd requestAd ) {
	dprintf( D_ALWAYS, "start_conversion_to_data_slot(): begin.\n" );

	ClassAd commandAd;
	commandAd.InsertAttr( ATTR_CLAIM_ID_LIST, mrec->claim_id.claimId() );
	commandAd.InsertAttr( "DesiredSlotPrefix", "data" );


	// dprintf( D_ALWAYS, "start_conversion_to_data_slot(): connecting to %s\n", mrec->peer );
	DCStartd startd( mrec->peer, nullptr );

	CondorError errorStack;
	const int forever = 0;
	const int connect_timeout = 20;
	const bool non_blocking = true;
	const bool blocking = false;
	ReliSock * sock = startd.reliSock(
		connect_timeout, forever, & errorStack, blocking
	);
	if( sock == nullptr) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not reliSock().\n" );
		co_return;
	}

	// FIXME: This version of startCommand() is blocking; apparently the
	// non_blocking above only applies to the initial connection.  Looks like
	// it's time for AwaitableStartCommand().  sigh.
	if(! startd.startCommand( COMMAND_DATA_SLOT, sock, 20, & errorStack )) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not startCommand().\n" );
		dprintf( D_ALWAYS, "maybe-because: %s\n", errorStack.getFullText().c_str() );
		co_return;
	}

	if(! startd.forceAuthentication( sock, & errorStack )) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not forceAuthentication().\n" );
		co_return;
	}

	sock->encode();
	if(! putClassAd( sock, commandAd )) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not putClassAd(commandAd).\n" );
		co_return;
	}

	if(! putClassAd( sock, requestAd )) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not putClassAd(requestAd).\n" );
		co_return;
	}

	if(! sock->end_of_message()) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not end message.\n" );
		co_return;
	}


	//
	// This is where the magic happens.  We assume that writes don't block
	// (not sure about how that works with EWOULDBLOCK on connect), so we
	// only need to return to the event loop before waiting for the reply.
	//
/*
	dprintf( D_ALWAYS, "start_conversion_to_data_slot(): waiting for reply.\n" );
	condor::dc::AwaitableDeadlineSocket ads;
	const int reply_timeout = 20;
	ads.deadline( sock, reply_timeout );
	auto [_, timed_out] = co_await(ads);
	ASSERT(_ == sock || timed_out);

	if( timed_out ) {
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): timed out.\n" );
		co_return;
	}
*/

dprintf( D_ALWAYS, "start_conversion_to_data_slot(): 7\n" );
	ClassAd replyAd;
	if(! getClassAd( sock, replyAd )) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not getClassAd(replyAd).\n" );
		co_return;
	}

dprintf( D_ALWAYS, "start_conversion_to_data_slot(): 8\n" );
	ClassAd newSlotAd;
	if(! getClassAd( sock, newSlotAd )) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not getClassAd(newSlotAd).\n" );
		co_return;
	}
	// FIXME: do something with the new machine ad.
	// ... anything other than just update the new mrec?

	if(! sock->end_of_message()) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): could not end message.\n" );
		co_return;
	}


dprintf( D_ALWAYS, "start_conversion_to_data_slot(): 9\n" );
	std::string resultString;
	replyAd.LookupString( ATTR_RESULT, resultString );
	CAResult result = getCAResultNum( resultString.c_str() );
	if( result != CA_SUCCESS ) {
		// ... FIXME ...
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): result was not success\n" );
		co_return;
	}

dprintf( D_ALWAYS, "start_conversion_to_data_slot(): 10\n" );
	// We shouldn't ever need this in anger.
	std::string claimIDString;
	if(! replyAd.LookupString( ATTR_CLAIM_ID, claimIDString )) {
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): result did not contain claim ID.\n" );
		co_return;
	}
	if( claimIDString != mrec->claim_id.claimId() ) {
		// The point of preserving the claim ID is how much of a pain it
		// would be to fix up all of the schedd's internal book keeping.
		dprintf( D_ALWAYS, "start_conversion_to_data_slot(): startd erroneously returned new claim ID.\n" );
		co_return;
	}


	//
	// ... FIXME ...
	//
	// Do all the things.
	//


	dprintf( D_ALWAYS, "start_conversion_to_data_slot(): success!\n" );
	co_return;
}
