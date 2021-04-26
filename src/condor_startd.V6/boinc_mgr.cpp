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
#include "condor_config.h"
#include "condor_debug.h"
#include "status_string.h"

#include "startd.h"
#include "boinc_mgr.h"


BOINC_BackfillSlot::BOINC_BackfillSlot( int slot_id )
	    : BackfillSlot( slot_id )
{
	dprintf( D_FULLDEBUG, "New BOINC_BackfillSlot (id %d) created\n", m_slot_id );
		// TODO
}


BOINC_BackfillSlot::~BOINC_BackfillSlot()
{
	dprintf( D_FULLDEBUG, "BOINC_BackfillSlot (id %d) deleted\n", m_slot_id );
		// TODO
}


bool
BOINC_BackfillSlot::init()
{
		// TODO
	return true;
}


bool
BOINC_BackfillSlot::start()
{
		// TODO
	return true;
}


bool
BOINC_BackfillSlot::suspend()
{
		// TODO
	return true;
}


bool
BOINC_BackfillSlot::resume()
{
		// TODO
	return true;
}


bool
BOINC_BackfillSlot::softkill()
{
		// TODO
	return true;
}


bool
BOINC_BackfillSlot::hardkill()
{
		// TODO
	return true;
}


void
BOINC_BackfillSlot::publish( ClassAd* /*ad*/ )
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
			// our child is still around, hardkill "all" slots
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
	std::string param_name;
	formatstr( param_name, "BOINC_%s", attr_name );
	char* tmp = param( param_name.c_str() );
	if( tmp ) {
		free( tmp );
		return true;
	}
	if( alt_name ) {
		formatstr( param_name, "BOINC_%s", alt_name );
		tmp = param( param_name.c_str() );
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
			// our child is still around, hardkill "all" slots
		hardkill( 0 );
		m_delete_boinc_mgr = true;
		return false;
	}

	return true;
}


bool
BOINC_BackfillMgr::addSlot( BOINC_BackfillSlot* /*boinc_slot*/ )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::rmSlot( int slot_id )
{
	if( ! m_slots[slot_id] ) {
		return false;
	}
	delete m_slots[slot_id];
	m_slots[slot_id] = NULL;
	m_num_slots--;

		// let the corresponding Resource know we're no longer running
		// a backfill client for it
	Resource* rip = resmgr->get_by_slot_id( slot_id );
	if( ! rip ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::rmSlot(): "
				 "can't find resource with slot id %d\n", slot_id );
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
BOINC_BackfillMgr::start( int slot_id )
{
	if( m_delete_boinc_mgr || resmgr->isShuttingDown() ) {
			// we're trying to shutdown, don't spawn anything
		return false;
	}

	if( m_slots[slot_id] ) {
		dprintf( D_ALWAYS, "BackfillSlot object for slot %d already exists\n",
				 slot_id );
		return true;
	}

	Resource* rip = resmgr->get_by_slot_id( slot_id );
	if( ! rip ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "can't find resource with slot id %d\n", slot_id );
		return false;
	}
	State s = rip->state();
	Activity a = rip->activity();

	if( s != backfill_state ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "Resource for slot id %d not in Backfill state (%s/%s)\n",
				 slot_id, state_to_string(s), activity_to_string(a) );
		return false;
	}
	if( a != idle_act ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "Resource for slot id %d not in Backfill/Idle (%s/%s)\n",
				 slot_id, state_to_string(s), activity_to_string(a) );
		return false;
	}

	if( m_boinc_starter && m_boinc_starter->active() ) {
			// already have a BOINC client running, allocate a new
			// BackfillSlot for this slot_id, and consider this done.
		dprintf( D_FULLDEBUG, "Slot %d wants to do backfill, already have "	
				 "a BOINC client running (pid %d)\n", slot_id, 
				 (int)m_boinc_starter->pid() );
	} else { 
		// no BOINC client running, we need to spawn it
		if( ! spawnClient(rip) ) {
			dprintf( D_ALWAYS,
					 "ERROR spawning BOINC client, can't start backfill!\n" );
			return false;
		}
	}

		// PHASE 2: split up slots, remove monolithic BOINC client
	m_slots[slot_id] = new BOINC_BackfillSlot( slot_id );
	m_num_slots++;

		// now that we have a BOINC client and a BOINC_BackfillSlot
		// object for this slot, change to Backfill/BOINC
	dprintf( D_ALWAYS, "State change: BOINC client running for slot%d\n",
			 slot_id ); 
	rip->change_state( busy_act );
	return TRUE; // XXX: change TRUE
}


