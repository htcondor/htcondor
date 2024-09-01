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
#include "condor_debug.h"
#include "proc.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_universe.h"
#include "VanillaToGrid.h"
#include "filename_tools.h"
#include "classad/classad_distribution.h"
#include "condor_config.h"

#include "submit_job.h"

bool VanillaToGrid::vanillaToGrid(classad::ClassAd * ad, int target_universe, const char * gridresource, bool is_sandboxed)
{
	ASSERT(ad);

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

	std::string remoteattr;
	remoteattr = "Remote_";
	remoteattr += ATTR_JOB_UNIVERSE;

	// Things we don't need or want
	ad->Delete(ATTR_CLUSTER_ID); // Definate no-no
	ad->Delete(ATTR_PROC_ID);


	ad->Delete(ATTR_BUFFER_BLOCK_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete("CondorPlatform"); // TODO: Find #define
	ad->Delete(ATTR_CONDOR_VERSION);  // TODO: Find #define
	ad->Delete(ATTR_CORE_SIZE);
	ad->Delete(ATTR_GLOBAL_JOB_ID); // Store in different ATTR here?
	//ad->Delete(ATTR_OWNER); // How does schedd filter?
		// User may be from non-default UID_DOMAIN; if so, we want to keep this
		// for the grid job.
		// But we can't keep it currently, since the schedd doesn't
		// immediately correct a "wrong" value when not using non-default
		// UID_DOMAIN (which isn't supported yet anyway).
	ad->Delete(ATTR_USER); // Schedd will set this with the proper UID_DOMAIN.
	ad->Delete(ATTR_Q_DATE);
	ad->Delete(ATTR_JOB_REMOTE_WALL_CLOCK);
	ad->Delete(ATTR_JOB_LAST_REMOTE_WALL_CLOCK);
	ad->Delete(ATTR_SERVER_TIME);
	ad->Delete(ATTR_AUTO_CLUSTER_ID);
	ad->Delete(ATTR_AUTO_CLUSTER_ATTRS);
	ad->Delete(ATTR_TOTAL_SUBMIT_PROCS);
	ad->Delete( ATTR_STAGE_IN_FINISH );
	ad->Delete( ATTR_STAGE_IN_START );
	ad->Delete( ATTR_ULOG_FILE );
	ad->Delete( ATTR_ULOG_USE_XML );
	ad->Delete( ATTR_DAGMAN_WORKFLOW_LOG );
	ad->Delete( ATTR_DAGMAN_WORKFLOW_MASK );

	// We aren't going to forward updates to this attribute,
	// so strip it out.
	// We do evaluate it locally in the source job ad.
	ad->Delete(ATTR_TIMER_REMOVE_CHECK);

	ad->Delete("SUBMIT_" ATTR_JOB_IWD); // the presence of this would prevent schedd from rewriting spooled iwd

	// Stuff to reset
	ad->InsertAttr(ATTR_JOB_STATUS, 1); // Idle
	ad->InsertAttr(ATTR_JOB_REMOTE_USER_CPU, 0.0);
	ad->InsertAttr(ATTR_JOB_REMOTE_SYS_CPU, 0.0);
	ad->InsertAttr(ATTR_JOB_EXIT_STATUS, 0);
	ad->InsertAttr(ATTR_COMPLETION_DATE, 0);
	ad->InsertAttr(ATTR_NUM_CKPTS, 0);
	ad->InsertAttr(ATTR_NUM_RESTARTS, 0);
	ad->InsertAttr(ATTR_NUM_SYSTEM_HOLDS, 0);
	ad->InsertAttr(ATTR_JOB_COMMITTED_TIME, 0);
	ad->InsertAttr(ATTR_COMMITTED_SLOT_TIME, 0);
	ad->InsertAttr(ATTR_CUMULATIVE_SLOT_TIME, 0);
	ad->InsertAttr(ATTR_TOTAL_SUSPENSIONS, 0);
	ad->InsertAttr(ATTR_LAST_SUSPENSION_TIME, 0);
	ad->InsertAttr(ATTR_CUMULATIVE_SUSPENSION_TIME, 0);
	ad->InsertAttr(ATTR_COMMITTED_SUSPENSION_TIME, 0);
	ad->InsertAttr(ATTR_ON_EXIT_BY_SIGNAL, false);


	//ad->Delete(ATTR_MY_TYPE); // Should be implied
	//ad->Delete(ATTR_TARGET_TYPE); // Should be implied.

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

	ad->InsertAttr(ATTR_JOB_UNIVERSE, target_universe);
	ad->Insert(remoteattr, olduniv);
		// olduniv is now controlled by ClassAd

	if( target_universe == CONDOR_UNIVERSE_GRID ) {
		ad->Delete(ATTR_CURRENT_HOSTS);

		// Set the grid resource
		if( gridresource ) {
			ad->InsertAttr(ATTR_GRID_RESOURCE, gridresource);
		}

		// Grid universe, unlike vanilla universe expects full output
		// paths for Out/Err.  In vanilla, these are basenames that
		// point into TransferOutputRemaps.  If the job is sandboxed,
		// then our remaps will apply when we fetch the output from
		// the completed job.  That's fine, so we leave the remaps
		// in place in that case.  Otherwise, we undo the remaps and
		// let the grid job write directly to the correct output
		// paths.

		std::string remaps;
		ad->EvaluateAttrString(ATTR_TRANSFER_OUTPUT_REMAPS,remaps);
		if( !is_sandboxed && remaps.size() ) {
			std::string remap_filename;
			std::string filename,filenames;

				// Don't need the remaps in the grid copy of the ad.
			ad->Delete(ATTR_TRANSFER_OUTPUT_REMAPS);

			if( ad->EvaluateAttrString(ATTR_JOB_OUTPUT,filename) ) {
				if( filename_remap_find(remaps.c_str(),filename.c_str(),remap_filename) ) {
					ad->InsertAttr(ATTR_JOB_OUTPUT,remap_filename.c_str());
				}
			}

			if( ad->EvaluateAttrString(ATTR_JOB_ERROR,filename) ) {
				if( filename_remap_find(remaps.c_str(),filename.c_str(),remap_filename) ) {
					ad->InsertAttr(ATTR_JOB_ERROR,remap_filename.c_str());
				}
			}
		}
	}

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
	update.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, time(0));
	if( hold_copied_from_target ) {
		update.InsertAttr( ATTR_HOLD_COPIED_FROM_TARGET_JOB, false );
	}
	return true;
}

