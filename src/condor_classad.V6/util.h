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

bool hex_to_double(const std::string &hex, double &number);

// The char buffer must be at least 17 bytes long. 
void double_to_hex(double number, char *hex);

/* This converts a string so that sequences like \t
 * (two-characters, slash and t) are converted into the 
 * correct characters like tab. It also converts octal sequences. 
 */
void convert_escapes(std::string &text, bool &validStr); 

END_NAMESPACE // classad

#endif//__VALUES_H__
