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
#include "parallel_master_proc.h"
#include "NTsenders.h"
#include "condor_attributes.h"
#include "condor_environ.h"
#include "condor_string.h"  // for strnewp
#include "my_hostname.h"
#include "env.h"

extern int main_shutdown_graceful();

ParallelMasterProc::ParallelMasterProc( ClassAd * jobAd ) : ParallelComradeProc( jobAd )
{
    dprintf ( D_FULLDEBUG, "Constructor of ParallelMasterProc::ParallelMasterProc\n" );
    dprintf ( D_PROTOCOL, "#4 - Parallel MasterProc Started.\n" );
}

ParallelMasterProc::~ParallelMasterProc() {}

int
ParallelMasterProc::StartJob()
{
	int rval;

    dprintf ( D_FULLDEBUG, "ParallelMasterProc::StartJob()\n" );

    if ( !alterEnv() ) {
        return FALSE;
    }
    dprintf ( D_PROTOCOL, "#5 - altered job environment, starting master:\n" );

        // The master starts like the comrades...
	rval = ParallelComradeProc::StartJob();

	return rval;
}


int
ParallelMasterProc::alterEnv()
{
/* This routine is here in the starter because only here do we know 
   if there is a PATH environment variable set.  This is important
   if there is no Env in the JobAd, or there is no PATH in said
   Env.  In this case, we get the path from the local environment
   and prepend stuff to it - we DON'T want to clobber the path
   totally (which is what we'd do if we set this PATH back in 
   the shadow....). */

/* task:  First, see if there's a PATH var. in the JobAd->env.  
   If there is, alter it.  If there isn't, insert one. */
    
    dprintf ( D_FULLDEBUG, "ParallelMasterProc::alterPath()\n" );

    char *tmp;
	char *env_str = NULL;
	Env envobject;
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, &env_str )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

	envobject.Merge(env_str);
	free(env_str);

    char *condor_rsh = param( "MPI_CONDOR_RSH_PATH" );
    if ( !condor_rsh ) {
        dprintf ( D_ALWAYS, "Can't find MPI_CONDOR_RSH_PATH "
                  "in config file! Aborting!\n" ); 
        return 0;
    } else {
		dprintf(D_FULLDEBUG, "MPI_CONDOR_RSH_PATH is %s\n", condor_rsh);
	}

	MyString path;
	MyString new_path;

	new_path = condor_rsh;
	new_path += ":";

	if(envobject.getenv("PATH",path)) {
        // The user gave us a path in env.  Find & alter:
        dprintf ( D_FULLDEBUG, "$PATH in ad:%s\n", path.Value() );

		new_path += path;
	}
	else {
        // User did not specify any env, or there is no 'PATH'
        // in env sent along.  We get $PATH and alter it.

        tmp = getenv( "PATH" );
        if ( tmp ) {
            dprintf ( D_FULLDEBUG, "No Path in ad, $PATH in env\n" );
            dprintf ( D_FULLDEBUG, "before: %s\n", tmp );
			new_path += tmp;
        }
        else {   // no PATH in env.  Make one.
            dprintf ( D_FULLDEBUG, "No Path in ad, no $PATH in env\n" );
			new_path = condor_rsh;
        }
    }
	envobject.Put("PATH",new_path.Value());

        /* We want to add one little thing to the environment:
           We want to put the var ATTR_MY_ADDRESS in here, 
           so condor_rsh can find it.  In this context, "MY"
		   referrs to the shadow.  Really. */
    char shad[128], foo[128];
    shad[0] = foo[0] = 0;
	if ( JobAd->LookupString( ATTR_MY_ADDRESS, shad ) < 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_MY_ADDRESS );
		return 0;
	}

    envobject.Put( "PARALLEL_SHADOW_SINFUL",shad );

		// In case the user job is linked with a newer version of
		// MPICH that honors the P4_RSHCOMMAND env var, let's set
		// that, too.
	MyString condor_rsh_command;
	condor_rsh_command = condor_rsh;
	condor_rsh_command += "/rsh";
	envobject.Put( "P4_RSHCOMMAND", condor_rsh_command.Value());

        // now put the env back into the JobAd:
	env_str = envobject.getDelimitedString();
    dprintf ( D_FULLDEBUG, "New env: %s\n", env_str );

	bool assigned = JobAd->Assign( ATTR_JOB_ENVIRONMENT,env_str );
	if(env_str) {
		delete[] env_str;
	}
	if(!assigned) {
		dprintf( D_ALWAYS, "Unable to update env! Aborting.\n" );
		return 0;
	}

    return 1;
}
