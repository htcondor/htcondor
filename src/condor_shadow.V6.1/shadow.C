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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "shadow.h"
#include "exit.h"
#include "condor_qmgr.h"
#include "condor_attributes.h"
#include "afs.h"
#include "condor_config.h"
#include "condor_email.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)    */

ReliSock *syscall_sock = NULL;

extern int do_REMOTE_syscall();
extern CShadow *Shadow;

/* CShadow class implementation */

CShadow::CShadow()
{
	Spool = NULL;
	FSDomain = UIDDomain = NULL;
	CkptServerHost = NULL;
	UseAFS = UseNFS = UseCkptServer = false;
	JobAd = NULL;
	Cluster = Proc = -1;
	Owner[0] = '\0';
	Iwd[0] = '\0';
	ScheddAddr = NULL;
	ExecutingHost = NULL;
	Capability = NULL;
	AFSCell = NULL;
	StarterNum = 2;		// standard starter; override as needed
	JobExitedGracefully = false;
	ExitStatus = ExitReason = -1;
}

CShadow::~CShadow()
{
	if (Spool) free(Spool);
	if (FSDomain) free(FSDomain);
	if (CkptServerHost) free(CkptServerHost);
	if (JobAd) FreeJobAd(JobAd);
}

void
CShadow::dprintf_va( int flags, char* fmt, va_list args )
{
	::dprintf( flags, "(res0) " );
	::_condor_dprintf_va( flags | D_NOHEADER, fmt, args );
}

void
CShadow::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	this->dprintf_va( flags, fmt, args );
	va_end( args );
}

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
		mycluster = Shadow->GetCluster();
		myproc = Shadow->GetProc();
	}

	if ( mycluster != -1 ) {
		fprintf( fp, "(%d.%d) (%d): ", mycluster,myproc,mypid );
	} else {
		fprintf( fp, "(?.?) (%d): ", mypid );
	}	

	return TRUE;
}

void
CShadow::Init(char schedd_addr[], char host[], char capability[],
			  char cluster[], char proc[])
{
	if (schedd_addr[0] != '<') {
		EXCEPT("Schedd_Addr not specified with sinful string.");
	}
	ScheddAddr = schedd_addr;
	if (host[0] != '<') {
		EXCEPT("Host not specified with sinful string.");
	}
	ExecutingHost = host;
	Capability = capability;
	Cluster = atoi(cluster);
	Proc = atoi(proc);

	DebugId = display_dprintf_header;
	
	Config();

	// Connect to the schedd to grab our JobAd and set ATTR_REMOTE_HOST.
	if (JobAd) FreeJobAd(JobAd);
	ConnectQ(schedd_addr);
	JobAd = ::GetJobAd(Cluster, Proc);
	SetAttributeString(Cluster,Proc,ATTR_REMOTE_HOST,ExecutingHost);
	DisconnectQ(NULL);
	if (!JobAd) {
		EXCEPT("Failed to get job ad from schedd.");
	}

	if (JobAd->LookupString(ATTR_OWNER, Owner) != 1) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_OWNER);
	}
	if (JobAd->LookupString(ATTR_JOB_IWD, Iwd) != 1) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_JOB_IWD);
	}

	RequestResource();

	InitUserLog();

	// log execute event
	ExecuteEvent event;
	strcpy(event.executeHost, ExecutingHost);
	if (!ULog.writeEvent(&event)) {
		dprintf(D_ALWAYS, "Unable to log ULOG_EXECUTE event\n");
	}

#if 0	// NEED TO PORT TO WIN32
	AFSCell = get_host_cell();
	if (!AFSCell) {
		AFSCell = "(NO CELL)";
	}
