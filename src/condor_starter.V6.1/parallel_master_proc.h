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

#ifndef _CONDOR_PARALLEL_MASTER_PROC
#define _CONDOR_PARALLEL_MASTER_PROC

#include "condor_common.h"
#include "condor_classad.h"
#include "parallel_comrade_proc.h"

/** This class is for the parallel "master" process.  It's derived from
	the comrade class (it's "more equal" than the others :-) ).  It 
	is special because it alters the path of the master before it's 
	started, so that our sneaky rsh can get called instead of the
	regular rsh.  Also, a variable is slipped into the environment for
	our rsh so it knows how to contact the shadow... */

class ParallelMasterProc : public ParallelComradeProc
{
 public:
    
    ParallelMasterProc( ClassAd * jobAd );
    virtual ~ParallelMasterProc();

    virtual int StartJob();

 private:

        /// puts sneaky rsh first in path; puts PARALLEL_SHADOW_SINFUL in env
    int alterEnv();
};

#endif
