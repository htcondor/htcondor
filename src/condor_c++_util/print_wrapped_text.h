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

#ifndef __PRINT_WRAPPED_TEXT_H
#define __PRINT_WRAPPED_TEXT_H

/** Print a single paragraph of text, wrapping it properly on each
    line. This is particularly useful when you don't know how long
    your text will be in advance, like when you have a filename as
    part of your text.  You should not use tabs or newlines embedded
    in the string, as that will confuse this method.  Tabs at the
    beginning and extra newlines at the end are fine, however.  You
    get a single newline at the end of the paragraph automatically. 
	@param text The text to print.
	@param output Where to print the text to, like stdout
	@param chars_per_line The maximum number of characters to print per line.
           This defaults to 78 characters, which is a reasonable guess.
*/
void print_wrapped_text(
    const char *text, 
	FILE *output, 
	int chars_per_line = 78);

#endif
