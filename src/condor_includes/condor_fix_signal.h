#ifndef CONDOR_FIX_SIGNAL_H
#define CONDOR_FIX_SIGNAL_H

#if defined(LINUX)
#define SignalHandler _hide_SignalHandler
#endif

#if defined(IRIX62)
#define SIGTRAP 5
#define SIGBUS 10
#endif

#include <signal.h>

#if defined(LINUX)
#undef SignalHandler
#endif

#endif CONDOR_FIX_SIGNAL_H
