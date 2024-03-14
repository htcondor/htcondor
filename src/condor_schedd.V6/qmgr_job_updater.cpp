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
#include "condor_io.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "internet.h"
#include "classad_merge.h"


#include "qmgr_job_updater.h"
#include "condor_qmgr.h"
#include "dc_schedd.h"
#include "condor_holdcodes.h"


QmgrJobUpdater::QmgrJobUpdater( ClassAd* job, const char* schedd_address )
	: job_ad(job), // we do *NOT* want to make our own copy of this ad
	m_schedd_obj(schedd_address),
	cluster(-1), proc(-1),
	q_update_tid(-1) 
{
	if( ! m_schedd_obj.locate() ) {
		EXCEPT("Invalid schedd address (%s)", schedd_address);
	}
	if( !job_ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
		EXCEPT("Job ad doesn't contain a %s attribute.", ATTR_CLUSTER_ID);
	}

	if( !job_ad->LookupInteger(ATTR_PROC_ID, proc)) {
		EXCEPT("Job ad doesn't contain a %s attribute.", ATTR_PROC_ID);
	}

	// It is safest to read this attribute now, before the ad is
	// potentially modified.  If it is missing, then SetEffectiveOwner
	// will just be a no-op.
	job_ad->LookupString(ATTR_OWNER, m_owner);

	initJobQueueAttrLists();

	// finally, clear all the dirty bits on this jobAd, so we only
	// update the queue with things that have changed after this
	// point. 
	job_ad->EnableDirtyTracking();
	job_ad->ClearAllDirtyFlags();
}

QmgrJobUpdater::~QmgrJobUpdater()
{
	if( q_update_tid >= 0 ) {
		daemonCore->Cancel_Timer( q_update_tid );
		q_update_tid = -1;
	}
}


