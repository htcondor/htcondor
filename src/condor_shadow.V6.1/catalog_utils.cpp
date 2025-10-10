#include "condor_common.h"
#include "condor_debug.h"

#include <utility>
#include <string>
#include <optional>
#include "compat_classad.h"
#include "baseshadow.h"
#include "catalog_utils.h"

#include "stl_string_utils.h"

std::optional<ListOfCatalogs>
computeCommonInputFileCatalogs( ClassAd * jobAd, BaseShadow * shadow ) {
	ListOfCatalogs common_file_catalogs;

	//
	// Which common files, if any, were we asked for?
	//
	std::string commonInputCatalogs;
	jobAd->LookupString("_x_common_input_catalogs", commonInputCatalogs);
	for( const auto & cifName : StringTokenIterator(commonInputCatalogs) ) {
		std::string commonInputFiles;
		jobAd->LookupString( "_x_catalog_" + cifName, commonInputFiles );

		auto internal_catalog_name = shadow->uniqueCIFName(cifName, commonInputFiles);
		if(! internal_catalog_name) {
			return {};
		}

		common_file_catalogs.push_back({* internal_catalog_name, commonInputFiles});
		// dprintf( D_ZKM, "Found common file catalog '%s' = '%s'\n", cifName.c_str(), commonInputFiles.c_str() );
	}

	return common_file_catalogs;
}


bool
computeCommonInputFiles( ClassAd * jobAd, BaseShadow * shadow, ListOfCatalogs & common_file_catalogs, int & required_version ) {
	std::string common_input_files;
	bool found_htc25_plumbing =
		jobAd->LookupString( ATTR_COMMON_INPUT_FILES, common_input_files );

	if( common_file_catalogs.empty() && found_htc25_plumbing ) {
		required_version = 1;
	}

	// Contrary to best practice, this may partially modify the i/o argument.
	if( found_htc25_plumbing ) {
		std::string default_name;
		long long int clusterID = 0;
		ASSERT( jobAd->LookupInteger( ATTR_CLUSTER_ID, clusterID ) );
		formatstr( default_name, "clusterID_%lld", clusterID );
		auto internal_catalog_name = shadow->uniqueCIFName(default_name, common_input_files);
		if(! internal_catalog_name) {
			return false;
		}
		common_file_catalogs.push_back({* internal_catalog_name, common_input_files});
	}

	return true;
}
