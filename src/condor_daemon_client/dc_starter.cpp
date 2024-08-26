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
#include "condor_ver_info.h"
#include "condor_version.h"
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
	std::string tmp;

	if( ! ad ) {
		dprintf( D_ALWAYS, 
				 "ERROR: DCStarter::initFromClassAd() called with NULL ad\n" );
		return false;
	}

	ad->LookupString( ATTR_STARTER_IP_ADDR, tmp );
	if( tmp.empty() ) {
			// If that's not defined, try ATTR_MY_ADDRESS
		ad->LookupString( ATTR_MY_ADDRESS, tmp );
	}
	if( tmp.empty() ) {
		dprintf( D_FULLDEBUG, "ERROR: DCStarter::initFromClassAd(): "
				 "Can't find starter address in ad\n" );
		return false;
	} else {
		if( is_valid_sinful(tmp.c_str()) ) {
			Set_addr(tmp);
			is_initialized = true;
		} else {
			dprintf( D_FULLDEBUG, 
					 "ERROR: DCStarter::initFromClassAd(): invalid %s in ad (%s)\n", 
					 ATTR_STARTER_IP_ADDR, tmp.c_str() );
		}
	}

	ad->LookupString(ATTR_VERSION, _version);

	return is_initialized;
}


bool
DCStarter::locate( LocateType /*method=LOCATE_FULL*/ )
{
	if( ! _addr.empty() ) {
		return true;
	} 
	return is_initialized;
}


bool
DCStarter::reconnect( ClassAd* req, ClassAd* reply, ReliSock* rsock,
					  int timeout, char const *sec_session_id )
{
	setCmdStr( "reconnectJob" );

	req->Assign( ATTR_COMMAND, getCommandString( CA_RECONNECT_JOB ) );

	return sendCACmd( req, reply, rsock, false, timeout, sec_session_id );
	

}

