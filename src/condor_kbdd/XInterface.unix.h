/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef __XINTERFACE_H__
#define __XINTERFACE_H__
#include "condor_common.h"
#include "condor_daemon_core.h"
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

    void SetBumpCheck(int delta_move, int delta_time);

    bool CheckActivity();
  private:
    bool ProcessEvents();
    void SelectEvents(Window win);
    bool QueryPointer();
	bool QuerySSExtension();
    bool Connect();
    bool TryUser(const char *user);

	void ReadUtmp();
	void FinishConnection();

    Display     *_display;
    char*       _display_name;
    time_t      _last_event;

    Window       _pointer_root;
    Screen       *_pointer_screen;
    unsigned int _pointer_prev_mask;
    int          _pointer_prev_x;
    int          _pointer_prev_y;

    int          _small_move_delta;
    int          _bump_check_after_idle_time_sec;

    int         _daemon_core_timer;
    bool 	needsCheck;
    bool 	hasXss; 

	// The following are for getting rid of the obnoxious utmp stuff.
	FILE * utmp_fp;
	std::vector<char *> *logged_on_users;
};
#endif //__XINTERFACE_H__
