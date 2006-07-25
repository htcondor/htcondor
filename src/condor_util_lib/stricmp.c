/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

 


#include <stdio.h>
#include <string.h>

#define to_lower(a) (((a)>='A'&&(a)<='Z')?((a)|040):(a))

/*
** Just like strcmp but case independent. 
*/
int
stricmp(register char* s1, register char* s2)
{
	while (to_lower(*s1) == to_lower(*s2)) {
		s2++;
		if (*s1++=='\0')
			return(0);
	}
	return(to_lower(*s1) - to_lower(*s2));
}


/*
 * Compare strings (at most n bytes):  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
strincmp( register char* s1, register char* s2, register int n )
{

	while (--n >= 0 && to_lower(*s1) == to_lower(*s2)) {
		s2++;
		if (*s1++ == '\0')
			return(0);
	}
	return(n<0 ? 0 : to_lower(*s1) - to_lower(*s2) );
}

/*
** Return a pointer to the first occurence of pattern in string.
*/
char *
substr( const char* string, const char* pattern )
{
	char	*str;
	int		n;

	n = strlen( pattern );
	for( str=(char*)string; *str; str++ ) {
		if( strncmp(str,pattern,n) == 0 ) {
			return str;
		}
	}
	return NULL;
}
