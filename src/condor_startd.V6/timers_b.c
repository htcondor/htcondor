/*****************************************************/
/* Various timer routines.                           */
/* Al Aburto, aburto@marlin.nosc.mil, 17 Jun 1994    */
/*                                                   */
/* t = dtime() outputs the current time in seconds.  */
/* Use CAUTION as some of these routines will mess   */
/* up when timing across the hour mark!!!            */
/*                                                   */
/* For timing I use the 'user' time whenever         */
/* possible. Using 'user+sys' time is a separate     */
/* issue.                                            */
/*                                                   */
/* Example Usage:                                    */
/* [timer options added here]                        */
/* main()                                            */
/* {                                                 */
/* double starttime,benchtime,dtime();               */
/*                                                   */
/* starttime = dtime();                              */ 
/* [routine to time]                                 */
/* benchtime = dtime() - starttime;                  */
/* }                                                 */
/*                                                   */
/* [timer code below added here]                     */
/*****************************************************/

/***************************************************************/
/* Timer options. You MUST uncomment one of the options below  */
/* or compile, for example, with the '-DUNIX' option.          */
/***************************************************************/

#if !defined(WIN32)
#define UNIX
#endif

/*****************************************************/
/*  UNIX dtime(). This is the preferred UNIX timer.  */
/*  Provided by: Markku Kolkka, mk59200@cc.tut.fi    */
/*  HP-UX Addition by: Bo Thide', bt@irfu.se         */
/*****************************************************/
#ifdef UNIX
#include <sys/types.h>
#include <sys/times.h>
#include <unistd.h>

#ifdef hpux
#include <sys/syscall.h>
#define getrusage(a,b) syscall(SYS_getrusage,a,b)
#endif

double dtime()
{
	double q;
	static long clock_tick = -1;
	struct tms	my_times;

	if ( clock_tick < 1 || clock_tick > 1000) {
		clock_tick = sysconf( _SC_CLK_TCK );
	}

/*	clock_tick = sysconf( _SC_CLK_TCK ); */

	times(&my_times);

	q = ((double) (my_times.tms_utime)) / ((double) clock_tick);

/*	q = (double)(rusage.ru_utime.tv_sec);
	q = q + (double)(rusage.ru_utime.tv_usec) * 1.0e-06; */
	
	return q;
}
#endif

#ifdef WIN32
#include <windows.h>
#include <sys/types.h>
#include <sys/timeb.h>

double dtime()
{
	struct _timeb timebuffer;
	_ftime( &timebuffer );
	return (double)timebuffer.time + (double)timebuffer.millitm/(double)1000;
}
#endif