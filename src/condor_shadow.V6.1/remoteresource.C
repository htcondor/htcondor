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


static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h) */

// for remote syscalls, this is currently in NTreceivers.C.
extern int do_REMOTE_syscall();

// for remote syscalls...
ReliSock *syscall_sock;
RemoteResource *thisRemoteResource;

// 90 seconds to wait for a socket timeout:
const int RemoteResource::SHADOW_SOCK_TIMEOUT = 90;

RemoteResource::RemoteResource( BaseShadow *shad ) {
	shadow = shad;
	executingHost = NULL;
	machineName = NULL;
	capability = NULL;
	starterAddress = NULL;
	jobAd = NULL;
	claimSock = new ReliSock();
	exitReason = exitStatus = -1;
}

RemoteResource::RemoteResource( BaseShadow *shad, const char * eHost, 
								const char * cbility ) {

	executingHost = new char[strlen(eHost)+1];
	capability = new char[strlen(cbility)+1];

	strcpy( executingHost, eHost );
	strcpy( capability, cbility );

	shadow = shad;
	machineName = NULL;
	starterAddress = NULL;
	jobAd = NULL;
	claimSock = new ReliSock();
	exitReason = exitStatus = -1;
}

RemoteResource::RemoteResource( const RemoteResource & ) {
		// XXX must implement!
}

RemoteResource& RemoteResource::operator = ( const RemoteResource& ) {
		// XXX must implement!
}

RemoteResource::~RemoteResource() {
	if ( executingHost ) delete [] executingHost;
	if ( machineName   ) delete [] machineName;
	if ( capability    ) delete [] capability;
	if ( starterAddress) delete [] starterAddress;
	if ( claimSock     ) delete claimSock;
	if ( jobAd         ) delete jobAd;
}

