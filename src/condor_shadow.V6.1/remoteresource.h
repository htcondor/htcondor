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
#include "condor_classad.h"
#include "reli_sock.h"
#include "baseshadow.h"
#include "file_transfer.h"

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

		/** Constructor with parameters.
			@param shadow A pointer to the shadow which created this instance.
			@param executingHost The sinful string for the remote startd.
			@param capability The capability that the remote startd wants.
		*/
	RemoteResource( BaseShadow *shadow, const char * executingHost, 
					const char * capability );

		/// Destructor
	virtual ~RemoteResource();

		/** This function connects to the executing host and does
			an ACTIVATE_CLAIM command on it.  The capability, starternum
			and Job ClassAd are pushed, and the executing host's 
			full machine name and (hopefully) an OK are returned.
			@param starterVersion The version number of the starter wanted.
			                  The default is 2.
			@return 0 if everthing went ok, -1 if error.				  
		 */ 
	virtual int requestIt( int starterVersion = 2 );

		/** Here we tell the remote starter to kill itself in a gentle manner.
			@return 0 on success, -1 if a problem occurred.
		*/
	virtual int killStarter();

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

		/** Return the sinful string of the remote host.
			@param executingHost Will contain the host's sinful string.
		*/ 
	void getExecutingHost( char *& executingHost );

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

		/** Return the capability for talking to the host.
			@param capability Will contain the capability for the host.
		*/ 
	void getCapability( char *& capability );
	
		/** Return the sinful string of the starter.
			@param starterAddr Will contain the starter's sinful string.
		*/
	void getStarterAddress( char *& starterAddr );

		/** Return the claim socket associated with this remote host.  
			@return The claim socket for this host.
		*/ 
	ReliSock* getClaimSock();

		/** Return the reason this host exited.
			@return The exit reason for this host.
		*/ 
	int  getExitReason();

		/** Return the status this host exited with.
			@return The exit status for this host.
		*/ 
	int  getExitStatus();
	
		/** Set the sinful string for this host.
			@param executingHost The sinful string for this host.
		*/
	void setExecutingHost( const char *executingHost );

		/** Set the capability for this host.
			@param cabability The capability of this host.
		*/
	void setCapability( const char *capability );

		/** Set the address (sinful string).
			@param The starter's sinful string 
		*/
	void setStarterAddress( const char *starterAddr );
	
		/** Set the reason this host exited.  
			@param reason Why did it exit?  Film at 11.
		*/
	virtual void setExitReason( int reason );

		/** Set the status this host exited with.  
			@param status Host exit status.
		*/
	virtual void setExitStatus( int status );

		/** Set this resource's jobAd */
	void setJobAd( ClassAd *jA ) { this->jobAd = jA; };

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

 protected:

		/** Set the claimSock associated with this host.  
			XXX is this needed at all?  Time will tell.
			@param claimSock The socket used for communication with
			                 this host.
		*/
	void setClaimSock( ReliSock* claimSock);

		/** The jobAd for this resource.  Why is this here and not
			in the shadow?  Well...if we're an MPI job, say, and we
			want the different nodes to have a slightly different ad
			for things like i/o, etc, we have to have one copy of 
			the ClassAd for each resource...and thus, it's here. */
	ClassAd *jobAd;

		/* internal data: if you can't figure the following out.... */
	char *executingHost;
	char *machineName;
	char *capability;
	char *starterAddress;
	char *fs_domain;
	char *uid_domain;
	ReliSock *claimSock;
	int exitReason;
	int exitStatus;

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

 private:

		// initialization done in both constructors.
	void init ( BaseShadow *shad );

		// Making these private PREVENTS copying.
	RemoteResource( const RemoteResource& );
	RemoteResource& operator = ( const RemoteResource& );
};


#endif


