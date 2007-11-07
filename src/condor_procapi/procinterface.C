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
#include "condor_pidenvid.h"
#include "procinterface.h"
#include "procapi_internal.h"


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
  int status; 

  ProcAPI::getProcInfo ( pid, pi, status );

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
  int status;

  ProcAPI::getProcSetInfo( pids, numpids, pi, status );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}

  /* getFamilyAd returns a ClassAd containing the sums of the monitored
     values of a 'family' of processes descended from one pid.  By 
     'family', I mean that pid and all children, children's children, etc. */
ClassAd * ProcAd::getFamilyAd ( pid_t fatherpid, PidEnvID *penvid ) {

  piPTR pi = NULL;
  ClassAd *ad;
  int status;

  ProcAPI::getFamilyInfo( fatherpid, pi, penvid, status );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}

ClassAd * ProcAd::dumpToAd( piPTR pi ) {

  char line[128];
  ClassAd *ad = new ClassAd;

  ad->SetMyTypeName( "PROCESS_INFORMATION" );
  ad->SetTargetTypeName( "ENQUIRING_MINDS_WANT_TO_KNOW" );

  sprintf ( line, "THIS_PID = %d", pi->pid );
  ad->Insert(line);
  sprintf ( line, "PARENT_PID = %ld", (long)pi->ppid );
  ad->Insert(line);
  sprintf ( line, "IMAGE_SIZE = %ld", (long)pi->imgsize );
  ad->Insert(line);
  sprintf ( line, "RESIDENT_SET_SIZE = %ld", (long)pi->rssize );
  ad->Insert(line);
  sprintf ( line, "MAJOR_PAGE_FAULTS = %ld", (long)pi->majfault );
  ad->Insert(line);
  sprintf ( line, "MINOR_PAGE_FAULTS = %ld", (long)pi->minfault );
  ad->Insert(line);
  sprintf ( line, "USER_TIME = %ld", (long)pi->user_time );
  ad->Insert(line);
  sprintf ( line, "SYSTEM_TIME = %ld", (long)pi->sys_time );
  ad->Insert(line);
  sprintf ( line, "PROCESS_AGE = %ld", (long)pi->age );
  ad->Insert(line);
  sprintf ( line, "PERCENT_CPU_USAGE = %6.2f",  pi->cpuusage );
  ad->Insert(line);

  return ad;
}
