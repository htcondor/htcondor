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
#define _POSIX_SOURCE

#ifndef __XINTERFACE_H__
#define __XINTERFACE_H__
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

class XInterface
{
  public:
    XInterface(int id);
    ~XInterface();

    
    bool CheckActivity();
  private:
    bool ProcessEvents();
    void SelectEvents(Window win);
    bool QueryPointer();
    bool Connect();
    int NextEntry();
    void TryUser(const char *user);

    Display     *_display;
    Window      _window;
    int         _screen;
    time_t      _last_event;

    Window       _pointer_root;
    Screen       *_pointer_screen;
    unsigned int _pointer_prev_mask;
    int          _pointer_prev_x;
    int          _pointer_prev_y;

    bool        _tried_root;
    bool        _tried_utmp;
    int         _daemon_core_timer;

    StringList  *_xauth_users;

	// The following are for getting rid of the obnoxious utmp stuff.
	FILE * utmp_fp;
	ExtArray<char *> *logged_on_users;
};

#if defined(DUX)
#	if USES_UTMPX
		static char *UtmpName = "/var/adm/utmpx";
		static char *AltUtmpName = "/etc/utmpx";
#	else
		static char *UtmpName = "/var/run/utmp";
		static char *AltUtmpName = "/var/adm/utmp";
#	endif
#elif defined(LINUX)
	static char *UtmpName = "/var/run/utmpx";
	static char *AltUtmpName = "/var/adm/utmpx";
#else
	static char *UtmpName = "/etc/utmpx";
	static char *AltUtmpName = "/var/adm/utmpx";
#endif

#endif //__XINTERFACE_H__



