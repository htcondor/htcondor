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
#include "parallelshadow.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.
#include "daemon.h"
#include "condor_config.h"
#include "spooled_job_files.h"
#include "condor_holdcodes.h"

RemoteResource *parallelMasterResource = NULL;

ParallelShadow::ParallelShadow() {
    nextResourceToStart = 0;
	numNodes = 0;
    shutDownMode = FALSE;
	actualExitReason = -1;
	info_tid = -1;
	is_reconnect = false;
	shutdownPolicy = ParallelShadow::WAIT_FOR_NODE0;
}

ParallelShadow::~ParallelShadow() {
        // Walk through list of Remote Resources.  Delete all.
    for ( size_t i=0 ; i<ResourceList.size() ; i++ ) {
        delete ResourceList[i];
    }
}

void 
ParallelShadow::init( ClassAd* job_ad, const char* schedd_addr, const char *xfer_queue_contact_info )
{

    if( ! job_ad ) {
        EXCEPT( "No job_ad defined!" );
    }

        // BaseShadow::baseInit - basic init stuff...
    baseInit( job_ad, schedd_addr, xfer_queue_contact_info );

        // make first remote resource the "master".  Put it first in list.
    MpiResource *rr = new MpiResource( this );
	parallelMasterResource = rr;

	std::string dir;
	SpooledJobFiles::getJobSpoolPath(jobAd, dir);
	job_ad->Assign(ATTR_REMOTE_SPOOL_DIR,dir);

	job_ad->Assign(ATTR_MPI_IS_MASTER, true);

	replaceNode( job_ad, 0 );
	rr->setNode( 0 );
	job_ad->Assign( ATTR_NODE, 0 );
    rr->setJobAd( job_ad );

	rr->setStartdInfo( job_ad );

    ResourceList.push_back(rr);

	shutdownPolicy = ParallelShadow::WAIT_FOR_NODE0;

	std::string policy;
	job_ad->LookupString(ATTR_PARALLEL_SHUTDOWN_POLICY, policy);
	if (policy == "WAIT_FOR_ALL") {
		dprintf(D_ALWAYS, "Setting parallel shutdown policy to WAIT_FOR_ALL\n");
		shutdownPolicy= WAIT_FOR_ALL;
	}
}


bool
ParallelShadow::shouldAttemptReconnect(RemoteResource *r) {
	if (shutdownPolicy == WAIT_FOR_ALL) {
		return true;
	}

	if ((shutdownPolicy == WAIT_FOR_NODE0) && (r != ResourceList[0])) {
		return false;
	}

	return true;
}

void
ParallelShadow::reconnect( void )
{

	dprintf( D_FULLDEBUG, "ParallelShadow::reconnect\n");
	is_reconnect = true;
	spawn();
}


bool 
ParallelShadow::supportsReconnect( void )
{
		// Iff all remote resources support reconect,
		// then this shadow does.  If any do not, we do not
    for ( size_t i=0 ; i<ResourceList.size() ; i++ ) {
        if (! ResourceList[i]->supportsReconnect()) {
			return false;
		}
    }
	
	return true;
}


void
ParallelShadow::spawn( void )
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
						(TimerHandlercpp)&ParallelShadow::getResources,
						"getResources", this );
	if( info_tid < 0 ) {
		EXCEPT( "Can't register DC timer!" );
	}
	// Start the timer for the periodic user job policy
	shadow_user_policy.startTimer();
}


