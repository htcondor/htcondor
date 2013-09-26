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
#include "condor_classad.h"
#include "condor_status.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "internet.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_email.h"
#include "condor_query.h"

#include "condor_daemon_core.h"
#include "status_types.h"
#include "totals.h"

#include "condor_collector.h"
#include "collector_engine.h"
#include "HashTable.h"
#include "hashkey.h"

#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_universe.h"
#include "ipv6_hostname.h"
#include "condor_threads.h"

#include "collector.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
#include "CollectorPlugin.h"
#endif
#endif

#include "ccb_server.h"

using std::vector;
using std::string;

//----------------------------------------------------------------

extern "C" char* CondorVersion( void );
extern "C" char* CondorPlatform( void );

CollectorStats CollectorDaemon::collectorStats( false, 0, 0 );
CollectorEngine CollectorDaemon::collector( &collectorStats );
int CollectorDaemon::ClientTimeout;
int CollectorDaemon::QueryTimeout;
char* CollectorDaemon::CollectorName;
Timeslice CollectorDaemon::view_sock_timeslice;
vector<CollectorDaemon::vc_entry> CollectorDaemon::vc_list;

ClassAd* CollectorDaemon::__query__;
int CollectorDaemon::__numAds__;
int CollectorDaemon::__failed__;
List<ClassAd>* CollectorDaemon::__ClassAdResultList__;
std::string CollectorDaemon::__adType__;
ExprTree *CollectorDaemon::__filter__;

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
CollectorList* CollectorDaemon::updateCollectors;
DCCollector* CollectorDaemon::updateRemoteCollector;
int CollectorDaemon::UpdateTimerId;

ClassAd *CollectorDaemon::query_any_result;
ClassAd CollectorDaemon::query_any_request;

OfflineCollectorPlugin CollectorDaemon::offline_plugin_;

StringList *viewCollectorTypes;

CCBServer *CollectorDaemon::m_ccb_server;
bool CollectorDaemon::filterAbsentAds;

//---------------------------------------------------------

// prototypes of library functions
typedef void (*SIGNAL_HANDLER)();
extern "C"
{
	void install_sig_handler( int, SIGNAL_HANDLER );
	void schedule_event ( int month, int day, int hour, int minute, int second, SIGNAL_HANDLER );
}
 
//----------------------------------------------------------------

void
computeProjection(ClassAd *full_ad, SimpleList<MyString> *projectionList,StringList &expanded_projection);

