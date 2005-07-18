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
#include "parallel_proc.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "env.h"

ParallelProc::ParallelProc( ClassAd * jobAd ) : VanillaProc( jobAd )
{
    dprintf ( D_FULLDEBUG, "Constructor of ParallelProc::ParallelProc\n" );
	Node = -1;
}

ParallelProc::~ParallelProc()  {}


int 
ParallelProc::StartJob()
{ 
	dprintf(D_FULLDEBUG,"in ParallelProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in ParallelProc::StartJob()!\n" ); 
		return 0;
	}
		// Grab ATTR_NODE out of the job ad and stick it in our
		// protected member so we can insert it back on updates, etc. 
	if( JobAd->LookupInteger(ATTR_NODE, Node) != 1 ) {
		dprintf( D_ALWAYS, "ERROR in ParallelProc::StartJob(): "
				 "No %s in job ad, aborting!\n", ATTR_NODE );
		return 0;
	} else {
		dprintf( D_FULLDEBUG, "Found %s = %d in job ad\n", ATTR_NODE, 
				 Node ); 
	}

	if ( ! addEnvVars() ) {
		dprintf( D_ALWAYS, "ERROR adding environment variable to job");
		return 0;
	}

    dprintf(D_PROTOCOL, "#11 - Parallel_proc starting up....\n" );

        // special args already in ad; simply start it up
    return VanillaProc::StartJob();
}


int
ParallelProc::addEnvVars() 
{
   dprintf ( D_FULLDEBUG, "ParallelProc::addEnvVars()\n" );

	   // Pull the environment out of the job ad...
	char *env_str = NULL;
	Env env;
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, &env_str )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

	env.Merge(env_str);
	free(env_str);

		// Add the remote spool dir, the "server" directory for
		// condor_chirp to stage files to/from
    char spool[128];
    spool[0] = 0;
	if ( JobAd->LookupString( ATTR_REMOTE_SPOOL_DIR, spool ) < 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_REMOTE_SPOOL_DIR);
		return 0;
	}

    env.Put( "_CONDOR_REMOTE_SPOOL_DIR", spool );

		// Add this node's number to CONDOR_PROCNO
	char buf[128];
	sprintf(buf, "%d", Node);
	env.Put("_CONDOR_PROCNO", buf);


		// And put the total number of nodes into CONDOR_NPROC
	int machine_count;
	if ( JobAd->LookupInteger( ATTR_CURRENT_HOSTS, machine_count ) !=  1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_CURRENT_HOSTS);
		return 0;
	}

	sprintf(buf, "%d", machine_count);
	env.Put("_CONDOR_NPROCS", buf);

		// Now stick the condor bin directory in front of the path,
		// so user scripts can call condor_config_val
    char *bin = param( "BIN" );
    if ( !bin ) {
        dprintf ( D_ALWAYS, "Can't find BIN "
                  "in config file! Aborting!\n" ); 
        return 0;
    }

	MyString path;
	MyString new_path;
	char *tmp;

	new_path = bin;
	new_path += ":";

	if(env.getenv("PATH",path)) {
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
			new_path = bin;
        }
    }
	env.Put("PATH",new_path.Value());

	char *condor_config = getenv( "CONDOR_CONFIG");
	if (condor_config) {
		env.Put("CONDOR_CONFIG", condor_config);
	}
	
        // now put the env back into the JobAd:
	env_str = env.getDelimitedString();
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

int
ParallelProc::JobCleanup( int pid, int status )
{ 
	return VanillaProc::JobCleanup( pid, status );
}


void 
ParallelProc::Suspend() { 
        /* We Comrades don't ever want to be suspended.  We 
           take it as a violation of our basic rights.  Therefore, 
           we walk off the job and notify the shadow immediately! */
	dprintf(D_FULLDEBUG,"in ParallelProc::Suspend()\n");
		// must do this so that we exit...
	daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
}


void 
ParallelProc::Continue() { 
	dprintf(D_FULLDEBUG,"in ParallelProc::Continue() (!)\n");    
        // really should never get here, but just in case.....
    VanillaProc::Continue();
}


bool
ParallelProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In ParallelProc::PublishUpdateAd()\n" );
	char buf[64];
	sprintf( buf, "%s = %d", ATTR_NODE, Node );
	ad->Insert( buf );

		// Now, call our parent class's version
	return VanillaProc::PublishUpdateAd( ad );
}

