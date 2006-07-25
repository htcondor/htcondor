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
