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


#include "condor_common.h"
#include "baseshadow.h"
#include "condor_classad.h"      // for ClassAds.
#include "condor_qmgr.h"         // have to talk to schedd's Q manager
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_config.h"       // for param()
#include "condor_email.h"        // for (you guessed it) email stuff

// these are declared static in baseshadow.h; allocate space here
UserLog BaseShadow::uLog;
BaseShadow* BaseShadow::myshadow_ptr = NULL;


// this appears at the bottom of this file:
extern "C" int display_dprintf_header(FILE *fp);

BaseShadow::BaseShadow() {
	spool = NULL;
	fsDomain = uidDomain = NULL;
	ckptServerHost = NULL;
	useAFS = useNFS = useCkptServer = false;
	jobAd = NULL;
	cluster = proc = -1;
	owner[0] = '\0';
	iwd[0] = '\0';
	scheddAddr = NULL;
	ASSERT( !myshadow_ptr );	// make cetain we're only instantiated once
	myshadow_ptr = this;
}

BaseShadow::~BaseShadow() {
	if (spool) free(spool);
	if (fsDomain) free(fsDomain);
	if (ckptServerHost) free(ckptServerHost);
	if (jobAd) FreeJobAd(jobAd);
}

void BaseShadow::baseInit( ClassAd *jobAd, char schedd_addr[], 
				  char cluster[], char proc[])
{
	if (schedd_addr[0] != '<') {
		EXCEPT("Schedd_Addr not specified with sinful string.");
	}
	scheddAddr = schedd_addr;
	this->cluster = atoi(cluster);
	this->proc = atoi(proc);

	if ( !jobAd->LookupString(ATTR_OWNER, owner)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_OWNER);
	}
	if ( !jobAd->LookupString(ATTR_JOB_IWD, iwd)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_JOB_IWD);
	}

        // put the shadow's sinful string into the jobAd.  Helpful for
        // the mpi shadow, at least...and a good idea in general.
	char buf[256];
    sprintf ( buf, "%s = \"%s\"", ATTR_MY_ADDRESS,
              daemonCore->InfoCommandSinfulString() );
    if ( !jobAd->Insert( buf )) {
        EXCEPT( "Failed to insert %s!", ATTR_MY_ADDRESS );
    }

	this->jobAd = jobAd;
	
	DebugId = display_dprintf_header;
	
	config();

	initUserLog();

		// register SIGUSR1 (condor_rm) for shutdown...
	daemonCore->Register_Signal(DC_SIGUSR1, "DC_SIGUSR1", 
		(SignalHandlercpp)&BaseShadow::handleJobRemoval, "HandleJobRemoval", 
		this, IMMEDIATE_FAMILY);

	// handle system calls with Owner's privilege
// XXX this belong here?  We'll see...
	init_user_ids(owner);
	set_user_priv();
	daemonCore->Register_Priv_State( PRIV_USER );

		// change directory, send mail if failure:
	if ( cdToIwd() == -1 ) {
		EXCEPT("Could not cd to initial working directory");
	}

	dumpClassad( "BaseShadow::baseInit()", this->jobAd, D_JOB );
}

void BaseShadow::config()
{
	char *tmp;

	if (spool) free(spool);
	spool = param("SPOOL");
	if (!spool) {
		EXCEPT("SPOOL not specified in config file.");
	}

	if (fsDomain) free(fsDomain);
	fsDomain = param("FILESYSTEM_DOMAIN");
	if (!fsDomain) {
		EXCEPT("FILESYSTEM_DOMAIN not specified in config file.");
	}

	if (uidDomain) free(uidDomain);
	uidDomain = param("UID_DOMAIN");
	if (!uidDomain) {
		EXCEPT("UID_DOMAIN not specified in config file.");
	}

	tmp = param("USE_AFS");
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		useAFS = true;
	} else {
		useAFS = false;
	}
	if (tmp) free(tmp);

	tmp = param("USE_NFS");
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		useNFS = true;
	} else {
		useNFS = false;
	}
	if (tmp) free(tmp);

	if (ckptServerHost) free(ckptServerHost);
	ckptServerHost = param("CKPT_SERVER_HOST");
	tmp = param("USE_CKPT_SERVER");
	if (tmp && ckptServerHost && (tmp[0] == 'T' || tmp[0] == 't')) {
		useCkptServer = true;
	} else {
		useCkptServer = false;
	}
	if (tmp) free(tmp);
}

