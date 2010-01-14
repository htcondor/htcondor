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
#include "mpishadow.h"
#include "condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.
#include "list.h"                // List class
#include "internet.h"            // sinful->hostname stuff
#include "daemon.h"
#include "env.h"
#include "condor_claimid_parser.h"


MPIShadow::MPIShadow() {
    nextResourceToStart = 0;
	numNodes = 0;
    shutDownMode = FALSE;
    ResourceList.fill(NULL);
    ResourceList.truncate(-1);
	actualExitReason = -1;
	info_tid = -1;
#if ! MPI_USES_RSH
	master_addr = NULL;
	mpich_jobid = NULL;
#endif
}

MPIShadow::~MPIShadow() {
        // Walk through list of Remote Resources.  Delete all.
    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
        delete ResourceList[i];
    }
#if ! MPI_USES_RSH
	if( master_addr ) {
		free( master_addr );
	}
	if( mpich_jobid ) {
		free( mpich_jobid );
	}

#endif /* ! MPI_USES_RSH */
}

void 
MPIShadow::init( ClassAd* job_ad, const char* schedd_addr, const char *xfer_queue_contact_info )
{

	char buf[256];

    if( ! job_ad ) {
        EXCEPT( "No job_ad defined!" );
    }

        // BaseShadow::baseInit - basic init stuff...
    baseInit( job_ad, schedd_addr, xfer_queue_contact_info );

		// Register command which gets updates from the starter
		// on the job's image size, cpu usage, etc.  Each kind of
		// shadow implements it's own version of this to deal w/ it
		// properly depending on parallel vs. serial jobs, etc. 
	daemonCore->
		Register_Command( SHADOW_UPDATEINFO, "SHADOW_UPDATEINFO", 
						  (CommandHandlercpp)&MPIShadow::updateFromStarter,
						  "MPIShadow::updateFromStarter", this, DAEMON ); 

#if MPI_USES_RSH	

        /* Register Command for sneaky rsh: */
	daemonCore->Register_Command( MPI_START_COMRADE, "MPI_START_COMRADE", 
		 (CommandHandlercpp)&MPIShadow::startComrade, "startComrade", 
		 this, WRITE );

#else /* ! MPI_USES_RSH */

		// initialize mpich_jobid, since we'll need it to spawn later
	sprintf( buf, "%s.%d.%d", my_full_hostname(), getCluster(),
			 getProc() );
	mpich_jobid = strdup( buf );

#endif /* ! MPI_USES_RSH */

        // make first remote resource the "master".  Put it first in list.
    MpiResource *rr = new MpiResource( this );

    sprintf( buf, "%s = %s", ATTR_MPI_IS_MASTER, "TRUE" );
    if( !job_ad->Insert(buf) ) {
        dprintf( D_ALWAYS, "Failed to insert %s into jobAd.\n", buf );
        shutDown( JOB_NOT_STARTED );
    }

	replaceNode( job_ad, 0 );
	rr->setNode( 0 );
	sprintf( buf, "%s = 0", ATTR_NODE );
	job_ad->InsertOrUpdate( buf );
    rr->setJobAd( job_ad );

	rr->setStartdInfo( job_ad );

	job_ad->Assign( ATTR_JOB_STATUS, RUNNING );
    ResourceList[ResourceList.getlast()+1] = rr;

}


void
MPIShadow::reconnect( void )
{
	EXCEPT( "reconnect is not supported for MPI universe!" );
}


bool 
MPIShadow::supportsReconnect( void )
{
	return false;
}


void
MPIShadow::spawn( void )
{
		/*
		  This is lame.  We should really do a better job of dealing
		  with the multiple ClassAds for MPI universe via the classad
		  file mechanism (pipe to STDIN, usually), instead of this
		  whole mess, and spawn() should really just call
		  "startMaster()".  however, in the race to get disconnected
		  operation working for vanilla, we cut a few corners and
		  leave this as it is.  whenever we're seriously looking at
		  MPI support again, we should fix this, too.
		*/
		/*
		  Finally, register a timer to call getResources(), which
		  sends a command to the schedd to get all the job classads,
		  startd sinful strings, and ClaimIds for all the matches
		  for our computation.  
		  In the future this will just be a backup way to get the
		  info, since the schedd will start to push all this info to
		  our UDP command port.  For now, this is the only way we get
		  the info.
		*/
	info_tid = daemonCore->
		Register_Timer( 1, 0,
						(TimerHandlercpp)&MPIShadow::getResources,
						"getResources", this );
	if( info_tid < 0 ) {
		EXCEPT( "Can't register DC timer!" );
	}
}


