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
#include "mpishadow.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.
#include "list.h"                // List class
#include "internet.h"            // sinful->hostname stuff


MPIShadow::MPIShadow() {
    nextResourceToStart = 0;
	numNodes = 0;
    shutDownMode = FALSE;
    ResourceList.fill(NULL);
    ResourceList.truncate(-1);
	actualExitReason = -1;
	actualExitStatus = -1;    
	info_tid = -1;
}

MPIShadow::~MPIShadow() {
        // Walk through list of Remote Resources.  Delete all.
    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
        delete ResourceList[i];
    }
}

void 
MPIShadow::init( ClassAd *jobAd, char schedd_addr[], char host[], 
                 char capability[], char cluster[], char proc[])
{

    if ( !jobAd ) {
        EXCEPT( "No jobAd defined!" );
    }

        // BaseShadow::baseInit - basic init stuff...
    baseInit( jobAd, schedd_addr, cluster, proc );

		// Register command which gets updates from the starter
		// on the job's image size, cpu usage, etc.  Each kind of
		// shadow implements it's own version of this to deal w/ it
		// properly depending on parallel vs. serial jobs, etc. 
	daemonCore->
		Register_Command( SHADOW_UPDATEINFO, "SHADOW_UPDATEINFO", 
						  (CommandHandlercpp)&MPIShadow::updateFromStarter,
						  "MPIShadow::updateFromStarter", this, WRITE ); 

        /* Register Command for sneaky rsh: */
	daemonCore->Register_Command( MPI_START_COMRADE, "MPI_START_COMRADE", 
		 (CommandHandlercpp)&MPIShadow::startComrade, "startComrade", 
		 this, WRITE );

        // make first remote resource the "master".  Put it first in list.
    MpiResource *rr = new MpiResource( this );
    rr->setExecutingHost ( host );
    rr->setCapability ( capability );
    rr->setMachineName ( "Unknown" );
    ClassAd *temp = new ClassAd( *(getJobAd() ) );

    char buf[128];
    sprintf ( buf, "%s = %s", ATTR_MPI_IS_MASTER, "TRUE" );
    if ( !temp->Insert( buf )) {
        dprintf ( D_ALWAYS, "Failed to insert %s into jobAd.\n", buf );
        shutDown ( JOB_NOT_STARTED, 0 );
    }

	replaceNode ( temp, 0 );
	rr->setNode( 0 );
	sprintf( buf, "%s = 0", ATTR_NODE );
	temp->InsertOrUpdate( buf );
    rr->setJobAd( temp );

    ResourceList[ResourceList.getlast()+1] = rr;

		/*
		  Finally, register a timer to call getResources(), which
		  sends a command to the schedd to get all the job classads,
		  startd sinful strings, and capabilities for all the matches
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
    char *capability = NULL;
    MpiResource *rr;
	int cluster;
	char buf[128];

    int numProcs=0;    // the # of procs to come
    int numInProc=0;   // the # in a particular proc.
	ClassAd *job_ad = NULL;
	ClassAd *tmp_ad = NULL;
	int nodenum = 1;
	ReliSock sock;
	int cmd = GIVE_MATCHES;

	cluster = getCluster();
    rr = ResourceList[0];
	rr->getCapability( capability );

	sock.timeout(20);

		// First, contact the schedd and send the command, the
		// cluster, and the capability

	if( ! sock.connect( getScheddAddr() ) ) {
		EXCEPT( "Can't connect to schedd at %s", getScheddAddr() );
	}
		
	sock.encode();
	if( ! sock.code(cmd) ) {
		EXCEPT( "Can't send cmd (%d) to schedd", cmd );
	}
	if( ! sock.code(cluster) ) {
		EXCEPT( "Can't send cluster (%d) to schedd\n", cluster );
	}
	if( ! sock.code(capability) ) {
		EXCEPT( "Can't send capability to schedd\n" );
	}

		// Now that we sent this, free the memory that was allocated
		// with getCapability() above
	delete [] capability;
	capability = NULL;

	if( ! sock.end_of_message() ) {
		EXCEPT( "Can't send EOM to schedd\n" );
	}
	
		// Ok, that's all we need to send, now we can read the data
		// back off the wire
	sock.decode();

        // We first get the number of proc classes we'll be getting.
    if ( !sock.code( numProcs ) ) {
        EXCEPT( "Failed to get number of procs" );
    }

        /* Now, do stuff for each proc: */
    for ( int i=0 ; i<numProcs ; i++ ) {
		job_ad = new ClassAd();
		if( !job_ad->get(sock)  ) {
            EXCEPT( "Failed to get job classad for proc %d", i );
		}
        if( !sock.code( numInProc ) ) {
            EXCEPT( "Failed to get number of matches in proc %d", i );
        }

        dprintf ( D_FULLDEBUG, "Got %d matches for proc # %d\n",
				  numInProc, i );

        for ( int j=0 ; j<numInProc ; j++ ) {
            if ( !sock.code( host ) ||
                 !sock.code( capability ) ) {
                EXCEPT( "Problem getting resource %d, %d", i, j );
            }
            dprintf ( D_FULLDEBUG, "Got host: %s   cap: %s\n",host,capability);
            
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
                free( capability );
                host = NULL;
                capability = NULL;
                continue;
            }

            rr = new MpiResource( this );
            rr->setExecutingHost ( host );
            rr->setCapability ( capability );
            rr->setMachineName ( "Unknown" );
			tmp_ad = new ClassAd ( *job_ad );
			replaceNode ( tmp_ad, nodenum );
			rr->setNode( nodenum );
			sprintf( buf, "%s = %d", ATTR_NODE, nodenum );
			tmp_ad->InsertOrUpdate( buf );
			sprintf( buf, "%s = \"%s\"", ATTR_MY_ADDRESS,
					 daemonCore->InfoCommandSinfulString() );
			tmp_ad->InsertOrUpdate( buf );
			rr->setJobAd( tmp_ad );
			nodenum++;

            ResourceList[ResourceList.getlast()+1] = rr;

                /* free stuff so next code() works correctly */
            free( host );
            free( capability );
            host = NULL;
            capability = NULL;

        } // end of for loop for this proc
        
		delete job_ad;
		job_ad = NULL;

    } // end of for loop on all procs...

    sock.end_of_message();

	numNodes = nodenum;  // for later use...

    dprintf ( D_PROTOCOL, "#1 - Shadow started; %d machines gotten.\n", 
			  numNodes );

    startMaster();

    return TRUE;
}


