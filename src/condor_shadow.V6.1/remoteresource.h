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

#ifndef REMOTERESOURCE_H
#define REMOTERESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_daemon_client.h"
#include "condor_classad.h"
#include "reli_sock.h"
#include "baseshadow.h"
#include "file_transfer.h"

/** The states that a remote resource can be in.  If you add anything
	here you must A) put it before _RR_STATE_THRESHOLD and B) add a
	string to Resource_State_String in remoteresource.C */ 
typedef enum {
		/// Before the starter has been spawned
	RR_PRE, 
		/// While it's running
	RR_EXECUTING,
		/** We've told the job to go away, but haven't received 
			confirmation that it's really dead.  This state is 
		    skipped if the job exits on its own. */
	RR_PENDING_DEATH,
		/// After it has stopped (for whatever reason...)
	RR_FINISHED,
		/// Suspended at the execution site
	RR_SUSPENDED,
		/// Starter exists, but the job is not executing yet 
	RR_STARTUP,
		/// The threshold must be last!
	_RR_STATE_THRESHOLD
} ResourceState;

	/** Return the string version of the given ResourceState */
const char* rrStateToString( ResourceState s );

/** This class represents one remotely running user job.  <p>

	This class has a Socket to the remote job from which it can handle
	syscalls.  It can also talk to the startd and request to start a
	starter, and it can kill the starter as well.<p>

	For all functions that start with 'get' and are passed a char*, 
	the following applies: if a NULL value is passed in as the argument, 
	space is new'ed for you, *unless* the member you requested is NULL.  
	If it is NULL, space is not new'ed and a NULL is returned.  If the 
	parameter you pass is not NULL, it is assumed to point to valid memory, 
	and the char* will be copied in.  If the member you request is NULL, 
	the parameter you passed in will be set to "".<p>
	
    <b>Summary of get* functions:</b><p>
	<pre>
	Parameter  Member   Results
	---------  ------   ----------
	  NULL     exists    string strnewp'ed and returned
	  NULL      NULL     NULL returned
	 !NULL     exists    strcpy occurs
	 !NULL      NULL     "" into parameter
	</pre>

	More to come...<p>

	@author Mike Yoder
*/
class RemoteResource : public Service {

 public: 

		/** Constructor.  Pass it a pointer to the shadow which creates
			it, this is handy while in handleSysCalls.  
		*/
	RemoteResource( BaseShadow *shadow );

		/// Destructor
	virtual ~RemoteResource();

		/** This function connects to the executing host and does
			an ACTIVATE_CLAIM command on it.  The capability, starternum
			and Job ClassAd are pushed, and the executing host's 
			full machine name and (hopefully) an OK are returned.
			@param starterVersion The version number of the starter
                   wanted. The default is 2.
			@return true on success, false on failure
		 */ 
	bool activateClaim( int starterVersion = 2 );

		/** Tell the remote starter to kill itself.
			@param graceful Should we do a graceful or fast shutdown?
			@return true on success, false if a problem occurred.
		*/
	virtual bool killStarter( bool graceful = false );

		/** Print out this representation of the remote resource.
			@param debugLevel The dprintf debug level you wish to use 
		*/
	virtual void dprintfSelf( int debugLevel);

		/** Print out the ending status of this job.
			Used to email the user at the end...
			@param fp A valid file pointer initialized for writing.
		*/
	virtual void printExit( FILE *fp );

		/** Each of these remote resources can handle syscalls.  The 
			claimSock gets registered with daemonCore, and it will
			call this function to handle it.
			@param sock Stuff comes in here
			@return KEEP_STREAM
		*/
	virtual int handleSysCalls( Stream *sock );

		/** Return the machine name of the remote host.
			@param machineName Will contain the host's machine name.
		*/ 
	void getMachineName( char *& machineName );

		/** Return the filesystem domain of the remote host.
			@param filesystemDomain Will contain the host's fs_domain
		*/
	void getFilesystemDomain( char *& filesystemDomain );

		/** Return the uid domain of the remote host.
			@param uidDomain Will contain the host's uid_domain
		*/
	void getUidDomain( char *& uidDomain );

		/** Return the sinful string of the starter.
			@param starterAddr Will contain the starter's sinful string.
		*/
	void getStarterAddress( char *& starterAddr );

		/** Return the sinful string of the remote startd.
			@param sinful Will contain the host's sinful string.  If
			NULL, this will be a string allocated with new().  If
			sinful already exists, we assume it's a buffer and print
			into it.
       */ 
   void getStartdAddress( char *& sinful );

		/** Return the capability string of the remote startd.
			@param cap Will contain the capability string.  If NULL,
			this will be a string allocated with new().  If cap
			already exists, we assume it's a buffer and print into it.
       */
   void getCapability( char *& cap );

		/** Return the arch string of the starter.
			@param arch Will contain the starter's arch string.
		*/
	void getStarterArch( char *& arch );

		/** Return the opsys string of the starter.
			@param opsys Will contain the starter's opsys string.
		*/
	void getStarterOpsys( char *& opsys );

		/** Return the claim socket associated with this remote host.  
			@return The claim socket for this host.
		*/ 
	ReliSock* getClaimSock();

