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
#include "parallelshadow.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.
#include "list.h"                // List class
#include "internet.h"            // sinful->hostname stuff
#include "daemon.h"


ParallelShadow::ParallelShadow() {
    nextResourceToStart = 0;
	numNodes = 0;
    shutDownMode = FALSE;
    ResourceList.fill(NULL);
    ResourceList.truncate(-1);
	actualExitReason = -1;
	info_tid = -1;
}

ParallelShadow::~ParallelShadow() {
        // Walk through list of Remote Resources.  Delete all.
    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
        delete ResourceList[i];
    }

		// this was allocated via classad->LookupString(char*, char**)
	free(parallelscriptshadow);
}

void 
ParallelShadow::init( ClassAd *jobAd, char schedd_addr[], char host[], 
                 char capability[], char cluster[], char proc[])
{
	char buf[256];

    if( !jobAd ) {
        EXCEPT("No jobAd defined!");
    }

        // BaseShadow::baseInit - basic init stuff...
    baseInit(jobAd, schedd_addr, cluster, proc);

		// Register command which gets updates from the starter
		// on the job's image size, cpu usage, etc.  Each kind of
		// shadow implements it's own version of this to deal w/ it
		// properly depending on parallel vs. serial jobs, etc. 
	daemonCore->
		Register_Command(SHADOW_UPDATEINFO, "SHADOW_UPDATEINFO", 
						 (CommandHandlercpp)&ParallelShadow::updateFromStarter,
						 "ParallelShadow::updateFromStarter", this, DAEMON); 

        /* Register Command for sneaky rsh: */
	daemonCore->Register_Command(MPI_START_COMRADE, "MPI_START_COMRADE", 
		 (CommandHandlercpp)&ParallelShadow::startComrade, "startComrade", 
		 this, WRITE);

        // make first remote resource the "master".  Put it first in list.
    ParallelResource *rr = new ParallelResource(this);
	rr->setStartdInfo(host, capability);
		// for now, set this to the sinful string.  when the starter
		// spawns, it'll do an RSC to register a real hostname...
	rr->setMachineName( host );
    ClassAd *tmp = new ClassAd( *(getJobAd() ) );

    sprintf( buf, "%s = %s", ATTR_PARALLEL_IS_MASTER, "TRUE" );
    if( !tmp->Insert(buf) ) {
        dprintf( D_ALWAYS, "Failed to insert %s into jobAd.\n", buf );
        shutDown( JOB_NOT_STARTED );
    }

	replaceNode(tmp, 0);
	rr->setNode(0);
	sprintf(buf, "%s = 0", ATTR_NODE);
	tmp->InsertOrUpdate(buf);
    rr->setJobAd(tmp);

    ResourceList[ResourceList.getlast() + 1] = rr;

		// setup the name of the job ad file
    snprintf(jobadfile, _POSIX_PATH_MAX, "%s/jobads.%d.%d",
			 getIwd(), getCluster(), getProc());

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
		Register_Timer(1, 0,
					   (TimerHandlercpp)&ParallelShadow::getResources,
					   "getResources", this);
	if( info_tid < 0 ) {
		EXCEPT("Can't register DC timer!");
	}

	postScript_rid = daemonCore->Register_Reaper("postScript",
								(ReaperHandlercpp)&ParallelShadow::postScript,
								"postScript", this);
}


