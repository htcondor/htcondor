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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"
#include "condor_open.h"

#include "daemon.h"
#include "dc_starter.h"
#include "internet.h"
#include "condor_claimid_parser.h"
#include "condor_base64.h"


DCStarter::DCStarter( const char* sName ) : Daemon( DT_STARTER, sName, NULL )
{
	is_initialized = false;
}


DCStarter::~DCStarter( void )
{
}


bool
DCStarter::initFromClassAd( ClassAd* ad )
{
	char* tmp = NULL;

	if( ! ad ) {
		dprintf( D_ALWAYS, 
				 "ERROR: DCStarter::initFromClassAd() called with NULL ad\n" );
		return false;
	}

	ad->LookupString( ATTR_STARTER_IP_ADDR, &tmp );
	if( ! tmp ) {
			// If that's not defined, try ATTR_MY_ADDRESS
		ad->LookupString( ATTR_MY_ADDRESS, &tmp );
	}
	if( ! tmp ) {
		dprintf( D_FULLDEBUG, "ERROR: DCStarter::initFromClassAd(): "
				 "Can't find starter address in ad\n" );
		return false;
	} else {
		if( is_valid_sinful(tmp) ) {
			New_addr( strnewp(tmp) );
			is_initialized = true;
		} else {
			dprintf( D_FULLDEBUG, 
					 "ERROR: DCStarter::initFromClassAd(): invalid %s in ad (%s)\n", 
					 ATTR_STARTER_IP_ADDR, tmp );
		}
		free( tmp );
		tmp = NULL;
	}

	if( ad->LookupString(ATTR_VERSION, &tmp) ) {
		New_version( strnewp(tmp) );
		free( tmp );
		tmp = NULL;
	}

	return is_initialized;
}


bool
DCStarter::locate( void )
{
	if( _addr ) {
		return true;
	} 
	return is_initialized;
}


bool
DCStarter::reconnect( ClassAd* req, ClassAd* reply, ReliSock* rsock,
					  int timeout, char const *sec_session_id )
{
	setCmdStr( "reconnectJob" );

	std::string line;

		// Add our own attributes to the request ad we're sending
	line = ATTR_COMMAND;
	line += "=\"";
	line += getCommandString( CA_RECONNECT_JOB );
	line += '"';
	req->Insert( line.c_str() );

	return sendCACmd( req, reply, rsock, false, timeout, sec_session_id );
	

}

// Based on dc_schedd.C's updateGSIcredential
DCStarter::X509UpdateStatus
DCStarter::updateX509Proxy( const char * filename, char const *sec_session_id)
{
	ReliSock rsock;
	rsock.timeout(60);
	if( ! rsock.connect(_addr) ) {
		dprintf(D_ALWAYS, "DCStarter::updateX509Proxy: "
			"Failed to connect to starter %s\n", _addr);
		return XUS_Error;
	}

	CondorError errstack;
	if( ! startCommand(UPDATE_GSI_CRED, &rsock, 0, &errstack, NULL, false, sec_session_id) ) {
		dprintf( D_ALWAYS, "DCStarter::updateX509Proxy: "
				 "Failed send command to the starter: %s\n",
				 errstack.getFullText().c_str());
		return XUS_Error;
	}

		// Send the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_file(&file_size,filename) < 0 ) {
		dprintf(D_ALWAYS,
			"DCStarter::updateX509Proxy "
			"failed to send proxy file %s (size=%ld)\n",
			filename, (long int)file_size);
		return XUS_Error;
	}

		// Fetch the result
	rsock.decode();
	int reply = 0;
	rsock.code(reply);
	rsock.end_of_message();

	switch(reply) {
		case 0: return XUS_Error;
		case 1: return XUS_Okay;
		case 2: return XUS_Declined;
	}
	dprintf(D_ALWAYS, "DCStarter::updateX509Proxy: "
		"remote side returned unknown code %d. Treating "
		"as an error.\n", reply);
	return XUS_Error;
}

