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
#include "shadow_user_policy.h"
#include "user_log.c++.h"
#include "exit.h"
#include "../h/shadow.h"

/* Forward declaration to prevent loops... */
class RemoteResource;

/// What kind of update to the job queue are we performing?
typedef enum { 
	U_PERIODIC,
	U_TERMINATE,
	U_HOLD,
	U_REMOVE,
	U_REQUEUE,
	U_EVICT
} update_t;

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
			@return A mailer file for sending email.  Don't forget
			to do an email_close() on it!  NULL returned if no email
			to be sent.
		 */
	FILE* shutDownEmail( int reason );

		/** Everyone should be able to shut down.<p>
			@param reason The reason the job exited (JOB_BLAH_BLAH)
		 */
	virtual void shutDown( int reason );

		/** Graceful shutdown method.  This is virtual so each kind of
			shadow can do the right thing.
		 */
	virtual void gracefulShutDown( void ) = 0;

		/** Put this job on hold, if requested, notify the user about
			it, and exit with the appropriate status so that the
			schedd actually puts the job on hold.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			@param reason String describing why the job is held
		*/
	void holdJob( const char* reason );

		/** Remove the job from the queue, if requested, notify the
			user about it, and exit with the appropriate status so
			that the schedd actually removes the job.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			@param reason String describing why the job is removed
		*/
	void removeJob( const char* reason );

		/** The job exited, but we want to put it back in the job
			queue so it will run again.  If requested, notify the user about
			it, and exit with the appropriate status so that the
			schedd doesn't remove the job from the queue.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
			@param reason String describing why the job is requeued
		*/
	void requeueJob( const char* reason );

		/** The job exited, and it's ready to leave the queue.  If
			requested, notify the user about it, log a terminate
			event, update the job queue, etc.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
		*/
	void terminateJob( void );

		/** The job exited but it's not ready to leave the queue.  
			We still want to log an evict event, possibly email the
			user, etc.  We want to update the job queue, but not with
			anything about how the job was killed.<p>
			This uses the virtual cleanUp() method to take care of any
			universe-specific code before we exit.
		*/
	void evictJob( int reason );

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

	virtual int getExitReason( void ) = 0;

		/** Initializes the user log.  'Nuff said. 
		 */
	void initUserLog();

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
	FILE* emailUser( const char *subjectline );

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

	virtual bool setMpiMasterInfo( char* str ) = 0;

		/** Connect to the job queue and update all relevent
			attributes of the job class ad.  This checks our job
			classad to find any dirty attributes, and compares them
			against the lists of attribute names we care about.  
			@param type What kind of update we want to do
			@return true on success, false on failure
		*/
	bool updateJobInQueue( update_t type );

		/** Timer handler which just calls updateJobInQueue with the
			right arguments so we do a periodic update.
		*/
	void periodicUpdateQ( void );

		/** Do whatever cleanup (like killing starter(s)) that's
			required before the shadow can exit.
		*/
	virtual void cleanUp( void ) = 0;

		/** Did this shadow's job exit by a signal or not?  This is
			virtual since each kind of shadow will need to implement a
			different method to decide this. */
	virtual bool exitedBySignal( void ) = 0;

		/** If this shadow's job exited by a signal, what was it?  If
			not, return -1.  This is virtual since shadow will need to
			implement a different method to supply this
			information. */ 
	virtual int exitSignal( void ) = 0;

		/** If this shadow's job exited normally, what was the return
			code?  If it was killed by a signal, return -1.  This is
			virtual since shadow will need to implement a different
			method to supply this information. */
	virtual int exitCode( void ) = 0;

		// make UserLog static so it can be accessed by EXCEPTION handler
	static UserLog uLog;

	void evalPeriodicUserPolicy( void );

	const char* getCoreName( void );

	virtual void resourceBeganExecution( RemoteResource* rr ) = 0;

		/** Start a timer to do periodic updates of the job queue for
			this job.
		*/
	void startQueueUpdateTimer( void );

 protected:
	
		/** Note that this is the base, "unexpanded" ClassAd for the job.
			If we're a regular shadow, this pointer gets copied into the
			remoteresource.  If we're an MPI job we expand it based on
			something like $(NODE) into each remoteresource. */
	ClassAd *jobAd;

	ShadowUserPolicy shadow_user_policy;

	void emailHoldEvent( const char* reason );

	void emailRemoveEvent( const char* reason );

	void logRequeueEvent( const char* reason );
		// virtual void emailRequeueEvent( const char* reason );

	void logTerminateEvent( int exitReason );

	void logEvictEvent( int exitReason );

	virtual void emailTerminateEvent( int exitReason ) = 0;

		/** Initialize our StringLists for attributes we want to keep
			updated in the job queue itself
		*/ 
	void initJobQueueAttrLists( void );

 private:

	// private methods

		/** See if there's enough swap space for this shadow, and if  
			not, exit with JOB_NO_MEM.
		*/
	void checkSwap( void );

		/** Since the email for most of our events should be so
			similar, we put the code in a shared method to avoid
			duplication.
			@param action String describing the action we're taking
			@param reason The reason we're taking the action
			@param subject The subject for the email
		*/
	void emailActionEvent( const char* action, const char* reason, 
						   const char* subject );

		/** Update a specific attribute from our job ad into the
			queue.  This checks the type of the given ExprTree and
			calls the appropriate SetAttribute* function to do the
			work.  This function assumes you're in the middle of a
			qmgmt operation, i.e., that you've called ConnectQ().
			@param The ExprTree you want to update in the job queue 
			@return success or failure to set the attribute
		 */
	bool updateExprTree( ExprTree* tree );

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
	char *core_file_name;
	float prev_run_bytes_sent;
	float prev_run_bytes_recvd;

	// misc
	int q_update_tid;

		/// Pointers to lists of attribute names we care about

		/** Attributes that should go in the job queue regardless of
			what action we're taking with this job. */
	StringList* common_job_queue_attrs;

		/// Attributes specific to certain kinds of updates
	StringList* hold_job_queue_attrs;
	StringList* evict_job_queue_attrs;
	StringList* remove_job_queue_attrs;
	StringList* requeue_job_queue_attrs;
	StringList* terminate_job_queue_attrs;

		// This makes this class un-copy-able:
	BaseShadow( const BaseShadow& );
	BaseShadow& operator = ( const BaseShadow& );

};

extern void dumpClassad( const char*, ClassAd*, int );

extern BaseShadow *Shadow;

#endif