void
MPIShadow::startMaster()
{
    dprintf ( D_FULLDEBUG, "In MPIShadow::startMaster()\n" );

    MpiResource *rr;
    char mach[128];
    char *sinful = new char[128];
    struct sockaddr_in sin;
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
    sprintf ( pgfilename, "%s/procgroup.%d.%d", getIwd(), getCluster(), getProc() );
    if ( (pg=fopen( pgfilename, "w" )) == NULL ) {
        dprintf (D_ALWAYS, "Failure to open %s for writing, errno %d\n", 
                 pgfilename, errno );
        shutdown( JOB_NOT_STARTED, 0 );
    }
        
        // get the machine name (using string_to_sin and sin_to_hostname)
    rr = ResourceList[0];
    rr->getExecutingHost( sinful );
    string_to_sin( sinful, &sin );
    sprintf( mach, "%s", sin_to_hostname( &sin, NULL ));
    fprintf( pg, "%s 0 condor_exec %s\n", mach, getOwner() );

    dprintf ( D_FULLDEBUG, "Procgroup file:\n" );
    dprintf ( D_FULLDEBUG, "%s 0 condor_exec %s\n", mach, getOwner() );

        // for each resource, get machine name, make pgfile entry
    for ( int i=1 ; i<=ResourceList.getlast() ; i++ ) {
        rr = ResourceList[i];
        rr->getExecutingHost( sinful );
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
        fclose( pg );
        shutDown( JOB_NOT_STARTED, 0 );
    }
#endif

    if ( fclose( pg ) == EOF ) {
        dprintf ( D_ALWAYS, "fclose failed!  errno = %d\n", errno );
        shutDown( JOB_NOT_STARTED, 0 );
    }

        // back to master resource:
    rr = ResourceList[0];

        // alter the master's args...
    hackMasterAd( rr->getJobAd() );

        // yak with this startd.  Others will come in startComrade.
    if ( rr->requestIt() == -1 ) {
        shutDown( JOB_NOT_STARTED, 0 );
    }

		// Register the master's claimSock for remote
		// system calls.  It's a bit funky, but works.
	daemonCore->Register_Socket(rr->getClaimSock(), "RSC Socket", 
       (SocketHandlercpp)&MpiResource::handleSysCalls,"HandleSyscalls",rr);

    nextResourceToStart++;

    dprintf ( D_PROTOCOL, "#3 - Just requested Master resource.\n" );

        /* Now the sneaky rsh intercepts the master's call to rsh, 
           and sends stuff to startComrade()... */
}

