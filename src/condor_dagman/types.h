/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

#ifndef _TYPES_H_
#define _TYPES_H_

#include "condor_common.h"    /* for <stdio.h> */
#include "condor_debug.h"

//---------------------------------------------------------------------------
#ifndef __cplusplus
typedef int bool;
#define true  1
#define false 0
#endif

///
typedef int JobID_t;

//---------------------------------------------------------------------------
/** An object to represent the Condor ID of a job.  Condor uses three integers
    (cluster, proc, subproc) to identify jobs.  This structure will be used to
    store those three numbers.
*/
class CondorID {
  public:
    /// Constructor
    CondorID () : _cluster(-1), _proc(-1), _subproc(-1) {}

    /// Copy Constructor
    CondorID (int cluster, int proc, int subproc):
        _cluster(cluster), _proc(proc), _subproc(subproc) {}
    
    ///
    inline void Set (int cluster, int proc, int subproc) {
        _cluster = cluster;
        _proc    = proc;
        _subproc = subproc;
    }

    /** Compares this condorID's with another.
        @param condorID the other CondorID to compare
        @return zero if they match
    */
    int Compare (const CondorID condorID) const;
    
    /** Test for equality between two CondorID's.
        @param the other CondorID object
        @return true if equal, false if not
    */
    inline bool operator == (const CondorID condorID) const {
        return Compare (condorID) == 0;
    }
    
    /// The job cluster
    int _cluster;

    /// The job process number
    int _proc;

    /// The job subprocess number
    int _subproc;
};


#endif /* #ifndef _TYPES_H_ */