void
ParallelShadow::getResources( int /* timerID */ )
{
    dprintf ( D_FULLDEBUG, "Getting machines from schedd now...\n" );

    char *host = NULL;
    char *claim_id = NULL;
    MpiResource *rr;
	int job_cluster;

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
	Daemon my_schedd (DT_SCHEDD, getScheddAddr(), NULL);

	if(!(sock = (ReliSock*)my_schedd.startCommand(GIVE_MATCHES))) {
		EXCEPT( "Can't connect to schedd at %s", getScheddAddr() );
	}
		
	sock->encode();
	if( ! sock->code(job_cluster) ) {
		EXCEPT( "Can't send cluster (%d) to schedd", job_cluster );
	}
	if( ! sock->code(claim_id) ) {
		EXCEPT( "Can't send ClaimId to schedd" );
	}

		// Now that we sent this, free the memory that was allocated
		// with getClaimId() above
	free( claim_id );
	claim_id = NULL;

	if( ! sock->end_of_message() ) {
		EXCEPT( "Can't send EOM to schedd" );
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

			if( !getClassAd(sock, *job_ad)  ) {
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
			job_ad->Assign( ATTR_NODE, nodenum );
			job_ad->Assign( ATTR_MY_ADDRESS,
			                daemonCore->InfoCommandSinfulString() );

				// We want the spool directory of Proc 0, so use data
				// member jobAd, NOT local variable job_ad.
			std::string dir;
			SpooledJobFiles::getJobSpoolPath(jobAd, dir);
			job_ad->Assign(ATTR_REMOTE_SPOOL_DIR, dir);

				// Put the correct claim id into this ad's ClaimId attribute.
				// Otherwise, it is the claim id of the master proc.
				// This is how the starter finds out about the claim id.
			job_ad->Assign(ATTR_CLAIM_ID,claim_id);

			rr->setJobAd( job_ad );
			nodenum++;

            ResourceList.push_back(rr);

                /* free stuff so next code() works correctly */
            free( host );
            free( claim_id );
            host = NULL;
            claim_id = NULL;

				/* Double check that the number of matches for this proc
				   is the same as in the job ad.  This prevents races
				   where a claim can be vacated after the schedd forks this
					shadow 
				*/
			

			int jobAdNumInProc = 0;
			if (!job_ad->LookupInteger(ATTR_MAX_HOSTS, jobAdNumInProc)) {
				dprintf(D_ALWAYS, "ERROR: no attribute MaxHosts in parallel job\n");	
				delete sock;
				return;
			};
			if (jobAdNumInProc != numInProc) {
				dprintf(D_ALWAYS, "ERROR -- job needs %d slots, but schedd gave us %d slots -- giving up\n", jobAdNumInProc, numInProc);
				BaseShadow::shutDown(JOB_NOT_STARTED, "Wrong number of slots");
				sock->end_of_message();
				delete sock;
				return;
			}
        } // end of for loop for this proc

    } // end of for loop on all procs...

    sock->end_of_message();

	numNodes = nodenum;  // for later use...

    dprintf ( D_PROTOCOL, "#1 - Shadow started; %d machines gotten.\n", 
			  numNodes );

    startMaster();

    delete sock;
}


void
ParallelShadow::startMaster()
{
    MpiResource *rr;
    dprintf ( D_FULLDEBUG, "In ParallelShadow::startMaster()\n" );

	rr = ResourceList[0];

	spawnNode( rr );

	spawnAllComrades();

}

void
ParallelShadow::spawnAllComrades( void )
{
		/* 
		   If this function is being called, we've already spawned the
		   root node and gotten its ip/port from our special pseudo
		   syscall.  So, all we have to do is loop over our remote
		   resource list, modify each resource's job ad with the
		   appropriate info, and spawn our node on each resource.
		*/

    MpiResource *rr;
	int size = ResourceList.size();
	while( nextResourceToStart < size ) {
        rr = ResourceList[nextResourceToStart];
		spawnNode( rr );  // This increments nextResourceToStart 
    }
	ASSERT( nextResourceToStart == numNodes );
}


void 
ParallelShadow::spawnNode( MpiResource* rr )
{
	if (is_reconnect) {
		dprintf(D_FULLDEBUG, "reconnecting to the following remote resource:\n");
		rr->dprintfSelf(D_FULLDEBUG);
		rr->reconnect();
	} else {
			// First, contact the startd to spawn the job
		if( rr->activateClaim() != OK ) {
			shutDown(JOB_NOT_STARTED, "Failed to activate claim", CONDOR_HOLD_CODE::FailedToActivateClaim);
			
		}
	}

    dprintf ( D_PROTOCOL, "Just requested resource for node %d\n",
			  nextResourceToStart );

	nextResourceToStart++;
}


void 
ParallelShadow::cleanUp( bool graceful )
{
	// kill all the starters
	MpiResource *r;
	for( size_t i=0 ; i<ResourceList.size() ; i++ ) {
		r = ResourceList[i];
		r->killStarter(graceful);
	}		
}

int ParallelShadow::JobSuspend(int sig)
{
	int iRet = 0;
	MpiResource *r;
	for( size_t i=0 ; i<ResourceList.size() ; i++ ) {
		r = ResourceList[i];
		if (!r || !r->suspend())
		{
			iRet = 1;
			dprintf ( D_ALWAYS, "ParallelShadow::JobSuspend() sig %d FAILED\n", sig );
		}
	}		
	
	return iRet;
	
}

