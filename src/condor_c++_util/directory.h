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
#ifndef DIRECTORY_H
#define DIRECTORY_H

#if defined (ULTRIX42) || defined(ULTRIX43)
	/* _POSIX_SOURCE should have taken care of this,
		but for some reason the ULTRIX version of dirent.h
		is not POSIX conforming without it...
	*/
#	define __POSIX
#endif

#include <dirent.h>

#if defined(SUNOS41) && defined(_POSIX_SOURCE)
	/* Note that function seekdir() is not required by POSIX, but the sun
	   implementation of rewinddir() (which is required by POSIX) is
	   a macro utilizing seekdir().  Thus we need the prototype.
	*/
	extern "C" void seekdir( DIR *dirp, long loc );
#endif

class Directory
{
public:
	Directory( const char *name );
	~Directory();
	void Rewind();
	char *Next();
private:
	DIR *dirp;
};

#endif
