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

/* This file is the implementation of the RemoteResource class. */

#include "condor_common.h"
#include "remoteresource.h"
#include "exit.h"             // for JOB_BLAH_BLAH exit reasons
#include "condor_debug.h"     // for D_debuglevel #defines
#include "condor_string.h"    // for strnewp()
#include "condor_attributes.h"
#include "internet.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dc_starter.h"

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
	"STARTUP",
	"RECONNECT",
};


RemoteResource::RemoteResource( BaseShadow *shad ) 
{
	shadow = shad;
	dc_startd = NULL;
	machineName = NULL;
	starterAddress = NULL;
	starterArch = NULL;
	starterOpsys = NULL;
	starter_version = NULL;
	jobAd = NULL;
	fs_domain = NULL;
	uid_domain = NULL;
	claim_sock = NULL;
	last_job_lease_renewal = 0;
	exit_reason = -1;
	exited_by_signal = false;
	exit_value = -1;
	memset( &remote_rusage, 0, sizeof(struct rusage) );
	disk_usage = 0;
	image_size = 0;
	state = RR_PRE;
	began_execution = false;
	supports_reconnect = false;
	next_reconnect_tid = -1;
	reconnect_attempts = 0;

	already_killed_graceful = false;
	already_killed_fast = false;
}


RemoteResource::~RemoteResource()
{
	if ( dc_startd     ) delete dc_startd;
	if ( machineName   ) delete [] machineName;
	if ( starterAddress) delete [] starterAddress;
	if ( starterArch   ) delete [] starterArch;
	if ( starterOpsys  ) delete [] starterOpsys;
	if ( starter_version ) delete [] starter_version;
	if ( uid_domain	   ) delete [] uid_domain;
	if ( fs_domain     ) delete [] fs_domain;
	if ( claim_sock    ) delete claim_sock;
	if ( jobAd         ) delete jobAd;
}


