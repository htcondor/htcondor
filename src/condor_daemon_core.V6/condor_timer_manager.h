/*
** Copyright 1993 by Michael J. Litzkow, Miron Livny and Jim Pruyne
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
**
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
**
** Author:  Jim Pruyne, Weiru Cai
**              University of Wisconsin, Computer Sciences Dept.
**
*/

////////////////////////////////////////////////////////////////////////////////
//
// Defines a timer driven by select timeouts that can handle scheduled events as
// well as periodical events. 
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _TIMERMANAGER_H_
#define _TIMERMANAGER_H_

#include "condor_constants.h"

#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

const	int		STAR = -1;

class Service {
};

typedef	int		(*Event)(Service*);
typedef int		(Service::*Eventcpp)();

// to make the timer handler stuff similar to the rest of Daemon Core...
#define TimerHandler Event
#define TimerHandlercpp Eventcpp

typedef struct tagTimer {
    time_t	        	when;
	unsigned			period;
    int     			id;
    Event				handler;
	Eventcpp			handlercpp;
	class Service*	   	service; 
    struct tagTimer*	next;
	int					is_cpp;
	char*				event_descrip;
	void*				data_ptr;
} Timer;

class TimerManager
{
	public:

		TimerManager();
		~TimerManager();

		int		NewTimer(Service* s, unsigned deltawhen, Event event, char *event_descrip,
						 unsigned period = 0, int id = -1);
		int		NewTimer(unsigned deltawhen, Event event, char *event_descrip, 
						 unsigned period = 0, int id = -1);
		int		NewTimer(Service* s, unsigned deltawhen, Eventcpp event, char *event_descrip,
						 unsigned period = 0, int id = -1);
		int	   	CancelTimer(int);
		void   	CancelAllTimers();
		void   	DumpTimerList(int, char* = NULL );
	  	int		Timeout(); 
		void	Start();

	private:

		int		NewTimer(Service* s, unsigned deltawhen, Event event, Eventcpp eventcpp, 
							char *event_descrip, unsigned period = 0, int id = -1, 
							int is_cpp = 0);
		Timer* 	timer_list;
		int	   	timer_ids;
		int		in_timeout;
};

#endif


























