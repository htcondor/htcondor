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
	: common_job_queue_attrs(0),
	hold_job_queue_attrs(0),
	evict_job_queue_attrs(0),
	remove_job_queue_attrs(0),
	requeue_job_queue_attrs(0),
	terminate_job_queue_attrs(0),
	checkpoint_job_queue_attrs(0),
	x509_job_queue_attrs(0),
	m_pull_attrs(0),
	job_ad(job), // we do *NOT* want to make our own copy of this ad
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
	if( common_job_queue_attrs ) { delete common_job_queue_attrs; }
	if( hold_job_queue_attrs ) { delete hold_job_queue_attrs; }
	if( evict_job_queue_attrs ) { delete evict_job_queue_attrs; }
	if( remove_job_queue_attrs ) { delete remove_job_queue_attrs; }
	if( requeue_job_queue_attrs ) { delete requeue_job_queue_attrs; }
	if( terminate_job_queue_attrs ) { delete terminate_job_queue_attrs; }
	if( checkpoint_job_queue_attrs ) { delete checkpoint_job_queue_attrs; }
	if( x509_job_queue_attrs ) { delete x509_job_queue_attrs; }
	delete m_pull_attrs;
}


void
QmgrJobUpdater::initJobQueueAttrLists( void )
{
	if( hold_job_queue_attrs ) { delete hold_job_queue_attrs; }
	if( evict_job_queue_attrs ) { delete evict_job_queue_attrs; }
	if( requeue_job_queue_attrs ) { delete requeue_job_queue_attrs; }
	if( remove_job_queue_attrs ) { delete remove_job_queue_attrs; }
	if( terminate_job_queue_attrs ) { delete terminate_job_queue_attrs; }
	if( common_job_queue_attrs ) { delete common_job_queue_attrs; }
	if( checkpoint_job_queue_attrs ) { delete checkpoint_job_queue_attrs; }
	if( x509_job_queue_attrs ) { delete x509_job_queue_attrs; }
	delete m_pull_attrs;

	common_job_queue_attrs = new StringList();
	common_job_queue_attrs->insert( ATTR_JOB_STATUS );
	common_job_queue_attrs->insert( ATTR_IMAGE_SIZE );
	common_job_queue_attrs->insert( ATTR_RESIDENT_SET_SIZE );
	common_job_queue_attrs->insert( ATTR_PROPORTIONAL_SET_SIZE );
	common_job_queue_attrs->insert( ATTR_MEMORY_USAGE );
	common_job_queue_attrs->insert( ATTR_DISK_USAGE );
	common_job_queue_attrs->insert( ATTR_SCRATCH_DIR_FILE_COUNT );
	common_job_queue_attrs->insert( ATTR_JOB_REMOTE_SYS_CPU );
	common_job_queue_attrs->insert( ATTR_JOB_REMOTE_USER_CPU );
	common_job_queue_attrs->insert( ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU );
	common_job_queue_attrs->insert( ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU );
	common_job_queue_attrs->insert( ATTR_TOTAL_SUSPENSIONS );
	common_job_queue_attrs->insert( ATTR_CUMULATIVE_SUSPENSION_TIME );
	common_job_queue_attrs->insert( ATTR_COMMITTED_SUSPENSION_TIME );
	common_job_queue_attrs->insert( ATTR_LAST_SUSPENSION_TIME );
	common_job_queue_attrs->insert( ATTR_BYTES_SENT );
	common_job_queue_attrs->insert( ATTR_BYTES_RECVD );
	common_job_queue_attrs->insert( ATTR_JOB_CURRENT_START_TRANSFER_OUTPUT_DATE );
	common_job_queue_attrs->insert( ATTR_JOB_CURRENT_FINISH_TRANSFER_OUTPUT_DATE );
	common_job_queue_attrs->insert( ATTR_JOB_CURRENT_START_TRANSFER_INPUT_DATE );
	common_job_queue_attrs->insert( ATTR_JOB_CURRENT_FINISH_TRANSFER_INPUT_DATE );

	common_job_queue_attrs->insert( ATTR_JOB_ACTIVATION_DURATION );
	common_job_queue_attrs->insert( ATTR_JOB_ACTIVATION_EXECUTION_DURATION );
	common_job_queue_attrs->insert( ATTR_JOB_ACTIVATION_SETUP_DURATION );
	common_job_queue_attrs->insert( ATTR_JOB_ACTIVATION_TEARDOWN_DURATION );

	common_job_queue_attrs->insert( "TransferInQueued" );
	common_job_queue_attrs->insert( "TransferInStarted" );
	common_job_queue_attrs->insert( "TransferInFinished" );
	common_job_queue_attrs->insert( "TransferOutQueued" );
	common_job_queue_attrs->insert( "TransferOutStarted" );
	common_job_queue_attrs->insert( "TransferOutFinished" );
	common_job_queue_attrs->insert( ATTR_TRANSFER_INPUT_STATS );
	common_job_queue_attrs->insert( ATTR_TRANSFER_OUTPUT_STATS );
	common_job_queue_attrs->insert( ATTR_NUM_JOB_STARTS );
	common_job_queue_attrs->insert( ATTR_JOB_CURRENT_START_EXECUTING_DATE );
	common_job_queue_attrs->insert( ATTR_CUMULATIVE_TRANSFER_TIME );
	common_job_queue_attrs->insert( ATTR_LAST_JOB_LEASE_RENEWAL );
	common_job_queue_attrs->insert( ATTR_JOB_COMMITTED_TIME );
	common_job_queue_attrs->insert( ATTR_COMMITTED_SLOT_TIME );
	common_job_queue_attrs->insert( ATTR_DELEGATED_PROXY_EXPIRATION );
	common_job_queue_attrs->insert( ATTR_BLOCK_WRITE_KBYTES );
	common_job_queue_attrs->insert( ATTR_BLOCK_READ_KBYTES );
	common_job_queue_attrs->insert( ATTR_BLOCK_WRITE_BYTES );
	common_job_queue_attrs->insert( ATTR_BLOCK_READ_BYTES );
	common_job_queue_attrs->insert( ATTR_BLOCK_WRITES );
	common_job_queue_attrs->insert( ATTR_BLOCK_READS );
	common_job_queue_attrs->insert( ATTR_NETWORK_IN );
	common_job_queue_attrs->insert( ATTR_NETWORK_OUT );
	common_job_queue_attrs->insert( ATTR_JOB_CPU_INSTRUCTIONS );
    common_job_queue_attrs->insert( "Recent" ATTR_BLOCK_READ_KBYTES );
    common_job_queue_attrs->insert( "Recent" ATTR_BLOCK_WRITE_KBYTES );
    common_job_queue_attrs->insert( "Recent" ATTR_BLOCK_READ_BYTES );
    common_job_queue_attrs->insert( "Recent" ATTR_BLOCK_WRITE_BYTES );
    common_job_queue_attrs->insert( "Recent" ATTR_BLOCK_READS );
    common_job_queue_attrs->insert( "Recent" ATTR_BLOCK_WRITES );
    common_job_queue_attrs->insert( "StatsLastUpdateTimeStarter" );
    common_job_queue_attrs->insert( "StatsLifetimeStarter" );
    common_job_queue_attrs->insert( "RecentStatsLifetimeStarter" );
    common_job_queue_attrs->insert( "RecentWindowMaxStarter" );
    common_job_queue_attrs->insert( "RecentStatsTickTimeStarter" );
	common_job_queue_attrs->insert( ATTR_JOB_VM_CPU_UTILIZATION );
	common_job_queue_attrs->insert( ATTR_TRANSFERRING_INPUT );
	common_job_queue_attrs->insert( ATTR_TRANSFERRING_OUTPUT );
	common_job_queue_attrs->insert( ATTR_TRANSFER_QUEUED );
	common_job_queue_attrs->insert( ATTR_JOB_TRANSFERRING_OUTPUT );
	common_job_queue_attrs->insert( ATTR_JOB_TRANSFERRING_OUTPUT_TIME );
	common_job_queue_attrs->insert( ATTR_NUM_JOB_COMPLETIONS );
	common_job_queue_attrs->insert( ATTR_IO_WAIT);

	// FIXME: What I'd actually like is a way to queue all attributes
	// not in any whitelist for delivery with the last update.
	common_job_queue_attrs->insert( "PreExitCode" );
	common_job_queue_attrs->insert( "PreExitSignal" );
	common_job_queue_attrs->insert( "PreExitBySignal" );
	common_job_queue_attrs->insert( "PostExitCode" );
	common_job_queue_attrs->insert( "PostExitSignal" );
	common_job_queue_attrs->insert( "PostExitBySignal" );

	hold_job_queue_attrs = new StringList();
	hold_job_queue_attrs->insert( ATTR_HOLD_REASON );
	hold_job_queue_attrs->insert( ATTR_HOLD_REASON_CODE );
	hold_job_queue_attrs->insert( ATTR_HOLD_REASON_SUBCODE );

	evict_job_queue_attrs = new StringList();
	evict_job_queue_attrs->insert( ATTR_LAST_VACATE_TIME );

	remove_job_queue_attrs = new StringList();
	remove_job_queue_attrs->insert( ATTR_REMOVE_REASON );

	requeue_job_queue_attrs = new StringList();
	requeue_job_queue_attrs->insert( ATTR_REQUEUE_REASON );

	terminate_job_queue_attrs = new StringList();
	terminate_job_queue_attrs->insert( ATTR_EXIT_REASON );
	terminate_job_queue_attrs->insert( ATTR_JOB_EXIT_STATUS );
	terminate_job_queue_attrs->insert( ATTR_JOB_CORE_DUMPED );
	terminate_job_queue_attrs->insert( ATTR_ON_EXIT_BY_SIGNAL );
	terminate_job_queue_attrs->insert( ATTR_ON_EXIT_SIGNAL );
	terminate_job_queue_attrs->insert( ATTR_ON_EXIT_CODE );
	terminate_job_queue_attrs->insert( ATTR_EXCEPTION_HIERARCHY );
	terminate_job_queue_attrs->insert( ATTR_EXCEPTION_TYPE );
	terminate_job_queue_attrs->insert( ATTR_EXCEPTION_NAME );
	terminate_job_queue_attrs->insert( ATTR_TERMINATION_PENDING );
	terminate_job_queue_attrs->insert( ATTR_JOB_CORE_FILENAME );
	terminate_job_queue_attrs->insert( ATTR_SPOOLED_OUTPUT_FILES );

	checkpoint_job_queue_attrs = new StringList();
	checkpoint_job_queue_attrs->insert( ATTR_NUM_CKPTS );
	checkpoint_job_queue_attrs->insert( ATTR_LAST_CKPT_TIME );
	checkpoint_job_queue_attrs->insert( ATTR_VM_CKPT_MAC );
	checkpoint_job_queue_attrs->insert( ATTR_VM_CKPT_IP );

	x509_job_queue_attrs = new StringList();
	x509_job_queue_attrs->insert( ATTR_X509_USER_PROXY_EXPIRATION );
	/* These are secure attributes, only settable by the schedd.
	 * Assume they won't change during job execution.
	x509_job_queue_attrs->insert( ATTR_X509_USER_PROXY_SUBJECT );
	x509_job_queue_attrs->insert( ATTR_X509_USER_PROXY_VONAME );
	x509_job_queue_attrs->insert( ATTR_X509_USER_PROXY_FIRST_FQAN );
	x509_job_queue_attrs->insert( ATTR_X509_USER_PROXY_FQAN );
	*/

	m_pull_attrs = new StringList();
	if ( job_ad->LookupExpr( ATTR_TIMER_REMOVE_CHECK ) ) {
		m_pull_attrs->insert( ATTR_TIMER_REMOVE_CHECK );
	}
}