int BaseShadow::cdToIwd() {
	if (chdir(iwd) < 0) {
		dprintf(D_ALWAYS, "\n\nPath does not exist.\n"
				"He who travels without bounds\n"
				"Can't locate data.\n\n" );
		dprintf( D_ALWAYS, "(Can't chdir to %s)\n", iwd);
		char *buf = new char [strlen(iwd)+20];
		sprintf(buf, "Can't access \"%s\".", iwd);
		FILE *mailer = NULL;
		if ( (mailer=emailUser(buf)) ) {
			fprintf(mailer,"Your job %d.%d specified an initial working\n",
				getCluster(),getProc());
			fprintf(mailer,"directory of %s.\nThis directory currently does\n",
				iwd);
			fprintf(mailer,"not exist.  If this directory is on a shared\n"
				"filesystem, this could be just a temporary problem.  Thus\n"
				"I will try again later\n");
			email_close(mailer);
		}
		delete buf;
		return -1;
	}
	return 0;
}

FILE* BaseShadow::emailUser(char *subjectline)
{
	char email_addr[256];

	dprintf(D_FULLDEBUG, "BaseShadow::emailUser() called.\n");

	if (!jobAd ) {
		return NULL;
	}
	
	/*
	** The job may have an email address to whom the notification message
    ** should go.  this info is in the classad.
    */
	email_addr[0] = '\0';
	if ( (!jobAd->LookupString(ATTR_NOTIFY_USER, email_addr)) ||
		 (email_addr[0] == '\0') ) {
		// no email address specified in the job ad; use owner
		strcpy(email_addr, owner);
	}

	if ( strchr(email_addr,'@') == NULL )
	{
		// No host name specified; add uid domain. 
		// Note: UID_DOMAIN is set to the fullhostname by default.
		strcat(email_addr,"@");
		strcat(email_addr,uidDomain);
	}

	return email_open(email_addr,subjectline);
}

FILE* BaseShadow::shutDownEmail(int reason, int exitStatus) 
{

		// everything else we do only makes sense if there is a JobAd, 
		// so bail out now if there is no JobAd.
	if ( !jobAd ) {
		return NULL;
	}

	// send email if user requested it
	int notification = NOTIFY_COMPLETE;	// default
	jobAd->LookupInteger(ATTR_JOB_NOTIFICATION,notification);
	int send_email = TRUE;
	switch( notification ) {
		case NOTIFY_NEVER:
			send_email = FALSE;
			break;
		case NOTIFY_ALWAYS:
			break;
		case NOTIFY_COMPLETE:
			if( reason != JOB_EXITED ) {
				send_email = FALSE;
			}
			break;
		case NOTIFY_ERROR:
			// do not send email if the job has not exited yet, or
			// if the job exited with something other than a signal.
			if( (reason != JOB_EXITED) || 
				((reason == JOB_EXITED) && 
				 (WIFEXITED(exitStatus))) ) {
                send_email = FALSE;
			}
			break;
		default:
			dprintf(D_ALWAYS, 
				"Condor Job %d.%d has unrecognized notification of %d\n",
				cluster, proc, notification );
	}

		// return the mailer 
	if ( send_email ) {
		FILE* mailer;
		char buf[50];

		sprintf(buf,"Job %d.%d",cluster,proc);
		if ( (mailer=emailUser(buf)) ) {
			return mailer;
		}
	}

	return NULL;
}


void BaseShadow::initUserLog()
{
	char tmp[_POSIX_PATH_MAX], logfilename[_POSIX_PATH_MAX];
	if (jobAd->LookupString(ATTR_ULOG_FILE, tmp) == 1) {
		if ( tmp[0] == '/' || tmp[0]=='\\' || (tmp[1]==':' && tmp[2]=='\\') ) {
			strcpy(logfilename, tmp);
		} else {
			sprintf(logfilename, "%s/%s", iwd, tmp);
		}
		uLog.initialize (owner, logfilename, cluster, proc, 0);
		dprintf(D_FULLDEBUG, "%s = %s\n", ATTR_ULOG_FILE, logfilename);
	} else {
		dprintf(D_FULLDEBUG, "no %s found\n", ATTR_ULOG_FILE);
	}
}

