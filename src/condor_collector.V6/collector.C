/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "condor_parser.h"
#include "condor_status.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "internet.h"
#include "fdprintf.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_parameters.h"
#include "condor_email.h"
#include "condor_query.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "../condor_status.V6/status_types.h"
#include "../condor_status.V6/totals.h"

#include "condor_collector.h"
#include "collector_engine.h"
#include "HashTable.h"
#include "hashkey.h"

#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_universe.h"
#include "my_hostname.h"

#include "collector.h"

//----------------------------------------------------------------

extern char* mySubSystem;
extern "C" char* CondorVersion( void );
extern "C" char* CondorPlatform( void );

CollectorStats CollectorDaemon::collectorStats( false, 0, 0 );
CollectorEngine CollectorDaemon::collector( &collectorStats );
int CollectorDaemon::ClientTimeout;
int CollectorDaemon::QueryTimeout;
char* CollectorDaemon::CollectorName;
Daemon* CollectorDaemon::View_Collector;
Sock* CollectorDaemon::view_sock;
SocketCache* CollectorDaemon::sock_cache;

ClassAd* CollectorDaemon::__query__;
int CollectorDaemon::__numAds__;
int CollectorDaemon::__failed__;
List<ClassAd>* CollectorDaemon::__ClassAdResultList__;

TrackTotals* CollectorDaemon::normalTotals;
int CollectorDaemon::submittorRunningJobs;
int CollectorDaemon::submittorIdleJobs;

CollectorUniverseStats CollectorDaemon::ustatsAccum;
CollectorUniverseStats CollectorDaemon::ustatsMonthly;

int CollectorDaemon::machinesTotal;
int CollectorDaemon::machinesUnclaimed;
int CollectorDaemon::machinesClaimed;
int CollectorDaemon::machinesOwner;

ForkWork CollectorDaemon::forkQuery;

ClassAd* CollectorDaemon::ad;
DCCollector* CollectorDaemon::updateCollector;
int CollectorDaemon::UpdateTimerId;

ClassAd *CollectorDaemon::query_any_result;
ClassAd CollectorDaemon::query_any_request;

//---------------------------------------------------------

// prototypes of library functions
typedef void (*SIGNAL_HANDLER)();
extern "C"
{
	void install_sig_handler( int, SIGNAL_HANDLER );
	void schedule_event ( int month, int day, int hour, int minute, int second, SIGNAL_HANDLER );
}

//----------------------------------------------------------------