int 
MPIShadow::getResources( void )
{
    dprintf ( D_FULLDEBUG, "Getting machines from schedd now...\n" );

    char *host = NULL;
    char *claim_id = NULL;
    MpiResource *rr;
	int job_cluster;
	char buf[_POSIX_PATH_MAX];

    int numProcs=0;    // the # of procs to come
    int numInProc=0;   // the # in a particular proc.
	ClassAd *job_ad = NULL;
	int nodenum = 1;
	ReliSock* sock;

	job_cluster = getCluster();
    rr = ResourceList[0];
	rr->getClaimId( claim_id );

		// First, contact the schedd and send the command, the
		// cluster, and the ClaimId
	Daemon my_schedd (DT_SCHEDD, NULL, NULL);

	if(!(sock = (ReliSock*)my_schedd.startCommand(GIVE_MATCHES))) {
		EXCEPT( "Can't connect to schedd at %s", getScheddAddr() );
	}
		
	sock->encode();
	if( ! sock->code(job_cluster) ) {
		EXCEPT( "Can't send cluster (%d) to schedd\n", job_cluster );
	}
	if( ! sock->code(claim_id) ) {
		EXCEPT( "Can't send ClaimId to schedd\n" );
	}

		// Now that we sent this, free the memory that was allocated
		// with getClaimId() above
	delete [] claim_id;
	claim_id = NULL;

	if( ! sock->end_of_message() ) {
		EXCEPT( "Can't send EOM to schedd\n" );
	}
	
		// Ok, that's all we need to send, now we can read the data
		// back off the wire
	sock->decode();

        // We first get the number of proc classes we'll be getting.
    if ( !sock->code( numProcs ) ) {
        EXCEPT( "Failed to get number of procs" );
    }

        /* Now, do stuff for each proc: */
    for ( int i=0 ; i<numProcs ; i++ ) {
        if( !sock->code( numInProc ) ) {
            EXCEPT( "Failed to get number of matches in proc %d", i );
        }

        dprintf ( D_FULLDEBUG, "Got %d matches for proc # %d\n",
				  numInProc, i );

        for ( int j=0 ; j<numInProc ; j++ ) {
            if ( !sock->code( host ) ||
                 !sock->code( claim_id ) ) {
                EXCEPT( "Problem getting resource %d, %d", i, j );
            }
			ClaimIdParser idp( claim_id );
            dprintf( D_FULLDEBUG, "Got host: %s id: %s\n", host, idp.publicClaimId() );
            
			job_ad = new ClassAd();
			if( !job_ad->initFromStream(*sock)  ) {
				EXCEPT( "Failed to get job classad for proc %d", i );
			}

            if ( i==0 && j==0 ) {
					/* 
					   TODO: once this is just backup for if the
					   schedd doesn't push it, we need to NOT ignore
					   the first match, since we don't already have
					   it, really.
					*/
                    /* first host passed on command line...we already 
                       have it!  We ignore it here.... */

                free( host );
                free( claim_id );
                host = NULL;
                claim_id = NULL;
				delete job_ad;
                continue;
            }

            rr = new MpiResource( this );
            rr->setStartdInfo( host, claim_id );
 				// for now, set this to the sinful string.  when the
 				// starter spawns, it'll do an RSC to register a real
				// hostname... 
			rr->setMachineName( host );

			replaceNode ( job_ad, nodenum );
			rr->setNode( nodenum );
			sprintf( buf, "%s = %d", ATTR_NODE, nodenum );
			job_ad->InsertOrUpdate( buf );
			sprintf( buf, "%s = \"%s\"", ATTR_MY_ADDRESS,
					 daemonCore->InfoCommandSinfulString() );
			job_ad->InsertOrUpdate( buf );
			rr->setJobAd( job_ad );
			nodenum++;

            ResourceList[ResourceList.getlast()+1] = rr;

                /* free stuff so next code() works correctly */
            free( host );
            free( claim_id );
            host = NULL;
            claim_id = NULL;

        } // end of for loop for this proc

    } // end of for loop on all procs...

    sock->end_of_message();

	numNodes = nodenum;  // for later use...

    dprintf ( D_PROTOCOL, "#1 - Shadow started; %d machines gotten.\n", 
			  numNodes );

    startMaster();

    return TRUE;
}