int ParallelShadow::JobResume(int sig)
{
	int iRet = 0;
	MpiResource *r;
	for( size_t i=0 ; i<ResourceList.size() ; i++ ) {
		r = ResourceList[i];
		if (!r || !r->resume())
		{
			iRet = 1;
			dprintf ( D_ALWAYS, "ParallelShadow::JobResume() sig %d FAILED\n", sig );
		}
	}
	
	return iRet;
}


bool
ParallelShadow::claimIsClosing( void )
{
	return false;
}

void 
ParallelShadow::gracefulShutDown( void )
{
	cleanUp();
}


void
ParallelShadow::emailTerminateEvent( int exitReason, update_style_t kind )
{
	size_t i;
	FILE* mailer;
	Email msg;
	std::string str;

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

		for (auto& s: StringTokenIterator(str))
		{
			fprintf( mailer, "%s\n", s.c_str());
		}

		fprintf( mailer, "\nExit codes are currently unavailable.\n\n");
		fprintf( mailer, "\nHave a nice day.\n" );

		return;
	}

	fprintf( mailer, "They are listed in the order they were started\n" );
	fprintf( mailer, "in, which is the same as MPI_Comm_rank.\n\n" );
	fprintf( mailer, "    Machine Name               Result\n" );
	fprintf( mailer, " ------------------------    -----------\n" );

	// This is the normal case of US_NORMAL. In this mode we can print
	// out a bunch of interesting things about the job which the remote
	// resources know about.

	int allexitsone = TRUE;
	int exit_code;
	for ( i=0 ; i<ResourceList.size() ; i++ ) {
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

	msg.writeCustom(jobAd);

	fprintf( mailer, "\nHave a nice day.\n" );
}


void 
ParallelShadow::shutDown( int exitReason, const char* reason_str, int reason_code, int reason_subcode )
{	
	if (exitReason != JOB_NOT_STARTED) {
		if (shutdownPolicy == WAIT_FOR_ALL) {
			
			unsigned int iResources = ResourceList.size();
			
			for ( size_t i=0 ; i<ResourceList.size() ; i++ ) {
				RemoteResource *r = ResourceList[i];
				// If the policy is wait for all nodes to exit
				// see if any are still running.  If so,
				// just return, and wait for them all to go
				if (r->getResourceState() != RR_FINISHED ) {
				    dprintf( D_FULLDEBUG, "ParallelShadow::shutDown WAIT_FOR_ALL Not all resources have FINISHED\n");
				    return;
				}
			}
			
			dprintf( D_FULLDEBUG, "ParallelShadow::shutDown WAIT_FOR_ALL - All(%d) resources have called exit/shutdown\n",iResources );
			
		}
			// If node0 is still running, don't really shut down
		RemoteResource *r =  ResourceList[0];
		if (r->getResourceState() != RR_FINISHED) {
			return;
		}
	}

	handleJobRemoval(0);

		/* if we're still here, we can call BaseShadow::shutDown() to
		   do the real work, which is shared among all kinds of
		   shadows.  the shutDown process will call other virtual
		   functions to get universe-specific goodness right. */
	BaseShadow::shutDown( exitReason, reason_str, reason_code, reason_subcode );
}


void
ParallelShadow::holdJob( const char* reason, int hold_reason_code, int hold_reason_subcode )
{
	if( ResourceList.size() && ResourceList[0] ) {
		ResourceList[0]->setExitReason( JOB_SHOULD_HOLD );
	}
	BaseShadow::holdJob(reason, hold_reason_code, hold_reason_subcode);
}

int 
ParallelShadow::handleJobRemoval( int sig ) {

    dprintf ( D_FULLDEBUG, "In handleJobRemoval, sig %d\n", sig );
	remove_requested = true;

	ResourceState s;

	bool allPre = true;

    for ( size_t i=0 ; i<ResourceList.size() ; i++ ) {
		s = ResourceList[i]->getResourceState();
		if (s != RR_PRE) {
			allPre = false;
		}
		if( s == RR_EXECUTING || s == RR_STARTUP ) {
			ResourceList[i]->setExitReason( JOB_KILLED );
			ResourceList[i]->killStarter();
		}
    }

	if (allPre) {
		BaseShadow::shutDown(JOB_SHOULD_REMOVE, "");
	}
	return 0;
}