void
QmgrJobUpdater::initJobQueueAttrLists( )
{
	common_job_queue_attrs = {
		ATTR_JOB_STATUS,
		ATTR_IMAGE_SIZE,
		ATTR_RESIDENT_SET_SIZE,
		ATTR_PROPORTIONAL_SET_SIZE,
		ATTR_MEMORY_USAGE,
		ATTR_DISK_USAGE,
		ATTR_SCRATCH_DIR_FILE_COUNT,
		ATTR_JOB_REMOTE_SYS_CPU,
		ATTR_JOB_REMOTE_USER_CPU,
		ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU,
		ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU,
		ATTR_TOTAL_SUSPENSIONS,
		ATTR_CUMULATIVE_SUSPENSION_TIME,
		ATTR_COMMITTED_SUSPENSION_TIME,
		ATTR_LAST_SUSPENSION_TIME,
		ATTR_BYTES_SENT,
		ATTR_BYTES_RECVD,
		ATTR_JOB_CURRENT_START_TRANSFER_OUTPUT_DATE,
		ATTR_JOB_CURRENT_FINISH_TRANSFER_OUTPUT_DATE,
		ATTR_JOB_CURRENT_START_TRANSFER_INPUT_DATE,
		ATTR_JOB_CURRENT_FINISH_TRANSFER_INPUT_DATE,

		ATTR_JOB_ACTIVATION_DURATION,
		ATTR_JOB_ACTIVATION_EXECUTION_DURATION,
		ATTR_JOB_ACTIVATION_SETUP_DURATION,
		ATTR_JOB_ACTIVATION_TEARDOWN_DURATION,

		"TransferInQueued",
		"TransferInStarted",
		"TransferInFinished",
		"TransferOutQueued",
		"TransferOutStarted",
		"TransferOutFinished",
		ATTR_TRANSFER_INPUT_STATS,
		ATTR_TRANSFER_OUTPUT_STATS,
		ATTR_NUM_JOB_STARTS,
		ATTR_JOB_CURRENT_START_EXECUTING_DATE,
		ATTR_CUMULATIVE_TRANSFER_TIME,
		ATTR_LAST_JOB_LEASE_RENEWAL,
		ATTR_JOB_COMMITTED_TIME,
		ATTR_COMMITTED_SLOT_TIME,
		ATTR_DELEGATED_PROXY_EXPIRATION,
		ATTR_BLOCK_WRITE_KBYTES,
		ATTR_BLOCK_READ_KBYTES,
		ATTR_BLOCK_WRITE_BYTES,
		ATTR_BLOCK_READ_BYTES,
		ATTR_BLOCK_WRITES,
		ATTR_BLOCK_READS,
		ATTR_NETWORK_IN,
		ATTR_NETWORK_OUT,
		ATTR_JOB_CPU_INSTRUCTIONS,
		"Recent" ATTR_BLOCK_READ_KBYTES,
		"Recent" ATTR_BLOCK_WRITE_KBYTES,
		"Recent" ATTR_BLOCK_READ_BYTES,
		"Recent" ATTR_BLOCK_WRITE_BYTES,
		"Recent" ATTR_BLOCK_READS,
		"Recent" ATTR_BLOCK_WRITES,
		"StatsLastUpdateTimeStarter",
		"StatsLifetimeStarter",
		"RecentStatsLifetimeStarter",
		"RecentWindowMaxStarter",
		"RecentStatsTickTimeStarter",
		ATTR_JOB_VM_CPU_UTILIZATION,
		ATTR_TRANSFERRING_INPUT,
		ATTR_TRANSFERRING_OUTPUT,
		ATTR_TRANSFER_QUEUED,
		ATTR_NUM_JOB_COMPLETIONS,
		ATTR_IO_WAIT,
		ATTR_JOB_CURRENT_RECONNECT_ATTEMPT,
		ATTR_TOTAL_JOB_RECONNECT_ATTEMPTS,

		// FIXME: What I'd actually like is a way to queue all attributes
		// not in any whitelist for delivery with the last update.
		//
		// (Why _do_ we filter the last job update?)
		"PreExitCode",
		"PreExitSignal",
		"PreExitBySignal",
		"PostExitCode",
		"PostExitSignal",
		"PostExitBySignal",

		ATTR_JOB_LAST_SHADOW_EXCEPTION,
		ATTR_JOB_CHECKPOINT_NUMBER
	};

	hold_job_queue_attrs = {
		ATTR_HOLD_REASON,
		ATTR_HOLD_REASON_CODE,
		ATTR_HOLD_REASON_SUBCODE
	};

	evict_job_queue_attrs = {
		ATTR_LAST_VACATE_TIME
	};

	remove_job_queue_attrs = {
		ATTR_REMOVE_REASON
	};

	requeue_job_queue_attrs = {
		ATTR_REQUEUE_REASON
	};

	terminate_job_queue_attrs = {
		ATTR_EXIT_REASON,
		ATTR_JOB_EXIT_STATUS,
		ATTR_JOB_CORE_DUMPED,
		ATTR_ON_EXIT_BY_SIGNAL,
		ATTR_ON_EXIT_SIGNAL,
		ATTR_ON_EXIT_CODE,
		ATTR_EXCEPTION_HIERARCHY,
		ATTR_EXCEPTION_TYPE,
		ATTR_EXCEPTION_NAME,
		ATTR_TERMINATION_PENDING,
		ATTR_JOB_CORE_FILENAME,
		ATTR_SPOOLED_OUTPUT_FILES
	};

	checkpoint_job_queue_attrs = {
		ATTR_NUM_CKPTS,
		ATTR_LAST_CKPT_TIME,
		ATTR_VM_CKPT_MAC,
		ATTR_VM_CKPT_IP
	};

	x509_job_queue_attrs = {
		ATTR_X509_USER_PROXY_EXPIRATION
	};

	if ( job_ad->LookupExpr( ATTR_TIMER_REMOVE_CHECK ) ) {
		m_pull_attrs = { ATTR_TIMER_REMOVE_CHECK };
	}
}


