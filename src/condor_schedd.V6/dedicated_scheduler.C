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

  * Really handle multiple procs per cluster, throughout the module
  * Be smarter about dealing with things in the future
  * Have a real scheduling algorithm
  * How do we interact w/ MAX_JOBS_RUNNING and all that stuff when
    we're negotiating for resource requests and not really jobs?
  * Flocking

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
extern char* Name;

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
	matches->fill(NULL);
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
AvailTimeList::removeResource( ClassAd* resource, ResTimeNode* &rtn ) 
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
			// Once we removed it from the list, we also need to
			// delete the ResTimeNode object itself, since the list
			// template is only dealing with pointers, and doesn't
			// deallocate the objects themselves.
		delete( rtn );
		rtn = NULL;
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
	resource_requests = NULL;
	scheduling_interval = 0;
	scheduling_tid = -1;

		// TODO: Be smarter about the sizes of these tables
	allocations = new HashTable < int, AllocationNode*> 
		( 199, hashFuncInt );
	all_matches = new HashTable < HashKey, match_rec*>
		( 199, hashFunction );
	all_matches_by_cap = new HashTable < HashKey, match_rec*>
		( 199, hashFunction );
	resource_requests = new HashTable < HashKey, ClassAd*>
		( 199, hashFunction );

	num_matches = 0;

	ds_owner = NULL;
	ds_name = NULL;
}


DedicatedScheduler::~DedicatedScheduler()
{
	if(	idle_jobs ) { delete idle_jobs; }
	if(	resources ) { delete resources; }
	if(	avail_time_list ) { delete avail_time_list; }
	if( ds_owner ) { delete [] ds_owner; }
	if( ds_name ) { delete [] ds_name; }

        // for the stored claim records
	AllocationNode* foo;
	match_rec* tmp;
    allocations->startIterations();
    while( allocations->iterate( foo ) ) {
        delete foo;
	}
    delete allocations;

		// First, delete the hashtable where we hash based on
		// capability strings.  Don't actually delete the match
		// records themselves, since we'll do those to the main
		// records stored in all_matches.
	delete all_matches_by_cap;

		// Now, we can clear out the actually match records, too. 
	all_matches->startIterations();
    while( all_matches->iterate( tmp ) ) {
        delete tmp;
	}
	delete all_matches;

	if( resource_requests ) { 
		ClassAd* ad;
		resource_requests->startIterations();
		while( resource_requests->iterate( ad ) ) {
			delete ad;
		}
		delete resource_requests;
	}
}