void
MPIShadow::startMaster()
{
    MpiResource *rr;
    dprintf ( D_FULLDEBUG, "In MPIShadow::startMaster()\n" );

		// This function does *TOTALLY* different things depending on
		// if we're using rsh to spawn the comrade nodes or if we're
		// getting the ip/port of the master via a file specified in
		// the environment and passing that back to the shadow to
		// spawn all the comrades at once.  However, in both cases, we
		// have to contact a startd to spawn the master node, so that
		// code is shared at the end...

#if MPI_USES_RSH

		// If we're using rsh, we've got to setup the procgroup file
		// (which is pretty expensive), hack the master ad to deal w/
		// file transfer stuff, command line args to specify the
		// procgroup file, etc, etc.

    FILE *pg;

        /* We use the list of resources to build a procgroup file, 
           then tell the master to start itself up. */

        /* Problem: none of the resources have started yet, so none
           have reported back their machine name (foo.cs.wisc.edu, etc)
           We use stuff in internet.c... */

        /* A p4 procgroup file has the following format:
 
           local_machine  0 [full_path_name] [loginname] 
           remote_machine 1 full_path_name [loginname] 
           ...

           We don't know the full path of the executable, so
           we'll just call it 'condor_exec'  This name has absolutely no
		   effect on the executable started, since condor does that...
        */

        /* XXX Note: a future version will have to figure out 
           how many to start on each machine; for now we assume
           one per machine */

        // first we open up the procgroup file (in working dir of job)
    char pgfilename[128];
    sprintf( pgfilename, "%s/procgroup.%d.%d", getIwd(), getCluster(), 
			 getProc() );
    if( (pg=safe_fopen_wrapper( pgfilename, "w" )) == NULL ) {
        dprintf( D_ALWAYS, "Failure to open %s for writing, errno %d\n", 
                 pgfilename, errno );
        shutDown( JOB_NOT_STARTED );
		return;
    }
        
    char mach[128];
    char *sinful = new char[128];
    struct sockaddr_in sin;

        // get the machine name (using string_to_sin and sin_to_hostname)
    rr = ResourceList[0];
    rr->getStartdAddress( sinful );
    string_to_sin( sinful, &sin );
    sprintf( mach, "%s", sin_to_hostname( &sin, NULL ));
    fprintf( pg, "%s 0 condor_exec %s\n", mach, getOwner() );

    dprintf ( D_FULLDEBUG, "Procgroup file:\n" );
    dprintf ( D_FULLDEBUG, "%s 0 condor_exec %s\n", mach, getOwner() );

        // for each resource, get machine name, make pgfile entry
    for ( int i=1 ; i<=ResourceList.getlast() ; i++ ) {
        rr = ResourceList[i];
        rr->getStartdAddress( sinful );
        string_to_sin( sinful, &sin );
        sprintf( mach, "%s", sin_to_hostname( &sin, NULL ) );
        fprintf( pg, "%s 1 condor_exec %s\n", mach, getOwner() );
        dprintf( D_FULLDEBUG, "%s 1 condor_exec %s\n", mach, getOwner() );
    }
    delete [] sinful;

        // set permissions on the procgroup file:
#ifndef WIN32
    if ( fchmod( fileno( pg ), 0666 ) < 0 ) {
        dprintf ( D_ALWAYS, "fchmod failed! errno %d\n", errno );
        shutDown( JOB_NOT_STARTED );
    }
#endif

    if ( fclose( pg ) == EOF ) {
        dprintf ( D_ALWAYS, "fclose failed!  errno = %d\n", errno );
        shutDown( JOB_NOT_STARTED );
    }

        // back to master resource:
    rr = ResourceList[0];

        // alter the master's args...
    hackMasterAd( rr->getJobAd() );

        // Once we actually spawn the job (below), the sneaky rsh
		// intercepts the master's call to rsh, and sends stuff to
		// startComrade()... 

#else /* ! MPI_USES_RSH */

		// All we have to do is modify the ad for the master to append
		// the MPICH-specific environment variables.
	rr = ResourceList[0];
	modifyNodeAd( rr->getJobAd() );

#endif /* ! MPI_USES_RSH */

		// In both cases, we've got to actually talk to a startd to
		// spawn the master node, register the claimSock for remote
		// system calls, and keep track of which resource to use
		// next.  All this stuff is done by spawnNode(), so just use
		// that.  
	spawnNode( rr );

    dprintf ( D_PROTOCOL, "#3 - Just requested Master resource.\n" );

}

#if (MPI_USES_RSH) 