/* This is basically a search-and-replace "#pArAlLeLnOdE#" with a number 
   for that node...so we can address each mpi node in the submit file. */
void
ParallelShadow::replaceNode ( ClassAd *ad, int nodenum ) {

	char node[9];
	const char *lhstr, *rhstr;

	snprintf( node, 9, "%d", nodenum );

	for ( auto itr = ad->begin(); itr != ad->end(); itr++ ) {
		lhstr = itr->first.c_str();
		rhstr = ExprTreeToString(itr->second);
		if( !lhstr || !rhstr ) {
			dprintf( D_ALWAYS, "Could not replace $(NODE) in ad!\n" );
			return;
		}

		std::string strRh(rhstr);
		if (replace_str(strRh, "#pArAlLeLnOdE#", node) > 0)
		{
			ad->AssignExpr( lhstr, strRh.c_str() );
			dprintf( D_FULLDEBUG, "Replaced $(NODE), now using: %s = %s\n", 
					 lhstr, strRh.c_str() );
		}
	}	
}


int
ParallelShadow::updateFromStarterClassAd(ClassAd* update_ad)
{
	ClassAd *job_ad = getJobAd();
	MpiResource* mpi_res = NULL;
	int mpi_node = -1;

		// First, figure out what remote resource this info belongs to. 
	if( ! update_ad->LookupInteger(ATTR_NODE, mpi_node) ) {
			// No ATTR_NODE in the update ad!
		dprintf( D_ALWAYS, "ERROR in ParallelShadow::updateFromStarter: "
				 "no %s defined in update ad, can't process!\n",
				 ATTR_NODE );
		return FALSE;
	}
	if( ! (mpi_res = findResource(mpi_node)) ) {
		dprintf( D_ALWAYS, "ERROR in ParallelShadow::updateFromStarter: "
				 "can't find remote resource for node %d, "
				 "can't process!\n", mpi_node );
		return FALSE;
	}

	int64_t prev_disk = getDiskUsage();
	struct rusage prev_rusage = getRUsage();


		// Now, we're in good shape.  Grab all the info we care about
		// and put it in the appropriate place.
	mpi_res->updateFromStarter(update_ad);

		// XXX TODO: Do we want to update our local job ad?  Do we
		// want to store the maximum in there?  Seperate stuff for
		// each node?  

	int64_t cur_disk = getDiskUsage();
    if( cur_disk > prev_disk ) {
		job_ad->Assign(ATTR_DISK_USAGE, cur_disk);
    }

    struct rusage cur_rusage = getRUsage();
    if( cur_rusage.ru_stime.tv_sec > prev_rusage.ru_stime.tv_sec ) {
        job_ad->Assign(ATTR_JOB_REMOTE_SYS_CPU, (double)cur_rusage.ru_stime.tv_sec);
    }
    if( cur_rusage.ru_utime.tv_sec > prev_rusage.ru_utime.tv_sec ) {
        job_ad->Assign(ATTR_JOB_REMOTE_USER_CPU, (double)cur_rusage.ru_utime.tv_sec);
    }
	return TRUE;
}


MpiResource*
ParallelShadow::findResource( int node )
{
	MpiResource* mpi_res;
	for( size_t i=0; i<ResourceList.size() ; i++ ) {
		mpi_res = ResourceList[i];
		if( node == mpi_res->getNode() ) {
			return mpi_res;
		}
	}
	return NULL;
}


struct rusage
ParallelShadow::getRUsage( void ) 
{
	MpiResource* mpi_res;
	struct rusage total;
	struct rusage cur;
	memset( &total, 0, sizeof(struct rusage) );
	for( size_t i=0; i<ResourceList.size() ; i++ ) {
		mpi_res = ResourceList[i];
		cur = mpi_res->getRUsage();
		total.ru_stime.tv_sec += cur.ru_stime.tv_sec;
		total.ru_utime.tv_sec += cur.ru_utime.tv_sec;
	}
	return total;
}


