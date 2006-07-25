/*
 *	This file is licensed under the terms of the Apache License Version 2.0 
 *	http://www.apache.org/licenses. This notice must appear in modified or not 
 *	redistributions of this file.
 *
 *	Redistributions of this Software, with or without modification, must
 *	reproduce the Apache License in: (1) the Software, or (2) the Documentation
 *	or some other similar material which is provided with the Software (if any).
 *
 *	Copyright 2005 Argonne National Laboratory. All rights reserved.
 */

#ifndef GM_GLOBAL_H
#define GM_GLOBAL_H

/*
 *	FILE CONTENT:
 *	Declaration of global functions for displaying errors
 *	and generating pseudorandom numbers.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/9/05	--	coding and testing finished
 *	9/16/05	--	added "const" in alert argument list
 *	9/17/05	--	added strlwr()
 *	9/22/05	--	added stricmp() and strnicmp()
 */

void alert(const char* s);
float randExponential( float mean );
float randNormal( float mean, float stdev );
#ifdef UNIX
	char* strlwr(char * tab);
	int stricmp( const char *string1, const char *string2 );
	int strnicmp( const char *string1, const char *string2, int len );
#endif
void global_test(void);

char *skipWhite(char *tab);
char *skipNonWhite(char *tab);

#endif
