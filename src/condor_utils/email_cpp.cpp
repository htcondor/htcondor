/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_email.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "classad_helpers.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "my_hostname.h"
#include "exit.h"               // for possible job exit_reason values
#include "metric_units.h"
#include "condor_arglist.h"
#include "condor_holdcodes.h"


extern "C" char* d_format_time(double);   // this should be in a .h


// "private" helper for when we already know the cluster/proc
static FILE* email_user_open_id( ClassAd *jobAd, int cluster, int proc, 
								 const char *subject );

static void
construct_custom_attributes( MyString &attributes, ClassAd* job_ad );

FILE *
email_user_open( ClassAd *jobAd, const char *subject )
{
	int cluster = 0, proc = 0;
	jobAd->LookupInteger( ATTR_CLUSTER_ID, cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, proc );
	return email_user_open_id( jobAd, cluster, proc, subject );
}


FILE *
email_user_open_id( ClassAd *jobAd, int cluster, int proc, 
					const char *subject )
{
	FILE* fp = NULL;
    char* email_addr = NULL;
    char* email_full_addr = NULL;
    int notification = NOTIFY_COMPLETE; // default

	ASSERT(jobAd);

		// TODO make this all work properly w/ shouldSend()
    jobAd->LookupInteger( ATTR_JOB_NOTIFICATION, notification );
    switch( notification ) {
	case NOTIFY_NEVER:
		dprintf( D_FULLDEBUG, 
				 "The owner of job %d.%d doesn't want email.\n",
				 cluster, proc );
		return NULL;
		break;
	case NOTIFY_COMPLETE:
			// Should this get email or not?
	case NOTIFY_ERROR:
	case NOTIFY_ALWAYS:
		break;
	default:
		dprintf( D_ALWAYS, 
				 "Condor Job %d.%d has unrecognized notification of %d\n",
				 cluster, proc, notification );
		break;
    }

    /*
	  Job may have an email address to whom the notification
	  message should go.  This info is in the classad.
    */
    if( ! jobAd->LookupString(ATTR_NOTIFY_USER, &email_addr) ) {
			// no email address specified in the job ad; try owner 
		if( ! jobAd->LookupString(ATTR_OWNER, &email_addr) ) {
				// we're screwed, give up.
			return NULL;
		}
	}
		// make sure we've got a valid address with a domain
	email_full_addr = email_check_domain( email_addr, jobAd );
	fp = email_open( email_full_addr, subject );
	free( email_addr );
	free( email_full_addr );
	return fp;
}

void
email_custom_attributes( FILE* mailer, ClassAd* job_ad )
{
	if( !mailer || !job_ad ) {
		return;
	}
    MyString attributes;

    construct_custom_attributes( attributes, job_ad );
    fprintf( mailer, "%s", attributes.Value( ) );
    return;
}

static void
construct_custom_attributes( MyString &attributes, ClassAd* job_ad )
{
    attributes = "";

	bool first_time = true;
	char *tmp = NULL;
	job_ad->LookupString( ATTR_EMAIL_ATTRIBUTES, &tmp );
	if( ! tmp ) {
		return;
	}

	StringList email_attrs;
	email_attrs.initializeFromString( tmp );
	free( tmp );
	tmp = NULL;
		
	ExprTree* expr_tree;
	email_attrs.rewind();
	while( (tmp = email_attrs.next()) ) {
		expr_tree = job_ad->LookupExpr(tmp);
		if( ! expr_tree ) {
            dprintf(D_ALWAYS, "Custom email attribute (%s) is undefined.", 
                    tmp);
			continue;
		}
		if( first_time ) {
			attributes.formatstr_cat( "\n\n" );
			first_time = false;
		}
		attributes.formatstr_cat( "%s = %s\n", tmp, ExprTreeToString(expr_tree) );
	}
    return;
}


