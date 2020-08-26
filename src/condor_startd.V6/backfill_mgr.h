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

#ifndef _CONDOR_BACKFILL_MGR_H
#define _CONDOR_BACKFILL_MGR_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "condor_state.h"
#include "extArray.h"


/**
   BackfillSlot base class

   Defines a consistent interface the startd will use to interact with
   a given computational instance of a backfill system on a single slot.
   Depending on the backfill backend, there might be multiple programs
   running, or a single program where resources are given and
   reclaimed dynamically.  In both cases, Condor will still think of
   these compute slots as separate entities, and the BackfillMgr class
   implemented for a given backend system will handle the details.
   Initially there will only be derived classes for BOINC, but
   someday, others might be added, too.
*/

class BackfillSlot
{
public:
	BackfillSlot( int slot_id );
	virtual ~BackfillSlot() = 0;

	int getSlotID() const { return m_slot_id; };
	virtual bool init() = 0;
	virtual bool start() = 0;
	virtual bool suspend() = 0;
	virtual bool resume() = 0;
	virtual bool softkill() = 0;
	virtual bool hardkill() = 0;
	virtual void publish( ClassAd* ) = 0;

protected:

		/// Protected data
	int        m_slot_id;
	Activity   m_activity;
	time_t     m_entered_current_activity;

		/// Protected methods
	bool enterActivity( Activity act );

};



/**
   BackfillMgr base class

   The BackfillMgr maintains a set of BackfillSlot objects, and
   defines a consistent interface the startd will use to interact with
   a given backfill backend system.  Initially, there will only be
   derived classes for BOINC, but someday, others might be added.
*/

class BackfillMgr : public Service
{
public:
	BackfillMgr(); 
	virtual ~BackfillMgr() = 0;

	virtual bool init() = 0;
	virtual bool reconfig() = 0;

		/** @return a string that identifies what kind of Backfill
			Manager (the BACKFILL_SYSTEM config knob) this is...
		*/
	virtual const char* backfillSystemName() = 0;

		/** The ResMgr wants this BackfillMgr to go away.  However,
			since there might be children or other cleanup that can't
			be done instantaneously, the ResMgr can't just delete us
			directly.  When they call destroy(), the BackfillMgr will
			do as much cleanup as possible.  If there's nothing else
			todo, destroy() will return true.  If there's still
			cleanup that needs to be done (e.g. waiting for children
			to exit), destroy() will return false to tell the ResMgr
			we're not ready to be deleted yet.  In this case, when the
			BackfillMgr is finally ready, it will call
			ResMgr::backfillMgrDone().
		*/
	virtual bool destroy() = 0;

		// for any of these, using slot_id 0 means all slots
	virtual bool rmSlot( int slot_id ) = 0;
	virtual bool start( int slot_id ) = 0;
	virtual bool suspend( int slot_id ) = 0;
	virtual bool resume( int slot_id ) = 0;
	virtual bool softkill( int slot_id ) = 0;
	virtual bool hardkill( int slot_id ) = 0;

protected:

	ExtArray<BackfillSlot*> m_slots;
	int m_num_slots;

};


#endif /* _CONDOR_BACKFILL_MGR_H */