int RemoteResource::requestIt( int starterVersion ) {

	int			reply;
	
	if ( !executingHost || !capability ) {
		shadow->dprintf ( D_ALWAYS, "executingHost or capability not defined"
				  "in requestIt.\n" );
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return -1;
	}

	if ( !jobAd ) {
		dprintf( D_ALWAYS, "JobAd not defined in RemoteResource\n" );
		return -1;
	}

	claimSock->close();	// make sure ClaimSock is a virgin socket
	claimSock->timeout(SHADOW_SOCK_TIMEOUT);
	if (!claimSock->connect(executingHost, 0)) {
		dprintf(D_ALWAYS, "failed to connect to execute host %s\n", 
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
		dprintf(D_ALWAYS, "failed to send ACTIVATE_CLAIM request to %s\n",
				executingHost);
		setExitReason(JOB_NOT_STARTED);
		return -1;
	}

	claimSock->decode();
	if (!claimSock->code(reply) || !claimSock->end_of_message()) {
		dprintf(D_ALWAYS, "failed to receive ACTIVATE_CLAIM reply from %s\n",
				executingHost);
		setExitReason(JOB_NOT_STARTED);
		return -1;
	}

	if (reply != OK) {
		dprintf(D_ALWAYS, "Request to run on %s was REFUSED.\n",
				executingHost);
		setExitReason(JOB_NOT_STARTED);
		return -1;
	}

	dprintf(D_ALWAYS, "Request to run on %s was ACCEPTED.\n",executingHost);
	return 0;
}

int RemoteResource::killStarter() {
	ReliSock	sock;

	if ( !executingHost ) {
		dprintf ( D_ALWAYS, "In killStarter, executingHost not defined.\n");
		return -1;
	}

	sock.timeout(SHADOW_SOCK_TIMEOUT);
	if (!sock.connect(executingHost, 0)) {
		dprintf(D_ALWAYS, "failed to connect to executing host %s\n",
				executingHost );
		return -1;
	}

	sock.encode();
	if (!sock.put(KILL_FRGN_JOB) || 
		!sock.put(capability)    ||
		!sock.end_of_message())
	{
		dprintf(D_ALWAYS, "failed to send KILL_FRGN_JOB to startd %s\n",
				executingHost );
		return -1;
	}

	return 0;
}

void RemoteResource::dprintfSelf( int debugLevel ) {
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

int RemoteResource::handleSysCalls( Stream *sock ) {

		// XXX I *think* this is what I want to do...
		// change value of the syscall_sock to correspond with that of
		// this claim sock right before do_REMOTE_syscall().

	syscall_sock = claimSock;
	thisRemoteResource = this;

	if (do_REMOTE_syscall() < 0) {
		dprintf(D_SYSCALLS, "Shadow: do_REMOTE_syscall returned < 0\n");
			// we call our shadow's shutdown method:
		shadow->shutDown(exitReason, exitStatus);
	}

	return KEEP_STREAM;
}

void RemoteResource::getExecutingHost( char *& eHost ) {
		// yes, this looks yucky, but I wanted to cover all the cases...

	if (!eHost) {
		int len;
		if ( executingHost )
			len = strlen(executingHost)+1;
		else
			len = 1;

		eHost = new char[len];
	}
	
	if ( executingHost )
		strcpy ( eHost, executingHost );
	else
		eHost[0] = '\0';
}

void RemoteResource::getMachineName( char *& mName ) {
		// yes, this looks yucky, but I wanted to cover all the cases...

	if (!mName) {
		int len;
		if ( machineName )
			len = strlen(machineName)+1;
		else
			len = 1;

		mName = new char[len];
	}
	
	if ( machineName )
		strcpy ( mName, machineName );
	else
		mName[0] = '\0';
}

void RemoteResource::getCapability( char *& cbility ) {
		// yes, this looks yucky, but I wanted to cover all the cases...

	if (!cbility) {
		int len;
		if ( capability )
			len = strlen(capability)+1;
		else
			len = 1;

		cbility = new char[len];
	}
	
	if ( capability )
		strcpy ( cbility, capability );
	else
		cbility[0] = '\0';
}

void RemoteResource::getStarterAddress( char *& starterAddr ) {
		// yes, this looks yucky, but I wanted to cover all the cases...

	if (!starterAddr) {
		int len;
		if ( starterAddress )
			len = strlen(starterAddress)+1;
		else
			len = 1;

		starterAddr = new char[len];
	}
	
	if ( starterAddress )
		strcpy ( starterAddr, starterAddress );
	else
		starterAddr[0] = '\0';
}

ReliSock* RemoteResource::getClaimSock() {
	return claimSock;
}

int RemoteResource::getExitReason() {
	return exitReason;
}

int RemoteResource::getExitStatus() {
	return exitStatus;
}

void RemoteResource::setExecutingHost( const char * eHost ) {
	if ( !eHost )
		return;

	if ( executingHost )
		delete [] executingHost;
	
	executingHost = new char[strlen(eHost)+1];
	strcpy( executingHost, eHost );
}

void RemoteResource::setMachineName( const char * mName ) {
	if ( !mName )
		return;

	if ( machineName )
		delete [] machineName;
	
	machineName = new char[strlen(mName)+1];
	strcpy( machineName, mName );
}

void RemoteResource::setCapability( const char * cbility ) {
	if ( !cbility )
		return;

	if ( capability )
		delete [] capability;
	
	capability = new char[strlen(cbility)+1];
	strcpy( capability, cbility );
}

void RemoteResource::setStarterAddress( const char * starterAddr ) {
	if ( !starterAddr )
		return;

	if ( starterAddress )
		delete [] starterAddress;
	
	starterAddress = new char[strlen(starterAddr)+1];
	strcpy( starterAddress, starterAddr );
}

void RemoteResource::setClaimSock( ReliSock * cSock ) {
	claimSock = cSock;
}

void RemoteResource::setExitReason( int reason ) {
// old code was { if (ExitReason != JOB_KILLED) ExitReason = reason; }
// from Todd.  Why?
	exitReason = reason;
}

void RemoteResource::setExitStatus( int status ) {
	exitStatus = status;
}
