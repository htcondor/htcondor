#define _POSIX_SOURCE

#ifndef __XINTERFACE_H__
#define __XINTERFACE_H__

#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

#include <time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "condor_debug.h"

class XInterface
{
  public:
    XInterface();
    ~XInterface();

    
    bool CheckActivity();
  private:
    bool ProcessEvents();
    void SelectEvents(Window win);
    bool QueryPointer();
    bool Connect();
    int NextEntry();

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
};


#endif //__XINTERFACE_H__