bool
RemoteResource::activateClaim( int starterVersion )
{
	int reply;
	const int max_retries = 20;
	const int retry_delay = 1;
	int num_retries = 0;

	if ( ! dc_startd ) {
		shadow->dprintf( D_ALWAYS, "Shadow doesn't have startd contact "
						 "information in RemoteResource::activateClaim()\n" ); 
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return false;
	}

	if ( !jobAd ) {
		shadow->dprintf( D_ALWAYS, "JobAd not defined in RemoteResource\n" );
		setExitReason(JOB_SHADOW_USAGE);  // no better exit reason available
		return false;
	}

		// we'll eventually return out of this loop...
	while( 1 ) {
		reply = dc_startd->activateClaim( jobAd, starterVersion,
										  &claim_sock );
		switch( reply ) {
		case OK:
			shadow->dprintf( D_ALWAYS, 
							 "Request to run on %s was ACCEPTED\n",
							 dc_startd->addr() );
				// first, set a timeout on the socket 
			claim_sock->timeout( 300 );
				// Now, register it for remote system calls.
				// It's a bit funky, but works.
			daemonCore->Register_Socket( claim_sock, "RSC Socket", 
				   (SocketHandlercpp)&RemoteResource::handleSysCalls, 
				   "HandleSyscalls", this );
			setResourceState( RR_STARTUP );		
			hadContact();
			return true;
			break;
		case CONDOR_TRY_AGAIN:
			shadow->dprintf( D_ALWAYS, 
							 "Request to run on %s was DELAYED\n",
							 dc_startd->addr() ); 
			num_retries++;
			if( num_retries > max_retries ) {
				dprintf( D_ALWAYS, "activateClaim(): Too many retries, "
						 "giving up.\n" );
				return false;
			}			  
			dprintf( D_FULLDEBUG, 
					 "activateClaim(): will try again in %d seconds\n",
					 retry_delay ); 
			sleep( retry_delay );
			break;

		case CONDOR_ERROR:
			shadow->dprintf( D_ALWAYS, "%s\n", dc_startd->error() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;

		case NOT_OK:
			shadow->dprintf( D_ALWAYS, 
							 "Request to run on %s was REFUSED\n",
							 dc_startd->addr() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;
		default:
			shadow->dprintf( D_ALWAYS, "Got unknown reply(%d) from "
							 "request to run on %s\n", reply,
							 dc_startd->addr() );
			setExitReason( JOB_NOT_STARTED );
			return false;
			break;
		}
	}
		// should never get here, but keep gcc happy and return
		// something. 
	return false;
}


bool
RemoteResource::killStarter( bool graceful )
{
	if( (graceful && already_killed_graceful) ||
		(!graceful && already_killed_fast) ) {
			// we've already sent this command to the startd.  we can
			// just return true right away
		return true;
	}
	if ( ! dc_startd ) {
		shadow->dprintf( D_ALWAYS, "RemoteResource::killStarter(): "
						 "DCStartd object NULL!\n");
		return false;
	}

	if( ! dc_startd->deactivateClaim(graceful) ) {
		shadow->dprintf( D_ALWAYS, "RemoteResource::killStarter(): "
						 "Could not send command to startd\n" );
		return false;
	}

	if( state != RR_FINISHED ) {
		setResourceState( RR_PENDING_DEATH );
	}

	if( graceful ) {
		already_killed_graceful = true;
	} else {
		already_killed_fast = true;
	}

	char* addr = dc_startd->addr();
	if( addr ) {
		dprintf( D_FULLDEBUG, "Killed starter (%s) at %s\n", 
				 graceful ? "graceful" : "fast", addr );
	}

	return true;
}


void
RemoteResource::dprintfSelf( int debugLevel )
{
	shadow->dprintf ( debugLevel, "RemoteResource::dprintSelf printing "
					  "host info:\n");
	if( dc_startd ) {
		char* addr = dc_startd->addr();
		char* id = dc_startd->getClaimId();
		shadow->dprintf( debugLevel, "\tstartdAddr: %s\n", 
						 addr ? addr : "Unknown" );
		shadow->dprintf( debugLevel, "\tClaimId: %s\n", 
						 id ? id : "Unknown" );
	}
	if( machineName ) {
		shadow->dprintf( debugLevel, "\tmachineName: %s\n",
						 machineName );
	}
	if( starterAddress ) {
		shadow->dprintf( debugLevel, "\tstarterAddr: %s\n", 
						 starterAddress );
	}
	shadow->dprintf( debugLevel, "\texit_reason: %d\n", exit_reason );
	shadow->dprintf( debugLevel, "\texited_by_signal: %s\n", 
					 exited_by_signal ? "True" : "False" );
	if( exited_by_signal ) {
		shadow->dprintf( debugLevel, "\texit_signal: %d\n", 
						 exit_value );
	} else {
		shadow->dprintf( debugLevel, "\texit_code: %d\n", 
						 exit_value );
	}
}


int
RemoteResource::handleSysCalls( Stream *sock )
{

		// change value of the syscall_sock to correspond with that of
		// this claim sock right before do_REMOTE_syscall().

	syscall_sock = claim_sock;
	thisRemoteResource = this;

	if (do_REMOTE_syscall() < 0) {
		shadow->dprintf(D_SYSCALLS,"Shadow: do_REMOTE_syscall returned < 0\n");
			// we call our shadow's shutdown method:
		shadow->shutDown( exit_reason );
			// close sock on this end...the starter has gone away.
		return TRUE;
	}
	hadContact();
	return KEEP_STREAM;
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
RemoteResource::getStartdAddress( char *& sinful )
{
	if( sinful ) {
		sinful[0] = '\0';
	}
	if( ! dc_startd ) {
		return;
	}
	char* addr = dc_startd->addr();
	if( ! addr ) {
		return;
	}
	if( ! sinful ) {
		sinful = strnewp( addr );
	} else {
		strcpy( sinful, addr );
	}
}


void
RemoteResource::getClaimId( char *& id )
{
	if( id ) {
		id[0] = '\0';
	}
	if( ! dc_startd ) {
		return;
	}
	char* my_id = dc_startd->getClaimId();
	if( ! my_id ) {
		return;
	}
	if( ! id ) {
		id = strnewp( my_id );
	} else {
		strcpy( id, my_id );
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
	return claim_sock;
}


void
RemoteResource::closeClaimSock( void )
{
	if( claim_sock ) {
		daemonCore->Cancel_Socket( claim_sock );
		delete claim_sock;
		claim_sock = NULL;
	}
}


int
RemoteResource::getExitReason()
{
	return exit_reason;
}


int
RemoteResource::exitSignal( void )
{
	if( exited_by_signal ) {
		return exit_value;
	}
	return -1;
}


int
RemoteResource::exitCode( void )
{
	if( ! exited_by_signal ) {
		return exit_value;
	}
	return -1;
}


void
RemoteResource::setStartdInfo( ClassAd* ad )
{
	char* name = NULL;
	ad->LookupString( ATTR_REMOTE_HOST, &name );
	if( ! name ) {
		ad->LookupString( ATTR_NAME, &name );
		if( ! name ) {
			EXCEPT( "ad includes neither %s nor %s!", ATTR_NAME,
					ATTR_REMOTE_HOST );
		}
	}

	char* pool = NULL;
	ad->LookupString( ATTR_REMOTE_POOL, &pool );
		// we don't care if there's no pool specified...

	char* claim_id = NULL;
	ad->LookupString( ATTR_CLAIM_ID, &claim_id );
	if( ! claim_id ) {
		EXCEPT( "ad does not include %s!", ATTR_CLAIM_ID );
	}

	char* addr = getAddrFromClaimId( claim_id );
	if( ! addr ) {
		EXCEPT( "invalid %s in ad (%s)", ATTR_CLAIM_ID, claim_id );
	}

	initStartdInfo( name, pool, addr, claim_id );
}


void
RemoteResource::setStartdInfo( const char *sinful, 
							   const char* claim_id ) 
{
	initStartdInfo( sinful, NULL, NULL, claim_id );
}


void
RemoteResource::initStartdInfo( const char *name, const char *pool,
								const char *addr, const char* claim_id )
{
	dprintf( D_FULLDEBUG, "in RemoteResource::initStartdInfo()\n" );  

	if( dc_startd ) {
		delete dc_startd;
	}
	dc_startd = new DCStartd( name, pool, addr, claim_id );

	if( name ) {
		setMachineName( name );
	} else if( addr ) {
		setMachineName( addr );
	} else {
		EXCEPT( "in RemoteResource::setStartdInfo() without name or addr" );
	}

		/*
		  Tell daemonCore that we're willing to grant DAEMON
		  permission to whatever machine we are claiming.
		  This greatly simplifies DaemonCore permission stuff
		  for flocking, since submitters don't have to know all the
		  hosts they might possibly run on, all they have to do is
		  trust the central managers of all the pools they're flocking
		  to (which they have to do, already).  
		  Added on 3/15/01 by Todd Tannenbaum <tannenba@cs.wisc.edu>
		*/
	char *ip = string_to_ipstr( dc_startd->addr() );
	if( ip ) {
		daemonCore->AddAllowHost( ip, WRITE );
		daemonCore->AddAllowHost( ip, DAEMON );
	} else {
		dprintf( D_ALWAYS, "ERROR: Can't convert \"%s\" to an IP address!\n", 
				 dc_startd->addr() );
	}
}


void
RemoteResource::setStarterInfo( ClassAd* ad )
{

	char* tmp = NULL;

	if( ad->LookupString(ATTR_STARTER_IP_ADDR, &tmp) ) {
		setStarterAddress( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_STARTER_IP_ADDR, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_UID_DOMAIN, &tmp) ) {
		if( uid_domain ) {
			delete [] uid_domain;
		}	
		uid_domain = strnewp( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_UID_DOMAIN, tmp );
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &tmp) ) {
		if( fs_domain ) {
			delete [] fs_domain;
		}	
		fs_domain = strnewp( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_FILE_SYSTEM_DOMAIN,
				 tmp );  
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_NAME, &tmp) ) {
		if( machineName ) {
			if( is_valid_sinful(machineName) ) {
				delete [] machineName;
				machineName = strnewp( tmp );
			}
		}	
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_MACHINE, tmp );
		free( tmp );
		tmp = NULL;
	} else if( ad->LookupString(ATTR_MACHINE, &tmp) ) {
		if( machineName ) {
			if( is_valid_sinful(machineName) ) {
				delete [] machineName;
				machineName = strnewp( tmp );
			}
		}	
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_MACHINE, tmp );
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_ARCH, &tmp) ) {
		if( starterArch ) {
			delete [] starterArch;
		}	
		starterArch = strnewp( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_ARCH, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_OPSYS, &tmp) ) {
		if( starterOpsys ) {
			delete [] starterOpsys;
		}	
		starterOpsys = strnewp( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_OPSYS, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_VERSION, &tmp) ) {
		if( starter_version ) {
			delete [] starter_version;
		}	
		starter_version = strnewp( tmp );
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_VERSION, tmp ); 
		free( tmp );
		tmp = NULL;
	}

	int tmp_int;
	if( ad->LookupBool(ATTR_HAS_RECONNECT, tmp_int) ) {
			// Whatever the starter defines in its own classad
			// overrides whatever we might think...
		supports_reconnect = tmp_int;
		dprintf( D_SYSCALLS, "  %s = %s\n", ATTR_HAS_RECONNECT, 
				 supports_reconnect ? "TRUE" : "FALSE" );
	} else {
		dprintf( D_SYSCALLS, "  %s = FALSE (not specified)\n",
				 ATTR_HAS_RECONNECT );
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
RemoteResource::setStarterAddress( const char * starterAddr )
{
	if( starterAddress ) {
		delete [] starterAddress;
	}	
	starterAddress = strnewp( starterAddr );
}


void
RemoteResource::setExitReason( int reason )
{
	// Set the exit_reason, but not if the reason is JOB_KILLED.
	// This prevents exit_reason being reset from JOB_KILLED to
	// JOB_NOT_CKPTED or some such when the starter gets killed
	// and the syscall sock goes away.

	shadow->dprintf( D_FULLDEBUG, "setting exit reason on %s to %d\n", 
					 machineName ? machineName : "???", reason ); 

	if( exit_reason != JOB_KILLED ) {
		exit_reason = reason;
	}

		// record that this resource is really finished
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
RemoteResource::setJobAd( ClassAd *jA )
{
	this->jobAd = jA;

		// now, see if anything we care about is defined in the ad.
		// this prevents us (for example) from logging another
		// ImageSizeEvent everytime we start running, even if the
		// image size hasn't changed at all...

	int int_value;
	float float_value;

	if( jA->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, float_value) ) {
		remote_rusage.ru_stime.tv_sec = (int) float_value; 
	}
			
	if( jA->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, float_value) ) {
		remote_rusage.ru_utime.tv_sec = (int) float_value; 
	}

	if( jA->LookupInteger(ATTR_IMAGE_SIZE, int_value) ) {
		image_size = int_value;
	}
			
	if( jA->LookupInteger(ATTR_DISK_USAGE, int_value) ) {
		disk_usage = int_value;
	}

	if( jA->LookupInteger(ATTR_LAST_JOB_LEASE_RENEWAL, int_value) ) {
		last_job_lease_renewal = (time_t)int_value;
	}
}


