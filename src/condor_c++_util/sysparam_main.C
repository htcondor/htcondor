#include "condor_sysparam.h"

#if			defined(SUNOS41)
SunOs      	ptype;
#elif  		defined(ULTRIX43) || defined(ULTRIX42)
Ultrix     	ptype;
#elif		defined(OSF1)
Osf1		ptype;
#elif		defined(HPUX9)
Hpux		ptype;
#elif 		defined(AIX32)
Aix			ptype;
#elif		defined(LINUX)
Linux		ptype;
#endif

BaseOs*     platform =  &ptype;

	