#endif

	daemonCore->Register_Signal(DC_SIGUSR1, "Job Removed", 
		(SignalHandlercpp)HandleJobRemoval, "HandleJobRemoval", 
		this, IMMEDIATE_FAMILY);

	daemonCore->Register_Socket(&ClaimSock, "RSC Socket", 
		(SocketHandlercpp)HandleSyscalls, "HandleSyscalls", this);

	syscall_sock = &ClaimSock;

	// handle system calls with Owner's privilege
	init_user_ids(Owner);
	set_user_priv();

	if (chdir(Iwd) < 0) {
		dprintf(D_ALWAYS, "Can't chdir to %s\n");
		char *buf = new char [strlen(Iwd)+20];
		sprintf(buf, "Can't access \"%s\".", Iwd);
		// TODO: need to have some handler to email the user here
		delete buf;
	}
}

void
CShadow::Config()
{
	char *tmp;

	if (Spool) free(Spool);
	Spool = param("SPOOL");
	if (!Spool) {
		EXCEPT("SPOOL not specified in config file.");
	}

	if (FSDomain) free(FSDomain);
	FSDomain = param("FILESYSTEM_DOMAIN");
	if (!FSDomain) {
		EXCEPT("FILESYSTEM_DOMAIN not specified in config file.");
	}

	if (UIDDomain) free(UIDDomain);
	UIDDomain = param("UID_DOMAIN");
	if (!UIDDomain) {
		EXCEPT("UID_DOMAIN not specified in config file.");
	}

	tmp = param("USE_AFS");
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		UseAFS = true;
	} else {
		UseAFS = false;
	}
	if (tmp) free(tmp);

	tmp = param("USE_NFS");
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		UseNFS = true;
	} else {
		UseNFS = false;
	}
	if (tmp) free(tmp);

	if (CkptServerHost) free(CkptServerHost);
	CkptServerHost = param("CKPT_SERVER_HOST");
	tmp = param("USE_CKPT_SERVER");
	if (tmp && CkptServerHost && (tmp[0] == 'T' || tmp[0] == 't')) {
		UseCkptServer = true;
	} else {
		UseCkptServer = false;
	}
	if (tmp) free(tmp);

}

void
CShadow::RequestResource()
{
	int			reply;

	ClaimSock.close();	// make sure ClaimSock is a virgin socket
	ClaimSock.timeout(SHADOW_SOCK_TIMEOUT);
	if (!ClaimSock.connect(ExecutingHost, 0)) {
		dprintf(D_ALWAYS, "failed to connect to execute host %s\n", 
				ExecutingHost);
		Shutdown(JOB_NOT_STARTED);
	}

	ClaimSock.encode();
	if (!ClaimSock.put(ACTIVATE_CLAIM) || !ClaimSock.code(Capability) || 
		!ClaimSock.code(StarterNum) || !JobAd->put(ClaimSock) || 
		!ClaimSock.end_of_message()) {
		dprintf(D_ALWAYS, "failed to send ACTIVATE_CLAIM request to %s\n",
				ExecutingHost);
		Shutdown(JOB_NOT_STARTED);
	}

	ClaimSock.decode();
	if (!ClaimSock.code(reply) || !ClaimSock.end_of_message()) {
		dprintf(D_ALWAYS, "failed to receive ACTIVATE_CLAIM reply from %s\n",
				ExecutingHost);
		Shutdown(JOB_NOT_STARTED);
	}

	if (reply != OK) {
		dprintf(D_ALWAYS, "Request to run on %s was REFUSED.\n",ExecutingHost);
		Shutdown(JOB_NOT_STARTED);
	}
	dprintf(D_ALWAYS, "Request to run on %s was ACCEPTED.\n",ExecutingHost);
}

