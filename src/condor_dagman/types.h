#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdio.h>

//---------------------------------------------------------------------------
#ifndef __cplusplus
typedef int bool;
#define true  1
#define false 0
#endif

//---------------------------------------------------------------------------
#define     OK   true
#define NOT_OK   false

///
typedef int JobID_t;

//---------------------------------------------------------------------------
///
template<class TYPE> int compare (TYPE a, TYPE b);

//---------------------------------------------------------------------------
/** Condor uses three integers to identify jobs. This structure 
    will be used to store those three numbers.  
*/
class CondorID {
 public:
  ///
  CondorID () : _cluster(-1), _proc(-1), _subproc(-1) {}

  ///
  CondorID (int cluster, int proc, int subproc):
    _cluster(cluster), _proc(proc), _subproc(subproc) {}

  ///
  inline void Set (int cluster, int proc, int subproc) {
    _cluster = cluster;
    _proc    = proc;
    _subproc = subproc;
  }

  /** Compare this condorID's with another
      @return zero if they match
  */
  int Compare (const CondorID condorID) const;

  ///
  inline bool operator == (const CondorID condorID) const {
    return Compare (condorID) == 0;
  }

  ///
  inline void Print (FILE * file = stdout) const {
    fprintf (file, "(%d.%d.%d)", _cluster, _proc, _subproc);
  }

  /** */ int _cluster;
  /** */ int _proc;
  /** */ int _subproc;
};

#endif /* #ifndef _TYPES_H_ */
