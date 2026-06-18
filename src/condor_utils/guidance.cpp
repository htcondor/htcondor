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


std::optional< std::tuple< std::string, std::string > >
determineCIFScopeAndType( const classad::ClassAd & jobAd ) {
    // Determine the cluster ID.
    std::string s;
    int clusterID = -1;
    if(! jobAd.LookupInteger( ATTR_DAGMAN_JOB_ID, clusterID )) {
        if(! jobAd.LookupInteger( ATTR_CLUSTER_ID, clusterID )) {
            return std::nullopt;
        }
        formatstr(s, "%d", clusterID);
        return {{s, ATTR_CLUSTER_ID}};
    }
    formatstr(s, "%d", clusterID );
    return {{s, ATTR_DAGMAN_JOB_ID}};
}


#define MAX_BASE_NAME_LENGTH 64

//
// This function generates "internal" names which will ideally never become
// user-visible.  These names _more_ specific than required by the scope-
// matching rules used for matchmaking because the identity of the EP is
// implicit in the match ad, but otherwise compatible.  We use them internally
// for simplicity (so that we have a single string rather than a tuple of
// values).
//
// The schedd uses them to determine if it needs to stage a catalog to a
// particular EP; the starter uses them to create a unique/specific directory
// for the catalog in its staging area.
//

std::optional<std::string>
makeCIFName(
    const classad::ClassAd & jobAd,
    const std::string & baseName,
    const std::string & startdAddress,
    const std::string & content
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
    // The internal name therefore includes the name of the schedd.  (Which
    // is not readily available in the job ClassAd, and is therefore derived
    // from the global job ID.)
    //
    // The internal name therefore includes a cluster ID, which may be
    // the DAGMan job ID.  We call determineCIFScopeAndType() to (a) make
    // sure we make the same determination as the matchmaking code and (b) so
    // that if we start using other attributes for the scope, this function
    // doesn't have to change.
    //
    // The internal name therefore includes the base name.
    //
    // [FIXME: We've decided this shouldn't be used, because it doesn't
    //  make conceptual sense.]
    // If the cluster ID is a DAGMan job ID, then a particular name could
    // refer to different contents in the different job submit files.  To
    // distinguish between catalogs in this scenario, we must include the
    // hash of the contents.
    //


    // Determine the scope.
    auto r = determineCIFScopeAndType(jobAd);
    if(! r) { return std::nullopt; }
    auto [scope, type] = * r;


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


    // Construct the content-hash.
    if(! AWSv4Impl::doSha256( content, messageDigest, & mdLength )) {
        return std::nullopt;
    }
    std::string contentHash;
    AWSv4Impl::convertMessageDigestToLowercaseHex( messageDigest, mdLength, contentHash );


    // Construct the full internal catalog name.
    std::string fullCIFName;
    // FIXME: Check the maximum base name length at submit time.
    formatstr( fullCIFName, "%.*s@%s#%s_%s=%s",
        MAX_BASE_NAME_LENGTH, baseName.c_str(),
        scheddName.c_str(), scope.c_str(), addressHash.c_str(), contentHash.c_str()
    );
    return fullCIFName;
}
