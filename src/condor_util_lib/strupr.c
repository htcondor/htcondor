/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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


