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

#include "common.h"
#include "util.h"

using namespace std;

BEGIN_NAMESPACE( classad )

void convert_escapes(string &text, bool &validStr)
{
	char *copy;
	int  length;
	int  source, dest;

	// We now it will be no longer than the original.
	length = text.length();
	copy = new char[length + 1];
	
	// We scan up to one less than the length, because we ignore
	// a terminating slash: it can't be an escape. 
	dest = 0;
	for (source = 0; source < length - 1; source++) {
		if (text[source] != '\\') {
			copy[dest++]= text[source]; 
		}
		else {
			source++;

			char new_char;
			switch(text[source]) {
			case 'a':	new_char = '\a'; break;
			case 'b':	new_char = '\b'; break;
			case 'f':	new_char = '\f'; break;
			case 'n':	new_char = '\n'; break;
			case 'r':	new_char = '\r'; break;
			case 't':	new_char = '\t'; break;
			case 'v':	new_char = '\v'; break;
			case '\\':	new_char = '\\'; break;
			case '\?':	new_char = '\?'; break;
			case '\'':	new_char = '\''; break;
			case '\"':	new_char = '\"'; break;
			default:   
				if (isodigit(text[source])) {
					int  number;
					// There are three allowed ways to have octal escape characters:
					//  \[0..3]nn or \nn or \n. We check for them in that order.
					if (   source <= length - 3
						&& text[source] >= '0' && text[source] <= '3'
						&& isodigit(text[source+1])
						&& isodigit(text[source+2])) {

						// We have the \[0..3]nn case
						char octal[4];
						octal[0] = text[source];
						octal[1] = text[source+1];
						octal[2] = text[source+2];
						octal[3] = 0;
						sscanf(octal, "%o", &number);
						new_char = number;
						source += 2; // to account for the two extra digits
					} else if (   source <= length -2
							   && isodigit(text[source+1])) {

						// We have the \nn case
						char octal[3];
						octal[0] = text[source];
						octal[1] = text[source+1];
						octal[2] = 0;
						sscanf(octal, "%o", &number);
						new_char = number;
						source += 1; // to account for the extra digit
					} else if (source <= length - 1) {
						char octal[2];
						octal[0] = text[source];
						octal[1] = 0;
						sscanf(octal, "%o", &number);
						new_char = number;
					} else {
						new_char = text[source];
					}
					if(number == 0) { // "\\0" is an invalid substring within a string literal
					  validStr = false;
					  delete [] copy;
					  return;
					}
				} else {
					new_char = text[source];
				}
				break;
			}
			copy[dest++] = new_char;
		}
	}
	copy[dest] = 0;
	text = copy;
	delete [] copy;
	return;
}

void 
getLocalTime(time_t *now, struct tm *localtm) {

#ifndef WIN32
	localtime_r( now, localtm );
#else
	// there is no localtime_r() on Windows, so for now
	// we just call localtime() and deep copy the result.

	struct tm *lt_ptr; 

	lt_ptr = localtime(now);

	if (localtm == NULL) { return; } 

	localtm->tm_sec   = lt_ptr->tm_sec;    /* seconds */
	localtm->tm_min   = lt_ptr->tm_min;    /* minutes */
	localtm->tm_hour  = lt_ptr->tm_hour;   /* hours */
	localtm->tm_mday  = lt_ptr->tm_mday;   /* day of the month */
	localtm->tm_mon   = lt_ptr->tm_mon;    /* month */
	localtm->tm_year  = lt_ptr->tm_year;   /* year */
	localtm->tm_wday  = lt_ptr->tm_wday;   /* day of the week */
	localtm->tm_yday  = lt_ptr->tm_yday;   /* day in the year */
	localtm->tm_isdst = lt_ptr->tm_isdst;  /* daylight saving time */
	
#endif
	
}

END_NAMESPACE // classad
