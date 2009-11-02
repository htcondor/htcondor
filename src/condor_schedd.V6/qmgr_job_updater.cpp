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


#include "qmgr_job_updater.h"
#include "condor_qmgr.h"


QmgrJobUpdater::QmgrJobUpdater( ClassAd* job, const char* schedd_address )
{
	q_update_tid = -1;

	if( ! is_valid_sinful(schedd_address) ) {
		EXCEPT( "schedd_addr not specified with valid address (%s)",
				schedd_address );
	}
	schedd_addr = strdup( schedd_address );

		// we do *NOT* want to make our own copy of this ad
	job_ad = job;

	cluster = proc = -1;
	if( !job_ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_CLUSTER_ID);
	}

	if( !job_ad->LookupInteger(ATTR_PROC_ID, proc)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_PROC_ID);
	}

	common_job_queue_attrs = NULL;
	hold_job_queue_attrs = NULL;
	evict_job_queue_attrs = NULL;
	remove_job_queue_attrs = NULL;
	requeue_job_queue_attrs = NULL;
	terminate_job_queue_attrs = NULL;
	checkpoint_job_queue_attrs = NULL;
	m_pull_attrs = NULL;
	initJobQueueAttrLists();

		// finally, clear all the dirty bits on this jobAd, so we only
		// update the queue with things that have changed after this
		// point. 
	job_ad->ClearAllDirtyFlags();
}


QmgrJobUpdater::~QmgrJobUpdater()
{
	if( q_update_tid >= 0 ) {
		daemonCore->Cancel_Timer( q_update_tid );
		q_update_tid = -1;
	}
	if( schedd_addr ) { free(schedd_addr); }
	if( common_job_queue_attrs ) { delete common_job_queue_attrs; }
	if( hold_job_queue_attrs ) { delete hold_job_queue_attrs; }
	if( evict_job_queue_attrs ) { delete evict_job_queue_attrs; }
	if( remove_job_queue_attrs ) { delete remove_job_queue_attrs; }
	if( requeue_job_queue_attrs ) { delete requeue_job_queue_attrs; }
	if( terminate_job_queue_attrs ) { delete terminate_job_queue_attrs; }
	if( checkpoint_job_queue_attrs ) { delete checkpoint_job_queue_attrs; }
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
	delete m_pull_attrs;

	common_job_queue_attrs = new StringList();
	common_job_queue_attrs->insert( ATTR_IMAGE_SIZE );
	common_job_queue_attrs->insert( ATTR_DISK_USAGE );
	common_job_queue_attrs->insert( ATTR_JOB_REMOTE_SYS_CPU );
	common_job_queue_attrs->insert( ATTR_JOB_REMOTE_USER_CPU );
	common_job_queue_attrs->insert( ATTR_TOTAL_SUSPENSIONS );
	common_job_queue_attrs->insert( ATTR_CUMULATIVE_SUSPENSION_TIME );
	common_job_queue_attrs->insert( ATTR_LAST_SUSPENSION_TIME );
	common_job_queue_attrs->insert( ATTR_BYTES_SENT );
	common_job_queue_attrs->insert( ATTR_BYTES_RECVD );
	common_job_queue_attrs->insert( ATTR_LAST_JOB_LEASE_RENEWAL );
	common_job_queue_attrs->insert( ATTR_JOB_COMMITTED_TIME );

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

	checkpoint_job_queue_attrs = new StringList();
	checkpoint_job_queue_attrs->insert( ATTR_NUM_CKPTS );
	checkpoint_job_queue_attrs->insert( ATTR_LAST_CKPT_TIME );
	checkpoint_job_queue_attrs->insert( ATTR_CKPT_ARCH );
	checkpoint_job_queue_attrs->insert( ATTR_CKPT_OPSYS );
	checkpoint_job_queue_attrs->insert( ATTR_VM_CKPT_MAC );
	checkpoint_job_queue_attrs->insert( ATTR_VM_CKPT_IP );

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
QmgrJobUpdater::updateAttr( const char *name, const char *expr, bool updateMaster )
{
	bool result;
	MyString err_msg;

	dprintf( D_FULLDEBUG, "QmgrJobUpdater::updateAttr: %s = %s\n",
			 name, expr );

	int p = proc;

	// For parallel universe jobs, we want all the sets and gets
	// to go to proc 0, so it can be used as a blackboard by all
	// procs in the cluster.
	if (updateMaster) {
		p = 0;
	}
	if( ConnectQ(schedd_addr,SHADOW_QMGMT_TIMEOUT) ) {
		if( SetAttribute(cluster,p,name,expr) < 0 ) {
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
				 "update (%s = %s): %s\n", name, expr, err_msg.Value() );
	}
	return result;
}


bool
QmgrJobUpdater::updateAttr( const char *name, int value, bool updateMaster )
{
	MyString buf;
    buf.sprintf("%d", value);
	return updateAttr(name, buf.Value(), updateMaster);
}


bool
QmgrJobUpdater::updateJob( update_t type )
{
	ExprTree* tree = NULL;
	bool is_connected = false;
	bool had_error = false;
	const char* name;
	char *value = NULL;
	
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
	case U_PERIODIC:
			// No special attributes needed...
		break;
	default:
		EXCEPT( "QmgrJobUpdater::updateJob: Unknown update type (%d)!", type );
	}

	job_ad->ResetExpr();
	while( job_ad->NextDirtyExpr(name, tree) ) {
		if( tree->invisible ) {
			continue;
		}

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
				if( ! ConnectQ(schedd_addr, SHADOW_QMGMT_TIMEOUT) ) {
					return false;
				}
				is_connected = true;
			}
			if( ! updateExprTree(name, tree) ) {
				had_error = true;
			}
		}
	}
	m_pull_attrs->rewind();
	while ( (name = m_pull_attrs->next()) ) {
		if ( !is_connected ) {
			if ( !ConnectQ( schedd_addr, SHADOW_QMGMT_TIMEOUT, true ) ) {
				return false;
			}
			is_connected = true;
		}
		if ( GetAttributeExprNew( cluster, proc, name, &value ) < 0 ) {
			had_error = true;
		} else {
			job_ad->AssignExpr( name, value );
		}
		free( value );
	}
	if( is_connected ) {
		DisconnectQ(NULL);
	} 
	if( had_error ) {
		return false;
	}
	job_ad->ClearAllDirtyFlags();
	return true;
}


void
QmgrJobUpdater::periodicUpdateQ( void )
{
	updateJob( U_PERIODIC );	
}


bool
QmgrJobUpdater::updateExprTree( const char *name, ExprTree* tree )
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
	if( SetAttribute(cluster, proc, name, value) < 0 ) {
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
