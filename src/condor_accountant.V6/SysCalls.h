#ifndef _SysCalls_H_
#define _SysCalls_H_

#include "condor_debug.h"
#include <sys/time.h>
#include <math.h>

static char *_FileName_ = __FILE__;

class SysCalls {

public:
  
  struct Time {
    long Sec;
    long MicroSec;
  };
  
  static Time GetTime() {
    struct timeval tp;
    if (gettimeofday(&tp,NULL)==-1) 
	{
		EXCEPT ("gettimeofday()");
	}
    Time T;
    T.Sec=tp.tv_sec;
    T.MicroSec=tp.tv_usec;
    return T;
  }

  static double DiffTime(Time From, Time To) {
    return double(To.Sec-From.Sec+(To.MicroSec-From.MicroSec)/1000000);
  }

  static double Power(double x, double y) { return pow(x,y); }
};

#endif
