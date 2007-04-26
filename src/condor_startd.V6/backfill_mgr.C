/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "backfill_mgr.h"


// // // // // // // // // // // // 
// BackfillSlot
// // // // // // // // // // // // 

BackfillSlot::BackfillSlot( int slot_id )
{
	m_slot_id = slot_id;
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

	m_num_slots = 0;
		// make sure our array is initialized with NULLs
	m_slots.fill( NULL );
	m_slots.setFiller( NULL );
}


BackfillMgr::~BackfillMgr()
{
	dprintf( D_FULLDEBUG, "Destroying a BackfillMgr\n" );
}


