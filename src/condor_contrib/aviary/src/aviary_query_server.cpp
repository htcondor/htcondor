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

// local includes
#include "Axis2SoapProvider.h"
#include "JobServerJobLogConsumer.h"
#include "JobServerObject.h"
#include "HistoryProcessingUtils.h"
#include "Globals.h"

// about self
DECL_SUBSYSTEM("QUERY_SERVER", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

using namespace std;
using namespace aviary::query;
using namespace aviary::soap;
using namespace aviary::history;

ClassAd	*ad = NULL;
Axis2SoapProvider* provider = NULL;
JobLogMirror *mirror = NULL;
JobServerJobLogConsumer *consumer = NULL;
JobServerObject *job_server = NULL;

extern MyString m_path;

void init_classad();
void Dump();
int HandleTransportSocket(Service *, Stream *);
int HandleResetSignal(Service *, int);
void ProcessHistoryTimer(Service*);

//-------------------------------------------------------------

int main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	// setup the job log consumer
	consumer = new JobServerJobLogConsumer();
	mirror = new JobLogMirror(consumer);
	mirror->init();

    // config then env for our all-important axis2 repo dir
    const char* log_file = "./aviary_query.axis2.log";
	string repo_path;
	char *tmp = NULL;
	if (tmp = param("WSFCPP_HOME")) {
		repo_path = tmp;
		free(tmp);
	}
	else if (tmp = getenv("WSFCPP_HOME")) {
		repo_path = tmp;
	}
	else {
		EXCEPT("No WSFCPP_HOME in config or env");
	}

	int port = param_integer("HTTP_PORT",9091);
	int level = param_integer("AXIS2_DEBUG_LEVEL",AXIS2_LOG_LEVEL_CRITICAL);

    // init transport here
    provider = new Axis2SoapProvider(level,log_file,repo_path.c_str());

    std::string axis_error;
    if (!provider->init(port,AXIS2_HTTP_DEFAULT_SO_TIMEOUT,axis_error)) {
		dprintf(D_ALWAYS, "%s\n",axis_error.c_str());
        EXCEPT("Failed to initialize Axis2SoapProvider");
    }

	init_classad();

	ReliSock *sock = new ReliSock;
	if (!sock) {
		EXCEPT("Failed to allocate transport socket");
	}

	if (!sock->assign(provider->getHttpListenerSocket())) {
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

	dprintf(D_ALWAYS,"Axis2 listener on http port: %d\n",port);

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

	return TRUE;
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

int 
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");

	return TRUE;
}

//-------------------------------------------------------------

void Stop()
{
	if (param_boolean("DUMP_STATE", false)) {
		Dump();
	}

	delete job_server;

	DC_Exit(0);
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");

	Stop();

	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");

	Stop();

	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int /* argc */, char* /* argv */ [] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}


int
HandleTransportSocket(Service *, Stream *)
{
	// respond to a transport callback here
	std::string provider_error;
    if (!provider->processHttpRequest(provider_error)) {
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
    processHistoryDirectory();
    processOrphanedIndices();
    processCurrentHistory();
}


void
Dump()
{
	dprintf(D_ALWAYS|D_NOHEADER, "DUMP called\n");
}
