/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "status_string.h"

#include "startd.h"
#include "boinc_mgr.h"


BOINC_BackfillVM::BOINC_BackfillVM( int vm_id )
	    : BackfillVM( vm_id )
{
	dprintf( D_FULLDEBUG, "New BOINC_BackfillVM (id %d) created\n", m_vm_id );
		// TODO
}


BOINC_BackfillVM::~BOINC_BackfillVM()
{
	dprintf( D_FULLDEBUG, "BOINC_BackfillVM (id %d) deleted\n", m_vm_id );
		// TODO
}


bool
BOINC_BackfillVM::init()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::start()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::suspend()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::resume()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::softkill()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::hardkill()
{
		// TODO
	return true;
}


void
BOINC_BackfillVM::publish( ClassAd* ad )
{
		// TODO
}


BOINC_BackfillMgr::BOINC_BackfillMgr()
	: BackfillMgr()
{
	dprintf( D_ALWAYS, "Instantiating a BOINC_BackfillMgr\n" );
	m_delete_boinc_mgr = false;
	m_boinc_starter = NULL;
	m_reaper_id = -1;
}


BOINC_BackfillMgr::~BOINC_BackfillMgr()
{
	dprintf( D_FULLDEBUG, "Destroying a BOINC_BackfillMgr\n" );
	if( m_boinc_starter && m_boinc_starter->active() ) {
			// our child is still around, hardkill "all" VMs
		hardkill( 0 );
		delete m_boinc_starter;
	}
}


static bool
param_boinc( const char* attr_name, const char* alt_name )
{
	if( ! attr_name ) {
		EXCEPT( "param_boinc() called with NULL attr_name" );
	}
	MyString param_name;
	param_name.sprintf( "BOINC_%s", attr_name );
	char* tmp = param( param_name.Value() );
	if( tmp ) {
		free( tmp );
		return true;
	}
	if( alt_name ) {
		param_name.sprintf( "BOINC_%s", alt_name );
		tmp = param( param_name.Value() );
		if( tmp ) {
			free( tmp );
			return true;
		}
	}
	dprintf( D_ALWAYS, "Trying to initialize a BOINC backfill manager, "
			 "but BOINC_%s not defined, failing\n", attr_name );
	return false;
}


bool
BOINC_BackfillMgr::init()
{
	return reconfig();
}


bool
BOINC_BackfillMgr::reconfig()
{
		// TODO be smart about if anything changes...

	if( ! param_boinc(ATTR_JOB_CMD, "Executable") ) {
		return false;
	}
	if( ! param_boinc(ATTR_JOB_IWD, "InitialDir") ) {
		return false;
	}
	if( ! param_boinc("Universe", ATTR_JOB_UNIVERSE) ) {
		return false;
	}
	
	return true;
}


bool
BOINC_BackfillMgr::destroy()
{
	if( m_boinc_starter && m_boinc_starter->active() ) {
			// our child is still around, hardkill "all" VMs
		hardkill( 0 );
		m_delete_boinc_mgr = true;
		return false;
	}

	return true;
}


bool
BOINC_BackfillMgr::addVM( BOINC_BackfillVM* boinc_vm )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::rmVM( int vm_id )
{
	if( ! m_vms[vm_id] ) {
		return false;
	}
	delete m_vms[vm_id];
	m_vms[vm_id] = NULL;
	m_num_vms--;

		// let the corresponding Resource know we're no longer running
		// a backfill client for it
	Resource* rip = resmgr->get_by_vm_id( vm_id );
	if( ! rip ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::rmVM(): "
				 "can't find resource with VM id %d\n", vm_id );
		return false;
	}
	if( m_delete_boinc_mgr ) {
			/*
			  if we're trying to delete the BackfillMgr, we should go
			  to the owner state, not go back to Backfill/Idle.
			  however, if we've already got a valid destination for
			  this resource, don't clobber that...
			*/
		if( rip->r_state->destination() == no_state ) {
			rip->r_state->set_destination( owner_state );
		}
	}

	rip->r_state->starterExited();

	return true;
}


bool
BOINC_BackfillMgr::start( int vm_id )
{
	if( m_delete_boinc_mgr || resmgr->isShuttingDown() ) {
			// we're trying to shutdown, don't spawn anything
		return false;
	}

	if( m_vms[vm_id] ) {
		dprintf( D_ALWAYS, "BackfillVM object for VM %d already exists\n",
				 vm_id );
		return true;
	}

	Resource* rip = resmgr->get_by_vm_id( vm_id );
	if( ! rip ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "can't find resource with VM id %d\n", vm_id );
		return false;
	}
	State s = rip->state();
	Activity a = rip->activity();

	if( s != backfill_state ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "Resource for VM id %d not in Backfill state (%s/%s)\n",
				 vm_id, state_to_string(s), activity_to_string(a) );
		return false;
	}
	if( a != idle_act ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "Resource for VM id %d not in Backfill/Idle (%s/%s)\n",
				 vm_id, state_to_string(s), activity_to_string(a) );
		return false;
	}

	if( m_boinc_starter && m_boinc_starter->active() ) {
			// already have a BOINC client running, allocate a new
			// BackfillVM for this vm_id, and consider this done.
		dprintf( D_FULLDEBUG, "VM %d wants to do backfill, already have "	
				 "a BOINC client running (pid %d)\n", vm_id, 
				 (int)m_boinc_starter->pid() );
	} else { 
		// no BOINC client running, we need to spawn it
		if( ! spawnClient() ) {
			dprintf( D_ALWAYS,
					 "ERROR spawning BOINC client, can't start backfill!\n" );
			return false;
		}
	}

		// PHASE 2: split up slots, remove monolithic BOINC client
	m_vms[vm_id] = new BOINC_BackfillVM( vm_id );
	m_num_vms++;

		// now that we have a BOINC client and a BOINC_BackfillVM
		// object for this VM, change to Backfill/BOINC
	dprintf( D_ALWAYS, "State change: BOINC client running for vm%d\n",
			 vm_id ); 
	return rip->change_state( busy_act );
}


