#ifndef _Time_H_
#define _Time_H_

#include "condor_debug.h"
#include <sys/time.h>

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

  int operator==(const Time& T) { return (Sec==T.Sec && MicroSec==T.MicroSec); }
  int operator<(const Time& T) { return (Sec==T.Sec ? MicroSec<T.MicroSec : Sec<T.Sec); }
  
  static Time Now() {
    struct timeval tp;
    if (gettimeofday(&tp,NULL)==-1) {
      EXCEPT ("gettimeofday()");
    }
    return Time(tp.tv_sec,tp.tv_usec);
  }

  static double DiffTime(Time From, Time To) {
    return double(To.Sec-From.Sec)+double(To.MicroSec-From.MicroSec)/1000000;
  }

};

#endif
