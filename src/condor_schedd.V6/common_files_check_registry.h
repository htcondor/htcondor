#ifndef   _CONDOR_COMMON_FILES_CHECK_REGISTRY_H
#define   _CONDOR_COMMON_FILES_CHECK_REGISTRY_H

//
// THE COMMON FILES CHECK: THE SCHEDD'S HALF
// =========================================
//
// The common files check makes sure that no job stages or maps common input
// files (see cxfer.cpp) unless they're the same files, in the same order,
// as every other catalog with the same name in the job's scope (its DAG, or
// if it isn't in a DAG, its cluster).  Read condor_utils/common_files_check.h
// first: it explains the rule, what "the same files" means, and how the work
// is divided between the shadows and the schedd.
//
// The schedd can't check files itself -- that requires filesystem I/O as
// the job's owner -- so it's the registry: it remembers what the shadows
// tell it, and tells each shadow what to expect.  It does so at four points,
// each a single call from schedd.cpp:
//
//   Scheduler::StartJob(), when it creates a transfer shadow (STAGING):
//     stagingStarted() remembers the prompting job's scope.
//
//   spawnJobHandlerRaw(), for any shadow which will stage or map catalogs:
//     addExpectations() gives a transfer shadow the scope's hashes, and a
//     job shadow the scope's hashes plus those of each staged copy it will
//     map.  The shadow verifies its catalogs against them before activating
//     its claim, and holds the job if they don't match.
//
//   CmdDirectAttach(), when a transfer shadow reports that it has staged
//     its catalogs: recordStagedCopies() remembers the hashes of the staged
//     copies, and records the first hashes for each catalog in the scope in
//     the scope's cluster ad.  If the copies conflict with the scope's
//     hashes, it holds the prompting job and returns false, and the caller
//     retires them unused.  (See step 3 in condor_utils/common_files_check.h
//     for what then happens to jobs waiting for them.)
//
//   transferShadowExitCode(), when a transfer shadow exits with
//     JOB_CXFER_FAILED_HOLD because it refused to stage mismatched files:
//     holdPromptingJob() puts the prompting job on hold.
//
// The state
// ---------
// The scope's hashes live in the scope's cluster ad (ATTR_COMMON_FILES_SCOPE_HASHES),
// so they survive restarts and leave with the scope.  The hashes of each
// staged copy live in its transfer shadow's shadow_rec (a
// CommonFilesStagingRecord), so they live exactly as long as the copy.
//
// Failing closed
// --------------
// Anything the registry can't establish -- no report from a transfer shadow,
// a failed write to the scope's cluster ad -- shows up as a missing hash,
// which makes the job shadow hold the job rather than map unverified files.
// The one exception is a scope with no cluster ad (e.g., the DAGMan job has
// left the queue): there's nowhere to record the scope's hashes, so jobs are
// checked only against the staged copies they'd map.
//

#include <string>
#include <map>
#include "condor_classad.h"
#include "proc.h"

struct shadow_rec;

// The common files check's state for one transfer shadow.  This is a member
// of shadow_rec, and only CommonFilesCheckRegistry uses it.
class CommonFilesStagingRecord {
	friend class CommonFilesCheckRegistry;

	private:
		// The prompting job's scope.  We record this when we start the
		// transfer shadow, because the prompting job may have left the queue
		// by the time the transfer shadow finishes staging; we must still
		// record its hashes, or no job could map its copies.
		int m_scopeID {-1};

		// The hashes the transfer shadow reported for each copy it staged,
		// by internal catalog name.
		struct StagedCopy {
			std::string name;  // the simple catalog name
			std::string ordered;
			std::string sorted;
		};
		std::map<std::string, StagedCopy> m_stagedCopies;
};

class CommonFilesCheckRegistry {
	public:
		static void stagingStarted( shadow_rec & transferShadow, const ClassAd & promptingJobAd );

		// Returns false if the staged copies must not be used.
		static bool recordStagedCopies( shadow_rec & transferShadow, const ClassAd & directAttachAd );

		static void addExpectations( ClassAd & shadowJobAd, const shadow_rec & shadow );

		static void holdPromptingJob( const PROC_ID & transferShadowID );
};

#endif /* _CONDOR_COMMON_FILES_CHECK_REGISTRY_H */
