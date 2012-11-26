/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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

// condor includes
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "get_daemon_name.h"
#include "subsystem_info.h"
#include "condor_config.h"
#include "stat_info.h"
#include "JobLogMirror.h"
#include "stl_string_utils.h"

// local includes
#include "AviaryProvider.h"
#include "JobServerJobLogConsumer.h"
#include "JobServerObject.h"
#include "HistoryProcessingUtils.h"
#include "Globals.h"
#include "AviaryUtils.h"
#include "LocatorObject.h"

using namespace std;
using namespace aviary::transport;
using namespace aviary::query;
using namespace aviary::history;
using namespace aviary::util;
using namespace aviary::locator;

ClassAd	*ad = NULL;
AviaryProvider* provider = NULL;
JobLogMirror *mirror = NULL;
JobServerJobLogConsumer *consumer = NULL;
JobServerObject *job_server = NULL;

extern MyString m_path;

void init_classad();
void Dump();
int HandleTransportSocket(Service *, Stream *);
int HandleResetSignal(Service *, int);
void ProcessHistoryTimer(Service*);

const char QUERY_SERVER[] = "QUERY_SERVER";

//-------------------------------------------------------------

void main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	// setup the job log consumer
	consumer = new JobServerJobLogConsumer();
	mirror = new JobLogMirror(consumer);
	mirror->init();

	init_classad();

    string log_name("aviary_query.log");
    string id_name("query"); id_name+=SEPARATOR; id_name+=getScheddName();
    provider = AviaryProviderFactory::create(log_name,id_name,
											 "CUSTOM",QUERY_SERVER, "services/query/");
    if (!provider) {
        EXCEPT("Unable to configure AviaryProvider. Exiting...");
    }

	ReliSock *sock = new ReliSock;
	if (!sock) {
		EXCEPT("Failed to allocate transport socket");
	}

	if (!sock->assign(provider->getListenerSocket())) {
		EXCEPT("Failed to bind transport socket");
	}
	int index;
	if (-1 == (index =
			   daemonCore->Register_Socket((Stream *) sock,
                                           "Transport method socket",
										   (SocketHandler)
										   HandleTransportSocket,
                                           "Handler for transport invocations"))) {
		EXCEPT("Failed to register transport socket");
	}

	job_server = JobServerObject::getInstance();

    // before doing any job history processing, set the location of the files
    // TODO: need to test mal-HISTORY values: HISTORY=/tmp/somewhere
    const char* tmp2 = param ( "HISTORY" );
    StatInfo si( tmp2 );
    tmp2 = si.DirPath ();
    if ( !tmp2 )
    {
        dprintf ( D_ALWAYS, "warning: No HISTORY defined - Aviary Query Server will not process history jobs\n" );
    }
    else
    {
        m_path = tmp2;
        dprintf ( D_FULLDEBUG, "HISTORY path is %s\n",tmp2 );
        // register a timer for processing of historical job files
        if (-1 == (index =
            daemonCore->Register_Timer(
                0,
                param_integer("HISTORY_INTERVAL",120),
                (TimerHandler)ProcessHistoryTimer,
                "Timer for processing job history files"
                ))) {
        EXCEPT("Failed to register history timer");
        }
    }

    // useful for testing job coalescing
    // and potentially just useful
	if (-1 == (index =
		daemonCore->Register_Signal(SIGUSR1,
				    "Forced Reset Signal",
				    (SignalHandler)
				    HandleResetSignal,
				    "Handler for Reset signals"))) {
		EXCEPT("Failed to register Reset signal");
	}

}

void
init_classad()
{
	if ( ad ) {
		delete ad;
	}
	ad = new ClassAd();

	ad->SetMyTypeName("QueryServer");
	ad->SetTargetTypeName("Daemon");

	char* default_name = default_daemon_name();
		if( ! default_name ) {
			EXCEPT( "default_daemon_name() returned NULL" );
		}
	ad->Assign(ATTR_NAME, default_name);
	delete [] default_name;

	ad->Assign(ATTR_MY_ADDRESS, my_ip_string());

	// Initialize all the DaemonCore-provided attributes
	daemonCore->publish( ad );

}

//-------------------------------------------------------------

void
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");
}

//-------------------------------------------------------------

void Stop()
{
	if (param_boolean("DUMP_STATE", false)) {
		Dump();
	}

	if (provider) {
		provider->invalidate();
		delete provider;
	}
	if (job_server) {
		delete job_server;
	}

	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");

	Stop();

	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");

	Stop();

	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem(QUERY_SERVER, SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}


int
HandleTransportSocket(Service *, Stream *)
{
	// respond to a transport callback here
	std::string provider_error;
    if (!provider->processRequest(provider_error)) {
        dprintf (D_ALWAYS,"Error processing request: %s\n",provider_error.c_str());
    }

	return KEEP_STREAM;
}

int
HandleResetSignal(Service *, int)
{
	consumer->Reset();

    return TRUE;
}

void ProcessHistoryTimer(Service*) {
	dprintf(D_FULLDEBUG, "ProcessHistoryTimer() called\n");
    process_history_files();
}


void
Dump()
{
	dprintf(D_ALWAYS|D_NOHEADER, "DUMP called\n");
}
