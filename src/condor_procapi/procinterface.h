/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
