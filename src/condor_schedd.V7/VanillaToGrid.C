/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_new_classads.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_universe.h"
#include "VanillaToGrid.h"
#define WANT_NAMESPACES
#undef open
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
	ad->Delete(ATTR_AUTO_CLUSTER_ID);
	ad->Delete(ATTR_AUTO_CLUSTER_ATTRS);



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

bool hold_copied_from_target_job(classad::ClassAd const &orig)
{
	bool hold_copied_from_target = false;
	if( orig.EvaluateAttrBool(ATTR_HOLD_COPIED_FROM_TARGET_JOB, hold_copied_from_target) )
	{
		if( hold_copied_from_target ) {
			return true;
		}
	}
	return false;
}

static bool set_job_status_simple(classad::ClassAd const &orig,classad::ClassAd &update,int new_status)
{
	int origstatus;
	bool hold_copied_from_target = hold_copied_from_target_job( orig );

	if( orig.EvaluateAttrInt(ATTR_JOB_STATUS, origstatus) ) {
		if( origstatus == HELD && !hold_copied_from_target ) {
			return false;
		}
		if(origstatus == new_status || origstatus == REMOVED) {
			return false;
		}
	}
	update.InsertAttr(ATTR_JOB_STATUS, new_status);
	update.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, (int)time(0));
	if( hold_copied_from_target ) {
		update.InsertAttr( ATTR_HOLD_COPIED_FROM_TARGET_JOB, false );
	}
	return true;
}

static void set_job_status_idle(classad::ClassAd const &orig, classad::ClassAd &update) {
	set_job_status_simple(orig,update,IDLE);
}

static void set_job_status_running(classad::ClassAd const &orig, classad::ClassAd &update) {
	set_job_status_simple(orig,update,RUNNING);
	// TODO: For new_status=RUNNING and set_job_status_simple()
	// returned true, should we be calling WriteExecuteEventToUserLog?
}

static void set_job_status_held(classad::ClassAd const &orig,classad::ClassAd &update,const char * hold_reason, int hold_code, int hold_subcode)
{
	int origstatus;
	if( orig.EvaluateAttrInt(ATTR_JOB_STATUS, origstatus) ) {
		if(origstatus == HELD ) {
			return;
		}
	}
	update.InsertAttr(ATTR_JOB_STATUS, HELD);
	update.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, (int)time(0));
	if( ! hold_reason) {
		hold_reason = "Unknown reason";
	}
	update.InsertAttr(ATTR_HOLD_REASON, hold_reason);
	update.InsertAttr(ATTR_HOLD_REASON_CODE, hold_code);
	update.InsertAttr(ATTR_HOLD_REASON_SUBCODE, hold_subcode);
	update.InsertAttr(ATTR_HOLD_COPIED_FROM_TARGET_JOB, true);

	classad::ExprTree * origexpr = update.Lookup(ATTR_RELEASE_REASON);
	if(origexpr) {
		classad::ExprTree * toinsert = origexpr->Copy(); 
		update.Insert(ATTR_LAST_RELEASE_REASON, toinsert);
	}
	update.Delete(ATTR_RELEASE_REASON);

	int numholds;
	if( ! orig.EvaluateAttrInt(ATTR_NUM_SYSTEM_HOLDS, numholds) ) {
		numholds = 0;
	}
	numholds++;
	update.InsertAttr(ATTR_NUM_SYSTEM_HOLDS, numholds);
}

bool update_job_status( classad::ClassAd const & orig, classad::ClassAd & newgrid, classad::ClassAd & update)
{
	// List courtesy of condor_gridmanager/condorjob.C CondorJob::ProcessRemoteAd
	// Added ATTR_SHADOW_BIRTHDATE so condor_q shows current run time

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
		ATTR_SHADOW_BIRTHDATE,
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

		// Check sanity of HoldCopiedFromTargetJob, because some
		// versions of the schedd do not know about this attribute,
		// so it can get into an inconsistent state.
	if( origstatus != HELD && hold_copied_from_target_job( orig ) ) {
		update.InsertAttr(ATTR_HOLD_COPIED_FROM_TARGET_JOB,false);
	}

	if(origstatus != newgridstatus) {
		switch(newgridstatus) {
		case IDLE:
			set_job_status_idle(orig,update);
			break;
		case RUNNING:
			set_job_status_running(orig,update);
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
			set_job_status_held(orig, update, reason.c_str(), reasoncode, reasonsubcode);
			}
			break;
		case REMOVED:
			// Do not pass back "removed" status to the orig job.
			break;
		default:
			update.InsertAttr(ATTR_JOB_STATUS, newgridstatus);
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
			update.Insert(attrs_to_copy[index], toinsert);
		}
	}

#if 0
	dprintf(D_FULLDEBUG, "Dirty fields updated:\n");
	for(classad::ClassAd::dirtyIterator it = update.dirtyBegin();
		it != update.dirtyEnd(); ++it) {
		classad::PrettyPrint unp;
		std::string val;
		classad::ExprTree * p = update.Lookup(*it);
		if( p ) {
			unp.Unparse(val, p);
		} else {
			val = "UNABLE TO LOCATE ATTRIBUTE";
		}
		dprintf(D_FULLDEBUG, "    %s = %s\n", it->c_str(), val.c_str());
	}
#endif

	return true;
}
