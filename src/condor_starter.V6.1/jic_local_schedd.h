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

#if !defined(_CONDOR_JIC_LOCAL_SCHEDD_H)
#define _CONDOR_JIC_LOCAL_SCHEDD_H

#include "jic_local_file.h"
#include "qmgr_job_updater.h"

/** 
	This is the child class of JICLocalFile (and therefore JICLocal
	and JobInfoCommunicator) that deals with running "local universe"
	jobs directly under a condor_schedd.  This JIC gets the job
	ClassAd info from a file (a pipe to STDIN, in fact).  Instead of
	simply reporting everything to a file, it reports info back to the
	schedd via special exit status codes.
*/

class JICLocalSchedd : public JICLocalFile {
public:

		/** Constructor 
			@param classad_filename Full path to the ClassAd, "-" if STDIN
			@param schedd_address Sinful string of the schedd's qmgmt port
			@param cluster Cluster ID number (if any)
			@param proc Proc ID number (if any)
			@param subproc Subproc ID number (if any)
		*/
	JICLocalSchedd( const char* classad_filename,
					const char* schedd_address,
					int cluster, int proc, int subproc );

		/// Destructor
	virtual ~JICLocalSchedd();

	virtual void allJobsGone( void );

		/// The starter has been asked to shutdown fast.
	virtual void gotShutdownFast( void );

		/// The starter has been asked to shutdown gracefully.
	virtual void gotShutdownGraceful( void );

		/// The starter has been asked to evict for condor_rm
	virtual void gotRemove( void );

		/// The starter has been asked to evict for condor_hold
	virtual void gotHold( void );

		/** Get the local job ad.  We need a JICLocalSchedd specific
			version of this so that after we grab the ad (using the
			JICLocal version), we need to initialize our
			QmgrJobUpdater object with a pointer to the ad.
		*/
	virtual bool getLocalJobAd( void );

		/** Notify the schedd that the job exited.  Actually, all we
			do here is update the job queue with final stats about the
			run, our exit code will actually notify the schedd about
			the job leaving the queue, etc.
			@param exit_status The exit status from wait()
			@param reason The Condor-defined exit reason
			@param user_proc The UserProc that was running the job
		*/
	virtual bool notifyJobExit( int exit_status, int reason, 
								UserProc* user_proc );  

protected:

		/// This version confirms we're handling a "local" universe job. 
	virtual bool getUniverse( void );

		/// Initialize our local UserLog-writing code.
	virtual bool initLocalUserLog( void );

		/// The value we will exit with to tell our schedd what happened
	int exit_code;

		/// The sinful string of the schedd's qmgmt command port
	char* schedd_addr;

		/// object for managing updates to the schedd's job queue for
		/// dynamic attributes in this job.
	QmgrJobUpdater* job_updater;

};


#endif /* _CONDOR_JIC_LOCAL_SCHEDD_H */
