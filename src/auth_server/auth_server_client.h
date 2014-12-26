#ifndef __AUTH_SERVER_CLIENT_H_
#define __AUTH_SERVER_CLIENT_H_

#include <vector>

#include "classad/classad_stl.h"
#include "dc_service.h"

#include "auth_server_constants.h"

class ReliSock;
class Stream;
class DaemonCommandProtocol;
class KeyInfo;
class CondorError;

namespace htcondor
{

class AuthServerClient : public Service {

public:
	AuthServerClient();

	void Reconfig();

	void TimerCallback();

	bool PassSocket(ReliSock &rsock, DaemonCommandProtocol *prot, const KeyInfo *key, const char *auth_methods, const CondorError &m_errstack, int auth_timeout);

	int HandleAuthDone(Stream *stream);

	int ReapServer(int pid, int status);

	void Stop();

private:

	class AuthServerProxy {

	public:
		AuthServerProxy(ReliSock *sock, pid_t pid)
		  : m_sock(sock), m_pid(pid), m_stopped(false)
		{}

		~AuthServerProxy();

		ReliSock &Sock() const {return *m_sock;}
		void ReleaseSock() {m_sock=NULL; SetStop();}
		pid_t Pid() const {return m_pid;}
		void SetStop() {m_stopped=true;}
		bool Valid() const {return m_sock;}
		bool Stopped() const {return m_stopped;}

	private:
		ReliSock *m_sock;
		pid_t m_pid;
		bool m_stopped;
	};

	int Handler(Stream *);

	void LaunchWorkers(int workers);
	bool LaunchServer();
	AuthServerProxy *PickServer();
	bool releaseSock(Stream *stream);
	typedef std::vector<classad_shared_ptr<AuthServerProxy> > ProxyList;
	ProxyList m_servers;

	typedef int IdType;
	typedef classad_unordered<IdType, DaemonCommandProtocol*> HandlerHashType;
	HandlerHashType m_handlers;
	IdType m_next_id;
	int m_rid;
	unsigned m_workers;
	unsigned m_next_server;
	bool m_initialized;

};

}

#endif
