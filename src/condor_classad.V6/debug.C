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

#include "common.h"
#include "debug.h"
#include <stdarg.h>

using namespace std;
BEGIN_NAMESPACE(classad)

int  _except_line_number = -1;
char *_except_file_name = NULL;

void _classad_except(char *format, ...)
{
	va_list arguments;

	fprintf(stderr, "**** ClassAd Failure in %s, line %d:\n",
			_except_file_name ? _except_file_name : "<unknown file>",
			_except_line_number);

	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	exit(1);
	return;
}
END_NAMESPACE
