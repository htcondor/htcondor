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
#include "shadow.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.

extern "C" char* d_format_time(double);

UniShadow::UniShadow() {
		// pass RemoteResource ourself, so it knows where to go if
		// it has to call something like shutDown().
	remRes = new RemoteResource( this );
}

UniShadow::~UniShadow() {
	if ( remRes ) delete remRes;
}

int
UniShadow::UpdateFromStarter(int command, Stream *s)
{
	ClassAd updateAd;
	ClassAd *jobad = getJobAd();
	char buf[300];
	int int_value;
	float float_value;
	
	// get info from the starter encapsulated in a ClassAd
	s->decode();
	updateAd.initFromStream(*s);
	s->end_of_message();

	if ( !jobad ) {
		// should never really happen...
		return FALSE;
	}

	// update the schedd queue and our jobad with the info from the starter
	if ( ConnectQ(getScheddAddr(), SHADOW_QMGMT_TIMEOUT) ) {

		// update image size
		if ( updateAd.LookupInteger(ATTR_IMAGE_SIZE, int_value) == 1 ) 
		{
			// lookup old image size
			int previous_size = 0;
			jobad->LookupInteger(ATTR_IMAGE_SIZE, previous_size);
			
			// if new image size is larger, update it
			if ( int_value > previous_size ) {

				SetAttributeInt(getCluster(),getProc(),ATTR_IMAGE_SIZE,
					int_value);
				sprintf(buf,"%s=%d",ATTR_IMAGE_SIZE,int_value);
				jobad->InsertOrUpdate(buf);

				// also update the User Log with an image size event
				JobImageSizeEvent event;
				event.size = int_value;
				if (!uLog.writeEvent (&event)) {
					dprintf (D_ALWAYS, 
						"Unable to log ULOG_IMAGE_SIZE event\n");
				}
			}
		}

		// update disk usage size
		if ( updateAd.LookupInteger(ATTR_DISK_USAGE, int_value) == 1 ) 
		{
			// lookup old disk usage size
			int previous_size = 0;
			jobad->LookupInteger(ATTR_DISK_USAGE, previous_size);
			
			// if new disk usage size is larger, update it
			if ( int_value > previous_size ) {

				SetAttributeInt(getCluster(),getProc(),ATTR_DISK_USAGE,
					int_value);
				sprintf(buf,"%s=%d",ATTR_DISK_USAGE,int_value);
				jobad->InsertOrUpdate(buf);
			}
		}

		// update remote sys cpu
		if ( updateAd.LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, 
			float_value) == 1 ) 
		{
			SetAttributeFloat(getCluster(),getProc(),ATTR_JOB_REMOTE_SYS_CPU,
				float_value);
			sprintf(buf,"%s=%f",ATTR_JOB_REMOTE_SYS_CPU,float_value);
			jobad->InsertOrUpdate(buf);
		}
			
		// update remote user cpu
		if ( updateAd.LookupFloat(ATTR_JOB_REMOTE_USER_CPU, 
			float_value) == 1 ) 
		{
			SetAttributeFloat(getCluster(),getProc(),ATTR_JOB_REMOTE_USER_CPU,
				float_value);
			sprintf(buf,"%s=%f",ATTR_JOB_REMOTE_USER_CPU,float_value);
			jobad->InsertOrUpdate(buf);
		}

		// close our connection to the queue
		DisconnectQ(NULL);

		return TRUE;
	}		

	return FALSE;
}


void UniShadow::init( ClassAd *jobAd, char schedd_addr[], char host[], 
					  char capability[], char cluster[], char proc[])
{
	if ( !jobAd ) {
		EXCEPT("No jobAd defined!");
	}

		// Register command which gets updates from the starter
		// on the job's image size, cpu usage, etc.  
		// Eventually this belongs in Remote Resource so we handle
		// this properly for parallel jobs.
	daemonCore->Register_Command( SHADOW_UPDATEINFO, "SHADOW_UPDATEINFO",
			(CommandHandlercpp)&UniShadow::UpdateFromStarter, 
			"UniShadow::UpdateFromStarter", this, WRITE);


		// we're only dealing with one host, so this is trivial:
	remRes->setExecutingHost( host );
	remRes->setCapability( capability );
	remRes->setMachineName( "Unknown" );
	
		// base init takes care of lots of stuff:
	baseInit( jobAd, schedd_addr, cluster, proc );

		// In this case we just pass the pointer along...
	remRes->setJobAd( jobAd );
	
		// yak with startd:
	if ( remRes->requestIt() == -1 ) {
		shutDown( JOB_NOT_STARTED, 0 );
	}

		// Make an execute event:
	ExecuteEvent event;
	strcpy( event.executeHost, host );
	if ( !uLog.writeEvent(&event)) {
		dprintf(D_ALWAYS, "Unable to log ULOG_EXECUTE event\n");
	}

		// Register the remote instance's claimSock for remote
		// system calls.  It's a bit funky, but works.
	daemonCore->Register_Socket(remRes->getClaimSock(), "RSC Socket", 
		(SocketHandlercpp)&RemoteResource::handleSysCalls, "HandleSyscalls", 
								remRes);

}

