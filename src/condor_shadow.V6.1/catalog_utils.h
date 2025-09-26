#ifndef   _CONDOR_CATALOG_UTILS_H
#define   _CONDOR_CATALOG_UTILS_H


// #include <utility>
// #include <string>
// #include <optional>
// #include "compat_classad.h"
// #include "shadow.h"

typedef std::pair< std::string, std::string > FileCatalog;

std::optional<std::vector<FileCatalog>> computeCommonInputFileCatalogs( ClassAd * jobAd, UniShadow * self );
bool computeCommonInputFiles( ClassAd * jobAd, UniShadow * self, std::vector<FileCatalog> & commonFileCatalogs, int & required_version );


#endif /* _CONDOR_CATALOG_UTILS_H */