int
DedicatedScheduler::initialize( void )
{
	char buf[256];
	char *tmp, *tmpname;
	

		// First, figure out what ds_owner and ds_name should be.
	if( ! Name ) {
		EXCEPT( "DedicatedScheduler::initialize(): Name is NULL" ); 
	}
	tmpname = strnewp( Name );
	if( (tmp = strchr(tmpname, '@')) ) {
			// There's an '@'
		*tmp = '\0';
		sprintf( buf, "DedicatedScheduler#%s", tmpname );
	} else {
			// No '@', that's what we'll use
		sprintf( buf, "DedicatedScheduler" );
	}
	delete [] tmpname;
	ds_owner = strnewp( buf );
	sprintf( buf, "%s@%s", ds_owner, my_full_hostname() );
	ds_name = strnewp( buf );

		// Call our reconfig() method, since that will register 
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
   The dedicated scheduler's negotate() method.  This is based heavily
   on Scheduler::negotiate().  However, b/c we're not really
   negotiating for jobs, but for resource requests, and since a
   million other things are different, these have to be seperate
   methods.  However, any changes to the protocol *MUST* be changed in
   *BOTH* methods, so please be careful!
   
   Also, to handle our requests and memory in a good way, we have a
   seperate function that does the negotiation for a particular
   request, negotiateRequest().  That has all the meat of the
   negotiation protocol in it.  This function just sets everything up,
   iterates over the requests, and deals with the results of each
   negotiation.
*/
int
DedicatedScheduler::negotiate( Stream* s, char* negotiator_name )
{
		// At this point, we've already read the command int, the
		// owner to negotiate for, and an eom off the wire.  
		// Now, we've just got to handle the per-job negotiation
		// protocol itself.

	ClassAd	*req;
	HashKey	key;
	int		max_reqs;
	int		reqs_rejected = 0;
	int		reqs_matched = 0;
	int		serviced_other_commands = 0;	
	int		op;
	PROC_ID	id;
	NegotiationResult result;

		// TODO: Can we do anything smarter w/ cluster and proc with
		// all of this stuff?
	id.cluster = id.proc = 0;

	if( resource_requests ) {
		max_reqs = resource_requests->getNumElements();
	} else {
		max_reqs = 0;
	}
	
		// Create a list for unfulfilled requests
	HashTable<HashKey,ClassAd*>* unmet_requests = 
		new HashTable<HashKey, ClassAd*>( 199, hashFunction );

	resource_requests->startIterations();
	while( (resource_requests->iterate(key,req)) ) {

			// No matter what happens, we're going to be done w/ this
			// request in the current request list.  Either we'll just
			// delete the request, b/c it will have been fulfilled, or
			// we'll put it on the list of unmet requests.
		resource_requests->remove(key);

		serviced_other_commands += daemonCore->ServiceCommandSocket();
#if 0
			// TODO: All of this is different for these resource
			// request ads.  We should try to handle rm or hold, but
			// we can't do it with this mechanism.

		if ( serviced_other_commands ) {
			// we have run some other schedd command, like condor_rm
			// or condor_q, while negotiating.  check and make certain
			// the job is still runnable, since things may have
			// changed since we built the prio_rec array (like,
			// perhaps condor_hold or condor_rm was done).
			if ( Runnable(&id) == FALSE ) {
				continue;
			}
		}
#endif

		result = negotiateRequest( req, s, negotiator_name,
								   reqs_matched, max_reqs );
		switch( result ) {
		case NR_ERROR:
				// Error communicating w/ the central manager.  We
				// need to move over all the requests to the unmet
				// list, then make the unmet list our real list, then 
				// return !KEEP_STREAM to let everyone else know we
				// couldn't talk to the CM.

				// Don't break, just fall through to the END_NEGOTIATE
				// case because most of the code is shared.  We'll
				// check result again when it matters...
		case NR_END_NEGOTIATE:
				// The CM told us we had to stop negotiating.

				// First of all, the current request is still unmet. 
			unmet_requests->insert( key, req );

				// Now, the rest of the requests still in our list are
				// also unmet...
			while( (resource_requests->iterate(key,req)) ) {
				unmet_requests->insert( key, req );
				resource_requests->remove( key );
			}
			
				// Now, switch over our list of unmet requests to be
				// the main list, and deallocate the old main list. 
			delete resource_requests;
			resource_requests = unmet_requests;

				// If we had a communication error, we need to just
				// return right now w/ (!KEEP_STREAM) so we close the
				// socket and don't try to re-use the connection.
			if( result == NR_ERROR ) {
				return( (!KEEP_STREAM) );
			} else {
					// Otherwise, we were told to END_NEGOTIATE, so we
					// can safely return KEEP_STREAM and try to re-use
					// the socket the next time around
				return( KEEP_STREAM );
					// TODO: Do we want to deal w/ flocking and/or
					// other reporting before we return?
			}
			break;

		case NR_MATCHED:
				// We got matched.  We're done with this request.
			delete req;
			reqs_matched++;
				// That's all we need to do, go onto the next req. 
			break;

		case NR_REJECTED:
				// This request was rejected.  Save it in our list
				// of unmet requests.
			reqs_rejected++;
			unmet_requests->insert( key, req );

				// That's all we can do, go onto the next req.
			break;

		case NR_LIMIT_REACHED:
				// TODO!!!
				// Pretty unclear right now how, exactly, we want to
				// handle this.  For now, just EXCEPT(), since
				// negotiateRequest() shouldn't be returning this.
			EXCEPT( "DedicatedScheduler::negotiateRequest() returned "
					"unsupported value: NR_LIMIT_REACHED" );

		default:
			EXCEPT( "Unknown return value (%d) from "
					"DedicatedScheduler::negotiateRequest()",
					(int)result );
		}
	}

		/*
		  If we broke out of here, we ran out of resource requests.
		  If there's anything in unmet_requests, we want that to be
		  our new resource_requests table.  In fact, even if
		  unmet_requests is empty, we still want to use that, since we
		  need to delete one of them to avoid leaking memory, and we
		  might as well just always use unmet_requests as the requests
		  we need to service in the future.
		*/
	delete resource_requests;
	resource_requests = unmet_requests;

		/*
		  We've broken out of our loops here because we're out of
		  requests.  The Negotiator has either asked us for one more
		  job or has told us to END_NEGOTIATE.  In either case, we
		  need to pull this command off the wire and flush it down the
		  bit-bucket before we stash this socket for future use.  If
		  we got told SEND_JOB_INFO, we need to tell the negotiator we
		  have NO_MORE_JOBS. -Derek Wright 1/8/98
		*/
	s->decode();
	if( !s->code(op) || !s->end_of_message() ) {
		dprintf( D_ALWAYS, "Error: Can't read command from negotiator.\n" );
		return( !(KEEP_STREAM) );
	}		
	switch( op ) {
	case SEND_JOB_INFO: 
		if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
			dprintf( D_ALWAYS, "Can't send NO_MORE_JOBS to mgr\n" );
			return( !(KEEP_STREAM) );
		}
		break;
	case END_NEGOTIATE:
		break;
	default: 
		dprintf( D_ALWAYS, 
				 "Got unexpected command (%d) from negotiator.\n", op );
		break;
	}

	if( reqs_matched < max_reqs || reqs_rejected > 0 ) {
		dprintf( D_ALWAYS, "Out of servers - %d reqs matched, %d reqs idle, "
				 "%d reqs rejected\n", reqs_matched, max_reqs -
				 reqs_matched, reqs_rejected ); 
	} else {
		// We are out of requests
		dprintf( D_ALWAYS, "Out of requests - %d reqs matched, "
				 "%d reqs idle\n", reqs_matched, max_reqs -
				 reqs_matched ); 
	}

	return KEEP_STREAM;
}


