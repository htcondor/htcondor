#ifndef   _CONDOR_COMMON_FILES_CHECK_H
#define   _CONDOR_COMMON_FILES_CHECK_H

//
// THE COMMON FILES CHECK
// ======================
//
// This is the final check ("circuit breaker") that a job using common file
// transfer gets exactly the common input files it asked for.  Its code is
// split between the shadow and the schedd; this comment is the map.  The
// schedd's half is described from its side in
// condor_schedd.V6/common_files_check_registry.h.
//
// The rule
// --------
// Jobs declare common input files with `transfer_common_input` (the
// CommonInputFiles attribute) or as named catalogs (CommonInputCatalogs and
// the corresponding _x_catalog_<name> attributes; common container images
// are catalogs, too).  Every catalog with a given name is presumed to list
// the same files, in the same order, as every other catalog with that name
// in the same scope: the DAG (DAGManJobId), or, for jobs not in a DAG, the
// cluster.  That presumption is what lets the schedd stage a catalog to an
// EP once and then let every job in the scope that lands there map it.
//
// condor_submit, DAGMan, and condor_dag_checker try to enforce the rule,
// but they can only compare the lists as written.  The same relative path
// from different initialdirs, or two paths through symlinks, can't be
// checked without looking at the filesystem.  So, before any job stages or
// maps common files, a shadow checks them against the files first staged
// for that catalog in its scope, and if they differ (or can't be checked),
// it puts the job on hold with HoldReasonCode 50 [CommonFilesMismatch].
// See COMMON_FILES_MISMATCH_SUB_CODE in condor_holdcodes.h.
//
// Jobs which fall back to ordinary ("uncommon") file transfer -- because
// of FORBID_COMMON_FILE_TRANSFER, a static slot, an old EP, and so on --
// are not checked: each transfers its own list, so it gets the files it
// asked for, whether or not they're really common.
//
// What "the same files" means
// ---------------------------
// hashCommonFiles() turns each catalog into what transferring it would put
// in the sandbox.  Each relative path is resolved against the job's IWD (as
// submitted, before spooling rewrote it), then std::filesystem::canonical()
// resolves symlinks and "..".  Each entry also records where it lands in the
// sandbox, which is how staging lays it out: under the last component of its
// path as listed (not as resolved), or, with a trailing slash ("the
// directory's contents" rather than "the directory"), in the top level.
// URLs are compared exactly.  A path which can't be resolved (e.g., the
// file was deleted) is hashed as unresolvable, so it won't match.
//
// We keep two hashes: of the entries in order, which must match, and of the
// entries sorted, which is only used to tell the user that they listed the
// right files in the wrong order.
//
// Canonicalizing requires filesystem I/O as the job's owner, so it happens
// only in shadows as we don't want the schedd to block doing I/O in home
// directories that may be on shared filesystems; the schedd just stores 
// and compares hashes, and passes them to shadows.
//
// Who does what, in order
// -----------------------
//  1. The schedd starts a transfer shadow to stage a job's catalogs to an EP
//     (CXFER_STATE::STAGING).  It gives the transfer shadow the scope's
//     recorded hashes, if any, in ATTR_COMMON_FILES_HASH_CHECK.
//
//  2. The transfer shadow, before activating its claim, verifies its
//     catalogs against those hashes (CommonFilesVerifier).  On a mismatch, it
//     holds the prompting job (the job it's staging for) rather than staging
//     anything.  Otherwise, after staging, it reports each staged copy's hashes
//     to the schedd in its DIRECT_ATTACH command ad
//     (ATTR_STAGED_CATALOG_HASHES).
//
//  3. The schedd, on DIRECT_ATTACH, remembers the staged copies' hashes in
//     the transfer shadow's record, and records the first hashes for each
//     catalog in the scope in the scope's cluster ad
//     (ATTR_COMMON_FILES_SCOPE_HASHES).
//     If the copies conflict with the scope's hashes, it retires them unused.
//
//  4. The schedd starts the shadow for a job which will map catalogs
//     (CXFER_STATE::MAPPING).  It gives the shadow the hashes of the scope and
//     of the exact staged copies it will map.
//
//  5. The job's shadow, before activating its claim, verifies its catalogs
//     against both.  On a mismatch, or if any hash it needs is missing, it
//     holds the job.
//
// Where the code is
// -----------------
//  - Here (condor_utils): hashing, the wire format between the shadows and
//    the schedd, and verification.
//  - condor_schedd.V6/common_files_check_registry.{h,cpp}: steps 1, 3, and 4,
//    and holding the prompting job when a transfer shadow refuses to stage.
//    Called from Scheduler::StartJob(), CmdDirectAttach(),
//    spawnJobHandlerRaw(), and transferShadowExitCode() in schedd.cpp.
//  - condor_shadow.V6.1/shadow.cpp: UniShadow::checkCommonFilesHashes(),
//    called from UniShadow::init(), and holdPromptingJobAndExit().
//  - condor_shadow.V6.1/pseudo_ops.cpp: the report in step 2, in
//    UniShadow::start_staging_only_conversation().
//
// Why check against the staged copy as well as the scope?
// -------------------------------------------------------
// The schedd names a staged copy by its catalog name, scope, EP, and a hash
// of the catalog's list *as written*, not as resolved.  Suppose a cluster
// has transfer_common_input = common.txt, and procs 0 and 2 have
// initialdir = dirA while proc 1 has initialdir = dirB.  Proc 0 stages
// dirA/common.txt to EP1 and sets the scope's hashes.  If proc 1's transfer
// shadow had staged dirB/common.txt to EP2 before learning of proc 0's
// hashes (two transfer shadows can stage concurrently), proc 2 could land on
// EP2 and find a copy whose name matches its own but whose contents are
// dirB's.  Proc 2's own hashes match the scope's, so only comparing against
// the staged copy catches this.  (The schedd also retires such a copy as
// soon as it hears about it, in step 3, so this should be rare.)
//
// Why is the scope's state in its cluster ad?
// -------------------------------------------
// Like "CommonTransferFailed", the scope's hashes are an attribute of the
// cluster ad of the scope (the DAGMan job's, for a DAG), so they survive a
// schedd restart and leave the queue with the scope; there's nothing to
// garbage-collect.  If the scope has no cluster ad (e.g., the DAGMan job has
// already left the queue), jobs are checked only against the staged copies
// they map: they can't get the wrong files, but the rule isn't enforced
// across the scope.
//

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <utility>
#include "compat_classad.h"
#include "catalog_utils.h"

