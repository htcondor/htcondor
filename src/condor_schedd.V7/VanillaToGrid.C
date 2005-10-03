#include "condor_common.h"
#include "condor_new_classads.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_universe.h"
#include "VanillaToGrid.h"
#define WANT_NAMESPACES
#include "classad_distribution.h"


bool VanillaToGrid::vanillaToGrid(classad::ClassAd * ad, const char * gridresource)
{
	ASSERT(ad);
	ASSERT(gridresource);

	/* TODO:
		- If job fails to specify transfer_input_files but has some (relying on
		  a shared filesystem), we're doomed.  Probably
		  if(should_transfer_files is NO, we should just fail.
		  (We'll do the right thing if grid_type=condor, but not other
		  grid types.)

		- Job may be relying on Condor automatically transfer output files.  If
		  transfer_output_files is set, we're probably good (again, assuming no
		  shared filesystem).  If it's not set, we can't tell the difference
		  between no output files and "everything"
		  (We'll do the right thing if grid_type=condor, but not other
		  grid types.)

		- I'm leaving WhenToTransferFiles alone.  It should work fine.

		- Should I leave FilesystemDomain in?  May be useful, but
		simultaneously will likely do the Wrong Thing on the remote
		side
	*/

	MyString remoteattr;
	remoteattr = "Remote_";
	remoteattr += ATTR_JOB_UNIVERSE;

	// Things we don't need or want
	ad->Delete(ATTR_CLUSTER_ID); // Definate no-no
	ad->Delete(ATTR_PROC_ID);

	ad->Delete(ATTR_JOB_STATUS); // Better be Idle?

	ad->Delete(ATTR_BUFFER_BLOCK_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete(ATTR_JOB_COMMITTED_TIME);
	ad->Delete(ATTR_COMPLETION_DATE);
	ad->Delete("CondorPlatform"); // TODO: Find #define
	ad->Delete("CondorVersion");  // TODO: Find #define
	ad->Delete(ATTR_CORE_SIZE);
	ad->Delete(ATTR_CUMULATIVE_SUSPENSION_TIME);
	ad->Delete(ATTR_CURRENT_HOSTS);
	ad->Delete(ATTR_ON_EXIT_BY_SIGNAL);
	ad->Delete(ATTR_JOB_EXIT_STATUS);
	ad->Delete(ATTR_GLOBAL_JOB_ID); // Store in different ATTR here?
	ad->Delete(ATTR_LAST_SUSPENSION_TIME);
	ad->Delete(ATTR_JOB_LOCAL_SYS_CPU);
	ad->Delete(ATTR_JOB_LOCAL_USER_CPU);
	ad->Delete(ATTR_NUM_CKPTS);
	ad->Delete(ATTR_NUM_RESTARTS);
	ad->Delete(ATTR_NUM_SYSTEM_HOLDS);
	ad->Delete(ATTR_OWNER); // How does schedd filter? 
	ad->Delete(ATTR_Q_DATE);
	ad->Delete(ATTR_JOB_REMOTE_SYS_CPU);
	ad->Delete(ATTR_JOB_REMOTE_USER_CPU);
	ad->Delete(ATTR_JOB_REMOTE_WALL_CLOCK);
	ad->Delete(ATTR_SERVER_TIME);
	ad->Delete(ATTR_TOTAL_SUSPENSIONS);

	ad->Delete(ATTR_MY_TYPE); // Shoul
	ad->Delete(ATTR_TARGET_TYPE); // Should be implied.
	// Set the grid resource
	ad->InsertAttr(ATTR_GRID_RESOURCE, gridresource);

	// Remap the universe
	classad::ExprTree * tmp;
	tmp = ad->Lookup(ATTR_JOB_UNIVERSE);
	if( ! tmp ) {
		EXCEPT("VanillaToGrid: job ad lacks universe");
	}
	classad::ExprTree * olduniv = tmp->Copy();
	if( ! olduniv) {
		EXCEPT("Unable to copy old universe");
	}

	ad->InsertAttr(ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID);
	ad->Insert(remoteattr.Value(), olduniv);
		// olduniv is now controlled by ClassAd

	return true;
}
