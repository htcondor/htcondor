/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
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
** Author:  Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*****************************************************************
**
** Maintain a list of events which are scheduled to occur on a periodic
** basis.  Events are represented by a function which will be executed at
** the time(s) when the event is scheduled to occur.  The specification of
** when the event is to occur is a 5-tuple, (month,day,hour,mintue,second)
** which is represented as an array of integers.  Zero or more elements in
** the tuple may be specified as STAR which serves as a wildcard in the
** manner of cron(1).  Star is represented by -1 in the integer array.
** The application should call event_mgr() regularly at times when it
** would be convenient to service an event.  At each call to event_mgr()
** any events which were scheduled to occur between the last call to
** event_mgr() and the current time will be executed.  This package does
** not use any timers or signals, and does not mess with SIGALRM.
** Exports:
** 	schedule_event( month, day, hour, minute, second, func )
** 	int		month, day, hour, minute, second;
** 	int 	(*func)();
** 
** 	event_mgr()
** 
** Example:
** 
** 	Assume two functions chime() which plays a simple tune and ring()
** 	which rings the bell a requisite number of times for the current hour
** 	of the day.  Then a clock which chimes on the quarter hour and rings
** 	on the hour can be specified as follows.  Note: this is not the way
** 	to implement this program in real life.
** 
** 	schedule_event( STAR, STAR, STAR, 0, 0, chime );
** 	schedule_event( STAR, STAR, STAR, 15, 0, chime );
** 	schedule_event( STAR, STAR, STAR, 30, 0, chime );
** 	schedule_event( STAR, STAR, STAR, 45, 0, chime );
** 	schedule_event( STAR, STAR, STAR, 0, 0, ring );
** 	schedule_event( 12, 31, 23, 59, 45, ring );		-- Special celebration
** 	schedule_event( 12, 31, 23, 59, 50, ring );		-- for New Year's Eve
** 	schedule_event( 12, 31, 23, 59, 55, ring );
** 
** 	for(;;) {
** 		event_mgr();
** 		sleep( 1 );
** 	}
**
*****************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#define TRUE 1
#define FALSE 0

/*
** Represent a moment in time by a 5 element tuple
** (month,day,hour,minute,second)
*/
#define MONTH	0
#define DAY		1
#define HOUR	2
#define MINUTE	3
#define SECOND	4
#define N_ELEM	5

typedef void (*FUNC_P)();

/*
** Events are to occur at times which match a pattern.  Patterns are
** represented by the same tuples, but may contain "*" as one or more
** elements.  E.g. (*,*,*,0,0) meaning every hour on the hour.
*/
#define STAR -1
typedef struct {
	int		pattern[N_ELEM];
	FUNC_P	func;
} EVENT;

static EVENT	Schedule[128];
static int	N_Events = 0;
static int	Initialized = FALSE;
static int	prev[N_ELEM];
static int	now[N_ELEM];


int schedule_event( int , int , int , int , int , FUNC_P );
int event_mgr();
static int event_due( int pattern[], int prev[], int now[] );
static int before( int t1[], int t2[] );
static int next_rightmost( int pattern[], int i );
static int get_moment( int cur[] );
static int display_moment( int mom[] );
static int check_schedule( int prev[], int now[] );







/* Exported function */
schedule_event( int month, int day, int hour, int minute, int second,
															FUNC_P func )
{
	Schedule[N_Events].pattern[MONTH] = month;
	Schedule[N_Events].pattern[DAY] = day;
	Schedule[N_Events].pattern[HOUR] = hour;
	Schedule[N_Events].pattern[MINUTE] = minute;
	Schedule[N_Events].pattern[SECOND] = second;
	Schedule[N_Events].func = func;
	N_Events += 1;
}

/* Exported function */
event_mgr()
{

	if( !Initialized ) {
		get_moment( prev );
		Initialized = TRUE;
		return;
	}
	get_moment( now );
	check_schedule( prev, now );
	bcopy( now, prev, sizeof(now) );
}


static
check_schedule( int prev[], int now[] )
{
	int		i;

	for( i=0; i<N_Events; i++ ) {
		if( event_due(Schedule[i].pattern,prev,now) ) {
			(*Schedule[i].func)();
		}
	}
}

	
/*
** Given a pattern which specifies the time(s) an event is to occur and
** tuples representing the previous time we checked and the current time,
** return true if the event should be triggered.
**
** The idea is that a pattern represents a (potentially large) set of moments
** in time.  We generate the earliest member of this set which comes after
** prev, and call it alpha.  We then compare alpha with the current time to
** see if the event is due, (or overdue), now.
*/
static
event_due( int pattern[], int prev[], int now[] )
{
	int		alpha[N_ELEM];
	int		i;

		/* First approximation to alpha */
	for( i=0; i<N_ELEM; i++ ) {
		if( pattern[i] == STAR ) {
			alpha[i] = prev[i];
		} else {
			alpha[i] = pattern[i];
		}
	}

		/* Find earliest time matching pattern which is after prev */
	if( before(alpha,prev) ) {
		i = next_rightmost( pattern, N_ELEM );
		if( i < 0 ) {
			return FALSE;
		}
		for(;;) {
			alpha[i] += 1;
			if( before(prev,alpha) ) {
				break;
			} else {
				alpha[i] = 0;
				i = next_rightmost( pattern, i );
				if( i < 0 ) {
					return FALSE;
				}
			}
		}
	}

	return before( alpha, now );
}

/*
** Return true if t1 is before t2, and false otherwise.
*/
static
before( int t1[], int t2[] )
{
	int		i;

	for( i=0; i<N_ELEM; i++ ) {
		if( t1[i] < t2[i] ) {
			return TRUE;
		}
		if( t1[i] > t2[i] ) {
			return FALSE;
		}
	}
	return FALSE;
}

/*
** Given a pattern and a subscript and a pattern, return the subscript of
** the next rightmost STAR in the pattern.  If there is none, return -1.
*/
static
next_rightmost( int pattern[], int i )
{
	for( i--; i >= 0; i-- ) {
		if( pattern[i] == STAR ) {
			return i;
		}
	}
	return -1;
}

/*
** Fill in a tuple with the current time.
*/
static
get_moment( int cur[] )
{
	struct tm	*tm;
	time_t	clock;

	(void)time( &clock );
	tm = localtime( &clock );
	cur[MONTH] = tm->tm_mon + 1;
	cur[DAY] = tm->tm_mday;
	cur[HOUR] = tm->tm_hour;
	cur[MINUTE] = tm->tm_min;
	cur[SECOND] = tm->tm_sec;
	/*
	display_moment( cur );
	*/
}

static
display_moment( int mom[] )
{
	printf( "%02d/%02d %02d:%02d:%02d\n",
			mom[MONTH], mom[DAY], mom[HOUR], mom[MINUTE], mom[SECOND] );
}