// Based on dc_schedd.C's updateGSIcredential
DCStarter::X509UpdateStatus
DCStarter::updateX509Proxy( const char * filename, char const *sec_session_id)
{
	ReliSock rsock;
	rsock.timeout(60);
	if( ! rsock.connect(_addr.c_str()) ) {
		dprintf(D_ALWAYS, "DCStarter::updateX509Proxy: "
			"Failed to connect to starter %s\n", _addr.c_str());
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
	if( ! rsock.connect(_addr.c_str()) ) {
		dprintf(D_ALWAYS, "DCStarter::delegateX509Proxy: "
			"Failed to connect to starter %s\n", _addr.c_str());
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
	int r = sock->get(success);
	if (!r) {
		dprintf(D_ALWAYS, "Error reading hold message reply from starter\n");
	}

	return success!=0;
}

bool
DCStarter::createJobOwnerSecSession(int timeout,char const *job_claim_id,char const *starter_sec_session,char const *session_info,std::string &owner_claim_id,std::string &error_msg,std::string &starter_version,std::string &starter_addr)
{
	ReliSock sock;

	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "DCStarter::createJobOwnerSecSession(%s,...) making connection to %s\n",
			getCommandStringSafe(CREATE_JOB_OWNER_SEC_SESSION), _addr.c_str());
	}

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
	if( !putClassAd(&sock, input) || !sock.end_of_message() ) {
		error_msg = "Failed to compose CREATE_JOB_OWNER_SEC_SESSION to starter";
		return false;
	}

	sock.decode();

	ClassAd reply;
	if( !getClassAd(&sock, reply) || !sock.end_of_message() ) {
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

bool
fnHadSharedPortProblem( void * v, int code, const char * /* subsys */, const char * msg ) {
	if( code == CEDAR_ERR_NO_SHARED_PORT ) {
		*(const char **)v = msg;
	}
	return true;
}

bool DCStarter::startSSHD(char const *known_hosts_file,char const *private_client_key_file,char const *preferred_shells,char const *slot_name,char const *ssh_keygen_args,ReliSock &sock,int timeout,char const *sec_session_id,std::string &remote_user,std::string &error_msg,bool &retry_is_sensible)
{

	retry_is_sensible = false;

#ifndef HAVE_SSH_TO_JOB
	error_msg = "This version of Condor does not support ssh key exchange.";
	return false;
#else
	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "DCStarter::startSSHD(%s,...) making connection to %s\n",
			getCommandStringSafe(START_SSHD), _addr.c_str());
	}

	CondorError errorStack;
	if( !connectSock(&sock, timeout, &errorStack) ) {
		const char * theSharedPortProblem = NULL;
		errorStack.walk( fnHadSharedPortProblem, &theSharedPortProblem );

		if( theSharedPortProblem != NULL ) {
			formatstr( error_msg, "Can't connect to starter: %s.", theSharedPortProblem );
		} else {
			error_msg = "Failed to connect to starter";
		}
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
	if( !putClassAd(&sock, input) || !sock.end_of_message() ) {
		error_msg = "Failed to send START_SSHD request to starter";
		return false;
	}

	ClassAd result;
	sock.decode();
	if( !getClassAd(&sock, result) || !sock.end_of_message() ) {
		error_msg = "Failed to read response to START_SSHD from starter";
		return false;
	}

	bool success = false;
	result.LookupBool(ATTR_RESULT,success);
	if( !success ) {
		std::string remote_error_msg;
		result.LookupString(ATTR_ERROR_STRING,remote_error_msg);
		formatstr(error_msg, "%s: %s",slot_name,remote_error_msg.c_str());
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
		formatstr(error_msg, "Failed to create %s: %s",
						  private_client_key_file,strerror(errno));
		free( decode_buf );
		return false;
	}
	if( fwrite(decode_buf,length,1,fp)!=1 ) {
		formatstr(error_msg, "Failed to write to %s: %s",
						  private_client_key_file,strerror(errno));
		fclose( fp );
		free( decode_buf );
		return false;
	}
	if( fclose(fp)!=0 ) {
		formatstr(error_msg, "Failed to close %s: %s",
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
		formatstr(error_msg, "Failed to create %s: %s",
						  known_hosts_file,strerror(errno));
		free( decode_buf );
		return false;
	}

		// prepend a host name pattern (*) to the public key to make a valid
		// record in the known_hosts file
	fprintf(fp,"* ");

	if( fwrite(decode_buf,length,1,fp)!=1 ) {
		formatstr(error_msg, "Failed to write to %s: %s",
						  known_hosts_file,strerror(errno));
		fclose( fp );
		free( decode_buf );
		return false;
	}

	if( fclose(fp)!=0 ) {
		formatstr(error_msg, "Failed to close %s: %s",
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

bool
DCStarter::peek(bool transfer_stdout, ssize_t &stdout_offset, bool transfer_stderr, ssize_t &stderr_offset, const std::vector<std::string> &filenames, std::vector<ssize_t> &offsets, size_t max_bytes, bool &retry_sensible, PeekGetFD &next, std::string &error_msg, unsigned timeout, const std::string &sec_session_id, DCTransferQueue *xfer_q)
{
	ClassAd ad;
	ad.InsertAttr(ATTR_JOB_OUTPUT, transfer_stdout);
	ad.InsertAttr("OutOffset", stdout_offset);
	ad.InsertAttr(ATTR_JOB_ERROR, transfer_stderr);
	ad.InsertAttr("ErrOffset", stderr_offset);
	ad.InsertAttr(ATTR_VERSION, CondorVersion());

	size_t total_files = 0;
	total_files += transfer_stdout ? 1 : 0;
	total_files += transfer_stderr ? 1 : 0;

	if (filenames.size())
	{
		total_files += filenames.size();
		std::vector<classad::ExprTree *> filelist; filelist.reserve(filenames.size());
		std::vector<classad::ExprTree *> offsetlist; offsetlist.reserve(filenames.size());
		std::vector<ssize_t>::const_iterator it2 = offsets.begin();
		for (std::vector<std::string>::const_iterator it = filenames.begin();
			it != filenames.end() && it2 != offsets.end();
			it++, it2++)
		{
			filelist.push_back(classad::Literal::MakeString(*it));
			offsetlist.push_back(classad::Literal::MakeInteger(*it2));
		}
		classad::ExprTree *list(classad::ExprList::MakeExprList(filelist));
		ad.Insert("TransferFiles", list);
		list = classad::ExprList::MakeExprList(offsetlist);
		ad.Insert("TransferOffsets", list);
	}

	ad.InsertAttr(ATTR_MAX_TRANSFER_BYTES, static_cast<long long>(max_bytes));

	ReliSock sock;

	if (IsDebugLevel(D_COMMAND)) {
		dprintf (D_COMMAND, "DCStarter::peek(%s,...) making connection to %s\n",
			getCommandStringSafe(STARTER_PEEK), _addr.c_str());
	}

	if( !connectSock(&sock, timeout, NULL) ) {
		error_msg = "Failed to connect to starter";
		return false;
	}

	if( !startCommand(STARTER_PEEK, &sock, timeout, NULL, NULL, false, sec_session_id.c_str()) ) {
		error_msg = "Failed to send START_PEEK to starter";
		return false;
	}
	sock.encode();
	if (!putClassAd(&sock, ad) || !sock.end_of_message()) {
		error_msg = "Failed to send request to starter";
		return false;
	}

	ClassAd response;
	sock.decode();
	if (!getClassAd(&sock, response) || !sock.end_of_message())
	{
		error_msg = "Failed to read response for peeking at logs.";
		return false;
	}
	dPrintAd(D_FULLDEBUG, response);

	bool success = false;
	if (!response.EvaluateAttrBool(ATTR_RESULT, success) || !success)
	{
		response.EvaluateAttrBool(ATTR_RETRY, retry_sensible);
		error_msg = "Remote operation failed.";
		response.EvaluateAttrString(ATTR_ERROR_STRING, error_msg);
		return false;
	}
	classad::Value valueX;
	classad_shared_ptr<classad::ExprList> list;
	if (!response.EvaluateAttr("TransferFiles", valueX) || !valueX.IsSListValue(list))
	{
		error_msg = "Unable to evaluate starter response";
		return false;
	}
	classad_shared_ptr<classad::ExprList> offlist;
	if (!response.EvaluateAttr("TransferOffsets", valueX) || !valueX.IsSListValue(offlist))
	{
		error_msg = "Unable to evaluate starter response (missing offsets)";
		return false;
	}

	size_t remaining = max_bytes;
	size_t file_count = 0;
	classad::ExprList::const_iterator it2 = offlist->begin();
	for (classad::ExprList::const_iterator it = list->begin();
		it != list->end() && it2 != offlist->end();
		it++, it2++)
	{
		classad::Value value;
		(*it2)->Evaluate(value);
		off_t off = -1;
		value.IsIntegerValue(off);
		(*it)->Evaluate(value);
		std::string filename;
		int64_t xfer_fd = -1;
		if (!value.IsStringValue(filename) && value.IsIntegerValue(xfer_fd))
		{
			if (xfer_fd == 0) filename = "_condor_stdout";
			if (xfer_fd == 1) filename = "_condor_stderr";
		}
		int fd = next.getNextFD(filename);
		filesize_t size = -1;
		int retval;
		if ((retval = sock.get_file(&size, fd, false, false, remaining, xfer_q)) && (retval != GET_FILE_MAX_BYTES_EXCEEDED))
		{
			error_msg = "Internal error when transferring file " + filename;
		}
		else if (size >= 0)
		{
			remaining -= max_bytes;
			file_count++;
			off += size;
		}
		else
		{
			error_msg = "Failed to transfer file " + filename;
		}
		if (xfer_fd == 0)
		{
			stdout_offset = off;
			//dprintf(D_FULLDEBUG, "New stdout offset: %ld\n", stdout_offset);
		}
		else if (xfer_fd == 1)
		{
			stderr_offset = off;
		}
		else
		{
			std::vector<ssize_t>::iterator it4 = offsets.begin();
			for (std::vector<std::string>::const_iterator it3 = filenames.begin();
				it3 != filenames.end() && it4 != offsets.end();
				it3++, it4++)
			{
				if (*it3 == filename) *it4 = off;
			}
		}
	}
	size_t remote_file_count;
	if (!sock.get(remote_file_count) || !sock.end_of_message())
	{
		error_msg = "Unable to get remote file count.";
		return false;
	}
	if (file_count != remote_file_count)
	{
		formatstr(error_msg, "Received %zu files, but remote side thought it sent %zu files\n", file_count, remote_file_count);
		return false;
	}
	if ((total_files != file_count) && !error_msg.size())
	{
		error_msg = "At least one file transfer failed.";
		return false;
	}
	return true;
}

