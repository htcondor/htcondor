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
    long l=(long) Sec;
    return asctime(localtime(&l));
  } 

};

#endif
