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
	bool init( void );

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


		// // // // // // // // // // // //
		// Job execution and state changes
		// // // // // // // // // // // //

		/** All jobs have been spawned by the starter.
		 */
	void allJobsSpawned( void );

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

		/** The last job this starter is controlling has exited.  Do
			whatever we have to do to cleanup and notify our
			controller. 
		*/
	bool allJobsDone( void );

		/** The last job this starter is controlling has been
   			completely cleaned up.  Since there's no shadow to tell us
			to go away, we have to exit ourselves.
		*/
	void allJobsGone( void );

		/** Someone is attempting to reconnect to this job.
		 */
	int reconnect( ReliSock* s, ClassAd* ad );


		// // // // // // // // // // // //
		// Notfication to our controller
		// // // // // // // // // // // //

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

	bool notifyStarterError( const char* err_msg, bool critical );

		// // // // // // // // // // // //
		// Misc utilities
		// // // // // // // // // // // //

		/** Make sure the given filename will be included in the
			output files of the job that are sent back to the job
			submitter.  
			@param filename File to add to the job's output list 
		*/
	void addToOutputFiles( const char* filename );


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

		/** Make sure the JobUniverse attribute in our job ClassAd
			(regardless of how we got it) is valid for a "local" job. 
			@param univ The value of the JobUniverse attribute
			@return true if allowed, false if invalid
		*/
	bool checkUniverse( int univ );


};


#endif /* _CONDOR_JIC_LOCAL_H */
