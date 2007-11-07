/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 


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
const char *
substr( const char* string, const char* pattern )
{
	const char	*str;
	int		n;

	n = strlen( pattern );
	for( str=string; *str; str++ ) {
		if( strncmp(str,pattern,n) == 0 ) {
			return str;
		}
	}
	return NULL;
}
