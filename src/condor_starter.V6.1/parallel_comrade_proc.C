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
#include "parallel_comrade_proc.h"
#include "condor_attributes.h"


ParallelComradeProc::ParallelComradeProc( ClassAd * jobAd ) : VanillaProc( jobAd )
{
    dprintf ( D_FULLDEBUG, "Constructor of ParallelComradeProc::ParallelComradeProc\n" );
	Node = -1;
}

ParallelComradeProc::~ParallelComradeProc()  {}


int 
ParallelComradeProc::StartJob()
{ 
	dprintf(D_FULLDEBUG,"in ParallelComradeProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in ParallelComradeProc::StartJob()!\n" ); 
		return 0;
	}
		// Grab ATTR_NODE out of the job ad and stick it in our
		// protected member so we can insert it back on updates, etc. 
	if( JobAd->LookupInteger(ATTR_NODE, Node) != 1 ) {
		dprintf( D_ALWAYS, "ERROR in ParallelComradeProc::StartJob(): "
				 "No %s in job ad, aborting!\n", ATTR_NODE );
		return 0;
	} else {
		dprintf( D_FULLDEBUG, "Found %s = %d in job ad\n", ATTR_NODE, Node ); 
	}

    dprintf(D_PROTOCOL, "#11 - Comrade starting up....\n" );

	/*
		// Give the script the name of the job ads file, its own node number,
		// and the original command line.
	int clust;
	JobAd->LookupInteger(ATTR_CLUSTER_ID, clust);
	char* cmd;
	JobAd->LookupString(ATTR_JOB_CMD, &cmd);
	char* args;	 
	JobAd->LookupString(ATTR_JOB_ARGUMENTS, &args);
	char tmp[_POSIX_PATH_MAX];
	char tmp2[_POSIX_PATH_MAX];

		// Here we have to escape the quotes in the args so
		// they won't screw up Insert.
	char* stepper1 = tmp2;
	char* stepper2 = args;
	char* last = strlen(args) + args;
	while( stepper2 <= last ) {
		if( *stepper2 == '"' ) {
			*stepper1 = '\\';
			stepper1++;
		}
		*stepper1 = *stepper2;
		stepper1++;
		stepper2++;
	}

	snprintf(tmp, _POSIX_PATH_MAX, "%s = \"%d jobads.%d %s %s\"",
			 ATTR_JOB_ARGUMENTS, Node, clust, cmd, tmp2);

	//TEMP
	dprintf(D_ALWAYS, "about to insert %s\n", tmp);

	JobAd->Insert(tmp);
	free(cmd);

	char tmp3[_POSIX_PATH_MAX];

		// Start up the script instead
	if( JobAd->LookupString(ATTR_PARALLEL_STARTUP_SCRIPT, tmp3) ) {
		char* trimmer = &tmp3[strlen(tmp3)];
		while( *trimmer != DIR_DELIM_CHAR ) trimmer--;
		trimmer++;
		snprintf(tmp, _POSIX_PATH_MAX, "%s = \"%s\"", ATTR_JOB_CMD, trimmer);
		JobAd->Insert(tmp);
		//TEMP
		dprintf(D_ALWAYS, "%s inserted into job ad\n", tmp);
	} else {
		//FIXME - deal with this case better
		dprintf(D_ALWAYS, "No %s attr found in job ad\n",
				ATTR_PARALLEL_STARTUP_SCRIPT);
	}
	*/

        // Special args already in ad; simply start it up
    return VanillaProc::StartJob();
}


int
ParallelComradeProc::JobCleanup( int pid, int status )
{ 
	return VanillaProc::JobCleanup( pid, status );
}


void 
ParallelComradeProc::Suspend() { 
        /* We Comrades don't ever want to be suspended.  We 
           take it as a violation of our basic rights.  Therefore, 
           we walk off the job and notify the shadow immediately! */
	dprintf(D_FULLDEBUG,"in ParallelComradeProc::Suspend()\n");
		// must do this so that we exit...
	daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
}


void 
ParallelComradeProc::Continue() { 
	dprintf(D_FULLDEBUG,"in ParallelComradeProc::Continue() (!)\n");    

        // really should never get here, but just in case.....
    VanillaProc::Continue();
}


bool
ParallelComradeProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In ParallelComradeProc::PublishUpdateAd()\n" );
	char buf[64];
	sprintf( buf, "%s = %d", ATTR_NODE, Node );
	ad->Insert( buf );

		// Now, call our parent class's version
	return VanillaProc::PublishUpdateAd( ad );
}
