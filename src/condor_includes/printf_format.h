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

#ifndef _CONDOR_PRINTF_FORMAT_H
#define _CONDOR_PRINTF_FORMAT_H


typedef enum {
	PFT_NONE = 0,
	PFT_INT,
	PFT_FLOAT,
	PFT_CHAR,
	PFT_STRING,
	PFT_POINTER
} printf_fmt_t;


struct printf_fmt_info {
		/* What kind of format string are we? */
	char fmt_letter;		/* actual letter in the % escape */
	printf_fmt_t type;		/* our enum type defined above */

		/* standard modifiers */
	int width;
	int precision;

		/* special-case modifiers */
	int is_short;		/* if the 'h' flag is set */
	int is_long;		/* if the 'l' flag is set */
	int is_long_long;	/* if the 'll' flag is set */
	int is_long_double;	/* if the 'L' flag is set */
	int is_alt;       	/* "alternate" flag ('#') is set */
	int is_pad;       	/* if the zero-padding flag ('0') is set */
	int is_left;      	/* if the left-adjust flag ('-') is set */
	int is_space;     	/* if the ' ' flag is set for signed values */
	int is_signed; 		/* if the '+' flag is set to show sign */
	int is_grouped; 	/* if the ''' flag is set to show grouping */
};


BEGIN_C_DECLS

int parsePrintfFormat(const char **fmt_p, struct printf_fmt_info *info);

END_C_DECLS


#endif /* _CONDOR_PRINTF_FORMAT_H */
