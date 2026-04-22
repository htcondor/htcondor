#include "condor_common.h"
#include "condor_debug.h"

#include "scheduler.h"
#include "catalog_utils.h"
#include "cxfer.h"

#include "qmgmt.h"
#include "condor_qmgr.h"

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
	// It might be wiser to have the startd not advertise that it
	// `hasCommonFilesTransfer` if it can't successfully map any
	// catalog it has thus stored, but since we want to deal with
	// older startds anyway, we'll just check here for now.
	//
	std::string lvmBackingStore;
	m_rec->my_match_ad->LookupString(
		ATTR_LVM_BACKING_STORE, lvmBackingStore
	);
	if(! lvmBackingStore.empty()) {
		return {CXFER_TYPE::CANT, {}};
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
