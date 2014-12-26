
#include "condor_common.h"
#include "condor_daemon_core.h"
#include "daemon_command.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "classad/classad.h"
#include "classad_oldnew.h"

#include "auth_server_client.h"

using namespace htcondor;


AuthServerClient::AuthServerProxy::~AuthServerProxy()
{
	if (m_sock)
	{
		daemonCore->Cancel_Socket(m_sock);
	}
	delete m_sock;
}


AuthServerClient::AuthServerClient()
	:
	m_next_id(1),
	m_rid(-1),
	m_workers(0),
	m_next_server(0),
	m_initialized(false)
{}


bool
AuthServerClient::PassSocket(ReliSock &rsock, DaemonCommandProtocol *prot, const KeyInfo *key, const char *auth_methods, const CondorError &m_errstack, int auth_timeout)
{
	AuthServerProxy *proxy_ptr = PickServer();
	if (!proxy_ptr)
	{
		dprintf(D_FULLDEBUG, "No active authorization servers.\n");
		return false;
	}
	AuthServerProxy &proxy = *proxy_ptr;
	ReliSock &psock = proxy.Sock();
	int fd = rsock.get_file_desc();

	// TODO: check error codes.
	classad::ClassAd ad;
	ad.InsertAttr("Id", m_next_id);
	ad.InsertAttr("AuthMethods", auth_methods);
	ad.InsertAttr("AuthTimeout", auth_timeout);
	if (key) {key->serialize(ad);}
	m_errstack.serialize(ad);

	psock.encode();
	int mycode = AUTH_SERVER_HANDLE;
	if (!psock.code(mycode))
	{
		proxy.ReleaseSock();
		dprintf(D_ALWAYS, "Failed to write code to child auth server.\n");
		return false;
	}
	if (!putClassAd(&psock, ad))
	{
		proxy.ReleaseSock();
		dprintf(D_ALWAYS, "Failed to send serialization classad to child auth server.\n");
		return false;
	}
	if (!psock.code_fd(fd))
	{
		proxy.ReleaseSock();
		dprintf(D_ALWAYS, "Failed to send FD to child auth server.\n");
		return false;
	}
	if (!psock.end_of_message())
	{
		proxy.ReleaseSock();
		dprintf(D_ALWAYS, "Failed to send EOM to child auth server for socket passing.\n");
		return false;
	}

	prot->incRefCount();
	m_handlers[m_next_id++] = prot;
	return true;
}


AuthServerClient::AuthServerProxy *
AuthServerClient::PickServer()
{
	unsigned tried = m_servers.size();
	for (unsigned idx=0; idx<tried; idx++)
	{
		unsigned pick = m_next_server++;
		if (pick >= m_servers.size())
		{
			pick = 0;
			m_next_server = 1;
		}
		AuthServerProxy *proxy = m_servers[pick].get();
		if (proxy && proxy->Valid())
		{
			return proxy;
		}
	}
	return NULL;
}


void
AuthServerClient::Reconfig()
{
	if (!m_initialized)
	{
		if (-1 == (m_rid = daemonCore->Register_Reaper("Auth server reaper", static_cast<ReaperHandlercpp>(&AuthServerClient::ReapServer), "Auth server handler", this)))
		{
			dprintf(D_ALWAYS, "Failed to register a reaper for the auth server.\n");
		}
		m_initialized = true;
		if (-1 == daemonCore->Register_Timer(0, 5, static_cast<TimerHandlercpp>(&AuthServerClient::TimerCallback), "Alive handler", this))
		{
			dprintf(D_ALWAYS, "Failed to register timer for auth server keepalive handler.");
		}
		else {dprintf(D_FULLDEBUG, "Registered auth server keepalive timer.\n");}
	}
	for (ProxyList::const_iterator it=m_servers.begin(); it!=m_servers.end(); it++)
	{
		if (-1 == kill((*it)->Pid(), SIGHUP))
		{
			dprintf(D_FULLDEBUG, "Failed to send reconfig to child auth server: %d (errno=%d, %s).\n", (*it)->Pid(), errno, strerror(errno));
		}
	}
	int workers = param_integer("AUTH_SERVER_WORKERS");
	LaunchWorkers(workers);
}