int
MPIShadow::startComrade( int cmd, Stream* s )
{
/* This command made by sneaky rsh:  condor_starter.V6.1/condor_rsh.C */

    dprintf ( D_FULLDEBUG, "Getting information for a comrade node\n" );
    
    char *comradeArgs = NULL;
    if ( !s->code( comradeArgs ) ||
         !s->end_of_message() )
    {
        dprintf ( D_ALWAYS, "Failed to receive comrade args!" );
        shutDown( JOB_NOT_STARTED, 0 );
    }

    dprintf ( D_PROTOCOL, "#8 - Received args from sneaky rsh\n" );
    dprintf ( D_FULLDEBUG, "Comrade args: %s\n", comradeArgs );

    MpiResource *rr = ResourceList[nextResourceToStart++];
    
        // modify this comrade's arguments...using the comradeArgs given.
    hackComradeAd( comradeArgs, rr->getJobAd() );

    dprintf ( D_PROTOCOL, "#9 - Added args to jobAd, now requesting:\n" );

        // yak with this startd; 
    if ( rr->requestIt() == -1 ) {
        shutDown( JOB_NOT_STARTED, 0 );
    }

		// Register the remote instance's claimSock for remote
		// system calls.  It's a bit funky, but works.
	daemonCore->Register_Socket(rr->getClaimSock(), "RSC Socket", 
       (SocketHandlercpp)&MpiResource::handleSysCalls,"HandleSyscalls",rr);
    
    dprintf ( D_PROTOCOL, "#10 - Just requested Comrade resource\n" );

	if ( nextResourceToStart == numNodes ) {
			/* This is the last node we're starting...make an execute event. */
		ExecuteEvent event;
		strcpy( event.executeHost, "MPI_job" );
		if ( !uLog.writeEvent( &event )) {
			dprintf ( D_ALWAYS, "Unable to log EXECUTE event." );
		}
	}

    return TRUE;
}

