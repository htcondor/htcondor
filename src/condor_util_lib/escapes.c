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