void
QmgrJobUpdater::startUpdateTimer( )
{
	if( q_update_tid >= 0 ) {
		return;
	}

	int q_interval = param_integer( "SHADOW_QUEUE_UPDATE_INTERVAL", 15*60 );

	q_update_tid = daemonCore->
		Register_Timer( q_interval, q_interval,
                        (TimerHandlercpp)&QmgrJobUpdater::periodicUpdateQ,
                        "periodicUpdateQ", this );
    if( q_update_tid < 0 ) {
        EXCEPT( "Can't register DC timer!" );
    }
	dprintf( D_FULLDEBUG, "QmgrJobUpdater: started timer to update queue "
			 "every %d seconds (tid=%d)\n", q_interval, q_update_tid );
}


void
QmgrJobUpdater::resetUpdateTimer( )
{
	if ( q_update_tid < 0 ) {
		startUpdateTimer();
	}

	int q_interval = param_integer( "SHADOW_QUEUE_UPDATE_INTERVAL", 15*60 );
	daemonCore->Reset_Timer( q_update_tid, 0, q_interval );
}


/*
  WARNING: This method is BAD NEWS for schedd scalability.  We make a
  whole new qmgmt connection for *every* attribute we update.  Worse
  yet, we use this method to service a pseudo syscall from the user
  job (pseudo_set_job_attr), so the schedd can be held hostage by
  user-jobs that call this syscall repeatedly.  :(

  I'm just moving this code as originally written by Doug Thain
  (03-Jul-03) into this new object.  I think it's a bad idea to leave
  the code like this.  We should seriously consider if we want to
  continue to allow this RSC to work in this way or how we should
  modify it to be less potentially harmful for schedd scalability.
*/
bool
QmgrJobUpdater::updateAttr( const char *name, const char *expr, bool updateMaster, bool log )
{
	bool result = false;
	std::string err_msg;
	SetAttributeFlags_t flags=0;

	dprintf( D_FULLDEBUG, "QmgrJobUpdater::updateAttr: %s = %s\n",
			 name, expr );

	int p = proc;

	// For parallel universe jobs, we want all the sets and gets
	// to go to proc 0, so it can be used as a blackboard by all
	// procs in the cluster.
	if (updateMaster) {
		p = 0;
	}

	if (log) {
		flags = SHOULDLOG;
	}
	if( ConnectQ(m_schedd_obj,SHADOW_QMGMT_TIMEOUT,false,nullptr,m_owner.c_str()) ) {
		if( SetAttribute(cluster,p,name,expr,flags) < 0 ) {
			err_msg = "SetAttribute() failed";
			result = FALSE;
		} else {
			result = TRUE;
		}
		DisconnectQ( nullptr );
	} else {
		err_msg = "ConnectQ() failed";
		result = FALSE;
	}

	if( result == FALSE ) {
		dprintf( D_ALWAYS, "QmgrJobUpdater::updateAttr: failed to "
				 "update (%s = %s): %s\n", name, expr, err_msg.c_str() );
	}
	return result;
}


bool
QmgrJobUpdater::updateAttr( const char *name, int value, bool updateMaster, bool log )
{
	std::string buf;
	formatstr(buf, "%d", value);
	return updateAttr(name, buf.c_str(), updateMaster, log);
}


