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

#if !defined(_CONDOR_VANILLA_PROC_H)
#define _CONDOR_VANILLA_PROC_H

#include "os_proc.h"
#include "killfamily.h"
#include "file_transfer.h"


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

		/** Transfer files; call OsProc::StartJob(), make a new 
			ProcFamily with new process as head. */
	virtual int StartJob();

		/** Upload files if requested; call OsProc::JobExit(); 
			make certain all decendants are dead with family->hardkill() */
	virtual int JobExit(int pid, int status);

		/** Call family->suspend() */
	virtual void Suspend();

		/** Cass family->resume() */
	virtual void Continue();

		/** Take a family snapshot, call OsProc::ShutDownGraceful() */
	virtual bool ShutdownGraceful();

		/** Do a family->hardkill(); */
	virtual bool ShutdownFast();

		/** Publish all attributes we care about for updating the
			shadow into the given ClassAd.  This function is just
			virtual, not pure virtual, since OsProc and any derived
			classes should implement a version of this that publishes
			any info contained in each class, and each derived version
			should also call it's parent's version, too.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad );

protected:
	virtual int UpdateShadow();

	int TransferCompleted(FileTransfer *);

private:
	ProcFamily *family;
	FileTransfer *filetrans;

	// timer id for periodically taking a ProcFamily snapshot
	int snapshot_tid;

	// timer id for periodically sending info on job to Shadow
	int shadowupdate_tid;

	// UDP socket back to the shadow command port
	SafeSock *shadowsock;

	// the real job executable name (after ATTR_JOB_CMD
	// is switched to condor_exec).
	char jobtmp[_POSIX_PATH_MAX];

	// if true, transfer files at vacate time (in addtion to job exit)
	bool TransferAtVacate;

};

#endif
