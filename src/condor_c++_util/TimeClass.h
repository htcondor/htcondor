#ifndef _TimeClass_H_
#define _TimeClass_H_

#include "condor_debug.h"
#if !defined(WIN32)
#include <sys/time.h>
#endif

static char *_FileName_ = __FILE__;

class Time {

private:

  long Sec;
  long MicroSec;

public:

  Time(long S=0, long MS=0) { Sec=S; MicroSec=MS; }
  Time(const Time& T) { Sec=T.Sec; MicroSec=T.MicroSec; }

  Time& operator=(const Time& T) {
    Sec=T.Sec;
    MicroSec=T.MicroSec;
    return *this;
  }

  int operator==(const Time& T) const { return (Sec==T.Sec && MicroSec==T.MicroSec); }
  int operator<(const Time& T) const { return (Sec==T.Sec ? MicroSec<T.MicroSec : Sec<T.Sec); }
  int operator<=(const Time& T) const { return (Sec==T.Sec ? MicroSec<=T.MicroSec : Sec<T.Sec); }
  int operator>(const Time& T) const { return (Sec==T.Sec ? MicroSec>T.MicroSec : Sec>T.Sec); }
  int operator>=(const Time& T) const { return (Sec==T.Sec ? MicroSec>=T.MicroSec : Sec>T.Sec); }
  
  static Time Now() {
#if defined(WIN32)
	  return Time(time(NULL), 0);
#else
    struct timeval tp;
    if (gettimeofday(&tp,NULL)==-1) {
      EXCEPT ("gettimeofday()");
    }
    return Time(tp.tv_sec,tp.tv_usec);
#endif
  }

  static double DiffTime(Time From, Time To) {
    return double(To.Sec-From.Sec)+double(To.MicroSec-From.MicroSec)/1000000;
  }

  friend ostream& operator<<(ostream& os, const Time& T) {
    os << T.Sec << ' ' << T.MicroSec;
    return os;
  }

  friend istream& operator>>(istream& is, Time& T) {
    long l;
    is >> l;
    T.Sec=l;
    is >> l;
    T.MicroSec=l;
    return is;
  }

};

#endif
