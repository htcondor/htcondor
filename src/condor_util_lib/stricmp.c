/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include <stdio.h>

#define to_lower(a) (((a)>='A'&&(a)<='Z')?((a)|040):(a))

/*
** Just like strcmp but case independent. 
*/
stricmp(s1,s2)
register char *s1, *s2;
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
strincmp(s1, s2, n)
register char *s1, *s2;
register n;
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
substr( string, pattern )
char	*string;
char	*pattern;
{
	char	*str, *s, *p;
	int		n;

	n = strlen( pattern );
	for( str=string; *str; str++ ) {
		if( strncmp(str,pattern,n) == 0 ) {
			return str;
		}
	}
	return NULL;
}
