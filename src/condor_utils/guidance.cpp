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
    const classad::ClassAd & jobAd,
    const std::string & baseName,
    const std::string & startdAddress
) {
    std::string cifName;

    //
    // The internal name of a catalog must be distinct across all startds,
    // so that it doesn't collide on the AP.  It must be distinct across all
    // APs, so that it doesn't collide on the EP.  It must be distinct for
    // each group of jobs deliberately sharing a set of catalogs.  The name
    // must distinguish between the different catalogs within that group.
    //
    // The internal name therefore ends with a hash of the startd address.
    //
    // The internal name therefore includes the name of the schedd.
    //
    // The internal name therefore includes a cluster ID, which may be
    // the DAGMan job ID.
    //
    // The internal name therefore includes the base name.
    //


    // Determine the cluster ID.
    int clusterID = -1;
    if(! jobAd.LookupInteger( ATTR_DAGMAN_JOB_ID, clusterID )) {
        if(! jobAd.LookupInteger( ATTR_CLUSTER_ID, clusterID )) {
            return std::nullopt;
        }
    }


    // Extract the schedd's name.
    std::string globalJobID;
    if(! jobAd.LookupString( ATTR_GLOBAL_JOB_ID, globalJobID )) {
        return std::nullopt;
    }
    auto sections = split( globalJobID, "#" );
    std::string scheddName = sections[0];


    // Construct the startd address hash.  We hash these addresses to
    // avoid accidentally creating filenames that are too long.
    unsigned int mdLength = 0;
    unsigned char messageDigest[EVP_MAX_MD_SIZE];
    if(! AWSv4Impl::doSha256( startdAddress, messageDigest, & mdLength )) {
        return std::nullopt;
    }
    std::string addressHash;
    AWSv4Impl::convertMessageDigestToLowercaseHex( messageDigest, mdLength, addressHash );


    // Construct the full internal catalog name.
    std::string fullCIFName;
    // FIXME: Check the maximum base name length at submit time.
    formatstr( fullCIFName, "%.64s@%s#%d_%s",
        baseName.c_str(), scheddName.c_str(), clusterID, addressHash.c_str()
    );
    return fullCIFName;
}
