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