int 
ParallelShadow::getResources( void )
{
    dprintf(D_FULLDEBUG, "Getting machines from schedd now...\n");

    char *host = NULL;
    char *capability = NULL;
    ParallelResource *rr;
	int cluster;
	char buf[128];

    int numProcs=0;    // the # of procs to come
    int numInProc=0;   // the # in a particular proc.
	ClassAd *job_ad = NULL;
	ClassAd *tmp_ad = NULL;
	int nodenum = 1;
	ReliSock* sock;

	cluster = getCluster();
    rr = ResourceList[0];
	rr->getCapability(capability);

		// First, contact the schedd and send the command, the
		// cluster, and the capability
	Daemon my_schedd(DT_SCHEDD, NULL, NULL);

	if(!(sock = (ReliSock*)my_schedd.startCommand(GIVE_MATCHES))) {
		EXCEPT("Can't connect to schedd at %s", getScheddAddr());
	}
		
	sock->encode();
	if( ! sock->code(cluster) ) {
		EXCEPT("Can't send cluster (%d) to schedd\n", cluster);
	}
	if( ! sock->code(capability) ) {
		EXCEPT("Can't send capability to schedd\n");
	}

		// Now that we sent this, free the memory that was allocated
		// with getCapability() above
	delete [] capability;
	capability = NULL;

	if( ! sock->end_of_message() ) {
		EXCEPT("Can't send EOM to schedd\n");
	}
	
		// Ok, that's all we need to send, now we can read the data
		// back off the wire
	sock->decode();

        // We first get the number of proc classes we'll be getting.
    if ( !sock->code(numProcs) ) {
        EXCEPT("Failed to get number of procs");
    }

        /* Now, do stuff for each proc: */
    for ( int i=0 ; i<numProcs ; i++ ) {
		job_ad = new ClassAd();
		if( !job_ad->initFromStream(*sock)  ) {
            EXCEPT("Failed to get job classad for proc %d", i);
		}
        if( !sock->code(numInProc) ) {
            EXCEPT("Failed to get number of matches in proc %d", i);
        }

        dprintf(D_FULLDEBUG, "Got %d matches for proc # %d\n", numInProc, i);

        for ( int j=0 ; j<numInProc ; j++ ) {
            if( !sock->code( host ) ||
                 !sock->code( capability ) ) {
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

            rr = new ParallelResource( this );
            rr->setStartdInfo( host, capability );
 				// for now, set this to the sinful string.  when the
 				// starter spawns, it'll do an RSC to register a real
				// hostname... 
			rr->setMachineName( host );

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

    sock->end_of_message();

	numNodes = nodenum;  // for later use...

    dprintf ( D_PROTOCOL, "#1 - Shadow started; %d machines gotten.\n", 
			  numNodes );

    startMaster();

    return TRUE;
}


void
ParallelShadow::startMaster()
{
    dprintf(D_FULLDEBUG, "In ParallelShadow::startMaster()\n");

    ParallelResource *rr;
	ClassAd* onead;
	rr = ResourceList[0];
	onead = rr->getJobAd();
	parallelscriptshadow = NULL;
	onead->LookupString(ATTR_PARALLEL_SCRIPT_SHADOW, &parallelscriptshadow);

        // for each resource, get job ad, spit to file
    char *sinful = new char[128];
    struct sockaddr_in sin;
    FILE *ja;
    if( !(ja = fopen(jobadfile, "w")) ) {
        dprintf(D_ALWAYS, "Failure to open %s for writing, errno %d\n",
				jobadfile, errno);
        shutDown(JOB_NOT_STARTED);
    }
    for( int i = 0; i <= ResourceList.getlast(); i++ ) {
        rr = ResourceList[i];
        onead = rr->getJobAd();
		onead->fPrint(ja);
        rr->getStartdAddress(sinful);
        string_to_sin(sinful, &sin);
			// This is the hostname of the machine that the job ad will
			// get. Yes, it does not make any sense for the job ad to have
			// the hostname of the machine it will run on. This is just a
			// tmporary hack.
		fprintf(ja, "ThisHost = \"%s\"\n", sin_to_hostname(&sin, NULL));
		fprintf(ja, "...\n");
    }
    delete [] sinful;

        // set permissions on the jobad file:
#ifdef WIN32
	perm p;
	if ( p.set_acls(jobadfile) < 0 ) { // sets 'Full' permissions on file
		  dprintf(D_ALWAYS, "perm::set_acls() failed!\n");
#else
	if ( fchmod(fileno(ja), 0666) < 0 ) {
        dprintf(D_ALWAYS, "fchmod failed! errno %d\n", errno);
#endif
        fclose(ja);
        shutDown(JOB_NOT_STARTED);
    }

    if( fclose(ja) == EOF ) {
        dprintf(D_ALWAYS, "fclose failed!  errno = %d\n", errno);
    }

	if( parallelscriptshadow ) {
		    // run shadow side script
		runParallelStartupScript(NULL);
	} else {
		dprintf(D_ALWAYS, "Error, job submitted as %s universe but without "
				"a %s.\n", CondorUniverseName(CONDOR_UNIVERSE_PARALLEL),
				ATTR_PARALLEL_SCRIPT_SHADOW);
		shutDown(JOB_NOT_STARTED);
	}
}

int
ParallelShadow::readJobAdFile( int node )
{
		// read the jobads back in
	FILE* newjobads;
	if( !(newjobads = fopen(jobadfile, "r")) ) {
		dprintf(D_ALWAYS, "Error in readJobAdFile in parallelshadow.C: could "
				"not open %s\n", jobadfile);
		shutDown(JOB_NOT_STARTED);
	}
	ClassAd* tmpad;
	int iseof = 0;
	int error = 0;
	int empty = 0;
	for( int i = 0;
		 i <= ResourceList.getlast() && !iseof && !error && !empty;
		 i++ ) {
		tmpad = new ClassAd(newjobads, "...", iseof, error, empty);
		if( i >= node ) {
			ResourceList[i]->setJobAd(tmpad);
		} else {
			delete tmpad;
		}
	}
	fclose(newjobads);
	ASSERT(!iseof && !error && !empty);

	//TODO - better error handling needed in previous code

	return 1;
}

int
ParallelShadow::runParallelStartupScript( char* rshargs )
{
	char*  binary;
	char*  scriptargs;			// will be the final argument list
	int    argcharcount = 0;	// size of scriptargs

	if( !parallelscriptshadow ) {
		dprintf(D_ALWAYS, "Error, parallelscriptshadow is null\n");
		shutDown(JOB_NOT_STARTED);
	}
	if( !jobadfile ) {
		dprintf(D_ALWAYS, "Error, jobadfile is null\n");
		shutDown(JOB_NOT_STARTED);
	}

		// determine necessary memory
	argcharcount += strlen(parallelscriptshadow);
	argcharcount += 1; // for space
	argcharcount += strlen(jobadfile);
	if( rshargs ) {
		argcharcount += 15; // for the node number
		argcharcount += strlen(rshargs);
	}

	scriptargs = (char*) calloc(argcharcount + 1, sizeof(char));// +1 for null

		// fill in
	strcat(scriptargs, parallelscriptshadow);
	strcat(scriptargs, " ");
	strcat(scriptargs, jobadfile);
	if( rshargs ) {
		char tmp[15];
		snprintf(tmp, 15, " %d ", nextResourceToStart);
		strcat(scriptargs, tmp);
		strcat(scriptargs, rshargs);
	}

		// clip off the extra stuff in `binary'
	binary = strdup(parallelscriptshadow);
	char* stepper = binary;
	if( isspace(*stepper) ) while( isspace(*stepper) ) stepper++;
	while( !isspace(*stepper) && *stepper ) stepper++;
	*stepper = 0;

	dprintf(D_FULLDEBUG, "about to run parallel script: %s\n", scriptargs);

		// run shadow side script
	int scriptpid = daemonCore->Create_Process(binary, scriptargs,
											   PRIV_USER_FINAL,postScript_rid);

	free(binary);
	free(scriptargs);

	if( !scriptpid ) {
		dprintf(D_ALWAYS, "Error, could not execute the script (%s)\n",
				scriptargs);
		shutDown(JOB_NOT_STARTED);
	}

	return 1;
}

int
ParallelShadow::postScript(int pid, int exit_status)
{
	dprintf(D_FULLDEBUG, "script exited with status %d\n", exit_status);

	FILE* scriptobit;
	char obitfile[_POSIX_PATH_MAX];
	snprintf(obitfile, _POSIX_PATH_MAX, "%s/scriptobit.%d",
			 getIwd(), pid);
	if( (scriptobit = fopen(obitfile, "r")) ) {
		char scripterr[256];
		fgets(scripterr, 256, scriptobit);
		fclose(scriptobit);
		unlink(obitfile);
		dprintf(D_ALWAYS, "Script exited with this error "
				"message:\n%s", scripterr);
		shutDown(JOB_NOT_STARTED);
	}

	if( nextResourceToStart ) {
			// read in the potentially modified ads
		readJobAdFile(nextResourceToStart);

		ParallelResource* rr = ResourceList[nextResourceToStart];
		dprintf(D_PROTOCOL, "#9 - Added args to jobAd, now requesting:\n");

			// Now, call the shared method to really spawn this node
		spawnNode(rr);
	} else {
		    // read the jobads back in
		readJobAdFile(0);

			// back to master resource:
		ParallelResource* rr = ResourceList[0];

			// now, we want to re-initialize the shadow_user_policy object
		    // with the ClassAd for our master node, since the one sitting
		    // in the Shadow object itself will never get updated with
		    // exit status, info about the run, etc, etc.
		shadow_user_policy.init(rr->getJobAd(), this);

            // Once we actually spawn the job (below), the sneaky rsh
		    // intercepts the master's call to rsh, and sends stuff to
		    // startComrade()... 
		spawnNode(rr);

		dprintf(D_PROTOCOL, "#3 - Just requested Master resource.\n");
	}

	return TRUE;
}

int
ParallelShadow::startComrade( int cmd, Stream* s )
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

		// run the parallel startup script again
	runParallelStartupScript(comradeArgs);

    return TRUE;
}


void 
ParallelShadow::spawnNode( ParallelResource* rr )
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
ParallelShadow::cleanUp( void )
{
		// unlink the jobads file:
	if( unlink(jobadfile) == -1 ) {
		if( errno != ENOENT ) {
			dprintf( D_ALWAYS, "Problem removing %s: errno %d.\n", 
  					 jobadfile, errno );
		}
	}

		// kill all the starters
	ParallelResource *r;
	int i;
    for( i=0 ; i<=ResourceList.getlast() ; i++ ) {
		r = ResourceList[i];
		r->killStarter();
	}		
}


void 
ParallelShadow::gracefulShutDown( void )
{
	cleanUp();
}


void
ParallelShadow::emailTerminateEvent( int exitReason )
{
	int i;
	FILE* mailer;
	mailer = shutDownEmail( exitReason );
	if( ! mailer ) {
			// nothing to do
		return;
	}

	fprintf(mailer,"Your parallel universe Condor job %d.%d has completed.\n", 
			getCluster(), getProc());

	fprintf(mailer, "\nHere are the machines that ran your job.\n");
	fprintf(mailer, "They are listed in the order they were started.\n\n");
	fprintf(mailer, "    Machine Name               Result\n" );
	fprintf(mailer, " ------------------------    -----------\n" );

	int exit_code;
	for ( i=0 ; i<=ResourceList.getlast() ; i++ ) {
		(ResourceList[i])->printExit( mailer );
		exit_code = (ResourceList[i])->exitCode();
	}

	fprintf( mailer, "\nHave a nice day.\n" );
	
	email_close(mailer);
}


void 
ParallelShadow::shutDown( int exitReason )
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
ParallelShadow::shutDownLogic( int& exitReason ) {

		/* What sucks for us here is that we know we want to shut 
		   down, but we don't know *why* we are shutting down.
		   We have to look through the set of ParallelResources
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
	ParallelResource *r;

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
ParallelShadow::handleJobRemoval( int sig ) {

    dprintf ( D_FULLDEBUG, "In handleJobRemoval, sig %d\n", sig );

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

/* This is basically a search-and-replace "#pArAlLeLnOdE#" with a number 
   for that node...so we can address each node in the submit file. */
void
ParallelShadow::replaceNode ( ClassAd *ad, int nodenum ) {

	ExprTree *rhs = NULL;
	char rhstr[ATTRLIST_MAX_EXPRESSION];
	char lhstr[128];
	char final[ATTRLIST_MAX_EXPRESSION];
	char node[9];

	sprintf( node, "%d", nodenum );

	ClassAd::iterator adIter;
	ClassAdUnParser unp;
	string rhstring;
	for( adIter = ad->begin( ); adIter != ad->end( ); adIter++ ) {
		rhstr[0] = '\0';
		lhstr[0] = '\0';
		strcpy( lhstr, adIter->first.c_str( ) );
		if( adIter->second ) {
			unp.Unparse( rhstring, adIter->second );
			strcpy( rhstr, rhstring.c_str( ) );
		}
		if( !rhs ) {
			dprintf( D_ALWAYS, "Could not replace $(NODE) in ad!\n" );
			dprintf( D_ALWAYS, "Could not replace $(NODE) in ad!\n" );
			return;
		}

		MyString strRh(rhstr);
		if (strRh.replaceString("#pArAlLeLnOdE#", node))
		{
			sprintf( final, "%s = %s", lhstr, strRh.Value());
			ad->InsertOrUpdate( final );
			dprintf( D_FULLDEBUG, "Replaced $(NODE), now using: %s\n", 
					 final );
		}
	}	
}


int
ParallelShadow::updateFromStarter(int command, Stream *s)
{
	ClassAd update_ad;
	ParallelResource* parallel_res = NULL;
	int parallel_node = -1;
	
	// get info from the starter encapsulated in a ClassAd
	s->decode();
	update_ad.initFromStream(*s);
	s->end_of_message();

		// First, figure out what remote resource this info belongs
		// to. 

	if( ! update_ad.LookupInteger(ATTR_NODE, parallel_node) ) {
			// No ATTR_NODE in the update ad!
		dprintf( D_ALWAYS, "ERROR in ParallelShadow::updateFromStarter: "
				 "no %s defined in update ad, can't process!\n",
				 ATTR_NODE );
		return FALSE;
	}
	if( ! (parallel_res = findResource(parallel_node)) ) {
		dprintf( D_ALWAYS, "ERROR in ParallelShadow::updateFromStarter: "
				 "can't find remote resource for node %d, "
				 "can't process!\n", parallel_node );
		return FALSE;
	}

		// Now, we're in good shape.  Grab all the info we care about
		// and put it in the appropriate place.
	parallel_res->updateFromStarter( &update_ad );

		// XXX TODO: Do we want to update our local job ad?  Do we
		// want to store the maximum in there?  Seperate stuff for
		// each node?  

	return TRUE;
}


ParallelResource*
ParallelShadow::findResource( int node )
{
	ParallelResource* parallel_res;
	int i;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		parallel_res = ResourceList[i];
		if( node == parallel_res->node() ) {
			return parallel_res;
		}
	}
	return NULL;
}


struct rusage
ParallelShadow::getRUsage( void ) 
{
	ParallelResource* parallel_res;
	struct rusage total;
	struct rusage cur;
	int i;
	memset( &total, 0, sizeof(struct rusage) );
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		parallel_res = ResourceList[i];
		cur = parallel_res->getRUsage();
		total.ru_stime.tv_sec += cur.ru_stime.tv_sec;
		total.ru_utime.tv_sec += cur.ru_utime.tv_sec;
	}
	return total;
}


int
ParallelShadow::getImageSize( void )
{
	ParallelResource* parallel_res;
	int i, max = 0, val;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		parallel_res = ResourceList[i];
		val = parallel_res->getImageSize();
		if( val > max ) {
			max = val;
		}
	}
	return max;
}


int
ParallelShadow::getDiskUsage( void )
{
	ParallelResource* parallel_res;
	int i, max = 0, val;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		parallel_res = ResourceList[i];
		val = parallel_res->getDiskUsage();
		if( val > max ) {
			max = val;
		}
	}
	return max;
}


float
ParallelShadow::bytesSent( void )
{
	ParallelResource* parallel_res;
	int i;
	float total = 0;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		parallel_res = ResourceList[i];
		total += parallel_res->bytesSent();
	}
	return total;
}


float
ParallelShadow::bytesReceived( void )
{
	ParallelResource* parallel_res;
	int i;
	float total = 0;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		parallel_res = ResourceList[i];
		total += parallel_res->bytesReceived();
	}
	return total;
}

int
ParallelShadow::getExitReason( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->getExitReason();
	}
	return -1;
}


bool
ParallelShadow::exitedBySignal( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitedBySignal();
	}
	return false;
}


int
ParallelShadow::exitSignal( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitSignal();
	}
	return -1;
}


int
ParallelShadow::exitCode( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitCode();
	}
	return -1;
}


void
ParallelShadow::resourceBeganExecution( RemoteResource* rr )
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
		strcpy( event.executeHost, "(parallel job)" );
		if ( !uLog.writeEvent( &event )) {
			dprintf ( D_ALWAYS, "Unable to log EXECUTE event." );
		}

			// Now that the whole job is running, start up a few
			// timers we need.
		shadow_user_policy.startTimer();
	}
}