void UniShadow::shutDown( int reason, int exitStatus ) 
{

		// Deactivate the claim
	if ( remRes ) {
		remRes->killStarter();
	}

		// exit now if there is no job ad
	if ( !getJobAd() ) {
		DC_Exit( reason );
	}
	
		// if we are being called from the exception handler, return
		// now to prevent infinite loop in case we call EXCEPT below.
	if ( reason == JOB_EXCEPTION ) {
		return;
	}

		// if job exited, check and see if it meets the user's
		// ExitRequirements, and/or update the job ad in the schedd
		// so that the exit status is recoreded in the history file.
	if ( reason == JOB_EXITED ) {
		char buf[200];
		sprintf(buf,"%s=%d",ATTR_JOB_EXIT_STATUS,exitStatus);
		jobAd->InsertOrUpdate(buf);
		int exit_requirements = TRUE;	// default to TRUE if not specified
		jobAd->EvalBool(ATTR_JOB_EXIT_REQUIREMENTS,jobAd,exit_requirements);
		if ( exit_requirements ) {
			// exit requirements are met; update the classad with
			// the exit status so it is properly recorded in the
			// history file.
			if ( ConnectQ(getScheddAddr(), SHADOW_QMGMT_TIMEOUT) ) {
				SetAttributeInt(getCluster(),getProc(),ATTR_JOB_EXIT_STATUS,
					exitStatus);
				DisconnectQ(NULL);
			}
		} else {
			// exit requirements expression is FALSE! 
			EXCEPT("Job exited with status %d; failed %s expression",
				exitStatus,ATTR_JOB_EXIT_REQUIREMENTS);
		}
	}

		// write stuff to user log:
	endingUserLog( exitStatus, reason, remRes );

		// returns a mailer if desired
	FILE* mailer;
	mailer = shutDownEmail( reason, exitStatus );
	if ( mailer ) {
		// gather all the info out of the job ad which we want to 
		// put into the email message.
		char JobName[_POSIX_PATH_MAX];
		JobName[0] = '\0';
		jobAd->LookupString( ATTR_JOB_CMD, JobName );

		char Args[_POSIX_ARG_MAX];
		Args[0] = '\0';
		jobAd->LookupString(ATTR_JOB_ARGUMENTS, Args);

		int q_date = 0;
		jobAd->LookupInteger(ATTR_Q_DATE,q_date);

		float remote_sys_cpu = 0.0;
		jobAd->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, remote_sys_cpu);

		float remote_user_cpu = 0.0;
		jobAd->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, remote_user_cpu);

		int image_size = 0;
		jobAd->LookupInteger(ATTR_IMAGE_SIZE, image_size);

		int shadow_bday = 0;
		jobAd->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );

		float previous_runs = 0;
		jobAd->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );

		time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
								archs, and this means that doing a (time_t*)
								cast on & of a 4 byte int makes my life hell.
								So we fix it by assigning the time we want to
								a real time_t variable, then using ctime()
								to convert it to a string */


		fprintf( mailer, "Your Condor job %d.%d \n",getCluster(),getProc());
		if ( JobName[0] ) {
			fprintf(mailer,"\t%s %s\n",JobName,Args);
		}
		
		fprintf(mailer,"has ");
		remRes->printExit( mailer );

		arch_time = q_date;
        fprintf(mailer, "\n\nSubmitted at:        %s", ctime(&arch_time));

        if( reason == JOB_EXITED ) {
                double real_time = time(NULL) - q_date;
				arch_time = time(NULL);
                fprintf(mailer, "Completed at:        %s", ctime(&arch_time));

                fprintf(mailer, "Real Time:           %s\n", 
					d_format_time(real_time));
        }
        fprintf( mailer, "\n" );

		fprintf(mailer, "Virtual Image Size:  %d Kilobytes\n\n", image_size);

        double rutime = remote_user_cpu;
        double rstime = remote_sys_cpu;
        double trtime = rutime + rstime;
		double wall_time = time(NULL) - shadow_bday;
		fprintf(mailer, "Statistics from last run:\n");
		fprintf(mailer, "Allocation/Run time:     %s\n",d_format_time(wall_time) );
        fprintf(mailer, "Remote User CPU Time:    %s\n", d_format_time(rutime) );
        fprintf(mailer, "Remote System CPU Time:  %s\n", d_format_time(rstime) );
        fprintf(mailer, "Total Remote CPU Time:   %s\n\n", d_format_time(trtime));

		double total_wall_time = previous_runs + wall_time;
		fprintf(mailer, "Statistics totaled from all runs:\n");
		fprintf(mailer, "Allocation/Run time:     %s\n",
										d_format_time(total_wall_time) );

		email_close(mailer);
	}

	remRes->dprintfSelf( D_FULLDEBUG );
	
		// does not return.
	DC_Exit( reason );
}

int UniShadow::handleJobRemoval(int sig) {
    dprintf ( D_FULLDEBUG, "In handleJobRemoval(), sig %d\n", sig );
	remRes->setExitReason( JOB_KILLED );
	remRes->killStarter();
		// more?
	return 0;
}

float UniShadow::bytesSent()
{
	return remRes->bytesSent();
}

float UniShadow::bytesReceived()
{
	return remRes->bytesRecvd();
}