char*
email_check_domain( const char* addr, ClassAd* job_ad )
{
	MyString full_addr = addr;

	if( full_addr.FindChar('@') >= 0 ) {
			// Already has a domain, we're done
		return strdup( addr );
	}

		// No host name specified; add a domain. 
	char* domain = NULL;

		// First, we check for EMAIL_DOMAIN in the config file
	domain = param( "EMAIL_DOMAIN" );

		// If that's not defined, we look for UID_DOMAIN in the job ad
	if( ! domain ) {
		job_ad->LookupString( ATTR_UID_DOMAIN, &domain );
	}

		// If that's not there, look for UID_DOMAIN in the config file
	if( ! domain ) {
		domain = param( "UID_DOMAIN" );
	} 
	
	if( ! domain ) {
			// we're screwed, we can't append a domain, just return
			// the username again... 
		return strdup( addr );
	}
	
	full_addr += '@';
	full_addr += domain;

		// No matter what method we used above to find the domain,
		// we've got to free() it now so we don't leak memory.
	free( domain );

	return strdup( full_addr.Value() );
}


Email::Email()
{
	init();
}


Email::~Email()
{
	if( fp ) {
		send();
	}
}


void
Email::init()
{
	fp = NULL;
	cluster = -1;
	proc = -1;
	email_admin = false;
}


void
Email::sendHold( ClassAd* ad, const char* reason )
{
	sendAction( ad, reason, "put on hold" );
}


void
Email::sendRemove( ClassAd* ad, const char* reason )
{
	sendAction( ad, reason, "removed" );
}

void
Email::sendRelease( ClassAd* ad, const char* reason )
{
	sendAction( ad, reason, "released from hold" );
}

void
Email::sendHoldAdmin( ClassAd* ad, const char* reason )
{
	email_admin = true;
	sendAction( ad, reason, "put on hold" );
}

void
Email::sendRemoveAdmin( ClassAd* ad, const char* reason )
{
	email_admin = true;
	sendAction( ad, reason, "removed" );
}

void
Email::sendReleaseAdmin( ClassAd* ad, const char* reason )
{
	email_admin = true;
	sendAction( ad, reason, "released from hold" );
}


void
Email::sendAction( ClassAd* ad, const char* reason,
						const char* action )
{
	if( ! ad ) {
		EXCEPT( "Email::sendAction() called with NULL ad!" );
	}

	if( ! open_stream(ad, -1, action) ) {
			// nothing to do
		return;
	}

	writeJobId( ad );

	fprintf( fp, "\nis being %s.\n\n", action );
	fprintf( fp, "%s", reason );

	send();
}


/*
void
Email::sendError( ClassAd* ad, const char* err_summary, 
				  const char* err_msg )
{
		// TODO!
}
*/


FILE*
Email::open_stream( ClassAd* ad, int exit_reason, const char* subject )
{
	if( ! shouldSend(ad, exit_reason) ) {
			// nothing to do
		return NULL;
	}

	ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	ad->LookupInteger( ATTR_PROC_ID, proc );

	MyString full_subject;
	full_subject.formatstr( "Condor Job %d.%d", cluster, proc );
	if( subject ) {
		full_subject += " ";
		full_subject += subject;
	}
	if(email_admin) {
		fp = email_admin_open( full_subject.Value() );
	} else {
		fp = email_user_open_id( ad, cluster, proc, full_subject.Value() );
	}
	return fp; 
}



bool
Email::writeJobId( ClassAd* ad )
{
		// if we're not currently open w/ a message, we're done
	if( ! fp ) {
		return false;
	}
	char* cmd = NULL;
	ad->LookupString( ATTR_JOB_CMD, &cmd );

	MyString args;
	ArgList::GetArgsStringForDisplay(ad,&args);

	fprintf( fp, "Condor job %d.%d\n", cluster, proc);
	if( cmd ) {
		fprintf( fp, "\t%s", cmd );
		free( cmd );
		cmd = NULL;
		if( !args.IsEmpty() ) {
			fprintf( fp, " %s\n", args.Value() );
		} else {
			fprintf( fp, "\n" );
		}
	}
	return true;
}


