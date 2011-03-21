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
