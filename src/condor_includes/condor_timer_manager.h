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
// Defines a timer driven by alarm signals that can handle scheduled events as
// well as periodical events. 
//
// Cai, Weiru
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _TIMERMANAGER_H_
#define _TIMERMANAGER_H_

#include "condor_constants.h"
#include <sys/time.h>

const	int		STAR = -1;

class Service
{
};

typedef	int		(*Event)(Service*);
typedef void	(*SIG_HANDLER)();

typedef struct tagTimer {
    unsigned        	when;
	unsigned			period;
    int     			id;
    void*				handler;
	class Service*	   	service; 
    struct tagTimer*	next;
	int					current;
} Timer;

class TimerManager
{
	public:

		TimerManager();
		~TimerManager();

		int		NewTimer(Service*, unsigned, void*, unsigned = 0, int = -1);
		int	   	CancelTimer(int);
		void   	CancelAllTimers();
		void   	DumpTimerList(int, char* = "");
		friend	void	SigalrmHandler();
		#if defined(VOID_SIGNAL_RETURN)
		void
		#else
		int
		#endif
	  			SigalrmHandler(); 
		void	Start();

		friend	class	DaemonCore;

	private:

		Timer* 	timer_list;
		int	   	timer_ids;
};

#endif


























