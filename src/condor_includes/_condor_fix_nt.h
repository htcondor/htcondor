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
#if !defined(_CONDOR_FIX_NT)
#define _CONDOR_FIX_NT

#include <io.h>
#include <fcntl.h>
#include <direct.h>		// for _chdir , etc
#define lseek _lseek
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#define O_APPEND _O_APPEND
#include <sys/stat.h>
typedef unsigned short mode_t;
#define stat _stat
#define fstat _fstat
#define MAXPATHLEN 1024
#define MAXHOSTNAMELEN 64
#define	_POSIX_PATH_MAX 255
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strincmp _strnicmp
#define strdup _strdup
#define chdir _chdir
#define fsync _commit
#define access _access
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
#define getwd(path) (int)_getcwd(path, _POSIX_PATH_MAX)

#endif