FILE *
CShadow::EmailUser(char *subjectline)
{
	char email_addr[256];

	dprintf(D_FULLDEBUG, "EmailUser() called.\n");

	if (!JobAd ) {
		return NULL;
	}
	
	/*
	** The job may have an email address to whom the notification message
    ** should go.  this info is in the classad.
    */
	email_addr[0] = '\0';
	if ( (JobAd->LookupString(ATTR_NOTIFY_USER, email_addr) != 1) ||
		 (email_addr[0] == '\0') ) {
		// no email address specified in the job ad; use owner
		strcpy(email_addr, Owner);
	}

	if ( strchr(email_addr,'@') == NULL )
	{
		// No host name specified; add uid domain. 
		// Note: UID_DOMAIN is set to the fullhostname by default.
		strcat(email_addr,"@");
		strcat(email_addr,UIDDomain);
	}

	return email_open(email_addr,subjectline);
}

void
CShadow::Shutdown(int reason)
{
		// update job queue with relavent info
	if ( JobAd ) {
		ConnectQ(ScheddAddr);
		DeleteAttribute(Cluster,Proc,ATTR_REMOTE_HOST);
		SetAttributeString(Cluster,Proc,ATTR_LAST_REMOTE_HOST,ExecutingHost);
		DisconnectQ(NULL);
	}

		// if we are being called from the exception handler, return
		// now.
	if ( reason == JOB_EXCEPTION ) {
		return;
	}

		// everything else we do only
		// makes sense if there is a JobAd, so bail out now if there
		// is no JobAd.
	if ( !JobAd ) {
		DC_Exit(reason);
		// DC_Exit() does not return
	}

	// send email if user requested it
	int notification = NOTIFY_COMPLETE;	// default
	JobAd->LookupInteger(ATTR_JOB_NOTIFICATION,notification);
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
			if( (reason != JOB_EXITED) || (WTERMSIG(JobStatus) == 0) ) {
				send_email = FALSE;
			}
			break;
		default:
			dprintf(D_ALWAYS, 
				"Condor Job %d.%d has unrecognized notification of %d\n",
				Cluster, Proc, notification );
	}
	if ( send_email ) {
		FILE* mailer;
		char buf[50];

		sprintf(buf,"Job %d.%d",Cluster,Proc);
		if ( (mailer=EmailUser(buf)) ) {
			fprintf(mailer,"Your Condor %s exited with status %d (0x%X).\n",
				buf,ExitStatus,ExitStatus);
			email_close(mailer);
		}
	}

	
	DC_Exit( reason );
}

void
CShadow::InitUserLog()
{
	char tmp[_POSIX_PATH_MAX], logfilename[_POSIX_PATH_MAX];
	if (JobAd->LookupString(ATTR_ULOG_FILE, tmp) == 1) {
		if (tmp[0] == '/') {
			strcpy(logfilename, tmp);
		} else {
			sprintf(logfilename, "%s/%s", Iwd, tmp);
		}
		ULog.initialize (Owner, logfilename, Cluster, Proc, 0);
		dprintf(D_FULLDEBUG, "%s = %s\n", ATTR_ULOG_FILE, logfilename);
	} else {
		dprintf(D_FULLDEBUG, "no %s found\n", ATTR_ULOG_FILE);
	}
}

int
CShadow::HandleSyscalls(Stream *sock)
{
	if (do_REMOTE_syscall() < 0) {
		dprintf(D_SYSCALLS, "Shadow: do_REMOTE_syscall returned < 0\n");
		KillStarter();
		Shutdown(ExitReason);
	}
	return KEEP_STREAM;
}

int
CShadow::HandleJobRemoval(int)
{
	ExitReason = JOB_KILLED;
	KillStarter();	
	return 0;
}

void
CShadow::KillStarter()
{
	ReliSock	sock;

	sock.timeout(SHADOW_SOCK_TIMEOUT);
	if (!sock.connect(ExecutingHost, 0)) {
		dprintf(D_ALWAYS, "failed to connect to executing host %s\n",
				ExecutingHost);
		return;
	}

	sock.encode();
	if (!sock.put(KILL_FRGN_JOB) || !sock.put(Capability) ||
		!sock.end_of_message()) {
		dprintf(D_ALWAYS, "failed to send KILL_FRGN_JOB to startd (%s)\n",
				ExecutingHost);
		return;
	}
}