bool
QmgrJobUpdater::updateJob( update_t type, SetAttributeFlags_t commit_flags )
{
	ExprTree* tree = nullptr;
	bool is_connected = false;
	bool had_error = false;
	const char* name = nullptr;
	char *value = nullptr;
	std::list< std::string > undirty_attrs;

	classad::References* job_queue_attrs = nullptr;
	switch( type ) {
	case U_HOLD:
		job_queue_attrs = &hold_job_queue_attrs;
		break;
	case U_REMOVE:
		job_queue_attrs = &remove_job_queue_attrs;
		break;
	case U_REQUEUE:
		job_queue_attrs = &requeue_job_queue_attrs;
		break;
	case U_TERMINATE:
		job_queue_attrs = &terminate_job_queue_attrs;
		break;
	case U_EVICT:
		job_queue_attrs = &evict_job_queue_attrs;
		break;
	case U_CHECKPOINT:
		job_queue_attrs = &checkpoint_job_queue_attrs;
		break;
	case U_X509:
		job_queue_attrs = &x509_job_queue_attrs;
		break;
	case U_STATUS:
		// This fixes a problem where OnExitHold evaluating to true
		// prevented ExitCode from being set; see HTCONDOR-599.
		job_queue_attrs = &terminate_job_queue_attrs;
		break;
	case U_PERIODIC:
			// No special attributes needed...
		break;
	default:
		EXCEPT( "QmgrJobUpdater::updateJob: Unknown update type (%d)!", type );
	}

	if (type == U_HOLD) {
		if (!ConnectQ(m_schedd_obj, SHADOW_QMGMT_TIMEOUT, false, nullptr, m_owner.c_str()) ) {
			return false;
		}
		is_connected = true;

		int job_status = 0;
		GetAttributeInt(cluster, proc, ATTR_JOB_STATUS, &job_status);
		if (job_status == HELD) {
			dprintf(D_FULLDEBUG, "Job already held, not updating hold reason code\n");
			job_queue_attrs = nullptr;
		}
	}

	for ( auto itr = job_ad->dirtyBegin(); itr != job_ad->dirtyEnd(); itr++ ) {
		name = itr->c_str();
		tree = job_ad->LookupExpr(name);
		if ( tree == nullptr ) {
			continue;
		}
		// There used to be a check for tree->invisible here,
		// but there are no codepaths that reach here with
		// private attributes set to invisible.

			// If we have the lists of attributes we care about and
			// this attribute is in one of the lists, actually do the
			// update into the job queue...  If either of these lists
			// aren't defined, we're careful here to not dereference a
			// NULL pointer...
		if( common_job_queue_attrs.count(name) ||
			(job_queue_attrs &&
			 job_queue_attrs->count(name)) ) {

			if( ! is_connected ) {
				if( ! ConnectQ(m_schedd_obj, SHADOW_QMGMT_TIMEOUT, false, nullptr, m_owner.c_str()) ) {
					return false;
				}
				is_connected = true;
			}
			if( ! updateExprTree(name, tree) ) {
				had_error = true;
			}
			undirty_attrs.emplace_back(name );
		}
	}
	for (auto& pull_name: m_pull_attrs) {
		if ( !is_connected ) {
			if ( !ConnectQ( m_schedd_obj, SHADOW_QMGMT_TIMEOUT, true ) ) {
				return false;
			}
			is_connected = true;
		}
		if ( GetAttributeExprNew( cluster, proc, pull_name.c_str(), &value ) < 0 ) {
			had_error = true;
		} else {
			job_ad->AssignExpr( pull_name, value );
			undirty_attrs.emplace_back(pull_name);
		}
		free( value );
	}
	if( is_connected ) {
		if( !had_error) {
			if( RemoteCommitTransaction(commit_flags)!=0 ) {
				dprintf(D_ALWAYS,"Failed to commit job update.\n");
				had_error = true;
			}
		}
		DisconnectQ(nullptr,false);
	} 
	if( had_error ) {
		return false;
	}
	for(auto & undirty_attr : undirty_attrs)
	{
		job_ad->MarkAttributeClean(undirty_attr);
	}
	return true;
}


bool
QmgrJobUpdater::retrieveJobUpdates( )
{
	ClassAd updates;
	CondorError errstack;
	StringList job_ids;
	char id_str[PROC_ID_STR_BUFLEN];

	ProcIdToStr(cluster, proc, id_str);
	job_ids.insert(id_str);

	if ( !ConnectQ( m_schedd_obj, SHADOW_QMGMT_TIMEOUT, false ) ) {
		return false;
	}
	if ( GetDirtyAttributes( cluster, proc, &updates ) < 0 ) {
		DisconnectQ(nullptr,false);
		return false;
	}
	DisconnectQ( nullptr, false );

	dprintf( D_FULLDEBUG, "Retrieved updated attributes from schedd\n" );
	dPrintAd( D_JOB, updates );
	MergeClassAds( job_ad, &updates, true );

	if ( m_schedd_obj.clearDirtyAttrs( &job_ids, &errstack ) == nullptr ) {
		dprintf( D_ALWAYS, "clearDirtyAttrs() failed: %s\n", errstack.getFullText().c_str() );
		return false;
	}
	return true;
}