static void set_job_status_idle(classad::ClassAd const &orig, classad::ClassAd &update) {
	int old_update_status = IDLE;
	update.EvaluateAttrInt( ATTR_JOB_STATUS, old_update_status );
	if ( set_job_status_simple(orig,update,IDLE) && old_update_status == RUNNING ) {
		WriteEvictEventToUserLog( orig );
	}
}

static void set_job_status_running(classad::ClassAd const &orig, classad::ClassAd &update) {
	int old_update_status = IDLE;
	update.EvaluateAttrInt( ATTR_JOB_STATUS, old_update_status );
	if ( set_job_status_simple(orig,update,RUNNING) && old_update_status == IDLE ) {
		WriteExecuteEventToUserLog( orig );
	}
}

static void set_job_status_held(classad::ClassAd const &orig,classad::ClassAd &update,const char * hold_reason, int hold_code, int hold_subcode)
{
	int origstatus;
	if( orig.EvaluateAttrInt(ATTR_JOB_STATUS, origstatus) ) {
		if(origstatus == HELD || origstatus == REMOVED || origstatus == COMPLETED) {
			return;
		}
	}
	update.InsertAttr(ATTR_JOB_STATUS, HELD);
	update.InsertAttr(ATTR_ENTERED_CURRENT_STATUS, time(0));
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

bool update_job_status( classad::ClassAd const & orig, classad::ClassAd & newgrid, classad::ClassAd & update, char* custom_attrs)
{
	// List courtesy of condor_gridmanager/condorjob.C CondorJob::ProcessRemoteAd
	// Added ATTR_SHADOW_BIRTHDATE so condor_q shows current run time

	const char *const attrs_to_copy[] = {
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
		ATTR_JOB_CURRENT_START_EXECUTING_DATE,
		ATTR_JOB_LAST_START_DATE,
		ATTR_SHADOW_BIRTHDATE,
		ATTR_JOB_REMOTE_SYS_CPU,
		ATTR_JOB_REMOTE_USER_CPU,
		ATTR_NUM_CKPTS,
		ATTR_NUM_JOB_STARTS,
		ATTR_NUM_JOB_RECONNECTS,
		ATTR_NUM_SHADOW_EXCEPTIONS,
		ATTR_NUM_SHADOW_STARTS,
		ATTR_NUM_MATCHES,
		ATTR_NUM_RESTARTS,
		ATTR_JOB_REMOTE_WALL_CLOCK,
		ATTR_JOB_LAST_REMOTE_WALL_CLOCK,
		ATTR_JOB_CORE_DUMPED,
		ATTR_IMAGE_SIZE,
		ATTR_MEMORY_USAGE,
		ATTR_RESIDENT_SET_SIZE,
		ATTR_PROPORTIONAL_SET_SIZE,
		ATTR_DISK_USAGE,
		ATTR_SCRATCH_DIR_FILE_COUNT,
		ATTR_SPOOLED_OUTPUT_FILES,
		ATTR_CPUS_USAGE,
		"CpusProvisioned",
		"DiskProvisioned",
		"MemoryProvisioned",
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
		case TRANSFERRING_OUTPUT:
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

	if (custom_attrs != nullptr) {
		for (const auto &attr: StringTokenIterator(custom_attrs)) {
			if (attr.empty()) continue;
			classad::ExprTree * origexpr = orig.Lookup(attr);
			classad::ExprTree * newgridexpr = newgrid.Lookup(attr);
			if( newgridexpr != nullptr && (origexpr == nullptr || ! (*origexpr == *newgridexpr) ) ) {
				classad::ExprTree * toinsert = newgridexpr->Copy(); 
				update.Insert(attr, toinsert);
			}
		}
	}

	std::string chirp_prefix;
	param(chirp_prefix, "CHIRP_DELAYED_UPDATE_PREFIX");
	if (chirp_prefix == "Chirp*") {
		for (const auto & [attr, expr]: newgrid) {
			if ( ! strncasecmp(attr.c_str(), "Chirp", 5) ) {
				classad::ExprTree *old_expr = orig.Lookup(attr);
				classad::ExprTree *new_expr = expr;
				if ( old_expr == nullptr || !(*old_expr == *new_expr) ) {
					update.Insert( attr, new_expr->Copy() );
				}
			}
		}
	} else if (!chirp_prefix.empty()) {
		// TODO cache the prefix_list
		std::vector<std::string> prefix_list = split(chirp_prefix);
		for (const auto & [attr, expr]: newgrid) {
			if (contains_anycase_withwildcard(prefix_list,attr)) {
				classad::ExprTree *old_expr = orig.Lookup(attr);
				classad::ExprTree *new_expr = expr;
				if ( old_expr == nullptr || !(*old_expr == *new_expr) ) {
					update.Insert( attr, new_expr->Copy() );
				}
			}
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