DCStarter::X509UpdateStatus
DCStarter::delegateX509Proxy( const char * filename, time_t expiration_time, char const *sec_session_id, time_t *result_expiration_time)
{
	ReliSock rsock;
	rsock.timeout(60);
	if( ! rsock.connect(_addr) ) {
		dprintf(D_ALWAYS, "DCStarter::delegateX509Proxy: "
			"Failed to connect to starter %s\n", _addr);
		return XUS_Error;
	}

	CondorError errstack;
	if( ! startCommand(DELEGATE_GSI_CRED_STARTER, &rsock, 0, &errstack, NULL, false, sec_session_id) ) {
		dprintf( D_ALWAYS, "DCStarter::delegateX509Proxy: "
				 "Failed send command to the starter: %s\n",
				 errstack.getFullText().c_str());
		return XUS_Error;
	}

		// Send the gsi proxy
	filesize_t file_size = 0;	// will receive the size of the file
	if ( rsock.put_x509_delegation(&file_size,filename,expiration_time,result_expiration_time) < 0 ) {
		dprintf(D_ALWAYS,
			"DCStarter::delegateX509Proxy "
			"failed to delegate proxy file %s (size=%ld)\n",
			filename, (long int)file_size);
		return XUS_Error;
	}

		// Fetch the result
	rsock.decode();
	int reply = 0;
	rsock.code(reply);
	rsock.end_of_message();

	switch(reply) {
		case 0: return XUS_Error;
		case 1: return XUS_Okay;
		case 2: return XUS_Declined;
	}
	dprintf(D_ALWAYS, "DCStarter::delegateX509Proxy: "
		"remote side returned unknown code %d. Treating "
		"as an error.\n", reply);
	return XUS_Error;
}

StarterHoldJobMsg::StarterHoldJobMsg( char const *hold_reason, int hold_code, int hold_subcode, bool soft ):
	DCMsg(STARTER_HOLD_JOB),
	m_hold_reason(hold_reason),
	m_hold_code(hold_code),
	m_hold_subcode(hold_subcode),
	m_soft(soft)
{
}

bool
StarterHoldJobMsg::writeMsg( DCMessenger * /*messenger*/, Sock *sock )
{
		// send hold job command to starter
	return
		sock->put(m_hold_reason) &&
		sock->put(m_hold_code) &&
		sock->put(m_hold_subcode) &&
		sock->put((int)m_soft);
}

DCMsg::MessageClosureEnum
StarterHoldJobMsg::messageSent( DCMessenger *messenger, Sock *sock )
{
		// now wait for reply
	messenger->startReceiveMsg(this,sock);
	return MESSAGE_CONTINUING;
}

bool
StarterHoldJobMsg::readMsg( DCMessenger * /*messenger*/, Sock *sock )
{
		// read reply from starter
	int success=0;
	sock->get(success);

	return success!=0;
}

bool
DCStarter::createJobOwnerSecSession(int timeout,char const *job_claim_id,char const *starter_sec_session,char const *session_info,MyString &owner_claim_id,MyString &error_msg,MyString &starter_version,MyString &starter_addr)
{
	ReliSock sock;

	if( !connectSock(&sock, timeout, NULL) ) {
		error_msg = "Failed to connect to starter";
		return false;
	}

	if( !startCommand(CREATE_JOB_OWNER_SEC_SESSION, &sock,timeout,NULL,NULL,false,starter_sec_session) ) {
		error_msg = "Failed to send CREATE_JOB_OWNER_SEC_SESSION to starter";
		return false;
	}

	ClassAd input;
	input.Assign(ATTR_CLAIM_ID,job_claim_id);
	input.Assign(ATTR_SESSION_INFO,session_info);

	sock.encode();
	if( !input.put(sock) || !sock.end_of_message() ) {
		error_msg = "Failed to compose CREATE_JOB_OWNER_SEC_SESSION to starter";
		return false;
	}

	sock.decode();

	ClassAd reply;
	if( !reply.initFromStream(sock) || !sock.end_of_message() ) {
		error_msg = "Failed to get response to CREATE_JOB_OWNER_SEC_SESSION from starter";
		return false;
	}

	bool success = false;
	reply.LookupBool(ATTR_RESULT,success);
	if( !success ) {
		reply.LookupString(ATTR_ERROR_STRING,error_msg);
		return false;
	}

	reply.LookupString(ATTR_CLAIM_ID,owner_claim_id);
	reply.LookupString(ATTR_VERSION,starter_version);
		// get the full starter address from the starter in case it contains
		// extra CCB info that we don't already know about
	reply.LookupString(ATTR_STARTER_IP_ADDR,starter_addr);
	return true;
}

