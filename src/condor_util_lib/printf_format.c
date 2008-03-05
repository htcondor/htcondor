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
#include "printf_format.h"


/* A couple of private helper-functions we need for our
   parsePrintfFormat() function below...
*/
static int
isFlagChar( char c )
{
	switch( c ) {
	case '#':
	case '0':
	case '-':
	case ' ':
	case '+':
	case '\'':
		return 1;
	}
	return 0;
}
		

static int
isLengthChar( char c )
{
	switch( c ) {
	case 'h':
	case 'l':
	case 'L':
	case 'q':
	case 'j':
	case 'z':
	case 't':
		return 1;
	}
	return 0;
}
		

/*
  we're going to use this function whenever the format string contains
  an int literal that we need to both store the value of and consume
  for the purposes of parsing the string.  examples are the width and
  precision.  so, we just read digits one at a time from the given
  string.  for each digit, we multiply what we've read so far by 10,
  and advance our pointer.
*/
static int
consumeInt( const char** str )
{
	int rval = 0;
	int digit_val;

	if( ! str || !*str ) {
		return -1;
	}

	while( isdigit(**str) ) {
			/* To get the int value of this digit, we can just
			   subtract the ASCII value of '0' from the given char,
			   since the ASCII codes for 0-9 are consecutive */
		digit_val = **str - '0';
		rval *= 10;
		rval += digit_val;
			/* advance to the next char in the string */
		(*str)++;
	}
	return rval;
}


/*
  Parse the printf format string pointed to by the given pointer.  We
  move the pointer forward as we consume characters, so the caller
  knows how much of the string we parsed.  Everything we learn about
  the string we store in the given structure, most importantly, what
  kind of conversion is being requested.  This only parses the first
  conversion string we encounter, so if the caller wants to know about
  multiple conversion characters, they have to call this multiple
  times, and only pass this function the substring of the format
  string they care about.  this returns FALSE if it runs out of
  characters before it figures out what it needs, otherwise TRUE if it
  successfully parsed something.

  NOTE: currently this function only parses format strings that
  conform to the kinds of modifiers, flags, width, precision, etc,
  specified in the Linux and BSD (OSX) printf(3) man pages.  Also,
  width and precision being defined as optional args to printf
  (e.g. "%*.*d") is not really supported.  We recognize the '*'
  characters, but we have no way to fill in the width or precision
  fields in the info struct we were passed (since we're not passed the
  variable args).  The "AltiVec vector" extensions mentioned in the
  OSX man page are *not* supported.  Also, M$ compiler-specific type
  flags (like "%I64d") are *not* supported (though it might not be
  that hard to add support for those in the future).
*/

int
parsePrintfFormat( const char **fmt_p, struct printf_fmt_info *info )
{
	if( ! **fmt_p ) {
		return FALSE;
	}

		/* find the first '%' character */
	while( **fmt_p && **fmt_p != '%' ) {
		(*fmt_p)++;
	}
		/* end of string */
	if( ! **fmt_p) {
		return FALSE;
	}
		/* now skip the '%' itself */
	(*fmt_p)++;
	if( ! **fmt_p ) {
		return FALSE;
	}

		/* clear out the struct we were passed */
	if( ! info ) {
		return FALSE;
	}
	memset( info, '\0', sizeof(struct printf_fmt_info) );

		/* First, look for modifying flags */
	while( **fmt_p && isFlagChar(**fmt_p) ) {
		switch( **fmt_p ) {
		case '#':
				/* The value should be converted to an "alternate form" */
			info->is_alt = 1;
			break;

		case '0':
				/* The value should be zero padded */
			info->is_pad = 1;
			break;

		case '-':
				/* The converted value is to be left adjusted */
			info->is_left = 1;
			break;

		case ' ':
				/* A blank should be left before a positive number */
			info->is_space = 1;
			break;

		case '+':
				/* A sign (+ or -) always placed before a number */
			info->is_signed = 1;
			break;

		case '\'':
				/* Show grouping in numbers if the locale information
				   indicates any.  */
			info->is_grouped = 1;
			break;
		}
		(*fmt_p)++;
	}
		

		/* Now, find the width, if any */
	if( **fmt_p == '*' ) {
			/* Width is given in an argument, we don't support this yet */
    } else if( isdigit(**fmt_p) ) {
			/* Width is a constant */
		info->width = consumeInt( fmt_p );
	}
	if( ! **fmt_p ) {
		return FALSE;
	}

		/* Next, find the precision, if any.  storing a negative value
		   means nothing was specified, a 0 is if the format really
		   says 0 */
	info->precision = -1;
	if( **fmt_p == '.') {
			/* found a precision.  first, skip past the '.' */
		(*fmt_p)++;
		if( ! **fmt_p ) {
			return FALSE;
		}
		if( **fmt_p == '*') {
				/* Precision is set as is an argument, we don't
				   support this yet, either.  */
		} else if( isdigit(**fmt_p) ) {
			info->precision = consumeInt( fmt_p );
		}
    }
	if( ! **fmt_p ) {
		return FALSE;
	}

		/* Next come the optional length modifiers  */
	while( **fmt_p && isLengthChar(**fmt_p) ) {
		switch( **fmt_p ) {
		case 'h':
				/* integer conversions are short int's */
			info->is_short = 1;
			break;

		case 'l':
			if( info->is_long ) {
					/* 'll' means long long */
				info->is_long_long = 1;
			} else {
					/* 'l' just means long int */
				info->is_long = 1;
			}
			break;

		case 'L':
				/* float conversion is a long double */
			info->is_long_double = 1;
			break;

		case 'q':
				/* "quad-precision", treat as long long */
			info->is_long_long = 1;
			break;

		case 'j':
		case 'z':
		case 't':
				/* we recognize these, but don't really do anything
				   with them yet */
			break;
		}
		(*fmt_p)++;
	}
	if( ! **fmt_p ) {
		return FALSE;
	}

		/* FINALLY, get the type of conversion for this format string! */
	info->fmt_letter = **fmt_p;

		/* since we consumed it, we should advance our pointer */
	(*fmt_p)++;
	
		/* Now that we have the conversion character, set our type */
	switch( info->fmt_letter ) {

			/* Standard types */
	case 'd':
	case 'i':
	case 'o':
	case 'u':
	case 'x':
	case 'X':
		info->type = PFT_INT;
		break;

	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	case 'a':
	case 'A':
		info->type = PFT_FLOAT;
		break;

	case 'c':
		info->type = PFT_CHAR;
		break;

	case 's':
		info->type = PFT_STRING;
		break;


			/* Some weird special cases mentioned in the man page */
    case 'p':
		info->type = PFT_POINTER;
		break;

    case 'n':
			/* don't really support this, but treat like an int */
		info->type = PFT_INT;
		break;

    case 'C':
		info->type = PFT_CHAR;
		info->is_long = 1;
		break;

    case 'S':
		info->type = PFT_STRING;
		info->is_long = 1;
		break;
		
    case '%':
			/* literal '%' char!  in this case, we just want to
			   recursively call ourselves and keep parsing, since we
			   want to treat this like any other string literal we'd
			   skip over.  we've already moved the fmt_p, and we'll
			   re-initialize the info struct... */
		return parsePrintfFormat( fmt_p, info );
		break;

    default:
      /* An unknown conversion char!  */
		info->type = PFT_NONE;
		return FALSE;
		break;
    }
	return TRUE;
}
