#include "condor_common.h"
#include "condor_timer_manager.h"
#include "condor_daemon_core.h"

template class HashTable<pid_t, DaemonCore::PidEntry*>;
