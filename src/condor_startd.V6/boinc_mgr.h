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
#ifndef _CONDOR_BOINC_MGR_H
#define _CONDOR_BOINC_MGR_H

#include "backfill_mgr.h"

class BOINC_BackfillVM : public BackfillVM
{
public:
	BOINC_BackfillVM( int vm_id );
	virtual ~BOINC_BackfillVM();

	virtual bool init();
	virtual bool start();
	virtual bool suspend();
	virtual bool resume();
	virtual bool softkill();
	virtual bool hardkill();
	virtual void publish( ClassAd* ad );

};


typedef bool (BOINC_BackfillVM::*BoincVmMember)();

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

	virtual bool addVM( BOINC_BackfillVM* boinc_vm );

		// for any of these, vm_id==0 means all VMs
	virtual bool rmVM( int vm_id );
	virtual bool start( int vm_id );
	virtual bool suspend( int vm_id );
	virtual bool resume( int vm_id );
	virtual bool softkill( int vm_id );
	virtual bool hardkill( int vm_id );

	int reaper( int pid, int status );

	bool walk( BoincVmMember );

protected:
	bool spawnClient( void );
	bool killClient( void );

	Starter* m_boinc_starter;
	int m_reaper_id;
	bool m_delete_boinc_mgr;

};


#endif /* _CONDOR_BOINC_MGR_H */