void CollectorDaemon::Init()
{
	dprintf(D_ALWAYS, "In CollectorDaemon::Init()\n");

	// read in various parameters from condor_config
	CollectorName=NULL;
	ad=NULL;
	View_Collector=NULL;
	view_sock=NULL;
	UpdateTimerId=-1;
	sock_cache = NULL;
	updateCollector = NULL;
	Config();

    // setup routine to report to condor developers
    // schedule reports to developers
	schedule_event( -1, 1,  0, 0, 0, reportToDevelopers );
	schedule_event( -1, 8,  0, 0, 0, reportToDevelopers );
	schedule_event( -1, 15, 0, 0, 0, reportToDevelopers );
	schedule_event( -1, 23, 0, 0, 0, reportToDevelopers );

	// install command handlers for queries
	daemonCore->Register_Command(QUERY_STARTD_ADS,"QUERY_STARTD_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_STARTD_PVT_ADS,"QUERY_STARTD_PVT_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,NEGOTIATOR);
#if WANT_QUILL
	daemonCore->Register_Command(QUERY_QUILL_ADS,"QUERY_QUILL_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
#endif /* WANT_QUILL */

	daemonCore->Register_Command(QUERY_SCHEDD_ADS,"QUERY_SCHEDD_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_MASTER_ADS,"QUERY_MASTER_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_CKPT_SRVR_ADS,"QUERY_CKPT_SRVR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_SUBMITTOR_ADS,"QUERY_SUBMITTOR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_LICENSE_ADS,"QUERY_LICENSE_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_COLLECTOR_ADS,"QUERY_COLLECTOR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,ADMINISTRATOR);
	daemonCore->Register_Command(QUERY_STORAGE_ADS,"QUERY_STORAGE_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_NEGOTIATOR_ADS,"QUERY_NEGOTIATOR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_HAD_ADS,"QUERY_HAD_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_Command(QUERY_ANY_ADS,"QUERY_ANY_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	
		// // // // // // // // // // // // // // // // // // // // //
		// WARNING!!!! If you add other invalidate commands here, you
		// also need to add them to the switch statement in the
		// sockCacheHandler() method!!!
		// // // // // // // // // // // // // // // // // // // // //

	// install command handlers for invalidations
	daemonCore->Register_Command(INVALIDATE_STARTD_ADS,"INVALIDATE_STARTD_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,DAEMON);

#if WANT_QUILL
	daemonCore->Register_Command(INVALIDATE_QUILL_ADS,"INVALIDATE_QUILL_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,DAEMON);
#endif /* WANT_QUILL */

	daemonCore->Register_Command(INVALIDATE_SCHEDD_ADS,"INVALIDATE_SCHEDD_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_MASTER_ADS,"INVALIDATE_MASTER_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_CKPT_SRVR_ADS,
		"INVALIDATE_CKPT_SRVR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_SUBMITTOR_ADS,
		"INVALIDATE_SUBMITTOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_LICENSE_ADS,
		"INVALIDATE_LICENSE_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_COLLECTOR_ADS,
		"INVALIDATE_COLLECTOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_STORAGE_ADS,
		"INVALIDATE_STORAGE_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_NEGOTIATOR_ADS,
		"INVALIDATE_NEGOTIATOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,NEGOTIATOR);
	daemonCore->Register_Command(INVALIDATE_HAD_ADS,
		"INVALIDATE_HAD_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_Command(INVALIDATE_ADS_GENERIC,
		"INVALIDATE_ADS_GENERIC", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);

		// // // // // // // // // // // // // // // // // // // // //
		// WARNING!!!! If you add other update commands here, you
		// also need to add them to the switch statement in the
		// sockCacheHandler() method!!!
		// // // // // // // // // // // // // // // // // // // // //

	// install command handlers for updates

#if WANT_QUILL
	daemonCore->Register_Command(UPDATE_QUILL_AD,"UPDATE_QUILL_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
#endif /* WANT_QUILL */

	daemonCore->Register_Command(UPDATE_STARTD_AD,"UPDATE_STARTD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_SCHEDD_AD,"UPDATE_SCHEDD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_SUBMITTOR_AD,"UPDATE_SUBMITTOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_LICENSE_AD,"UPDATE_LICENSE_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_MASTER_AD,"UPDATE_MASTER_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_CKPT_SRVR_AD,"UPDATE_CKPT_SRVR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_COLLECTOR_AD,"UPDATE_COLLECTOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,ALLOW);
	daemonCore->Register_Command(UPDATE_STORAGE_AD,"UPDATE_STORAGE_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_NEGOTIATOR_AD,"UPDATE_NEGOTIATOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,NEGOTIATOR);
	daemonCore->Register_Command(UPDATE_HAD_AD,"UPDATE_HAD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_Command(UPDATE_AD_GENERIC, "UPDATE_AD_GENERIC",
				     (CommandHandler)receive_update,
				     "receive_update", NULL, DAEMON);
				     

	// ClassAd evaluations use this function to resolve names
	// ClassAdLookupRegister( process_global_query, this );

	forkQuery.Initialize( );
}

int CollectorDaemon::receive_query_cedar(Service* s,
										 int command,
										 Stream* sock)
{
	ClassAd ad;

	sock->decode();
	sock->timeout(ClientTimeout);
    if( !ad.initFromStream(*sock) || !sock->eom() )
    {
        dprintf(D_ALWAYS,"Failed to receive query on TCP: aborting\n");
        return FALSE;
    }

		// here we set up a network timeout of a longer duration
	sock->timeout(QueryTimeout);

	// Initial query handler
	AdTypes whichAds = receive_query_public( command );

	// Perform the query
	List<ClassAd> results;
	ForkStatus	fork_status = FORK_FAILED;
	int	   		return_status = 0;
    if (whichAds != (AdTypes) -1) {
		fork_status = forkQuery.NewJob( );
		if ( FORK_PARENT == fork_status ) {
			return 1;
		} else {
			// Child / Fork failed / busy
			ad.fPrint(stderr);
			process_query_public (whichAds, &ad, &results);
		}
	}

	// send the results via cedar
	sock->encode();
	results.Rewind();
	ClassAd *curr_ad = NULL;
	int more = 1;
	
	while ( (curr_ad=results.Next()) ) 
    {
        if (!sock->code(more) || !curr_ad->put(*sock))
        {
            dprintf (D_ALWAYS, 
                    "Error sending query result to client -- aborting\n");
            return_status = 0;
			goto END;
        }
    }

	// end of query response ...
	more = 0;
	if (!sock->code(more))
	{
		dprintf (D_ALWAYS, "Error sending EndOfResponse (0) to client\n");
	}

	// flush the output
	if (!sock->end_of_message())
	{
		dprintf (D_ALWAYS, "Error flushing CEDAR socket\n");
	}
	return_status = 1;


    // all done; let daemon core will clean up connection
  END:
	if ( FORK_CHILD == fork_status ) {
		forkQuery.WorkerDone( );		// Never returns
	}
	return return_status;
}

AdTypes
CollectorDaemon::receive_query_public( int command )
{
	AdTypes whichAds;

    switch (command)
    {
	  case QUERY_STARTD_ADS:
		dprintf (D_ALWAYS, "Got QUERY_STARTD_ADS\n");
		whichAds = STARTD_AD;
		break;
		
	  case QUERY_SCHEDD_ADS:
		dprintf (D_ALWAYS, "Got QUERY_SCHEDD_ADS\n");
		whichAds = SCHEDD_AD;
		break;

#if WANT_QUILL
	  case QUERY_QUILL_ADS:
		dprintf (D_ALWAYS, "Got QUERY_QUILL_ADS\n");
		whichAds = QUILL_AD;
		break;
#endif /* WANT_QUILL */
		
	  case QUERY_SUBMITTOR_ADS:
		dprintf (D_ALWAYS, "Got QUERY_SUBMITTOR_ADS\n");
		whichAds = SUBMITTOR_AD;
		break;

	  case QUERY_LICENSE_ADS:
		dprintf (D_ALWAYS, "Got QUERY_LICENSE_ADS\n");
		whichAds = LICENSE_AD;
		break;

	  case QUERY_MASTER_ADS:
		dprintf (D_ALWAYS, "Got QUERY_MASTER_ADS\n");
		whichAds = MASTER_AD;
		break;
		
	  case QUERY_CKPT_SRVR_ADS:
		dprintf (D_ALWAYS, "Got QUERY_CKPT_SRVR_ADS\n");
		whichAds = CKPT_SRVR_AD;	
		break;
		
	  case QUERY_STARTD_PVT_ADS:
		dprintf (D_ALWAYS, "Got QUERY_STARTD_PVT_ADS\n");
		whichAds = STARTD_PVT_AD;
		break;

	  case QUERY_COLLECTOR_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_COLLECTOR_ADS\n");
		whichAds = COLLECTOR_AD;
		break;

	  case QUERY_STORAGE_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_STORAGE_ADS\n");
		whichAds = STORAGE_AD;
		break;

	  case QUERY_NEGOTIATOR_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_NEGOTIATOR_ADS\n");
		whichAds = NEGOTIATOR_AD;
		break;

	  case QUERY_HAD_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_HAD_ADS\n");
		whichAds = HAD_AD;
		break;

	  case QUERY_ANY_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_ANY_ADS\n");
		whichAds = ANY_AD;
		break;

	  default:
		dprintf(D_ALWAYS,
				"Unknown command %d in receive_query_public()\n",
				command);
		whichAds = (AdTypes) -1;
    }

	return whichAds;
}

int CollectorDaemon::receive_invalidation(Service* s, int command, Stream* sock)
{
    struct sockaddr_in *from;
	AdTypes whichAds;
	ClassAd ad;

	from = ((Sock*)sock)->endpoint();

	sock->decode();
	sock->timeout(ClientTimeout);
    if( !ad.initFromStream(*sock) || !sock->eom() )
    {
        dprintf( D_ALWAYS, 
				 "Failed to receive invalidation on %s: aborting\n",
				 sock->type() == Stream::reli_sock ? "TCP" : "UDP" );
        return FALSE;
    }

    // cancel timeout --- collector engine sets up its own timeout for
    // collecting further information
    sock->timeout(0);

    switch (command)
    {
	  case INVALIDATE_STARTD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_STARTD_ADS\n");
		whichAds = STARTD_AD;
		break;
		
	  case INVALIDATE_SCHEDD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_SCHEDD_ADS\n");
		whichAds = SCHEDD_AD;
		break;

#if WANT_QUILL
	  case INVALIDATE_QUILL_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_QUILL_ADS\n");
		whichAds = QUILL_AD;
		break;
#endif /* WANT_QUILL */
		
	  case INVALIDATE_SUBMITTOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_SUBMITTOR_ADS\n");
		whichAds = SUBMITTOR_AD;
		break;

	  case INVALIDATE_LICENSE_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_LICENSE_ADS\n");
		whichAds = LICENSE_AD;
		break;

	  case INVALIDATE_MASTER_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_MASTER_ADS\n");
		whichAds = MASTER_AD;
		break;
		
	  case INVALIDATE_CKPT_SRVR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_CKPT_SRVR_ADS\n");
		whichAds = CKPT_SRVR_AD;	
		break;
		
	  case INVALIDATE_COLLECTOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_COLLECTOR_ADS\n");
		whichAds = COLLECTOR_AD;
		break;

	  case INVALIDATE_NEGOTIATOR_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_NEGOTIATOR_ADS\n");
		whichAds = NEGOTIATOR_AD;
		break;

	  case INVALIDATE_HAD_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_HAD_ADS\n");
		whichAds = HAD_AD;
		break;

	  case INVALIDATE_STORAGE_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_STORAGE_ADS\n");
		whichAds = STORAGE_AD;
		break;

          case INVALIDATE_ADS_GENERIC:
		dprintf(D_ALWAYS, "Got INVALIDATE_ADS_GENERIC\n");
		whichAds = GENERIC_AD;
		break;

	  default:
		dprintf(D_ALWAYS,"Unknown command %d in receive_invalidation()\n", 
			command);
		whichAds = (AdTypes) -1;
    }
   
    if (whichAds != (AdTypes) -1)
		process_invalidation (whichAds, ad, sock);

	// if the invalidation was for the STARTD ads, also invalidate startd
	// private ads with the same query ad
	if (command == INVALIDATE_STARTD_ADS)
		process_invalidation (STARTD_PVT_AD, ad, sock);

	if(View_Collector && ((command == INVALIDATE_STARTD_ADS) || 
		(command == INVALIDATE_SUBMITTOR_ADS)) ) {
		send_classad_to_sock(command, View_Collector, &ad);
	}	

	if( sock_cache && sock->type() == Stream::reli_sock ) {
			// if this is a TCP update and we've got a cache, stash
			// this socket for future updates...
		return stashSocket( sock );
	}

    // all done; let daemon core will clean up connection
	return TRUE;
}


int CollectorDaemon::receive_update(Service *s, int command, Stream* sock)
{
    int	insert;
	sockaddr_in *from;
	ClassAd *ad;

	/* assume the ad is malformed... other functions set this value */
	insert = -3;

  		// unless the collector has been configured to use a socket
  		// cache for TCP updates, refuse any update commands that come
  		// in via TCP... 
	if( ! sock_cache && sock->type() == Stream::reli_sock ) {
		// update via tcp; sorry buddy, use udp or you're outa here!
		dprintf(D_ALWAYS,"Received UPDATE command via TCP; ignored\n");
		// let daemon core clean up the socket
		return TRUE;
	}

	// get endpoint
	from = ((Sock*)sock)->endpoint();

    // process the given command
	if (!(ad = collector.collect (command,(Sock*)sock,from,insert)))
	{
		if (insert == -2)
		{
			// this should never happen assuming we never register QUERY
			// commands with daemon core, but it cannot hurt to check...
			dprintf (D_ALWAYS,"Got QUERY command (%d); not supported for UDP\n",
						command);
		}

		if (insert == -3)
		{
			/* this happens when we get a classad for which a hash key could
				not been made. This occurs when certain attributes are needed
				for the particular catagory the ad is destined for, but they
				are not present in the ad. */
			dprintf (D_ALWAYS,
				"Received malformed ad from command (%d). Ignoring.\n",
				command);
		}
	}

	if(View_Collector && ((command == UPDATE_STARTD_AD) || 
			(command == UPDATE_SUBMITTOR_AD)) ) {
		send_classad_to_sock(command, View_Collector, ad);
	}	

	if( sock_cache && sock->type() == Stream::reli_sock ) {
			// if this is a TCP update and we've got a cache, stash
			// this socket for future updates...
		return stashSocket( sock );
	}

	// let daemon core clean up the socket
	return TRUE;
}


int
CollectorDaemon::stashSocket( Stream* sock )
{
		
	ReliSock* rsock;
	char* addr = sin_to_string( ((Sock*)sock)->endpoint() );
	rsock = sock_cache->findReliSock( addr );
	if( ! rsock ) {
			// don't have it in the socket already, see if the cache
			// is full.  if not, add this socket to the cache so we
			// can reuse it for future updates.  if we're full, we're
			// going to have to screw this connection and not cache
			// it, to allow the cache to be useful for the other
			// daemons.
		if( sock_cache->isFull() ) {
			dprintf( D_ALWAYS, "WARNING: socket cache (size: %d) "
					 "is full - NOT caching TCP updates from %s\n", 
					 sock_cache->size(), addr );
			return TRUE;
		} 
		sock_cache->addReliSock( addr, (ReliSock*)sock );

			// now that it's in our socket, we want to register this
			// socket w/ DaemonCore so we wake up if there's more data
			// to read...
		daemonCore->Register_Socket( sock, "TCP Cached Socket", 
									 (SocketHandler)sockCacheHandler,
									 "sockCacheHandler", NULL, DAEMON );
	}

		// if we're here, it means the sock is in the cache (either
		// because it was there already, or because we just added it).
		// either, way, we don't want daemonCore to mess with the
		// socket...
	return KEEP_STREAM;
}


int
CollectorDaemon::sockCacheHandler( Service*, Stream* sock )
{
	int cmd;
	char* addr = sin_to_string( ((Sock*)sock)->endpoint() );
	sock->decode();
	dprintf( D_FULLDEBUG, "Activity on stashed TCP socket from %s\n", 
			 addr ); 

	if( ! sock->code(cmd) ) {
			// can't read an int, the other side probably closed the
			// socket, which is why select() woke up.
		dprintf( D_FULLDEBUG,
				 "Socket has been closed, removing from cache\n" );
		daemonCore->Cancel_Socket( sock );
		sock_cache->invalidateSock( addr );
		return KEEP_STREAM;
	}

	switch( cmd ) {
#if WANT_QUILL
	case UPDATE_QUILL_AD:
#endif /* WANT_QUILL */
	case UPDATE_STARTD_AD:
	case UPDATE_SCHEDD_AD:
	case UPDATE_MASTER_AD:
	case UPDATE_GATEWAY_AD:
	case UPDATE_CKPT_SRVR_AD:
	case UPDATE_SUBMITTOR_AD:
	case UPDATE_COLLECTOR_AD:
	case UPDATE_NEGOTIATOR_AD:
	case UPDATE_HAD_AD:
	case UPDATE_LICENSE_AD:
	case UPDATE_STORAGE_AD:
	case UPDATE_AD_GENERIC:
		return receive_update( NULL, cmd, sock );
		break;

#if WANT_QUILL
	case INVALIDATE_QUILL_ADS:
#endif /* WANT_QUILL */
	case INVALIDATE_STARTD_ADS:
	case INVALIDATE_SCHEDD_ADS:
	case INVALIDATE_MASTER_ADS:
	case INVALIDATE_GATEWAY_ADS:
	case INVALIDATE_CKPT_SRVR_ADS:
	case INVALIDATE_SUBMITTOR_ADS:
	case INVALIDATE_COLLECTOR_ADS:
	case INVALIDATE_NEGOTIATOR_ADS:
	case INVALIDATE_HAD_ADS:
	case INVALIDATE_LICENSE_ADS:
	case INVALIDATE_STORAGE_ADS:
		return receive_invalidation( NULL, cmd, sock );
		break;

	default:
		dprintf( D_ALWAYS,
				 "ERROR: invalid command %d on stashed TCP socket\n", cmd );
		daemonCore->Cancel_Socket( sock );
		sock_cache->invalidateSock( addr );
		return KEEP_STREAM;
		break;
    }
	EXCEPT( "Should never reach here" );
	return FALSE;
}


int CollectorDaemon::query_scanFunc (ClassAd *ad)
{
    int more = 1;

	if (ad < CollectorEngine::THRESHOLD) return 1;

    if ((*ad) >= (*__query__))
    {
		// Found a match --- append to our results list
		__ClassAdResultList__->Append(ad);
        __numAds__++;
    }

    return 1;
}

/*
Examine the given ad, and see if it satisfies the query.
If so, return zero, causing the scan to stop.
Otherwise, return 1.
*/

int CollectorDaemon::select_by_match( ClassAd *ad )
{
	if(ad<CollectorEngine::THRESHOLD) {
		return 1;
	}

	if( query_any_request <= *ad ) {
		query_any_result = ad;
		return 0;
	}
	return 1;
}

/*
This function is called by the global reference mechanism.
It convert the constraint string into a query ad, and runs
a global query, returning a duplicate of the ad matched.
On failure, it returns 0.
*/

#if 0
ClassAd * CollectorDaemon::process_global_query( const char *constraint, void *arg )
{
	CondorQuery query(ANY_AD);

       	query.addANDConstraint(constraint);
	query.getQueryAd(query_any_request);
	query_any_request.SetTargetTypeName (ANY_ADTYPE);
	query_any_result = 0;

	if(!collector.walkHashTable(ANY_AD,select_by_match)) {
		return new ClassAd(*query_any_result);
	} else {
		return 0;
	}
}
#endif

void CollectorDaemon::process_query_public (AdTypes whichAds,
											ClassAd *query,
											List<ClassAd>* results)
{
	// set up for hashtable scan
	__query__ = query;
	__numAds__ = 0;
	__ClassAdResultList__ = results;

	if (!collector.walkHashTable (whichAds, query_scanFunc))
	{
		dprintf (D_ALWAYS, "Error sending query response\n");
	}


	dprintf (D_ALWAYS, "(Sending %d ads in response to query)\n", __numAds__);
}	

int CollectorDaemon::invalidation_scanFunc (ClassAd *ad)
{
	static char buffer[64];
	
	sprintf( buffer, "%s = -1", ATTR_LAST_HEARD_FROM );

	if (ad < CollectorEngine::THRESHOLD) return 1;

    if ((*ad) >= (*__query__))
    {
		ad->Insert( buffer );			
        __numAds__++;
    }

    return 1;
}

void CollectorDaemon::process_invalidation (AdTypes whichAds, ClassAd &query, Stream *sock)
{
	// here we set up a network timeout of a longer duration
	sock->timeout(QueryTimeout);

	// set up for hashtable scan
	__query__ = &query;
	__numAds__ = 0;

	sock->encode();
	if (!sock->put(0) || !sock->end_of_message()) {
		dprintf( D_ALWAYS, "Unable to acknowledge invalidation\n" );
		return;
	}

	// first set all the "LastHeardFrom" attributes to low values ...
	AdTypes queryAds = (whichAds == GENERIC_AD) ? ANY_AD : whichAds;
	collector.walkHashTable (queryAds, invalidation_scanFunc);

	// ... then invoke the housekeeper
	collector.invokeHousekeeper (whichAds);

	dprintf (D_ALWAYS, "(Invalidated %d ads)\n", __numAds__);
}	



int CollectorDaemon::reportStartdScanFunc( ClassAd *ad )
{
	return normalTotals->update( ad );
}

int CollectorDaemon::reportSubmittorScanFunc( ClassAd *ad )
{
	int tmp1, tmp2;
	if( !ad->LookupInteger( ATTR_RUNNING_JOBS , tmp1 ) ||
		!ad->LookupInteger( ATTR_IDLE_JOBS, tmp2 ) )
			return 0;
	submittorRunningJobs += tmp1;
	submittorIdleJobs	 += tmp2;

	return 1;
}

int CollectorDaemon::reportMiniStartdScanFunc( ClassAd *ad )
{
    char buf[80];

    if ( !ad->LookupString( ATTR_STATE, buf ) )
        return 0;
    machinesTotal++;
    switch ( buf[0] ) {
        case 'C':
            machinesClaimed++;
            break;
        case 'U':
            machinesUnclaimed++;
            break;
        case 'O':
            machinesOwner++;
            break;
    }

	// Count the number of jobs in each universe
	int		universe;
	if ( ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) ) {
		ustatsAccum.accumulate( universe );
	}

	// Done
    return 1;
}

void CollectorDaemon::reportToDevelopers (void)
{
	char	buffer[128];
	FILE	*mailer;
	TrackTotals	totals( PP_STARTD_NORMAL );

    // compute machine information
    machinesTotal = 0;
    machinesUnclaimed = 0;
    machinesClaimed = 0;
    machinesOwner = 0;
	ustatsAccum.Reset( );

    if (!collector.walkHashTable (STARTD_AD, reportMiniStartdScanFunc)) {
            dprintf (D_ALWAYS, "Error counting machines in devel report \n");
    }

    // If we don't have any machines reporting to us, bail out early
    if(machinesTotal == 0) 	
        return; 

	if( ( normalTotals = new TrackTotals( PP_STARTD_NORMAL ) ) == NULL ) {
		dprintf( D_ALWAYS, "Didn't send monthly report (failed totals)\n" );
		return;
	}

	// Accumulate our monthly maxes
	ustatsMonthly.setMax( ustatsAccum );

	sprintf( buffer, "Collector (%s):  Monthly report", 
			 my_full_hostname() );
	if( ( mailer = email_developers_open(buffer) ) == NULL ) {
		dprintf (D_ALWAYS, "Didn't send monthly report (couldn't open mailer)\n");		
		return;
	}

	fprintf( mailer , "This Collector has the following IDs:\n");
	fprintf( mailer , "    %s\n", CondorVersion() );
	fprintf( mailer , "    %s\n\n", CondorPlatform() );

	normalTotals = &totals;
	if (!collector.walkHashTable (STARTD_AD, reportStartdScanFunc)) {
		dprintf (D_ALWAYS, "Error making monthly report (startd scan) \n");
	}
		
	// output totals summary to the mailer
	totals.displayTotals( mailer, 20 );

	// now output information about submitted jobs
	submittorRunningJobs = 0;
	submittorIdleJobs = 0;
	if( !collector.walkHashTable( SUBMITTOR_AD, reportSubmittorScanFunc ) ) {
		dprintf( D_ALWAYS, "Error making monthly report (submittor scan)\n" );
	}
	fprintf( mailer , "%20s\t%20s\n" , ATTR_RUNNING_JOBS , ATTR_IDLE_JOBS );
	fprintf( mailer , "%20d\t%20d\n" , submittorRunningJobs,submittorIdleJobs );

	// If we've got any, find the maxes
	if ( ustatsMonthly.getCount( ) ) {
		fprintf( mailer , "\n%20s\t%20s\n" , "Universe", "Max Running Jobs" );
		int		univ;
		for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
			const char	*name = ustatsMonthly.getName( univ );
			if ( name ) {
				fprintf( mailer, "%20s\t%20d\n",
						 name, ustatsMonthly.getValue(univ) );
			}
		}
		fprintf( mailer, "%20s\t%20d\n",
				 "All", ustatsMonthly.getCount( ) );
	}
	ustatsMonthly.Reset( );
	
	email_close( mailer );
	return;
}
	
void CollectorDaemon::Config()
{
	dprintf(D_ALWAYS, "In CollectorDaemon::Config()\n");

    char *tmp;
    int     ClassadLifetime;
    int     MasterCheckInterval;

    tmp = param ("CLIENT_TIMEOUT");
    if( tmp ) {
        ClientTimeout = atoi( tmp );
        free( tmp );
    } else {
        ClientTimeout = 30;
    }

    tmp = param ("QUERY_TIMEOUT");
    if( tmp ) {
        QueryTimeout = atoi( tmp );
        free( tmp );
    } else {
        QueryTimeout = 60;
    }

    tmp = param ("CLASSAD_LIFETIME");
    if( tmp ) {
        ClassadLifetime = atoi( tmp );
        free( tmp );
    } else {
        ClassadLifetime = 900;
    }

    tmp = param ("MASTER_CHECK_INTERVAL");
    if( tmp ) {
        MasterCheckInterval = atoi( tmp );
        free( tmp );
    } else {
        MasterCheckInterval = 0;
        // MasterCheckInterval = 10800;    // three hours
    }

    if (CollectorName) free (CollectorName);
    CollectorName = param("COLLECTOR_NAME");

    // handle params for Collector updates
    if ( UpdateTimerId >= 0 ) {
            daemonCore->Cancel_Timer(UpdateTimerId);
            UpdateTimerId = -1;
    }

    tmp = param ("CONDOR_DEVELOPERS_COLLECTOR");
    if (tmp == NULL) {
            tmp = strdup("condor.cs.wisc.edu");
    }
    if (stricmp(tmp,"NONE") == 0 ) {
            free(tmp);
            tmp = NULL;
    }
	int i;
    char* tmp1 = param("COLLECTOR_UPDATE_INTERVAL");
    if ( tmp1 ) {
            i = atoi(tmp1);
    } else {
            i = 900;                // default to 15 minutes
    }
    if ( tmp && i ) {
		if( updateCollector ) {
				// we should just delete it.  since we never use TCP
				// for these updates, we don't really loose anything
				// by destroying the object and recreating it...  
			delete updateCollector;
			updateCollector = NULL;
        }
		updateCollector = new DCCollector( tmp, DCCollector::UDP );
		if( UpdateTimerId < 0 ) {
			UpdateTimerId = daemonCore->
				Register_Timer( 1, i, (TimerHandler)sendCollectorAd,
								"sendCollectorAd" );
		}
    } else {
		if( updateCollector ) {
			delete updateCollector;
			updateCollector = NULL;
		}
		if( UpdateTimerId > 0 ) {
			daemonCore->Cancel_Timer( UpdateTimerId );
			UpdateTimerId = -1;
		}
	}

	init_classad(i);

    if (tmp)
        free(tmp);
    if (tmp1)
        free(tmp1);

    // set the appropriate parameters in the collector engine
    collector.setClientTimeout( ClientTimeout );
    collector.scheduleHousekeeper( ClassadLifetime );
    if (MasterCheckInterval>0) collector.scheduleDownMasterCheck( MasterCheckInterval );

    // if we're not the View Collector, let's set something up to forward
    // all of our ads to the view collector.
    if(View_Collector) {
        delete View_Collector;
    }

    if(view_sock) {
        delete view_sock;
    }	

    tmp = param("CONDOR_VIEW_HOST");
    if(tmp) {
       if(!same_host(my_full_hostname(), tmp) ) {
           dprintf(D_ALWAYS, "Will forward ads on to View Server %s\n", tmp);
           View_Collector = new DCCollector( tmp );
       } 
       free(tmp);
       if(View_Collector) {
           view_sock = View_Collector->safeSock(); 
       }
    }

	tmp = param( "COLLECTOR_SOCKET_CACHE_SIZE" );
	if( tmp ) {
		int size = atoi( tmp );
		if( size ) {
			if( size < 0 || size > 64000 ) {
					// the upper bound here is because a TCP port
					// can't be any bigger than 64K (65536).  however,
					// b/c of reserved ports and some other things, we
					// leave it at 64000, just to be safe...
				EXCEPT( "COLLECTOR_SOCKET_CACHE_SIZE must be between "
						"0 and 64000 (you used: %s)", tmp );
			}
			if( sock_cache ) {
				if( size > sock_cache->size() ) {
					sock_cache->resize( size );
				}
			} else {
				sock_cache = new SocketCache( size );
			}
		} 
		free( tmp );
	}
	if( sock_cache ) {
		dprintf( D_FULLDEBUG, 
				 "Using a SocketCache for TCP updates (size: %d)\n",
				 sock_cache->size() );
	} else {
		dprintf( D_FULLDEBUG, "No SocketCache, will refuse TCP updates\n" );
	}		

    tmp = param ("COLLECTOR_CLASS_HISTORY_SIZE");
    if( tmp ) {
		int	size = atoi( tmp );
        collectorStats.setClassHistorySize( size );
    } else {
        collectorStats.setClassHistorySize( 1024 );
    }

    tmp = param ("COLLECTOR_DAEMON_STATS");
	if( tmp ) {
		if( ( *tmp == 't' || *tmp == 'T' ) ) {
			collectorStats.setDaemonStats( true );
		} else {
			collectorStats.setDaemonStats( false );
		}
		free( tmp );
	} else {
		collectorStats.setDaemonStats( true );
	}

    tmp = param ("COLLECTOR_DAEMON_HISTORY_SIZE");
    if( tmp ) {
		int	size = atoi( tmp );
        collectorStats.setDaemonHistorySize( size );
    } else {
        collectorStats.setDaemonHistorySize( 128 );
    }

    tmp = param ("COLLECTOR_QUERY_WORKERS");
    if( tmp ) {
		int	num = atoi( tmp );
        forkQuery.setMaxWorkers( num );
    } else {
        forkQuery.setMaxWorkers( 0 );
    }

    return;
}

void CollectorDaemon::Exit()
{
	return;
}

void CollectorDaemon::Shutdown()
{
	return;
}

int CollectorDaemon::sendCollectorAd()
{
    // compute submitted jobs information
    submittorRunningJobs = 0;
    submittorIdleJobs = 0;
    if( !collector.walkHashTable( SUBMITTOR_AD, reportSubmittorScanFunc ) ) {
         dprintf( D_ALWAYS, "Error making collector ad (submittor scan)\n" );
    }

    // compute machine information
    machinesTotal = 0;
    machinesUnclaimed = 0;
    machinesClaimed = 0;
    machinesOwner = 0;
	ustatsAccum.Reset( );
    if (!collector.walkHashTable (STARTD_AD, reportMiniStartdScanFunc)) {
            dprintf (D_ALWAYS, "Error making collector ad (startd scan) \n");
    }

    // If we don't have any machines, then bail out. You oftentimes
    // see people run a collector on each macnine in their pool. Duh.
    if(machinesTotal == 0) {
		return 1;
	} 
    // insert values into the ad
    char line[100];
    sprintf(line,"%s = %d",ATTR_RUNNING_JOBS,submittorRunningJobs);
    ad->Insert(line);
    sprintf(line,"%s = %d",ATTR_IDLE_JOBS,submittorIdleJobs);
    ad->Insert(line);
    sprintf(line,"%s = %d",ATTR_NUM_HOSTS_TOTAL,machinesTotal);
    ad->Insert(line);
    sprintf(line,"%s = %d",ATTR_NUM_HOSTS_CLAIMED,machinesClaimed);
    ad->Insert(line);
    sprintf(line,"%s = %d",ATTR_NUM_HOSTS_UNCLAIMED,machinesUnclaimed);
    ad->Insert(line);
    sprintf(line,"%s = %d",ATTR_NUM_HOSTS_OWNER,machinesOwner);
    ad->Insert(line);

	// Accumulate for the monthly
	ustatsMonthly.setMax( ustatsAccum );

	// If we've got any universe reports, find the maxes
	ustatsAccum.publish( ATTR_CURRENT_JOBS_RUNNING, ad );
	ustatsMonthly.publish( ATTR_MAX_JOBS_RUNNING, ad );

	// Collector engine stats, too
	collectorStats.publishGlobal( ad );

    // Send the ad
	char *update_addr = updateCollector->addr();
	if (!update_addr) update_addr = "(null)";
	if( ! updateCollector->sendUpdate(UPDATE_COLLECTOR_AD, ad) ) {
		dprintf( D_ALWAYS, "Can't send UPDATE_COLLECTOR_AD to collector "
				 "(%s): %s\n", update_addr,
				 updateCollector->error() );
		return 0;
    }
    return 1;
}
 
void CollectorDaemon::init_classad(int interval)
{
    if( ad ) delete( ad );
    ad = new ClassAd();

    ad->SetMyTypeName(COLLECTOR_ADTYPE);
    ad->SetTargetTypeName("");

    char *tmp;
    ad->Assign(ATTR_MACHINE, my_full_hostname());

    tmp = param( "CONDOR_ADMIN" );
    if( tmp ) {
        ad->Assign( ATTR_CONDOR_ADMIN, tmp );
        free( tmp );
    }

    if( CollectorName ) {
            ad->Assign( ATTR_NAME, CollectorName );
    } else {
            ad->Assign( ATTR_NAME, my_full_hostname() );
    }

    ad->Assign( ATTR_COLLECTOR_IP_ADDR, global_dc_sinful() );

    if ( interval > 0 ) {
            ad->Assign( ATTR_UPDATE_INTERVAL, 24*interval );
    }

    // In case COLLECTOR_EXPRS is set, fill in our ClassAd with those
    // expressions.
    config_fill_ad( ad );      
}

void 
CollectorDaemon::send_classad_to_sock(int cmd, Daemon * d, ClassAd* theAd)
{
    // view_sock is static
    if(!view_sock) {
	dprintf(D_ALWAYS, "Trying to forward ad on, but no connection to View "
		"Collector!\n");
        return;
    }
    if(!theAd) {
	dprintf(D_ALWAYS, "Trying to forward ad on, but ad is NULL!!!\n"); 
        return;
    }
    if (! d->startCommand(cmd, view_sock)) {
        dprintf( D_ALWAYS, "Can't send command %d to View Collector\n", cmd);
        view_sock->end_of_message();
        return;
    }

    if( theAd ) {
        if( ! theAd->put( *view_sock ) ) {
            dprintf( D_ALWAYS, "Can't forward classad to View Collector\n");
            view_sock->end_of_message();
            return;
        }
    }
    if( ! view_sock->end_of_message() ) {
        dprintf( D_ALWAYS, "Can't send end_of_message to View Collector\n");
        return;
    }
    return;
}

//  Collector stats on universes
CollectorUniverseStats::CollectorUniverseStats( void )
{
	Reset( );
}

//  Collector stats on universes
CollectorUniverseStats::CollectorUniverseStats( CollectorUniverseStats &ref )
{
	int		univ;

	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		perUniverse[univ] = ref.perUniverse[univ];
	}
	count = ref.count;
}

CollectorUniverseStats::~CollectorUniverseStats( void )
{
}

void
CollectorUniverseStats::Reset( void )
{
	int		univ;

	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		perUniverse[univ] = 0;
	}
	count = 0;
}

void
CollectorUniverseStats::accumulate(int univ )
{
	if (  ( univ >= 0 ) && ( univ < CONDOR_UNIVERSE_MAX ) ) {
		perUniverse[univ]++;
		count++;
	}
}

int
CollectorUniverseStats::getValue (int univ )
{
	if (  ( univ >= 0 ) && ( univ < CONDOR_UNIVERSE_MAX ) ) {
		return perUniverse[univ];
	} else {
		return -1;
	}
}

int
CollectorUniverseStats::getCount ( void )
{
	return count;
}

int
CollectorUniverseStats::setMax( CollectorUniverseStats &ref )
{
	int		univ;

	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		if ( ref.perUniverse[univ] > perUniverse[univ] ) {
			perUniverse[univ] = ref.perUniverse[univ];
		}
		if ( ref.count > count ) {
			count = ref.count;
		}
	}
	return 0;
}

const char *
CollectorUniverseStats::getName( int univ )
{
	return CondorUniverseNameUcFirst( univ );
}

int
CollectorUniverseStats::publish( const char *label, ClassAd *ad )
{
	int	univ;
	char line[100];

	// Loop through, publish all universes with a name
	for( univ=0;  univ<CONDOR_UNIVERSE_MAX;  univ++) {
		const char *name = getName( univ );
		if ( name ) {
			sprintf( line, "%s%s = %d", label, name, getValue( univ ) );
			ad->Insert(line);
		}
	}
	sprintf( line, "%s%s = %d", label, "All", count );
	ad->Insert(line);

	return 0;
}
