/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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



