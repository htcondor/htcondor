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

 

#ifndef __libgxx_sys_dir_h

extern "C" {

#ifdef __sys_dir_h_recursive
#include_next <sys/dir.h>
#else
#define __sys_dir_h_recursive
#define opendir __hide_opendir
#define closedir __hide_closedir
#define readdir __hide_readdir
#define telldir __hide_telldir
#define seekdir __hide_seekdir
#if ! (defined(__ultrix__) || defined(__sun__) || defined(AIX32)) 
#define rewinddir __hide_rewinddir
#endif

#include_next <sys/dir.h>

#define __libgxx_sys_dir_h
#undef opendir
#undef closedir
#undef readdir
#undef telldir
#undef seekdir
#if ! (defined(__ultrix__) || defined(__sun__) || defined(AIX32))
#undef rewinddir
#endif

DIR *opendir(const char *);
int closedir(DIR *);
#ifdef __dirent_h_recursive
// Some operating systems we won't mention (such as the imitation
// of Unix marketed by IBM) implement dirent.h by including sys/dir.h,
// in which case sys/dir.h defines struct dirent, rather than
// the struct direct originally used by BSD.
struct dirent *readdir(DIR *);
#else
struct direct *readdir(DIR *);
#endif
long telldir(DIR *);
void seekdir(DIR *, long);
#ifndef rewinddir
void rewinddir(DIR *);
#endif
#endif
}

#endif