int
MPIShadow::startComrade( int /* cmd */, Stream* s )
{
/* This command made by sneaky rsh:  condor_starter.V6.1/condor_rsh.C */

    dprintf ( D_FULLDEBUG, "Getting information for a comrade node\n" );
    
    char *comradeArgs = NULL;
    if ( !s->code( comradeArgs ) ||
         !s->end_of_message() )
    {
        dprintf ( D_ALWAYS, "Failed to receive comrade args!" );
        shutDown( JOB_NOT_STARTED );
    }

    dprintf ( D_PROTOCOL, "#8 - Received args from sneaky rsh\n" );
    dprintf ( D_FULLDEBUG, "Comrade args: %s\n", comradeArgs );

    MpiResource *rr = ResourceList[nextResourceToStart];
    
        // modify this comrade's arguments...using the comradeArgs given.
    hackComradeAd( comradeArgs, rr->getJobAd() );

    dprintf ( D_PROTOCOL, "#9 - Added args to jobAd, now requesting:\n" );

		// Now, call the shared method to really spawn this node
	spawnNode( rr );

    return TRUE;
}

void 
MPIShadow::hackMasterAd( ClassAd *ad )
{
/* simple:  get args, add -p4pg (and more...), put args back in */

	ArgList args;
	MyString args_error;
	if(!args.AppendArgsFromClassAd(ad,&args_error)) {
		dprintf( D_ALWAYS, "Aborting.  Failed to read arguments from JobAd: %s\n", 
				 args_error.Value() );
		shutDown( JOB_NOT_STARTED );
	}
    
	args.InsertArg("-p4pg",0);

	MyString procgroup;
	procgroup.sprintf("procgroup.%d.%d",getCluster(),getProc());
	args.InsertArg(procgroup.Value(),1);

	if(!args.InsertArgsIntoClassAd(ad,NULL,&args_error)) {
		dprintf( D_ALWAYS, "Unable to update args! Aborting: %s\n",
				 args_error.Value());
		shutDown( JOB_NOT_STARTED );
	}

		// While we're at it, if the job wants files transfered,
		// include the procgroup file in the list of input files.
		// This is only needed on the master.
	char *transfer_files = NULL;
	if( !ad->LookupString(ATTR_TRANSFER_FILES, &transfer_files) ) {
			// Nothing, we're done.
		return;
	}
		// Ok, we found it.  If it's set to anything other than
		// "Never", we need to do our work.
	if( transfer_files[0] == 'n' || transfer_files[0] == 'N' ) {
			// It's "never", we're done.
		free(transfer_files);
		return;
	}
	free(transfer_files);
	transfer_files = NULL;

		// Now, see if they already gave us a list.
	MyString new_transfer_files;
	if( !ad->LookupString(ATTR_TRANSFER_INPUT_FILES, &transfer_files) ) {
			// Nothing here, so we can safely add it ourselves. 
		new_transfer_files.sprintf( "%s = \"procgroup.%d.%d\"",
				 ATTR_TRANSFER_INPUT_FILES, getCluster(), getProc() ); 
	} else {
			// There's a list already.  We've got to append to it. 
		new_transfer_files.sprintf( "%s = \"%s, procgroup.%d.%d\"",
				 ATTR_TRANSFER_INPUT_FILES, transfer_files, getCluster(),
				 getProc() );

	}
	free(transfer_files);
	transfer_files = NULL;

	dprintf( D_FULLDEBUG, "About to append to job ad: %s\n",
			 new_transfer_files.Value() );
	if ( !ad->Insert( new_transfer_files.Value() )) {
		dprintf( D_ALWAYS, "Unable to update %s! Aborting.\n",
				 ATTR_TRANSFER_INPUT_FILES );
		shutDown( JOB_NOT_STARTED );
	}
}

void
MPIShadow::hackComradeAd( char *comradeArgs, ClassAd *ad )
{

/* Args are in form:
   exec_machine -l username -n executable master_machine port -p4amslave

   We only want the stuff after the executable...
   executable master_machine port -p4amslave
*/

	ArgList args;
	MyString args_error;
    char *tmparg;

        // we expect the executable name to be "condor_exec"
    if ( !(tmparg = strstr( comradeArgs, "condor_exec" )) ) {
        dprintf ( D_ALWAYS, "No \"condor_exec\" found in comradeArgs!\n" );
        dprintf ( D_ALWAYS, "Comrade Args: %s\n", comradeArgs );
        shutDown( JOB_NOT_STARTED );
    }
	if(!args.AppendArgsV1Raw(&tmparg[12],&args_error)) {
		dprintf(D_ALWAYS, "Failed to insert comradArgs! %s\n",
				args_error.Value());
		shutDown( JOB_NOT_STARTED );
	}

	if(!args.AppendArgsFromClassAd(ad,&args_error)) {
        dprintf ( D_ALWAYS, "Failed to get Job args in hackComradeAd: %s\n",
				  args_error.Value());
        shutDown( JOB_NOT_STARTED );
    }

    free( comradeArgs );

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string);
    dprintf ( D_FULLDEBUG, "Inserting args: %s\n", args_string.Value() );

	if(!args.InsertArgsIntoClassAd(ad,NULL,&args_error)) {
        dprintf ( D_ALWAYS, "Failed to insert Job args! %s\n",
				  args_error.Value() );
        shutDown( JOB_NOT_STARTED );
    }
}

