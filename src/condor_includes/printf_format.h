/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