void
RemoteResource::updateFromStarter( ClassAd* update_ad )
{
	int int_value;
	float float_value;
	char string_value[ATTRLIST_MAX_EXPRESSION];
	char tmp[ATTRLIST_MAX_EXPRESSION];

	dprintf( D_FULLDEBUG, "Inside RemoteResource::updateFromStarter()\n" );
	hadContact();

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

	if( update_ad->LookupString(ATTR_EXCEPTION_HIERARCHY,string_value) ) {
		sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_HIERARCHY,string_value);
		jobAd->Insert( tmp );
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_NAME,string_value) ) {
		sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_NAME,string_value);
		jobAd->Insert( tmp );
	}

	if( update_ad->LookupString(ATTR_EXCEPTION_TYPE,string_value) ) {
		sprintf(tmp,"%s=\"%s\"",ATTR_EXCEPTION_TYPE,string_value);
		jobAd->Insert( tmp );
	}

	if( update_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_value) ) {
		if( int_value ) {
			sprintf( tmp, "%s=TRUE", ATTR_ON_EXIT_BY_SIGNAL );
			exited_by_signal = true;
		} else {
			sprintf( tmp, "%s=FALSE", ATTR_ON_EXIT_BY_SIGNAL );
			exited_by_signal = false;
		}
		jobAd->Insert( tmp );
	}

	if( update_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_value) ) {
		sprintf( tmp, "%s=%d", ATTR_ON_EXIT_SIGNAL, int_value );
		exit_value = int_value;
		jobAd->Insert( tmp );
	}

	if( update_ad->LookupInteger(ATTR_ON_EXIT_CODE, int_value) ) {
		sprintf( tmp, "%s=%d", ATTR_ON_EXIT_CODE, int_value );
		exit_value = int_value;
		jobAd->Insert( tmp );
	}

	if( update_ad->LookupString(ATTR_EXIT_REASON,string_value) ) {
		sprintf( tmp, "%s=\"%s\"", ATTR_EXIT_REASON, string_value );
		jobAd->Insert( tmp );
	}

	if( update_ad->LookupBool(ATTR_JOB_CORE_DUMPED, int_value) ) {
		if( int_value ) {
			sprintf( tmp, "%s=TRUE", ATTR_JOB_CORE_DUMPED );
		} else {
			sprintf( tmp, "%s=FALSE", ATTR_JOB_CORE_DUMPED );
		}
		jobAd->Insert( tmp );
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
		free( job_state );
		if( state == RR_PENDING_DEATH ) {
				// we're trying to shutdown, so don't bother recording
				// what we just heard from the starter.  we're done
				// dealing with this update.
			return;
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
	}
		// now that we've gotten an update, we should evaluate our
		// periodic user policy again, since we have new information
		// and something might now evaluate to true which we should
		// act on.
	shadow->evalPeriodicUserPolicy();
}