#else /* ! MPI_USES_RSH */

void
MPIShadow::spawnAllComrades( void )
{
		/* 
		   If this function is being called, we've already spawned the
		   root node and gotten its ip/port from our special pseudo
		   syscall.  So, all we have to do is loop over our remote
		   resource list, modify each resource's job ad with the
		   appropriate info, and spawn our node on each resource.
		*/

    MpiResource *rr;
	int last = ResourceList.getlast();
	while( nextResourceToStart <= last ) {
        rr = ResourceList[nextResourceToStart];
		modifyNodeAd( rr->getJobAd() );
		spawnNode( rr );  // This increments nextResourceToStart 
    }
	ASSERT( nextResourceToStart == numNodes );
}


bool
MPIShadow::modifyNodeAd( ClassAd* ad )
{
		/*
		  This function has to set 3 environment variables which need
		  to be set whether we're the root or a comrade node:

		  MPICH_JOBID - Job id string
		  MPICH_IPROC - The rank number of this node
		  MPICH_NPROC - The # of total ranks

		  In addition, if we're a comrade, we also need to set:

		  MPICH_ROOT  - host or ip and port of the root node.

		  If we're the root node, we need to set MPICH_ROOT, but we
		  just want to use the hostname of the root node, not
		  specifying a port, since we'll let the master bind to
		  whatever it can and tell us where it's listening via the
		  MPICH_EXTRA env var.
		*/

	Env env;
	MyString env_errors;
	if( !env.MergeFrom(ad,&env_errors) ) {
		dprintf( D_ALWAYS, 
				 "ERROR: cannot get environment from job ad: %s\n", 
				 env_errors.Value() );
		shutDown( JOB_NOT_STARTED );
		return false;
	}

		// Now, add all the MPICH-specific variables.

		// NPROC is easy, since numNodes already holds the total
		// number of nodes we're going to spawn
	char numNodesString[127];
	sprintf(numNodesString,"%d",numNodes);
	env.SetEnv("MPICH_NPROC",numNodesString);

		// We need the delimiter for all the rest of them... 

		// The value we should use for MPICH_JOBID is held in
		// mpich_jobid, since we want that to be constant across all
		// nodes we spawn, and we can just compute it once when we
		// start up and reuse it.
	env.SetEnv("MPICH_JOBID",mpich_jobid);

		// Conveniently, nextResourceToStart always holds the right
		// value for IPROC, since that's what we use to keep track of
		// what node we're spawning...
	char nextResourceToStartStr[127];
	sprintf(nextResourceToStartStr,"%d",nextResourceToStart);
	env.SetEnv("MPICH_IPROC",nextResourceToStartStr);

		// Now, if we're a comrade (rank > 0), we also need to add
		// MPICH_ROOT, which is what we got from our pseudo syscall. 
	if( nextResourceToStart > 0 ) {
		env.SetEnv("MPICH_ROOT",master_addr);
	} else {
			// For the root node, we need to set MPICH_ROOT just to
			// the hostname of the resource where it's executing
		char* sinful = NULL;
		ResourceList[0]->getStartdAddress( sinful );
			// Now, we've got a sinful string, so, parse out the ip
			// address of it.
		char* foo;
		foo = strchr( sinful, ':' );
		if( foo ) {
			*foo = '\0';
		} else {
			dprintf( D_ALWAYS, 
					 "ERROR: can't parse sinful string (%s) of root node!\n", 
					 sinful );
			delete [] sinful;
			shutDown( JOB_NOT_STARTED );
		}
		env.SetEnv("MPICH_ROOT",&sinful[1]);
		delete [] sinful;
	}

		// Now, the env contains the modified environment
		// attribute, so we just need to re-insert that into our ad. 
	if(!env.InsertEnvIntoClassAd(ad,&env_errors)) {
		dprintf(D_ALWAYS,"ERROR: failed to update job environment: %s\n",
				env_errors.Value());
		shutDown( JOB_NOT_STARTED );
		return false;
	}
	return true;
}

#endif /* MPI_USES_RSH */


void 
MPIShadow::spawnNode( MpiResource* rr )
{
		// First, contact the startd to spawn the job
    if( rr->activateClaim() != OK ) {
        shutDown( JOB_NOT_STARTED );
    }

    dprintf ( D_PROTOCOL, "Just requested resource for node %d\n",
			  nextResourceToStart );

	nextResourceToStart++;
}


