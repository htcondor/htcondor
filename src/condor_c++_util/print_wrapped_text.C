/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"
#include "print_wrapped_text.h"

void print_wrapped_text(
    const char *text, 
	FILE *output, 
	int chars_per_line)
{
	char *text_copy, *t, *token;
	int char_count;

	text_copy = strdup(text);
	t = text_copy;

	char_count = 0;
	token = strtok(text_copy, " \t");
	while (token != NULL) {
		int token_length;
		token_length = strlen(token);
		if (token_length < (chars_per_line - char_count)) {
			fprintf(output, "%s", token);
			char_count += token_length;
		}
		else {
			// We should do a better job here: we might have
			// some text that is longer than chars_per_line long.
			fprintf(output, "\n%s", token);
			char_count = token_length;
		}
		if (char_count < chars_per_line) {
			fprintf(output, " ");
			char_count++;
		} else {
			fprintf(output, "\n");
			char_count = 0;
		}			
		token = strtok(NULL, " \t");
	}
	fprintf(output, "\n");

	free(t);
	return;
}