bool
RemoteResource::recordSuspendEvent( ClassAd* update_ad )
{
	char *rt = NULL;
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

	/* If we have been asked to record the state of these suspend/resume
		events in realtime, then update the schedd right now */
	rt = param( "REAL_TIME_JOB_SUSPEND_UPDATES" );
	if( rt != NULL ) {
		if (strcasecmp(rt, "true") == MATCH) {
			shadow->updateJobInQueue(U_PERIODIC);
		}
		free(rt);
		rt = NULL;
	}
	
	return rval;
}


bool
RemoteResource::recordResumeEvent( ClassAd* update_ad )
{
	char *rt = NULL;
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

	/* If we have been asked to record the state of these suspend/resume
		events in realtime, then update the schedd right now */
	rt = param( "REAL_TIME_JOB_SUSPEND_UPDATES" );
	if( rt != NULL ) {
		if (strcasecmp(rt, "true") == MATCH) {
			shadow->updateJobInQueue(U_PERIODIC);
		}
		free(rt);
		rt = NULL;
	}

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

	if( exit_value == -1 ) {
			/* 
			   Backwards compatibility code...  If we don't have a
			   real value for exit_value yet, it means the starter
			   we're talking to doesn't parse the status integer
			   itself and set the various ClassAd attributes
			   appropriately.  To prevent any trouble in this case, we
			   do the parsing here so everything in the shadow can
			   still work.  Doing this ourselves is potentially
			   inaccurate, since the starter might be a different
			   platform and we might get different results, but it's
			   better than nothing.  However, this is what we've
			   always done in the past, so it's no less accurate than
			   an old shadow talking to the same starter...
			*/
		char tmp[64];
		if( WIFSIGNALED(exit_status) ) {
			exited_by_signal = true;
			sprintf( tmp, "%s=TRUE", ATTR_ON_EXIT_BY_SIGNAL );
			jobAd->Insert( tmp );

			exit_value = WTERMSIG( exit_status );
			sprintf( tmp, "%s=%d", ATTR_ON_EXIT_SIGNAL, exit_value );
			jobAd->Insert( tmp );

		} else {
			exited_by_signal = false;
			sprintf( tmp, "%s=FALSE", ATTR_ON_EXIT_BY_SIGNAL );
			jobAd->Insert( tmp );

			exit_value = WEXITSTATUS( exit_status );
			sprintf( tmp, "%s=%d", ATTR_ON_EXIT_CODE, exit_value );
			jobAd->Insert( tmp );
		}
	}
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


void 
RemoteResource::beginExecution( void )
{
	if( began_execution ) {
			// Only call this function once per remote resource
		return;
	}

	began_execution = true;
	setResourceState( RR_EXECUTING );

		// Let our shadow know so it can make global decisions (for
		// example, should it log a JOB_EXECUTE event)
	shadow->resourceBeganExecution( this );
}

void
RemoteResource::hadContact( void )
{
		// Length: ATTR_LAST_JOB_LEASE_RENEWAL is 19, '=' is 1,
		// MAX_INT is 10, and another 10 to spare should plenty... 
	char contact_buf[40];
	last_job_lease_renewal = time(0);
	snprintf( contact_buf, 32, "%s=%d", ATTR_LAST_JOB_LEASE_RENEWAL,
			  (int)last_job_lease_renewal );
	jobAd->Insert( contact_buf );
}


bool
RemoteResource::supportsReconnect( void )
{
		// even if the starter we're talking to supports reconnect, we
		// only want to return true if the job we're running supports
		// it too (i.e. has a GlobalJobId and a JobLeaseDuration).
	const char* gjid = shadow->getGlobalJobId();
	if( ! gjid ) {
		return false;
	}
	int tmp;
	if( ! jobAd->LookupInteger(ATTR_JOB_LEASE_DURATION, tmp) ) {
		return false;
	}

		// if we got this far, the job supports it, so we can just
		// return whether the remote resource does or not.
	return supports_reconnect;
}


void
RemoteResource::reconnect( void )
{
	static int lease_duration = -1;
	const char* gjid = shadow->getGlobalJobId();
	if( ! gjid ) {
		EXCEPT( "Shadow in reconnect mode but %s is not in the job ad!",
				ATTR_GLOBAL_JOB_ID );
	}
	if( lease_duration < 0 ) { 
			// if it's our first time, figure out what we've got to
			// work with...
		dprintf( D_FULLDEBUG, "Trying to reconnect job %s\n", gjid );
		if( ! jobAd->LookupInteger(ATTR_JOB_LEASE_DURATION,
								   lease_duration) ) {
			EXCEPT( "Shadow in reconnect mode but %s is not in the job ad!",
					ATTR_JOB_LEASE_DURATION );
		}
		if( ! last_job_lease_renewal ) {
				// if we were spawned in reconnect mode, this should
				// be set.  if we're just trying a reconnect because
				// the syscall socket went away, we'll already have
				// initialized last_job_lease_renewal when we started
				// the job
			EXCEPT( "Shadow in reconnect mode but %s is not in the job ad!",
					ATTR_LAST_JOB_LEASE_RENEWAL );
		}
		dprintf( D_ALWAYS, "Trying to reconnect to disconnected job\n" );
		dprintf( D_ALWAYS, "%s: %d %s", ATTR_LAST_JOB_LEASE_RENEWAL,
				 (int)last_job_lease_renewal, 
				 ctime(&last_job_lease_renewal) );
		dprintf( D_ALWAYS, "%s: %d seconds\n",
				 ATTR_JOB_LEASE_DURATION, lease_duration );
	}

		// If we got here, we're trying to reconnect.  keep track of
		// that since we need to know in certain situations...
	if( state != RR_RECONNECT ) {
		setResourceState( RR_RECONNECT );
	}

		// each time we get here, see how much time remains...
	time_t now = time(0);
	int remaining = lease_duration - (now - last_job_lease_renewal);
	if( remaining <= 0 ) {
	dprintf( D_ALWAYS, "%s remaining: EXPIRED!\n",
			 ATTR_JOB_LEASE_DURATION );
		MyString reason = "Job disconnected too long: ";
		reason += ATTR_JOB_LEASE_DURATION;
		reason += " (";
		reason += lease_duration;
		reason += " seconds) expired";
		shadow->reconnectFailed( reason.Value() );
	}
	dprintf( D_ALWAYS, "%s remaining: %d\n", ATTR_JOB_LEASE_DURATION,
			 remaining );

	if( next_reconnect_tid >= 0 ) {
		EXCEPT( "in reconnect() and timer for next attempt already set" );
	}

    int delay = shadow->nextReconnectDelay( reconnect_attempts );
	if( delay > remaining ) {
		delay = remaining;
	}
	if( delay ) {
			// only need to dprintf if we're not doing it right away
		dprintf( D_ALWAYS, "Scheduling another attempt to reconnect "
				 "in %d seconds\n", delay );
	}
	next_reconnect_tid = daemonCore->
		Register_Timer( delay,
						(TimerHandlercpp)&RemoteResource::attemptReconnect,
						"RemoteResource::attemptReconnect()", this );

	if( next_reconnect_tid < 0 ) {
		EXCEPT( "Failed to register timer!" );
	}
}


void
RemoteResource::attemptReconnect( void )
{
		// now that the timer went off, clear out this variable so we
		// don't get confused later.
	next_reconnect_tid = -1;

		// if if this attempt fails, we need to remember we tried
	reconnect_attempts++;

	if( ! starterAddress ) {
		if( ! locateReconnectStarter() ) {
			return;
		}
	}
		// if we got here, we already know the starter's address, so
		// we can go on to directly request a reconnect... 
	requestReconnect(); 
}


bool
RemoteResource::locateReconnectStarter( void )
{
	dprintf( D_ALWAYS, "Attempting to locate disconnected starter\n" );
	const char* gjid = shadow->getGlobalJobId();
	ClassAd reply;
	if( dc_startd->locateStarter(gjid, &reply, 20) ) {
			// it worked, save the results and return success.
		char* tmp = NULL;
		if( reply.LookupString(ATTR_STARTER_IP_ADDR, &tmp) ) {
			setStarterAddress( tmp );
			dprintf( D_ALWAYS, "Found starter: %s\n", tmp );
			free( tmp );
			return true;
		} else {
			EXCEPT( "impossible: locateStarter() returned success "
					"but %s not found", ATTR_STARTER_IP_ADDR );
		}
	}
	
		// if we made it here figure out what kind of error we got and
		// act accordingly.  in all cases we want to either exit
		// completely or return false so that attemptReconnect() just
		// returns instead of calling requestReconnect().

	dprintf( D_ALWAYS, "locateStarter(): %s\n", dc_startd->error() );

	switch( dc_startd->errorCode() ) {

	case CA_FAILURE:

			// communication successful but GlobalJobId or starter not
			// found.  either way, we know the job is gone, and can
			// safely give up and restart.
		shadow->reconnectFailed( "Job not found at execution machine" );
		break;

	case CA_NOT_AUTHENTICATED:
			// some condor daemon is listening on the port, but it
			// doesn't believe us anymore, so it can't still be our
			// old startd. :( if our job was still there, the old
			// startd would be willing to talk to us.  Just to be
			// safe, try one last time to see if we can kill the old
			// starter.  We don't want the schedd to try this, since
			// it'd block, but we don't have anything better to do,
			// and it helps ensure run-only-once semantics for jobs.
		shadow->cleanUp();
		shadow->reconnectFailed( "Startd is no longer at old port, "
								 "job must have been killed" );
		break;

	case CA_CONNECT_FAILED:
	case CA_COMMUNICATION_ERROR:
			// for both of these, we need to keep trying until the
			// lease_duration expires, since the startd might still be alive
			// and only the network is dead...
		reconnect();
		break;

			// All the errors that can only be programmer mistakes:
			// starter should never return any of these...
	case CA_NOT_AUTHORIZED:
	case CA_INVALID_STATE:
	case CA_INVALID_REQUEST:
	case CA_INVALID_REPLY:
		EXCEPT( "impossible: startd returned %s for locateStarter",
				getCAResultString(dc_startd->errorCode()) );
		break;
	case CA_LOCATE_FAILED:
			// remember, this means we couldn't even find the address
			// of the startd, not the starter.  we already know the
			// startd's addr from the ClaimId...
		EXCEPT( "impossible: startd address already known" );
		break;
	case CA_SUCCESS:
		EXCEPT( "impossible: success already handled" );
		break;
	}
	return false;
}


void
RemoteResource::requestReconnect( void )
{
	DCStarter starter( starterAddress );

	dprintf( D_ALWAYS, "Attempting to reconnect to starter %s\n", 
			 starterAddress );
		// We want this on the heap, since if this works, we're going
		// to hold onto it and don't want it on the stack...
	ReliSock* rsock = new ReliSock;
		// we don't want to block forever trying to connect and
		// reestablish contact.  We'll retry if we have to.
	rsock->timeout(30);
	ClassAd req;
	ClassAd reply;
	MyString msg;

		// First, fill up the request with all the data we need
	shadow->publishShadowAttrs( &req );

		// And also put in the request the ATTR_TRANS_SOCK and
		// the ATTR_TRANS_KEY --- the starter will need these two
		// atttribute value in order to re-establish the FileTransfer
		// objects.  To get the values for these, just instantiate a 
		// FileTransfer server object right here.  No worries if
		// one already exists, the FileTransfer object will just
		// quickly and quietly return success in that case.
	ASSERT(jobAd);
	filetrans.Init( jobAd, true, PRIV_USER );
	char* value = NULL;
	jobAd->LookupString(ATTR_TRANSFER_KEY,&value);
	if (value) {
		msg.sprintf("%s=\"%s\"",ATTR_TRANSFER_KEY,value);
		req.Insert(msg.Value());
		free(value);
		value = NULL;
	} else {
		dprintf( D_ALWAYS,"requestReconnect(): failed to determine %s\n",
			ATTR_TRANSFER_KEY );
	}
	jobAd->LookupString(ATTR_TRANSFER_SOCKET,&value);
	if (value) {
		msg.sprintf("%s=\"%s\"",ATTR_TRANSFER_SOCKET,value);
		req.Insert(msg.Value());
		free(value);
		value = NULL;
	} else {
		dprintf( D_ALWAYS,"requestReconnect(): failed to determine %s\n",
			ATTR_TRANSFER_SOCKET );
	}


		// try the command itself...
	if( ! starter.reconnect(&req, &reply, rsock) ) {
		dprintf( D_ALWAYS, "Attempt to reconnect failed: %s\n", 
				 starter.error() );
		delete rsock;
		switch( starter.errorCode() ) {
		case CA_CONNECT_FAILED:
		case CA_COMMUNICATION_ERROR:
			// for both of these, we need to keep trying until the
			// lease_duration expires, since the starter might still be alive
			// and only the network is dead...
			reconnect();
			return;  // reconnect will return right away, and we want
					 // to hand control back to DaemonCore ASAP
			break;

		case CA_NOT_AUTHORIZED:
		case CA_NOT_AUTHENTICATED:
				/*
				  Somehow we authenticated improperly and the starter
				  doesn't think we own our job anymore. :( Trying
				  again won't help us at this point...  Normally this
				  would never happen.  However, if it does, all we can
				  do is try to kill the job (maybe the startd will
				  trust us *grin*), and return failure.
				*/
			shadow->cleanUp();
			msg = "Starter refused request (";
			msg += starter.error();
			msg += ')';
			shadow->reconnectFailed( msg.Value() );
			break;

				// All the errors that can only be programmer
				// mistakes: the starter should never return them...  
		case CA_FAILURE:
		case CA_INVALID_STATE:
		case CA_INVALID_REQUEST:
		case CA_INVALID_REPLY:
			EXCEPT( "impossible: starter returned %s for %s",
					getCAResultString(dc_startd->errorCode()),
					getCommandString(CA_RECONNECT_JOB) );
			break;
		case CA_LOCATE_FAILED:
				// we couldn't even find the address of the starter, but
				// we already know it or we wouldn't be trying this
				// method...
			EXCEPT( "impossible: starter address already known" );
			break;
		case CA_SUCCESS:
			EXCEPT( "impossible: success already handled" );
			break;
		}
	}

	dprintf( D_ALWAYS, "Reconnect SUCCESS: connection re-established\n" );

	dprintf( D_FULLDEBUG, "Registering socket for future syscalls\n" );
	if( claim_sock ) {
		dprintf( D_FULLDEBUG, "About to cancel old claim_sock\n" );
		daemonCore->Cancel_Socket( claim_sock );
		delete claim_sock;
	}
	claim_sock = rsock;
	claim_sock->timeout( 300 );
	daemonCore->Register_Socket( claim_sock, "RSC Socket", 
				   (SocketHandlercpp)&RemoteResource::handleSysCalls, 
				   "HandleSyscalls", this );

		// Read all the info out of the reply ad and stash it in our
		// private data members, just like we do when we get the
		// pseudo syscall on job startup for the starter to register
		// this stuff about itself. 
	setStarterInfo( &reply );

	began_execution = true;
	setResourceState( RR_EXECUTING );
	reconnect_attempts = 0;
	hadContact();

		// Tell the Shadow object so it can take care of the rest.
	shadow->resourceReconnected( this );

		// that's it, we're done!  we can now return to DaemonCore and
		// wait to service requests on the syscall or command socket
	return;
}