		/** Return the reason this host exited.
			@return The exit reason for this host.
		*/ 
	int  getExitReason();

		/** Set the sinful string and capability for the startd
			associated with this remote resource.
			@param sinful The sinful string for the startd
			@param capability The capability string for the startd
		*/
	void setStartdInfo( const char *sinful, const char* capability );

		/** Set the address (sinful string).
			@param The starter's sinful string 
		*/
	void setStarterAddress( const char *starterAddr );
	
		/** Set the Starter's Arch
			@param arch The starter's arch string 
		*/
	void setStarterArch( const char *arch );
	
		/** Set the Starter's Opsys
			@param arch The starter's opsys string 
		*/
	void setStarterOpsys( const char *opsys );
	
		/** Set the Starter's version string
			@param version The starter's version string
		*/
	void setStarterVersion( const char *version );
	
		/** Set the reason this host exited.  
			@param reason Why did it exit?  Film at 11.
		*/
	virtual void setExitReason( int reason );

		/** Set this resource's jobAd, and initialize any relevent
			local variables from the ad if the attributes are there.
		*/
	void setJobAd( ClassAd *jA );

		/** Get this resource's jobAd */
	ClassAd* getJobAd() { return this->jobAd; };

		/** Set the machine name for this host.
			@param machineName The machine name of this host.
		*/
	void setMachineName( const char *machineName );

		/** Set the filesystem domain for this host.
			@param filesystemDomain The filesystem domain of this host.
		*/
	void setFilesystemDomain( const char *filesystemDomain );

		/** Set the uid domain for this host.
			@param uidDomain The uid domain of this host.
		*/
	void setUidDomain( const char *uidDomain );

		/// The number of bytes sent to this resource.
	float bytesSent();
		
		/// The number of bytes received from this resource.
	float bytesReceived();

	FileTransfer filetrans;

	virtual void resourceExit( int exit_reason, int exit_status );

	virtual void updateFromStarter( ClassAd* update_ad );

	int getImageSize( void ) { return image_size; };
	int getDiskUsage( void ) { return disk_usage; };
	struct rusage getRUsage( void ) { return remote_rusage; };

		/** Return the state that this resource is in. */
	ResourceState getResourceState() { return state; };

		/** Change this resource's state */
	void setResourceState( ResourceState s );

		/** Record the fact that our starter suspended the job.  This
			includes updating our in-memory job ad, logging an event
			to the UserLog, etc.
			@param update_ad ClassAd with update info from the starter
			@return true on success, false on failure
		*/
	virtual bool recordSuspendEvent( ClassAd* update_ad );

		/** Record the fact that our starter resumed the job.  This
			includes updating our in-memory job ad, logging an event
			to the UserLog, etc.
			@param update_ad ClassAd with update info from the starter
			@return true on success, false on failure
		*/
	virtual bool recordResumeEvent( ClassAd* update_ad );

		/** Write the given event to the UserLog.  This is virtual so
			each kind of RemoteResource can define its own version.
			@param event Pointer to ULogEvent to write
			@return true on success, false on failure
		*/
	virtual bool writeULogEvent( ULogEvent* event );

		/// Did the job on this resource exit with a signal?
	bool exitedBySignal( void ) { return exited_by_signal; };

		/** If the job on this resource exited with a signal, return
			the signal.  If not, return -1. */
	int exitSignal( void );

		/** If the job on this resource exited normally, return the
			exit code.  If it was killed by a signal, return -1. */ 
	int exitCode( void );

		/** This method is called when the job at the remote resource
			has finally started to execute.  We use this to log the
			appropriate user log event(s), start various timers, etc. 
		*/
	virtual void beginExecution( void );

 protected:

		/** The jobAd for this resource.  Why is this here and not
			in the shadow?  Well...if we're an MPI job, say, and we
			want the different nodes to have a slightly different ad
			for things like i/o, etc, we have to have one copy of 
			the ClassAd for each resource...and thus, it's here. */
	ClassAd *jobAd;

		/* internal data: if you can't figure the following out.... */
	char *machineName;
	char *starterAddress;
	char *starterArch;
	char *starterOpsys;
	char *starter_version;
	char *fs_domain;
	char *uid_domain;
	ReliSock *claim_sock;
	int exit_reason;
	int exit_value;
	bool exited_by_signal;

		/// This is the timeout period to hear from a startd.  (90 seconds).
	static const int SHADOW_SOCK_TIMEOUT;

		// More resource-specific stuff goes here.

		// we keep a pointer to the shadow class around.  This is useful
		// in handleSysCalls.
	BaseShadow *shadow;
	
		// Info about the job running on this particular remote
		// resource:  
	struct rusage	remote_rusage;
	int 			image_size;
	int 			disk_usage;

	DCStartd* dc_startd;

	ResourceState state;

	bool began_execution;

private:

		/** For debugging, print out the values of various statistics
			related to our bookkeeping of suspend/resume activity for
			the job.
			@param debug_level What dprintf level to use
		*/
	void printSuspendStats( int debug_level );

		// Making these private PREVENTS copying.
	RemoteResource( const RemoteResource& );
	RemoteResource& operator = ( const RemoteResource& );
};


#endif