void CollectorDaemon::Init()
{
	dprintf(D_ALWAYS, "In CollectorDaemon::Init()\n");

	// read in various parameters from condor_config
	CollectorName=NULL;
	ad=NULL;
	viewCollectorTypes = NULL;
	UpdateTimerId=-1;
	updateCollectors = NULL;
	updateRemoteCollector = NULL;
	Config();

	/* TODO: Eval notes and refactor when time permits.
	 * 
	 * per-review <tstclair> this is a really unintuive and I would consider unclean.
	 * Maybe if we care about cron like events we should develop a clean mechanism
	 * which doesn't indirectly hook into daemon-core timers. */

    // setup routine to report to condor developers
    // schedule reports to developers
	schedule_event( -1, 1,  0, 0, 0, reportToDevelopers );
	schedule_event( -1, 8,  0, 0, 0, reportToDevelopers );
	schedule_event( -1, 15, 0, 0, 0, reportToDevelopers );
	schedule_event( -1, 23, 0, 0, 0, reportToDevelopers );

	// install command handlers for queries
	daemonCore->Register_CommandWithPayload(QUERY_STARTD_ADS,"QUERY_STARTD_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_STARTD_PVT_ADS,"QUERY_STARTD_PVT_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,NEGOTIATOR);
#ifdef HAVE_EXT_POSTGRESQL
	daemonCore->Register_CommandWithPayload(QUERY_QUILL_ADS,"QUERY_QUILL_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
#endif /* HAVE_EXT_POSTGRESQL */

	daemonCore->Register_CommandWithPayload(QUERY_SCHEDD_ADS,"QUERY_SCHEDD_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_MASTER_ADS,"QUERY_MASTER_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_CKPT_SRVR_ADS,"QUERY_CKPT_SRVR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_SUBMITTOR_ADS,"QUERY_SUBMITTOR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_LICENSE_ADS,"QUERY_LICENSE_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_COLLECTOR_ADS,"QUERY_COLLECTOR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,ADMINISTRATOR);
	daemonCore->Register_CommandWithPayload(QUERY_STORAGE_ADS,"QUERY_STORAGE_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_NEGOTIATOR_ADS,"QUERY_NEGOTIATOR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_HAD_ADS,"QUERY_HAD_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_XFER_SERVICE_ADS,"QUERY_XFER_SERVICE_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_LEASE_MANAGER_ADS,"QUERY_LEASE_MANAGER_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_ANY_ADS,"QUERY_ANY_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
    daemonCore->Register_CommandWithPayload(QUERY_GRID_ADS,"QUERY_GRID_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_GENERIC_ADS,"QUERY_GENERIC_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	
	// install command handlers for invalidations
	daemonCore->Register_CommandWithPayload(INVALIDATE_STARTD_ADS,"INVALIDATE_STARTD_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,ADVERTISE_STARTD_PERM);

#ifdef HAVE_EXT_POSTGRESQL
	daemonCore->Register_CommandWithPayload(INVALIDATE_QUILL_ADS,"INVALIDATE_QUILL_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,DAEMON);
#endif /* HAVE_EXT_POSTGRESQL */

	daemonCore->Register_CommandWithPayload(INVALIDATE_SCHEDD_ADS,"INVALIDATE_SCHEDD_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(INVALIDATE_MASTER_ADS,"INVALIDATE_MASTER_ADS",
		(CommandHandler)receive_invalidation,"receive_invalidation",NULL,ADVERTISE_MASTER_PERM);
	daemonCore->Register_CommandWithPayload(INVALIDATE_CKPT_SRVR_ADS,
		"INVALIDATE_CKPT_SRVR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_SUBMITTOR_ADS,
		"INVALIDATE_SUBMITTOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(INVALIDATE_LICENSE_ADS,
		"INVALIDATE_LICENSE_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_COLLECTOR_ADS,
		"INVALIDATE_COLLECTOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_STORAGE_ADS,
		"INVALIDATE_STORAGE_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_NEGOTIATOR_ADS,
		"INVALIDATE_NEGOTIATOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(INVALIDATE_HAD_ADS,
		"INVALIDATE_HAD_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_ADS_GENERIC,
		"INVALIDATE_ADS_GENERIC", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_XFER_SERVICE_ADS,
		"INVALIDATE_XFER_ENDPOINT_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_LEASE_MANAGER_ADS,
		"INVALIDATE_LEASE_MANAGER_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
    daemonCore->Register_CommandWithPayload(INVALIDATE_GRID_ADS,
        "INVALIDATE_GRID_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);

	// install command handlers for updates

#ifdef HAVE_EXT_POSTGRESQL
	daemonCore->Register_CommandWithPayload(UPDATE_QUILL_AD,"UPDATE_QUILL_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
#endif /* HAVE_EXT_POSTGRESQL */

	daemonCore->Register_CommandWithPayload(UPDATE_STARTD_AD,"UPDATE_STARTD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,ADVERTISE_STARTD_PERM);
	daemonCore->Register_CommandWithPayload(MERGE_STARTD_AD,"MERGE_STARTD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(UPDATE_SCHEDD_AD,"UPDATE_SCHEDD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(UPDATE_SUBMITTOR_AD,"UPDATE_SUBMITTOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,ADVERTISE_SCHEDD_PERM);
	daemonCore->Register_CommandWithPayload(UPDATE_LICENSE_AD,"UPDATE_LICENSE_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_MASTER_AD,"UPDATE_MASTER_AD",
		(CommandHandler)receive_update,"receive_update",NULL,ADVERTISE_MASTER_PERM);
	daemonCore->Register_CommandWithPayload(UPDATE_CKPT_SRVR_AD,"UPDATE_CKPT_SRVR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_COLLECTOR_AD,"UPDATE_COLLECTOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,ALLOW);
	daemonCore->Register_CommandWithPayload(UPDATE_STORAGE_AD,"UPDATE_STORAGE_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_NEGOTIATOR_AD,"UPDATE_NEGOTIATOR_AD",
		(CommandHandler)receive_update,"receive_update",NULL,NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(UPDATE_HAD_AD,"UPDATE_HAD_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_XFER_SERVICE_AD,"UPDATE_XFER_SERVICE_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_LEASE_MANAGER_AD,"UPDATE_LEASE_MANAGER_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_AD_GENERIC, "UPDATE_AD_GENERIC",
		(CommandHandler)receive_update,"receive_update", NULL, DAEMON);
    daemonCore->Register_CommandWithPayload(UPDATE_GRID_AD,"UPDATE_GRID_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);

    // install command handlers for updates with acknowledgement

    daemonCore->Register_CommandWithPayload(
		UPDATE_STARTD_AD_WITH_ACK,
		"UPDATE_STARTD_AD_WITH_ACK",
		(CommandHandler)receive_update_expect_ack,
		"receive_update_expect_ack",NULL,ADVERTISE_STARTD_PERM);

    // add all persisted ads back into the collector's store
    // process the given command
    int     insert = -3;
    ClassAd *tmpad;
    offline_plugin_.rewind ();
    while ( offline_plugin_.iterate ( tmpad ) ) {
		tmpad = new ClassAd(*tmpad);
	    if ( !collector.collect (UPDATE_STARTD_AD, tmpad, condor_sockaddr::null,
								 insert ) ) {
		    
            if ( -3 == insert ) {

                /* this happens when we get a classad for which a hash 
                key could not been made. This occurs when certain 
                attributes are needed for the particular category the
                ad is destined for, but they are not present in the 
                ad itself. */
			    dprintf (
                    D_ALWAYS,
				    "Received malformed ad. Ignoring.\n" );

	        }
			delete tmpad;
	    }

    }

	forkQuery.Initialize( );
}

int CollectorDaemon::receive_query_cedar(Service* /*s*/,
										 int command,
										 Stream* sock)
{
	ClassAd cad;

	sock->decode();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	bool ep = CondorThreads::enable_parallel(true);
	bool res = !getClassAd(sock, cad) || !sock->end_of_message();
	CondorThreads::enable_parallel(ep);
    if( res )
    {
        dprintf(D_ALWAYS,"Failed to receive query on TCP: aborting\n");
        return FALSE;
    }

		// here we set up a network timeout of a longer duration
	sock->timeout(QueryTimeout);

	// Initial query handler
	AdTypes whichAds = receive_query_public( command );

	UtcTime begin(true);

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
			process_query_public (whichAds, &cad, &results);
		}
	}

	UtcTime end_write, end_query(true);

	// send the results via cedar
	sock->encode();
	results.Rewind();
	ClassAd *curr_ad = NULL;
	int more = 1;
	

		// See if query ad asks for server-side projection
	string projection = "";

	while ( (curr_ad=results.Next()) )
    {
		StringList expanded_projection;
		StringList *attr_whitelist=NULL;

		projection = "";
		cad.EvalString(ATTR_PROJECTION, curr_ad, projection);
		SimpleList<MyString> projectionList;

		::split_args(projection.c_str(), &projectionList);

		if (projectionList.Number() > 0) {
			computeProjection(curr_ad, &projectionList, expanded_projection);
			attr_whitelist = &expanded_projection;
		}
		
        if (!sock->code(more) || !putClassAd(sock, *curr_ad, false, attr_whitelist))
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

	end_write.getTime();

	dprintf (D_ALWAYS,
			 "Query info: matched=%d; skipped=%d; query_time=%f; send_time=%f; type=%s; requirements={%s}; peer=%s; projection={%s}\n",
			 __numAds__,
			 __failed__,
			 end_query.difference(begin),
			 end_write.difference(end_query),
			 AdTypeToString(whichAds),
			 ExprTreeToString(__filter__),
			 sock->peer_description(),
			 projection.c_str());

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

#ifdef HAVE_EXT_POSTGRESQL
	  case QUERY_QUILL_ADS:
		dprintf (D_ALWAYS, "Got QUERY_QUILL_ADS\n");
		whichAds = QUILL_AD;
		break;
#endif /* HAVE_EXT_POSTGRESQL */
		
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

	  case QUERY_XFER_SERVICE_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_XFER_SERVICE_ADS\n");
		whichAds = XFER_SERVICE_AD;
		break;

	  case QUERY_LEASE_MANAGER_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_LEASE_MANAGER_ADS\n");
		whichAds = LEASE_MANAGER_AD;
		break;

	  case QUERY_GENERIC_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_GENERIC_ADS\n");
		whichAds = GENERIC_AD;
		break;

	  case QUERY_ANY_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_ANY_ADS\n");
		whichAds = ANY_AD;
		break;
      
      case QUERY_GRID_ADS:
        dprintf (D_FULLDEBUG,"Got QUERY_GRID_ADS\n");
        whichAds = GRID_AD;
		break;

	  default:
		dprintf(D_ALWAYS,
				"Unknown command %d in receive_query_public()\n",
				command);
		whichAds = (AdTypes) -1;
    }

	return whichAds;
}

int CollectorDaemon::receive_invalidation(Service* /*s*/,
										  int command,
										  Stream* sock)
{
	AdTypes whichAds;
	ClassAd cad;

	sock->decode();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

    if( !getClassAd(sock, cad) || !sock->end_of_message() )
    {
        dprintf( D_ALWAYS,
				 "Failed to receive invalidation on %s: aborting\n",
				 sock->type() == Stream::reli_sock ? "TCP" : "UDP" );
        return FALSE;
    }
#if defined(ADD_TARGET_SCOPING)
	RemoveExplicitTargetRefs( cad );
#endif

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

#ifdef HAVE_EXT_POSTGRESQL
	  case INVALIDATE_QUILL_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_QUILL_ADS\n");
		whichAds = QUILL_AD;
		break;
#endif /* HAVE_EXT_POSTGRESQL */
		
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

	  case INVALIDATE_XFER_SERVICE_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_XFER_SERVICE_ADS\n");
		whichAds = XFER_SERVICE_AD;
		break;

	  case INVALIDATE_LEASE_MANAGER_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_LEASE_MANAGER_ADS\n");
		whichAds = LEASE_MANAGER_AD;
		break;

	  case INVALIDATE_STORAGE_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_STORAGE_ADS\n");
		whichAds = STORAGE_AD;
		break;

	  case INVALIDATE_ADS_GENERIC:
		dprintf(D_ALWAYS, "Got INVALIDATE_ADS_GENERIC\n");
		whichAds = GENERIC_AD;
		break;

      case INVALIDATE_GRID_ADS:
        dprintf(D_ALWAYS, "Got INVALIDATE_GRID_ADS\n");
        whichAds = GRID_AD;
		break;

	  default:
		dprintf(D_ALWAYS,"Unknown command %d in receive_invalidation()\n",
			command);
		whichAds = (AdTypes) -1;
    }

    if (whichAds != (AdTypes) -1)
		process_invalidation (whichAds, cad, sock);

	// if the invalidation was for the STARTD ads, also invalidate startd
	// private ads with the same query ad
	if (command == INVALIDATE_STARTD_ADS)
		process_invalidation (STARTD_PVT_AD, cad, sock);

    /* let the off-line plug-in invalidate the given ad */
    offline_plugin_.invalidate ( command, cad );

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
	CollectorPluginManager::Invalidate(command, cad);
#endif
#endif

	if (viewCollectorTypes) {
		forward_classad_to_view_collector(command,
										  ATTR_TARGET_TYPE,
										  &cad);
	} else if ((command == INVALIDATE_STARTD_ADS) || (command == INVALIDATE_SUBMITTOR_ADS)) {
		send_classad_to_sock(command, &cad);
    }

	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		return stashSocket( (ReliSock *)sock );
	}

    // all done; let daemon core will clean up connection
	return TRUE;
}


int CollectorDaemon::receive_update(Service* /*s*/, int command, Stream* sock)
{
    int	insert;
	ClassAd *cad;

	/* assume the ad is malformed... other functions set this value */
	insert = -3;

	// get endpoint
	condor_sockaddr from = ((Sock*)sock)->peer_addr();

    // process the given command
	if (!(cad = collector.collect (command,(Sock*)sock,from,insert)))
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

		return FALSE;

	}

	/* let the off-line plug-in have at it */
	offline_plugin_.update ( command, *cad );

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
	CollectorPluginManager::Update(command, *cad);
#endif
#endif

	if (viewCollectorTypes) {
		forward_classad_to_view_collector(command,
										  ATTR_MY_TYPE,
										  cad);
	} else if ((command == UPDATE_STARTD_AD) || (command == UPDATE_SUBMITTOR_AD)) {
        send_classad_to_sock(command, cad);
	}

	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		return stashSocket( (ReliSock *)sock );
	}

	// let daemon core clean up the socket
	return TRUE;
}

int CollectorDaemon::receive_update_expect_ack( Service* /*s*/,
												int command,
												Stream *stream )
{

    if ( NULL == stream ) {
        return FALSE;
    }

    Sock        *socket = (Sock*) stream;
    ClassAd     *updateAd = new ClassAd;
    const int   timeout = 5;
    int         ok      = 1;
    
    socket->decode ();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	socket->timeout(1);

    if ( !getClassAd ( socket, *updateAd ) ) {

        dprintf ( 
            D_ALWAYS,
            "receive_update_expect_ack: "
            "Failed to read class ad off the wire, aborting\n" );

        return FALSE;

    }

    /* assume the ad is malformed */
    int insert = -3;
    
    /* get peer's IP/port */
	condor_sockaddr from = socket->peer_addr();

    /* "collect" the ad */
    ClassAd *cad = collector.collect ( 
        command,
        updateAd,
        from,
        insert );

    if ( !cad ) {

        /* attempting to "collect" a QUERY or INVALIDATE command?!? */
        if ( -2 == insert ) {

	        dprintf (
                D_ALWAYS,
                "receive_update_expect_ack: "
                "Got QUERY or INVALIDATE command (%d); these are "
                "not supported.\n",
                command );

        }

        /* this happens when we get a classad for which a hash key 
        could not been made. This occurs when certain attributes are 
        needed for the particular catagory the ad is destined for, 
        but they are not present in the ad. */
	    if ( -3 == insert ) {
			
	        dprintf (
                D_ALWAYS,
                "receive_update_expect_ack: "
		        "Received malformed ad from command (%d).\n",
                command );

	    }

    } else {

        socket->encode ();
        socket->timeout ( timeout );

        /* send an acknowledgment that we received the ad */
        if ( !socket->code ( ok ) ) {
        
            dprintf ( 
                D_ALWAYS,
                "receive_update_expect_ack: "
                "Failed to send acknowledgement to host %s, "
                "aborting\n",
                socket->peer_ip_str () );
        
            /* it's ok if we fail here, since we won't drop the ad,
            it's only on the client side that any error should be
            treated as fatal */

        }

        if ( !socket->end_of_message () ) {
        
            dprintf ( 
                D_FULLDEBUG, 
                "receive_update_expect_ack: "
                "Failed to send update EOM to host %s.\n", 
                socket->peer_ip_str () );
            
	    }   
        
    }

    /* let the off-line plug-in have at it */
	if(cad)
    offline_plugin_.update ( command, *cad );

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
    CollectorPluginManager::Update ( command, *cad );
#endif
#endif

	if (viewCollectorTypes) {
		forward_classad_to_view_collector(command,
										  ATTR_MY_TYPE,
										  cad);
	} else if (UPDATE_STARTD_AD_WITH_ACK == command) {
		send_classad_to_sock(command, cad);
	}

	// let daemon core clean up the socket
	return TRUE;
}


int
CollectorDaemon::stashSocket( ReliSock* sock )
{
	if( daemonCore->SocketIsRegistered( sock ) ) {
		return KEEP_STREAM;
	}

	MyString msg;
	if( daemonCore->TooManyRegisteredSockets(sock->get_file_desc(),&msg) ) {
		dprintf(D_ALWAYS,
				"WARNING: cannot register TCP update socket from %s: %s\n",
				sock->peer_description(), msg.Value());
		return FALSE;
	}

		// Register this socket w/ DaemonCore so we wake up if
		// there's more data to read...
	int rc = daemonCore->Register_Command_Socket(
		sock, "Update Socket" );

	if( rc < 0 ) {
		dprintf(D_ALWAYS,
				"Failed to register TCP socket from %s for updates: error %d.\n",
				sock->peer_description(),rc);
		return FALSE;
	}

	dprintf( D_FULLDEBUG,
			 "Registered TCP socket from %s for updates.\n",
			 sock->peer_description() );

	return KEEP_STREAM;
}

int CollectorDaemon::query_scanFunc (ClassAd *cad)
{
	if ( !__adType__.empty() ) {
		std::string type = "";
		cad->LookupString( ATTR_MY_TYPE, type );
		if ( strcasecmp( type.c_str(), __adType__.c_str() ) != 0 ) {
			return 1;
		}
	}

	classad::Value result;
	bool val;
	if ( EvalExprTree( __filter__, cad, NULL, result ) &&
		 result.IsBooleanValueEquiv(val) && val ) {
		// Found a match 
        __numAds__++;
		__ClassAdResultList__->Append(cad);
    } else {
		__failed__++;
	}

    return 1;
}

void CollectorDaemon::process_query_public (AdTypes whichAds,
											ClassAd *query,
											List<ClassAd>* results)
{
#if defined(ADD_TARGET_SCOPING)
	RemoveExplicitTargetRefs( *query );
#endif
	// set up for hashtable scan
	__query__ = query;
	__numAds__ = 0;
	__failed__ = 0;
	__ClassAdResultList__ = results;
	// An empty adType means don't check the MyType of the ads.
	// This means either the command indicates we're only checking one
	// type of ad, or the query's TargetType is "Any" (match all ad types).
	__adType__ = "";
	if ( whichAds == GENERIC_AD || whichAds == ANY_AD ) {
		query->LookupString( ATTR_TARGET_TYPE, __adType__ );
		if ( strcasecmp( __adType__.c_str(), "any" ) == 0 ) {
			__adType__ = "";
		}
	}

	__filter__ = query->LookupExpr( ATTR_REQUIREMENTS );
	if ( __filter__ == NULL ) {
		dprintf (D_ALWAYS, "Query missing %s\n", ATTR_REQUIREMENTS );
		return;
	}

	// If ABSENT_REQUIREMENTS is defined, rewrite filter to filter-out absent ads 
	// if ATTR_ABSENT is not alrady referenced in the query.
	if ( filterAbsentAds ) {	// filterAbsentAds is true if ABSENT_REQUIREMENTS defined
		StringList job_refs;      // job attrs referenced by requirements
		StringList machine_refs;  // machine attrs referenced by requirements
		bool checks_absent = false;

		query->GetReferences(ATTR_REQUIREMENTS,job_refs,machine_refs);
		checks_absent = machine_refs.contains_anycase( ATTR_ABSENT );
		if (!checks_absent) {
			MyString modified_filter;
			modified_filter.formatstr("(%s) && (%s =!= True)",
				ExprTreeToString(__filter__),ATTR_ABSENT);
			query->AssignExpr(ATTR_REQUIREMENTS,modified_filter.Value());
			__filter__ = query->LookupExpr(ATTR_REQUIREMENTS);
			if ( __filter__ == NULL ) {
				dprintf (D_ALWAYS, "Failed to parse modified filter: %s\n", 
					modified_filter.Value());
				return;
			}
			dprintf(D_FULLDEBUG,"Query after modification: *%s*\n",modified_filter.Value());
		}
	}

	if (!collector.walkHashTable (whichAds, query_scanFunc))
	{
		dprintf (D_ALWAYS, "Error sending query response\n");
	}

	dprintf (D_ALWAYS, "(Sending %d ads in response to query)\n", __numAds__);
}	

//
// Setting ATTR_LAST_HEARD_FROM to 0 causes the housekeeper to invalidate
// the ad.  Since we don't want that -- we just want the ad to expire --
// set the time to the next-smallest legal value, instead.  Expiring
// invalidated ads allows the offline plugin to decide if they should go
// absent, instead.
//
int CollectorDaemon::expiration_scanFunc (ClassAd *cad)
{
    return setAttrLastHeardFrom( cad, 1 );
}

int CollectorDaemon::invalidation_scanFunc (ClassAd *cad)
{
    return setAttrLastHeardFrom( cad, 0 );
}

int CollectorDaemon::setAttrLastHeardFrom (ClassAd* cad, unsigned long time)
{
	if ( !__adType__.empty() ) {
		std::string type = "";
		cad->LookupString( ATTR_MY_TYPE, type );
		if ( strcasecmp( type.c_str(), __adType__.c_str() ) != 0 ) {
			return 1;
		}
	}

	classad::Value result;
	bool val;
	if ( EvalExprTree( __filter__, cad, NULL, result ) &&
		 result.IsBooleanValueEquiv(val) && val ) {

		cad->Assign( ATTR_LAST_HEARD_FROM, time );
        __numAds__++;
    }

    return 1;
}

void CollectorDaemon::process_invalidation (AdTypes whichAds, ClassAd &query, Stream *sock)
{
	if (param_boolean("IGNORE_INVALIDATE", false)) {
		dprintf(D_ALWAYS, "Ignoring invalidate (IGNORE_INVALIDATE=TRUE)\n");
		return;
	}

	// here we set up a network timeout of a longer duration
	sock->timeout(QueryTimeout);

	bool query_contains_hash_key = false;

    bool expireInvalidatedAds = param_boolean( "EXPIRE_INVALIDATED_ADS", false );
    if( expireInvalidatedAds ) {
        __numAds__ = collector.expire( whichAds, query, &query_contains_hash_key );
    } else {        
	__numAds__ = collector.remove( whichAds, query, &query_contains_hash_key );
    }

    if ( !query_contains_hash_key )
	{
		dprintf ( D_ALWAYS, "Walking tables to invalidate... O(n)\n" );

		// set up for hashtable scan
		__query__ = &query;
		__filter__ = query.LookupExpr( ATTR_REQUIREMENTS );
		// An empty adType means don't check the MyType of the ads.
		// This means either the command indicates we're only checking
		// one type of ad, or the query's TargetType is "Any" (match
		// all ad types).
		__adType__ = "";
		if ( whichAds == GENERIC_AD || whichAds == ANY_AD ) {
			query.LookupString( ATTR_TARGET_TYPE, __adType__ );
			if ( strcasecmp( __adType__.c_str(), "any" ) == 0 ) {
				__adType__ = "";
			}
		}

		if ( __filter__ == NULL ) {
			dprintf (D_ALWAYS, "Invalidation missing %s\n", ATTR_REQUIREMENTS );
			return;
		}

        if (expireInvalidatedAds)
        {
            collector.walkHashTable (whichAds, expiration_scanFunc);
            collector.invokeHousekeeper (whichAds);
        } else if (param_boolean("HOUSEKEEPING_ON_INVALIDATE", true)) 
		{
			// first set all the "LastHeardFrom" attributes to low values ...
			collector.walkHashTable (whichAds, invalidation_scanFunc);

			// ... then invoke the housekeeper
			collector.invokeHousekeeper (whichAds);
		} else 
		{
			__numAds__ = collector.invalidateAds(whichAds, query);
		}
	}

	dprintf (D_ALWAYS, "(Invalidated %d ads)\n", __numAds__ );

		// Suppose lots of ads are getting invalidated and we have no clue
		// why.  That is what the following block of code tries to solve.
	if( __numAds__ > 1 ) {
		dprintf(D_ALWAYS, "The invalidation query was this:\n");
		dPrintAd(D_ALWAYS, query);
	}
}	



int CollectorDaemon::reportStartdScanFunc( ClassAd *cad )
{
	return normalTotals->update( cad );
}

int CollectorDaemon::reportSubmittorScanFunc( ClassAd *cad )
{
	int tmp1, tmp2;
	if( !cad->LookupInteger( ATTR_RUNNING_JOBS , tmp1 ) ||
		!cad->LookupInteger( ATTR_IDLE_JOBS, tmp2 ) )
			return 0;
	submittorRunningJobs += tmp1;
	submittorIdleJobs	 += tmp2;

	return 1;
}

int CollectorDaemon::reportMiniStartdScanFunc( ClassAd *cad )
{
    char buf[80];
	int iRet = 0;

	if ( cad && cad->LookupString( ATTR_STATE, buf, sizeof(buf) ) )
	{
		machinesTotal++;
		switch ( buf[0] )
		{
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
		int universe;
		if ( cad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) )
		{
			ustatsAccum.accumulate( universe );
		}
		iRet = 1;
	}

    return iRet;
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
			 get_local_fqdn().Value() );
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

    char	*tmp;
    int     ClassadLifetime;

    ClientTimeout = param_integer ("CLIENT_TIMEOUT",30);
    QueryTimeout = param_integer ("QUERY_TIMEOUT",60);
    ClassadLifetime = param_integer ("CLASSAD_LIFETIME",900);

    if (CollectorName) free (CollectorName);
    CollectorName = param("COLLECTOR_NAME");

	// handle params for Collector updates
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}

	if( updateCollectors ) {
		delete updateCollectors;
		updateCollectors = NULL;
	}
	updateCollectors = CollectorList::create( NULL );

	tmp = param ("CONDOR_DEVELOPERS_COLLECTOR");
	if (tmp == NULL) {
#ifdef NO_PHONE_HOME
		tmp = strdup("NONE");
#else
		tmp = strdup("condor.cs.wisc.edu");
#endif
	}
	if (strcasecmp(tmp,"NONE") == 0 ) {
		free(tmp);
		tmp = NULL;
	}

	if( updateRemoteCollector ) {
		// we should just delete it.  since we never use TCP
		// for these updates, we don't really loose anything
		// by destroying the object and recreating it...
		delete updateRemoteCollector;
		updateRemoteCollector = NULL;
	}
	if ( tmp ) {
		updateRemoteCollector = new DCCollector( tmp, DCCollector::UDP );
	}

	free( tmp );
	
	int i = param_integer("COLLECTOR_UPDATE_INTERVAL",900); // default 15 min
	if( UpdateTimerId < 0 ) {
		UpdateTimerId = daemonCore->
			Register_Timer( 1, i, sendCollectorAd,
							"sendCollectorAd" );
	}

	tmp = param(COLLECTOR_REQUIREMENTS);
	MyString collector_req_err;
	if( !collector.setCollectorRequirements( tmp, collector_req_err ) ) {
		EXCEPT("Handling of '%s=%s' failed: %s\n",
			   COLLECTOR_REQUIREMENTS,
			   tmp ? tmp : "(null)",
			   collector_req_err.Value());
	}
	if( tmp ) {
		free( tmp );
		tmp = NULL;
	}

	init_classad(i);

    // set the appropriate parameters in the collector engine
    collector.setClientTimeout( ClientTimeout );
    collector.scheduleHousekeeper( ClassadLifetime );

    offline_plugin_.configure ();

    // if we're not the View Collector, let's set something up to forward
    // all of our ads to the view collector.
    for (vector<vc_entry>::iterator e(vc_list.begin());  e != vc_list.end();  ++e) {
        delete e->collector;
        delete e->sock;
    }
    vc_list.clear();

    tmp = param("CONDOR_VIEW_HOST");
    if (tmp) {
        StringList cvh(tmp);
        free(tmp);
        cvh.rewind();
        while (char* vhost = cvh.next()) {
            Daemon* vhd = new DCCollector(vhost);
            Sinful view_addr( vhd->addr() );
            Sinful my_addr( daemonCore->publicNetworkIpAddr() );

            if (my_addr.addressPointsToMe(view_addr)) {
                dprintf(D_ALWAYS, "Not forwarding to View Server %s - self referential\n", vhost);
                delete vhd;
                continue;
            }
            dprintf(D_ALWAYS, "Will forward ads on to View Server %s\n", vhost);

            Sock* vhsock = NULL;
            if (vhd->hasUDPCommandPort()) {
                vhsock = new SafeSock();
            } else {
                vhsock = new ReliSock();
            }

            vc_list.push_back(vc_entry());
            vc_list.back().name = vhost;
            vc_list.back().collector = vhd;
            vc_list.back().sock = vhsock;
        }
    }

    if (!vc_list.empty()) {
        // protect against frequent time-consuming reconnect attempts
        view_sock_timeslice.setTimeslice(0.05);
        view_sock_timeslice.setMaxInterval(1200);
    }

	if (viewCollectorTypes) delete viewCollectorTypes;
	viewCollectorTypes = NULL;
	if (!vc_list.empty()) {
		tmp = param("CONDOR_VIEW_CLASSAD_TYPES");
		if (tmp) {
			viewCollectorTypes = new StringList(tmp);
			char *printable_string = viewCollectorTypes->print_to_string();
			dprintf(D_ALWAYS, "CONDOR_VIEW_CLASSAD_TYPES configured, will forward ad types: %s\n",
					printable_string?printable_string:"");
			free(printable_string);
		}
	}

	int size = param_integer ("COLLECTOR_CLASS_HISTORY_SIZE",1024);
	collectorStats.setClassHistorySize( size );

	bool collector_daemon_stats = param_boolean ("COLLECTOR_DAEMON_STATS",true);
	collectorStats.setDaemonStats( collector_daemon_stats );

    size = param_integer ("COLLECTOR_DAEMON_HISTORY_SIZE",128);
    collectorStats.setDaemonHistorySize( size );

	time_t garbage_interval = param_integer( "COLLECTOR_STATS_SWEEP", DEFAULT_COLLECTOR_STATS_GARBAGE_INTERVAL );
	collectorStats.setGarbageCollectionInterval( garbage_interval );

    size = param_integer ("COLLECTOR_QUERY_WORKERS", 2);
    forkQuery.setMaxWorkers( size );

	bool ccb_server_enabled = param_boolean("ENABLE_CCB_SERVER",true);
	if( ccb_server_enabled ) {
		if( !m_ccb_server ) {
			dprintf(D_ALWAYS, "Enabling CCB Server.\n");
			m_ccb_server = new CCBServer;
		}
		m_ccb_server->InitAndReconfig();
	}
	else if( m_ccb_server ) {
		dprintf(D_ALWAYS, "Disabling CCB Server.\n");
		delete m_ccb_server;
		m_ccb_server = NULL;
	}

	if ( (tmp=param("ABSENT_REQUIREMENTS")) ) {
		filterAbsentAds = true;
		free(tmp);
	} else {
		filterAbsentAds = false;
	}

	return;
}

void CollectorDaemon::Exit()
{
	// Clean up any workers that have exited but haven't been reaped yet.
	// This can occur if the collector receives a query followed
	// immediately by a shutdown command.  The worker will exit but
	// not be reaped because the SIGTERM from the shutdown command will
	// be processed before the SIGCHLD from the worker process exit.
	// Allowing the stack to clean up worker processes is problematic
	// because the collector will be shutdown and the daemonCore
	// object deleted by the time the worker cleanup is attempted.
	forkQuery.DeleteAll( );
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}
	return;
}

void CollectorDaemon::Shutdown()
{
	// Clean up any workers that have exited but haven't been reaped yet.
	// This can occur if the collector receives a query followed
	// immediately by a shutdown command.  The worker will exit but
	// not be reaped because the SIGTERM from the shutdown command will
	// be processed before the SIGCHLD from the worker process exit.
	// Allowing the stack to clean up worker processes is problematic
	// because the collector will be shutdown and the daemonCore
	// object deleted by the time the worker cleanup is attempted.
	forkQuery.DeleteAll( );
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}
	return;
}

void CollectorDaemon::sendCollectorAd()
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

    daemonCore->dc_stats.Publish(*ad);
    daemonCore->monitor_data.ExportData(ad);

	// Send the ad
	int num_updated = updateCollectors->sendUpdates(UPDATE_COLLECTOR_AD, ad, NULL, false);
	if ( num_updated != updateCollectors->number() ) {
		dprintf( D_ALWAYS, "Unable to send UPDATE_COLLECTOR_AD to all configured collectors\n");
	}

       // If we don't have any machines, then bail out. You oftentimes
       // see people run a collector on each macnine in their pool. Duh.
	if(machinesTotal == 0) {
		return ;
	}
	if ( updateRemoteCollector ) {
		char update_addr_default [] = "(null)";
		char *update_addr = updateRemoteCollector->addr();
		if (!update_addr) update_addr = update_addr_default;
		if( ! updateRemoteCollector->sendUpdate(UPDATE_COLLECTOR_AD, ad, NULL, false) ) {
			dprintf( D_ALWAYS, "Can't send UPDATE_COLLECTOR_AD to collector "
					 "(%s): %s\n", update_addr,
					 updateRemoteCollector->error() );
			return;
		}
	}
}

void CollectorDaemon::init_classad(int interval)
{
    if( ad ) delete( ad );
    ad = new ClassAd();

    SetMyTypeName(*ad, COLLECTOR_ADTYPE);
    SetTargetTypeName(*ad, "");

    char *tmp;
    tmp = param( "CONDOR_ADMIN" );
    if( tmp ) {
        ad->Assign( ATTR_CONDOR_ADMIN, tmp );
        free( tmp );
    }

    MyString id;
    if( CollectorName ) {
            if( strchr( CollectorName, '@' ) ) {
               id.formatstr( "%s", CollectorName );
            } else {
               id.formatstr( "%s@%s", CollectorName, get_local_fqdn().Value() );
            }
    } else {
            id.formatstr( "%s", get_local_fqdn().Value() );
    }
    ad->Assign( ATTR_NAME, id.Value() );

    ad->Assign( ATTR_COLLECTOR_IP_ADDR, global_dc_sinful() );

    if ( interval > 0 ) {
            ad->Assign( ATTR_UPDATE_INTERVAL, 24*interval );
    }

		// Publish all DaemonCore-specific attributes, which also handles
		// COLLECTOR_ATTRS for us.
	daemonCore->publish(ad);
}

void
CollectorDaemon::forward_classad_to_view_collector(int cmd,
										   const char *filterAttr,
										   ClassAd *ad_to_forward)
{
	if (vc_list.empty()) return;

	if (!filterAttr) {
		send_classad_to_sock(cmd, ad_to_forward);
		return;
	}

	std::string type;
	if (!ad_to_forward->EvaluateAttrString(std::string(filterAttr), type)) {
		dprintf(D_ALWAYS, "Failed to lookup %s on ad, not forwarding\n", filterAttr);
		return;
	}

	if (viewCollectorTypes->contains_anycase(type.c_str())) {
		dprintf(D_ALWAYS, "Forwarding ad: type=%s command=%s\n",
				type.c_str(), getCommandString(cmd));
		send_classad_to_sock(cmd, ad_to_forward);
	}
}


void CollectorDaemon::send_classad_to_sock(int cmd, ClassAd* theAd) {
    if (vc_list.empty()) return;

    if (!theAd) {
        dprintf(D_ALWAYS, "Trying to forward ad on, but ad is NULL\n");
        return;
    }

    for (vector<vc_entry>::iterator e(vc_list.begin());  e != vc_list.end();  ++e) {
        Daemon* view_coll = e->collector;
        Sock* view_sock = e->sock;
        const char* view_name = e->name.c_str();

        bool raw_command = false;
        if (!view_sock->is_connected()) {
            // We must have gotten disconnected.  (Or this is the 1st time.)
            // In case we keep getting disconnected or fail to connect,
            // and each connection attempt takes a long time, restrict
            // what fraction of our time we spend trying to reconnect.
            if (view_sock_timeslice.isTimeToRun()) {
                dprintf(D_ALWAYS,"Connecting to CONDOR_VIEW_HOST %s\n", view_name);

                // Only run timeslice timer for TCP, since connect on UDP 
                // is instantaneous.
                if (view_sock->type() == Stream::reli_sock) {
	                view_sock_timeslice.setStartTimeNow();
                }
                view_coll->connectSock(view_sock,20);
                if (view_sock->type() == Stream::reli_sock) {
	                view_sock_timeslice.setFinishTimeNow();
                }

                if (!view_sock->is_connected()) {
                    dprintf(D_ALWAYS,"Failed to connect to CONDOR_VIEW_HOST %s so not forwarding ad.\n", view_name);
                    continue;
                }
            } else {
                dprintf(D_FULLDEBUG,"Skipping forwarding of ad to CONDOR_VIEW_HOST %s, because reconnect is delayed for %us.\n", view_name, view_sock_timeslice.getTimeToNextRun());
                continue;
            }
        } else if (view_sock->type() == Stream::reli_sock) {
            // we already did the security handshake the last time
            // we sent a command on this socket, so just send a
            // raw command this time to avoid reauthenticating
            raw_command = true;
        }

        // Run timeslice timer if raw_command is false, since this means 
        // startCommand() may need to initiate an authentication round trip 
        // and thus could block if the remote view collector is unresponsive.
        if ( raw_command == false ) {
	        view_sock_timeslice.setStartTimeNow();
        }
        bool start_command_result = 
			view_coll->startCommand(cmd, view_sock, 20, NULL, NULL, raw_command);
        if ( raw_command == false ) {
	        view_sock_timeslice.setFinishTimeNow();
        }

        if (! start_command_result ) {
            dprintf( D_ALWAYS, "Can't send command %d to View Collector %s\n", 
					cmd, view_name);
            view_sock->end_of_message();
            view_sock->close();
            continue;
        }

        if (theAd) {
            if (!putClassAd(view_sock, *theAd)) {
                dprintf( D_ALWAYS, "Can't forward classad to View Collector %s\n", view_name);
                view_sock->end_of_message();
                view_sock->close();
                continue;
            }
        }

        if (cmd == UPDATE_STARTD_AD) {
            // Forward the startd private ad as well.  This allows the
            // target collector to act as an aggregator for multiple collectors
            // that balance the load of authenticating connections from
            // the rest of the pool.
            AdNameHashKey hk;
            ClassAd *pvt_ad;
            ASSERT( makeStartdAdHashKey (hk, theAd) );
            pvt_ad = collector.lookup(STARTD_PVT_AD,hk);
            if (pvt_ad) {
                if (!putClassAd(view_sock, *pvt_ad)) {
                    dprintf( D_ALWAYS, "Can't forward startd private classad to View Collector %s\n", view_name);
                    view_sock->end_of_message();
                    view_sock->close();
                    continue;
                }
            }
        }

        if (!view_sock->end_of_message()) {
            dprintf(D_ALWAYS, "Can't send end_of_message to View Collector %s\n", view_name);
            view_sock->close();
            continue;
        }
    }
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

	// Given a full ad, and a StringList of expressions, compute
	// the list of all attributes those exressions depend on.

	// So, if the projection is passed in "foo", and foo is an expression
	// that expands to bar, we return bar
	
void
computeProjection(ClassAd *full_ad, SimpleList<MyString> *projectionList,StringList &expanded_projection) {
    projectionList->Rewind();

		// For each expression in the list...
	MyString attr;
	while (projectionList->Next(attr)) {
		StringList externals; // shouldn't have any

			// Get the indirect attributes
		if( !full_ad->GetExprReferences(attr.Value(), expanded_projection, externals) ) {
			dprintf(D_FULLDEBUG,
				"computeProjection failed to parse "
				"requested ClassAd expression: %s\n",attr.Value());
		}
	}
}
