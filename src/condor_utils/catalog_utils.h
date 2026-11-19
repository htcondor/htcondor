#ifndef   _CONDOR_CATALOG_UTILS_H
#define   _CONDOR_CATALOG_UTILS_H


// #include <utility>
// #include <string>
// #include <optional>
// #include "compat_classad.h"

using ListOfCatalogs = std::vector< std::pair< std::string, std::string > >;

std::optional<ListOfCatalogs>
computeCommonInputFileCatalogs(
	ClassAd * jobAd,
	const std::string & startdAddress,
	std::map< std::string, std::string > * internalToSimpleNameMap = NULL
);

bool
computeCommonInputFiles(
	ClassAd * jobAd,
	const std::string & startdAddress,
	ListOfCatalogs & commonFileCatalogs,
	int & required_version,
	std::map< std::string, std::string > * internalToSimpleNameMap = NULL
);


#endif /* _CONDOR_CATALOG_UTILS_H */
