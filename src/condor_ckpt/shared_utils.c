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


/*
   This file is for shared utility functions and other support code
   that is shared between the standalone checkpointing and the RSC
   checkpointing libraries.
*/

#include "condor_common.h"
#include "condor_sys.h"		/* For SYS_write */
#include "shared_utils.h"

static int _condor_itoa(long quantity, char *out, int base);

/* the length of the output buffer */
#define VFPRINTF_LEN 384
#define DECIMAL 0
#define HEXADECIMAL 1
#define OCTAL 2
/* these are the number or ASCII digits a 64bit quantity needs with a minus
	sign and stuff */
#define ASCII64DECIMAL 22
#define ASCII64HEXADECIMAL 17
#define ASCII64OCTAL 23
#define ABS(x) ((x)<0)?((x)*-1):x

/* 
   Our special version of vfprintf() that doesn't use the clib and
   doesn't call malloc(), etc.
*/
int
_condor_vfprintf_va( int fd, const char* fmt, va_list args )
{
	char out[VFPRINTF_LEN + 1];	/* the output buffer passed to SYS_write */
	const char *i;	/* pointer into the fmt */
	char *o, *p;	/* pointers into the out and %s, if any */
	int c = 0;		/* number of output characters from conversion of fmt */
	double f;		/* any float-like quantity from va_list */
	long d;			/* any int-like quantity from va_list */
	int loopIndex;		/* loopIndex usage in for loops */
#if 0
		/* neither of these are used, i wonder why they're here */
	int signedness;	/* is the int-like quantity signed or not? */
	int val;		/* how many chars the ASCII rep is of a number */
#endif

	/* how many digits you need to represent in ASCII base 10 a 64 bit 
		quantity plus a minus sign and a null character. For those interested,
		the equation is ceil(ln(numberinbase)/ln(base)) + minus sign + NULL.
		If you are wondering why I did only 64, it is because the
		values on the va_list are promoted to _at max_ that size. */
	char d64[ASCII64DECIMAL];

	/* how many ASCII digits in base 16 you need to represent a 64
		bit number */
	char x64[ASCII64HEXADECIMAL];

	/* how many ASCII digits in base 8 you need to represent a 64 bit
		number */
	char o64[ASCII64OCTAL];


	/* XXX I'm assuming I need to promote the types I use va_arg() on,
		The ANSI C spec is a little unclear about this, but compiler tests
		tell me that the promotion happens, even in "new-style" function
		declarations */

	for(loopIndex = 0; loopIndex < VFPRINTF_LEN+1; loopIndex++)
	{
		out[loopIndex] = 0;
	}

	i = fmt;
	o = out;

	while(*i != '\0' && c < VFPRINTF_LEN) /* minus one for the NULL */
	{
		/* if it isn't a conversion character delimiter, just copy it */
		if (*i != '%' && *i != '\\')
		{
			*o++ = *i++;
			c++;
			continue;
		}
		else if (*i == '\\') /* must be an escape sequence */
		{
			switch(*++i)
			{
				case 'a':
					*o = '\a';
					break;
				case 'b':
					*o = '\b';
					break;
				case 'f':
					*o = '\f';
					break;
				case 'n':
					*o = '\n';
					break;
				case 'r':
					*o = '\r';
					break;
				case 't':
					*o = '\t';
					break;
				case 'v':
					*o = '\v';
					break;
				case '\\':
					*o = '\\';
					break;
				case '?':
					*o = '?';
					break;
				case '\'':
					*o = '\'';
					break;
				case '\"':
					*o = '\"';
					break;

				/* XXX need to handle octal and hex numbers */

				default:
					/* not really correct behaviour, but close enough */
					*o = *i;
					break;
			}
			i++;
			o++;
			c++;
			continue;
		}

		/* *i is a '%' at this point in the algorithm */

		/* if it is a conversion character delimiter, then deal with it.
			XXX skip all of the justifying and precision stuff for now, if
			we need it, we can add it later */

		switch(*++i)
		{
			case 'c':
				d = va_arg(args, int);
				if (c < VFPRINTF_LEN)
				{
					*o++ = (char)d;
					c++;
				}
				i++;
				break;
			case 's':
				p = va_arg(args, char *);
				while(*p != '\0' && c < VFPRINTF_LEN)
				{
					*o++ = *p++;
					c++;
				}
				i++;
				break;
			case 'f':
				f = va_arg(args, double);
				i++;
				break;
			case 'p':
			case 'x':
				d = va_arg(args, unsigned long);
				_condor_itoa(d, x64, HEXADECIMAL);
				p = x64;
				while(*p != '\0' && c < VFPRINTF_LEN)
				{
					*o++ = *p++;
					c++;
				}
				i++;
				break;
			case 'o':
				d = va_arg(args, int);
				_condor_itoa(d, o64, OCTAL);
				p = o64;
				while(*p != '\0' && c < VFPRINTF_LEN)
				{
					*o++ = *p++;
					c++;
				}
				i++;
				break;
			case 'i':
			case 'd':
				d = va_arg(args, int);
				_condor_itoa(d, d64, DECIMAL);
				p = d64;
				while(*p != '\0' && c < VFPRINTF_LEN)
				{
					*o++ = *p++;
					c++;
				}
				i++;
				break;
			case 'u':
				d = va_arg(args, int);
				_condor_itoa(d, d64, DECIMAL);
				p = d64;
				while(*p != '\0' && c < VFPRINTF_LEN)
				{
					*o++ = *p++;
					c++;
				}
				i++;
				break;
			case '%':
				*o++ = *i++;
				c++;
				break;
			default: /* not really correct behaviour, but good enough */
				*o++ = *i++;
				c++;
				break;
		}
	}

	/* return the number of bytes actually written */
#if defined(Darwin) || defined(CONDOR_FREEBSD)
#ifndef SYS_write
#define SYS_write 4
#endif
#endif
	return SYSCALL(SYS_write, fd, out, c);
}


