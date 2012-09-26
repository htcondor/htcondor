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
#include "credd.windows.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "store_cred.h"
#include "internet.h"
#include "get_daemon_name.h"
#include "subsystem_info.h"

//-------------------------------------------------------------

CredDaemon *credd;

CredDaemon::CredDaemon() : m_name(NULL), m_update_collector_tid(-1)
{

	reconfig();

		// Command handler for the user condor_store_cred tool
	daemonCore->Register_Command( STORE_CRED, "STORE_CRED", 
								(CommandHandler)&store_cred_handler, 
								"store_cred_handler", NULL, WRITE, 
								D_FULLDEBUG );

		// Command handler for daemons to get the password
	daemonCore->Register_Command( CREDD_GET_PASSWD, "CREDD_GET_PASSWD", 
								(CommandHandlercpp)&CredDaemon::get_passwd_handler, 
								"get_passwd_handler", this, DAEMON, 
								D_FULLDEBUG );

		// NOP command for testing authentication
	daemonCore->Register_Command( CREDD_NOP, "CREDD_NOP",
								(CommandHandlercpp)&CredDaemon::nop_handler,
								"nop_handler", this, DAEMON,
								D_FULLDEBUG );

		// set timer to periodically advertise ourself to the collector
	daemonCore->Register_Timer(0, m_update_collector_interval,
		(TimerHandlercpp)&CredDaemon::update_collector, "update_collector", this);
}

CredDaemon::~CredDaemon()
{
	// tell our collector we're going away
	invalidate_ad();

	if (m_name != NULL) {
		free(m_name);
	}
}

void
CredDaemon::reconfig()
{
	// get our daemon name; if CREDD_HOST is defined, we use it
	// as our name. this is because clients that have CREDD_HOST
	// defined will query the Collector for a CredD ad that has
	// a name matching its setting for CREDD_HOST. but CREDD_HOST
	// will not necessarily match what default_daemon_name()
	// returns - for example, if CREDD_HOST is set to a DNS alias
	//
	if (m_name != NULL) {
		free(m_name);
	}
	m_name = param("CREDD_HOST");
	if (m_name == NULL) {
		char* tmp = default_daemon_name();
		ASSERT(tmp != NULL);
		m_name = strdup(tmp);
		ASSERT(m_name != NULL);
		delete[] tmp;
	}
	if(m_name == NULL) {
		EXCEPT("default_daemon_name() returned NULL");
	}

	// how often do we update the collector?
	m_update_collector_interval = param_integer ("CREDD_UPDATE_INTERVAL",
												 5 * MINUTE);

	// fill in our classad
	initialize_classad();

	// reset the timer (if it exists) to update the collector
	if (m_update_collector_tid != -1) {
		daemonCore->Reset_Timer(m_update_collector_tid, 0, m_update_collector_interval);
	}
}

void
CredDaemon::initialize_classad()
{
	m_classad.Clear();

	m_classad.SetMyTypeName(CREDD_ADTYPE);
	m_classad.SetTargetTypeName("");

	MyString line;

	line.formatstr("%s = \"%s\"", ATTR_NAME, m_name );
	m_classad.Insert(line.Value());

	line.formatstr ("%s = \"%s\"", ATTR_CREDD_IP_ADDR,
			daemonCore->InfoCommandSinfulString() );
	m_classad.Insert(line.Value());

        // Publish all DaemonCore-specific attributes, which also handles
        // SUBSYS_ATTRS for us.
    daemonCore->publish(&m_classad);
}

void
CredDaemon::update_collector()
{
	daemonCore->sendUpdates(UPDATE_AD_GENERIC, &m_classad, NULL, true);
}

void
CredDaemon::invalidate_ad()
{
	ClassAd query_ad;
	query_ad.SetMyTypeName(QUERY_ADTYPE);
	query_ad.SetTargetTypeName(CREDD_ADTYPE);

	MyString line;
	line.formatstr("%s = TARGET.%s == \"%s\"", ATTR_REQUIREMENTS, ATTR_NAME, m_name);
    query_ad.Insert(line.Value());
	query_ad.Assign(ATTR_NAME,m_name);

	daemonCore->sendUpdates(INVALIDATE_ADS_GENERIC, &query_ad, NULL, true);
}

void
CredDaemon::get_passwd_handler(int i, Stream *s)
{
	char *client_user = NULL;
	char *client_domain = NULL;
	char *client_ipaddr = NULL;
	int result;
	char * user = NULL;
	char * domain = NULL;
	char * password = NULL;	

	/* Check our connection.  We must be very picky since we are talking
	   about sending out passwords.  We want to make certain
	     a) the Stream is a ReliSock (tcp)
		 b) it is authenticated (and thus authorized by daemoncore)
		 c) it is encrypted
	*/

	if ( s->type() != Stream::reli_sock ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt via UDP from %s\n",
				((Sock*)s)->peer_addr().to_sinful().Value());
		return;
	}

	ReliSock* sock = (ReliSock*)s;

	if ( !sock->triedAuthentication() ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt without authentication from %s\n",
				sock->peer_addr().to_sinful().Value());
		goto bail_out;
	}

	if ( !sock->get_encryption() ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt without encryption from %s\n",
				sock->peer_addr().to_sinful().Value());
		goto bail_out;
	}

		// Get the username and domain from the wire

	sock->decode();
	
	result = sock->code(user);
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to recv user.\n");
		goto bail_out;
	}
	
	result = sock->code(domain);
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to recv domain.\n");
		goto bail_out;
	}
	
	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to recv eom.\n");
		goto bail_out;
	}

	client_user = strdup(sock->getOwner());
	client_domain = strdup(sock->getDomain());
	client_ipaddr = strdup(sock->peer_addr().to_sinful().Value());

		// Now fetch the password from the secure store -- 
		// If not LocalSystem, this step will fail.
	password = getStoredCredential(user,domain);
	if (!password) {
		dprintf(D_ALWAYS,
			"Failed to fetch password for %s@%s requested by %s@%s at %s\n",
			user,domain,
			client_user,client_domain,client_ipaddr);
		goto bail_out;
	}

		// Got the password, send it
	sock->encode();
	result = sock->code(password);
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to send password.\n");
		free(password);
		goto bail_out;
	}

	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to send eom.\n");
		goto bail_out;
	}

		// Now that we sent the password, immediately zero it out from ram
	SecureZeroMemory(password,strlen(password));

	dprintf(D_ALWAYS,
			"Fetched user %s@%s password requested by %s@%s at %s\n",
			user,domain,client_user,client_domain,client_ipaddr);

bail_out:
	if (client_user) free(client_user);
	if (client_domain) free(client_domain);
	if (client_ipaddr) free(client_ipaddr);
	if (user) free(user);
	if (domain) free(domain);
	if (password) free(password);
}

void
CredDaemon::nop_handler(int, Stream*)
{
	return;
}

//-------------------------------------------------------------

void
main_init(int argc, char *argv[])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	credd = new CredDaemon;
}

//-------------------------------------------------------------

void
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");

	credd->reconfig();
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");

	delete credd;

	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");

	delete credd;

	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem( "CREDD", SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