int64_t
ParallelShadow::getImageSize( int64_t & memory_usage, int64_t & rss, int64_t & pss )
{
	MpiResource* mpi_res;
	int64_t max_size = 0, max_usage = 0, max_rss = 0, max_pss = -1;
	for(size_t i=0; i<ResourceList.size() ; i++ ) {
		mpi_res = ResourceList[i];
		int64_t usage = 0, rs = 0, ps = 0;
		int64_t val = mpi_res->getImageSize( usage, rs, ps );
		max_size = MAX(val, max_size);
		max_usage = MAX(usage, max_usage);
		max_rss = MAX(rs, max_rss);
		max_pss = MAX(ps, max_pss);
	}
	rss = max_rss;
	pss = max_pss;
	memory_usage = max_usage;
	return max_size;
}


int64_t
ParallelShadow::getDiskUsage( void )
{
	MpiResource* mpi_res;
	int64_t max = 0, val;
	for( size_t i=0; i<ResourceList.size() ; i++ ) {
		mpi_res = ResourceList[i];
		val = mpi_res->getDiskUsage();
		if( val > max ) {
			max = val;
		}
	}
	return max;
}


uint64_t
ParallelShadow::bytesSent( void )
{
	uint64_t total = 0;
	for (MpiResource* mpi_res : ResourceList) {
		total += mpi_res->bytesSent();
	}
	return total;
}


uint64_t
ParallelShadow::bytesReceived( void )
{
	uint64_t total = 0;
	for (MpiResource* mpi_res : ResourceList) {
		total += mpi_res->bytesReceived();
	}
	return total;
}

void
ParallelShadow::getFileTransferStats(ClassAd &upload_stats, ClassAd &download_stats)
{
	MpiResource* mpi_res;
	for( size_t i=0; i<ResourceList.size() ; i++ ) {
		mpi_res = ResourceList[i];
		ClassAd* res_upload_file_stats = &mpi_res->m_upload_file_stats;
		ClassAd* res_download_file_stats = &mpi_res->m_download_file_stats;

		// Calculate upload_stats as a cumulation of all resource upload stats
		for (auto it = res_upload_file_stats->begin(); it != res_upload_file_stats->end(); it++) {
			const std::string& attr = it->first;

			// Lookup the value of this attribute. We only count integer values.
			classad::Value attr_val;
			long long this_val;
			it->second->Evaluate(attr_val);
			if (!attr_val.IsIntegerValue(this_val)) {
				continue;
			}

			// Lookup the previous value if it exists
			long long prev_val = 0;
			upload_stats.LookupInteger(attr, prev_val);

			upload_stats.InsertAttr(attr, prev_val + this_val);
		}

		// Calculate download_stats as a cumulation of all resource download stats
		for (auto it = res_download_file_stats->begin(); it != res_download_file_stats->end(); it++) {
			const std::string& attr = it->first;

			// Lookup the value of this attribute. We only count integer values.
			classad::Value attr_val;
			long long this_val;
			it->second->Evaluate(attr_val);
			if (!attr_val.IsIntegerValue(this_val)) {
				continue;
			}

			// Lookup the previous value if it exists
			long long prev_val = 0;
			download_stats.LookupInteger(attr, prev_val);

			download_stats.InsertAttr(attr, prev_val + this_val);
		}
	}
}

void
ParallelShadow::getFileTransferStatus(FileTransferStatus &upload_status,FileTransferStatus &download_status)
{
	MpiResource* mpi_res;
	for( size_t i=0; i<ResourceList.size() ; i++ ) {
		mpi_res = ResourceList[i];
		FileTransferStatus this_upload_status = XFER_STATUS_UNKNOWN;
		FileTransferStatus this_download_status = XFER_STATUS_UNKNOWN;
		mpi_res->getFileTransferStatus(this_upload_status,this_download_status);

		if( this_upload_status == XFER_STATUS_ACTIVE || upload_status == XFER_STATUS_UNKNOWN ) {
			upload_status = this_upload_status;
		}
		if( this_download_status == XFER_STATUS_ACTIVE || download_status == XFER_STATUS_UNKNOWN ) {
			download_status = this_download_status;
		}
	}
}

int
ParallelShadow::getExitReason( void )
{
	if( ResourceList.size() && ResourceList[0] ) {
		return ResourceList[0]->getExitReason();
	}
	return -1;
}


bool
ParallelShadow::setMpiMasterInfo( char*   /*str*/ )
{
	return false;
}


bool
ParallelShadow::exitedBySignal( void )
{
	if( ResourceList.size() && ResourceList[0] ) {
		return ResourceList[0]->exitedBySignal();
	}
	return false;
}


