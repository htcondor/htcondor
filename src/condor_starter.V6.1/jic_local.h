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


#if !defined(_CONDOR_JIC_LOCAL_H)
#define _CONDOR_JIC_LOCAL_H

#include "job_info_communicator.h"

/** 
	This is the child class of JobInfoCommunicator that deals with
	running "local" jobs, namely, ones without a shadow.  It is itself
	an abstract base class for different versions of "local" jobs that
	might get their job classads from different places: the config
	file, a file from the filesystem, etc, etc.  JICLocal has some
	pure virtual functions, so you can't instantiate one of these, you
	have to instantiate a subclass.
*/


class JICLocal : public JobInfoCommunicator {
public:

		/// Constructor
	JICLocal();

		/// Destructor
	virtual ~JICLocal();

		// // // // // // // // // // // //
		// Initialization
		// // // // // // // // // // // //

		/// Initialize ourselves
	virtual bool init( void );

		/// Read anything relevent from the config file
	void config( void );

		/** Setup the execution environment for the job.  
		 */
	void setupJobEnvironment( void );

		/** Get the local job ad.  This is pure virutal since
			different kinds of JICLocal subclasses will do this in
			different ways, and we don't want anyone instantiating a
			JICLocal object directly.
		*/
	virtual bool getLocalJobAd( void ) = 0;

		// // // // // // // // // // // //
		// Information about the job 
		// // // // // // // // // // // //

		/** Total bytes sent by this job.  Since we're talking a local 
			job, this will always be 0.
		*/
	float bytesSent( void );

		/** Total bytes received by this job.  Since we're talking a
			local job, this will always be 0.
		*/
	float bytesReceived( void );

		/** Since the logic for getting the std filenames out of the
			job ad and munging them are identical for all 3, just use
			a helper to avoid duplicating code.  The only magic we do
			here is to make sure we've got full paths, and if they're
			not already, we prepend the job's iwd.
			@param attr_name ClassAd attribute name to lookup
			@return a strdup() allocated string for the filename we 
			        care about, or NULL if it's not in the job ad or
					points to /dev/null (or equiv).
		*/
	char* getJobStdFile( const char* attr_name );

		// // // // // // // // // // // //
		// Job Actions
		// // // // // // // // // // // //

		/** 
		 * These just are just place holders so that things
		 * will compile nicely. The logic that does real work
		 * is in JICLocalSchedd
		 **/ 
	virtual bool holdJob( const char*, int /*hold_reason_code*/, int /*hold_reason_subcode*/ ) { return ( false ); }
	virtual bool removeJob( const char* ) { return ( false ); }
	virtual bool terminateJob( const char* ) { return ( false ); }
	virtual bool requeueJob( const char* ) { return ( false ); }

		// // // // // // // // // // // //
		// Job execution and state changes
		// // // // // // // // // // // //

		/** The starter has been asked to suspend.  Take whatever
			steps make sense for the JIC, and notify our job
			controller what happend.
		*/
	void Suspend( void );

		/** The starter has been asked to continue.  Take whatever
			steps make sense for the JIC, and notify our job
			controller what happend.
		*/
	void Continue( void );

		/**
		   During cleanup, handle the step when file transfer would
		   happen.  We don't do any file transfer for JICLocal, so
		   just return true so we move directly onto the next stage.
		*/
	bool transferOutput( bool & ) { return true; };
	bool transferOutputMopUp( void ) { return true; };

		/** The last job this starter is controlling has been
   			completely cleaned up.  Since there's no shadow to tell us
			to go away, we have to exit ourselves.
		*/
	virtual void allJobsGone( void );

		/** Someone is attempting to reconnect to this job.
		 */
	int reconnect( ReliSock* s, ClassAd* ad );

		/** Someone is attempting to disconnect to this job.
		 */
	void disconnect();


		// // // // // // // // // // // //
		// Notfication to our controller
		// // // // // // // // // // // //

		/**
		   Write a periodic update ClassAd to a file, if requested.

		   @param update_ad Update ad to use if you've already got the info
		   @return true if success, false if failure
		*/
	virtual bool periodicJobUpdate(ClassAd* update_ad = NULL);

		/** Notify our controller that the job is about to spawn
		 */
	void notifyJobPreSpawn( void );

		/** Notify our controller that the job exited
			@param exit_status The exit status from wait()
			@param reason The Condor-defined exit reason
			@param user_proc The UserProc that was running the job
		*/
	bool notifyJobExit( int exit_status, int reason, 
						UserProc* user_proc );  

	int notifyJobTermination( UserProc* ) { return 0; };

	virtual bool notifyStarterError( const char* err_msg, bool critical, int hold_reason_code, int hold_reason_subcode );

		// // // // // // // // // // // //
		// Misc utilities
		// // // // // // // // // // // //

		/** Make sure the given filename will be included in the
			output files of the job that are sent back to the job
			submitter.  
			@param filename File to add to the job's output list 
		*/
	void addToOutputFiles( const char* filename );

		/** Make sure the given filename will be excluded from the
			list of files that the job sends back to the submitter.  
			@param filename File to remove from the job's output list 
		*/
	void removeFromOutputFiles( const char* filename );


protected:

		// // // // // // // // // // // //
		// Protected helper methods
		// // // // // // // // // // // //

		/** Register some important information about ourself that the
			job controller might need.
			@return true on success, false on failure
		*/
	bool registerStarterInfo( void );

		/** Initialize the priv_state code with the appropriate user
			for this job.
			@return true on success, false on failure
		*/
	bool initUserPriv( void );

		/** Publish information into the given classad for updates to
			our job controller
			@param ad ClassAd pointer to publish into
			@return true if success, false if failure
		*/ 
	bool publishUpdateAd( ClassAd* ad );

		/** Initialize our version of important information for this
			job which the starter will want to know.  This should
			init the following: orig_job_name, job_input_name, 
			job_output_name, job_error_name, job_iwd, and
			job_universe.  
			@return true on success, false on failure */
	bool initJobInfo( void );

		/** 
			Figure out the cluster and proc.  If they were given on
			the command-line, use those values.  If not there, check
			the classad.  If not there, use "1.0"
		*/
	void initJobId( void );

		/** Make sure the JobUniverse attribute in our job ClassAd
			(regardless of how we got it) is valid for a "local" job. 
			@param univ The value of the JobUniverse attribute
			@return true if allowed, false if invalid
		*/
	bool checkUniverse( int univ );

		/** Initialize our local UserLog-writing code.
		 */
	virtual bool initLocalUserLog( void );

		/// If the job ad says so, acquire user credentials
		// returns 0 if nothing to do, 1 if success < 0 on failure
	int initUserCredentials();
	void refreshSandboxCredentials_from_timer( int /* timerID */ ) { (void)initUserCredentials(); }
	int m_refresh_sandbox_creds_tid; /// timer id of the credential refresh timer

};


#endif /* _CONDOR_JIC_LOCAL_H */
