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

/* This file is the implementation of the RemoteResource class. */

#include "condor_common.h"
#include "remoteresource.h"
#include "exit.h"             // for JOB_BLAH_BLAH exit reasons
#include "condor_debug.h"     // for D_debuglevel #defines
#include "condor_string.h"    // for strnewp()
#include "condor_attributes.h"

// for remote syscalls, this is currently in NTreceivers.C.
extern int do_REMOTE_syscall();

// for remote syscalls...
ReliSock *syscall_sock;
RemoteResource *thisRemoteResource;

// 90 seconds to wait for a socket timeout:
const int RemoteResource::SHADOW_SOCK_TIMEOUT = 90;

RemoteResource::RemoteResource( BaseShadow *shad ) 
{
	executingHost = NULL;
	capability = NULL;
	init( shad );
}


RemoteResource::RemoteResource( BaseShadow * shad, 
								const char * eHost, 
								const char * cbility )
{
	executingHost = strnewp( eHost );
	capability    = strnewp( cbility );
	init( shad );
}


void
RemoteResource::init( BaseShadow *shad )
{
	shadow = shad;
	machineName = NULL;
	starterAddress = NULL;
	jobAd = NULL;
	fs_domain = NULL;
	uid_domain = NULL;
	claimSock = new ReliSock();
	exitReason = exitStatus = -1;
	memset( &remote_rusage, 0, sizeof(struct rusage) );
	disk_usage = 0;
	image_size = 0;
}

RemoteResource::~RemoteResource()
{
	if ( executingHost ) delete [] executingHost;
	if ( machineName   ) delete [] machineName;
	if ( capability    ) delete [] capability;
	if ( starterAddress) delete [] starterAddress;
	if ( uid_domain	   ) delete [] uid_domain;
	if ( fs_domain     ) delete [] fs_domain;
	if ( claimSock     ) delete claimSock;
	if ( jobAd         ) delete jobAd;
}


int
RemoteResource::requestIt( int starterVersion )
{
/* starterVersion is a default to 2. */

	int reply;
	
	if ( !executingHost || !capability ) {
		shadow->dprintf ( D_ALWAYS, "executingHost or capability not defined"
				  "in requestIt.\n" );
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return -1;
	}

	if ( !jobAd ) {
		shadow->dprintf( D_ALWAYS, "JobAd not defined in RemoteResource\n" );
		return -1;
	}

	claimSock->close();	// make sure ClaimSock is a virgin socket
	claimSock->timeout(SHADOW_SOCK_TIMEOUT);
	if (!claimSock->connect(executingHost, 0)) {
		shadow->dprintf(D_ALWAYS, "failed to connect to execute host %s\n", 
				executingHost);
		setExitReason(JOB_NOT_STARTED);
		return -1;
	}

	claimSock->encode();
	if (!claimSock->put(ACTIVATE_CLAIM)  ||
		!claimSock->code(capability)     ||
		!claimSock->code(starterVersion) ||
		!jobAd->put(*claimSock)          ||
		!claimSock->end_of_message())
	{
		shadow->dprintf(D_ALWAYS, "failed to send ACTIVATE_CLAIM "
                        "request to %s\n", executingHost);
		setExitReason(JOB_NOT_STARTED);
		return -1;
	}

	claimSock->decode();
	if (!claimSock->code(reply) || !claimSock->end_of_message()) {
		shadow->dprintf(D_ALWAYS, "failed to receive ACTIVATE_CLAIM "
                        "reply from %s\n", executingHost);
		setExitReason(JOB_NOT_STARTED);
		return -1;
	}

	if (reply != OK) {
		shadow->dprintf(D_ALWAYS, "Request to run on %s was REFUSED.\n",
				executingHost);
		setExitReason(JOB_NOT_STARTED);
		return -1;
	}

	shadow->dprintf(D_ALWAYS, "Request to run on %s was ACCEPTED.\n",
                    executingHost);
	return 0;
}


int
RemoteResource::killStarter()
{

	ReliSock sock;

	if ( !executingHost ) {
		shadow->dprintf ( D_ALWAYS, "In killStarter, "
                          "executingHost not defined.\n");
		return -1;
	}

	shadow->dprintf( D_ALWAYS, "Removing machine \"%s\".\n", 
					 machineName ? machineName : executingHost );

	sock.timeout(SHADOW_SOCK_TIMEOUT);
	if (!sock.connect(executingHost, 0)) {
		shadow->dprintf(D_ALWAYS, "failed to connect to executing host %s\n",
				executingHost );
		return -1;
	}

	sock.encode();
	if (!sock.put(KILL_FRGN_JOB) || 
		!sock.put(capability)    ||
		!sock.end_of_message())
	{
		shadow->dprintf(D_ALWAYS, "failed to send KILL_FRGN_JOB "
                        "to startd %s\n", executingHost );
		return -1;
	}

	return 0;
}