int
ParallelShadow::exitSignal( void )
{
	if( ResourceList.size() && ResourceList[0] ) {
		return ResourceList[0]->exitSignal();
	}
	return -1;
}


int
ParallelShadow::exitCode( void )
{
	if( ResourceList.size() && ResourceList[0] ) {
		return ResourceList[0]->exitCode();
	}
	return -1;
}


void
ParallelShadow::resourceBeganExecution( RemoteResource* rr )
{
	bool all_executing = true;

	for( size_t i=0; i<ResourceList.size() && all_executing ; i++ ) {
		if( ResourceList[i]->getResourceState() != RR_EXECUTING ) {
			all_executing = false;
		}
	}

	if( all_executing ) {
			// All nodes in this computation are now running, so we 
			// can finally log the execute event.
		ExecuteEvent event;
		event.setExecuteHost( "MPI_job" );
		if ( !uLog.writeEvent( &event, jobAd )) {
			dprintf ( D_ALWAYS, "Unable to log EXECUTE event.\n" );
		}
		
			// Now that everything is started, we can finally invoke
			// the base copy of this function to handle shared code.
		BaseShadow::resourceBeganExecution(rr);
	}
}


void
ParallelShadow::resourceReconnected( RemoteResource* /* rr */ )
{
		// Since our reconnect worked, clear attemptingReconnectAtStartup
		// flag so if we disconnect again and fail, we will exit
		// with JOB_SHOULD_REQUEUE instead of JOB_RECONNECT_FAILED.
		// See gt #4783.

	// If the shadow started in reconnect mode, check the job ad to see
	// if we previously heard about the job starting execution, and set
	// up our state accordingly.
	// But wait until we've reconnected to all starters.
	if (attemptingReconnectAtStartup) {
		bool all_connected = true;

		for( size_t i=0; i<ResourceList.size() && all_connected; i++ ) {
			if( ResourceList[i]->getResourceState() == RR_RECONNECT ) {
				all_connected = false;
			}
		}
		if (all_connected) {
			time_t job_execute_date = 0;
			time_t claim_start_date = 0;
			jobAd->LookupInteger(ATTR_JOB_CURRENT_START_EXECUTING_DATE, job_execute_date);
			jobAd->LookupInteger(ATTR_JOB_CURRENT_START_DATE, claim_start_date);
			if ( job_execute_date >= claim_start_date ) {
				began_execution = true;
			}

			attemptingReconnectAtStartup = false;
		}
	}

	// Start the timer for the periodic user job policy
	shadow_user_policy.startTimer();

		// If we know the job is already executing, ensure the timers
		// that are supposed to start then are running.
	if (began_execution) {
			// Start the timer for updating the job queue for this job
		startQueueUpdateTimer();
	}
}


void
ParallelShadow::logDisconnectedEvent( const char* reason )
{
	JobDisconnectedEvent event;
	if (reason) { event.setDisconnectReason(reason); }

/*
	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.setStartdAddr( dc_startd->addr() );
	event.setStartdName( dc_startd->name() );

	if( !uLog.writeEvent(&event,jobAd) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_DISCONNECTED event\n" );
	}
*/
}


void
ParallelShadow::logReconnectedEvent( void )
{
	JobReconnectedEvent event;

/*
	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.setStartdAddr( dc_startd->addr() );
	event.setStartdName( dc_startd->name() );

	char* starter = NULL;
	remRes->getStarterAddress( starter );
	event.setStarterAddr( starter );
	free( starter );
	starter = NULL;

*/
	if( !uLog.writeEvent(&event,jobAd) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RECONNECTED event\n" );
	}

}


void
ParallelShadow::logReconnectFailedEvent( const char* reason )
{
	JobReconnectFailedEvent event;

	if (reason) { event.setReason(reason); }

/*
	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.setStartdName( dc_startd->name() );
*/

	if( !uLog.writeEvent(&event,jobAd) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RECONNECT_FAILED event\n" );
	}
		//EXCEPT( "impossible: MPIShadow doesn't support reconnect" );
}

bool
ParallelShadow::updateJobAttr( const char *name, const char *expr, bool log )
{
	return job_updater->updateAttr( name, expr, true, log );
}

bool
ParallelShadow::updateJobAttr( const char *name, int64_t value, bool log )
{
	return job_updater->updateAttr( name, value, true, log );
}

