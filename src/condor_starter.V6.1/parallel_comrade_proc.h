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

#ifndef _CONDOR_PARALLEL_COMRADE_PROC_H
#define _CONDOR_PARALLEL_COMRADE_PROC_H

#include "condor_common.h"
#include "condor_classad.h"
#include "vanilla_proc.h"

/** Mostly, this class is a wrapper around VanillaProc, with the
	exception that Suspends really cause the job to exit, and that we
	have to keep track of the parallel Node (a.k.a. "parallel Rank") of
	this task.  The notion for this class is that all the parallel nodes
	are	"comrades" (none of this master-slave stuff...). */

class ParallelComradeProc : public VanillaProc
{
 public:

    ParallelComradeProc( ClassAd * jobAd );
    virtual ~ParallelComradeProc();

		/** Pull the parallel Node out of the job ClassAd and save it.
			Then, just call VanillaProc::StartJob() to do the real
			work. */
    virtual int StartJob();

		/** We don't need to do anything special, but because C++ is
			lame, we need to define this.  ParallelMasterProc wants to
			implement its own version, and everyone just wants to call
			up the class hierarchy.  However, there's no way in C++ to
			say "climb up my class hierarchy and call this function
			for the next class that implements it."  So, everyone
			really has to define their own and explicitly call their
			parent's, even if they have nothing special to add.
		*/
    virtual int JobCleanup( int pid, int status );

		/** Parallel tasks shouldn't be suspended.  So, if we get a
			suspend, instead of sending the parallel task a SIGSTOP, we
			tell it to shutdown, instead. */
    virtual void Suspend();

		/** This is here just so we can print out a log message, since
			we don't expect this will ever get called. */
    virtual void Continue();

		/** Publish our parallel Node. */
	virtual bool PublishUpdateAd( ClassAd* ad );

 protected:
	int Node;

};

#endif