void
AuthServerClient::LaunchWorkers(int workers)
{
	int cur_workers = static_cast<int>(m_workers);
	if (static_cast<unsigned>(cur_workers) != m_servers.size())
	{
		cur_workers = m_servers.size();
	}
	if (cur_workers != workers)
	{
		dprintf(D_FULLDEBUG, "Desired workers: %d; current workers: %d.\n", workers, cur_workers);
	}
	if (workers - cur_workers > 0)
	{
		for (unsigned idx=0; idx<workers-static_cast<unsigned>(cur_workers); idx++)
		{
			LaunchServer();
		}
	}
	else if (workers < cur_workers)
	{
		for (unsigned idx=0; idx<static_cast<unsigned>(cur_workers)-workers; idx++)
		{
			m_servers[idx]->SetStop();
		}
	}
	m_workers = workers;
}


int
AuthServerClient::ReapServer(int pid, int status)
{
	if (WIFEXITED(status) && WEXITSTATUS(status))
	{
		dprintf(D_ALWAYS, "Auth server at PID %d failed with %d.\n", pid, WEXITSTATUS(status));
	}
	else if (WIFSIGNALED(status))
	{
		dprintf(D_ALWAYS, "Auth server at PID %d failed with signal %d.\n", pid, WTERMSIG(status));
	}
	bool found_proxy = false;
	for (ProxyList::iterator it=m_servers.begin(); it!=m_servers.end(); it++)
	{
		if ((*it)->Pid() == pid)
		{
			found_proxy = true;
			m_servers.erase(it);
			break;
		}
	}
	dprintf(D_FULLDEBUG, "Auth server exited; there are now %lu running.\n", m_servers.size());
	if (!found_proxy)
	{
		dprintf(D_ALWAYS, "Reaped PID %d, but not a known auth server.\n", pid);
	}
	LaunchWorkers(m_workers);
	return 0;
}


void
AuthServerClient::Stop()
{
	LaunchWorkers(0);
}


void
AuthServerClient::TimerCallback()
{
	dprintf(D_VERBOSE | D_FULLDEBUG, "Sending keepalive for auth servers.\n");
	for (ProxyList::iterator it=m_servers.begin(); it!=m_servers.end(); it++)
	{
		if (!(*it)->Valid()) {dprintf(D_FULLDEBUG, "Skipping invalid auth server %d.\n", (*it)->Pid()); continue;}
		ReliSock &sock = (*it)->Sock();
		sock.encode();
		int mycode;
		if ((*it)->Stopped())
		{
			// Log to FULLDEBUG as these servers are stopping anyway - maybe the reaper just hasn't fired?
			mycode = AUTH_SERVER_STOP;
			if (!sock.code(mycode)) {dprintf(D_FULLDEBUG, "Failed to send stop signal to auth server.\n"); continue;};
			if (!sock.end_of_message()) {dprintf(D_FULLDEBUG, "Failed to send stop EOM to auth server.\n"); continue;}
		}
		else
		{
			mycode = AUTH_SERVER_ALIVE;
			if (!sock.code(mycode)) {dprintf(D_ALWAYS, "Failed to send alive to auth server.\n"); (*it)->SetStop(); continue;}
			if (!sock.end_of_message()) {dprintf(D_ALWAYS, "Failed to send alive EOM to auth server.\n"); (*it)->SetStop(); continue;}
		}
	}
}


bool
AuthServerClient::LaunchServer()
{
	ReliSock *mysock = new ReliSock();
	ReliSock childsock; // This is on the stack - close when function exits
	if (!mysock->unix_socketpair(childsock))
	{
		dprintf(D_ALWAYS, "Failed to create sockets for auth server communication.\n");
		return false;
	}

	std::string exec_name;
	if (!param(exec_name, "AUTH_SERVER"))
	{
		dprintf(D_ALWAYS, "Unable to determine auth server's location.\n");
		return false;
	}
	SubsystemInfo* subsys = get_mySubSystem();
	ArgList args;
	args.AppendArg("condor_auth_server");
	args.AppendArg("-f");
	args.AppendArg("-subsys");
	args.AppendArg(subsys->getName());
	args.AppendArg("-type");
	args.AppendArg(subsys->getTypeName());

	Stream *inheritList[] = {&childsock, NULL};

	// NOTE: Sadly enough, we launch as PRIV_ROOT so we can read the hostcert.
	pid_t childpid = daemonCore->Create_Process(exec_name.c_str(), args, PRIV_ROOT, m_rid, false, false, NULL, NULL, NULL, inheritList);
	if (childpid == -1)
	{
		dprintf(D_ALWAYS, "Failed to launch the auth server.\n");
		return false;
	}
	else 
	{
		dprintf(D_FULLDEBUG, "Successfully launched auth process %d.\n", childpid);
	}

	if (-1 == daemonCore->Register_Socket(mysock, "Child auth socket",
		static_cast<SocketHandlercpp>(&AuthServerClient::Handler), "Authentication callback handler",
		this))
	{
		dprintf(D_ALWAYS, "Failed to register child auth socket.\n");
		delete mysock;
		return false;
	}


	classad_shared_ptr<AuthServerProxy> proxy(new AuthServerProxy(mysock, childpid));
	m_servers.push_back(proxy);
	return true;
}


