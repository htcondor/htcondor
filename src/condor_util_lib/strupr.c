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


/*
  NOTE: we do *NOT* want to include "condor_common.h" and all the
  associated header files for this, since this file defines two very
  simple functions and we want to be able to build it in a stand-alone
  version of the UserLog parser for libcondorapi.a
*/


#ifndef _tolower
#define _tolower(c) ((c) + 'a' - 'A')
#endif

#ifndef _toupper
#define _toupper(c) ((c) + 'A' - 'a')
#endif


/*
** Convert a string, in place, to the uppercase version of it. 
*/
char*
strupr(char* src)
{
	register char* tmp;
	tmp = src;
	while( tmp && *tmp ) {
        if( *tmp >= 'a' && *tmp <= 'z' ) {
			*tmp = _toupper( *tmp );
		}
		tmp++;
	}
	return src;
}


/*
** Convert a string, in place, to the lowercase version of it. 
*/
char*
strlwr(char* src)
{
	register char* tmp;
	tmp = src;
	while( tmp && *tmp ) {
        if( *tmp >= 'A' && *tmp <= 'Z' ) {
			*tmp = _tolower( *tmp );
		}
		tmp++;
	}
	return src;
}


