#include "condor_common.h"
#include "condor_debug.h"

#include <string>
#include "classad/classad.h"
#include "guidance.h"

#include "condor_attributes.h"
#include "stl_string_utils.h"


//
// The EP needs to distinguish between common filesets.  For the moment,
// we're only sharing common files within the same cluster, so we just need
// the job's cluster ID and the schedd to which that cluster ID is unique.
//
// (The global job ID is proc-specific, and also includes a date that may
// will not be the same for all procs in some cases, so we can't use that.
// There's no (globally-unique) schedd identifier in the job ad, so we'll
// just use the first part of the global job ID.  We're not presently
// worried about cluster ID re-use because of a schedd reinstallation
// confusing anything.)
//

std::string
makeCIFName( const classad::ClassAd & jobAd ) {
    // FIXME: error-handling, all throughout.

    std::string globalJobID;
    jobAd.LookupString( ATTR_GLOBAL_JOB_ID, globalJobID );
    auto sections = split( globalJobID, "#" );

    int clusterID = -1;
    jobAd.LookupInteger( ATTR_CLUSTER_ID, clusterID );

    std::string cifName;
    formatstr( cifName, "%s#%d", sections[0].c_str(), clusterID );

    return cifName;
}
