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
#include "mpi_master_proc.h"
#include "condor_attributes.h"
#include "condor_string.h"  // for strnewp
#include "my_hostname.h"

MPIMasterProc::MPIMasterProc( ClassAd * jobAd ) : MPIComradeProc( jobAd )
{
    dprintf ( D_FULLDEBUG, "Constructor of MPIMasterProc::MPIMasterProc\n" );
    dprintf ( D_PROTOCOL, "#4 - MPI MasterProc Started.\n" );
}

MPIMasterProc::~MPIMasterProc() {}

int
MPIMasterProc::StartJob() {

    dprintf ( D_FULLDEBUG, "MPIMasterProc::StartJob()\n" );

    if ( !alterEnv() ) {
        return FALSE;
    }

    dprintf ( D_PROTOCOL, "#5 - altered job environment, starting master:\n" );

        // The master starts like the comrades...
    return MPIComradeProc::StartJob();
}

int
MPIMasterProc::alterEnv() {
/* This routine is here in the starter because only here do we know 
   if there is a PATH environment variable set.  This is important
   if there is no Env in the JobAd, or there is no PATH in said
   Env.  In this case, we get the path from the local environment
   and prepend stuff to it - we DON'T want to clobber the path
   totally (which is what we'd do if we set this PATH back in 
   the shadow....). */

/* task:  First, see if there's a PATH var. in the JobAd->env.  
   If there is, alter it.  If there isn't, insert one. */
    
    dprintf ( D_FULLDEBUG, "MPIMasterProc::alterPath()\n" );

    char *tmp;
	char env[_POSIX_ARG_MAX];
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, env )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

    char *condor_rsh = param( "MPI_CONDOR_RSH_PATH" );
    if ( !condor_rsh ) {
        dprintf ( D_ALWAYS, "Can't find MPI_CONDOR_RSH_PATH "
                  "in config file! Aborting!\n" ); 
        return 0;
    }

    char path[_POSIX_ARG_MAX];
    if ( (env[0] == '\0') ||
         (!strstr( env, "PATH" ) ) ) {
            // User did not specify any env, or there is no 'PATH'
            // in env sent along.  We get $PATH and alter it.

        tmp = getenv( "PATH" );
        if ( tmp ) {
            dprintf ( D_FULLDEBUG, "No Path in ad, $PATH in env\n" );
            dprintf ( D_FULLDEBUG, "before: %s\n", tmp );
            sprintf ( path, "PATH=%s:%s", condor_rsh, tmp );
        }
        else {   // no PATH in env.  Make one.
            dprintf ( D_FULLDEBUG, "No Path in ad, no $PATH in env\n" );
            sprintf ( path, "PATH=%s", condor_rsh );
        }

        if ( env[0] == '\0' ) {
            sprintf ( env, "%s = \"%s", ATTR_JOB_ENVIRONMENT, path );
        } else {
            char bar[4096];
            sprintf ( bar, "%s = \"%s;%s", ATTR_JOB_ENVIRONMENT, env, path );
            strcpy( env, bar );
        }
    }
    else {
            // The user gave us a path in env.  Find & alter:
        dprintf ( D_FULLDEBUG, "$PATH in ad...env:\n" );
        dprintf ( D_FULLDEBUG, "%s\n", env );
        
        /* The env. is a ';' delimited list, the elements within 
           the path are ':' delimited.  */

        char *tok;
        int n = 0;
        tmp = strnewp( env );
        memset( env, 0, _POSIX_ARG_MAX );

        n += sprintf ( env, "%s = \"", ATTR_JOB_ENVIRONMENT );

        tok = strtok( tmp, ";" );
        while ( tok ) {
            if ( !strncmp( tok, "PATH=", 5 ) ) {
                    // match!
                n += sprintf( &env[n], "PATH=%s:%s;", condor_rsh, &tok[5] );
            }
            else {  // not PATH, stick back in...
                n += sprintf( &env[n], "%s;", tok );
            }
            tok = strtok( NULL, ";" );
        }

        delete [] tmp;
    }
    
        /* We want to add one little thing to the environment:
           We want to put the var MPI_MY_ADDRESS in here, 
           so condor_rsh can find it.  In this context, "MY"
		   referrs to the shadow.  Really. */
    char shad[128], foo[128];
    shad[0] = foo[0] = 0;
	if ( JobAd->LookupString( ATTR_MY_ADDRESS, shad ) < 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_MY_ADDRESS );
		return 0;
	}

    sprintf( foo, ";MPI_SHADOW_SINFUL=%s\"", shad );
    strcat( env, foo );

        // now put the env back into the JobAd:
    dprintf ( D_FULLDEBUG, "%s\n", env );
	if ( !JobAd->InsertOrUpdate( env ) ) {
		dprintf( D_ALWAYS, "Unable to update env! Aborting.\n" );
		return 0;
	}

    return 1;
}
