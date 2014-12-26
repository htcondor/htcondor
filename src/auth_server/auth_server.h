
#ifndef __AUTH_SERVER_H_
#define __AUTH_SERVER_H_

#include "condor_common.h"

#include <string>

#include "classy_counted_ptr.h"

#include "dc_service.h"

#include "auth_server_constants.h"

class Stream;
class KeyInfo;
class ReliSock;
class CondorError;

namespace htcondor {

class AuthServer : public Service {

public:
	AuthServer();

	void Reconfig();
	void CleanUp();
	void TimerCallback();
	int Handler(Stream *);

private:

	class AuthServerState : Service, public ClassyCountedPtr
	{
	public:

		AuthServerState(int id, AuthServer &parent, CondorError *errstack, KeyInfo **key);
		~AuthServerState();
		bool Start(ReliSock *asock);

	private:
		int Handle(Stream *stream);

		AuthServer &m_parent;
		CondorError *m_errstack;
		KeyInfo **m_key;
		int m_id;
	};

	int HandleAuth(Stream *stream);	
	int HandleAuthFinish(int id, ReliSock *asock, CondorError *errstack, std::string const &method_used, int auth_success);

	ReliSock *m_sock;
	unsigned m_keepalives;
	unsigned m_alive_auths;
	char m_stop_code;
	bool m_stop;
	bool m_initialized;
};

}

#endif // __AUTH_SERVER_H_