bool DCStarter::startSSHD(char const *known_hosts_file,char const *private_client_key_file,char const *preferred_shells,char const *slot_name,char const *ssh_keygen_args,ReliSock &sock,int timeout,char const *sec_session_id,MyString &remote_user,MyString &error_msg,bool &retry_is_sensible)
{

	retry_is_sensible = false;

#ifndef HAVE_SSH_TO_JOB
	error_msg = "This version of Condor does not support ssh key exchange.";
	return false;
#else
	if( !connectSock(&sock, timeout, NULL) ) {
		error_msg = "Failed to connect to starter";
		return false;
	}

	if( !startCommand(START_SSHD, &sock,timeout,NULL,NULL,false,sec_session_id) ) {
		error_msg = "Failed to send START_SSHD to starter";
		return false;
	}

	ClassAd input;

	if( preferred_shells && *preferred_shells ) {
		input.Assign(ATTR_SHELL,preferred_shells);
	}

	if( slot_name && *slot_name ) {
			// This is a little silly.
			// We are telling the remote side the name of the slot so
			// that it can put it in the welcome message.
		input.Assign(ATTR_NAME,slot_name);
	}

	if( ssh_keygen_args && *ssh_keygen_args ) {
		input.Assign(ATTR_SSH_KEYGEN_ARGS,ssh_keygen_args);
	}

	sock.encode();
	if( !input.put(sock) || !sock.end_of_message() ) {
		error_msg = "Failed to send START_SSHD request to starter";
		return false;
	}

	ClassAd result;
	sock.decode();
	if( !result.initFromStream(sock) || !sock.end_of_message() ) {
		error_msg = "Failed to read response to START_SSHD from starter";
		return false;
	}

	bool success = false;
	result.LookupBool(ATTR_RESULT,success);
	if( !success ) {
		std::string remote_error_msg;
		result.LookupString(ATTR_ERROR_STRING,remote_error_msg);
		error_msg.formatstr("%s: %s",slot_name,remote_error_msg.c_str());
		retry_is_sensible = false;
		result.LookupBool(ATTR_RETRY,retry_is_sensible);
		return false;
	}

	result.LookupString(ATTR_REMOTE_USER,remote_user);

	std::string public_server_key;
	if( !result.LookupString(ATTR_SSH_PUBLIC_SERVER_KEY,public_server_key) ) {
		error_msg = "No public ssh server key received in reply to START_SSHD";
		return false;
	}
	std::string private_client_key;
	if( !result.LookupString(ATTR_SSH_PRIVATE_CLIENT_KEY,private_client_key) ) {
		error_msg = "No ssh client key received in reply to START_SSHD";
		return false;
	}


		// store the private client key
	unsigned char *decode_buf = NULL;
	int length = -1;
	condor_base64_decode(private_client_key.c_str(),&decode_buf,&length);
	if( !decode_buf ) {
		error_msg = "Error decoding ssh client key.";
		return false;
	}
	FILE *fp = safe_fcreate_fail_if_exists(private_client_key_file,"a",0400);
	if( !fp ) {
		error_msg.formatstr("Failed to create %s: %s",
						  private_client_key_file,strerror(errno));
		free( decode_buf );
		return false;
	}
	if( fwrite(decode_buf,length,1,fp)!=1 ) {
		error_msg.formatstr("Failed to write to %s: %s",
						  private_client_key_file,strerror(errno));
		fclose( fp );
		free( decode_buf );
		return false;
	}
	if( fclose(fp)!=0 ) {
		error_msg.formatstr("Failed to close %s: %s",
						  private_client_key_file,strerror(errno));
		free( decode_buf );
		return false;
	}
	fp = NULL;
	free( decode_buf );
	decode_buf = NULL;


		// store the public server key in the known_hosts file
	length = -1;
	condor_base64_decode(public_server_key.c_str(),&decode_buf,&length);
	if( !decode_buf ) {
		error_msg = "Error decoding ssh server key.";
		return false;
	}
	fp = safe_fcreate_fail_if_exists(known_hosts_file,"a",0600);
	if( !fp ) {
		error_msg.formatstr("Failed to create %s: %s",
						  known_hosts_file,strerror(errno));
		free( decode_buf );
		return false;
	}

		// prepend a host name pattern (*) to the public key to make a valid
		// record in the known_hosts file
	fprintf(fp,"* ");

	if( fwrite(decode_buf,length,1,fp)!=1 ) {
		error_msg.formatstr("Failed to write to %s: %s",
						  known_hosts_file,strerror(errno));
		fclose( fp );
		free( decode_buf );
		return false;
	}

	if( fclose(fp)!=0 ) {
		error_msg.formatstr("Failed to close %s: %s",
						  known_hosts_file,strerror(errno));
		free( decode_buf );
		return false;
	}
	fp = NULL;
	free( decode_buf );
	decode_buf = NULL;

	return true;
#endif
}