// The hashes of one catalog's files, as the files would be transferred;
// see "What 'the same files' means", above.
struct CommonFilesHashes {
	std::string ordered;      // hash of the entries in the order listed
	std::string sorted;       // hash of the entries, sorted
	std::string description;  // the canonical entries, for hold reasons
};

// Does filesystem I/O; call only as the job's owner.
std::optional<CommonFilesHashes>
hashCommonFiles( const std::string & commonFiles, const std::string & iwd );

// The job's catalogs as (simple name, contents) pairs, including the one
// implied by ATTR_COMMON_INPUT_FILES.  Empty if the job has no common files
// or its scope can't be determined.
ListOfCatalogs
getCommonInputCatalogsBySimpleName( const ClassAd & jobAd );


//
// The wire format.  Every ClassAd these functions read or write is private
// to this file, so the shadows and the schedd can't disagree about it.
//

// The hashes of a catalog staged by the transfer shadow for job `jobId`.
struct CommonFilesFingerprint {
	std::string ordered;
	std::string sorted;
	std::string jobId;
};

// Step 2: the transfer shadow hashes the catalogs it staged and adds the
// report to the DIRECT_ATTACH command ad.  Does filesystem I/O; call only as
// the job's owner.
void
addStagedCatalogsReport(
	ClassAd & cmdAd, const ClassAd & jobAd,
	const ListOfCatalogs & stagedCatalogs,
	const std::map<std::string, std::string> & internalToSimpleNameMap
);

// Step 3: the schedd reads the report.  Returns nothing if there's no
// report at all (e.g., the transfer shadow failed to hash its catalogs).
struct StagedCatalogReport {
	std::string internalName;  // identifies the staged copy
	std::string name;          // the simple catalog name
	std::string ordered;
	std::string sorted;
};

std::optional<std::vector<StagedCatalogReport>>
parseStagedCatalogsReport( const ClassAd & cmdAd );

// Step 3: the scope's hashes, by simple catalog name, as stored in the
// scope's cluster ad.
using CommonFilesScopeRecord = std::map<std::string, CommonFilesFingerprint>;

CommonFilesScopeRecord
parseScopeRecord( const ClassAd & scopeClusterAd );

// Returns the ClassAd expression to store as ATTR_COMMON_FILES_SCOPE_HASHES.
std::string
unparseScopeRecord( const CommonFilesScopeRecord & record );

// Steps 1 and 4: what the schedd tells a shadow to expect, by simple
// catalog name.  A transfer shadow gets only `scope`; a job shadow also
// gets `staged`.
struct CommonFilesExpectation {
	std::optional<CommonFilesFingerprint> scope;
	std::optional<CommonFilesFingerprint> staged;
};

void
insertExpectations(
	ClassAd & shadowJobAd,
	const std::map<std::string, CommonFilesExpectation> & expectations
);


//
// Steps 2 and 5: a shadow verifies its job's catalogs against what the
// schedd told it to expect.  Does filesystem I/O; call only as the job's
// owner.  The caller puts the job on hold if verify() returns a failure.
//
class CommonFilesVerifier {
	public:
		enum class Role {
			TransferShadow,  // about to stage the catalogs
			JobShadow,       // about to map catalogs someone else staged
		};

		struct Failure {
			std::string reason;  // the hold reason
			int subcode;         // a COMMON_FILES_MISMATCH_SUB_CODE
		};

		CommonFilesVerifier( const ClassAd & jobAd, Role role ) :
			m_jobAd(jobAd), m_role(role) {}

		std::optional<Failure> verify() const;

	private:
		const ClassAd & m_jobAd;
		Role m_role;
};

#endif /* _CONDOR_COMMON_FILES_CHECK_H */
