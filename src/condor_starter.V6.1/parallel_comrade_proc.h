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
