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
  ClassAd * getFamilyAd ( pid_t fatherpid );

 private:

  ClassAd * dumpToAd ( piPTR );
};

#endif
