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

#ifndef BASESHADOW_H
#define BASESHADOW_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"
#include "user_log.c++.h"
#include "exit.h"
#include "../h/shadow.h"

/* Forward declaration to prevent loops... */
class RemoteResource;

/** This is the base class for the various incarnations of the Shadow.<p>

	If you want to change something related to the shadow, 
	make sure you know how these classes interact.  If it's a general
	shadow thing, you very well may want to add it to this class.  
	However, if it is specific to one remote resource, you want to
	look at RemoteResource.  If it is single, MPI, or PVM (one day!)
	specific, make the change in that derived class. <p>

	More to come...<p>


	This class has some pure virtual functions, so it's an abstract
	class.  You therefore can't instantiate it; you must instantiate
	one of its dervide classes.<p>

	Based heavily on code by Todd Tannenbaum.
	@see RemoteResource
	@author Mike Yoder
*/
class BaseShadow : public Service 
{
 public:
		/// Default constructor
	BaseShadow();

		/// Destructor, it's virtual
	virtual ~BaseShadow();

		/** This is the basic initialization function.

			It does the following:
			<ul>
			 <li>Puts the args into class data members
			 <li>Stores the classAd, checks its info.
			 <li>calls config()
			 <li>calls initUserLog()
			 <li>registers handleJobRemoval on DC_SIGUSR1
			</ul>
			It should be called right after the constructor.
			@param jobAd The Ad for this job.
			@param schedd_addr The sinful string of the schedd
			@param cluster This job's cluster number
			@param proc This job's proc number
		*/
	void baseInit( ClassAd *jobAd, char schedd_addr[], 
					   char cluster[], char proc[]);

		/** Everyone must make an init with a bunch of parameters.<p>
			This function is <b>pure virtual</b>.
		 */
	virtual void init( ClassAd *jobAd, char schedd_addr[], char host[], 
			   char capability[], char cluster[], char proc[] ) = 0;

		/** Here, we param for lots of stuff in the config file.  Things
			param'ed for are: SPOOL, FILESYSTEM_DOMAIN, UID_DOMAIN, 
			USE_AFS, USE_NFS, and CKPT_SERVER_HOST.
		*/
	virtual void config();

		/** This function should be called when the job is ready to 
			shut down.  It decides wether or not to email the user, 
			based on the reason and some parameters in the jobAd. 
			If we should email the user, it opens a mailer file with
			the subject line "Job Cluster.Proc" and returns the FILE*.
			@param reason The reason for shutting down this job.
			@param exitStatus Status upon exit.
			@return A mailer file for sending email.  Don't forget
			to do an email_close() on it!  NULL returned if no email
			to be sent.
		 */
	FILE* shutDownEmail(int reason, int exitStatus);

		/** Everyone should be able to shut down.<p>
			This function is <b>pure virtual</b>.
			@param reason The reason the job exited (JOB_BLAH_BLAH)
			@param exitStatus The status upon exit
		 */
	virtual void shutDown( int reason, int exitStatus ) = 0;

		/** The total number of bytes sent over the network on
			behalf of this job.
			Each shadow class should override this function and
			do whatever it needs to do.
			The default implementation in the base just returns
			zero bytes.
			@return number of bytes sent over the network.
		*/
	virtual float bytesSent() { return 0.0; }

		/** The total number of bytes received over the network on
			behalf of this job.
			Each shadow class should override this function and
			do whatever it needs to do.
			The default implementation in the base just returns
			zero bytes.
			@return number of bytes sent over the network.
		*/
	virtual float bytesReceived() { return 0.0; }

		/** Initializes the user log.  'Nuff said. 
		 */
	void initUserLog();

		/** Write out the proper things to the user log upon
			a job's exit.  
			@param exitStatus The number from wait()
			@param exitReason The reason we exited
			@param res The remote resource that exited.  Needed for 
			             such things as amount of bytes transferred, etc.
		*/
	void endingUserLog( int exitStatus, int exitReason );

		/** Change to the 'Iwd' directory.  Send email if problem.
			@return 0 on success, -1 on failure.
		*/
	int cdToIwd();

		/** Remove this job.  This function is <b>pure virtual</b>.

		 */
	virtual int handleJobRemoval(int sig) = 0;

		/** This function returns a file pointer that one can 
			write an email message into.
			@return A mail message file pointer.
		*/
	FILE* emailUser(char *subjectline);

		/** This is used to tack on something (like "res #") 
			after the header and before the text of a dprintf
			message.
		*/
	virtual void dprintf_va( int flags, char* fmt, va_list args );

		/** A local dprintf maker that uses dprintf_va...
		 */
	void dprintf( int flags, char* fmt, ... );

		/// Returns the jobAd for this job
	ClassAd *getJobAd() { return jobAd; }
		/// Returns this job's cluster number
	int getCluster() { return cluster; }
		/// Returns this job's proc number
	int getProc() { return proc; }
		/// Returns the spool
	char *getSpool() { return spool; }
		/// Returns the schedd address
	char *getScheddAddr() { return scheddAddr; }
        /// Returns the current working dir for the job
    char *getIwd() { return iwd; }
        /// Returns the owner of the job - found in the job ad
    char *getOwner() { return owner; }

		/// Called by EXCEPT handler to log to user log
	static void BaseShadow::log_except(char *msg);

		/// Used by static and global functions to access shadow object
	static BaseShadow* myshadow_ptr;

		/** Method to handle command from the starter to update info 
			about the job.  Each kind of shadow needs to handle this
			differently, since, for example, cpu usage is stored in
			the remote resource object, and in mpi, we've got a whole
			list of those, while in the unishadow, we've only got 1.
		*/
	virtual int updateFromStarter(int command, Stream *s) = 0;

	virtual int getImageSize( void ) = 0;

	virtual int getDiskUsage( void ) = 0;

	virtual struct rusage getRUsage( void ) = 0;

		// make UserLog static so it can be accessed by EXCEPTION handler
	static UserLog uLog;

 protected:
	
		/** Note that this is the base, "unexpanded" ClassAd for the job.
			If we're a regular shadow, this pointer gets copied into the
			remoteresource.  If we're an MPI job we expand it based on
			something like $(NODE) into each remoteresource. */
	ClassAd *jobAd;

 private:

	// config file parameters
	char *spool;
	char *fsDomain;
	char *uidDomain;
	char *ckptServerHost;
	bool useAFS;
	bool useNFS;
	bool useCkptServer;

	// job parameters
	int cluster;
	int proc;
	char owner[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];
	char *scheddAddr;
	bool jobExitedGracefully;

		// This makes this class un-copy-able:
	BaseShadow( const BaseShadow& );
	BaseShadow& operator = ( const BaseShadow& );

};

extern void dumpClassad( const char*, ClassAd*, int );

#endif

