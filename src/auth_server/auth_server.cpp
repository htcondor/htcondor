
#include "auth_server.h"

#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"

using namespace htcondor;

AuthServer::AuthServer()
	:
	m_keepalives(0),
	m_alive_auths(0),
	m_stop_code(0),
	m_stop(false),
	m_initialized(false)
{}


void
AuthServer::Reconfig()
{
	dprintf(D_FULLDEBUG, "Reconfiguring auth server at PID %d.\n", getpid());
	if (!m_initialized)
	{
		Stream **streams = daemonCore->GetInheritedSocks();
		Stream *stream;
		unsigned idx = 0;
		while ( (stream = streams[idx++]) )
		{
			if (stream->type() != Stream::reli_sock) {continue;}
			daemonCore->Register_Socket(stream, "Parent auth socket",
				static_cast<SocketHandlercpp>(&AuthServer::Handler), "Authentication handler",
				this);
			m_sock = static_cast<ReliSock*>(stream);
		}
		if (idx != 2)
		{
			EXCEPT("Unexpected number of sockets inherited (%d).", idx);
		}
		if (-1 == daemonCore->Register_Timer(20, 20, static_cast<TimerHandlercpp>(&AuthServer::TimerCallback), "Alive handler", this))
		{
			EXCEPT("Failed to register timer for keepalive handler.");
		}
		m_initialized = true;
	}
}


int
AuthServer::Handler(Stream *stream)
{
	dprintf(D_VERBOSE | D_FULLDEBUG, "Got message for auth server.\n");
	stream->decode();
	int mycode;
	if (!stream->code(mycode))
	{
		dprintf(D_ALWAYS, "Error when reading from parent socket; auth server will stop.\n");
		m_stop = true;
		return false;
	}
	AuthServerCodes authcode = static_cast<AuthServerCodes>(mycode);
	switch (authcode)
	{
	case AUTH_SERVER_ALIVE:
		m_keepalives++;
		break;
	case AUTH_SERVER_STOP:
		m_stop = true;
		break;
	case AUTH_SERVER_HANDLE:
		return HandleAuth(stream);
	default:
		EXCEPT("Unknown auth server code: %d.", authcode);
	};
	if (!stream->end_of_message())
	{
		dprintf(D_ALWAYS, "Error when reading EOM from parent socket; auth server will stop.\n");
		m_stop = true;
		return false;
	}
	return KEEP_STREAM;
}


int
AuthServer::HandleAuth(Stream *stream)
{
	if (stream->type() != Stream::reli_sock)
	{
		dprintf(D_ALWAYS, "Callback on a non-relisock.\n");
		return false;
	}
	ReliSock *rsock = static_cast<ReliSock*>(stream);
	rsock->decode();
	int fd;
	classad::ClassAd *ad_ptr;
	if (!(ad_ptr = getClassAd(rsock)))
	{
		dprintf(D_ALWAYS, "Error when receiving ad from parent.\n");
		m_stop = true;
		return false;
	}
	classad_shared_ptr<classad::ClassAd> ad_safe(ad_ptr);
	classad::ClassAd &ad = *ad_ptr;
	if (!rsock->code_fd(fd))
	{
		dprintf(D_ALWAYS, "Error when receiving FD from parent.\n");
		m_stop = true;
		return false;
	}
	if (!rsock->end_of_message())
	{
		dprintf(D_ALWAYS, "Error when receiving EOM for FD from parent.\n");
		m_stop = true;
		return false;
	}

	int id; ad.EvaluateAttrInt("Id", id);
	std::string auth_methods; ad.EvaluateAttrString("AuthMethods", auth_methods);
	int auth_timeout; ad.EvaluateAttrInt("AuthTimeout", auth_timeout);
	KeyInfo **key = NULL; key = new KeyInfo*;
	if (ad.Lookup("KeyInfoProtocol"))
	{
		*key = new KeyInfo(ad);
		dprintf(D_SECURITY, "Auth server got session key of length %d\n", (*key)->getKeyLength());
	}
	CondorError *errstack = new CondorError(ad);

	ReliSock *asock = new ReliSock();
	asock->assignConnectedSocket(fd);

	char *method_used_char = NULL;
	std::string method_used;
	int auth_success = asock->authenticate(*key, auth_methods.c_str(), errstack, auth_timeout, true, &method_used_char);
	if (method_used_char)
	{
		method_used = method_used_char;
	}
	delete method_used_char;

	m_alive_auths++;
	if (auth_success == 2)
	{
		classy_counted_ptr<AuthServerState> r = new AuthServerState(id, *this, errstack, key);
		if (!r->Start(asock))
		{
			if (errstack) {errstack->push("AUTHENTICATE", 1, "Unable to create auth server state handler.");}
			HandleAuthFinish(id, asock, errstack, "", 1);
		}
	}
	else
	{
		if (key) {delete *key;}
		delete key;
		HandleAuthFinish(id, asock, errstack, method_used, auth_success);
	}

	return KEEP_STREAM;
}

