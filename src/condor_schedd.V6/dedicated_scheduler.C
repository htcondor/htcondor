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

/*
  TODO:

  1) Really handle multiple procs per cluster, throughout the module
  2) Be smarter about dealing with things in the future
  3) Have a real scheduling algorithm
*/

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dedicated_scheduler.h"
#include "scheduler.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_qmgr.h"
#include "condor_query.h"
#include "condor_adtypes.h"
#include "condor_state.h"
#include "condor_string.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "get_full_hostname.h"
#include "proc.h"
#include "exit.h"

extern Scheduler scheduler;

extern void mark_job_running(PROC_ID*);
extern void mark_job_stopped(PROC_ID*);


/*
  Stash this value as a static variable to this whole file, since we
  use it again and again.  Everytime we call handleDedicatedJobs(), we
  update the value
*/
static time_t now;

const int DedicatedScheduler::MPIShadowSockTimeout = 60;


//////////////////////////////////////////////////////////////
//  AllocationNode
//////////////////////////////////////////////////////////////

AllocationNode::AllocationNode( int cluster_id, int n_procs )
{
	cluster = cluster_id;
	num_procs = n_procs;
	capability = NULL;
	status = A_NEW;
	num_resources = 0;

	jobs = new ExtArray< ClassAd* >(num_procs);
	matches = new ExtArray< MRecArray* >(num_procs);
	jobs->fill(NULL);
	jobs->truncate(-1);
	matches->fill(NULL);
	matches->truncate(-1);
}


AllocationNode::~AllocationNode()
{
	int i;
	for( i=0; i<num_procs; i++ ) {
		if( (*matches)[i] ) {
			delete (*matches)[i];
		}
	}
	delete jobs;
	delete matches;
	if( capability ) {
		delete [] capability;
	}
}


void
AllocationNode::setCapability( const char* new_capab )
{
	if( capability ) {
		delete [] capability;
	}
	capability = strnewp( new_capab );
}		


//////////////////////////////////////////////////////////////
//  AvailTimeList
//////////////////////////////////////////////////////////////

// Print out the contents of the list for debugging purposes
void
AvailTimeList::display( int debug_level )
{
	Item<ResTimeNode> *cur;
	ResTimeNode *tmp;
	ClassAd *res;

	dprintf( debug_level, "Begining AvailTimeList::display()\n" );

		// Stash our current pointer, so we don't mess up any on-going
		// iterations
	cur = current;
	Rewind();
	while( (tmp = Next()) ) {
		dprintf( debug_level, "Resources available at time %d:\n", 
				 tmp->time );
		tmp->res_list->Rewind();
		while( (res = tmp->res_list->Next()) ) {
			displayResource( res, "   ", debug_level );
		}
	}
	dprintf( debug_level, "Finished AvailTimeList::display()\n" );
}


void
AvailTimeList::addResource( match_rec* mrec )
{
	ResTimeNode *rtn, *tmp;
	char buf[128];
	bool wants_insert = false;
	ClassAd* resource = mrec->my_match_ad;
	
	time_t t = findAvailTime( mrec );
	sprintf( buf, "in addResource() - Time: %d ", (int)t );
	displayResource( resource, buf, D_FULLDEBUG );
	
	Rewind();
	while( (tmp = Next()) ) {
		if( tmp->time < t ) {
			continue;
		}
		if( tmp->time == t ) { 
				// Found one with the same time already, just insert
				// this resource into that ResTimeNode's list
			dprintf( D_FULLDEBUG, "Already have an entry for time %d\n",
					 (int)t );
			tmp->res_list->Insert( resource );
				// We're done, so reset the current pointer and return 
			return;
		} else {
				/*
				  There's an entry for a later time, but not ours, so
				  we want to insert our new node before this one.  Set
				  a flag so we call Insert(), which uses the current
				  position, instead of Append(), which adds it to the
				  end of the list.  We have to do this since calling
				  Insert() when we're at the end of the list inserts
				  it before the last item, not at the end of the list!
				  Thanks, list template. *grr*
				*/
			wants_insert = true;
			break;
		}
	}

		/*
		  If we got here, it's because we need to insert a new
		  ResTimeNode into the list for this timestamp.  Either the
		  list ran out of entries, or we bailed out early b/c we have
		  an entry for a later timestamp.  So, just create a new
		  ResTimeNode and insert it into our current position in the
		  list.
		*/
	dprintf( D_FULLDEBUG, "Creating new entry for time %d\n",
			 (int)t );
	rtn = new ResTimeNode(t);
	rtn->res_list->Insert( resource );
	if( wants_insert ) {
		Insert( rtn );
	} else {
		Append( rtn );
	}
}


bool
AvailTimeList::hasAvailResources( void )
{
	ResTimeNode* rtn;
	if( ! (rtn = Head()) ) {
			// The list is empty
		return false;
	}
		// If there's a non-zero time at the head, there's nothing
		// available right now
	return rtn->time ? false : true;
}


void
AvailTimeList::removeResource( ClassAd* resource, ResTimeNode* rtn ) 
{
	rtn->res_list->Delete( resource );
	if( rtn->res_list->Length() == 0 ) {
			// That was the last resource for the given ResTimeNode,
			// so we need to delete that node from our list entirely. 
		if( Current() == rtn ) {
			DeleteCurrent();
		} else {
			Delete( rtn );
		}
	}
}


AvailTimeList::~AvailTimeList()
{
	ResTimeNode* rtn;
	Rewind();
	while( (rtn = Next()) ) {
		delete rtn;
		DeleteCurrent();
	}
}


//////////////////////////////////////////////////////////////
//  ResTimeNode
//////////////////////////////////////////////////////////////

ResTimeNode::ResTimeNode( time_t t)
{
	time = t;
	res_list = new CAList;
	num_matches = 0;
}


ResTimeNode::~ResTimeNode()
{
	delete res_list;
}


