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
    CondorID() = default;
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
    int Compare (const CondorID& condorID) const;

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

    /** Test for inequality between two CondorID's.
        @param the other CondorID object
        @return true if less, false if not
    */
    inline bool operator< (const CondorID &condorID) const {
        return Compare (condorID) == -1;
    }

		/** Comparison function for use with SelfDrainingQueue.
			This method is static (to live in the CondorID namespace).
			Takes pointers to two CondorID objects (though the
			pointers are of type ServiceData* to work properly w/
			SelfDrainingQueue and all the rest.
			@return -1 if this < other, 0 if this == other, and 1 if this > other
		*/
	virtual int ServiceDataCompare( ServiceData const* other ) const;

		/** For use with SelfDrainingQueue. */
	virtual size_t HashFn( ) const;

    /// The job cluster
    int _cluster{-1};

    /// The job process number
    int _proc{-1};

    /// The job subprocess number
    int _subproc{-1};
};


#endif /* _CONDOR_ID_H_ */