/*
  This function handles the negotiation protocol for a given request
  ClassAd.  It returns an enum that describes the results.
*/
NegotiationResult
DedicatedScheduler::negotiateRequest( ClassAd* req, Stream* s, 
									  char* negotiator_name, 
									  int reqs_matched, int max_reqs )
{
	char	temp[512];
	char	*capability = NULL;	// capability for each match made
	char	*tmp;
	char	*sinful;
	int		perm_rval;
	int		op;
	PROC_ID	id;

		// TODO: Can we do anything smarter w/ cluster and proc with
		// all of this stuff?
	id.cluster = id.proc = 0;

	ContactStartdArgs *args;
	ClassAd *my_match_ad;

		// We're just going to return out of here when we're supposed
		// to. 
	while(1) {
			/* Wait for manager to request job info */
		
		s->decode();
		if( !s->code(op) ) {
			dprintf( D_ALWAYS, "Can't receive request from manager\n" );
			s->end_of_message();
			return NR_ERROR;
		}

			// All commands from CM during the negotiation cycle just
			// send the command followed by eom, except PERMISSION,
			// PERMISSION_AND_AD, and REJECTED_WITH_REASON. Do the
			// end_of_message here, except for those commands.
		if( (op != PERMISSION) && (op != PERMISSION_AND_AD) &&
			(op != REJECTED_WITH_REASON) ) {
			if( !s->end_of_message() ) {
				dprintf( D_ALWAYS, "Can't receive eom from manager\n" );
				return NR_ERROR;
			}
		}
		
		switch( op ) {
		case REJECTED_WITH_REASON: {
			char *diagnostic_message = NULL;
			if( !s->code(diagnostic_message) ||
				!s->end_of_message() ) {
				dprintf( D_ALWAYS,
						 "Can't receive request from manager\n" );
				return NR_ERROR;
			}
				// TODO: Identify request in logfile
			dprintf( D_FULLDEBUG, "Request rejected: %s\n",
					 diagnostic_message );
			sprintf( temp, "%s = \"%s\"",
					 ATTR_LAST_REJ_MATCH_REASON,
					 diagnostic_message );
			req->Insert( temp );
			free(diagnostic_message);
		}
			// don't break: fall through to REJECTED case
		case REJECTED:
			sprintf( temp, "%s = %d", ATTR_LAST_REJ_MATCH_TIME,
					 (int)time(0) ); 
			req->Insert( temp );

			return NR_REJECTED;
			break;

		case SEND_JOB_INFO: {
				// The Negotiator wants us to send it a job. 
#if 0
				// TODO: Figure out how all this stuff should work. 
				// For now, we just don't do the checks.

				// First, make sure we could start another
				// shadow without violating some limit.
			if( ! scheduler.canSpawnShadow(reqs_matched, max_reqs) ) { 
					// We can't start another shadow.  Tell the
					// negotiator we're done.
				if( !s->snd_int(NO_MORE_JOBS,TRUE) ) {
						// We failed to talk to the CM
					dprintf( D_ALWAYS, "Can't send NO_MORE_JOBS to mgr\n" ); 
					return NR_ERROR;
				} else {
					return NR_LIMIT_REACHED;
				}
			}
#endif // 0

				// If we got this far, we can spawn another
				// shadow, so keep going w/ our regular work. 

				/* Send a job description */
			s->encode();
			if( !s->put(JOB_INFO) ) {
				dprintf( D_ALWAYS, "Can't send JOB_INFO to mgr\n" );
				return NR_ERROR;
			}

				// request match diagnostics
			sprintf(temp, "%s = True", ATTR_WANT_MATCH_DIAGNOSTICS);
			req->Insert( temp );

					// Send the ad to the negotiator
			if( !req->put(*s) ) {
				dprintf( D_ALWAYS, "Can't send job ad to mgr\n" ); 
				s->end_of_message();
				return NR_ERROR;
			}
			if( !s->end_of_message() ) {
				dprintf( D_ALWAYS, "Can't send job eom to mgr\n" );
				return NR_ERROR;
			}

				// TODO: How to identify this request in the logs? 
			dprintf( D_FULLDEBUG, "Sent request\n" );
			break;
		}

		case PERMISSION:
		case PERMISSION_AND_AD:
				// If things are cool, contact the startd.
			dprintf ( D_FULLDEBUG, "In case PERMISSION\n" );

#if 0
			SetAttributeInt( id.cluster, id.proc,
							 ATTR_LAST_MATCH_TIME, (int)time(0) );
#endif

			if( !s->get(capability) ) {
				dprintf( D_ALWAYS, "Can't receive capability from mgr\n" ); 
				return NR_ERROR;
			}
			my_match_ad = NULL;
			if ( op == PERMISSION_AND_AD ) {
					// get startd ad from negotiator as well
				my_match_ad = new ClassAd();
				if( !my_match_ad->get(*s) ) {
					dprintf( D_ALWAYS, "Can't get my match ad from mgr\n" ); 
					delete my_match_ad;
					free( capability );
					return NR_ERROR;
				}
			}
			if( !s->end_of_message() ) {
				dprintf( D_ALWAYS, "Can't receive eom from mgr\n" );
				if( my_match_ad ) {
					delete my_match_ad;
				}
				free( capability );
				return NR_ERROR;
			}

			dprintf( D_PROTOCOL, "## 4. Received capability %s\n",
					 capability ); 

			if ( my_match_ad ) {
				dprintf( D_PROTOCOL, "Received match ad\n" );
			}

				// capability is in the form
				// "<xxx.xxx.xxx.xxx:xxxx>#xxxxxxx" 
				// where everything upto the # is the sinful
				// string of the startd
			sinful = strdup( capability );
			tmp = strchr( sinful, '#');
			if( tmp ) {
				*tmp = '\0';
			} else {
				dprintf( D_ALWAYS, "Can't find '#' in capability!\n" );
					// What else should we do here?
				free( sinful );
				free( capability );
				sinful = NULL;
				capability = NULL;
				if( my_match_ad ) {
					delete my_match_ad;
				}
				break;
			}
				// sinful should now point to the sinful string of the
				// startd we were matched with.
			
				/* 
				   Here we don't want to call contactStartd directly
				   because we do not want to block the negotiator for
				   this, and because we want to minimize the
				   possibility that the startd will have to block/wait
				   for the negotiation cycle to finish before it can
				   finish the claim protocol.  So...we enqueue the
				   args for a later call.  (The later call will be
				   made from the startdContactSockHandler)
				*/

				// Note, we want to claim this startd as the
				// "DedicatedScheduler" owner, which is why we call
				// owner() here...
			args = new ContactStartdArgs( capability, owner(),
										  sinful, id, my_match_ad,	
										  negotiator_name, true );

				// Now that the info is stored in the above
				// object, we can deallocate all of our strings
				// and other memory.

			free( sinful );
			sinful = NULL;
			free( capability );
			capability = NULL;
			if( my_match_ad ) {
				delete my_match_ad;
				my_match_ad = NULL;
			}
				
			if( scheduler.enqueueStartdContact(args) ) {
				perm_rval = 1;	// happiness
			} else {
				perm_rval = 0;	// failure
				delete( args );
			}
				
			reqs_matched += perm_rval;
#if 0 
				// Once we've got all this buisness w/ "are we
				// spawning a new shadow?" worked out, we'll need to
				// do this so our MAX_JOBS_RUNNING check still works. 
			scheduler.addActiveShadows( perm_rval *
										shadow_num_increment );  
#endif

				// We're done, return success
			return NR_MATCHED;
			break;

		case END_NEGOTIATE:
			dprintf( D_ALWAYS, "Lost priority - %d requests matched\n", 
					 reqs_matched );

			return NR_END_NEGOTIATE;
			break;

		default:
			dprintf( D_ALWAYS, "Got unexpected request (%d)\n", op );
			return NR_ERROR;
			break;
		}
	}
	EXCEPT( "while(1) loop exited!" );
	return NR_ERROR;
}


