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
#include "internet.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// for remote syscalls, this is currently in NTreceivers.C.
extern int do_REMOTE_syscall();

// for remote syscalls...
ReliSock *syscall_sock;
RemoteResource *thisRemoteResource;

// 90 seconds to wait for a socket timeout:
const int RemoteResource::SHADOW_SOCK_TIMEOUT = 90;

static char *Resource_State_String [] = {
	"PRE", 
	"EXECUTING", 
	"PENDING_DEATH", 
	"FINISHED",
	"SUSPENDED",
};

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
	executingHost = NULL;
	setExecutingHost( eHost );
	capability    = strnewp( cbility );
	init( shad );
}


void
RemoteResource::init( BaseShadow *shad )
{
	shadow = shad;
	machineName = NULL;
	starterAddress = NULL;
	starterArch = NULL;
	starterOpsys = NULL;
	jobAd = NULL;
	fs_domain = NULL;
	uid_domain = NULL;
	claimSock = new ReliSock();
	exitReason = exitStatus = -1;
	memset( &remote_rusage, 0, sizeof(struct rusage) );
	disk_usage = 0;
	image_size = 0;
	state = RR_PRE;
}

RemoteResource::~RemoteResource()
{
	if ( executingHost ) delete [] executingHost;
	if ( machineName   ) delete [] machineName;
	if ( capability    ) delete [] capability;
	if ( starterAddress) delete [] starterAddress;
	if ( starterArch   ) delete [] starterArch;
	if ( starterOpsys  ) delete [] starterOpsys;
	if ( uid_domain	   ) delete [] uid_domain;
	if ( fs_domain     ) delete [] fs_domain;
	if ( claimSock     ) delete claimSock;
	if ( jobAd         ) delete jobAd;
}


bool
RemoteResource::requestIt( int starterVersion )
{
/* starterVersion is a default to 2. */

	int reply;
	
	if ( !executingHost || !capability ) {
		shadow->dprintf ( D_ALWAYS, "executingHost or capability not defined"
				  "in requestIt.\n" );
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return false;
	}

	if ( !jobAd ) {
		shadow->dprintf( D_ALWAYS, "JobAd not defined in RemoteResource\n" );
		return false;
	}

	claimSock->close();	// make sure ClaimSock is a virgin socket
	claimSock->timeout(SHADOW_SOCK_TIMEOUT);
	if (!claimSock->connect(executingHost, 0)) {
		shadow->dprintf(D_ALWAYS, "failed to connect to execute host %s\n", 
				executingHost);
		setExitReason(JOB_NOT_STARTED);
		return false;
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
		return false;
	}

	claimSock->decode();
	if (!claimSock->code(reply) || !claimSock->end_of_message()) {
		shadow->dprintf(D_ALWAYS, "failed to receive ACTIVATE_CLAIM "
                        "reply from %s\n", executingHost);
		setExitReason(JOB_NOT_STARTED);
		return false;
	}

	if (reply != OK) {
		shadow->dprintf(D_ALWAYS, "Request to run on %s was REFUSED.\n",
				executingHost);
		setExitReason(JOB_NOT_STARTED);
		return false;
	}

	shadow->dprintf( D_ALWAYS, "Request to run on %s was ACCEPTED.\n",
					 executingHost );
	setResourceState( RR_EXECUTING );
	return true;
}