void
RemoteResource::dprintfSelf( int debugLevel )
{
	shadow->dprintf ( debugLevel, "RemoteResource::dprintSelf printing "
					  "host info:\n");
	if ( executingHost )
		shadow->dprintf ( debugLevel, "\texecutingHost: %s\n", executingHost);
	if ( capability )
		shadow->dprintf ( debugLevel, "\tcapability:    %s\n", capability);
	if ( machineName )
		shadow->dprintf ( debugLevel, "\tmachineName:   %s\n", machineName);
	if ( starterAddress )
		shadow->dprintf ( debugLevel, "\tstarterAddr:   %s\n", starterAddress);
	shadow->dprintf ( debugLevel, "\texitReason:    %d\n", exitReason);
	shadow->dprintf ( debugLevel, "\texitStatus:    %d\n", exitStatus);
}


void
RemoteResource::printExit( FILE *fp )
{
		/* Add more cases to the switch as they develop... */

	switch ( exitReason ) {
	case JOB_EXITED: {
		if ( WIFSIGNALED(exitStatus) ) {
			fprintf ( fp, "died on %s.\n", 
					  daemonCore->GetExceptionString(WTERMSIG(exitStatus)) );
		}
		else {
#ifndef WIN32
			if ( WEXITSTATUS(exitStatus) == ENOEXEC ) {
					/* What has happened here is this:  The starter
					   forked, but the exec failed with ENOEXEC and
					   called exit(ENOEXEC).  The exit appears normal, 
					   however, and the status is ENOEXEC - or 8.  If
					   the user job can exit with a status of 8, we're
					   hosed.  A better solution should be found. */
				fprintf( fp, "exited because of an invalid binary.\n" );
			} else 
#endif  // of ifndef WIN32
			{
				fprintf ( fp, "exited normally with status %d.\n", 
						  WEXITSTATUS(exitStatus) );
			}
		}

		break;
	}
	case JOB_KILLED: {
		fprintf ( fp, "was forceably removed with condor_rm.\n" );
		break;
	}
	case JOB_NOT_CKPTED: {
		fprintf ( fp, "was removed by condor, without a checkpoint.\n" );
		break;
	}
	case JOB_NOT_STARTED: {
		fprintf ( fp, "was never started.\n" );
		break;
	}
	case JOB_SHADOW_USAGE: {
		fprintf ( fp, "had incorrect arguments to the condor_shadow.\n" );
		fprintf ( fp, "                                    "
				  "This is an internal problem...\n" );
		break;
	}
	default: {
		fprintf ( fp, "has a strange exit reason of %d.\n", exitReason );
	}
	} // switch()
}


int
RemoteResource::handleSysCalls( Stream *sock )
{

		// change value of the syscall_sock to correspond with that of
		// this claim sock right before do_REMOTE_syscall().

	syscall_sock = claimSock;
	thisRemoteResource = this;

	if (do_REMOTE_syscall() < 0) {
		shadow->dprintf(D_SYSCALLS,"Shadow: do_REMOTE_syscall returned < 0\n");
			// we call our shadow's shutdown method:
		shadow->shutDown(exitReason, exitStatus);
			// close sock on this end...the starter has gone away.
		return TRUE;
	}

	return KEEP_STREAM;
}


void
RemoteResource::getExecutingHost( char *& eHost )
{

	if (!eHost) {
		eHost = strnewp ( executingHost );
	} else {
		if ( executingHost ) {
			strcpy ( eHost, executingHost );
		} else {
			eHost[0] = '\0';
		}
	}
}


void
RemoteResource::getMachineName( char *& mName )
{

	if ( !mName ) {
		mName = strnewp( machineName );
	} else {
		if ( machineName ) {
			strcpy( mName, machineName );
		} else {
			mName[0] = '\0';
		}
	}
}


void
RemoteResource::getUidDomain( char *& uidDomain )
{

	if ( !uidDomain ) {
		uidDomain = strnewp( uid_domain );
	} else {
		if ( uid_domain ) {
			strcpy( uidDomain, uid_domain );
		} else {
			uidDomain[0] = '\0';
		}
	}
}


void
RemoteResource::getFilesystemDomain( char *& filesystemDomain )
{

	if ( !filesystemDomain ) {
		filesystemDomain = strnewp( fs_domain );
	} else {
		if ( fs_domain ) {
			strcpy( filesystemDomain, fs_domain );
		} else {
			filesystemDomain[0] = '\0';
		}
	}
}


