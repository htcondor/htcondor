#ifndef CONDOR_FIX_SIGNAL_H
#define CONDOR_FIX_SIGNAL_H

#if defined(LINUX)
#	define SignalHandler _hide_SignalHandler
#endif

#if defined(IRIX62)
#	define SIGTRAP 5
#	define SIGBUS 10
#	define _save_XOPEN4UX _XOPEN4UX
#	undef _XOPEN4UX
#	define _XOPEN4UX 1
#endif

#include <signal.h>

#if defined(LINUX)
#	undef SignalHandler
#endif

#if defined(IRIX62)
#   undef _XOPEN4UX
#   define _XOPEN4UX _save_XOPEN4UX
#   undef _save_XOPEN4UX
#endif

#endif CONDOR_FIX_SIGNAL_H
