
#include "condor_common.h"
#ifdef WIN32
#include "dynuser.h"

/* This is the global object to access Dynuser. 
*/
dynuser		myDyn;
dynuser		*myDynuser = &myDyn;

#endif //of WIN32