void
BaseShadow::endingUserLog( int exitStatus, int exitReason )
{
	struct rusage run_remote_rusage;
	memset( &run_remote_rusage, 0, sizeof(struct rusage) );

	run_remote_rusage = getRUsage();

	switch( exitReason ) {

		case JOB_CKPTED:
		case JOB_NOT_CKPTED:
			// A vacate was performed on the resource, and the job
			// was thrown off either with or without a checkpoint.
			{
			JobEvictedEvent event;
			event.checkpointed = (exitReason == JOB_CKPTED);
			
			// TODO: fill in local rusage
			// event.run_local_rusage = ???
			
			// remote rusage
			event.run_remote_rusage = run_remote_rusage;
			
			
				// we want to log the events from the perspective of the
				// user job, so if the shadow *sent* the bytes, then that
				// means the user job *received* the bytes
			event.recvd_bytes = bytesSent();
			event.sent_bytes = bytesReceived();

			if (!uLog.writeEvent (&event)) {
				dprintf (D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n");
			}
			}
			break;

		case JOB_EXITED:	
			// Job exited on its own, normally or abnormally
			{
			JobTerminatedEvent event;
			if ( (event.normal = (WIFEXITED(exitStatus)!=0)) ) {
				event.returnValue = WEXITSTATUS(exitStatus);
			} else {
				event.signalNumber = WTERMSIG(exitStatus);
			}
			
			// TODO: fill in local/total rusage
			// event.run_local_rusage = r;
			event.run_remote_rusage = run_remote_rusage;
			// event.total_local_rusage = r;
			event.total_remote_rusage = run_remote_rusage;

			/* we want to log the events from the perspective 
			   of the user job, so if the shadow *sent* the 
			   bytes, then that means the user job *received* 
			   the bytes */
			event.recvd_bytes = bytesSent();
			event.sent_bytes = bytesReceived();
			// TODO: total sent and recvd
			event.total_recvd_bytes = bytesSent();
			event.total_sent_bytes = bytesReceived();
			
			if (!uLog.writeEvent (&event)) {
				dprintf (D_ALWAYS,"Unable to log "
						 "ULOG_JOB_TERMINATED event\n");
			}
			}
			break;	

	}	// end of switch
}

// Note: log_except is static
void
BaseShadow::log_except(char *msg)
{
	// log shadow exception event
	ShadowExceptionEvent event;
	sprintf(event.message, msg);

	
	if ( BaseShadow::myshadow_ptr ) {
		// we want to log the events from the perspective of the
		// user job, so if the shadow *sent* the bytes, then that
		// means the user job *received* the bytes
		event.recvd_bytes = BaseShadow::myshadow_ptr->bytesSent();
		event.sent_bytes = BaseShadow::myshadow_ptr->bytesReceived();
	} else {
		event.recvd_bytes = 0.0;
		event.sent_bytes = 0.0;
	}

	if (!uLog.writeEvent (&event))
	{
		::dprintf (D_ALWAYS, "Unable to log ULOG_SHADOW_EXCEPTION event\n");
	}
}

void BaseShadow::dprintf_va( int flags, char* fmt, va_list args )
{
		// Print nothing in this version.  A subclass like MPIShadow
		// might like to say ::dprintf( flags, "(res %d)"
	::dprintf( flags, "" );
	::_condor_dprintf_va( flags | D_NOHEADER, fmt, args );
}

void BaseShadow::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	this->dprintf_va( flags, fmt, args );
	va_end( args );
}

// This is declared in main.C, and is a pointer to one of the 
// various flavors of derived classes of BaseShadow.  
// It is only needed for this last function.
extern BaseShadow *Shadow;

// This function is called by dprintf - always display our job, proc,
// and pid in our log entries. 
extern "C" 
int
display_dprintf_header(FILE *fp)
{
	static pid_t mypid = 0;
	static int mycluster = -1;
	static int myproc = -1;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	if (mycluster == -1) {
		mycluster = Shadow->getCluster();
		myproc = Shadow->getProc();
	}

	if ( mycluster != -1 ) {
		fprintf( fp, "(%d.%d) (%ld): ", mycluster,myproc,mypid );
	} else {
		fprintf( fp, "(?.?) (%ld): ", mypid );
	}	

	return TRUE;
}



