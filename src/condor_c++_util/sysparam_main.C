#include "condor_sysparam.h"

#if			defined(SUNOS41)
SunOs       ptype;
#elif  		defined(ULTRIX43) || defined(ULTRIX42)
Ultrix      ptype;
#elif		defined(OSF1)
Osf1		ptype;
#endif

BaseOs*     platform =  &ptype;

	
