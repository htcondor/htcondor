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

#if !defined(_CONDOR_VANILLA_PROC_H)
#define _CONDOR_VANILLA_PROC_H

#include "os_proc.h"
#include "killfamily.h"


/* forward reference */
class SafeSock;

/** The Vanilla-type job process class.  Uses procfamily to do its
	dirty work.
 */
class VanillaProc : public OsProc
{
public:
		/// Constructor
	VanillaProc( ClassAd * jobAd );
		/// Destructor
	virtual ~VanillaProc();

		/** call OsProc::StartJob(), make a new ProcFamily with new
			process as head. */
	virtual int StartJob();

		/** call OsProc::JobCleanup(); make certain all decendants are
			dead with family->hardkill() */
	virtual int JobCleanup(int pid, int status);

		/** Call family->suspend() */
	virtual void Suspend();

		/** Cass family->resume() */
	virtual void Continue();

		/** Take a family snapshot, call OsProc::ShutDownGraceful() */
	virtual bool ShutdownGraceful();

		/** Do a family->hardkill(); */
	virtual bool ShutdownFast();

		/** Publish all attributes we care about for updating the job
			controller into the given ClassAd.  This function is just
			virtual, not pure virtual, since OsProc and any derived
			classes should implement a version of this that publishes
			any info contained in each class, and each derived version
			should also call it's parent's version, too.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad );

protected:

private:
	ProcFamily *family;

	// timer id for periodically taking a ProcFamily snapshot
	int snapshot_tid;
};

#endif
