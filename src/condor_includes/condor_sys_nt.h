/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONDOR_SYS_NT_H
#define CONDOR_SYS_NT_H

// #define NOGDI
#define NOSOUND
#include <winsock2.h>
#include <windows.h>

#include <io.h>
#include <fcntl.h>
#include <direct.h>		// for _chdir , etc
#include <sys/utime.h>	// for struct _utimbuf & friends
#include <string.h>
#define lseek _lseek
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#define O_APPEND _O_APPEND
#define O_TRUNC _O_TRUNC
#include <sys/stat.h>
typedef unsigned short mode_t;
typedef DWORD pid_t;
#define stat _stat
#define fstat _fstat
#define MAXPATHLEN 1024
#define MAXHOSTNAMELEN 64
#define	_POSIX_PATH_MAX 255
#define pipe(fds) _pipe(fds,2048,_O_BINARY)
#define popen _popen
#define pclose _pclose
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strincmp _strnicmp
#define strdup _strdup
#define strupr _strupr
#define strlwr _strlwr
#define chdir _chdir
#define fsync _commit
#define access _access
#define execl _execl  
#define execv _execv
#define putenv _putenv
#define itoa _itoa
#define utime _utime
#define utimbuf _utimbuf
#define R_OK 4
#define W_OK 2
#define X_OK 4
#define F_OK 0
#define sleep(x) Sleep(x*1000)
#define getpid _getpid
#include <process.h>
#include <time.h>
#include <lmcons.h> // for UNLEN
#if !defined(_POSIX_)
#	define _POSIX_
#	define _CONDOR_DEFINED_POSIX_
#endif
#include <limits.h>
#if defined(_CONDOR_DEFINED_POSIX_)
#	undef _POSIX_
#	undef _CONDOR_DEFINED_POSIX_
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include "condor_file_lock.h"
#include "condor_fix_assert.h"

#define getwd(path) (int)_getcwd(path, _POSIX_PATH_MAX)
#define mkdir(path,mode) (int)_mkdir(path)
#define S_IRWXU 0
#define S_IRWXG 1
#define S_IRWXO 2
#define S_ISDIR(mode) (((mode)&_S_IFDIR) == _S_IFDIR)
#define S_ISREG(mode) (((mode)&_S_IFREG) == _S_IFREG)
#define rint(num) floor(num + .5)

#define ETIMEDOUT ERROR_TIMEOUT

typedef fd_set *SELECT_FDSET_PTR;

struct rusage {
    struct timeval ru_utime;    /* user time used */
    struct timeval ru_stime;    /* system time used */
    long    ru_maxrss;      /* XXX: 0 */
    long    ru_ixrss;       /* XXX: 0 */
    long    ru_idrss;       /* XXX: sum of rm_asrss */
    long    ru_isrss;       /* XXX: 0 */
    long    ru_minflt;      /* any page faults not requiring I/O */
    long    ru_majflt;      /* any page faults requiring I/O */
    long    ru_nswap;       /* swaps */
    long    ru_inblock;     /* block input operations */
    long    ru_oublock;     /* block output operations */
    long    ru_msgsnd;      /* messages sent */
    long    ru_msgrcv;      /* messages received */
    long    ru_nsignals;        /* signals received */
    long    ru_nvcsw;       /* voluntary context switches */
    long    ru_nivcsw;      /* involuntary " */
};

#endif /* CONDOR_SYS_NT_H */

