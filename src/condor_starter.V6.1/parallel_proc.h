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


#ifndef _CONDOR_PARALLEL_PROC_H
#define _CONDOR_PARALLEL_PROC_H

#include "condor_common.h"
#include "condor_classad.h"
#include "vanilla_proc.h"

/** Mostly, this class is a wrapper around VanillaProc, with the
	exception that Suspends really cause the job to exit, and that we
	have to keep track of the MPI Node (a.k.a. "MPI Rank") of this
	task.  
*/

class ParallelProc : public VanillaProc
{
 public:

    ParallelProc( ClassAd * jobAd );
    virtual ~ParallelProc();

		/** Pull the Node out of the job ClassAd and save it.
			Then, just call VanillaProc::StartJob() to do the real
			work. */
    virtual int StartJob();

		/** Add environment variables
			CONDOR_NPROCS
			CONDOR_PROCNO
			CONDOR_REMOTE_SPOOL_DIR
		  
			to the environment of each job
		*/
	virtual int addEnvVars();

    virtual bool JobReaper( int pid, int status );

		/** Parallen tasks shouldn't be suspended.  So, if we get a
			suspend, instead of sending the task a SIGSTOP, we
			tell it to shutdown, instead. */
    virtual void Suspend();

		/** This is here just so we can print out a log message, since
			we don't expect this will ever get called. */
    virtual void Continue();

		/** Publish our Node. */
	virtual bool PublishUpdateAd( ClassAd* ad );


	virtual bool ShutdownFast();
 protected:
	int Node;

};

#endif
