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


	ad->Delete(ATTR_BUFFER_BLOCK_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete("CondorPlatform"); // TODO: Find #define
	ad->Delete("CondorVersion");  // TODO: Find #define
	ad->Delete(ATTR_CORE_SIZE);
	ad->Delete(ATTR_CURRENT_HOSTS);
	ad->Delete(ATTR_GLOBAL_JOB_ID); // Store in different ATTR here?
	//ad->Delete(ATTR_OWNER); // How does schedd filter? 
	ad->Delete(ATTR_Q_DATE);
	ad->Delete(ATTR_JOB_REMOTE_WALL_CLOCK);
	ad->Delete(ATTR_SERVER_TIME);



	// Stuff to reset
	ad->InsertAttr(ATTR_JOB_STATUS, 1); // Idle
	ad->InsertAttr(ATTR_JOB_REMOTE_USER_CPU, 0.0);
	ad->InsertAttr(ATTR_JOB_REMOTE_SYS_CPU, 0.0);
	ad->InsertAttr(ATTR_JOB_EXIT_STATUS, 0);
	ad->InsertAttr(ATTR_COMPLETION_DATE, 0);
	ad->InsertAttr(ATTR_JOB_LOCAL_SYS_CPU, 0.0);
	ad->InsertAttr(ATTR_JOB_LOCAL_USER_CPU, 0.0);
	ad->InsertAttr(ATTR_NUM_CKPTS, 0);
	ad->InsertAttr(ATTR_NUM_RESTARTS, 0);
	ad->InsertAttr(ATTR_NUM_SYSTEM_HOLDS, 0);
	ad->InsertAttr(ATTR_JOB_COMMITTED_TIME, 0);
	ad->InsertAttr(ATTR_TOTAL_SUSPENSIONS, 0);
	ad->InsertAttr(ATTR_LAST_SUSPENSION_TIME, 0);
	ad->InsertAttr(ATTR_CUMULATIVE_SUSPENSION_TIME, 0);
	ad->InsertAttr(ATTR_ON_EXIT_BY_SIGNAL, false);


	//ad->Delete(ATTR_MY_TYPE); // Should be implied
	//ad->Delete(ATTR_TARGET_TYPE); // Should be implied.
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



static void set_job_status_idle(classad::ClassAd &orig)
{
	int origstatus;
	if( orig.EvaluateAttrInt(ATTR_JOB_STATUS, origstatus) ) {
		if(origstatus == IDLE || origstatus == HELD || origstatus == REMOVED) {
			return;
		}
	}
	orig.InsertAttr(ATTR_JOB_STATE, IDLE);
	orig.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, (int)time(0));
}

static void set_job_status_running(classad::ClassAd &orig)
{
	int origstatus;
	if( orig.EvaluateAttrInt(ATTR_JOB_STATUS, origstatus) ) {
		if(origstatus == RUNNING || origstatus == HELD || origstatus == REMOVED) {
			return;
		}
	}
	orig.InsertAttr(ATTR_JOB_STATE, RUNNING);
	orig.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, (int)time(0));
	// TODO: Should we be calling WriteExecuteEventToUserLog?
}

static void set_job_status_held(classad::ClassAd &orig,
	const char * hold_reason, int hold_code, int hold_subcode)
{
	int origstatus;
	if( orig.EvaluateAttrInt(ATTR_JOB_STATUS, origstatus) ) {
		if(origstatus == HELD ) {
			return;
		}
	}
	orig.InsertAttr(ATTR_JOB_STATE, HELD);
	orig.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, (int)time(0));
	if( ! hold_reason) {
		hold_reason = "Unknown reason";
	}
	orig.InsertAttr(ATTR_HOLD_REASON, hold_reason);
	orig.InsertAttr(ATTR_HOLD_REASON_CODE, hold_code);
	orig.InsertAttr(ATTR_HOLD_REASON_SUBCODE, hold_subcode);

	classad::ExprTree * origexpr = orig.Lookup(ATTR_RELEASE_REASON);
	if(origexpr) {
		classad::ExprTree * toinsert = origexpr->Copy(); 
		orig.Insert(ATTR_LAST_RELEASE_REASON, toinsert);
	}
	orig.Delete(ATTR_RELEASE_REASON);

	int numholds;
	if( ! orig.EvaluateAttrInt(ATTR_NUM_SYSTEM_HOLDS, numholds) ) {
		numholds = 0;
	}
	numholds++;
	orig.InsertAttr(ATTR_NUM_SYSTEM_HOLDS, numholds);
}

