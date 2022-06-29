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
		dprintf( D_ALWAYS, "ERROR adding environment variable to job\n");
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
	Env env;
	std::string env_errors;
	if ( !env.MergeFrom(JobAd, env_errors) ) {
		dprintf( D_ALWAYS, "Failed to read environment from JobAd: %s\n", 
				 env_errors.c_str() );
		return 0;
	}

		// Add the remote spool dir, the "server" directory for
		// condor_chirp to stage files to/from
	std::string spool;
	if ( ! JobAd->LookupString( ATTR_REMOTE_SPOOL_DIR, spool ) ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_REMOTE_SPOOL_DIR);
		return 0;
	}

    env.SetEnv( "_CONDOR_REMOTE_SPOOL_DIR", spool.c_str() );

		// Add this node's number to CONDOR_PROCNO
	char buf[128];
	sprintf(buf, "%d", Node);
	env.SetEnv("_CONDOR_PROCNO", buf);


		// And put the total number of nodes into CONDOR_NPROC
	int machine_count;
	if ( ! JobAd->LookupInteger( ATTR_CURRENT_HOSTS, machine_count ) ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_CURRENT_HOSTS);
		return 0;
	}

	sprintf(buf, "%d", machine_count);
	env.SetEnv("_CONDOR_NPROCS", buf);

		// Now stick the condor bin directory in front of the path,
		// so user scripts can call condor_config_val
    char *bin = param( "BIN" );
    if ( !bin ) {
        dprintf ( D_ALWAYS, "Can't find BIN "
                  "in config file! Aborting!\n" ); 
        return 0;
    }

	std::string path;
	std::string new_path;
	char *tmp;

	new_path = bin;
	new_path += ":";

	if(env.GetEnv("PATH",path)) {
        // The user gave us a path in env.  Find & alter:
        dprintf ( D_FULLDEBUG, "$PATH in ad:%s\n", path.c_str() );

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
	free(bin);
	env.SetEnv("PATH",new_path.c_str());

	char *condor_config = getenv( "CONDOR_CONFIG");
	if (condor_config) {
		env.SetEnv("CONDOR_CONFIG", condor_config);
	}
	
	if(IsFulldebug(D_FULLDEBUG)) {
		std::string env_str;
		env.getDelimitedStringForDisplay( env_str);
		dprintf ( D_FULLDEBUG, "New env: %s\n", env_str.c_str() );
	}

        // now put the env back into the JobAd:
	if(!env.InsertEnvIntoClassAd(JobAd, env_errors)) {
		dprintf( D_ALWAYS, "Unable to update env! Aborting: %s\n",
				 env_errors.c_str());
		return 0;
	}

	return 1;
}


bool
ParallelProc::JobReaper( int pid, int status )
{ 
	return VanillaProc::JobReaper( pid, status );
}


void 
ParallelProc::Suspend() { 
        /* We Comrades don't ever want to be suspended.  We 
           take it as a violation of our basic rights.  Therefore, 
           we walk off the job and notify the shadow immediately! */
	dprintf(D_FULLDEBUG,"in ParallelProc::Suspend()\n");
		// must do this so that we exit...
	daemonCore->Signal_Myself(SIGQUIT);
}


void 
ParallelProc::Continue() { 
	dprintf(D_FULLDEBUG,"in ParallelProc::Continue() (!)\n");    
        // really should never get here, but just in case.....
    VanillaProc::Continue();
}


bool
ParallelProc::ShutdownFast() {
	return VanillaProc::ShutdownGraceful();
}

bool
ParallelProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In ParallelProc::PublishUpdateAd()\n" );
	ad->Assign( ATTR_NODE, Node );

		// Now, call our parent class's version
	return VanillaProc::PublishUpdateAd( ad );
}