bool
AuthServerClient::releaseSock(Stream *stream)
{
	for (ProxyList::iterator it=m_servers.begin(); it!=m_servers.end(); it++)
	{
		if (stream == &((*it)->Sock()))
		{
			(*it)->ReleaseSock();
			return true;
		}
	}
	return false;
}

int
AuthServerClient::Handler(Stream * stream)
{
	stream->decode();
	int mycode;
	if (!stream->code(mycode) || !stream->end_of_message())
	{
		releaseSock(stream);
		dprintf(D_ALWAYS, "Failed to read from auth server.\n");
		// Note we *always* KEEP_STREAM, even on error; this is because
		// there may be outstanding callbacks..
		return KEEP_STREAM;
	}
	AuthServerCodes code = static_cast<AuthServerCodes>(mycode);
	if (code != AUTH_SERVER_HANDLE)
	{
		releaseSock(stream);
		dprintf(D_ALWAYS, "Unknown auth server code.\n");
		return KEEP_STREAM;
	}

	int fd = -1;
	std::string method_used, socket_serialized;
	ReliSock &psock = *static_cast<ReliSock *>(stream);

	classad::ClassAd *ad_ptr = NULL;
	if (!(ad_ptr=getClassAd(&psock)))
	{
		dprintf(D_ALWAYS, "Failed to get response ad from auth server.\n");
		return KEEP_STREAM;
	}
	classad_shared_ptr<classad::ClassAd> ad_safe(ad_ptr);
	classad::ClassAd &ad = *ad_ptr;
	if (!ad.EvaluateAttrString("Socket", socket_serialized))
	{
		dprintf(D_FULLDEBUG, "Response ad has no serialized socket info.\n");
	}
	CondorError *errstack = NULL;
	if (ad.Lookup("CondorError"))
	{
		errstack = new CondorError(ad);
	}
	int auth_result = 1;
	ad.EvaluateAttrInt("Status", auth_result);
	ad.EvaluateAttrString("MethodUsed", method_used);

	IdType id;
	if (!ad.EvaluateAttrInt("Id", id))
	{
		releaseSock(stream);
		dprintf(D_ALWAYS, "Failed to get transaction ID from auth server.\n");
		delete errstack;
		return KEEP_STREAM;
	}

	if (!psock.code_fd(fd))
	{
		releaseSock(stream);
		dprintf(D_ALWAYS, "Failed to get file descriptor from auth server.\n");
		delete errstack;
		return KEEP_STREAM;
	}
	if (!psock.end_of_message())
	{
		releaseSock(stream);
		dprintf(D_ALWAYS, "Failed to get file descriptor EOM from auth server.\n");
		delete errstack;
		return KEEP_STREAM;
	}
	ReliSock *rsock = new ReliSock();
	if (!rsock->assignConnectedSocket(fd))
	{
		dprintf(D_ALWAYS, "Failed to assigned socket from child.\n");
		delete rsock;
		delete errstack;
		return KEEP_STREAM;
	}
		// Note: this function ASSERTs on error.
	rsock->serialize(socket_serialized.c_str());

	HandlerHashType::iterator it = m_handlers.find(id);
	DaemonCommandProtocol *handler = NULL;
	if (it == m_handlers.end())
	{
		dprintf(D_ALWAYS, "Callback invoked for a handler we don't recognize.\n");
		delete rsock;
		delete errstack;
		return KEEP_STREAM;
	}
	else
	{
		handler = it->second;
		m_handlers.erase(it);
	}
	if (handler)
	{
		// Handler takes ownership of ReliSock and errstack.
		handler->FinishExternalAuth(auth_result, method_used, rsock, errstack);
	}
	else
	{
		delete rsock;
		delete errstack;
	}

	return KEEP_STREAM;
}

