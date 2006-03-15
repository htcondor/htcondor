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
#include "win_credd.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "store_cred.h"
#include "internet.h"
#include "get_daemon_name.h"

//-------------------------------------------------------------

char* mySubSystem = "CREDD";		// used by Daemon Core

CredDaemon *credd;

CredDaemon::CredDaemon() : m_name(NULL), m_collectors(NULL), m_update_collector_tid(-1)
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

	delete m_collectors;
}

void
CredDaemon::reconfig()
{
	// get our daemon name
	if (m_name != NULL) delete[] m_name;
	m_name = default_daemon_name();
	if(m_name == NULL) {
		EXCEPT("default_daemon_name() returned NULL");
	}

	// how often do we update the collector?
	m_update_collector_interval = param_integer ("CREDD_UPDATE_INTERVAL",
												 5 * MINUTE);

	// fill in our classad
	initialize_classad();

	if (m_collectors != NULL) delete m_collectors;
	m_collectors = CollectorList::create();

	// reset the timer (if it exists) to update the collector
	if (m_update_collector_tid != -1) {
		daemonCore->Reset_Timer(m_update_collector_tid, 0, m_update_collector_interval);
	}
}

void
CredDaemon::initialize_classad()
{
	m_classad.clear();

	m_classad.SetMyTypeName(CREDD_ADTYPE);
	m_classad.SetTargetTypeName("");

	MyString line;
	line.sprintf("%s = \"%s\"", ATTR_MACHINE, my_full_hostname());
	m_classad.Insert(line.Value());

	line.sprintf("%s = \"%s\"", ATTR_NAME, m_name );
	m_classad.Insert(line.Value());

	line.sprintf ("%s = \"%s\"", ATTR_CREDD_IP_ADDR,
			daemonCore->InfoCommandSinfulString() );
	m_classad.Insert(line.Value());

	config_fill_ad(&m_classad);
}

void
CredDaemon::update_collector()
{
	ASSERT(m_collectors != NULL);
	m_collectors->sendUpdates(UPDATE_AD_GENERIC, &m_classad);
}

void
CredDaemon::invalidate_ad()
{
	ClassAd query_ad;
	query_ad.SetMyTypeName(QUERY_ADTYPE);
	query_ad.SetTargetTypeName(CREDD_ADTYPE);

	MyString line;
	line.sprintf("%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_NAME, m_name);
    query_ad.Insert(line.Value());

	m_collectors->sendUpdates(INVALIDATE_ADS_GENERIC, &query_ad);
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
			sin_to_string(((Sock*)s)->endpoint()));
		return;
	}

	ReliSock* sock = (ReliSock*)s;

	if ( !sock->isAuthenticated() ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt without authentication from %s\n",
			sin_to_string(sock->endpoint()));
		goto bail_out;
	}

	if ( !sock->get_encryption() ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt without encryption from %s\n",
			sin_to_string(sock->endpoint()));
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
	client_ipaddr = strdup(sin_to_string(sock->endpoint()));

		// Now fetch the password from the secure store -- 
		// If not LocalSystem, this step will fail.
	password = getStoredCredential(user,domain);
	if (!password) {
		dprintf(D_ALWAYS,
			"Failed to fetch password for %s@%s for %s@%s at %s\n",
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
			"Fetched user %s@%s password for %s@%s at %s\n",
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

int
main_init(int argc, char *argv[])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	credd = new CredDaemon;

	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool is_full )
{
	dprintf(D_ALWAYS, "main_config() called\n");

	credd->reconfig();

	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");

	delete credd;

	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");

	delete credd;

	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

