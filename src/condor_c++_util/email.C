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
