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

#ifndef PROCINTERFACE_H
#define PROCINTERFACE_H

#include "condor_classad.h"
#include "procapi.h"

/** This is the interface between my ProcAPI class and Condor's ClassAd
    library.  Basically, I return a classad with the information requested,
    and I ask the ProcAPI class for the info.

    @author Mike Yoder
    @see ProcAPI procInfo
*/
class ProcAd {
 public:
  /// Default constructor
  ProcAd();
  /// Destructor
  ~ProcAd();

  /** getProcAd returns a ClassAd containing information about one process.
      Basically, ProcAPI::getProcInfo gets called, and the information
      returned by it gets stuffed into a ClassAd.

      @return A ClassAd with the desired information.
      @param pid The pid of the process to monitor.
      @see ProcAPI::getProcInfo
  */
  ClassAd * getProcAd ( pid_t pid );
  
  /** getProcSetAd returns a ClassAd containing the sums of the monitored 
      values for a set of pids.  These pids are specified by an array of 
      pids ('pids') that has 'numpids' elements.  Basically, 
      ProcAPI::getProcSetInfo gets called, and the info stuffed into
      a ClassAd.

      @return A ClassAd with the desired info.
      @param pids An array of pids to look at
      @param numpids the number of elements in pids.
      @see ProcAPI::getProcSetInfo
  */
  ClassAd * getProcSetAd ( pid_t *pids, int numpids );

  /** getFamilyAd returns a ClassAd containing the sums of the monitored
      values of a 'family' of processes descended from one pid.  By 
      'family', I mean that pid and all children, children's children, etc.
      ProcAPI::getFamilyInfo is called for the information.

      @return A ClassAd with the desired info
      @param fatherpid The elder pid in the family
      @see ProcAPI::getFamilyInfo
  */
  ClassAd * getFamilyAd ( pid_t fatherpid, PidEnvID *penvid );

 private:

  ClassAd * dumpToAd ( piPTR );
};

#endif