void
RemoteResource::getCapability( char *& cbility )
{

	if (!cbility) {
		cbility = strnewp( capability );
	} else {
		if ( capability ) {
			strcpy( cbility, capability );
		} else {
			cbility[0] = '\0';
		}
	}
}


void
RemoteResource::getStarterAddress( char *& starterAddr )
{

	if (!starterAddr) {
		starterAddr = strnewp( starterAddress );
	} else {
		if ( starterAddress ) {
			strcpy( starterAddr, starterAddress );
		} else {
			starterAddr[0] = '\0';
		}
	}
}


ReliSock*
RemoteResource::getClaimSock()
{
	return claimSock;
}


int
RemoteResource::getExitReason()
{
	return exitReason;
}


int
RemoteResource::getExitStatus()
{
	return exitStatus;
}


void
RemoteResource::setExecutingHost( const char * eHost )
{

	if ( executingHost )
		delete [] executingHost;
	
	executingHost = strnewp( eHost );
}

void
RemoteResource::setMachineName( const char * mName )
{

	if ( machineName )
		delete [] machineName;
	
	machineName = strnewp ( mName );
}

void
RemoteResource::setUidDomain( const char * uidDomain )
{

	if ( uid_domain )
		delete [] uid_domain;
	
	uid_domain = strnewp ( uidDomain );
}


void
RemoteResource::setFilesystemDomain( const char * filesystemDomain )
{

	if ( fs_domain )
		delete [] fs_domain;
	
	fs_domain = strnewp ( filesystemDomain );
}


void
RemoteResource::setCapability( const char * cbility )
{

	if ( capability )
		delete [] capability;
	
	capability = strnewp ( cbility );
}


void
RemoteResource::setStarterAddress( const char * starterAddr )
{

	if ( starterAddress )
		delete [] starterAddress;
	
	starterAddress = strnewp( starterAddr );
}


void
RemoteResource::setClaimSock( ReliSock * cSock )
{
	claimSock = cSock;
}


void
RemoteResource::setExitReason( int reason )
{
	// Set the exitReason, but not if the reason is JOB_KILLED.
	// This prevents exitReason being reset from JOB_KILLED to
	// JOB_NOT_CKPTED or some such when the starter gets killed
	// and the syscall sock goes away.

	shadow->dprintf ( D_FULLDEBUG, "setting exit reason on %s to %d\n", 
					  machineName ? machineName : executingHost, reason );

	if ( exitReason != JOB_KILLED ) {
		exitReason = reason;
	}
}


void
RemoteResource::setExitStatus( int status )
{
	shadow->dprintf ( D_FULLDEBUG, "setting exit status on %s to %d\n", 
					  machineName ? machineName : "???", status );

	exitStatus = status;
}


float
RemoteResource::bytesSent()
{
	float bytes = 0.0;

	// add in bytes sent by transferring files
	bytes += filetrans.TotalBytesSent();

	// add in bytes sent via remote system calls

	/*** until the day we support syscalls in the new shadow 
	if (syscall_sock) {
		bytes += syscall_sock->get_bytes_sent();
	}
	****/
	
	return bytes;
}


float
RemoteResource::bytesReceived()
{
	float bytes = 0.0;

	// add in bytes sent by transferring files
	bytes += filetrans.TotalBytesReceived();

	// add in bytes sent via remote system calls

	/*** until the day we support syscalls in the new shadow 
	if (syscall_sock) {
		bytes += syscall_sock->get_bytes_recvd();
	}
	****/
	
	return bytes;
}


void
RemoteResource::updateFromStarter( ClassAd* update_ad )
{
	int int_value;
	float float_value;
	
	dprintf( D_FULLDEBUG, "Inside RemoteResource::updateFromStarter()\n" );

	if( update_ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, float_value) ) {
		remote_rusage.ru_stime.tv_sec = (int) float_value; 
	}
			
	if( update_ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, float_value) ) {
		remote_rusage.ru_utime.tv_sec = (int) float_value; 
	}

	if( update_ad->LookupInteger(ATTR_IMAGE_SIZE, int_value) ) {
		image_size = int_value;
	}
			
	if( update_ad->LookupInteger(ATTR_DISK_USAGE, int_value) ) {
		disk_usage = int_value;
	}

}


void
RemoteResource::resourceExit( int exit_reason, int exit_status )
{
	dprintf( D_FULLDEBUG, "Inside RemoteResource::resourceExit()\n" );
	setExitReason( exit_reason );
	setExitStatus( exit_status );
}
