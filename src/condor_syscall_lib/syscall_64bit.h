#ifndef CONDOR_SYSCALL_64BIT_H
#define CONDOR_SYSCALL_64BIT_H

#if HAS_64BIT_STRUCTS

#if defined(HPUX)
#	include "condor_hpux_64bit_types.h"
#else 
    typedef long long off64_t;
    struct stat64;
    struct rlimit64;
#endif /* HPUX */

#endif /* HAS_64BIT_STRUCTS */

#endif /* CONDOR_SYSCALL_64BIT_H */
