
#include "condor_common.h"
#include "condor_config.h"

#ifdef HAVE_SD_DAEMON_H
#include "systemd/sd-daemon.h"
#endif

#ifdef LINUX
#ifndef SD_LISTEN_FDS_START
#define SD_LISTEN_FDS_START 3
#endif

#ifndef LIBSYSTEMD_DAEMON_SO
#define LIBSYSTEMD_DAEMON_SO "libsystemd.so.0"
#endif
#endif /* LINUX */

#ifdef UNIX
#include <dlfcn.h>
#endif

#include "systemd_manager.h"

#include "condor_debug.h"
#include "stl_string_utils.h"

using namespace condor_utils;

SystemdManager *SystemdManager::m_singleton = NULL;

SystemdManager::~SystemdManager()
{
	if (m_handle)
	{
#ifdef UNIX
		dlclose(m_handle);
#endif
	}
}

SystemdManager::SystemdManager()
	: m_watchdog_usecs(0),
	  m_need_restart(false),
	  m_handle(NULL),
	  m_notify_handle(NULL),
	  m_listen_fds_handle(NULL),
	  m_is_socket_handle(NULL)
{
#ifdef LINUX
	const char *tmp_val = getenv("NOTIFY_SOCKET");
	m_notify_socket = tmp_val ? tmp_val : "";
	if (!m_notify_socket.empty() && (tmp_val = getenv("WATCHDOG_USEC")))
	{
		YourStringDeserializer tmp(tmp_val);
		if ( ! tmp.deserialize_int(&m_watchdog_usecs))
		{
			m_watchdog_usecs = 1'000'000;
			dprintf(D_ALWAYS, "Unable to parse watchdog interval from systemd; assuming 1s\n");
		}
	}

#ifdef UNIX
	dlerror();
#ifdef LIBSYSTEMD_DAEMON_SO
	m_handle = dlopen(LIBSYSTEMD_DAEMON_SO, RTLD_NOW|RTLD_LOCAL);
#else
        m_handle = NULL;
#endif
	const char *error;
	if (m_handle == NULL)
	{
		if ((error = dlerror()))
		{
			dprintf(D_FULLDEBUG, "systemd integration unavailable: %s.\n", error);
		}
		return;
	}
#endif
	m_notify_handle = (notify_handle_t)GetHandle("sd_notify");
	m_listen_fds_handle = (listen_fds_t)GetHandle("sd_listen_fds");
	m_is_socket_handle = (is_socket_t)GetHandle("sd_is_socket");

	InitializeFDs();
#endif // LINUX
}

const SystemdManager &
SystemdManager::GetInstance()
{
	if (!m_singleton) { m_singleton = new SystemdManager(); }
	return *m_singleton;
}


void
SystemdManager::InitializeFDs()
{
	if (!m_listen_fds_handle || !m_is_socket_handle) { return; }

	int fds = (*m_listen_fds_handle)(1);
	if (fds < 0)
	{
		EXCEPT("Failed to retrieve sockets from systemd");
	}
	if (fds == 0)
	{
#ifdef LINUX
		dprintf(D_FULLDEBUG, "No sockets passed from systemd\n");
#endif
	}
	else
	{
		dprintf(D_FULLDEBUG, "systemd passed %d sockets.\n", fds);
		m_need_restart = true;
	}
#ifdef SD_LISTEN_FDS_START
	for (int fd=SD_LISTEN_FDS_START; fd<SD_LISTEN_FDS_START+fds; fd++) {
		if ((*m_is_socket_handle)(fd, 0, SOCK_STREAM, 1)) { m_inet_fds.push_back(fd); }
	}
#endif
}

// Setup systemd-related process state prior to a call to exec(),
// so that the new process image sees the same systemd configuration
// this process started with.
// This assumes this object is the only thing disturbing the systemd
// process state (environment variables and inherited FDs).
// If that can't be done, return false.
bool
SystemdManager::PrepareForExec() const
{
	if ( m_need_restart ) {
		return false;
	}
#ifdef LINUX
	if ( !m_notify_socket.empty() ) {
		setenv( "NOTIFY_SOCKET", m_notify_socket.c_str(), 1 );
	}
#endif
	return true;
}

void *
SystemdManager::GetHandle(const std::string &name)
{
#ifdef UNIX
	if (!m_handle) {return NULL;}
	void *handle = NULL;
	dlerror();
	const char *error;
	if (!(handle = dlsym(m_handle, name.c_str())) && (error = dlerror()))
	{
		dprintf(D_ALWAYS, "systemd integration available but %s missing: %s.\n", name.c_str(), error);
	}
	return handle;
#else
	return NULL;
#endif
}

int
SystemdManager::Notify(const char *fmt, ... ) const
{
	if (m_notify_handle == NULL || m_watchdog_usecs == 0)
	{
		return 0;
	}
	std::string tmp_str;
	va_list argptr;
	va_start(argptr, fmt);
	vformatstr(tmp_str, fmt, argptr);
	va_end(argptr);

#ifdef LINUX
	setenv("NOTIFY_SOCKET", m_notify_socket.c_str(), 1);
#endif
	return (*m_notify_handle)(1, tmp_str.c_str());
}

