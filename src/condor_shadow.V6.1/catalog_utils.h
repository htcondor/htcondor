#ifndef   _CONDOR_CATALOG_UTILS_H
#define   _CONDOR_CATALOG_UTILS_H


// #include <utility>
// #include <string>
// #include <optional>
// #include "compat_classad.h"
// #include "baseshadow.h"

using ListOfCatalogs = std::vector< std::pair< std::string, std::string > >;

std::optional<ListOfCatalogs> computeCommonInputFileCatalogs( ClassAd * jobAd, BaseShadow * shadow );
bool computeCommonInputFiles( ClassAd * jobAd, BaseShadow * shadow, ListOfCatalogs & commonFileCatalogs, int & required_version );


#endif /* _CONDOR_CATALOG_UTILS_H */
