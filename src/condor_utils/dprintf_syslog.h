
#include "dprintf_internal.h"
#include <syslog.h>

class DprintfSyslogFactory;

class DprintfSyslog
{
	friend class DprintfSyslogFactory;

public:
	static void Log(int, int, time_t, struct tm*, const char * message, DebugFileInfo* info)
	{
		if (!info || !info->userData)
		{
			return;
		}
		DprintfSyslog * logger = static_cast<DprintfSyslog*>(info->userData);
		logger->Log(message);
	}

	~DprintfSyslog();

protected:
	DprintfSyslog() {}

private:
	void Log(const char *);
};

class DprintfSyslogFactory
{
	friend class DprintfSyslog;

public:
	static DprintfSyslog *NewLog(int facility)
	{
		DprintfSyslogFactory & factory = getInstance();
		return factory.NewDprintfSyslog(facility);
	}

protected:
	void DecCount()
	{
		m_count--;
		if (m_count == 0)
		{
			closelog();
		}
	}

	static DprintfSyslogFactory & getInstance()
	{
		if (!m_singleton)
		{
			m_singleton = new DprintfSyslogFactory();
		}
		return *m_singleton;
	}

private:
	DprintfSyslog * NewDprintfSyslog(int facility)
	{
		DprintfSyslog * logger = new DprintfSyslog();
		if (!logger) return NULL;
		if (m_count == 0)
		{
			openlog(NULL, LOG_PID|LOG_NDELAY, facility);
		}
		m_count++;
		return logger;
	}

	DprintfSyslogFactory() :
		m_count(0)
	{
	}

	static DprintfSyslogFactory *m_singleton;

	unsigned int m_count;
};
