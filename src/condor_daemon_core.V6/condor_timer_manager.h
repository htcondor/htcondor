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
/** The base class for all DC - using classes.  
 */
class Service {
public:
		/** This is virtual because the Service class requires one vitual 
			function so that a vtable is made for this class.  If it
			isn't, daemonCore is horribly broken.
		*/			
	virtual ~Service() { };
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
		int		ResetTimer(int tid, unsigned when, unsigned period = 0);
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
		int		did_reset;
};

#endif


