void 
MPIShadow::cleanUp( void )
{
        // unlink the procgroup file:
    char pgfilename[512];
    sprintf( pgfilename, "%s/procgroup.%d.%d", getIwd(), getCluster(), 
			 getProc() );
    if( unlink( pgfilename ) == -1 ) {
        if( errno != ENOENT ) {
            dprintf( D_ALWAYS, "Problem removing %s: errno %d.\n", 
					 pgfilename, errno );
        }
    }

		// kill all the starters
	MpiResource *r;
	int i;
    for( i=0 ; i<=ResourceList.getlast() ; i++ ) {
		r = ResourceList[i];
		r->killStarter();
	}		
}


void 
MPIShadow::gracefulShutDown( void )
{
	cleanUp();
}


void
MPIShadow::emailTerminateEvent( int exitReason, update_style_t kind )
{
	int i;
	FILE* mailer;
	Email msg;
	MyString str;
	char *s;

	mailer = msg.open_stream( jobAd, exitReason );
	if( ! mailer ) {
			// nothing to do
		return;
	}

	fprintf( mailer, "Your Condor-MPI job %d.%d has completed.\n", 
			 getCluster(), getProc() );

	fprintf( mailer, "\nHere are the machines that ran your MPI job.\n");

	if (kind == US_TERMINATE_PENDING) {
		fprintf( mailer, "    Machine Name         \n" );
		fprintf( mailer, " ------------------------\n" );

		// Don't print out things like the exit codes since they have
		// been lost and it is a little too much work to add them into
		// the jobad for the amount of time I have to get this working.
		// This should be a more rare case in any event.

		jobAd->LookupString(ATTR_REMOTE_HOSTS, str);
		StringList slist(str.Value());
		slist.rewind();
		while((s = slist.next()) != NULL)
		{
			fprintf( mailer, "%s\n", s);

		}

		fprintf( mailer, "\nExit codes are currently unavailable.\n\n");
		fprintf( mailer, "\nHave a nice day.\n" );

		return;

	}

	fprintf( mailer, "They are listed in the order they were started\n" );
	fprintf( mailer, "in, which is the same as MPI_Comm_rank.\n\n" );
	
	fprintf( mailer, "    Machine Name               Result\n" );
	fprintf( mailer, " ------------------------    -----------\n" );

	int allexitsone = TRUE;
	int exit_code;
	for ( i=0 ; i<=ResourceList.getlast() ; i++ ) {
		(ResourceList[i])->printExit( mailer );
		exit_code = (ResourceList[i])->exitCode();
		if( exit_code != 1 ) {
			allexitsone = FALSE;
		}
	}

	if ( allexitsone ) {
		fprintf ( mailer, "\nCondor has noticed that all of the " );
		fprintf ( mailer, "processes in this job \nhave an exit status " );
		fprintf ( mailer, "of 1.  This *might* be the result of a core\n");
		fprintf ( mailer, "dump.  Condor can\'t tell for sure - the " );
		fprintf ( mailer, "MPICH library catches\nSIGSEGV and exits" );
		fprintf ( mailer, "with a status of one.\n" );
	}

	fprintf( mailer, "\nHave a nice day.\n" );
	
}


void 
MPIShadow::shutDown( int exitReason )
{
		/* With many resources, we have to figure out if all of
		   them are done, and we have to figure out if we need
		   to kill others.... */
	if( !shutDownLogic( exitReason ) ) {
		return;  // leave if we're not *really* ready to shut down.
	}

		/* if we're still here, we can call BaseShadow::shutDown() to
		   do the real work, which is shared among all kinds of
		   shadows.  the shutDown process will call other virtual
		   functions to get universe-specific goodness right. */
	BaseShadow::shutDown( exitReason );
}


