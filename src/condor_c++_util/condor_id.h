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

#ifndef _CONDOR_ID_H_
#define _CONDOR_ID_H_

#include "condor_common.h"
#include "dc_service.h"


/** An object to represent the Condor ID of a job.  Condor uses three
    integers (cluster, proc, subproc) to identify jobs.  This
    structure will be used to store those three numbers.
	This object used to live in src/condor_dagman/types.[Ch]
*/

class CondorID : public ServiceData
{
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

	/** Parses provided string of the form "cluster[.proc][.subproc]",
		and updates this object's values accordingly.
		@param s the string to parse
		@return 0-3, corresponding to the number of elements found & set
	*/
	int SetFromString( const char* s );

    /** Test for equality between two CondorID's.
        @param the other CondorID object
        @return true if equal, false if not
    */
    inline bool operator == (const CondorID &condorID) const {
        return Compare (condorID) == 0;
    }

		/** Comparison function for use with SelfDrainingQueue.
			This method is static (to live in the CondorID namespace).
			Takes pointers to two CondorID objects (though the
			pointers are of type ServiceData* to work properly w/
			SelfDrainingQueue and all the rest.
			@return -1 if a < b, 0 if a == b, and 1 if a > b
		*/
	static int ServiceDataCompare( ServiceData* a, ServiceData* b );

    /// The job cluster
    int _cluster;

    /// The job process number
    int _proc;

    /// The job subprocess number
    int _subproc;
};


#endif /* _CONDOR_ID_H_ */