bool
ResTimeNode::satisfyJob( ClassAd* job, int max_hosts, 
						 CAList* candidates ) 
{
	ClassAd* candidate;
	
	dprintf( D_FULLDEBUG, "Checking resources available at time %d\n", 
			 (int)time );

	res_list->Rewind();
	num_matches = 0;
	while( (candidate = res_list->Next()) ) {
		if( *candidate == *job ) {
				// There's a match
			candidates->Insert( candidate );
			num_matches++;
		}
		if( num_matches == max_hosts ) {
				// We've found all we need for this job
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////
//  DedicatedScheduler
//////////////////////////////////////////////////////////////

DedicatedScheduler::DedicatedScheduler()
{
	idle_jobs = NULL;
	avail_time_list = NULL;
	resources = NULL;
	scheduling_interval = 0;
	scheduling_tid = -1;

		// TODO: Be smarter about the sizes of these tables
	allocations = new HashTable < int, AllocationNode*> 
		( 199, hashFuncInt );
	all_matches = new HashTable < HashKey, match_rec*>
		( 199, hashFunction );

	num_matches = 0;

	ds_owner = strnewp( "DedicatedScheduler" );
	char buf[256];
	sprintf( buf, "%s@%s", ds_owner, my_full_hostname() );
	ds_name = strnewp( buf );
}


DedicatedScheduler::~DedicatedScheduler()
{
	if(	idle_jobs ) delete idle_jobs;
	if(	resources ) delete resources;
	if(	avail_time_list ) delete avail_time_list;
	if( ds_owner ) delete [] ds_owner;
	if( ds_name ) delete [] ds_name;

        // for the stored claim records
	AllocationNode* foo;
	match_rec* tmp;
    allocations->startIterations();
    while( allocations->iterate( foo ) ) {
        delete foo;
	}
    delete allocations;

	all_matches->startIterations();
    while( all_matches->iterate( tmp ) ) {
        delete tmp;
	}
	delete all_matches;
}


int
DedicatedScheduler::initialize( void )
{
	char buf[256];

		// First, call our reconfig() method, since that will register 
		// the DaemonCore timer for us, if we need it at all.
	this->reconfig();

		// Next, fill in the dummy job ad we're going to send to 
		// startds for claiming them.
	dummy_job.SetMyTypeName( JOB_ADTYPE );
	dummy_job.SetTargetTypeName( STARTD_ADTYPE );
	sprintf( buf, "%s = True", ATTR_REQUIREMENTS );
	dummy_job.Insert( buf );
	sprintf( buf, "%s = \"%s\"", ATTR_OWNER, ds_owner );
	dummy_job.Insert( buf );
	sprintf( buf, "%s = \"%s\"", ATTR_USER, ds_name );
	dummy_job.Insert( buf );
	sprintf( buf, "%s = %d", ATTR_JOB_UNIVERSE, MPI );
	dummy_job.Insert( buf );
	sprintf( buf, "%s = %d", ATTR_JOB_STATUS, IDLE );
	dummy_job.Insert( buf );
	config_fill_ad( &dummy_job );

		// Next, register our special MPI shadow reaper
	rid = daemonCore->
		Register_Reaper( "MPI reaper", 
						 (ReaperHandlercpp)&DedicatedScheduler::reaper,
						 "DedicatedScheduler::reaper", this );
	if( rid <= 0 ) {
			// This is lame, but Register_Reaper returns FALSE on
			// failure, even though it seems like reaper id 0 is
			// valid... who knows.
		EXCEPT( "Can't register daemonCore reaper!" );
	}

		// Now, register a handler for the special command that the
		// MPI shadow sends us if it needs to get information about
		// the matches for a given cluster.proc
	daemonCore->
		Register_Command( GIVE_MATCHES, "GIVE_MATCHES", 
						  (CommandHandlercpp)&DedicatedScheduler::giveMatches,
						  "DedicatedScheduler::giveMatches", this, 
						  IMMEDIATE_FAMILY );

		// Now, go out and claim all the resources we care about.
	ClassAd* r;
	getDedicatedResourceInfo();
	resources->Rewind();
	while( (r = resources->Next()) ) { 
		requestClaim( r );
	}
	return TRUE;
}


int
DedicatedScheduler::reconfig( void )
{
	char* tmp;
	int old_int = scheduling_interval;

	tmp = param( "DEDICATED_SCHEDULING_INTERVAL" );
	if( ! tmp ) {
		scheduling_interval = 300;
	} else {
		 scheduling_interval = atoi( tmp );
		free( tmp );
	}

	if( old_int != scheduling_interval ) {
		if( scheduling_tid != -1 ) {
			daemonCore->Cancel_Timer( scheduling_tid );
		}
		scheduling_tid = daemonCore->
			Register_Timer( scheduling_interval, scheduling_interval,
							(Eventcpp)&DedicatedScheduler::handleDedicatedJobs,
							"handleDedicatedJobs", this );
	}

		// TODO: What about changing the resource list?

	return TRUE;
}


int
DedicatedScheduler::shutdown_fast( void )
{
		// TODO: any other cleanup?
	match_rec* mrec;
	all_matches->startIterations();
    while( all_matches->iterate( mrec ) ) {
		releaseClaim( mrec, false );
	}
	return TRUE;
}


int
DedicatedScheduler::shutdown_graceful( void )
{
		// TODO: any other cleanup?
	match_rec* mrec;
	all_matches->startIterations();
    while( all_matches->iterate( mrec ) ) {
		releaseClaim( mrec, true );
	}
	return TRUE;
}


/*
  This function is used to request a claim on a startd we're supposed
  to be able to control.  It sets up a bunch of dummy stuff, then
  calls claimStartd() to actually do the work.
*/
bool
DedicatedScheduler::requestClaim( ClassAd* r )
{
	char buf[128];
	char* cap;
	match_rec* m_rec;
	PROC_ID jobId;
	ClassAd* copy;

		// Dummy values for now
	jobId.proc = -1;
	jobId.cluster = -1;

	if( ! r->LookupString(ATTR_STARTD_IP_ADDR, buf) ) {
		dprintf( D_ALWAYS, "ERROR in requestClaim: "
				 "no %s in classad!\n", ATTR_STARTD_IP_ADDR );
		return false;
	}

		// First, make up the capability string for this resource.
		// This allocates memory, so beware.  We also grab the sinful
		// string while we're at it, just to avoid duplicate efforts. 
	cap = makeCapability( r, buf );

		// We need to make our own copy of the resource ad to put in
		// the match record, since the match_rec destructor will
		// delete it automatically.
	copy = new ClassAd( *r );

		// Now, create a match_rec for this resource and fill in the
		// fields we already know about
	m_rec = new match_rec( cap, buf, &jobId, copy, NULL, NULL );

		// the match_rec has its own copy of this, so we can safely
		// free it here
	free( cap );

		// Next, insert this match_rec into our hashtable
	all_matches->insert( HashKey(m_rec->id), m_rec );
	num_matches++;

	if( claimStartd(m_rec, &dummy_job, true) ) {
		return true;
	} else {
		m_rec->status = M_UNCLAIMED;
		return false;
	}
}


// Before each bad return we check to see if there's a pending call in
// the contact queue.
#define BAILOUT                        \
        mrec->status = M_UNCLAIMED;    \
		scheduler.checkContactQueue(); \
		return FALSE;

int
DedicatedScheduler::startdContactSockHandler( Stream *sock )
{
		// all return values are non - KEEP_STREAM.  
		// Therefore, DaemonCore will cancel this socket at the
		// end of this function, which is exactly what we want!

	int reply;

	dprintf( D_FULLDEBUG, "In Scheduler::startdContactSockHandler\n" );

		// since all possible returns from here result in the socket being
		// cancelled, we begin by decrementing the # of contacts.
	scheduler.delRegContact();

		// fetch the match record.  the daemon core DataPtr specifies the
		// id of the match (which is really the startd capability).  use
		// this id to pull out the actual mrec from our hashtable.
	char *id = (char *) daemonCore->GetDataPtr();
	match_rec *mrec = NULL;
	if( id ) {
		HashKey key(id);
		all_matches->lookup(key, mrec);
			// the id was allocated with strdup() when
			// Register_DataPtr() was called 
		free(id);	
	}

	if( !mrec ) {
		// The match record must have been deleted.  Nothing left to do, close
		// up shop.
		dprintf( D_FULLDEBUG, "startdContactSockHandler(): mrec not found\n" );
		scheduler.checkContactQueue();
		return FALSE;
	}
	
	mrec->status = M_CLAIMED; // we assume things will work out.

	// Now, we set the timeout on the socket to 1 second.  Since we 
	// were called by as a Register_Socket callback, this should not 
	// block if things are working as expected.  
	// However, if the Startd wigged out and sent a 
	// partial int or some such, we cannot afford to block. -Todd 3/2000
	sock->timeout(1);

 	if( !sock->rcv_int(reply, TRUE) ) {
		dprintf( D_ALWAYS, "Response problem from startd.\n" );	
		BAILOUT;
	}

	if( reply == OK ) {
		dprintf (D_PROTOCOL, "(Request was accepted)\n");
		if( !mrec->sent_alive_interval ) {
	 		sock->encode();
			if( !sock->put(scheduler.dcSockSinful()) ) {
				dprintf( D_ALWAYS, "Couldn't send schedd string to startd.\n");
				BAILOUT;
			}
			if( !sock->snd_int(scheduler.aliveInterval(), TRUE) ) {
				dprintf( D_ALWAYS, "Couldn't send aliveInterval to startd.\n");
				BAILOUT;
			}
		}
	} else if( reply == NOT_OK ) {
		dprintf( D_PROTOCOL, "(Request was NOT accepted)\n" );
		BAILOUT;
	} else {
		dprintf( D_ALWAYS, "Unknown reply from startd.\n");
		BAILOUT;
	}

	scheduler.checkContactQueue();

	return TRUE;
}
#undef BAILOUT



bool
DedicatedScheduler::releaseClaim( match_rec* m_rec, bool use_tcp = true )
{
	Sock *sock;
	if( use_tcp ) {
		ReliSock rsock;
		sock = &rsock;
	} else {
		SafeSock ssock;
		sock = &ssock;
	}
	sock->timeout(2);
	sock->connect( m_rec->peer );
	sock->encode();
	sock->put( RELEASE_CLAIM );
	sock->put( m_rec->id );
	sock->end_of_message();

	DelMrec( m_rec );
	return true;
}


bool
DedicatedScheduler::deactivateClaim( match_rec* m_rec )
{
	int cmd = DEACTIVATE_CLAIM;
	ReliSock sock;

	sock.timeout( STARTD_CONTACT_TIMEOUT );

	if( !sock.connect(m_rec->peer, 0) ) {
        dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Couldn't connect to startd.\n" );
		return false;
	}
	sock.encode();
	if( !sock.put(cmd) ) {
        dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Can't code cmd int (%d)\n", cmd );
		return false;
	}
	if( !sock.put(m_rec->id) ) {
        dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Can't code capability (%s)\n", m_rec->id );
		return false;
	}
	if( !sock.end_of_message() ) {
        dprintf( D_ALWAYS, "ERROR in deactivateClaim(): "
				 "Can't send EOM\n" );
		return false;
	}
	sock.close();
	
		// Now, just update the fields in our m_rec.
	m_rec->cluster = -1;
	m_rec->proc = -1;
	m_rec->shadowRec = NULL;
	m_rec->num_exceptions = 0;
		// Status is no longer active, but we're still claimed
	m_rec->status = M_CLAIMED;
	m_rec->allocated = false;

	return true;
}


void
DedicatedScheduler::sendAlives( void )
{
	match_rec	*mrec;
	int		  	numsent=0;

	all_matches->startIterations();
	while( all_matches->iterate(mrec) == 1 ) {
		if( mrec->status == M_ACTIVE || mrec->status == M_CLAIMED ) {
			if( sendAlive( mrec ) ) {
				numsent++;
			}
		}
	}
			
	dprintf( D_PROTOCOL, "## 6. (Done sending alive messages to "
			 "%d dedicated startds)\n", numsent );
}

int
DedicatedScheduler::reaper( int pid, int status )
{
	shadow_rec*		srec;

	srec = scheduler.FindSrecByPid( pid );
	if( !srec ) {
			// Can't find the shadow record!  This is bad.
		dprintf( D_ALWAYS, "ERROR: Can't find shadow record for pid %d!\n" 
				 "\t\tAborting DedicatedScheduler::reaper()\n", pid );
		return FALSE;
	}

	if ( daemonCore->Was_Not_Responding(pid) ) {
			// this shadow was killed by daemon core because it was hung.
			// make the schedd treat this like a Shadow Exception so job
			// just goes back into the queue as idle, but if it happens
			// to many times we relinquish the match.
		dprintf( D_FULLDEBUG,
				 "Shadow pid %d successfully killed because it was hung.\n"
				 ,pid);
		status = JOB_EXCEPTION;
	}
	if( WIFEXITED(status) ) {			
		dprintf( D_FULLDEBUG, "Shadow pid %d exited with status %d\n",
				 pid, WEXITSTATUS(status) );

		switch( WEXITSTATUS(status) ) {
		case JOB_NO_MEM:
			scheduler.swap_space_exhausted();
		case JOB_EXEC_FAILED:
			break;
		case JOB_CKPTED:
		case JOB_NOT_CKPTED:
		case JOB_NOT_STARTED:
			if (!srec->removed) {
				nukeMpi( srec );
			}
			break;
		case JOB_SHADOW_USAGE:
			EXCEPT("shadow exited with incorrect usage!\n");
			break;
		case JOB_BAD_STATUS:
			EXCEPT("shadow exited because job status != RUNNING");
			break;
		case JOB_NO_CKPT_FILE:
		case JOB_KILLED:
		case JOB_COREDUMPED:
			set_job_status( srec->job_id.cluster, srec->job_id.proc, 
							REMOVED );
			break;
		case JOB_EXITED:
			dprintf(D_FULLDEBUG, "Reaper: JOB_EXITED\n");
			set_job_status( srec->job_id.cluster, srec->job_id.proc,
							COMPLETED );
			nukeMpi( srec );
			break;
		case JOB_SHOULD_HOLD:
			dprintf( D_ALWAYS, "Putting job %d.%d on hold\n",
					 srec->job_id.cluster, srec->job_id.proc );
			set_job_status( srec->job_id.cluster, srec->job_id.proc, 
							HELD );
			nukeMpi( srec );
			break;
		case DPRINTF_ERROR:
			dprintf( D_ALWAYS, 
					 "ERROR: Shadow had fatal error writing to its log file.\n" );
				// We don't want to break, we want to fall through 
				// and treat this like a shadow exception for now.
		case JOB_EXCEPTION:
			if ( WEXITSTATUS(status) == JOB_EXCEPTION ){
				dprintf( D_ALWAYS,
						 "ERROR: Shadow exited with job exception code!\n");
			}
				// We don't want to break, we want to fall through 
				// and treat this like a shadow exception for now.
		default:
				/* the default case is now a shadow exception in case ANYTHING
				   goes wrong with the shadow exit status */
			if ( (WEXITSTATUS(status) != DPRINTF_ERROR) &&
				 (WEXITSTATUS(status) != JOB_EXCEPTION) )
				{
					dprintf( D_ALWAYS,
							 "ERROR: Shadow exited with unknown value!\n");
				}
				// record that we had an exception.  This function will
				// relinquish the match if we get too many
				// exceptions 
			if( !srec->removed ) {
				nukeMpi( srec );
				scheduler.HadException(srec->match);
			}
			break;
		}
	} else if( WIFSIGNALED(status) ) {
		dprintf( D_ALWAYS, "Shadow pid %d died with %s\n",
				 pid, daemonCore->GetExceptionString(status) );
	}
	scheduler.delete_shadow_rec( pid );
	return TRUE;
}


/* 
   This command handler reads a cluster int and capability off the
   wire, and replies with the number of matches for that job, followed
   by the capabilities for each match, and an EOF.
*/

int
DedicatedScheduler::giveMatches( int, Stream* stream )
{
	int cluster;
	char *cap = NULL, *sinful = NULL;
	MRecArray* matches;
	ClassAd *job_ad;
	int i, p, last;

	dprintf( D_FULLDEBUG, "Entering DedicatedScheduler::giveMatches()\n" );

	stream->decode();
	if( ! stream->code(cluster) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "can't read cluster - aborting\n" );
			// TODO: other cleanup?
		return FALSE;
	}
	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "can't read capability - aborting\n" );
			// TODO: other cleanup?
		return FALSE;
	}
	
		// Now that we have a job id, try to find this job in our
		// table of matches, and make sure the capability is good
	AllocationNode* alloc;
	if( allocations->lookup(cluster, alloc) < 0 ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "can't find cluster %d in allocation table - aborting\n", 
				 cluster ); 
			// TODO: other cleanup?
		free( cap );
		return FALSE;
	}

		// Next, see if the capability we got matches what we have in
		// our table.

	if( strcmp(alloc->capability, cap) ) {
			// No match, abort
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::giveMatches: "
				 "incorrect capability (%s) given for cluster %d "
				 "- aborting\n", cap, cluster );
			// TODO: other cleanup?
		free( cap );
		return FALSE;
	}		

		// Now that we're done checking it, we must free the
		// capability string we got back from CEDAR, since that
		// allocated memory for us
	free( cap );
	cap = NULL;

		/*
		  If we got here, we successfully read the job id, found the
		  job in our table, and verified the correct capability for
		  the match.  So, we're good to stuff all the required info
		  onto the wire.

		  We stuff in the following form:

		  int num_procs (job "classes", for different requirements, etc)
		  for( p=0, p<num_procs, p++ ) {
		      ClassAd job_ad[p]
		      int num_matches[p]
		      for( m=0, m<num_matches, m++ ) {
		          char* sinful_string_of_startd[p][m]
		          char* capability[p][m]
		      }
		  }
		  EOM
		*/

	stream->encode();

	dprintf( D_FULLDEBUG, "Pushing %d procs for cluster %d\n",
			 alloc->num_procs, cluster );
	if( ! stream->code(alloc->num_procs) ) {
		dprintf( D_ALWAYS, "ERROR in giveMatches: can't send num procs\n" );
		return FALSE;
	} else {
		dprintf( D_ALWAYS, "giveMatches: put %d num procs on wire\n",
				 alloc->num_procs ); 
	}		

	for( p=0; p<alloc->num_procs; p++ ) {
		job_ad = new ClassAd( *((*alloc->jobs)[p]) );
		matches = (*alloc->matches)[p];
		last = matches->getlast() + 1; 
		dprintf( D_FULLDEBUG, "In proc %d, num_matches: %d", p, last );
		if( ! job_ad->put(*stream) ) {
			dprintf( D_ALWAYS, "ERROR in giveMatches: "
					 "can't send job classad for proc %d\n", p );
			delete job_ad;
			return FALSE;
		}
		delete job_ad;
		if( ! stream->code(last) ) {
			dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
					 "number of matches (%d) for proc %d\n", last, p );
			return FALSE;
		}			

		for( i=0; i<last; i++ ) {
			sinful = (*matches)[i]->peer;
			cap = (*matches)[i]->id;
			if( ! stream->code(sinful) ) {
				dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
						 "address (%s) for match %d of proc %d\n", 
						 sinful, i, p );
				return FALSE;
			}				
			if( ! stream->code(cap) ) {
				dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
						 "capability for match %d of proc %d\n", i, p );
				return FALSE;
			}				
		}
	}
	if( ! stream->end_of_message() ) {
		dprintf( D_ALWAYS, "ERROR in giveMatches: can't send "
				 "end of message\n" );
		return FALSE;
	}
	return TRUE;
}