bool
RemoteResource::killStarter()
{

	ReliSock sock;

	if ( !executingHost ) {
		shadow->dprintf ( D_ALWAYS, "In killStarter, "
                          "executingHost not defined.\n");
		return false;
	}

	shadow->dprintf( D_ALWAYS, "Removing machine \"%s\".\n", 
					 machineName ? machineName : executingHost );

	sock.timeout(SHADOW_SOCK_TIMEOUT);
	if (!sock.connect(executingHost, 0)) {
		shadow->dprintf(D_ALWAYS, "failed to connect to executing host %s\n",
				executingHost );
		return false;
	}

	sock.encode();
	if (!sock.put(KILL_FRGN_JOB) || 
		!sock.put(capability)    ||
		!sock.end_of_message())
	{
		shadow->dprintf(D_ALWAYS, "failed to send KILL_FRGN_JOB "
                        "to startd %s\n", executingHost );
		return false;
	}

	if( state != RR_FINISHED ) {
		setResourceState( RR_PENDING_DEATH );
	}
	return true;
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
	char ename[ATTRLIST_MAX_EXPRESSION];
	int got_exception = jobAd->LookupString(ATTR_EXCEPTION_NAME,ename);

	switch ( exitReason ) {
	case JOB_EXITED: {
		if ( WIFSIGNALED(exitStatus) ) {
			
			if(got_exception) {
				fprintf ( fp, "died with exception %s\n", ename );
			} else {
				fprintf ( fp, "died on %s.\n", 
						  daemonCore->GetExceptionString(WTERMSIG(exitStatus)) );
			}
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


void
RemoteResource::getStarterArch( char *& arch )
{
	if(! arch ) {
		arch = strnewp( starterArch );
	} else {
		if ( starterArch ) {
			strcpy( arch, starterArch );
		} else {
			arch[0] = '\0';
		}
	}
}


void
RemoteResource::getStarterOpsys( char *& opsys )
{
	if(! opsys ) {
		opsys = strnewp( starterOpsys );
	} else {
		if ( starterOpsys ) {
			strcpy( opsys, starterOpsys );
		} else {
			opsys[0] = '\0';
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

		/*
		  Tell daemonCore that we're willing to
		  grant WRITE permission to whatever machine we are claiming.
		  This greatly simplifies DaemonCore permission stuff
		  for flocking, since submitters don't have to know all the
		  hosts they might possibly run on, all they have to do is
		  trust the central managers of all the pools they're flocking
		  to (which they have to do, already).  
		  Added on 3/15/01 by Todd Tannenbaum <tannenba@cs.wisc.edu>
		*/
	char *addr;
	if( (addr = string_to_ipstr(eHost)) ) {
		daemonCore->AddAllowHost( addr, WRITE );
	} else {
		dprintf( D_ALWAYS, "ERROR: Can't convert \"%s\" to an IP address!\n", 
				 eHost );
	}

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
	if( starterAddress ) {
		delete [] starterAddress;
	}	
	starterAddress = strnewp( starterAddr );
}


void
RemoteResource::setStarterArch( const char * arch )
{
	if( starterArch ) {
		delete [] starterArch;
	}	
	starterArch = strnewp( arch );
}


void
RemoteResource::setStarterOpsys( const char * opsys )
{
	if( starterOpsys ) {
		delete [] starterOpsys;
	}	
	starterOpsys = strnewp( opsys );
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
	setResourceState( RR_FINISHED );
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

	if( DebugFlags & D_MACHINE ) {
		dprintf( D_MACHINE, "Update ad:\n" );
		update_ad->dPrint( D_MACHINE );
		dprintf( D_MACHINE, "--- End of ClassAd ---\n" );
	}

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

	char* job_state = NULL;
	ResourceState new_state = state;
	update_ad->LookupString( ATTR_JOB_STATE, &job_state );
	if( job_state ) { 
			// The starter told us the job state, see what it is and
			// if we need to log anything to the UserLog
		if( stricmp(job_state, "Suspended") == MATCH ) {
			new_state = RR_SUSPENDED;
		} else if ( stricmp(job_state, "Running") == MATCH ) {
			new_state = RR_EXECUTING;
		} else { 
				// For our purposes in here, we don't care about any
				// other possible states at the moment.  If the job
				// has state "Exited", we'll see all that in a second,
				// and we don't want to log any events here. 
		}
		if( new_state != state ) {
				// State change!  Let's log the appropriate event to
				// the UserLog and keep track of suspend/resume
				// statistics.
			switch( new_state ) {
			case RR_SUSPENDED:
				recordSuspendEvent( update_ad );
				break;
			case RR_EXECUTING:
				recordResumeEvent( update_ad );
				break;
			default:
				EXCEPT( "Trying to log state change for invalid state %s",
						rrStateToString(new_state) );
			}
				// record the new state
			setResourceState( new_state );
		}
		free( job_state );
	}
}


bool
RemoteResource::recordSuspendEvent( ClassAd* update_ad )
{
	bool rval = true;
		// First, grab the number of pids that were suspended out of
		// the update ad.
	int num_pids;
	if( ! update_ad->LookupInteger(ATTR_NUM_PIDS, num_pids) ) {
		dprintf( D_FULLDEBUG, "update ad from starter does not define %s\n",
				 ATTR_NUM_PIDS );  
		num_pids = 0;
	}
	
		// Now, we can log this event to the UserLog
	JobSuspendedEvent event;
	event.num_pids = num_pids;
	if( !writeULogEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_SUSPENDED event\n" );
		rval = false;
	}

		// Finally, we need to update some attributes in our in-memory
		// copy of the job ClassAd
	int now = (int)time(NULL);
	int total_suspensions = 0;
	char tmp[256];

	jobAd->LookupInteger( ATTR_TOTAL_SUSPENSIONS, total_suspensions );
	total_suspensions++;
	sprintf( tmp, "%s = %d", ATTR_TOTAL_SUSPENSIONS, total_suspensions );
	jobAd->Insert( tmp );

	sprintf( tmp, "%s = %d", ATTR_LAST_SUSPENSION_TIME, now );
	jobAd->Insert( tmp );

		// Log stuff so we can check our sanity
	printSuspendStats( D_FULLDEBUG );

	return rval;
}


bool
RemoteResource::recordResumeEvent( ClassAd* update_ad )
{
	bool rval = true;

		// First, log this to the UserLog
	JobUnsuspendedEvent event;
	if( !writeULogEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_UNSUSPENDED event\n" );
		rval = false;
	}

		// Now, update our in-memory copy of the job ClassAd
	int now = (int)time(NULL);
	int cumulative_suspension_time = 0;
	int last_suspension_time = 0;
	char tmp[256];

		// add in the time I spent suspended to a running total
	jobAd->LookupInteger( ATTR_CUMULATIVE_SUSPENSION_TIME,
						  cumulative_suspension_time );
	jobAd->LookupInteger( ATTR_LAST_SUSPENSION_TIME,
						  last_suspension_time );
	cumulative_suspension_time += now - last_suspension_time;

	sprintf( tmp, "%s = %d", ATTR_CUMULATIVE_SUSPENSION_TIME,
			 cumulative_suspension_time );
	jobAd->Insert( tmp );

		// set the current suspension time to zero, meaning not suspended
	sprintf(tmp, "%s = 0", ATTR_LAST_SUSPENSION_TIME );
	jobAd->Insert( tmp );

		// Log stuff so we can check our sanity
	printSuspendStats( D_FULLDEBUG );

	return rval;
}


bool
RemoteResource::writeULogEvent( ULogEvent* event )
{
	if( !shadow->uLog.writeEvent(event) ) {
		return false;
	}
	return true;
}


void
RemoteResource::printSuspendStats( int debug_level )
{
	int total_suspensions = 0;
	int last_suspension_time = 0;
	int cumulative_suspension_time = 0;

	dprintf( debug_level, "Statistics about job suspension:\n" );
	jobAd->LookupInteger( ATTR_TOTAL_SUSPENSIONS, total_suspensions );
	dprintf( debug_level, "%s = %d\n", ATTR_TOTAL_SUSPENSIONS, 
			 total_suspensions );

	jobAd->LookupInteger( ATTR_LAST_SUSPENSION_TIME,
						  last_suspension_time );
	dprintf( debug_level, "%s = %d\n", ATTR_LAST_SUSPENSION_TIME,
			 last_suspension_time );

	jobAd->LookupInteger( ATTR_CUMULATIVE_SUSPENSION_TIME, 
						  cumulative_suspension_time );
	dprintf( debug_level, "%s = %d\n",
			 ATTR_CUMULATIVE_SUSPENSION_TIME,
			 cumulative_suspension_time );
}


void
RemoteResource::resourceExit( int exit_reason, int exit_status )
{
	dprintf( D_FULLDEBUG, "Inside RemoteResource::resourceExit()\n" );
	setExitReason( exit_reason );
	setExitStatus( exit_status );
}


void 
RemoteResource::setResourceState( ResourceState s )
{
	shadow->dprintf( D_FULLDEBUG,
					 "Resource %s changing state from %s to %s\n",
					 machineName ? machineName : "???", 
					 rrStateToString(state), 
					 rrStateToString(s) );
	state = s;
}


const char*
rrStateToString( ResourceState s )
{
	if( s > _RR_STATE_THRESHOLD ) {
		return "Unknown State";
	}
	return Resource_State_String[s];
}
