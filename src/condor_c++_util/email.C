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

#include "condor_common.h"
#include "condor_email.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "my_hostname.h"

extern "C" {

FILE *
email_user_open( ClassAd *jobAd, const char *subject )
{
	FILE* fp = NULL;
    char* email_addr = NULL;
    char* email_full_addr = NULL;
	int cluster = 0, proc = 0;
    int notification = NOTIFY_COMPLETE; // default

	ASSERT(jobAd);

    jobAd->LookupInteger( ATTR_JOB_NOTIFICATION, notification );
	jobAd->LookupInteger( ATTR_CLUSTER_ID, cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, proc );

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

	bool first_time = true;
	char *attr_str = NULL, *tmp = NULL;
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
		expr_tree = job_ad->Lookup(tmp);
		if( ! expr_tree ) {
			continue;
		}
		if( first_time ) {
			fprintf( mailer, "\n\n" );
			first_time = false;
		}
		expr_tree->PrintToNewStr( &attr_str );
		fprintf( mailer, "%s\n", attr_str );
		free( attr_str );
		attr_str = NULL;
	}
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


} /* extern "C" */
