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
#include "backfill_mgr.h"


// // // // // // // // // // // // 
// BackfillSlot
// // // // // // // // // // // // 

BackfillSlot::BackfillSlot( int slot_id )
{
	m_slot_id = slot_id;
	m_activity = no_act;
	m_entered_current_activity = 0;
}


BackfillSlot::~BackfillSlot()
{
}


bool
BackfillSlot::enterActivity( Activity act )
{
	if( m_activity == act ) {
			// already there, nothing else to do
		return true;
	}

		// verify before we do?
	m_activity = act;
	m_entered_current_activity = time(0);

	return true;
}



// // // // // // // // // // // // 
// BackfillMgr
// // // // // // // // // // // // 

BackfillMgr::BackfillMgr()
{
	dprintf( D_FULLDEBUG, "Instantiating a BackfillMgr\n" );
}


BackfillMgr::~BackfillMgr()
{
	dprintf( D_FULLDEBUG, "Destroying a BackfillMgr\n" );
}


