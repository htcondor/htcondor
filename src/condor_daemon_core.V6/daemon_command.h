/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#include <string>

#ifndef _CONDOR_DAEMON_COMMAND_H_
#define _CONDOR_DAEMON_COMMAND_H_

class DaemonCommandProtocol: Service, public ClassyCountedPtr {

	friend class DaemonCore;

public:
	DaemonCommandProtocol(Stream* sock,bool is_command_sock);
	~DaemonCommandProtocol();

	int doProtocol();

private:

	enum CommandProtocolState {
		CommandProtocolAcceptTCPRequest,
		CommandProtocolAcceptUDPRequest,
		CommandProtocolReadHeader,
		CommandProtocolReadCommand,
		CommandProtocolAuthenticate,
		CommandProtocolAuthenticateContinue,
		CommandProtocolPostAuthenticate,
		CommandProtocolExecCommand
	} m_state;

	enum CommandProtocolResult {
		CommandProtocolContinue,
		CommandProtocolFinished,
		CommandProtocolInProgress
	};

	Sock   *m_sock;

#ifdef HAVE_EXT_GSOAP
	bool m_is_http_post;
	bool m_is_http_get;
#endif

	bool m_nonblocking;
	bool m_delete_sock;
	bool m_sock_had_no_deadline;
	int	m_is_tcp;
	int m_req;            // the command that was sent
	int	m_reqFound;
	int	m_result;
	int m_perm;
	MyString m_user;
	ClassAd *m_policy;
	ClassAd m_auth_info;
	KeyInfo *m_key;
	char    *m_sid;
	void *m_prev_sock_ent;

	UtcTime m_handle_req_start_time;
	UtcTime m_async_waiting_start_time;
	float m_async_waiting_time;
	SecMan *m_sec_man;
	ExtArray<DaemonCore::CommandEnt> &m_comTable;
	const static std::string WaitForSocketDataString;
	int m_real_cmd;       // for DC_AUTHENTICATE, the final command to execute
	int m_auth_cmd;       // for DC_AUTHENTICATE, the command the security session will be used for
	int m_cmd_index;
	CondorError *m_errstack;

	bool m_new_session;
	SecMan::sec_feat_act m_will_enable_encryption;
	SecMan::sec_feat_act m_will_enable_integrity;

	CommandProtocolResult AcceptTCPRequest();
	CommandProtocolResult AcceptUDPRequest();
	CommandProtocolResult ReadHeader();
	CommandProtocolResult ReadCommand();
	CommandProtocolResult Authenticate();
	CommandProtocolResult AuthenticateContinue();
	CommandProtocolResult AuthenticateFinish(int auth_success, char *method_used);
	CommandProtocolResult PostAuthenticate();
	CommandProtocolResult ExecCommand();
	CommandProtocolResult WaitForSocketData();
	int SocketCallback( Stream *stream );
	int finalize();
};

#endif
