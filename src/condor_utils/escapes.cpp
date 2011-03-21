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

#include "condor_common.h"

#include <string.h>

#include "escapes.h"

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