void
QmgrJobUpdater::startUpdateTimer( void )
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
QmgrJobUpdater::resetUpdateTimer( void )
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
	bool result;
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
	if( ConnectQ(m_schedd_obj,SHADOW_QMGMT_TIMEOUT,false,NULL,m_owner.c_str()) ) {
		if( SetAttribute(cluster,p,name,expr,flags) < 0 ) {
			err_msg = "SetAttribute() failed";
			result = FALSE;
		} else {
			result = TRUE;
		}
		DisconnectQ( NULL );
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
	ExprTree* tree = NULL;
	bool is_connected = false;
	bool had_error = false;
	const char* name;
	char *value = NULL;
	std::list< std::string > undirty_attrs;
	
	StringList* job_queue_attrs = NULL;
	switch( type ) {
	case U_HOLD:
		job_queue_attrs = hold_job_queue_attrs;
		break;
	case U_REMOVE:
		job_queue_attrs = remove_job_queue_attrs;
		break;
	case U_REQUEUE:
		job_queue_attrs = requeue_job_queue_attrs;
		break;
	case U_TERMINATE:
		job_queue_attrs = terminate_job_queue_attrs;
		break;
	case U_EVICT:
		job_queue_attrs = evict_job_queue_attrs;
		break;
	case U_CHECKPOINT:
		job_queue_attrs = checkpoint_job_queue_attrs;
		break;
	case U_X509:
		job_queue_attrs = x509_job_queue_attrs;
		break;
	case U_STATUS:
		// This fixes a problem where OnExitHold evaluating to true
		// prevented ExitCode from being set; see HTCONDOR-599.
		job_queue_attrs = terminate_job_queue_attrs;
		break;
	case U_PERIODIC:
			// No special attributes needed...
		break;
	default:
		EXCEPT( "QmgrJobUpdater::updateJob: Unknown update type (%d)!", type );
	}

	if (type == U_HOLD) {
		if (!ConnectQ(m_schedd_obj, SHADOW_QMGMT_TIMEOUT, false, NULL, m_owner.c_str()) ) {
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
		if ( tree == NULL ) {
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
		if( (common_job_queue_attrs &&
			 common_job_queue_attrs->contains_anycase(name)) || 
			(job_queue_attrs &&
			 job_queue_attrs->contains_anycase(name)) ) {

			if( ! is_connected ) {
				if( ! ConnectQ(m_schedd_obj, SHADOW_QMGMT_TIMEOUT, false, NULL, m_owner.c_str()) ) {
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
	m_pull_attrs->rewind();
	while ( (name = m_pull_attrs->next()) ) {
		if ( !is_connected ) {
			if ( !ConnectQ( m_schedd_obj, SHADOW_QMGMT_TIMEOUT, true ) ) {
				return false;
			}
			is_connected = true;
		}
		if ( GetAttributeExprNew( cluster, proc, name, &value ) < 0 ) {
			had_error = true;
		} else {
			job_ad->AssignExpr( name, value );
			undirty_attrs.emplace_back(name );
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
		DisconnectQ(NULL,false);
	} 
	if( had_error ) {
		return false;
	}
	for(std::list< std::string >::iterator itr = undirty_attrs.begin();
		itr != undirty_attrs.end();
		++itr)
	{
		job_ad->MarkAttributeClean(*itr);
	}
	return true;
}


bool
QmgrJobUpdater::retrieveJobUpdates( void )
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
		DisconnectQ(NULL,false);
		return false;
	}
	DisconnectQ( NULL, false );

	dprintf( D_FULLDEBUG, "Retrieved updated attributes from schedd\n" );
	dPrintAd( D_JOB, updates );
	MergeClassAds( job_ad, &updates, true );

	if ( m_schedd_obj.clearDirtyAttrs( &job_ids, &errstack ) == NULL ) {
		dprintf( D_ALWAYS, "clearDirtyAttrs() failed: %s\n", errstack.getFullText().c_str() );
		return false;
	}
	return true;
}


void
QmgrJobUpdater::periodicUpdateQ( void )
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
	StringList* job_queue_attrs = NULL;
	switch( type ) {
	case U_NONE:
			// this is the default case for the update_t
		job_queue_attrs = common_job_queue_attrs;
		break;
	case U_HOLD:
		job_queue_attrs = hold_job_queue_attrs;
		break;
	case U_REMOVE:
		job_queue_attrs = remove_job_queue_attrs;
		break;
	case U_REQUEUE:
		job_queue_attrs = requeue_job_queue_attrs;
		break;
	case U_TERMINATE:
		job_queue_attrs = terminate_job_queue_attrs;
		break;
	case U_EVICT:
		job_queue_attrs = evict_job_queue_attrs;
		break;
	case U_CHECKPOINT:
		job_queue_attrs = checkpoint_job_queue_attrs;
		break;
	case U_X509:
		job_queue_attrs = x509_job_queue_attrs;
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
	if( job_queue_attrs->contains_anycase(attr) ) { 
		return false;
	}
	job_queue_attrs->insert( attr );
	return true;
}
