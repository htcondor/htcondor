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
// BackfillVM
// // // // // // // // // // // // 

BackfillVM::BackfillVM( int vm_id )
{
	m_vm_id = vm_id;
}


BackfillVM::~BackfillVM()
{
}


bool
BackfillVM::enterActivity( Activity act )
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

	m_num_vms = 0;
		// make sure our array is initialized with NULLs
	m_vms.fill( NULL );
	m_vms.setFiller( NULL );
}


BackfillMgr::~BackfillMgr()
{
	dprintf( D_FULLDEBUG, "Destroying a BackfillMgr\n" );
}