bool
BOINC_BackfillMgr::spawnClient( void )
{ 
	dprintf( D_FULLDEBUG, "Entering BOINC_BackfillMgr::spawnClient()\n" );

	if( m_reaper_id < 0 ) {
		m_reaper_id = daemonCore->Register_Reaper( "BOINC reaper",
			(ReaperHandlercpp)&BOINC_BackfillMgr::reaper,
			"BOINC_BackfillMgr::reaper()", this );
		ASSERT( m_reaper_id != FALSE );
	}

	if( m_boinc_starter && m_boinc_starter->active() ) {
			// shouldn't happen, but bail out, just in case
		dprintf( D_ALWAYS, "ERROR: BOINC_BackfillMgr::spawnClient() "
				 "called with active m_boinc_starter object (pid %d)\n",
				 (int)m_boinc_starter->pid() );
		return false;
	}

	if( ! m_boinc_starter ) {
		Starter* tmp_starter;
		ClassAd fake_ad;
		MyString fake_req;
		fake_req.sprintf( "%s = %s", ATTR_REQUIREMENTS,
						  ATTR_HAS_JIC_LOCAL_CONFIG );
		fake_ad.Insert( fake_req.Value() );
		tmp_starter = resmgr->starter_mgr.findStarter( &fake_ad, NULL );
		if( ! tmp_starter ) {
			dprintf( D_ALWAYS, "ERROR: Can't find a starter with %s\n",
					 ATTR_HAS_JIC_LOCAL_CONFIG );
			return false;
		}
		m_boinc_starter = tmp_starter;
		m_boinc_starter->setIsBOINC( true );
		m_boinc_starter->setReaperID( m_reaper_id );
	}

		// now, we can actually spawn the BOINC client
	if( ! m_boinc_starter->spawn(time(NULL), NULL) ) {
		dprintf( D_ALWAYS, "ERROR spawning BOINC client\n" );
		return false;
	}
	dprintf( D_FULLDEBUG, "Spawned BOINC starter: (pid %d)\n", 
			 (int)m_boinc_starter->pid() );
	return true;
}


bool
BOINC_BackfillMgr::killClient( void )
{
	bool rval = true;
#ifdef WIN32
	EXCEPT( "Condor BOINC support does NOT work on windows" ); 
#else 
	rval = m_boinc_starter->killHard();
	if( ! rval ) {
		dprintf( D_ALWAYS, "BOINC_BackfillMgr::killClient(): "
				 "ERROR telling BOINC starter (pid %d) to hardkill\n",
				 (int)m_boinc_starter->pid() );
	} else {
		dprintf( D_FULLDEBUG, "BOINC_BackfillMgr::killClient(): "
				 "told BOINC starter (pid %d) to hardkill\n",
				 (int)m_boinc_starter->pid() );
	}

#endif /* WIN32 */

	return rval;
}


int
BOINC_BackfillMgr::reaper( int pid, int status )
{
	MyString status_str;
	statusString( status, status_str );
	dprintf( D_ALWAYS, "BOINC client (pid %d) %s\n", pid, status_str.Value() );
	if( ! m_boinc_starter ) {
		EXCEPT( "Impossible: BOINC_BackfillMgr::reaper() pid [%d] "
				"called while m_boinc_starter is NULL!", pid );
	}
	if( (int)m_boinc_starter->pid() != pid ) {
		EXCEPT( "Impossible: BOINC_BackfillMgr::reaper() pid [%d] "
				"doesn't match m_boinc_starter's pid [%d]", pid, 
				(int)m_boinc_starter->pid() );
	}
	
		// tell our starter object its starter exited
	m_boinc_starter->exited();
	delete m_boinc_starter;
	m_boinc_starter = NULL;

		// once the client is gone, delete all our compute slots
	int i, max = m_vms.getsize();
	for( i=0; i < max; i++ ) {
		rmVM( i );
	}

	if( resmgr->isShuttingDown() ) {
		startd_check_free();
	}

	if( m_delete_boinc_mgr ) {
		resmgr->backfillMgrDone();
	}

	return TRUE;
}


bool
BOINC_BackfillMgr::suspend( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::resume( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::softkill( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::hardkill( int vm_id )
{
	if( ! (m_boinc_starter && m_boinc_starter->active()) ) {
			// no BOINC client running, we're done
		return true;
	}

		// PHASE 2: handle different vm_ids differently...
	if( vm_id != 0 && m_num_vms > 1 ) {
			// we're just trying to remove a single VM object, but
			// there are other active BOINC VMs so we'll leave the
			// client running (on an SMP).  in this case, we'll just
			// remove the one object and be done immediately.
		if( ! m_vms[vm_id] ) {
			dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::hardkill(%d) "
					 "no BackfillVM object with that id\n", vm_id );
			return false;
		}
		return rmVM( vm_id );
	}

		// if we're here, we're done and we should really kill the
		// BOINC client. so, we can wait to remove the VM objects
		// until the reaper goes off...
	return killClient();
}


bool
BOINC_BackfillMgr::walk( BoincVmMember member_func )
{
	bool rval = true;
	int i, num = 0, max = m_vms.getsize();
	for( i = 0; num < m_num_vms && i < max; i++ ) {
		if( m_vms[i] ) { 
			num++;
			if( ! (((BOINC_BackfillVM*)m_vms[i])->*(member_func))() ) {
				rval = false;
			}
		}
	}
	return rval;
}
