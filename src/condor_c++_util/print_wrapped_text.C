/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