int
AuthServer::HandleAuthFinish(int id, ReliSock *asock, CondorError *errstack, std::string const &method_used, int auth_success)
{
	m_alive_auths--;
	m_sock->encode();
	AuthServerCodes code = AUTH_SERVER_HANDLE;
	int mycode = static_cast<int>(code);
	if (!m_sock->code(mycode) || !m_sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Error when sending response code to parent.\n");
		m_stop = true;
		delete errstack;
		return false;
	}
	if (daemonCore->SocketIsRegistered(asock))
	{
		daemonCore->Cancel_Socket(asock);
	}
	classad::ClassAd ad;
	if (method_used.size())
	{
		ad.InsertAttr("MethodUsed", method_used);
	}
	ad.InsertAttr("Status", auth_success);
	ad.InsertAttr("Id", id);
	if (errstack) {errstack->serialize(ad);}
	delete errstack;
	char *serialized_asock = asock->serialize();
	if (serialized_asock)
	{
		ad.InsertAttr("Socket", serialized_asock);
		delete serialized_asock;
	}
	if (!putClassAd(m_sock, ad))
	{
		dprintf(D_ALWAYS, "Error when sending response ad to parent.\n");
		m_stop = true;
		delete asock;
		return false;
	}
	if (!m_sock->put_fd(asock->get_file_desc()))
	{
		dprintf(D_ALWAYS, "Error when sending FD back to parent.\n");
		m_stop = true;
		delete asock;
		return false;
	}
	delete asock;
	if (!m_sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Error when writing EOM to parent socket; auth server will stop.\n");
		m_stop = true;
		return false;
	}
	return KEEP_STREAM;
}


void
AuthServer::TimerCallback()
{
	if ((m_alive_auths == 0) && (m_stop))
	{
		DC_Exit(m_stop_code);
	}
	if (m_keepalives == 0)
	{
		dprintf(D_ALWAYS, "Auth server will exit due to lack of keepalives.\n");
		m_stop = true;
		m_stop_code = 1;
	}
	m_keepalives = 0;
}


void
AuthServer::CleanUp()
{
}

AuthServer::AuthServerState::AuthServerState(int id, AuthServer &parent, CondorError *errstack, KeyInfo **key) :
	m_parent(parent),
	m_errstack(errstack),
	m_key(key),
	m_id(id)
{
}

AuthServer::AuthServerState::~AuthServerState()
{
	if (m_key) {delete *m_key;}
	delete m_key;
}

bool
AuthServer::AuthServerState::Start(ReliSock *sock)
{
	if (sock->get_deadline() == 0) {
		int TCP_SESSION_DEADLINE = param_integer("SEC_TCP_SESSION_DEADLINE",120);
		sock->set_deadline_timeout(TCP_SESSION_DEADLINE);
	}

	if (-1 == daemonCore->Register_Socket(sock, "Remote authentication socket",
		static_cast<SocketHandlercpp>(&AuthServer::AuthServerState::Handle), "Asynchronous authentication handler callback",
		this))
	{
		dprintf(D_ALWAYS, "Failed to register remote authentication socket.\n");
		return false;
	}
	incRefCount();
	return true;
}

int
AuthServer::AuthServerState::Handle(Stream *stream)
{
	if (!stream || (stream->type() != Stream::reli_sock)) {EXCEPT("Callback for unknown socket.");}

	ReliSock &rsock = *static_cast<ReliSock*>(stream);
	if (rsock.deadline_expired())
	{
		dprintf(D_ALWAYS, "AuthServer: deadline for security handshake with %s has expired.\n", rsock.peer_description());
		m_parent.HandleAuthFinish(m_id, &rsock, m_errstack, "", 1);
		decRefCount();
		return false;
	}

	char *method_used = NULL;
	int auth_result = rsock.authenticate_continue(m_errstack, true, &method_used);
	if (auth_result == 2)
	{
		return KEEP_STREAM;
	}
	std::string method_used_str;
	if (method_used) {method_used_str = method_used;}
	int retval = m_parent.HandleAuthFinish(m_id, &rsock, m_errstack, method_used_str, auth_result);
	decRefCount();
	return retval;
}