bool
BOINC_BackfillMgr::spawnClient( Resource *rip )
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
		std::string fake_req;
		bool no_starter = false;
		formatstr( fake_req, "TARGET.%s", ATTR_HAS_JIC_LOCAL_CONFIG );
		fake_ad.AssignExpr( ATTR_REQUIREMENTS, fake_req.c_str() );
		tmp_starter = resmgr->starter_mgr.newStarter( &fake_ad, NULL, no_starter );
		if( ! tmp_starter ) {
			dprintf( D_ALWAYS, "ERROR: Can't find a starter with %s\n",
					 ATTR_HAS_JIC_LOCAL_CONFIG );
			return false;
		}
		m_boinc_starter = tmp_starter;
		m_boinc_starter->setIsBOINC( true );
		m_boinc_starter->setReaperID( m_reaper_id );
		ASSERT( rip );
		m_boinc_starter->setExecuteDir( rip->executeDir() );
	}

		// now, we can actually spawn the BOINC client
	if( ! m_boinc_starter->spawn(NULL, time(NULL), NULL) ) {
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
	bool rval = m_boinc_starter->killHard(killing_timeout);
	if( ! rval ) {
		dprintf( D_ALWAYS, "BOINC_BackfillMgr::killClient(): "
				 "ERROR telling BOINC starter (pid %d) to hardkill\n",
				 (int)m_boinc_starter->pid() );
	} else {
		dprintf( D_FULLDEBUG, "BOINC_BackfillMgr::killClient(): "
				 "told BOINC starter (pid %d) to hardkill\n",
				 (int)m_boinc_starter->pid() );
	}
	return rval;
}


int
BOINC_BackfillMgr::reaper( int pid, int status )
{
	std::string status_str;
	statusString( status, status_str );
	dprintf( D_ALWAYS, "BOINC client (pid %d) %s\n", pid, status_str.c_str() );
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
	m_boinc_starter->exited(NULL, status);
	delete m_boinc_starter;
	m_boinc_starter = NULL;

		// once the client is gone, delete all our compute slots
	int i, max = m_slots.getsize();
	for( i=0; i < max; i++ ) {
		rmSlot( i );
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
BOINC_BackfillMgr::suspend( int /*slot_id*/ )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::resume( int /*slot_id*/ )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::softkill( int /*slot_id*/ )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::hardkill( int slot_id )
{
	if( ! (m_boinc_starter && m_boinc_starter->active()) ) {
			// no BOINC client running, we're done
		return true;
	}

		// PHASE 2: handle different slot_ids differently...
	if( slot_id != 0 && m_num_slots > 1 ) {
			// we're just trying to remove a single slot object, but
			// there are other active BOINC slots so we'll leave the
			// client running (on an SMP).  in this case, we'll just
			// remove the one object and be done immediately.
		if( ! m_slots[slot_id] ) {
			dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::hardkill(%d) "
					 "no BackfillSlot object with that id\n", slot_id );
			return false;
		}
		return rmSlot( slot_id );
	}

		// if we're here, we're done and we should really kill the
		// BOINC client. so, we can wait to remove the slot objects
		// until the reaper goes off...
	return killClient();
}


bool
BOINC_BackfillMgr::walk( BoincSlotMember member_func )
{
	bool rval = true;
	int i, num = 0, max = m_slots.getsize();
	for( i = 0; num < m_num_slots && i < max; i++ ) {
		if( m_slots[i] ) { 
			num++;
			if( ! (((BOINC_BackfillSlot*)m_slots[i])->*(member_func))() ) {
				rval = false;
			}
		}
	}
	return rval;
}
