
#ifndef __SYSTEMD_MANAGER_H_
#define __SYSTEMD_MANAGER_H_

#include "condor_header_features.h"

#include <string>
#include <vector>

namespace condor_utils
{

class SystemdManager
{
public:
	static const SystemdManager &GetInstance();

	bool PrepareForExec() const;
	int Notify(const char *fmt, ... ) const /*CHECK_PRINTF_FORMAT(1,2)*/;
	const std::vector<int> &GetFDs() const {return m_inet_fds;};
	int GetWatchdogUsecs() const {return m_watchdog_usecs;};

private:
	SystemdManager();
	~SystemdManager();

	void * GetHandle(const std::string &name);
	void InitializeFDs();

	static SystemdManager *m_singleton;

	typedef int (*notify_handle_t)(int unset_environment, const char *state);
	typedef int (*listen_fds_t)(int unset_environment);
	typedef int (*is_socket_t)(int fd, int family, int type, int listening);

	int m_watchdog_usecs;
	bool m_need_restart;
	void *m_handle;
	notify_handle_t m_notify_handle;
	listen_fds_t m_listen_fds_handle;
	is_socket_t m_is_socket_handle;
	std::string m_notify_socket;
	std::vector<int> m_inet_fds;
};

}

#endif
