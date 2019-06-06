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
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_email.h"

#include "condor_daemon_core.h"
#include "status_types.h"
#include "totals.h"

#include "condor_collector.h"
#include "collector_engine.h"
#include "hashkey.h"

#include "condor_adtypes.h"
#include "condor_universe.h"
#include "ipv6_hostname.h"
#include "condor_threads.h"

#include "collector.h"

#if defined(HAVE_DLOPEN) && !defined(DARWIN)
#include "CollectorPlugin.h"
#endif

#include "ccb_server.h"

#ifdef TRACK_QUERIES_BY_SUBSYS
#include "subsystem_info.h" // so we can track query by client subsys
#endif

using std::vector;
using std::string;

//----------------------------------------------------------------

extern "C" char* CondorVersion( void );
extern "C" char* CondorPlatform( void );

CollectorStats CollectorDaemon::collectorStats( false, 0 );
CollectorEngine CollectorDaemon::collector( &collectorStats );
int CollectorDaemon::HandleQueryInProcPolicy = HandleQueryInProcSmallTableAndQuery;
int CollectorDaemon::ClientTimeout;
int CollectorDaemon::QueryTimeout;
char* CollectorDaemon::CollectorName = NULL;
Timeslice CollectorDaemon::view_sock_timeslice;
vector<CollectorDaemon::vc_entry> CollectorDaemon::vc_list;

ClassAd* CollectorDaemon::__query__;
int CollectorDaemon::__numAds__;
int CollectorDaemon::__resultLimit__;
int CollectorDaemon::__failed__;
List<ClassAd>* CollectorDaemon::__ClassAdResultList__;
std::string CollectorDaemon::__adType__;
ExprTree *CollectorDaemon::__filter__;

TrackTotals* CollectorDaemon::normalTotals = NULL;
int CollectorDaemon::submittorRunningJobs;
int CollectorDaemon::submittorIdleJobs;
int CollectorDaemon::submittorNumAds;

CollectorUniverseStats CollectorDaemon::ustatsAccum;
CollectorUniverseStats CollectorDaemon::ustatsMonthly;

int CollectorDaemon::machinesTotal;
int CollectorDaemon::machinesUnclaimed;
int CollectorDaemon::machinesClaimed;
int CollectorDaemon::machinesOwner;
int CollectorDaemon::startdNumAds;

ClassAd* CollectorDaemon::ad = NULL;
CollectorList* CollectorDaemon::collectorsToUpdate = NULL;
DCCollector* CollectorDaemon::worldCollector = NULL;
int CollectorDaemon::UpdateTimerId;

OfflineCollectorPlugin CollectorDaemon::offline_plugin_;

StringList *viewCollectorTypes;

CCBServer *CollectorDaemon::m_ccb_server;
bool CollectorDaemon::filterAbsentAds;

Queue<CollectorDaemon::pending_query_entry_t *> CollectorDaemon::query_queue_high_prio;
Queue<CollectorDaemon::pending_query_entry_t *> CollectorDaemon::query_queue_low_prio;
int CollectorDaemon::ReaperId = -1;
int CollectorDaemon::max_query_workers = 4;
int CollectorDaemon::reserved_for_highprio_query_workers = 1;
int CollectorDaemon::max_pending_query_workers = 50;
int CollectorDaemon::max_query_worktime = 0;
int CollectorDaemon::active_query_workers = 0;
int CollectorDaemon::pending_query_workers = 0;

#ifdef TRACK_QUERIES_BY_SUBSYS
bool CollectorDaemon::want_track_queries_by_subsys = false;
#endif

//---------------------------------------------------------



// prototypes of library functions
typedef void (*SIGNAL_HANDLER)();
extern "C"
{
	void schedule_event ( int month, int day, int hour, int minute, int second, SIGNAL_HANDLER );
}
 
//----------------------------------------------------------------