void 
MPIShadow::hackMasterAd( ClassAd *ad )
{
/* simple:  get args, add -p4pg (and more...), put args back in */

	char args[_POSIX_ARG_MAX];
	if ( !ad->LookupString( ATTR_JOB_ARGUMENTS, args )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ARGUMENTS );
		shutDown( JOB_NOT_STARTED, 0 );
	}
    
    char tmp[_POSIX_ARG_MAX];

    sprintf ( tmp, "%s = \"-p4pg procgroup.%d.%d %s\"", 
              ATTR_JOB_ARGUMENTS, getCluster(), getProc(), args );

    ad->Delete( ATTR_JOB_ARGUMENTS );

	if ( !ad->Insert( tmp )) {
		dprintf( D_ALWAYS, "Unable to update args! Aborting.\n" );
		shutDown( JOB_NOT_STARTED, 0 );
	}

		// While we're at it, if the job wants files transfered,
		// include the procgroup file in the list of input files.
		// This is only needed on the master.
	if( !ad->LookupString(ATTR_TRANSFER_FILES, args) ) {
			// Nothing, we're done.
		return;
	}
		// Ok, we found it.  If it's set to anything other than
		// "Never", we need to do our work.
	if( args[0] == 'n' || args[0] == 'N' ) {
			// It's "never", we're done.
		return;
	}

		// Now, see if they already gave us a list.
	if( !ad->LookupString(ATTR_TRANSFER_INPUT_FILES, args) ) {
			// Nothing here, so we can safely add it ourselves. 
		sprintf( tmp, "%s = \"procgroup.%d.%d\"",
				 ATTR_TRANSFER_INPUT_FILES, getCluster(), getProc() ); 
	} else {
			// There's a list already.  We've got to append to it. 
		sprintf( tmp, "%s = \"%s, procgroup.%d.%d\"",
				 ATTR_TRANSFER_INPUT_FILES, args, getCluster(),
				 getProc() );

	}
	dprintf( D_FULLDEBUG, "About to append to job ad: %s\n", tmp );
	if ( !ad->Insert( tmp )) {
		dprintf( D_ALWAYS, "Unable to update %s! Aborting.\n",
				 ATTR_TRANSFER_INPUT_FILES );
		shutDown( JOB_NOT_STARTED, 0 );
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

    char newargs[_POSIX_ARG_MAX];
    char *tmparg;
        // we expect the executable name to be "condor_exec"
    if ( !(tmparg = strstr( comradeArgs, "condor_exec" )) ) {
        dprintf ( D_ALWAYS, "No \"condor_exec\" found in comradeArgs!\n" );
        dprintf ( D_ALWAYS, "Comrade Args: %s\n", comradeArgs );
        shutDown( JOB_NOT_STARTED, 0 );
    }
    sprintf( newargs, "%s", &tmparg[12] );
    
    char args[2048];
    if ( !ad->LookupString( ATTR_JOB_ARGUMENTS, args )) {
        dprintf ( D_ALWAYS, "Failed to get Job args in hackComradeAd!\n" );
        shutDown( JOB_NOT_STARTED, 0 );
    }

    char tmp[2048];
    sprintf( tmp, "%s = \"%s %s\"", 
			 ATTR_JOB_ARGUMENTS, newargs, args );
    free( comradeArgs );

    dprintf ( D_FULLDEBUG, "Inserting args: %s\n", tmp );

    ad->Delete( ATTR_JOB_ARGUMENTS );

    if ( !ad->Insert( tmp )) {
        dprintf ( D_ALWAYS, "Failed to insert Job args!\n" );
        shutDown( JOB_NOT_STARTED, 0 );
    }
}