int 
MPIShadow::shutDownLogic( int& exitReason ) {

		/* What sucks for us here is that we know we want to shut 
		   down, but we don't know *why* we are shutting down.
		   We have to look through the set of MpiResources
		   and figure out which have exited, how they exited, 
		   and if we should kill them all... Basically, the only
		   time we *don't* remove everything is when all the 
		   resources have exited normally.  */

	dprintf( D_FULLDEBUG, "Entering shutDownLogic(r=%d)\n", 
			 exitReason );

		/* if we have a 'pre-startup' exit reason, we can just
		   dupe that to all resources and exit right away. */
	if ( exitReason == JOB_NOT_STARTED  ||
		 exitReason == JOB_SHADOW_USAGE ) {
		for ( int i=0 ; i<ResourceList.getlast() ; i++ ) {
			(ResourceList[i])->setExitReason( exitReason );
		}
		return TRUE;
	}

		/* Now we know that *something* started... */
	
	int normal_exit = FALSE;

		/* If the job on the resource has exited normally, then
		   we don't want to remove everyone else... */
	if( (exitReason == JOB_EXITED) && !(exitedBySignal()) ) {
		dprintf( D_FULLDEBUG, "Normal exit\n" );
		normal_exit = TRUE;
	}

	if ( (!normal_exit) && (!shutDownMode) ) {
		/* We get here and try to shut everyone down.  Don't worry;
		   this function will only fire once. */
		handleJobRemoval( 666 );

		actualExitReason = exitReason;
		shutDownMode = TRUE;
	}

		/* We now have to figure out if everyone has finished... */
	
	int alldone = TRUE;
	MpiResource *r;

    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
		r = ResourceList[i];
		char *res = NULL;
		r->getMachineName( res );
		dprintf( D_FULLDEBUG, "Resource %s...%13s %d\n", res,
				 rrStateToString(r->getResourceState()), 
				 r->getExitReason() );
		delete [] res;
		switch ( r->getResourceState() )
		{
			case RR_PENDING_DEATH:
				alldone = FALSE;  // wait for results to come in, and
			case RR_FINISHED:
				break;            // move on...
			case RR_PRE: {
					// what the heck is going on? - shouldn't happen.
				r->setExitReason( JOB_NOT_STARTED );
				break;
			}
		    case RR_STARTUP:
			case RR_EXECUTING: {
				if ( !normal_exit ) {
					r->killStarter();
				}
				alldone = FALSE;
				break;
			}
			default: {
				dprintf ( D_ALWAYS, "ERROR: Don't know state %d\n", 
						  r->getResourceState() );
			}
		} // switch()
	} // for()

	if ( (!normal_exit) && shutDownMode ) {
		/* We want the exit reason  to be set to the exit
		   reason of the job that caused us to shut down.
		   Therefore, we set this here: */
		exitReason = actualExitReason;
	}

	if ( alldone ) {
			// everyone has reported in their exit status...
		dprintf( D_FULLDEBUG, "All nodes have finished, ready to exit\n" );
		return TRUE;
	}

	return FALSE;
}

int 
MPIShadow::handleJobRemoval( int sig ) {

    dprintf ( D_FULLDEBUG, "In handleJobRemoval, sig %d\n", sig );
	remove_requested = true;

	ResourceState s;

    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
		s = ResourceList[i]->getResourceState();
		if( s == RR_EXECUTING || s == RR_STARTUP ) {
			ResourceList[i]->setExitReason( JOB_KILLED );
			ResourceList[i]->killStarter();
		}
    }

	return 0;
}

/* This is basically a search-and-replace "#MpInOdE#" with a number 
   for that node...so we can address each mpi node in the submit file. */
void
MPIShadow::replaceNode ( ClassAd *ad, int nodenum ) {

	ExprTree *tree = NULL;
	char node[9];
	const char *lhstr, *rhstr;

	sprintf( node, "%d", nodenum );

	ad->ResetExpr();
	while( ad->NextExpr(lhstr, tree) ) {
		rhstr = ExprTreeToString(tree);
		if( !lhstr || !rhstr ) {
			dprintf( D_ALWAYS, "Could not replace $(NODE) in ad!\n" );
			return;
		}

		MyString strRh(rhstr);
		if (strRh.replaceString("#MpInOdE#", node))
		{
			ad->AssignExpr(lhstr, strRh.Value());
			dprintf( D_FULLDEBUG, "Replaced $(NODE), now using: %s = %s\n", 
					 lhstr, strRh.Value() );
		}
	}	
}


int
MPIShadow::updateFromStarter(int /* command */, Stream *s)
{
	ClassAd update_ad;
	s->decode();
	update_ad.initFromStream(*s);
	s->end_of_message();
	return updateFromStarterClassAd(&update_ad);
}


int
MPIShadow::updateFromStarterClassAd(ClassAd* update_ad)
{
	MpiResource* mpi_res = NULL;
	int mpi_node = -1;
	
		// First, figure out what remote resource this info belongs to. 
	if( ! update_ad->LookupInteger(ATTR_NODE, mpi_node) ) {
			// No ATTR_NODE in the update ad!
		dprintf( D_ALWAYS, "ERROR in MPIShadow::updateFromStarter: "
				 "no %s defined in update ad, can't process!\n",
				 ATTR_NODE );
		return FALSE;
	}
	if( ! (mpi_res = findResource(mpi_node)) ) {
		dprintf( D_ALWAYS, "ERROR in MPIShadow::updateFromStarter: "
				 "can't find remote resource for node %d, "
				 "can't process!\n", mpi_node );
		return FALSE;
	}

		// Now, we're in good shape.  Grab all the info we care about
		// and put it in the appropriate place.
	mpi_res->updateFromStarter(update_ad);

		// XXX TODO: Do we want to update our local job ad?  Do we
		// want to store the maximum in there?  Seperate stuff for
		// each node?  

	return TRUE;
}


