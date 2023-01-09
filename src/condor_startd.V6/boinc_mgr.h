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

#ifndef _CONDOR_BOINC_MGR_H
#define _CONDOR_BOINC_MGR_H

#include "backfill_mgr.h"

class BOINC_BackfillSlot : public BackfillSlot
{
public:
	BOINC_BackfillSlot( int slot_id );
	virtual ~BOINC_BackfillSlot();

	virtual bool init();
	virtual bool start();
	virtual bool suspend();
	virtual bool resume();
	virtual bool softkill();
	virtual bool hardkill();
	virtual void publish( ClassAd* ad );

};


typedef bool (BOINC_BackfillSlot::*BoincSlotMember)();

class BOINC_BackfillMgr : public BackfillMgr
{
public:
	BOINC_BackfillMgr();
	~BOINC_BackfillMgr();

	virtual bool init();
	virtual bool reconfig();
	virtual const char* backfillSystemName() { return "BOINC"; };

		/** 
			In the case of a BOINC_BackfillMgr, we might have children
			to reap before our object can be deleted...
		*/
	virtual bool destroy();

	virtual bool addSlot( BOINC_BackfillSlot* boinc_slot );

		// for any of these, slot_id==0 means all slots
	virtual bool rmSlot( int slot_id );
	virtual bool start( int slot_id );
	virtual bool suspend( int slot_id );
	virtual bool resume( int slot_id );
	virtual bool softkill( int slot_id );
	virtual bool hardkill( int slot_id );

	int reaper( int pid, int status );

protected:
	bool spawnClient( class Resource * );
	bool killClient( void );

	Starter* m_boinc_starter;
	int m_reaper_id;
	bool m_delete_boinc_mgr;

};


#endif /* _CONDOR_BOINC_MGR_H */