bool
Email::writeExit( ClassAd* ad, int exit_reason )
{
		// if we're not currently open w/ a message, we're done
	if( ! fp ) {
		return false;
	}

		// gather all the info out of the job ad which we want to 
		// put into the email message.

	int had_core = FALSE;
	if( ! ad->LookupBool(ATTR_JOB_CORE_DUMPED, had_core) ) {
		if( exit_reason == JOB_COREDUMPED ) {
			had_core = TRUE;
		}
	}

		// TODO ATTR_JOB_CORE_FILE_NAME...
		// we need the IWD in both cases...

	int q_date = 0;
	ad->LookupInteger( ATTR_Q_DATE, q_date );
	
	double remote_sys_cpu = 0.0;
	ad->LookupFloat( ATTR_JOB_REMOTE_SYS_CPU, remote_sys_cpu );
	
	double remote_user_cpu = 0.0;
	ad->LookupFloat( ATTR_JOB_REMOTE_USER_CPU, remote_user_cpu );
	
	int image_size = 0;
	ad->LookupInteger( ATTR_IMAGE_SIZE, image_size );
	
	int shadow_bday = 0;
	ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	
	double previous_runs = 0;
	ad->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );
	
	time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
						   archs, and this means that doing a (time_t*)
						   cast on & of a 4 byte int makes my life hell.
						   So we fix it by assigning the time we want to
						   a real time_t variable, then using ctime()
						   to convert it to a string */
	
	time_t now = time(NULL);

	writeJobId( ad );
	MyString msg;
	if( ! printExitString(ad, exit_reason, msg)	) {
		msg += "exited in an unknown way";
	}
	fprintf( fp, "%s\n", msg.Value() );

	if( had_core ) {
			// TODO!
			// fprintf( fp, "Core file is: %s\n", getCoreName() );
		fprintf( fp, "Core file generated\n" );
	}

	arch_time = q_date;
	fprintf(fp, "\n\nSubmitted at:        %s", ctime(&arch_time));
	
	if( exit_reason == JOB_EXITED || exit_reason == JOB_COREDUMPED ) {
		double real_time = now - q_date;
		arch_time = now;
		fprintf(fp, "Completed at:        %s", ctime(&arch_time));
		
		fprintf(fp, "Real Time:           %s\n", 
				d_format_time(real_time));
	}	

	fprintf( fp, "\n" );
	
	fprintf(fp, "Virtual Image Size:  %d Kilobytes\n\n", image_size);
	
	double rutime = remote_user_cpu;
	double rstime = remote_sys_cpu;
	double trtime = rutime + rstime;
	double wall_time = 0;
	fprintf(fp, "Statistics from last run:\n");
	if(shadow_bday != 0) {	// Handle cases where this wasn't set (grid)
		wall_time = now - shadow_bday;
	}
	fprintf(fp, "Allocation/Run time:     %s\n",d_format_time(wall_time) );
	fprintf(fp, "Remote User CPU Time:    %s\n", d_format_time(rutime) );
	fprintf(fp, "Remote System CPU Time:  %s\n", d_format_time(rstime) );
	fprintf(fp, "Total Remote CPU Time:   %s\n\n", d_format_time(trtime));
	
	double total_wall_time = previous_runs + wall_time;
	fprintf(fp, "Statistics totaled from all runs:\n");
	fprintf(fp, "Allocation/Run time:     %s\n",
			d_format_time(total_wall_time) );

		// TODO: deal w/ bytes directly from the classad
	return true;
}


