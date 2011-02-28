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

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "get_daemon_name.h"
#include "subsystem_info.h"
#include "condor_config.h"
#include "stat_info.h"

#include "Axis2SoapProvider.h"

// about self
DECL_SUBSYSTEM("QUERY_SERVER", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

ClassAd	*ad = NULL;
Axis2SoapProvider* provider = NULL;

extern MyString m_path;

void init_classad();
void Dump();
int HandleTransportSocket(Service *, Stream *);

//-------------------------------------------------------------

int main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_ALWAYS, "main_init() called\n");

    // TODO: may want to get these from condor config?
    const char* log_file = "./axis2.log";
    std::string repo_path = getenv("WSFCPP_HOME");

    // init transport here
    provider = new Axis2SoapProvider(AXIS2_LOG_LEVEL_DEBUG,log_file,repo_path.c_str());
    std::string axis_error;

    if (!provider->init(DEFAULT_PORT,AXIS2_HTTP_DEFAULT_SO_TIMEOUT,axis_error)) {
        EXCEPT("Failed to initialize Axis2SoapProvider");
    }

	init_classad();

	ReliSock *sock = new ReliSock;
	if (!sock) {
		EXCEPT("Failed to allocate transport socket");
	}
	// TODO: get socket from transport here?
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

    // TODO: update classad impls here?
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
	// TODO: respond to a transport callback here?
	std::string provider_error;
    if (!provider->processHttpRequest(provider_error)) {
        dprintf (D_ALWAYS,"Error processing request: %s\n",provider_error.c_str());
    }

	return KEEP_STREAM;
}


void
Dump()
{
	dprintf(D_ALWAYS|D_NOHEADER, "DUMP called\n");
}