int
DedicatedScheduler::handleDedicatedJobs( void )
{
	dprintf( D_FULLDEBUG, "Starting "
			 "DedicatedScheduler::handleDedicatedJobs\n" );

// 	now = time(NULL);
		// Just for debugging, set now to 0 to make everything easier
		// to parse when looking at log files.
	now = 0;
	dprintf( D_FULLDEBUG, "Set now to: %d\n", (int)now );

	if( ! getDedicatedJobs() ) { 
		dprintf( D_FULLDEBUG, "No dedicated jobs found, "
				 "done with handleDedicatedJobs()\n" );
			// Don't treat this as an error...
		return TRUE;
	}

	if( ! getDedicatedResourceInfo() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't get resources, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

	sortResources();

	if( ! computeSchedule() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't compute schedule, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

	if( ! spawnJobs() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't spawn jobs, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}
	
		// Now that we're done, free up the memory we allocated, so we
		// don't use up these resources when we don't need them.  If
		// we bail out early, we won't leak, since we delete all of
		// these things again before we need to create them the next
		// time around.
	delete( idle_jobs );
	idle_jobs = NULL;
	if( avail_time_list ) {
		delete avail_time_list;
		avail_time_list = NULL;
	}

	dprintf( D_FULLDEBUG, 
			 "Finished DedicatedScheduler::handleDedicatedJobs\n" );
	return TRUE;
}


// Iterate through the whole job queue, finding all idle MPI jobs 
bool
DedicatedScheduler::getDedicatedJobs( void )
{
	ClassAd *job;
	char constraint[128];
	int i;

	if( idle_jobs ) {
		delete idle_jobs;
	}
	idle_jobs = new ExtArray<ClassAd*>;
	idle_jobs->fill(NULL);
	idle_jobs->truncate(-1);

	sprintf( constraint, "%s == %d && %s == %d", ATTR_JOB_UNIVERSE, 
			 MPI, ATTR_JOB_STATUS, IDLE );

	job = GetNextJobByConstraint( constraint, 1 );	// start new interation
	if( ! job ) {
			// No dedicated jobs, we're done!
		dprintf( D_FULLDEBUG, 
				 "DedicatedScheduler::getDedicatedJobs found no jobs\n" );
		return false;
	}
		// If we're still here, we found something, so keep looking.
		// We can safely use the same iteration, since no one else
		// will have done anything between the last call and these
		// (for example, handling commands, etc).

		// We need to be very careful, since GetNextJobByConstraint()
		// is just returning pointers to the versions in the job
		// queue, and we don't want to be deleting or changing those
		// versions at all.
	i = 0;
	(*idle_jobs)[i++] = job;
	while( (job = GetNextJobByConstraint(constraint,0)) ) {
		(*idle_jobs)[i++] = job;
	}

	dprintf( D_FULLDEBUG, "Found %d dedicated job(s)\n", i );

		// Now, sort them by qdate
	qsort( &(*idle_jobs)[0], i, sizeof(ClassAd*), jobSortByDate );

		// Show the world what we've got
	listDedicatedJobs( D_FULLDEBUG );

	return true;
}
	

void
DedicatedScheduler::listDedicatedJobs( int debug_level )
{
	ClassAd* job;
	int cluster, proc;
	char owner[100];

	if( ! idle_jobs ) {
		dprintf( debug_level, "DedicatedScheduler: No dedicated jobs\n" );
		return;
	}
	dprintf( debug_level, "DedicatedScheduler: Listing all dedicated "
			 "jobs - \n" );
	int i, last = idle_jobs->getlast();
	for( i=0; i<=last; i++ ) {
		job = (*idle_jobs)[i];
		job->LookupInteger( ATTR_CLUSTER_ID, cluster );
		job->LookupInteger( ATTR_PROC_ID, proc );
		job->LookupString( ATTR_OWNER, owner );
		dprintf( debug_level, "Dedicated job: %d.%d %s\n", cluster,
				 proc, owner );
	}
}


bool
DedicatedScheduler::getDedicatedResourceInfo( void )
{
	StringList config_list;
	CondorQuery	query(STARTD_AD);

	char *tmp, constraint[256];

	if( resources ) {
		delete resources;
	}
	resources = new ClassAdList;

	tmp = param( "DEDICATED_RESOURCE_NAMES" );
	if( tmp ) {
		config_list.initializeFromString( tmp );
		free( tmp );
		config_list.rewind();
		while( (tmp = config_list.next()) ) {
			sprintf( constraint, "%s == \"%s\"", ATTR_MACHINE, tmp );
			query.addORConstraint( constraint );
		}
			// This should fill in resources with all the classads we
			// care about
		query.fetchAds( *resources );
	} else {
		dprintf( D_ALWAYS, "No DEDICATED_RESOURCE_NAMES in config file\n"
				 "DedicatedScheduler::getDedicatedResourceInfo aborting\n" );
		return false;
	}

	dprintf( D_FULLDEBUG, "Found %d dedicated resources\n",
			 resources->Length() );
	listDedicatedResources( D_FULLDEBUG, resources );

	return true;
}


void
DedicatedScheduler::sortResources( void )
{
	ClassAd* res;
	char* cap;
	match_rec* mrec;

		// We want to sort all the resources we have into the
		// avail_time_list
	if( avail_time_list ) {
		delete avail_time_list;
	}
	avail_time_list = new AvailTimeList;
	resources->Rewind();
	while( (res = resources->Next()) ) {
		cap = makeCapability( res );
		HashKey key(cap);
		free( cap );
		if( all_matches->lookup(key, mrec) < 0 ) {
				// We don't have a match_rec for this resource yet, so
				// request one
			requestClaim( res );
			continue;
		}
			// If we made it this far, this resource is already
			// claimed by us, so we can add it to our list of
			// available resources.
		avail_time_list->addResource( mrec );
	}
	avail_time_list->display( D_FULLDEBUG );
}



void
DedicatedScheduler::listDedicatedResources( int debug_level,
											ClassAdList* resources )
{
	ClassAd* ad;

	if( ! resources ) {
		dprintf( debug_level, "DedicatedScheduler: "
				 "No dedicated resources\n" );
		return;
	}
	dprintf( debug_level, "DedicatedScheduler: Listing all "
			 "dedicated resources - \n" );
	resources->Rewind();
	while( (ad = resources->Next()) ) {
		displayResource( ad, "Dedicated: ", debug_level );
	}
}


bool
DedicatedScheduler::spawnJobs( void )
{
	AllocationNode* allocation;
	char* shadow;
	char args[512];
	match_rec* mrec;
	shadow_rec* srec;
	int pid, i, n, p;
	PROC_ID id;

	if( ! allocations ) {
			// Nothing to do
		return true;
	}

		// TODO: handle multiple procs per cluster!

	shadow = param( "SHADOW_MPI" );
	if( ! shadow ) {
		dprintf( D_ALWAYS, "ERROR: SHADOW_MPI not defined in config file -- "
				 "can't spawn MPI jobs, aborting\n" );
		return false;
	}

		// Get shadow's nice increment:
    char *shad_nice = param( "SHADOW_RENICE_INCREMENT" );
    int niceness = 0;
    if ( shad_nice ) {
        niceness = atoi( shad_nice );
        free( shad_nice );
    }

	allocations->startIterations();
	while( allocations->iterate(allocation) ) {
		if( allocation->status != A_NEW ) {
			continue;
		}
		id.cluster = allocation->cluster;
		id.proc = 0;
		MRecArray* cur_matches = (*allocation->matches)[0];
		mrec = (*cur_matches)[0];
		sprintf( args, "condor_shadow -f %s %s %s %d 0", 
				 scheduler.shadowSockSinful(), mrec->peer, 
				 mrec->id, allocation->cluster );
		pid = daemonCore->
			Create_Process( shadow, args, PRIV_ROOT, rid, TRUE, NULL,
							NULL, FALSE, NULL, NULL, niceness );

		if( ! pid ) {
			// TODO: handle Create_Process failure
			dprintf( D_ALWAYS, "Failed to spawn MPI shadow for job "
					 "%d.%d\n", id.cluster, id.proc );  
			mark_job_stopped( &id );
			continue;
		}

		dprintf ( D_ALWAYS, "New MPI shadow spawned, pid = %d\n", pid );

		mark_job_running( &id );
		srec = scheduler.add_shadow_rec( pid, &id, mrec, -1 );

        SetAttributeInt( id.cluster, id.proc, ATTR_CURRENT_HOSTS,
						 allocation->num_resources );

		allocation->status = A_RUNNING;
		allocation->setCapability( mrec->id );

		     // We must set all the match recs to point at this srec.
		for( p=0; p<allocation->num_procs; p++ ) {
			n = ((*allocation->matches)[p])->getlast();
			for( i=0; i<=n; i++ ) {
				(*(*allocation->matches)[p])[i]->shadowRec = srec;
				(*(*allocation->matches)[p])[i]->status = M_ACTIVE;
			}
		}
			// TODO: Deal w/ push/pulling matches!
	}

	free( shadow );
	return true;
}


bool
DedicatedScheduler::computeSchedule( void )
{
		// Initialization
	int total_matched;
	int proc, cluster, max_hosts;
	ClassAd *job = NULL, *ad;
	CAList *candidates = NULL;
	ResTimeNode *cur;
	AllocationNode *alloc;
	char* cap;
	match_rec* mrec;
	int i, l, last;
	MRecArray* new_matches;

		// For each job, try to satisfy it as soon as possible.
	l = idle_jobs->getlast();
	for( i=0; i<=l; i++ ) {
		job = (*idle_jobs)[i];

		if( ! avail_time_list->hasAvailResources() ) {
				// We no longer have any resources that are available
				// to run jobs right now, so there's nothing more we
				// can do.
			return true;
		}

			// Grab some useful info out of the job ad
		if( ! job->LookupInteger(ATTR_CLUSTER_ID, cluster) ) {
				// ATTR_CLUSTER_ID is undefined!
			continue;
		}
		if( ! job->LookupInteger(ATTR_PROC_ID, proc) ) {
				// ATTR_PROC_ID is undefined!
			continue;
		}
		if( ! job->LookupInteger(ATTR_MAX_HOSTS, max_hosts) ) {
				// ATTR_MAX_HOSTS is undefined!
			continue;
		}

		dprintf( D_FULLDEBUG, 
				 "Trying to find %d resources for dedicated job %d.%d\n",
				 max_hosts, cluster, proc );

			// These are the potential resources for the job
		candidates = new CAList;
		
			// First, see if we can satisfy this job with resources
			// that are available right now
		total_matched = 0;
		avail_time_list->Rewind();
		cur = avail_time_list->Next();
		if( ! cur ) {
				// We ran out of resources... this job is screwed.
			dprintf( D_FULLDEBUG, "Out of resources for dedicated "
					 "job %d.%d\n", cluster, proc );
			delete candidates;
			candidates = NULL;
			continue;
		}		
		
		if( cur->satisfyJob(job, max_hosts, candidates) ) {
				// We're done with this job.  Make an allocation node
				// for it, remove the given classads from this
				// ResTimeNode, and continue

			dprintf( D_FULLDEBUG, "Satisfied job %d at time %d\n",
					 cluster, (int)cur->time );

				// TODO: Handle multiple procs!
			alloc = new AllocationNode( cluster, 1 );
			(*alloc->jobs)[0] = job;
			last = 0;
			new_matches = new MRecArray();
			new_matches->fill(NULL);
			new_matches->truncate(-1);

				// Debugging hack: allow config file to specify which
				// ip address we want the master to run on.

			char* master_ip = param( "master_ip" );
			int m_ip_len = 0;
			if( master_ip ) {
				m_ip_len = strlen( master_ip );
				last = 1;
			}
			candidates->Rewind();
			while( (ad = candidates->Next()) ) {
				cap = makeCapability( ad );
				HashKey key(cap);
				if( all_matches->lookup(key, mrec) < 0 ) {
					EXCEPT( "no match for %s in all_matches table, yet "
							"allocated to dedicated job %d.0!", cap,
							cluster ); 
				}
				free( cap );
				char* tmp_ip = &(mrec->peer)[1];
				if( master_ip && 
					! (strncmp(tmp_ip, master_ip, m_ip_len)) )
				{ 
						// found it
					(*new_matches)[0] = mrec;
				} else {
					(*new_matches)[last] = mrec;
					last++;
				}
				avail_time_list->removeResource( ad, cur );
			}
			(*alloc->matches)[0] = new_matches;
			alloc->num_resources = last;

				// Save this AllocationNode in our table
			allocations->insert( cluster, alloc );

				// Now that we're done with these candidates, we can
				// safely delete the list.
			delete candidates;
			candidates = NULL;
			continue;
		}
			// If we're here, we couldn't schedule the job right now.

			// For now, just give up on this job, delete the
			// candidates (since there weren't enough to satisfy the
			// job and we therefore can't use them), and try to
			// schedule another.
			// NOTE: This algorithm can lead to starvation of large
			// jobs in the face of lots of small jobs.

			// TODO: Try to schedule the job in the future, so we
			// avoid starvation, and don't schedule resources now that
			// will be needed later for this job.
		delete candidates;
		candidates = NULL;
	}
	return true;
}	


void
DedicatedScheduler::nukeMpi ( shadow_rec* srec )
{
		/* remove all in cluster */
	AllocationNode* alloc;
	MRecArray* matches;
	int i, n, m;

	if( allocations->lookup( srec->job_id.cluster, alloc ) != -1 ) {
		alloc->status = A_DYING;
		for( i=0; i<alloc->num_procs; i++ ) {
			matches = (*alloc->matches)[i];
			n = matches->getlast();
			for( m=0 ; m <= n ; m++ ) {
				deactivateClaim( (*matches)[m] );
			}
			matches->fill( NULL );
			matches->truncate(-1);
		}
	}
		/* it may be that the mpi shadow crashed and left around a 
		   file named 'procgroup' in the IWD of the job.  We should 
		   check and delete it here. */
	char pg_file[512];
	((*alloc->jobs)[0])->LookupString( ATTR_JOB_IWD, pg_file );  
	strcat ( pg_file, "/procgroup" );
	if ( unlink ( pg_file ) == -1 ) {
		if ( errno != ENOENT ) {
			dprintf ( D_FULLDEBUG, "Couldn't remove %s. errno %d.\n", 
					  pg_file, errno );
		}
	}
}


bool
DedicatedScheduler::DelMrec( match_rec* rec )
{
	if( ! rec ) {
		dprintf( D_ALWAYS, "Null parameter to DelMrec() -- "
				 "match not deleted\n" );
		return false;
	}
	return DelMrec( rec->id );
}


bool
DedicatedScheduler::DelMrec( char* capability )
{
	match_rec* rec;
	if( ! capability ) {
		dprintf( D_ALWAYS, "Null parameter to DelMrec() -- "
				 "claim not deleted\n" );
		return false;
	}
	HashKey key( capability );
	if( all_matches->lookup(key, rec) < 0 ) {
		dprintf( D_ALWAYS, "mrec for \"%s\" not found -- "
				 "match not deleted\n", capability );
		return false;
	}
	all_matches->remove(key);
	delete rec;
	return true;
}


//////////////////////////////////////////////////////////////
//  Utility functions
//////////////////////////////////////////////////////////////
 
// Given a pointer to a resource ClassAd, return the epoc time that
// describes when the resource will next be available.
time_t
findAvailTime( match_rec* mrec )
{
	char state[128];
	int duration, current, begin, universe, avail;
	State s;
	Activity act;
	ClassAd* resource = mrec->my_match_ad;

	resource->LookupString( ATTR_STATE, state );
	s = string_to_state( state );
	switch( s ) {

	case unclaimed_state:
	case owner_state:
	case matched_state:
			// Available to run a job right now
		return now;
		break;

	case claimed_state:
			// First, see if we've already allocated this node
		if( mrec->allocated ) {
				// TODO: Once we're smarter about the future, we need
				// to be smarter about what to do in this case, too.
			return now + 300;
		}

			// In the claimed_state, activity matters
		resource->LookupString( ATTR_ACTIVITY, state );
		act = string_to_activity( state );
		if( act == idle_act ) {
				// We're available now
			return now;
		} 
		resource->LookupInteger( ATTR_JOB_UNIVERSE, universe );
		if( ! resource->LookupInteger(ATTR_JOB_DURATION, duration) ) { 
				// Not defined, assume it's available now
				// TODO: Allow admin to provide a value for how much
				// it should "cost" to kill a job?
			if( universe == VANILLA ) {
				return now + 15;
			}
			if( universe == STANDARD ) {
				return now + 120;
			}
			return now + 60;
		} 

			// We found a duration, now just see when we started
			// running, compute the difference, and add that to the
			// current time
		if( ! resource->LookupInteger(ATTR_LAST_HEARD_FROM, current) ) { 
			dprintf( D_ALWAYS, "ERROR: no %s in resource ClassAd!\n", 
					 ATTR_LAST_HEARD_FROM );
			return now;
		}
		if( ! resource->LookupInteger(ATTR_ENTERED_CURRENT_ACTIVITY, 
									  begin) ) { 
			dprintf( D_ALWAYS, "ERROR: no %s in resource ClassAd!\n", 
					 ATTR_ENTERED_CURRENT_ACTIVITY );
			return now;
		}
		avail = now + (duration - (current - begin));
		if( avail < now ) {
			return now;
		} else {
			return avail;
		}
		break;

	case preempting_state:
			// TODO: Allow admins to tune this
		return now + 60;
		break;
	default:
			// Unknown state!
		dprintf( D_ALWAYS, 
				 "ERROR: unknown state (%d) for resource ClassAd!\n",
				 (int)s );
		break;
	}
	return now;
}


// Comparison function for sorting job ads by QDate
int
jobSortByDate( const void *ptr1, const void* ptr2 )
{
	ClassAd *ad1 = (ClassAd*)ptr1;
	ClassAd *ad2 = (ClassAd*)ptr2;

	int qdate1, qdate2;

	if( !ad1->LookupInteger (ATTR_Q_DATE, qdate1) ||
        !ad2->LookupInteger (ATTR_Q_DATE, qdate2) )
	{
        return -1;
    }

	return (qdate1 < qdate2);
}


void
displayResource( ClassAd* ad, char* str, int debug_level )
{
	char arch[128], opsys[128], name[128];
	ad->LookupString( ATTR_NAME, name );
	ad->LookupString( ATTR_OPSYS, opsys );
	ad->LookupString( ATTR_ARCH, arch );
	dprintf( debug_level, "%s%s\t%s\t%s\n", str, name, opsys, arch );
}


// This function takes a resource classAd and creates the
// corresponding capability string we can use as the dedicated
// scheduler to claim/activate it.  The string returned is memory
// allocated with malloc(), so it must be free()'ed when done.
// This returns NULL on error.
char*
makeCapability( ClassAd* resource, char* addr = NULL )
{
	char sinbuf[128];
	char* sinful;
	char buf[32];
	char* tmp;
	int vm_id;
	char* capab;

	tmp = param( "DEDICATED_SCHEDULER_CAPABILITY_STRING" );
	if( ! tmp ) {
		EXCEPT( "DEDICATED_SCHEDULER_CAPABILITY_STRING not defined!" );
	}
	if( addr ) {
		sinful = addr;
	} else {
		sinbuf[0] = '\0';
		if( ! resource->LookupString(ATTR_STARTD_IP_ADDR, sinbuf) ) {
			dprintf( D_ALWAYS, "ERROR in makeCapability: "
					 "no %s in classad!\n", ATTR_STARTD_IP_ADDR );
			return NULL;
		}
		sinful = sinbuf;
	}
	if( ! resource->LookupInteger(ATTR_VIRTUAL_MACHINE_ID, vm_id) ) {
		dprintf( D_ALWAYS, "ERROR in makeCapability: no %s in classad!\n",
				 ATTR_VIRTUAL_MACHINE_ID );
		return NULL;
	}
	sprintf( buf, "%d", vm_id );

		// Now, we've got all the info we need, so just create the
		// string:
	capab = (char*)malloc( sizeof(char) * (strlen(sinful) +
										   strlen(tmp) +
										   strlen(buf) + 3 ) );
	if( ! capab ) {
		EXCEPT( "Out of memory!" );
	}
	sprintf( capab, "%s#%s.%s", sinful, tmp, buf );
	free( tmp );
	return capab;
}