// This method sucks.  As soon as we have a real solution for storing
// all 4 of these values in the job classad, it should be removed.
void
Email::writeBytes( float run_sent, float run_recv, float tot_sent,
				   float tot_recv )
{
		// if we're not currently open w/ a message, we're done
	if( ! fp ) {
		return;
	}

	fprintf( fp, "\nNetwork:\n" );
	fprintf( fp, "%10s Run Bytes Received By Job\n", 
			 metric_units(run_recv) );
	fprintf( fp, "%10s Run Bytes Sent By Job\n",
			 metric_units(run_sent) );
	fprintf( fp, "%10s Total Bytes Received By Job\n", 
			 metric_units(tot_recv) );
	fprintf( fp, "%10s Total Bytes Sent By Job\n",
			 metric_units(tot_sent) );
}

void 
Email::writeCustom( ClassAd *ad )
{
		// if we're not currently open w/ a message, we're done
	if( ! fp ) {
		return;
	}

    MyString attributes;

    construct_custom_attributes( attributes, ad );
    fprintf( fp, "%s", attributes.Value() );

    return;
}

bool
Email::send( void )
{
	if( ! fp ) {
		return false;
	}
	email_close(fp);
	init();
	return true;
}


void
Email::sendExit( ClassAd* ad, int exit_reason )
{
	open_stream( ad, exit_reason );
	writeExit( ad, exit_reason );
    writeCustom( ad );
	send();
}


// This method sucks.  As soon as we have a real solution for storing
// all 4 of these values in the job classad, it should be removed.
void
Email::sendExitWithBytes( ClassAd* ad, int exit_reason,
						  float run_sent, float run_recv,
						  float tot_sent, float tot_recv )
{
	open_stream( ad, exit_reason );
	writeExit( ad, exit_reason );
	writeBytes( run_sent, run_recv, tot_sent, tot_recv );
    writeCustom( ad );
	send();
}


bool 
Email::shouldSend( ClassAd* ad, int exit_reason, bool is_error )
{
	if ( !ad ) {
		return false;
	}

	int ad_cluster = 0, ad_proc = 0;
	int exit_by_signal = FALSE;
	int code = -1, status = -1;

	// send email if user requested it
	int notification = NOTIFY_COMPLETE;	// default
	ad->LookupInteger( ATTR_JOB_NOTIFICATION, notification );

	switch( notification ) {
		case NOTIFY_NEVER:
			return false;
			break;
		case NOTIFY_ALWAYS:
			return true;
			break;
		case NOTIFY_COMPLETE:
			if( exit_reason==JOB_EXITED || exit_reason==JOB_COREDUMPED ) {
				return true;
			}
			break;
		case NOTIFY_ERROR:
				// only send email if the job was killed by a signal
				// and/or core dumped.
			if( is_error || (exit_reason == JOB_COREDUMPED) ) {
				return true;
			}
			ad->LookupBool( ATTR_ON_EXIT_BY_SIGNAL, exit_by_signal );
			if( exit_reason == JOB_EXITED && exit_by_signal ) {
				return true;
			}
				// send mail if job was put on hold because of some problem
				// (not via condor_hold, periodic hold, or submitted on hold)
			if( (ad->LookupInteger(ATTR_JOB_STATUS, status) &&
				status == HELD) &&
				(ad->LookupInteger(ATTR_HOLD_REASON_CODE, code) &&
				code != CONDOR_HOLD_CODE_UserRequest &&
				code != CONDOR_HOLD_CODE_JobPolicy &&
				code != CONDOR_HOLD_CODE_SubmittedOnHold) )
			{
					return true;
			}
			break;
		default:
			ad->LookupInteger( ATTR_CLUSTER_ID, ad_cluster );
			ad->LookupInteger( ATTR_PROC_ID, ad_proc );
			dprintf( D_ALWAYS,  "Condor Job %d.%d has "
					 "unrecognized notification of %d\n",
					 ad_cluster, ad_proc, notification );
				// When in doubt, better send it anyway...
			return true;
			break;
	}
	return false;
}