void
QmgrJobUpdater::periodicUpdateQ( int /* timerID */ )
{
		// For performance, use a NONDURABLE transaction.
	updateJob( U_PERIODIC, NONDURABLE );
}

bool
QmgrJobUpdater::updateExprTree( const char *name, ExprTree* tree ) const
{
	if( ! tree ) {
		dprintf( D_ALWAYS, "QmgrJobUpdater::updateExprTree: tree is NULL!\n" );
		return false;
	}
	if( ! name ) {
		dprintf( D_ALWAYS,
				 "QmgrJobUpdater::updateExprTree: can't find name!\n" );
		return false;
	}		
	const char* value = ExprTreeToString( tree );
	if( ! value ) {
		dprintf( D_ALWAYS,
				 "QmgrJobUpdater::updateExprTree: can't find value!\n" );
		return false;
	}		
		// This code used to be smart about figuring out what type of
		// expression this is and calling SetAttributeInt(), 
		// SetAttributeString(), or whatever it needed.  However, all
		// these "special" versions of SetAttribute*() just force
		// everything back down into the flat string representation
		// and call SetAttribute(), so it was both a waste of effort
		// here, and made this code needlessly more complex.  
		// Derek Wright, 3/25/02

		// We use SetAttribute_NoAck to improve performance, since this
		// avoids a lot of round-trips between the schedd and shadow.
		// This means we may not detect failure until CommitTransaction()
		// or the next call to SetAttribute().
	if( SetAttribute(cluster, proc, name, value, SetAttribute_NoAck) < 0 ) {
		dprintf( D_ALWAYS, 
				 "updateExprTree: Failed SetAttribute(%s, %s)\n",
				 name, value );
		return false;
	}
	dprintf( D_FULLDEBUG, 
			 "Updating Job Queue: SetAttribute(%s = %s)\n",
			 name, value );
	return true;
}



bool
QmgrJobUpdater::watchAttribute( const char* attr, update_t type  )
{
		// first, figure out what attribute list to add this to, based
		// on the type we were given.
	classad::References* job_queue_attrs = nullptr;
	switch( type ) {
	case U_NONE:
			// this is the default case for the update_t
		job_queue_attrs = &common_job_queue_attrs;
		break;
	case U_HOLD:
		job_queue_attrs = &hold_job_queue_attrs;
		break;
	case U_REMOVE:
		job_queue_attrs = &remove_job_queue_attrs;
		break;
	case U_REQUEUE:
		job_queue_attrs = &requeue_job_queue_attrs;
		break;
	case U_TERMINATE:
		job_queue_attrs = &terminate_job_queue_attrs;
		break;
	case U_EVICT:
		job_queue_attrs = &evict_job_queue_attrs;
		break;
	case U_CHECKPOINT:
		job_queue_attrs = &checkpoint_job_queue_attrs;
		break;
	case U_X509:
		job_queue_attrs = &x509_job_queue_attrs;
		break;
	case U_STATUS:
		EXCEPT( "Programmer error: QmgrJobUpdater::watchAttribute() called "
				"with U_STATUS" );
		break;
	case U_PERIODIC:
		EXCEPT( "Programmer error: QmgrJobUpdater::watchAttribute() called "
				"with U_PERIODIC" );
		break;
	default:
		EXCEPT( "QmgrJobUpdater::watchAttribute: Unknown update type (%d)!", type );
		break;
	}

		// do the real work.  if the attribute we were given is
		// already in the requested list, we return false, otherwise,
		// we insert and return true.
	if( job_queue_attrs->count(attr) ) {
		return false;
	}
	job_queue_attrs->insert( attr );
	return true;
}
