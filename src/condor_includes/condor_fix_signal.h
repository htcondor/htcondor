/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONDOR_FIX_SIGNAL_H
#define CONDOR_FIX_SIGNAL_H

#if defined(LINUX)
#	define SignalHandler _hide_SignalHandler
#endif

#if defined(IRIX62)
#define SIGTRAP 5
#define SIGBUS 10
#define _save_NO_ANSIMODE _NO_ANSIMODE
#undef _NO_ANSIMODE
#define _NO_ANSIMODE 1
#define _save_XOPEN4UX _XOPEN4UX
#undef _XOPEN4UX
#define _XOPEN4UX 1
#endif

#include <signal.h>

#if defined(LINUX)
#undef SignalHandler
#endif

#if defined(IRIX62)
#undef _NO_ANSIMODE
#define _NO_ANSIMODE _save_NO_ANSIMODE
#undef _XOPEN4UX
#define _XOPEN4UX _save_XOPEN4UX
#undef _save_NO_ANSIMODE
#undef _save_XOPEN4UX
#endif

#endif CONDOR_FIX_SIGNAL_H
