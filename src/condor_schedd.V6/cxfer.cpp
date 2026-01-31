#include "condor_common.h"
#include "condor_debug.h"

#include "scheduler.h"
#include "catalog_utils.h"
#include "cxfer.h"

#include "qmgmt.h"


extern Scheduler scheduler;


std::tuple<
	CXFER_TYPE,
	std::optional<ListOfCatalogs>
>
determine_cxfer_type( match_rec * m_rec, PROC_ID * jobID ) {
	// Admins can disable all common file transfers.
	bool disallowed = param_boolean("FORBID_COMMON_FILE_TRANSFER", false);
	if( disallowed ) {
		return {CXFER_TYPE::FORBIDDEN, {}};
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


	// Now we start looking at the job ad.
	ClassAd * jobAd = GetJobAd(* jobID);
	auto common_file_catalogs = computeCommonInputFileCatalogs( jobAd, m_rec->peer );
	if(! common_file_catalogs) {
		dprintf( D_ERROR, "Failed to construct unique name(s) for catalog(s), falling back to uncommon transfer.\n" );
		return {CXFER_TYPE::CANT, {}};
	}
	// Even though we don't (presently) care if the common files could be
	// transferred by a slightly older version of the starter, we still need
	// call this function to add `MY.CommonFiles` to the list of catalogs.
	int required_version = 2;
	if(! computeCommonInputFiles( jobAd, m_rec->peer, * common_file_catalogs, required_version )) {
		dprintf( D_ERROR, "Failed to constructo unique name for " ATTR_COMMON_INPUT_FILES " catalog, falling back to uncommon transfer.\n" );
		return {CXFER_TYPE::CANT, {}};
	}
	// If we don't find any common file catalogs, return CXFER_TYPE::None.
	if( common_file_catalogs->size() == 0 ) {
		return {CXFER_TYPE::NONE, {}};
	}


	// We now have a list of all common the file catalogs for this job.  If
	// any catalog has not started staging, return CXFER_TYPE::STAGING.  If
	// any catalog has not finished staging, return CXFER_TYPE::MAPPING.  If
	// all of them have been staged, return CXFER_TYPE::READY.
	for( const auto & [cifName, commonInputFiles] : * common_file_catalogs ) {
		dprintf( D_ALWAYS, "%s = %s\n", cifName.c_str(), commonInputFiles.c_str() );

		// I don't know how the schedd decides which shadows to spawn when
		// it does reconnects after a fast shutdown, so I don't know how much
		// of this will have to be changed to deal with that.  However, since
		// this case doesn't work with the previous shadow logic anyway, we
		// can save that for later.

		// The cifName is unique in this AP's LOCK directory, so a
		// single-level lookup table is sufficient.
		auto entry = scheduler.getShadowForCatalog( cifName );
		if( entry ) {
			switch((*entry)->cxfer_state) {
				case CXFER_STATE::INVALID:
					dprintf( D_ERROR, "Common transfer state unset in transfer shadow record, falling back to uncommon transfer.\n" );
					return {CXFER_TYPE::CANT, common_file_catalogs};
				case CXFER_STATE::STAGING:
					return {CXFER_TYPE::MAPPING, common_file_catalogs};
				case CXFER_STATE::STAGED:
					continue;
				case CXFER_STATE::MAPPING:
					// Then we have become terribly confused somehow.
					dprintf( D_ERROR, "Common transfer state set to waiting in transfer shadow record, falling back to uncommon transfer.\n" );
					return {CXFER_TYPE::CANT, common_file_catalogs};
			}
		} else {
			return {CXFER_TYPE::STAGING, common_file_catalogs};
		}
	}

	return {CXFER_TYPE::READY, common_file_catalogs};
}