bool update_job_status( classad::ClassAd & orig, classad::ClassAd & newgrid)
{
	// List courtesy of condor_gridmanager/condorjob.C CondorJob::ProcessRemoteAd

	const char *attrs_to_copy[] = {
		ATTR_BYTES_SENT,
		ATTR_BYTES_RECVD,
		ATTR_COMPLETION_DATE,
		ATTR_JOB_RUN_COUNT,
		ATTR_JOB_START_DATE,
		ATTR_ON_EXIT_BY_SIGNAL,
		ATTR_ON_EXIT_SIGNAL,
		ATTR_ON_EXIT_CODE,
		ATTR_EXIT_REASON,
		ATTR_JOB_CURRENT_START_DATE,
		ATTR_JOB_LOCAL_SYS_CPU,
		ATTR_JOB_LOCAL_USER_CPU,
		ATTR_JOB_REMOTE_SYS_CPU,
		ATTR_JOB_REMOTE_USER_CPU,
		ATTR_NUM_CKPTS,
		ATTR_NUM_GLOBUS_SUBMITS,
		ATTR_NUM_MATCHES,
		ATTR_NUM_RESTARTS,
		ATTR_JOB_REMOTE_WALL_CLOCK,
		ATTR_JOB_CORE_DUMPED,
		ATTR_EXECUTABLE_SIZE,
		ATTR_IMAGE_SIZE,
		NULL };		// list must end with a NULL
		// ATTR_JOB_STATUS

		
	int origstatus;
	if( ! orig.EvaluateAttrInt(ATTR_JOB_STATUS, origstatus) ) {
		dprintf(D_ALWAYS, "Unable to read attribute %s from original ad\n", ATTR_JOB_STATUS);
		return false;
	}
	int newgridstatus;
	if( ! newgrid.EvaluateAttrInt(ATTR_JOB_STATUS, newgridstatus) ) {
		dprintf(D_ALWAYS, "Unable to read attribute %s from new ad\n", ATTR_JOB_STATUS);
		return false;
	}
	if(origstatus != newgridstatus) {
		switch(newgridstatus) {
		case IDLE:
			set_job_status_idle(orig);
			break;
		case RUNNING:
			set_job_status_running(orig);
			break;
		case HELD:
			{
			int reasoncode;
			int reasonsubcode;
			if( ! newgrid.EvaluateAttrInt(ATTR_HOLD_REASON_CODE, reasoncode) ) {
				reasoncode = 0;
			}
			if( ! newgrid.EvaluateAttrInt(ATTR_HOLD_REASON_SUBCODE, reasonsubcode) ) {
				reasonsubcode = 0;
			}
			std::string reason;
			if( ! newgrid.EvaluateAttrString(ATTR_HOLD_REASON, reason) ) {
				reasonsubcode = 0;
			}
			set_job_status_held(orig, reason.c_str(), reasoncode, reasonsubcode);
			}
			break;
		default:
			orig.InsertAttr(ATTR_JOB_STATE, newgridstatus);
			break;
		}
	}

	// updateRuntimeStats?
	// See  condor_gridmanager/basejob.C BaseJob::updateRuntimeStats
		

	int index = -1;
	while ( attrs_to_copy[++index] != NULL ) {
		classad::ExprTree * origexpr = orig.Lookup(attrs_to_copy[index]);
		classad::ExprTree * newgridexpr = newgrid.Lookup(attrs_to_copy[index]);
		if( newgridexpr != NULL && (origexpr == NULL || ! (*origexpr == *newgridexpr) ) ) {
			classad::ExprTree * toinsert = newgridexpr->Copy(); 
			orig.Insert(attrs_to_copy[index], toinsert);
		}
	}

	return true;
}
