#include "condor_common.h"
#include "condor_debug.h"

#include "scheduler.h"
#include "cxfer.h"
#include "catalog_utils.h"

#include "qmgmt.h"


CXFER_TYPE
determine_cxfer_type( match_rec * m_rec, PROC_ID * jobID ) {
	// Admins can disable all common file transfers.
	bool disallowed = param_boolean("FORBID_COMMON_FILE_TRANSFER", false);
	if( disallowed ) {
		return CXFER_TYPE::FORBIDDEN;
	}


	// The match ad (`m_rec->my_match_ad`) already has the `CondorVersion`
	// and `HasCommonFilesTransfer` attributes, so we can check capability;
	// we're assuming that the starter has the same version as the startd.
	std::string starter_version;
	m_rec->my_match_ad->LookupString( ATTR_VERSION, starter_version );
	CondorVersionInfo cvi( starter_version.c_str() );
	if(! cvi.built_since_version(25, 2, 0)) {
		return CXFER_TYPE::CANT;
	}

	int hasCommonFilesTransfer = 0;
	m_rec->my_match_ad->LookupInteger(
		ATTR_HAS_COMMON_FILES_TRANSFER, hasCommonFilesTransfer
	);
	if( hasCommonFilesTransfer < 2 ) {
		return CXFER_TYPE::CANT;
	}


	// Now we start looking at the job ad.  If we don't find any common file
	// catalogs, return CXFER_TYPE::None.
	ClassAd * jobAd = GetJobAd(* jobID);
	auto common_file_catalogs = computeCommonInputFileCatalogs( jobAd, m_rec->peer );
	if(! common_file_catalogs) {
		dprintf( D_ERROR, "Failed to construct unique name(s) for catalog(s), falling back to uncommon transfer.\n" );
		return CXFER_TYPE::CANT;
	}
	// Even though we don't (presently) care if the common files could be
	// transferred by a slightly older version of the starter, we still need
	// call this function to add `MY.CommonFiles` to the list of catalogs.
	int required_version = 2;
	if(! computeCommonInputFiles( jobAd, m_rec->peer, * common_file_catalogs, required_version )) {
		dprintf( D_ERROR, "Failed to constructo unique name for " ATTR_COMMON_INPUT_FILES " catalog, falling back to uncommon transfer.\n" );
		return CXFER_TYPE::CANT;
	}


	// We now have a list of all common the file catalogs for this job.  If
	// any catalog has not started staging, return CXFER_TYPE::STAGING.  If
	// any catalog has not finished staging, return CXFER_TYPE::MAPPING.  If
	// all of them have been staged, return CXFER_TYPE::READY.
	for( const auto & [cifName, commonInputFiles] : * common_file_catalogs ) {
		dprintf( D_ZKM, "%s = %s\n", cifName.c_str(), commonInputFiles.c_str() );

        // ...
	}

	return CXFER_TYPE::NONE;
}
