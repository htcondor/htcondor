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
    char email_addr[256];
    email_addr[0] = '\0';
	int cluster = 0, proc = 0;
    int notification = NOTIFY_COMPLETE; // default

    jobAd->EvaluateAttrInt( ATTR_JOB_NOTIFICATION, notification );
	jobAd->EvaluateAttrInt( ATTR_CLUSTER_ID, cluster );
	jobAd->EvaluateAttrInt( ATTR_PROC_ID, proc );

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
    if ( (!jobAd->EvaluateAttrString(ATTR_NOTIFY_USER, email_addr, 256)) ||
         (email_addr[0] == '\0') ) {
			// no email address specified in the job ad; try owner 
		if ( (!jobAd->EvaluateAttrString(ATTR_OWNER, email_addr, 256)) ||
			 (email_addr[0] == '\0') ) {
				// we're screwed, give up.
			return NULL;
		}
	}

    if ( strchr(email_addr,'@') == NULL ) {
			// No host name specified; add a domain. 
		char* domain = NULL;

			// First, we check for EMAIL_DOMAIN in the config file
		domain = param( "EMAIL_DOMAIN" );

			// If that's not defined, we look for UID_DOMAIN in the
			// job ad
		if( ! domain ) {
            string s;
            if (jobAd->EvaluateAttrString( ATTR_UID_DOMAIN, s )) {
                domain = strdup(s.data());
            }
        }

			// If that's not there, look for UID_DOMAIN in the config
			// file
		if( ! domain ) {
			domain = param( "UID_DOMAIN" );
		} 

			// Now, we can append the domain to our address.
        strcat( email_addr, "@" );
        strcat( email_addr, domain );

			// No matter what method we used above to find the domain,
			// we've got to free() it now so we don't leak memory.
		free( domain );
    }

    return email_open( email_addr, subject );
}

} /* extern "C" */
