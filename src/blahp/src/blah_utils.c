/*
#  File:     blah_utils.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   30 Mar 2009 - Original release.
#
#  Description:
#   Utility functions for blah protocol
#
#
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "blah_utils.h"

const char *blah_omem_msg = "out\\ of\\ memory";

char *
make_message(const char *fmt, ...)
{
	/* Dynamically allocate a string of proper length
	   and initialize it with the provided parameters. 
	*/
	int n;
	char *result = NULL;
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap) + 1;
	va_end(ap);

	result = (char *) malloc (n);
	if (result)
	{
		va_start(ap, fmt);
		vsnprintf(result, n, fmt, ap);
		va_end(ap);
	}

	return(result);
}

char *
escape_spaces(const char *str)
{
	/* Makes message strings compatible with blah protocol:
	   escape white spaces with a backslash,
	   replace tabs with spaces, CR and LF with '-'.
	*/
	char *result = NULL;
	size_t i, j;
	size_t len = strlen(str);

	result = (char *) malloc (len * 2 + 1);
	if (result)
	{
		for (i = 0, j = 0; i <= len; i++)
		{
			switch (str[i])
			{
			case '\r':
				break;
			case '\n':
				if (str[i+1] == '\0') {
					break;
				}
				result[j++] = '-';
				break;
			case '\t':
			case ' ':
				result[j++] = '\\';
				result[j++] = ' ';
				break;
			default:
				result[j++] = str[i];
			}
		}
	}
	return(result);
}

