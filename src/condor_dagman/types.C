#include "types.h"

//---------------------------------------------------------------------------
template<class TYPE> int compare(TYPE a, TYPE b) {
  if (a == b) return 0;
  return (a > b ? 1 : -1);
}

//---------------------------------------------------------------------------
int CondorID::Compare (const CondorID condorID) const {
  int result = compare (_cluster, condorID._cluster);
  if (result == 0) result = compare (_proc, condorID._proc);
  if (result == 0) result = compare (_subproc, condorID._subproc);
  return result;
}