void CollectorDaemon::Init()
{
	dprintf(D_ALWAYS, "In CollectorDaemon::Init()\n");

	// read in various parameters from condor_config
	CollectorName=NULL;
	ad=NULL;
	viewCollectorTypes = NULL;
	UpdateTimerId=-1;
	collectorsToUpdate = NULL;
	worldCollector = NULL;
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
	if(param_boolean("PROTECT_COLLECTOR_ADS", false)) {
		daemonCore->Register_CommandWithPayload(QUERY_COLLECTOR_ADS,"QUERY_COLLECTOR_ADS",
			(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,ADMINISTRATOR);
	} else {
		daemonCore->Register_CommandWithPayload(QUERY_COLLECTOR_ADS,"QUERY_COLLECTOR_ADS",
			(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	}
	daemonCore->Register_CommandWithPayload(QUERY_STORAGE_ADS,"QUERY_STORAGE_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_ACCOUNTING_ADS,"QUERY_ACCOUNTING_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_NEGOTIATOR_ADS,"QUERY_NEGOTIATOR_ADS",
		(CommandHandler)receive_query_cedar,"receive_query_cedar",NULL,READ);
	daemonCore->Register_CommandWithPayload(QUERY_HAD_ADS,"QUERY_HAD_ADS",
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
	daemonCore->Register_CommandWithPayload(INVALIDATE_ACCOUNTING_ADS,
		"INVALIDATE_ACCOUNTING_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(INVALIDATE_NEGOTIATOR_ADS,
		"INVALIDATE_NEGOTIATOR_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,NEGOTIATOR);
	daemonCore->Register_CommandWithPayload(INVALIDATE_HAD_ADS,
		"INVALIDATE_HAD_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(INVALIDATE_ADS_GENERIC,
		"INVALIDATE_ADS_GENERIC", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);
    daemonCore->Register_CommandWithPayload(INVALIDATE_GRID_ADS,
        "INVALIDATE_GRID_ADS", (CommandHandler)receive_invalidation,
		"receive_invalidation",NULL,DAEMON);

	// install command handlers for updates
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
	daemonCore->Register_CommandWithPayload(UPDATE_AD_GENERIC, "UPDATE_AD_GENERIC",
		(CommandHandler)receive_update,"receive_update", NULL, DAEMON);
    daemonCore->Register_CommandWithPayload(UPDATE_GRID_AD,"UPDATE_GRID_AD",
		(CommandHandler)receive_update,"receive_update",NULL,DAEMON);
	daemonCore->Register_CommandWithPayload(UPDATE_ACCOUNTING_AD,"UPDATE_ACCOUNTING_AD",
		(CommandHandler)receive_update,"receive_update",NULL,NEGOTIATOR);

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

	// add an exponential moving average counter of updates received.
	daemonCore->dc_stats.NewProbe("Collector", "UpdatesReceived", AS_COUNT | IS_CLS_SUM_EMA_RATE | IF_BASICPUB);

	// add a reaper for our query threads spawned off via Create_Thread
	if ( ReaperId == -1 ) {
		ReaperId = daemonCore->Register_Reaper("CollectorDaemon::QueryReaper",
						(ReaperHandler)&CollectorDaemon::QueryReaper,
						"CollectorDaemon::QueryReaper()",NULL);
	}	
}

collector_runtime_probe HandleQuery_runtime;
collector_runtime_probe HandleLocate_runtime;
collector_runtime_probe HandleQueryForked_runtime;
collector_runtime_probe HandleQueryMissedFork_runtime;
collector_runtime_probe HandleLocateForked_runtime;
collector_runtime_probe HandleLocateMissedFork_runtime;


template <typename T>
class _condor_variable_auto_accum_runtime : public _condor_runtime
{
public:
	_condor_variable_auto_accum_runtime(T * store) : runtime(store) { }; // remember where to save result
	~_condor_variable_auto_accum_runtime() { (*runtime) += elapsed_runtime(); };
	T * runtime;
};


int CollectorDaemon::receive_query_cedar(Service* /*s*/,
										 int command,
										 Stream* sock)
{
	int return_status = TRUE;
	pending_query_entry_t *query_entry = NULL;
	bool handle_in_proc;
	bool is_locate;
	KnownSubsystemId clientSubsys = SUBSYSTEM_ID_UNKNOWN;
	AdTypes whichAds;
	ClassAd *cad = new ClassAd();
	ASSERT(cad);

	_condor_variable_auto_accum_runtime<collector_runtime_probe> rt(&HandleQuery_runtime);
	//double rt_last = rt.begin;

	sock->decode();

		// Avoid lengthy blocking on communication with our peer.
		// This command-handler should not get called until data
		// is ready to read.
	sock->timeout(1);

	bool ep = CondorThreads::enable_parallel(true);
	bool res = !getClassAd(sock, *cad) || !sock->end_of_message();
	CondorThreads::enable_parallel(ep);
    if( res )
    {
        dprintf(D_ALWAYS,"Failed to receive query on TCP: aborting\n");
		return_status = FALSE;
		goto END;
    }

	// Initial query handler
	whichAds = receive_query_public( command );

	is_locate = cad->Lookup(ATTR_LOCATION_QUERY) != NULL;
	if (is_locate) { rt.runtime = &HandleLocate_runtime; }

	// Figure out whether to handle the query inline or to fork.
	handle_in_proc = false;
	if (HandleQueryInProcPolicy == HandleQueryInProcAlways) {
		handle_in_proc = true;
	} else if (HandleQueryInProcPolicy == HandleQueryInProcNever) {
		handle_in_proc = false;
	} else {
		bool is_bigtable = ((whichAds == GENERIC_AD) || (whichAds == ANY_AD) || (whichAds == STARTD_PVT_AD) || (whichAds == STARTD_AD) || (whichAds == MASTER_AD));
		if (HandleQueryInProcPolicy == HandleQueryInProcSmallTable) {
			handle_in_proc = !is_bigtable;
		} else {
			bool small_query = is_locate;
			if ( ! is_locate) {
				long long result_limit = 0;
				bool has_limit = cad->EvaluateAttrInt(ATTR_LIMIT_RESULTS, result_limit);
				bool has_projection = cad->Lookup(ATTR_PROJECTION);
				small_query = has_projection && has_limit && (result_limit < 10);
			}
			switch (HandleQueryInProcPolicy) {
				case HandleQueryInProcSmallQuery: handle_in_proc = small_query; break;
				case HandleQueryInProcSmallTableAndQuery: handle_in_proc = !is_bigtable && small_query; break;
				case HandleQueryInProcSmallTableOrQuery: handle_in_proc = !is_bigtable || small_query; break;
			}
		}
	}
	// If we are not allowed any forked query workers, i guess we are going in-proc
	if ( max_query_workers < 1 ) {
		handle_in_proc = true;
	}

	// Set a deadline on the query socket if the admin specified one in the config,
	// but if the socket came to us with a previous (shorter) deadline, honor it.
	if ( max_query_worktime > 0 ) {   // max_query_worktime came from config file
		time_t new_deadline = time(NULL) + max_query_worktime;
		time_t sock_deadline = sock->get_deadline();
		if ( sock_deadline > 0 ) {
			new_deadline = MIN(new_deadline,sock_deadline);
		}
		sock->set_deadline(new_deadline);
		// dprintf(D_FULLDEBUG,"QueryWorker old sock_deadline = %d, now new_deadline = %d\n",sock_deadline,new_deadline);
	}

	// malloc a query_entry struct.  we must use malloc here, not new, since
	// DaemonCore::Create_Thread requires a buffer created with malloc(), as it 
	// will insist on calling free().  Sigh.
	query_entry = (pending_query_entry_t *) malloc( sizeof(pending_query_entry_t) );
	ASSERT(query_entry);
	query_entry->cad = cad;
	query_entry->is_locate = is_locate;
	query_entry->subsys[0] = 0;
	query_entry->sock = sock;
	query_entry->whichAds = whichAds;

#ifdef TRACK_QUERIES_BY_SUBSYS
	if ( want_track_queries_by_subsys ) {
		std::string subsys;
		const std::string &sess_id = static_cast<Sock *>(sock)->getSessionID();
		daemonCore->getSecMan()->getSessionStringAttribute(sess_id.c_str(),ATTR_SEC_SUBSYSTEM,subsys);
		if ( ! subsys.empty()) {
			clientSubsys = getKnownSubsysNum(subsys.c_str());
			strncpy(query_entry->subsys, subsys.c_str(), COUNTOF(query_entry->subsys));
			query_entry->subsys[COUNTOF(query_entry->subsys)-1] = 0;
		}
	}
#endif

	// Now we are ready to either invoke a worker thread directly to handle the query,
	// or enqueue a request to run the worker thread later.
	if ( handle_in_proc ) {
		// We want to immediately handle the query inline in this process.
		// So in this case, we simply directly invoke our worker thread function.
		dprintf(D_FULLDEBUG,"QueryWorker: about to handle query in-process\n");
		return_status = receive_query_cedar_worker_thread((void *)query_entry,sock);
	} else {
		// Enqueue the query to ultimately run in a forked process created created with
		// DaemonCore::Create_Thread().  
		// We add the pending query entry to a queue, and then directly invoke the worker 
		// thread reaper, as the reaper handles popping entries off the queue and invoking
		// Create_Thread once where are enough worker slots.
		// Note in this case, we must return KEEP_STEAM so the socket to our client is not
		// closed before we handle the request; it will be closed by the reaper ( in parent process)
		// and/or by daemoncore (in child process).  Either way, don't close it upon exit from this
		// command handler.
		int did_we_fork = FALSE;

		// We want to add a query request into the queue.
		// Decide if it should go into the high priority or low priorirty queue
		// based upon the Subsystem attribute in the session for this connection;
		// if the request is from the NEGOTIATOR, it is high priority.
		// Also high priority if command is from the superuser (i.e. via condor_sos).
		bool high_prio_query = false;
		if ( clientSubsys == SUBSYSTEM_ID_UNKNOWN ) {
			// If we have not yet determined the subsystem, we must do that now.
			std::string subsys;
			const std::string &sess_id = static_cast<Sock *>(sock)->getSessionID();
			daemonCore->getSecMan()->getSessionStringAttribute(sess_id.c_str(),ATTR_SEC_SUBSYSTEM,subsys);
			if ( ! subsys.empty()) {
				clientSubsys = getKnownSubsysNum(subsys.c_str());
				strncpy(query_entry->subsys, subsys.c_str(), COUNTOF(query_entry->subsys));
				query_entry->subsys[COUNTOF(query_entry->subsys)-1] = 0;
			}
		}
		if ( clientSubsys == SUBSYSTEM_ID_NEGOTIATOR || daemonCore->Is_Command_From_SuperUser(sock) )
		{
			high_prio_query = true;
		}

		// Now that we know if the incoming query is high priority or not,
		// place it into the proper queue if we don't already have too many pending.
		if ( ((high_prio_query==false) &&
			  (active_query_workers + pending_query_workers <  max_query_workers + max_pending_query_workers - reserved_for_highprio_query_workers))
			 ||
			 ((high_prio_query==true) &&
			  (active_query_workers - reserved_for_highprio_query_workers + query_queue_high_prio.Length() <  max_query_workers + max_pending_query_workers))
		   )
		{
			if ( high_prio_query ) {
				query_queue_high_prio.enqueue( query_entry );
			} else {
				query_queue_low_prio.enqueue( query_entry );
			}
			did_we_fork = QueryReaper(NULL, -1, -1);
			cad = NULL; // set this to NULL so we won't delete it below; our reaper will remove it
			query_entry = NULL; // set this to NULL so we won't free it below; daemoncore will remove it
			return_status = KEEP_STREAM; // tell daemoncore to not mess with socket when we return
		} else {
			dprintf( D_ALWAYS, 
				"QueryWorker: dropping %s priority query request due to max pending workers of %d ( max %d reserved %d active %d pending %d )\n", 
				high_prio_query ? "high" : "low",
				max_pending_query_workers, max_query_workers, reserved_for_highprio_query_workers, active_query_workers, pending_query_workers );
			collectorStats.global.DroppedQueries += 1;
		}

		// Update a few statistics
		if ( !daemonCore->DoFakeCreateThread() ) {  // if we are configured to really fork()...
			if (did_we_fork == TRUE) {
				// A new worker was forked off
				if (is_locate) { rt.runtime = &HandleLocateForked_runtime; } else { rt.runtime = &HandleQueryForked_runtime; }
			} else {
				// Did not yet fork off a new worker cuz we are at a limit
				if (is_locate) { rt.runtime = &HandleLocateMissedFork_runtime; } else { rt.runtime = &HandleQueryMissedFork_runtime; }
			}
		}
	}

#ifdef TRACK_QUERIES_BY_SUBSYS
	if ( want_track_queries_by_subsys ) {
		// count the number of queries for each client subsystem.
		if (clientSubsys >= 0 && clientSubsys < SUBSYSTEM_ID_COUNT) {
			if (handle_in_proc) {
				collectorStats.global.InProcQueriesFrom[clientSubsys] += 1;
			} else {
				collectorStats.global.ForkQueriesFrom[clientSubsys] += 1;
			}
		}
	}
#endif

END:
    // all done
	delete cad;
	free(query_entry);
	return return_status;
}


// Return 1 if forked a worker, 0 if not, and -1 upon an error.
int CollectorDaemon::QueryReaper(Service *, int pid, int /* exit_status */ )
{
	if ( pid >= 0 ) {
		dprintf(D_FULLDEBUG,
			"QueryWorker: Child %d done\n", pid);
		if (active_query_workers > 0 ) {
			active_query_workers--;
		}
		collectorStats.global.ActiveQueryWorkers = active_query_workers;
	}

	// Grab a queue_entry to service, ignoring "stale" (old) entries.
	bool high_prio_query;
	pending_query_entry_t * query_entry = NULL;	
	while ( query_entry == NULL ) {
		// Pull of an entry from our high_prio queue; if nothing there, grab
		// one from our low prio queue.  Ignore "stale" (old) requests.

		high_prio_query = query_queue_high_prio.Length() > 0;

		// Dequeue a high priority entry if worker slots available.
		if ( active_query_workers < max_query_workers ) {
			query_queue_high_prio.dequeue(query_entry);
			// If high priority queue is empty, dequeue a low priority entry
			// if a worker slot (minus those reserved only for high prioirty) is available.
			if ((query_entry == NULL) &&
			    (active_query_workers < (max_query_workers - reserved_for_highprio_query_workers)))
			{
				query_queue_low_prio.dequeue(query_entry);
			}
		}

		// Update our pending stats counters.  Note we need to do this regardless
		// of if query_entry==NULL, since we may be here because something was either
		// recently added into the queue, or recently removed from the queue.
		pending_query_workers = query_queue_high_prio.Length() + query_queue_low_prio.Length();
		collectorStats.global.PendingQueries = pending_query_workers;

		// If query_entry==NULL, we are not forking anything now, so we're done for now
		if ( query_entry == NULL ) {
			// If we are not forking because we hit fork limits, dprintf
			if ( pending_query_workers > 0  ) {
				dprintf( D_ALWAYS,
					"QueryWorker: delay forking %s priority query because too many workers ( max %d reserved %d active %d pending %d ) \n",
					high_prio_query ? "high" : "low",
					max_query_workers, reserved_for_highprio_query_workers, active_query_workers, pending_query_workers );
			}
			return 0;
		}

		// If we are here because a fork worker just exited (pid >= 0), or
		// if there are still more pending queries in the queue, then it is possible
		// that the query_entry we are about to service has been sitting around
		// in the queue for some time.  So we need to check if it is "stale"
		// before we spend time forking.
		if ( pid >= 0 || pending_query_workers > 0 ) {
			// Consider a query_request to be stale if
			//   a) our deadline on the socket has expired, or
			//   b) the client has closed the TCP socket.
			// Note that we can figure out (b) by asking CEDAR if
			// the socket is "readReady()", which will return true if
			// either the socket is closed or the socket has data waiting to be read.
			// Since we know there should be nothing more to read on this socket (the
			// client should now be awaiting our reponse), if readReady() return true
			// we can safely assume that TCP thinks the socket has been closed.
			if ( query_entry->sock->deadline_expired() ||
				static_cast<Sock *>(query_entry->sock)->readReady() )
			{
				dprintf( D_ALWAYS, 
					"QueryWorker: dropping stale query request because %s ( max %d active %d pending %d )\n", 
					query_entry->sock->deadline_expired() ? "max worktime expired" : "client gone",
					max_query_workers, active_query_workers, pending_query_workers );
				collectorStats.global.DroppedQueries += 1;				
				// now deallocate everything with this query_entry
				delete query_entry->sock;
				delete query_entry->cad;
				free(query_entry); 
				query_entry = NULL;  // so we will loop and dequeue another entry
			}
		}
	}  // end of while queue_entry == NULL

	// If we have made it here, we are allowed to fork another worker
	// to handle the query represented by query_entry. Fork one!
	// First stash a copy of query_entry->sock and query_entry->cad so 
	// we can deallocate the memory associated with these after a succesfull 
	// call to Create_Thread - we need to stash them away because DaemonCore will
	// free the query_entry struct itself.
	Stream *sock = query_entry->sock;
	query_entry->sock = NULL;
	ClassAd *query_classad = query_entry->cad;
	int tid = daemonCore->
		Create_Thread((ThreadStartFunc)&CollectorDaemon::receive_query_cedar_worker_thread,
		    (void *)query_entry, sock, ReaperId);
	if (tid == FALSE) {
		dprintf(D_ALWAYS,
				"ERROR: Create_Thread failed trying to fork a QueryWorker!\n");
		free(query_entry); // daemoncore won't free this if Create_Thread fails
		delete sock;
		delete query_classad;
		return -1;
	}

	// If we made it here, we forked off another worker. 

	// Increment our count of active workers
	active_query_workers++;
	collectorStats.global.ActiveQueryWorkers = active_query_workers;

	// Also close query_entry->sock since DaemonCore
	// will have cloned this socket for the child, and we have no need to write anything
	// out here in the parent once the child finishes.
	delete sock;
	sock = NULL;

	// Also deallocate the memory associated with the stashed in query_classad.
	// It is safe to do this now, because if Create_Thread already forked the child
	// now has a copy, or the Create_Thread ran in-proc it has already completed.
	delete query_classad;
	query_classad = NULL;

	dprintf(D_ALWAYS,
			"QueryWorker: forked new %sworker with id %d ( max %d active %d pending %d )\n",
			high_prio_query ? "high priority " : "", tid,
			max_query_workers, active_query_workers, pending_query_workers);

	return 1;
}


int CollectorDaemon::receive_query_cedar_worker_thread(void *in_query_entry, Stream* sock)
{
	int return_status = TRUE;
	double begin = condor_gettimestamp_double();
	List<ClassAd> results;

		// If our peer is at least 8.9.3 and has NEGOTIATOR authz, then we'll
		// trust it to handle our capabilities.
	bool filter_private_ads = true;
	auto *verinfo = sock->get_peer_version();
	if (verinfo && verinfo->built_since_version(8, 9, 3)) {
		auto addr = static_cast<ReliSock*>(sock)->peer_addr();
		if (USER_AUTH_SUCCESS == daemonCore->Verify("send private ads", NEGOTIATOR, addr, static_cast<ReliSock*>(sock)->getFullyQualifiedUser())) {
			filter_private_ads = false;
		}
	}

	// Pull out relavent state from query_entry
	pending_query_entry_t *query_entry = (pending_query_entry_t *) in_query_entry;
	ClassAd *cad = query_entry->cad;
	bool is_locate = query_entry->is_locate;
	AdTypes whichAds = query_entry->whichAds;

		// Always send private attributes in private ads.
	if (whichAds == STARTD_PVT_AD) {
		filter_private_ads = false;
	}

	// Perform the query

	if (whichAds != (AdTypes) -1) {
		process_query_public (whichAds, cad, &results);
	}

	double end_query = condor_gettimestamp_double();
	double end_write = 0.0;

	// send the results via cedar			
	sock->timeout(QueryTimeout); // set up a network timeout of a longer duration
	sock->encode();
	results.Rewind();
	ClassAd *curr_ad = NULL;
	int more = 1;
	
		// See if query ad asks for server-side projection
	string projection = "";
		// turn projection string into a set of attributes
	classad::References proj;
	bool evaluate_projection = false;
	if (cad->LookupString(ATTR_PROJECTION, projection) && ! projection.empty()) {
		StringTokenIterator list(projection);
		const std::string * attr;
		while ((attr = list.next_string())) { proj.insert(*attr); }
	} else if (cad->Lookup(ATTR_PROJECTION)) {
		// if projection is not a simple string, then assume that evaluating it as a string in the context of the ad will work better
		// (the negotiator sends this sort of projection)
		evaluate_projection = true;
	}

	while ( (curr_ad=results.Next()) )
	{
		// if querying collector ads, and the collectors own ad appears in this list.
		// then we want to shove in current statistics. we do this by chaining a
		// temporary stats ad into the ad to be returned, and publishing updated
		// statistics into the stats ad.  we do this because if the verbosity level
		// is increased we do NOT want to put the high-verbosity attributes into
		// our persistent collector ad.
		ClassAd * stats_ad = NULL;
		if ((whichAds == COLLECTOR_AD) && collector.isSelfAd(curr_ad)) {
			dprintf(D_ALWAYS,"Query includes collector's self ad\n");
			// update stats in the collector ad before we return it.
			std::string stats_config;
			cad->LookupString("STATISTICS_TO_PUBLISH",stats_config);
			if (stats_config != "stored") {
				dprintf(D_ALWAYS,"Updating collector stats using a chained ad and config=%s\n", stats_config.c_str());
				stats_ad = new ClassAd();
				daemonCore->dc_stats.Publish(*stats_ad, stats_config.c_str());
				daemonCore->monitor_data.ExportData(stats_ad, true);
				collectorStats.publishGlobal(stats_ad, stats_config.c_str());
				stats_ad->ChainToAd(curr_ad);
				curr_ad = stats_ad; // send the stats ad instead of the self ad.
			}
		}

		if (evaluate_projection) {
			proj.clear();
			projection.clear();
			if (EvalString(ATTR_PROJECTION, cad, curr_ad, projection) && ! projection.empty()) {
				StringTokenIterator list(projection);
				const std::string * attr;
				while ((attr = list.next_string())) { proj.insert(*attr); }
			}
		}

		bool send_failed = (!sock->code(more) || !putClassAd(sock, *curr_ad, filter_private_ads ? PUT_CLASSAD_NO_PRIVATE : 0, proj.empty() ? NULL : &proj));
        
		if (stats_ad) {
			stats_ad->Unchain();
			delete stats_ad;
		}

		if (send_failed)
        {
            dprintf (D_ALWAYS,
                    "Error sending query result to client -- aborting\n");
            return_status = 0;
			goto END;
        }

		if (sock->deadline_expired()) {
			dprintf( D_ALWAYS,
				"QueryWorker: max_worktime expired while sending query result to client -- aborting\n");
			return_status = 0;
			goto END;
		}

	} // end of while loop for next result ad to send

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

	end_write = condor_gettimestamp_double();

	dprintf (D_ALWAYS,
			 "Query info: matched=%d; skipped=%d; query_time=%f; send_time=%f; type=%s; requirements={%s}; locate=%d; limit=%d; from=%s; peer=%s; projection={%s}; filter_private_ads=%d\n",
			 __numAds__,
			 __failed__,
			 end_query - begin,
			 end_write - end_query,
			 AdTypeToString(whichAds),
			 ExprTreeToString(__filter__),
			 is_locate,
			 (__resultLimit__ == INT_MAX) ? 0 : __resultLimit__,
			 query_entry->subsys,
			 sock->peer_description(),
			 projection.c_str(),
			 filter_private_ads);
END:
	
	// All done.  Deallocate memory allocated in this method.  Note that DaemonCore 
	// will supposedly free() the query_entry struct itself and also delete sock.

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

	  case QUERY_ACCOUNTING_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_ACCOUNTING_ADS\n");
		whichAds = ACCOUNTING_AD;
		break;

	  case QUERY_NEGOTIATOR_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_NEGOTIATOR_ADS\n");
		whichAds = NEGOTIATOR_AD;
		break;

	  case QUERY_HAD_ADS:
		dprintf (D_FULLDEBUG,"Got QUERY_HAD_ADS\n");
		whichAds = HAD_AD;
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

	  case INVALIDATE_ACCOUNTING_ADS:
		dprintf (D_ALWAYS, "Got INVALIDATE_ACCOUNTING_ADS\n");
		whichAds = ACCOUNTING_AD;
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

#if defined(HAVE_DLOPEN) && !defined(DARWIN)
	CollectorPluginManager::Invalidate(command, cad);
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


collector_runtime_probe CollectorEngine_receive_update_runtime;
collector_runtime_probe CollectorEngine_ru_pre_collect_runtime;
collector_runtime_probe CollectorEngine_ru_collect_runtime;
collector_runtime_probe CollectorEngine_ru_plugins_runtime;
collector_runtime_probe CollectorEngine_ru_forward_runtime;
collector_runtime_probe CollectorEngine_ru_stash_socket_runtime;

int CollectorDaemon::receive_update(Service* /*s*/, int command, Stream* sock)
{
    int	insert;
	ClassAd *cad;
	_condor_auto_accum_runtime<collector_runtime_probe> rt(CollectorEngine_receive_update_runtime);
	double rt_last = rt.begin;

	daemonCore->dc_stats.AddToAnyProbe("UpdatesReceived", 1);

	/* assume the ad is malformed... other functions set this value */
	insert = -3;

	// get endpoint
	condor_sockaddr from = ((Sock*)sock)->peer_addr();

	CollectorEngine_ru_pre_collect_runtime += rt.tick(rt_last);
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
	CollectorEngine_ru_collect_runtime += rt.tick(rt_last);

	/* let the off-line plug-in have at it */
	offline_plugin_.update ( command, *cad );

#if defined(HAVE_DLOPEN) && !defined(DARWIN)
	CollectorPluginManager::Update(command, *cad);
#endif

	CollectorEngine_ru_plugins_runtime += rt.tick(rt_last);

	if (viewCollectorTypes) {
		forward_classad_to_view_collector(command,
										  ATTR_MY_TYPE,
										  cad);
	} else if ((command == UPDATE_STARTD_AD) || (command == UPDATE_SUBMITTOR_AD)) {
        send_classad_to_sock(command, cad);
	}

	CollectorEngine_ru_forward_runtime += rt.tick(rt_last);

	if( sock->type() == Stream::reli_sock ) {
			// stash this socket for future updates...
		int rv = stashSocket( (ReliSock *)sock );
		CollectorEngine_ru_stash_socket_runtime += rt.tick(rt_last);
		return rv;
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

#if defined(HAVE_DLOPEN) && !defined(DARWIN)
    CollectorPluginManager::Update ( command, *cad );
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
		if (__numAds__ >= __resultLimit__) {
			return 0; // tell it to stop iterating, we have all the results we want
		}
    } else {
		__failed__++;
	}

    return 1;
}


void CollectorDaemon::process_query_public (AdTypes whichAds,
											ClassAd *query,
											List<ClassAd>* results)
{
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

	__resultLimit__ = INT_MAX; // no limit
	if ( ! query->LookupInteger(ATTR_LIMIT_RESULTS, __resultLimit__) || __resultLimit__ <= 0) {
		__resultLimit__ = INT_MAX; // no limit
	}

	// See if we should exclude Collector Ads from generic queries.  Still
	// give them out for specific collector queries, which is registered as
	// ADMINISTRATOR when PROTECT_COLLECTOR_ADS is true.  This setting is
	// designed only for use at the UW, and as such this knob is not present
	// in the param table.
	if ((whichAds != COLLECTOR_AD) && param_boolean("PROTECT_COLLECTOR_ADS", false)) {
		dprintf(D_FULLDEBUG, "Received query with generic type; filtering collector ads\n");
		MyString modified_filter;
		modified_filter.formatstr("(%s) && (MyType =!= \"Collector\")",
			ExprTreeToString(__filter__));
		query->AssignExpr(ATTR_REQUIREMENTS,modified_filter.Value());
		__filter__ = query->LookupExpr(ATTR_REQUIREMENTS);
		if ( __filter__ == NULL ) {
			dprintf (D_ALWAYS, "Failed to parse modified filter: %s\n", 
				modified_filter.Value());
			return;
		}
		dprintf(D_FULLDEBUG,"Query after modification: *%s*\n",modified_filter.Value());
	}

	// If ABSENT_REQUIREMENTS is defined, rewrite filter to filter-out absent ads 
	// if ATTR_ABSENT is not alrady referenced in the query.
	if ( filterAbsentAds ) {	// filterAbsentAds is true if ABSENT_REQUIREMENTS defined
		classad::References machine_refs;  // machine attrs referenced by requirements
		bool checks_absent = false;

		GetReferences(ATTR_REQUIREMENTS,*query,NULL,&machine_refs);
		checks_absent = machine_refs.count( ATTR_ABSENT );
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
	++submittorNumAds;

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

	++startdNumAds;

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
    startdNumAds = 0;
	ustatsAccum.Reset( );

    if (!collector.walkHashTable (STARTD_AD, reportMiniStartdScanFunc)) {
            dprintf (D_ALWAYS, "Error counting machines in devel report \n");
    }

	// If we don't have any machines reporting to us, bail out early
	if (machinesTotal == 0) return;

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

	normalTotals = NULL;

	// output totals summary to the mailer
	totals.displayTotals( mailer, 20 );

	// now output information about submitted jobs
	submittorRunningJobs = 0;
	submittorIdleJobs = 0;
	submittorNumAds = 0;
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

	HandleQueryInProcPolicy = HandleQueryInProcSmallTableOrQuery;
	auto_free_ptr policy(param("HANDLE_QUERY_IN_PROC_POLICY"));
	if (policy) {
		bool boolval;
		if (string_is_boolean_param(policy, boolval)) {
			if( boolval ) {
				HandleQueryInProcPolicy = HandleQueryInProcAlways;
			} else {
				HandleQueryInProcPolicy = HandleQueryInProcNever;
			}
		} else if (YourStringNoCase("always") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcAlways;
		} else if (YourStringNoCase("never") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcNever;
		} else if (YourStringNoCase("small_table") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallTable;
		} else if (YourStringNoCase("small_query") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallQuery;
		} else if (YourStringNoCase("small_table_or_query") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallTableOrQuery;
		} else if (YourStringNoCase("small_table_and_query") == policy) {
			HandleQueryInProcPolicy = HandleQueryInProcSmallTableAndQuery;
		} else {
			dprintf(D_ALWAYS, "Unknown value for HANDLE_QUERY_IN_PROC_POLICY, using default of SMALL_TABLE_OR_QUERY\n");
		}
	}

	// handle params for Collector updates
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}

	DCCollectorAdSequences * adSeq = NULL;
	if( collectorsToUpdate ) {
		adSeq = collectorsToUpdate->detachAdSequences();
		delete collectorsToUpdate;
		collectorsToUpdate = NULL;
	}
	collectorsToUpdate = CollectorList::create(NULL, adSeq);

	//
	// If we don't use the network to update ourselves, we could allow
	// [UPDATE|INVALIDATE]_COLLECTOR_AD[S] to use TCP (and therefore
	// shared port).  This would also bypass the need for us to have a
	// UDP command socket (which we currently won't, if we're using
	// shared port without an assigned port number).
	//

	DCCollector * daemon = NULL;
	collectorsToUpdate->rewind();
	const char * myself = daemonCore->InfoCommandSinfulString();
	if( myself == NULL ) {
		EXCEPT( "Unable to determine my own address, aborting rather than hang.  You may need to make sure the shared port daemon is running first." );
	}
	Sinful mySinful( myself );
	Sinful mySharedPortDaemonSinful = mySinful;
	mySharedPortDaemonSinful.setSharedPortID( NULL );
	while( collectorsToUpdate->next( daemon ) ) {
		const char * current = daemon->addr();
		if( current == NULL ) { continue; }

		Sinful currentSinful( current );
		if( mySinful.addressPointsToMe( currentSinful ) ) {
			collectorsToUpdate->deleteCurrent();
			continue;
		}

		// addressPointsToMe() doesn't know that the shared port daemon
		// forwards connections that otherwise don't ask to be forwarded
		// to the collector.  This means that COLLECTOR_HOST doesn't need
		// to include ?sock=collector, but also that mySinful has a
		// shared port address and currentSinful may not.  Since we know
		// that we're trying to contact the collector here -- that is, we
		// can tell we're not contacting the shared port daemon in the
		// process of doing something else -- we can safely assume that
		// any currentSinful without a shared port ID intends to connect
		// to the default collector.
		dprintf( D_FULLDEBUG, "checking for self: '%s', '%s, '%s'\n", mySinful.getSharedPortID(), mySharedPortDaemonSinful.getSinful(), currentSinful.getSinful() );
		if( mySinful.getSharedPortID() != NULL && mySharedPortDaemonSinful.addressPointsToMe( currentSinful ) ) {
			// Check to see if I'm the default collector.
			std::string collectorSPID;
			param( collectorSPID, "SHARED_PORT_DEFAULT_ID" );
			if(! collectorSPID.size()) { collectorSPID = "collector"; }
			if( strcmp( mySinful.getSharedPortID(), collectorSPID.c_str() ) == 0 ) {
				dprintf( D_FULLDEBUG, "Skipping sending update to myself via my shared port daemon.\n" );
				collectorsToUpdate->deleteCurrent();
			}
		}
	}

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

	if( worldCollector ) {
		// FIXME: WTF does this mean w/r/t using TCP for collectorsToUpdate?
		// we should just delete it.  since we never use TCP
		// for these updates, we don't really loose anything
		// by destroying the object and recreating it...
		delete worldCollector;
		worldCollector = NULL;
	}
	if ( tmp ) {
		worldCollector = new DCCollector( tmp, DCCollector::UDP );
	}

	free( tmp );
	
	int i = param_integer("COLLECTOR_UPDATE_INTERVAL",900); // default 15 min
	if( UpdateTimerId < 0 ) {
		UpdateTimerId = daemonCore->
			Register_Timer( 1, i, sendCollectorAd,
							"sendCollectorAd" );
	}

	collector.m_allowOnlyOneNegotiator = param_boolean("COLLECTOR_ALLOW_ONLY_ONE_NEGOTIATOR", false);
	// This it temporary (for 8.7.0) just in case we need to turn off the new getClassAdEx options
	collector.m_get_ad_options = param_integer("COLLECTOR_GETAD_OPTIONS", GET_CLASSAD_FAST | GET_CLASSAD_LAZY_PARSE);
	collector.m_get_ad_options &= (GET_CLASSAD_LAZY_PARSE | GET_CLASSAD_FAST | GET_CLASSAD_NO_CACHE);
	MyString opts;
	if (collector.m_get_ad_options & GET_CLASSAD_FAST) { opts += "fast "; }
	if (collector.m_get_ad_options & GET_CLASSAD_NO_CACHE) { opts += "no-cache "; }
	else if (collector.m_get_ad_options & GET_CLASSAD_LAZY_PARSE) { opts += "lazy-parse "; }
	if (opts.empty()) { opts = "none "; }
	dprintf(D_ALWAYS, "COLLECTOR_GETAD_OPTIONS set to %s(0x%x)\n", opts.c_str(), collector.m_get_ad_options);

	tmp = param(COLLECTOR_REQUIREMENTS);
	MyString collector_req_err;
	if( !collector.setCollectorRequirements( tmp, collector_req_err ) ) {
		EXCEPT("Handling of '%s=%s' failed: %s",
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
            DCCollector* vhd = new DCCollector(vhost, DCCollector::CONFIG_VIEW);
            Sinful view_addr( vhd->addr() );
            Sinful my_addr( daemonCore->publicNetworkIpAddr() );

            if (my_addr.addressPointsToMe(view_addr)) {
                dprintf(D_ALWAYS, "Not forwarding to View Server %s - self referential\n", vhost);
                delete vhd;
                continue;
            }
            dprintf(D_ALWAYS, "Will forward ads on to View Server %s\n", vhost);

            Sock* vhsock = NULL;
            if (vhd->useTCPForUpdates()) {
                vhsock = new ReliSock();
            } else {
                vhsock = new SafeSock();
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
			free(tmp);
		}
	}

	int history_size = 1024;

	bool collector_daemon_stats = param_boolean ("COLLECTOR_DAEMON_STATS",true);
	collectorStats.setDaemonStats( collector_daemon_stats );
	if (param_boolean("RESET_DC_STATS_ON_RECONFIG",false)) {
		daemonCore->dc_stats.Clear();
		collectorStats.global.Clear();
	}

	history_size = param_integer ("COLLECTOR_DAEMON_HISTORY_SIZE",128);
	collectorStats.setDaemonHistorySize( history_size );

	time_t garbage_interval = param_integer( "COLLECTOR_STATS_SWEEP", DEFAULT_COLLECTOR_STATS_GARBAGE_INTERVAL );
	collectorStats.setGarbageCollectionInterval( garbage_interval );

    max_query_workers = param_integer ("COLLECTOR_QUERY_WORKERS", 4, 0);
	max_pending_query_workers = param_integer ("COLLECTOR_QUERY_WORKERS_PENDING", 50, 0);
	max_query_worktime = param_integer("COLLECTOR_QUERY_MAX_WORKTIME",0,0);
	reserved_for_highprio_query_workers = param_integer("COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO",1,0);

	// max_query_workers had better be at least one greater than reserved_for_highprio_query_workers,
	// or condor_status queries will never be answered.
	// note we do allow max_query_workers to be zero, which means do all queries in-proc - this
	// is useful for developers profiling the code, since profiling forked workers is a pain.
	if ( max_query_workers - reserved_for_highprio_query_workers < 1) {
		reserved_for_highprio_query_workers = MAX(0, (max_query_workers - 1) );
		dprintf(D_ALWAYS,
				"Warning: Resetting COLLECTOR_QUERY_WORKERS_RESERVE_FOR_HIGH_PRIO to %d\n",
				reserved_for_highprio_query_workers);
	}

#ifdef TRACK_QUERIES_BY_SUBSYS
	want_track_queries_by_subsys = param_boolean("COLLECTOR_TRACK_QUERY_BY_SUBSYS",true);
#endif

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
	// forkQuery.DeleteAll( );
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}
	free( CollectorName );
	delete ad;
	delete collectorsToUpdate;
	delete worldCollector;
	delete m_ccb_server;
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
	// forkQuery.DeleteAll( );
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}
	free( CollectorName );
	delete ad;
	delete collectorsToUpdate;
	delete worldCollector;
	delete m_ccb_server;
	return;
}

void CollectorDaemon::sendCollectorAd()
{
    // compute submitted jobs information
    submittorRunningJobs = 0;
    submittorIdleJobs = 0;
    submittorNumAds = 0;
    if( !collector.walkHashTable( SUBMITTOR_AD, reportSubmittorScanFunc ) ) {
         dprintf( D_ALWAYS, "Error making collector ad (submittor scan)\n" );
    }
    collectorStats.global.SubmitterAds = submittorNumAds;

    // compute machine information
    machinesTotal = 0;
    machinesUnclaimed = 0;
    machinesClaimed = 0;
    machinesOwner = 0;
    startdNumAds = 0;
	ustatsAccum.Reset( );
    if (!collector.walkHashTable (STARTD_AD, reportMiniStartdScanFunc)) {
            dprintf (D_ALWAYS, "Error making collector ad (startd scan) \n");
    }
    collectorStats.global.MachineAds = startdNumAds;

    // insert values into the ad
    ad->InsertAttr(ATTR_RUNNING_JOBS,submittorRunningJobs);
    ad->InsertAttr(ATTR_IDLE_JOBS,submittorIdleJobs);
    ad->InsertAttr(ATTR_NUM_HOSTS_TOTAL,machinesTotal);
    ad->InsertAttr(ATTR_NUM_HOSTS_CLAIMED,machinesClaimed);
    ad->InsertAttr(ATTR_NUM_HOSTS_UNCLAIMED,machinesUnclaimed);
    ad->InsertAttr(ATTR_NUM_HOSTS_OWNER,machinesOwner);

	// Accumulate for the monthly
	ustatsMonthly.setMax( ustatsAccum );

	// If we've got any universe reports, find the maxes
	ustatsAccum.publish( ATTR_CURRENT_JOBS_RUNNING, ad );
	ustatsMonthly.publish( ATTR_MAX_JOBS_RUNNING, ad );

	// Collector engine stats, too
	collectorStats.publishGlobal( ad, NULL );
    daemonCore->dc_stats.Publish(*ad);
    daemonCore->monitor_data.ExportData(ad);

	//
	// Make sure that our own ad is in the collector hashtable table, even if send-updates won't put it there
	// note that sendUpdates might soon _overwrite_ this ad with a differnt copy, but that's ok once we have
	// identified the selfAd with the collector engine, it's supposed to keep the self ad pointer updated
	// as it replaces the collector ad in that hashtable slot.
	//
	int error = 0;
	ClassAd * selfAd = new ClassAd(*ad);
	if( ! collector.collect( UPDATE_COLLECTOR_AD, selfAd, condor_sockaddr::null, error ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to add my own ad to myself (%d).\n", error );
	}
	collector.identifySelfAd(selfAd);

	// inserting the selfAd into the collector hashtable will stomp the update counters
	// so clear out the per-daemon Updates* stats to avoid confusion with the global stats
	// and re-publish the global collector stats.
	//PRAGMA_REMIND("tj: remove this code once the collector generates it's ad when queried.")
	selfAd->Delete(ATTR_UPDATESTATS_HISTORY);
	selfAd->Delete(ATTR_UPDATESTATS_SEQUENCED);
	collectorStats.publishGlobal(selfAd, NULL);

	// Send the ad
	int num_updated = collectorsToUpdate->sendUpdates(UPDATE_COLLECTOR_AD, ad, NULL, false);
	if ( num_updated != collectorsToUpdate->number() ) {
		dprintf( D_ALWAYS, "Unable to send UPDATE_COLLECTOR_AD to all configured collectors\n");
	}

	// update the world ad, but only if there are some machines. You oftentimes
	// see people run a collector on each macnine in their pool. Duh.
	if ( worldCollector && machinesTotal > 0) {
		char update_addr_default [] = "(null)";
		const char *update_addr = worldCollector->addr();
		if (!update_addr) update_addr = update_addr_default;
		if( ! worldCollector->sendUpdate(UPDATE_COLLECTOR_AD, ad, collectorsToUpdate->getAdSeq(), NULL, false) ) {
			dprintf( D_ALWAYS, "Can't send UPDATE_COLLECTOR_AD to collector "
					 "(%s): %s\n", update_addr,
					 worldCollector->error() );
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

	ClassAd *pvtAd = NULL;
	if (cmd == UPDATE_STARTD_AD) {
		// Forward the startd private ad as well.  This allows the
		// target collector to act as an aggregator for multiple collectors
		// that balance the load of authenticating connections from
		// the rest of the pool.
		AdNameHashKey hk;
		ASSERT( makeStartdAdHashKey (hk, theAd) );
		pvtAd = collector.lookup(STARTD_PVT_AD,hk);
	}

	bool should_forward = true;
	theAd->LookupBool( ATTR_SHOULD_FORWARD, should_forward );
	if ( !should_forward && pvtAd ) {
		pvtAd->LookupBool( ATTR_SHOULD_FORWARD, should_forward );
	}
	if ( !should_forward ) {
		// TODO Should we remove the ShouldForward attribute?
		dprintf( D_FULLDEBUG, "Trying to forward ad on, but %s=False\n", ATTR_SHOULD_FORWARD );
		return;
	}

    for (vector<vc_entry>::iterator e(vc_list.begin());  e != vc_list.end();  ++e) {
        DCCollector* view_coll = e->collector;
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

        // If there's a private startd ad, send that as well.
        if (pvtAd) {
            if (!putClassAd(view_sock, *pvtAd)) {
                dprintf( D_ALWAYS, "Can't forward startd private classad to View Collector %s\n", view_name);
                view_sock->end_of_message();
                view_sock->close();
                continue;
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

