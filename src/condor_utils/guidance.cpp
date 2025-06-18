#include "condor_common.h"
#include "condor_debug.h"

#include <string>
#include "classad/classad.h"

#include <optional>
#include "guidance.h"

#include "condor_attributes.h"
#include "stl_string_utils.h"

#include <openssl/evp.h>
#include "AWSv4-impl.h"


//
// The EP needs to distinguish between common filesets.  For the moment,
// we're only sharing common files within the same cluster, so we just need
// the job's cluster ID and the schedd to which that cluster ID is unique.
//
// (The global job ID is proc-specific, and also includes a date that may
// not be the same for all procs in some cases, so we can't use that.
// There's no (globally-unique) schedd identifier in the job ad, so we'll
// just use the first part of the global job ID.  We're not presently
// worried about cluster ID re-use because of a schedd reinstallation
// confusing anything.)
//
// Arguably, it should be the shadow's responsibility to combine the CIF
// name with the startd's address to generate the key for shadow IPC.
//

std::optional<std::string>
makeCIFName(
    const classad::ClassAd & jobAd, const std::string & startdAddress
) {
    std::string globalJobID;
    if(! jobAd.LookupString( ATTR_GLOBAL_JOB_ID, globalJobID )) {
        return std::nullopt;
    }
    auto sections = split( globalJobID, "#" );

    // If the submitter gave us a name to use, scope with their name and
    // the name of the schedd, but use it instead of the cluster ID.
    std::string userSuppliedName;
    if( jobAd.LookupString( "CIFName", userSuppliedName ) ) {
        std::string submitter;
        if(! jobAd.LookupString( ATTR_USER, submitter )) {
            return std::nullopt;
        }

        std::string cifName;
        formatstr( cifName, "%s@%s-%s_%s",
            submitter.c_str(), sections[0].c_str(), userSuppliedName.c_str(),
            startdAddress.c_str()
        );

        return cifName;
    }

    int clusterID = -1;
    if(! jobAd.LookupInteger( ATTR_CLUSTER_ID, clusterID )) {
        return std::nullopt;
    }


    // Some startdAddress sinfuls are so long that they exceed the maximum
    // filename length.  Hash them, instead.
    unsigned int mdLength = 0;
    unsigned char messageDigest[EVP_MAX_MD_SIZE];
    if(! AWSv4Impl::doSha256( startdAddress, messageDigest, & mdLength )) {
        return std::nullopt;
    }
    std::string addressHash;
    AWSv4Impl::convertMessageDigestToLowercaseHex( messageDigest, mdLength, addressHash );

    std::string cifName;
    formatstr( cifName, "%s#%d_%s", sections[0].c_str(), clusterID, addressHash.c_str() );


    return cifName;
}
