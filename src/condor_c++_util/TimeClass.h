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
#ifndef _TimeClass_H_
#define _TimeClass_H_

#include "condor_debug.h"
#if !defined(WIN32)
#include <sys/time.h>
#endif

static char *_FileName_ = __FILE__;

class Time {

private:

  double Sec;

public:

  Time(double d=0) { Sec=d; }
  Time(const Time& T) { Sec=T.Sec; }

  Time& operator=(const Time& T) {
    Sec=T.Sec;
    return *this;
  }

  int operator==(const Time& T) const { return (Sec==T.Sec); }
  int operator<(const Time& T) const { return (Sec<T.Sec); }
  int operator<=(const Time& T) const { return (Sec<=T.Sec); }
  int operator>(const Time& T) const { return (Sec>T.Sec); }
  int operator>=(const Time& T) const { return (Sec>=T.Sec); }
  
  static int Now() { return time(NULL); }

/*
  static Time Now() {
#if defined(WIN32)
	  return Time(time(NULL));
#else
    struct timeval tp;
    if (gettimeofday(&tp,NULL)==-1) {
      EXCEPT ("gettimeofday()");
    }
    double d=((double)tp.tv_sec)+((double)tp.tv_usec)/1000000;
    return Time(d);
#endif
  }
*/

/*
  friend Time operator-(const Time& T1, const Time& T2) {
    return Time(T1.Sec-T2.Sec);
  }

  friend ostream& operator<<(ostream& os, const Time& T) {
    double d=T.Sec;
    os << d;
    return os;
  }

  friend istream& operator>>(istream& is, Time& T) {
    double d;
    is >> d;
    T.Sec=d;
    return is;
  }
*/

  operator double() const { return Sec; }

  const char* Asc() const {
#if defined(OSF1)
	  int l=(int)Sec;
#else
	  long l=(long) Sec;
#endif
	  return asctime(localtime(&l));
  } 

};

#endif
