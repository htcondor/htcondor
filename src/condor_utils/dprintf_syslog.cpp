
#include "condor_common.h"
#include "condor_debug.h"
#include "dprintf_syslog.h"

DprintfSyslogFactory * DprintfSyslogFactory::m_singleton = NULL;

void
DprintfSyslog::Log(const char * message)
{
	syslog(LOG_INFO, "%s", message);
}

DprintfSyslog::~DprintfSyslog()
{
	DprintfSyslogFactory &factory = DprintfSyslogFactory::getInstance();
	factory.DecCount();
}

