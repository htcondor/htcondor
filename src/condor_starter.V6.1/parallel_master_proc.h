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
