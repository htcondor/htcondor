#include "condor_common.h"
#include "condor_debug.h"

#include <string>
#include "classad/classad.h"
#include "guidance.h"

#include "condor_attributes.h"
#include "stl_string_utils.h"

std::string
makeCIFName( const classad::ClassAd & jobAd ) {
    // FIXME: error-handling, all throughout.

    std::string globalJobID;
    jobAd.LookupString( ATTR_GLOBAL_JOB_ID, globalJobID );
    auto sections = split( globalJobID, "#" );

    int clusterID = -1;
    jobAd.LookupInteger( ATTR_CLUSTER_ID, clusterID );

    std::string cifName;
    formatstr( cifName, "%s#%d\n", sections[0].c_str(), clusterID );
    return cifName;
}