/*
   A helper function to _condor_vfprintf_va(). It will convert a
   binary representation into an ASCII representation based upon the
   axes of signed, unsigned, and hex, octal, decimal. If the base is
   hex or octal, the signedness is ignored.
*/
int
_condor_itoa(long quantity, char *out, int base)
{
	int i;
	unsigned long mask;
	unsigned char byte;
	unsigned long hexquant;
	unsigned int octquant;
	int numchars, maxchars;
	char *p, *q;
	int divi, sum, mod;
	unsigned char basemap[16];

	/* if it is zero, just fast quit */
	if( quantity == 0 ) {
		out[0] = '0'; 
		out[1] = 0;
		numchars = 1;
		return numchars;
	}

	/* We initialzie the basemap[] array in this fasion to avoid the GNU
	 * compiler from generating a call to memcpy() behind the scenes.
	 * We cannot afford to have memcpy() or any other library function
	 * invoked here since this function is called by debug statement deep
	 * inside of our restart code. -Todd 12/99 */
	basemap[0] = '0';
	basemap[1] = '1';
	basemap[2] = '2';
	basemap[3] = '3';
	basemap[4] = '4';
	basemap[5] = '5';
	basemap[6] = '6';
	basemap[7] = '7';
	basemap[8] = '8';
	basemap[9] = '9';
	basemap[10] = 'a';
	basemap[11] = 'b';
	basemap[12] = 'c';
	basemap[13] = 'd';
	basemap[14] = 'e';
	basemap[15] = 'f';

	
	switch( base ) {
	case HEXADECIMAL:	/* This always assumes a long argument */
		numchars = 0;
		hexquant = (unsigned long)quantity;
		
		mask = 0xffUL << ((sizeof(unsigned long) - 1) * 8);
		
			/* put the number into the out array, including leading zeros */
		for( i=0; i<sizeof(unsigned long); i++ ) {
			byte = (unsigned char)
					((mask & hexquant) >> (sizeof(unsigned long)-i-1)*8);
			out[i*2] = basemap[(byte&0xf0)>>4];
			out[i*2+1] = basemap[byte&0x0f];
			mask >>= 8;
		}
		out[sizeof(unsigned long)*2] = 0;
		
			/* are there any leading zeros? */
		p = out;
		while( *p == '0' ) p++;
		
			/* If there were no leading zeros, then each byte was translated
				into 2 ascii characters */
		if( p == out ) {
			return sizeof(unsigned long)*2;
		}		
		/* if there were leading zeros, then subtract from the total ascii
			characters that could have been written the amount of ascii
			characters actually written */
		numchars = (sizeof(unsigned long)*2) - (p - out);
		
			/* strip out the leading zeros */
		q = out;
		while( *p != '\0' ) {
			*q++ = *p++;
		}
		*q = 0;

		return numchars;
		break;
		
	case OCTAL:		/* This needs fixing to assume long arguments */
		octquant = (unsigned int)quantity;
		mask = 0xc0000000;
		byte = (mask & octquant) >> 30;
		out[0] = basemap[byte];
		numchars = 1;
		
		mask = 0x38000000;

		for( i = 27; i >= 0; i-=3 ) {
			byte = (mask & octquant) >> i;
			out[numchars++] = basemap[byte];
			mask >>= 3;
		}
		out[numchars] = 0;
		
			/* are there any leading zeros? */
		p = out;
		while( *p == '0' ) p++;
		
			/* no leading zeros */
		if( p == out ) {
			return 8;
		}
		numchars = 8 - (p - out);

			/* strip out the leading zeros */
		q = out;
		while( *p != '\0' ) {
			*q++ = *p++;
		}
		*q = 0;

		return numchars;
		break;

	case DECIMAL:		/* Needs fixing to assume long arguments. */
		sum = 1;
		numchars = 0;
		
			/* the simple case */
		if( quantity == 0 ) {
			out[0] = '0';
			out[1] = 0;
			return 1;
		}
		
			/* create the ASCII representation backwards */
		if (sizeof(int) == 4) {
			maxchars = 10;
		} else if (sizeof(int) == 8) {
			maxchars = 20;
		} else {
			maxchars = 10;		/* use a safe default  */
		}
		for( i = 1; i <= maxchars; i++ ) {
			out[numchars] = 0;
			divi = quantity / sum;
			sum *= 10;
			mod = divi % 10;
			if (divi == 0) break;
			out[numchars] = basemap[ABS(mod)];
			numchars++;
		}
		if( quantity < 0 ) {
			out[numchars++] = '-';
		}
		out[numchars] = 0;
		
			/* now reverse the entire buffer */
		
			/* find the start and end of the buffer */
		p = q = out;
		while( *q != '\0' ) q++;
		q--;
			
			/* do the reversing operation, in place */
		while( p != q && q > p ) {
				/* swap two int-like quantities */
			*p ^= *q;
			*q ^= *p;
			*p ^= *q;
			*p++; *q--;
		}
		return numchars;
		break;

	default:
		out[0] = 'N';
		out[1] = '/';
		out[2] = 'A';
		out[3] = 0;
		return 3;
		break;
	}
	
		/* just in case I entered this function with garbage. */
	out[0] = 'N';
	out[1] = '/';
	out[2] = 'A';
	out[3] = 0;
	return 3;
}