/*
  This function is used to request a claim on a startd we're supposed
  to be able to control.  It creates a match record for it, stores
  that in our table, then calls claimStartd() to actually do the
  work of the claiming protocol.
*/
bool
DedicatedScheduler::contactStartd( ContactStartdArgs *args )
{

	dprintf( D_FULLDEBUG, "In DedicatedScheduler::contactStartd()\n" );

    dprintf( D_FULLDEBUG, "%s %s %s %d.%d\n", args->capability(), 
			 args->owner(), args->sinful(), args->cluster(),
			 args->proc() ); 

	match_rec* m_rec;
	PROC_ID id;

		// Dummy values for now
	id.cluster = -1;
	id.proc = -1;

		// Now, create a match_rec for this resource
	m_rec = new match_rec( args->capability(), args->sinful(), &id,
						   args->matchAd(), args->owner(),
						   args->pool() );  

	char buf[256];
	buf[0] = '\0';
	if( ! m_rec->my_match_ad->LookupString(ATTR_NAME, buf) ) {
		dprintf( D_ALWAYS, "ERROR: No %s in resource ad: "
				 "Aborting contactStartd()\n", ATTR_NAME );
		delete m_rec;
		return false;
	}

		// Next, insert this match_rec into our hashtables
	all_matches->insert( HashKey(buf), m_rec );
	all_matches_by_cap->insert( HashKey(m_rec->id), m_rec );
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
	char *cap = (char *) daemonCore->GetDataPtr();
	match_rec *mrec = NULL;
	if( cap ) {
		HashKey key(cap);
		all_matches_by_cap->lookup(key, mrec);
			// the id was allocated with strdup() when
			// Register_DataPtr() was called 
		free(cap);	
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

		// These two sock variables have to live in the top-level
		// stack frame, since if you put them inside the if() below
		// where we set what sock points to, they go out of scope,
		// right away, get destructed, etc. :(
	ReliSock rsock;
	SafeSock ssock;

	if( use_tcp ) {
		sock = &rsock;
	} else {
		sock = &ssock;
	}
	sock->timeout(2);
	sock->connect( m_rec->peer );
	sock->encode();
	sock->put( RELEASE_CLAIM );
	sock->put( m_rec->id );
	sock->end_of_message();

	if( DebugFlags & D_FULLDEBUG ) { 
		char name_buf[256];
		name_buf[0] = '\0';
		m_rec->my_match_ad->LookupString( ATTR_NAME, name_buf );
		dprintf( D_FULLDEBUG, "DedicatedScheduler: releasing claim on %s\n", 
				 name_buf );
	}

	DelMrec( m_rec );
	return true;
}


bool
DedicatedScheduler::deactivateClaim( match_rec* m_rec )
{
	int cmd = DEACTIVATE_CLAIM;
	ReliSock sock;

	if( ! m_rec ) {
        dprintf( D_ALWAYS, "ERROR in deactivateClaim(): NULL m_rec\n" ); 
		return false;
	}

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
	
		// Clear out this match rec, since it's no longer allocated to
		// a given MPI job.
	deallocMatchRec( m_rec );

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
	int q_status;  // status of this job in the queue

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
		dprintf( D_ALWAYS, 
				 "Shadow pid %d successfully killed because it was hung.\n", 
				 pid );
		status = JOB_EXCEPTION;
	}

	if( GetAttributeInt(srec->job_id.cluster, srec->job_id.proc, 
						ATTR_JOB_STATUS, &q_status) < 0 ) {
		EXCEPT( "ERROR no job status for %d.%d in "
				"DedicatedScheduler::reaper()!",
				srec->job_id.cluster, srec->job_id.proc );
	}

	if( WIFEXITED(status) ) {			
		dprintf( D_ALWAYS, "Shadow pid %d exited with status %d\n",
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
				shutdownMpiJob( srec );
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
			if( q_status != HELD ) {
				set_job_status( srec->job_id.cluster, srec->job_id.proc,  
								REMOVED );
			}
			break;
		case JOB_EXITED:
			dprintf(D_FULLDEBUG, "Reaper: JOB_EXITED\n");
			if( q_status != HELD ) {
				set_job_status( srec->job_id.cluster,
								srec->job_id.proc, COMPLETED );
			}
			shutdownMpiJob( srec );
			break;
		case JOB_SHOULD_HOLD:
			dprintf( D_ALWAYS, "Putting job %d.%d on hold\n",
					 srec->job_id.cluster, srec->job_id.proc );
			set_job_status( srec->job_id.cluster, srec->job_id.proc, 
							HELD );
			shutdownMpiJob( srec );
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
				shutdownMpiJob( srec );
				scheduler.HadException( srec->match );
			}
			break;
		}
	} else if( WIFSIGNALED(status) ) {
		dprintf( D_ALWAYS, "Shadow pid %d died with %s\n",
				 pid, daemonCore->GetExceptionString(status) );

		dprintf( D_ALWAYS, "Shadow exited prematurely, "
				 "shutting down MPI computation\n" );

			// If the shadow died with a signal, it's in bad shape,
			// and probably didn't clean up anything.  So, try to
			// shutdown the computation so we don't leave jobs lying
			// around... 
		shutdownMpiJob( srec );
	}

		// Regardless of what happened, we need to remove the
		// allocation for this shadow.
	removeAllocation( srec );

		// Tell the Scheduler class this shadow pid is gone...
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
		dprintf( D_FULLDEBUG, "giveMatches: put %d num procs on wire\n",
				 alloc->num_procs ); 
	}		

	for( p=0; p<alloc->num_procs; p++ ) {
		job_ad = new ClassAd( *((*alloc->jobs)[p]) );
		matches = (*alloc->matches)[p];
		last = matches->getlast() + 1; 
		dprintf( D_FULLDEBUG, "In proc %d, num_matches: %d\n", p, last );
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

		// See if we have any idle MPI jobs to run.
	if( ! getDedicatedJobs() ) { 
		dprintf( D_FULLDEBUG, "No dedicated jobs found, "
				 "done with handleDedicatedJobs()\n" );
			// Don't treat this as an error...
		return TRUE;
	}

		// See what all the possible resources we *could* get are.  
	if( ! getDedicatedResourceInfo() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't get resources, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// Sort them, so we know if we can use them or not.
	sortResources();

		// Figure out what we want to do, based on what we have now.  
	if( ! computeSchedule() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't compute schedule, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// Try to spawn any jobs that we can currently service. 
	if( ! spawnJobs() ) { 
		dprintf( D_ALWAYS, "ERROR: Can't spawn jobs, "
				 "aborting handleDedicatedJobs\n" );
		return FALSE;
	}

		// If we have any resources we need to aquire, ask for them. 
	if( ! requestResources() ) {
		dprintf( D_ALWAYS, "ERROR: Can't request resources, "
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

	char constraint[256];

	if( resources ) {
		delete resources;
	}
	resources = new ClassAdList;

	
	sprintf( constraint, "DedicatedScheduler == \"%s\"", name() ); 
	query.addORConstraint( constraint );

		// This should fill in resources with all the classads we care
		// about
	query.fetchAds( *resources );

	dprintf( D_FULLDEBUG, "Found %d dedicated resources\n",
			 resources->Length() );
	listDedicatedResources( D_FULLDEBUG, resources );

	return true;
}


void
DedicatedScheduler::sortResources( void )
{
	ClassAd* res;
	match_rec* mrec;
	char buf[256];
	buf[0] = '\0';

		// We want to sort all the resources we have into the
		// avail_time_list
	if( avail_time_list ) {
		delete avail_time_list;
	}
	avail_time_list = new AvailTimeList;
	resources->Rewind();
	while( (res = resources->Next()) ) {
		if( ! res->LookupString(ATTR_NAME, buf) ) {
			dprintf( D_ALWAYS, "DedicatedScheduler::sortResources: ERROR: "
					 "%s not found in ClassAd, skipping resource",
					 ATTR_NAME ); 
				// Don't do anything w/ this resource, just go on. 
			continue;
		}
		HashKey key(buf);
		if( all_matches->lookup(key, mrec) < 0 ) {
				// We don't have a match_rec for this resource yet, so
				// request one
			dprintf( D_FULLDEBUG, "sortResources(): No match_rec for %s, "
					 "requesting\n", buf );
			generateRequest( res );
			continue;
		}

			// If we made it this far, this resource is already
			// claimed by us, so we can add it to our list of
			// available resources.  

			// First, we want to update my_match_ad which we're
			// storing in the match_rec, since that's now got
			// (probably) stale info, and we always want to use the
			// most recent.
		delete( mrec->my_match_ad );
		mrec->my_match_ad = new ClassAd( *res );

			// Now, we can actually add this resource to our list. 
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
		if( ! cur_matches ) {
			EXCEPT( "spawnJobs(): allocation node has no matches!" );
		}
		mrec = (*cur_matches)[0];
		if( ! mrec ) {
			EXCEPT( "spawnJobs(): allocation node has NULL first match!" );
		}
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
			// TODO: Deal w/ pushing matches (this is just a
			// performance optimization, not a correctness thing). 
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

				// Debugging hack: allow config file to specify which
				// ip address we want the master to run on.

			char* master_ip = param( "mpi_master_ip" );
			int m_ip_len = 0;
			if( master_ip ) {
				m_ip_len = strlen( master_ip );
				last = 1;
			}
			candidates->Rewind();
			while( (ad = candidates->Next()) ) {
				char name_buf[256];
				name_buf[0] = '\0';
				ad->LookupString(ATTR_NAME, name_buf);
				HashKey key(name_buf);
				if( all_matches->lookup(key, mrec) < 0 ) {
					EXCEPT( "no match for %s in all_matches table, yet " 
							"allocated to dedicated job %d.0!",
							name_buf, cluster ); 
				}
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
					// Also, mark that this mrec is allocated.
				mrec->allocated = true;

					// Put the right cluster + proc into the mrec,
					// since we might need to use it later...
				mrec->cluster = cluster;
				mrec->proc = 0;

					// Finally, take this out of our avail_time_list
					// so we don't use it for anything else in this
					// scheduling cycle.
				avail_time_list->removeResource( ad, cur );
			}
			if( (*new_matches)[0] == NULL ) {
					// What happened is that the user requested a
					// given host to be the master w/ the
					// "mpi_master_ip" config file setting, but that
					// host wasn't in the list of matches for this
					// job.  So, we need to fix up the array a little
					// bit and warn the user what happened. 

					// If master_ip isn't set and this happened,
					// something is seriously messed up...
				ASSERT( master_ip );

					// First, decrement last, since it was incremented
					// when we added the final match above.
				last--;

					// Move the last match over to be the first. 
				(*new_matches)[0] = (*new_matches)[last];
				(*new_matches)[last] = NULL;

					// Finally truncate the array to 1 less than last,
					// since that's now the last valid entry in the
					// array.  This way getlast() will return the
					// right thing down the road.
				new_matches->truncate(last-1);

				dprintf( D_ALWAYS, "WARNING: MPI_MASTER_IP set in config "
						 "file as %s, but that host isn't allocated to job "
						 "%d.%d.  Master will be: %s\n", master_ip,
						 cluster, proc, (*new_matches)[0]->peer );
			}
			(*alloc->matches)[0] = new_matches;
			alloc->num_resources = last;

				// Save this AllocationNode in our table
			allocations->insert( cluster, alloc );

				// Get rid of our master_ip string, so we don't leak
				// memory. 
			if( master_ip ) {
				free( master_ip );
			}

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


// This function is used to remove the allocation associated with the
// given shadow and perform any other clean-up related to the shadow
// exiting. 
void
DedicatedScheduler::removeAllocation( shadow_rec* srec )
{
	AllocationNode* alloc;
	MRecArray* matches;
	int i, n, m;

	dprintf( D_FULLDEBUG, "DedicatedScheduler::removeAllocation, "
			 "cluster %d\n", srec->job_id.cluster );

	if( ! srec ) {
		EXCEPT( "DedicatedScheduler::removeAllocation: srec is NULL!" );
	}
	if( allocations->lookup( srec->job_id.cluster, alloc ) < 0 ) {
		EXCEPT( "DedicatedScheduler::removeAllocation(): can't find " 
				"allocation node for cluster %d! Aborting", 
				srec->job_id.cluster ); 
	}

		// First, mark all the match records as no longer allocated to
		// our MPI job.
	for( i=0; i<alloc->num_procs; i++ ) {
		matches = (*alloc->matches)[i];
		n = matches->getlast();
		for( m=0 ; m <= n ; m++ ) {
			deallocMatchRec( (*matches)[m] );
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

		// This "allocation" is no longer valid.  So, delete the
		// allocation node from our table.
	allocations->remove( srec->job_id.cluster );

		// Finally, delete the object itself so we don't leak it. 
	delete alloc;
}


// This function is used to deactivate all the claims used by a
// given shadow.
void
DedicatedScheduler::shutdownMpiJob( shadow_rec* srec )
{
	AllocationNode* alloc;
	MRecArray* matches;
	int i, n, m;

	dprintf( D_FULLDEBUG, "DedicatedScheduler::shutdownMpiJob, cluster %d\n",
			 srec->job_id.cluster );

	if( ! srec ) {
		EXCEPT( "DedicatedScheduler::shutdownMpiJob: srec is NULL!" );
	}
	if( allocations->lookup( srec->job_id.cluster, alloc ) < 0 ) {
		EXCEPT( "DedicatedScheduler::shutdownMpiJob(): can't find " 
				"allocation node for cluster %d! Aborting", 
				srec->job_id.cluster ); 
	}
	alloc->status = A_DYING;
	for( i=0; i<alloc->num_procs; i++ ) {
		matches = (*alloc->matches)[i];
		n = matches->getlast();
		for( m=0 ; m <= n ; m++ ) {
			deactivateClaim( (*matches)[m] );
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
DedicatedScheduler::DelMrec( char* cap )
{
	match_rec* rec;

	char name_buf[256];
	name_buf[0] = '\0';

	if( ! cap ) {
		dprintf( D_ALWAYS, "Null parameter to DelMrec() -- "
				 "claim not deleted\n" );
		return false;
	}

		// First, delete it from our table hashed on capability. 
	HashKey key( cap );
	if( all_matches_by_cap->lookup(key, rec) < 0 ) {
		dprintf( D_ALWAYS, "mrec for \"%s\" not found -- "
				 "match not deleted\n", cap );
		return false;
	}
	all_matches_by_cap->remove(key);

		// Now that we have the mrec again, we have to see if this
		// match record is stored in our table of allocation nodes,
		// and if so, we need to remove it from there, so we don't
		// have dangling pointers, etc.  We can look it up w/ the
		// cluster from the mrec.
	AllocationNode* alloc;
	if( allocations->lookup(rec->cluster, alloc) < 0 ) {
			// Cool, this match wasn't allocated to anyone, so we
			// don't have to worry about it.  If the match isn't
			// allocated to anyone, the cluster better be -1.
		ASSERT( rec->cluster == -1 );
	} else {
			// Bummer, this match was allocated to one of our jobs.
			// We don't have to worry about shutting it down, since
			// the shadow takes care of that.  However, we need to
			// delete this match record from the matches in the
			// allocation node, or we're going to have a dangling
			// pointer in there.
			// TODO: Handle multiple procs!
		MRecArray* rec_array = (*alloc->matches)[0];
		int i, last = rec_array->getlast();
		bool found_it = false;
		for( i=0; i<= last; i++ ) {
				// In case you were wondering, this works just fine if
				// the mrec we care about is in the last position.
				// The first assignment will be a no-op, but no harm
				// is done, and the thing that matters is that we NULL
				// out the entry in the array and truncate it so that
				// getlast() will return the right value.
			if( (*rec_array)[i] == rec ) {
				found_it = true;
				(*rec_array)[i] = (*rec_array)[last];
				(*rec_array)[last] = NULL;
					// We want to decrement last so we break out of
					// this for loop before checking the element we
					// NULL'ed out.  Otherwise, the truncate below
					// will just get undone when we inspect the last
					// element. 
				last--;
					// Truncate our array so we realize the match is
					// gone, and don't consider it in the future.
				rec_array->truncate(last);
			}
		}		
		if( ! found_it ) {
				// This sucks.  We think this match record belongs to
				// a cluster that we have an allocation node for, but
				// we couldn't find the match record in the allocation
				// node.  This must be a programmer error...
			dprintf( D_ALWAYS, "ERROR deleting match record for cluster %d\n",
					 rec->cluster );  
			dprintf( D_ALWAYS, "Allocation node for this cluster doesn't "
					 "include the match rec\n" );
			EXCEPT( "Can't delete m_rec from allocation node for cluster %d",
					rec->cluster );
		}
	}

		// Now, we can delete it from the main table hashed on name.
	rec->my_match_ad->LookupString( ATTR_NAME, name_buf );
	HashKey key2( name_buf );
	all_matches->remove(key2);

		// If this match record is associated with a shadow record,
		// clear out the match record from that shadow record to avoid
		// a dangling pointer.
	if( rec->shadowRec ) {
		rec->shadowRec->match = NULL;
	}

		// Finally, delete the match rec itself.
	delete rec;

	dprintf( D_FULLDEBUG, "Deleted match rec for %s\n", name_buf );

	return true;
}


bool
DedicatedScheduler::removeShadowRecFromMrec( shadow_rec* shadow )
{
	bool		found = false;
	match_rec	*mrec;

	all_matches->startIterations();
    while( all_matches->iterate( mrec ) ) {
		if( mrec->shadowRec == shadow ) {
			deallocMatchRec( mrec );
			found = true;
		}
	}
	return found;
}


// TODO: Deal w/ flocking!
void
DedicatedScheduler::publishRequestAd( void )
{
	ClassAd ad;
	char tmp[1024];

	dprintf( D_FULLDEBUG, "In DedicatedScheduler::publishRequestAd()\n" );

	ad.SetMyTypeName(SCHEDD_ADTYPE);
	ad.SetTargetTypeName(STARTD_ADTYPE);

	config_fill_ad( &ad );

	sprintf( tmp, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() );  
	ad.InsertOrUpdate( tmp );

	sprintf( tmp, "%s = \"%s\"", ATTR_SCHEDD_IP_ADDR, 
			 scheduler.dcSockSinful() );
	ad.InsertOrUpdate( tmp );

		// Tell negotiator to send us the startd ad
	sprintf( tmp, "%s = True", ATTR_WANT_RESOURCE_AD );
	ad.InsertOrUpdate( tmp );

	sprintf( tmp, "%s = \"%s\"", ATTR_SCHEDD_NAME, Name );
	ad.InsertOrUpdate( tmp );

	sprintf( tmp, "%s = \"%s\"", ATTR_NAME, name() );
	ad.InsertOrUpdate( tmp );

		// Finally, we need to tell it how many "jobs" we want to
		// negotiate.  These are really how many resource requests
		// we've got. 
	sprintf( tmp, "%s = %d", ATTR_IDLE_JOBS,
			 resource_requests->getNumElements() ); 
	dprintf( D_FULLDEBUG, "%s\n", tmp );
	ad.InsertOrUpdate( tmp );
	
		// TODO: Eventually, we could try to publish this info as
		// well, so condor_status -sub and friends could tell people
		// something useful about the DedicatedScheduler...
	sprintf( tmp, "%s = 0", ATTR_RUNNING_JOBS );
	ad.InsertOrUpdate( tmp );
	
	sprintf( tmp, "%s = 0", ATTR_HELD_JOBS );
	ad.InsertOrUpdate( tmp );

	sprintf( tmp, "%s = 0", ATTR_FLOCKED_JOBS );
	ad.InsertOrUpdate( tmp );

		// Now, we can actually send this off to the CM:
		// Port doesn't matter, since we've got the sinful string. 
	scheduler.updateCentralMgr( UPDATE_SUBMITTOR_AD, &ad,
								scheduler.Collector->addr(), 0 );  
}


/*
  TODO: do something smarter w/ requirements than arch, opsys and
  memory? 
*/
void
DedicatedScheduler::generateRequest( ClassAd* machine_ad )
{
	char tmp[1024];
	char namebuf[1024];
	char arch[128];
	char opsys[128];
	int	memory;

		// First, grab some things out of the machine ad so we can
		// figure out what requirements + rank should be.  
	
	namebuf[0] = '\0';
	arch[0] = '\0';
	opsys[0] = '\0';
	if( ! machine_ad->LookupString(ATTR_NAME, namebuf) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::generateRequest():"
				 " %s not defined in machine ClassAd\n", ATTR_NAME );
		return;
	}

		// before we get any further, make sure we don't already have
		// a request for this name.
	HashKey key(namebuf);
	ClassAd* req = NULL;

	if( resource_requests->lookup(key, req) == 0 ) {
			// Found it.  Bail out now
		dprintf( D_FULLDEBUG, "DedicatedScheduler::generateRequest: "
				 "Already have a resource request for %s, aborting\n",
				 namebuf ); 
			// displayResourceRequests();
		return;
	}

	if( ! machine_ad->LookupString(ATTR_ARCH, arch) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::generateRequest():"
				 " %s not defined in machine ClassAd\n", ATTR_ARCH );
		return;
	}
	if( ! machine_ad->LookupString(ATTR_OPSYS, opsys) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::generateRequest():"
				 " %s not defined in machine ClassAd\n", ATTR_OPSYS );
		return;
	}
	if( ! machine_ad->LookupInteger(ATTR_MEMORY, memory) ) {
		dprintf( D_ALWAYS, "ERROR in DedicatedScheduler::generateRequest():"
				 " %s not defined in machine ClassAd\n", ATTR_MEMORY );
		return;
	}

		// Now, we're in good shape.

		// First, generate the request ad itself.
	req = new ClassAd;
	req->SetMyTypeName(JOB_ADTYPE);
	req->SetTargetTypeName(STARTD_ADTYPE);

		// We'll require that we get a machine of the same arch, opsys
		// and at least as much memory...
	sprintf( tmp, "%s = (%s == \"%s\") && (%s == \"%s\") && (%s >= %d)", 
			 ATTR_REQUIREMENTS, ATTR_ARCH, arch, ATTR_OPSYS, opsys, 
			 ATTR_MEMORY, memory );
	req->InsertOrUpdate( tmp );

		// But we'll prefer to get the exact machine we were given.
	sprintf( tmp, "%s = (%s == \"%s\")", ATTR_RANK, ATTR_NAME, namebuf );
	req->InsertOrUpdate( tmp );
	
		// Now, fill in a bunch of stuff about ourself to identify us,
		// and make it seem like this is a real job...
	sprintf( tmp, "%s = \"%s\"", ATTR_USER, name() );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = \"%s\"", ATTR_OWNER, owner() );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = 0", ATTR_IMAGE_SIZE );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = %d", ATTR_JOB_UNIVERSE, MPI );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = FALSE", ATTR_NICE_USER );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = 1", ATTR_MIN_HOSTS );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = 1", ATTR_MAX_HOSTS );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = 0", ATTR_CURRENT_HOSTS );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = 0", ATTR_CLUSTER_ID );
	req->InsertOrUpdate( tmp );

	sprintf( tmp, "%s = 0", ATTR_PROC_ID );
	req->InsertOrUpdate( tmp );

		// Finally, add this request to our array.
	resource_requests->insert( key, req );
	
	dprintf( D_FULLDEBUG, "Added resource request\n" );
	displayRequest( req, "GenerateRequest: ", D_FULLDEBUG );
}


bool
DedicatedScheduler::requestResources( void )
{
	if( resource_requests ) {
			// If we've got things we want to grab, publish a ClassAd
			// to ask to negotiate for them...
		displayResourceRequests();
		publishRequestAd();	
	}
	return true;
}


void
DedicatedScheduler::displayResourceRequests( void )
{
	int level = D_FULLDEBUG;
	ClassAd* req;

	if( ! resource_requests ) {
		dprintf( level, "displayResourceRequests: "
				 "No resource_request array\n" );
		return;
	}

	dprintf( level, "Starting displayResourceRequests()\n" );

	resource_requests->startIterations();
	while( resource_requests->iterate(req) ) {
		if( ! req ) {
			continue;
		}
		displayRequest( req, "", D_FULLDEBUG );
	}
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

		// First, see if we've already allocated this node.  We want
		// to check this first, not the resource state in the classad,
		// since we might have stale classad info, but we always have
		// accurate info in the match record about allocations.
	if( mrec->allocated ) {
			// TODO: Once we're smarter about the future, we need
			// to be smarter about what to do in this case, too.
		return now + 300;
	}

		// First, switch on the status of the mrec
	switch( mrec->status ) {
	case M_UNCLAIMED:
	case M_STARTD_CONTACT_LIMBO:
			// Not yet claimed, so not yet available. 
			// TODO: Be smarter here.
		return now + 500;
	case M_CLAIMED:
    case M_ACTIVE:
			// Actually claimed by us, break out and let the state
		break;
	default:
		EXCEPT( "Unknown status in match rec (%d)", mrec->status );
	}

	resource->LookupString( ATTR_STATE, state );
	s = string_to_state( state );
	switch( s ) {

	case unclaimed_state:
	case owner_state:
	case matched_state:
			// Shouldn't really be here, since we're checking the mrec
			// status above.  However, we might have stale classad
			// info, so don't freak out.  
		return now + 30;
		break;

	case claimed_state:
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


void
displayRequest( ClassAd* ad, char* str, int debug_level )
{
	ExprTree* expr;
	expr = ad->Lookup( ATTR_RANK );
	char rank[1024];
	rank[0] = '\0';
	expr->PrintToStr( rank );
	dprintf( debug_level, "%s%s\n", str, rank );
}


char*
getCapability( ClassAd* resource )
{
	char cap_buf[256];
	cap_buf[0] = '\0';

	if( ! resource->LookupString(ATTR_CAPABILITY, cap_buf) ) {
			// No capability in ad...
		return NULL;
	}
	return( strdup(cap_buf) );
}


void
deallocMatchRec( match_rec* mrec )
{
		// We might call this with a NULL mrec, so don't seg fault.
	if( ! mrec ) {
		return;
	}
	mrec->allocated = false;
	mrec->cluster = -1;
	mrec->proc = -1;
	mrec->shadowRec = NULL;
	mrec->num_exceptions = 0;
		// Status is no longer active, but we're still claimed
	mrec->status = M_CLAIMED;
}
