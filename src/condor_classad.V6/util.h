/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __UTILS_H__
#define __UTILS_H__

#include "common.h"

BEGIN_NAMESPACE( classad )

// macro for recognising octal digits
#define isodigit(x) (( (x) - '0' < 8 ) && ((x) - '0' >= 0))

// A structure which represents the Absolute Time Literal
struct abstime_t 
{
	time_t secs;   // seconds from the epoch (UTC) 
	int    offset; // seconds east of Greenwich 
};

/* This converts a string so that sequences like \t
 * (two-characters, slash and t) are converted into the 
 * correct characters like tab. It also converts octal sequences. 
 */
void convert_escapes(std::string &text, bool &validStr); 

void getLocalTime(time_t *now, struct tm *localtm);

END_NAMESPACE // classad

#endif//__UTILS_H__
