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
#include "procinterface.h"


ProcAd::ProcAd() {

}

ProcAd::~ProcAd() {

}

  /* getProcAd returns a ClassAd containing information about one process.
     Information can be retrieved on any process owned by the same user, 
     and on some systems (Linux, HPUX, Sol2.6) information can be gathered 
     from all processes.  See the ProcInfo struct definitions in procapi.h
     to see the exact definitions of what gets returned.  */
ClassAd * ProcAd::getProcAd ( pid_t pid ) {

  piPTR pi = NULL;
  ClassAd *ad;
  ProcAPI::getProcInfo ( pid, pi );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}
  
  /* getProcSetAd returns a ClassAd containing the sums of the monitored 
     values for a set of pids.  These pids are specified by an array of 
     pids ('pids') that has 'numpids' elements. */
ClassAd * ProcAd::getProcSetAd ( pid_t *pids, int numpids ) {

  piPTR pi = NULL;
  ClassAd *ad;
  ProcAPI::getProcSetInfo( pids, numpids, pi );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}

  /* getFamilyAd returns a ClassAd containing the sums of the monitored
     values of a 'family' of processes descended from one pid.  By 
     'family', I mean that pid and all children, children's children, etc. */
ClassAd * ProcAd::getFamilyAd ( pid_t fatherpid ) {

  piPTR pi = NULL;
  ClassAd *ad;
  ProcAPI::getFamilyInfo( fatherpid, pi );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}

ClassAd *
ProcAd::dumpToAd( piPTR pi ) {

	ClassAd *ad = new ClassAd;

	ad->SetMyTypeName( "PROCESS_INFORMATION" );
	ad->SetTargetTypeName( "ENQUIRING_MINDS_WANT_TO_KNOW" );

	ad->InsertAttr( "THIS_PID", (int)pi->pid );
	ad->InsertAttr( "PARENT_PID", (int)pi->ppid );
	ad->InsertAttr( "IMAGE_SIZE", (int)pi->imgsize );
	ad->InsertAttr( "RESIDENT_SET_SIZE", (int)pi->rssize );
	ad->InsertAttr( "MAJOR_PAGE_FAULTS", (int)pi->majfault );
	ad->InsertAttr( "MINOR_PAGE_FAULTS", (int)pi->minfault );
	ad->InsertAttr( "USER_TIME", (int)pi->user_time );
	ad->InsertAttr( "SYSTEM_TIME", (int)pi->sys_time );
	ad->InsertAttr( "PROCESS_AGE", (int)pi->age );
	ad->InsertAttr( "PERCENT_CPU_USAGE", (double)pi->cpuusage );
	
	return ad;
}
