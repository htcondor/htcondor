#ifndef CONDOR_FIX_SIGNAL_H
#define CONDOR_FIX_SIGNAL_H

#if defined(LINUX)
#define SignalHandler _hide_SignalHandler
#endif

#include <signal.h>

#if defined(LINUX)
#undef SignalHandler
#endif

#endif CONDOR_FIX_SIGNAL_H