void 
MPIShadow::shutDown( int exitReason, int exitStatus ) {

		/* With many resources, we have to figure out if all of
		   them are done, and we have to figure out if we need
		   to kill others.... */
	int i;

	if ( !shutDownLogic( exitReason, exitStatus ) ) {
		return;  // leave if we're not *really* ready to shut down.
	}

        // unlink the procgroup file:
    char pgfilename[128];
    sprintf( pgfilename, "%s/procgroup.%d.%d", getIwd(), getCluster(), getProc() );
    if ( unlink( pgfilename ) == -1 ) {
        if ( errno != ENOENT ) {
            dprintf ( D_ALWAYS, "Problem removing %s: errno %d.\n", 
                      pgfilename, errno );
        }
    }

		/* write a terminate event to the user log. */
	endingUserLog( exitStatus, exitReason );

	// As we leave, we need to deactivate the claim.
	// Does this go here, or should it go earlier
	// does killing the starter
	MpiResource *r;
    for ( i=0 ; i<=ResourceList.getlast() ; i++ ) {
		r = ResourceList[i];
		char *tmp = NULL;
		r->getExecutingHost( tmp );
		dprintf(D_FULLDEBUG, "Killing the starter on %s\n",tmp);
		r->killStarter();
		delete [] tmp;
	}		

	MpiResource *rr;
    if ( getJobAd() ) {
            // For lack of anything better to do here, set the last 
            // executing host to the host the master was on.
        char *tmp = NULL;
        rr = ResourceList[0];
        if ( rr ) {
            rr->getExecutingHost( tmp );
			if (!ConnectQ(getScheddAddr(), SHADOW_QMGMT_TIMEOUT)) {
				EXCEPT("Failed to connect to schedd!");
			}
            DeleteAttribute( getCluster(), getProc(), ATTR_REMOTE_HOST );
            SetAttributeString( getCluster(), getProc(), 
                                ATTR_LAST_REMOTE_HOST, tmp );
            DisconnectQ( NULL );
            delete [] tmp;
        } else {
            DC_Exit( exitReason );
        }
    } else {
        DC_Exit( exitReason );
    }


		// if we are being called from the exception handler, return
		// now.
	if ( exitReason == JOB_EXCEPTION ) {
		return;
	}

		// returns a mailer if desired
	FILE* mailer;
	mailer = shutDownEmail( exitReason, exitStatus );
	if ( mailer ) {
		fprintf( mailer, "Your Condor-MPI job %d.%d has completed.\n", 
				 getCluster(), getProc() );

		fprintf( mailer, "\nHere are the machines that ran your MPI job.\n");
		fprintf( mailer, "They are listed in the order they were started\n" );
		fprintf( mailer, "in, which is the same as MPI_Comm_rank.\n\n" );
		
		fprintf( mailer, "    Machine Name               Result\n" );
		fprintf( mailer, " ------------------------    -----------\n" );

		int allexitsone = TRUE;
		int status;
		for ( i=0 ; i<=ResourceList.getlast() ; i++ ) {
			(ResourceList[i])->printExit( mailer );
			status = (ResourceList[i])->getExitStatus();
			if ( WEXITSTATUS(status) != 1 ) {
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

		email_close(mailer);
	}
    
		// does not return.
	DC_Exit( exitReason );
}

int 
MPIShadow::shutDownLogic( int& exitReason, int& exitStatus ) {

		/* What sucks for us here is that we know we want to shut 
		   down, but we don't know *why* we are shutting down.
		   We have to look through the set of MpiResources
		   and figure out which have exited, how they exited, 
		   and if we should kill them all... Basically, the only
		   time we *don't* remove everything is when all the 
		   resources have exited normally.  */

	dprintf ( D_FULLDEBUG, "Entering shutDownLogic(r=%d, s=0x%x).\n",
			  exitReason, exitStatus );

		/* if we have a 'pre-startup' exit reason, we can just
		   dupe that to all resources and exit right away. */
	if ( exitReason == JOB_NOT_STARTED  ||
		 exitReason == JOB_SHADOW_USAGE ) {
		for ( int i=0 ; i<ResourceList.getlast() ; i++ ) {
			(ResourceList[i])->setExitReason( exitReason );
			(ResourceList[i])->setExitStatus( exitStatus );
		}
		return TRUE;
	}

		/* Now we know that *something* started... */
	
	int normal_exit = FALSE;

		/* If the job on the resource has exited normally, then
		   we don't want to remove everyone else... */
	if ( (exitReason == JOB_EXITED) && (WIFEXITED(exitStatus)) )  {
		dprintf ( D_FULLDEBUG, "Normal exit\n" );
		normal_exit = TRUE;
	}

	if ( (!normal_exit) && (!shutDownMode) ) {
		/* We get here and try to shut everyone down.  Don't worry;
		   this function will only fire once. */
		handleJobRemoval( 666 );

		actualExitReason = exitReason;
		actualExitStatus = exitStatus;
		shutDownMode = TRUE;
	}

		/* We now have to figure out if everyone has finished... */
	
	int alldone = TRUE;
	MpiResource *r;

    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
		r = ResourceList[i];
		char *res = NULL;
		r->getMachineName( res );
		dprintf ( D_FULLDEBUG, "Resource %s...%13s %d 0x%x\n", res,
				  Resource_State_String[r->getResourceState()], 
				  r->getExitReason(), r->getExitStatus() );
		delete [] res;
		switch ( r->getResourceState() )
		{
			case PENDING_DEATH:
				alldone = FALSE;  // wait for results to come in, and
			case FINISHED:
				break;            // move on...
			case PRE: {
					// what the heck is going on? - shouldn't happen.
				r->setExitStatus( 0 );
				r->setExitReason( JOB_NOT_STARTED );
				break;
			}
			case EXECUTING: {
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
		/* We want the exit reason and status to be set to the exit
		   reason and status of the job that caused us to shut down.
		   Therefore, we set these here: */
		exitReason = actualExitReason;
		exitStatus = actualExitStatus;
	}

	if ( alldone ) {
			// everyone has reported in their exit status...
		dprintf ( D_FULLDEBUG, "We're really ready to exit!\n" );
		return TRUE;
	}

	return FALSE;
}

int 
MPIShadow::handleJobRemoval( int sig ) {

    dprintf ( D_FULLDEBUG, "In handleJobRemoval, sig %d\n", sig );

    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
		if ( (ResourceList[i]->getResourceState() == EXECUTING ) ) {
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

	ExprTree *tree = NULL, *rhs = NULL, *lhs = NULL;
	char rhstr[ATTRLIST_MAX_EXPRESSION];
	char lhstr[128];
	char final[ATTRLIST_MAX_EXPRESSION];
	char node[9];
	char *tmp;
	int i;

	sprintf( node, "%d", nodenum );

	ad->ResetExpr();
	while( (tree = ad->NextExpr()) ) {
		rhstr[0] = '\0';
		lhstr[0] = '\0';
		if( (lhs = tree->LArg()) ) {
			lhs->PrintToStr (lhstr);
		}
		if( (rhs = tree->RArg()) ) {
			rhs->PrintToStr (rhstr);
		}
		if( !lhs || !rhs ) {
			dprintf( D_ALWAYS, "Could not replace $(NODE) in ad!\n" );
			return;
		}
		if( (tmp=strstr(rhstr, "#MpInOdE#")) ) {
			for( i=0 ; i<(int)strlen(node) ; i++ ) {
				*(tmp++) = node[i];
			}
			*(tmp++) = '\0';
			sprintf( final, "%s = %s%s", lhstr, rhstr,
					 tmp+(8-strlen(node)) ); 
			ad->InsertOrUpdate( final );
		}
	}	
}

/* handy for debugging... */
int 
MPIShadow::printAdToFile(ClassAd *ad, char* JobHistoryFileName) {

    FILE* LogFile=fopen(JobHistoryFileName,"a");
    if ( !LogFile ) {
        dprintf(D_ALWAYS,"ERROR saving to history file; cannot open%s\n", 
                JobHistoryFileName);
        return false;
    }
    if (!ad->fPrint(LogFile)) {
        dprintf(D_ALWAYS, "ERROR in Scheduler::LogMatchEnd - failed to "
                "write classad to log file %s\n", JobHistoryFileName);
        fclose(LogFile);
        return false;
    }
    fprintf(LogFile,"***\n");   // separator
    fclose(LogFile);
    return true;
}


int
MPIShadow::updateFromStarter(int command, Stream *s)
{
	ClassAd update_ad;
	MpiResource* mpi_res = NULL;
	int mpi_node = -1;
	
	// get info from the starter encapsulated in a ClassAd
	s->decode();
	update_ad.get(*s);
	s->end_of_message();

		// First, figure out what remote resource this info belongs
		// to. 

	if( ! update_ad.LookupInteger(ATTR_NODE, mpi_node) ) {
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
	mpi_res->updateFromStarter( &update_ad );

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
		if( node == mpi_res->node() ) {
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




