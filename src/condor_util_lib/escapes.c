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
#include "condor_common.h"

#include <string.h>

static int getHexDigitValue( int );

char *collapse_escapes( char *str )
{
	char *cp = str;
	char *np;
	int  value;
	int	 length = strlen( str );
	int  count;

	while( *cp ) {
		// skip over non escape characters
		while( *cp && *cp != '\\' ) cp++;

		// end of string?
		if( !*cp ) break;

		// ASSERT: *cp == '\\'
		np = cp + 1;
		switch( *np ) {
			case 'a':	value = '\a'; np++; break;
			case 'b':	value = '\b'; np++; break;
			case 'f':	value = '\f'; np++; break;
			case 'n':	value = '\n'; np++; break;
			case 'r':	value = '\r'; np++; break;
			case 't':	value = '\t'; np++; break;
			case 'v':	value = '\v'; np++; break;
			case '\\':	value = '\\'; np++; break;
			case '\?':	value = '\?'; np++; break;
			case '\'':	value = '\''; np++; break;
			case '\"':	value = '\"'; np++; break;

			default:
				// octal sequence
				if( isdigit( *np ) ) {
					value = 0;
					while( *np && isdigit( *np ) ) {
						value += value*8 + ((*np) - '0');
						np++;
					}
				} else if ( *np == 'x' ) {
					// hexadecimal sequence
					value = 0;
					np++;
					while( *np && isxdigit( *np ) ) {
						value += value*16 + getHexDigitValue(*np);
						np++;
					}
				} else {
 					// just copy the character over
					value = *np;
					np++;
				}
		}

		// calculate how much of the string has to be shifted over
		count  = length - (np - str) + 1;
		length = length - (np - cp)  + 1;
		
		// stuff the value into *cp, and shift the tail of the string over
		*cp = value;
		memmove( cp+1, np, count );

		// next character to process
		cp++;
	}
	
	return str;
}


static int 
getHexDigitValue( int c )
{
	c = tolower(c);
	if (isdigit(c)) return (c - '0');
	if (isxdigit(c))return (c - 'a') + 10;
	return 0;
}
