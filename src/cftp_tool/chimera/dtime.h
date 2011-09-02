/**
 ** dtime.h
 */

#ifndef DTIME_H
#define DTIME_H

#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   ** dtime:
   **  returns the time of day in double format with microsecond precision
   */
    extern double dtime ();

  /**
   ** dalarm: 
   **  generates a SIGALRM signal in #time# seconds
   */
    extern void dalarm (double time);

  /**
   ** dalarm:
   **  sleeps for #time# seconds
   */
    extern void dsleep (double time);

  /**
   ** dtotv: 
   **  returns the struct timeval representation of double #d#
   */
    extern struct timeval dtotv (double d);

  /**
   ** tvtod:
   **  returns the double representation of timeval #tv#
   */
    extern double tvtod (struct timeval tv);

#ifdef __cplusplus
}
#endif

#endif				/* DTIME_H */