MpiResource*
MPIShadow::findResource( int node )
{
	MpiResource* mpi_res;
	int i;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		if( node == mpi_res->getNode() ) {
			return mpi_res;
		}
	}
	return NULL;
}


struct rusage
MPIShadow::getRUsage( void ) 
{
	MpiResource* mpi_res;
	struct rusage total;
	struct rusage cur;
	int i;
	memset( &total, 0, sizeof(struct rusage) );
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		cur = mpi_res->getRUsage();
		total.ru_stime.tv_sec += cur.ru_stime.tv_sec;
		total.ru_utime.tv_sec += cur.ru_utime.tv_sec;
	}
	return total;
}


int
MPIShadow::getImageSize( void )
{
	MpiResource* mpi_res;
	int i, max = 0, val;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		val = mpi_res->getImageSize();
		if( val > max ) {
			max = val;
		}
	}
	return max;
}


int
MPIShadow::getDiskUsage( void )
{
	MpiResource* mpi_res;
	int i, max = 0, val;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		val = mpi_res->getDiskUsage();
		if( val > max ) {
			max = val;
		}
	}
	return max;
}


float
MPIShadow::bytesSent( void )
{
	MpiResource* mpi_res;
	int i;
	float total = 0;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		total += mpi_res->bytesSent();
	}
	return total;
}


float
MPIShadow::bytesReceived( void )
{
	MpiResource* mpi_res;
	int i;
	float total = 0;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		total += mpi_res->bytesReceived();
	}
	return total;
}

int
MPIShadow::getExitReason( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->getExitReason();
	}
	return -1;
}

bool
MPIShadow::claimIsClosing( void )
{
	return false;
}


bool
MPIShadow::setMpiMasterInfo( char* str )
{
#if ! MPI_USES_RSH
	if( master_addr ) {
		free( master_addr );
	}
	master_addr = strdup( str );

		// now that we know this, we can set a timer to actually start 
		// spawning all the comrade nodes.  we do this with a timer so
		// that we can quickly complete the pseudo syscall we're in
		// the middle of and return to DaemonCore ASAP...
	int tid;
	tid = daemonCore->
		Register_Timer( 0, 0,
						(TimerHandlercpp)&MPIShadow::spawnAllComrades,  
						"MPIShadow::spawnAllComrades", this ); 
	if( tid < 0 ) {
		EXCEPT( "Can't register DaemonCore Timer!" );
	}
	return true;
#else /* ! MPI_USES_RSH */
		// Shut the compiler up
	str = str;

	return false;
#endif /* ! MPI_USES_RSH */
}


bool
MPIShadow::exitedBySignal( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitedBySignal();
	}
	return false;
}


int
MPIShadow::exitSignal( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitSignal();
	}
	return -1;
}


int
MPIShadow::exitCode( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitCode();
	}
	return -1;
}


void
MPIShadow::resourceBeganExecution( RemoteResource* rr )
{
	bool all_executing = true;

	int i;
	for( i=0; i<=ResourceList.getlast() && all_executing ; i++ ) {
		if( ResourceList[i]->getResourceState() != RR_EXECUTING ) {
			all_executing = false;
		}
	}

	if( all_executing ) {
			// All nodes in this computation are now running, so we 
			// can finally log the execute event.
		ExecuteEvent event;
		strcpy( event.executeHost, "MPI_job" );
		if ( !uLog.writeEvent( &event, jobAd )) {
			dprintf ( D_ALWAYS, "Unable to log EXECUTE event." );
		}
		
			// Now that everything is started, we can finally invoke
			// the base copy of this function to handle shared code.
		BaseShadow::resourceBeganExecution(rr);
	}
}


void
MPIShadow::resourceReconnected( RemoteResource* /* rr */ )
{
	EXCEPT( "impossible: MPIShadow doesn't support reconnect" );
}


void
MPIShadow::logDisconnectedEvent( const char* /* reason */ )
{
	EXCEPT( "impossible: MPIShadow doesn't support reconnect" );
}


void
MPIShadow::logReconnectedEvent( void )
{
	EXCEPT( "impossible: MPIShadow doesn't support reconnect" );
}


void
MPIShadow::logReconnectFailedEvent( const char* /* reason */ )
{
	EXCEPT( "impossible: MPIShadow doesn't support reconnect" );
}
