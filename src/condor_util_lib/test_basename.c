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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basename.h"

//TEMP static const char * VERSION = "0.9.0";

int // 0 = OK, 1 = failed
TestAPath(const char *path, const char *expectedDir,
		const char *expectedFile)
{
	int result = 0;

	printf("\nTesting path <%s>\n", path);
	char *dir = condor_dirname(path);
	char *file = condor_basename(path);

	if ( strcmp(dir, expectedDir) ) {
		printf("ERROR: dirname returned <%s>, should have returned <%s>\n",
				dir, expectedDir);
		result = 1;
	} else {
		printf("dirname OK\n");
	}

	if ( strcmp(file, expectedFile) ) {
		printf("ERROR: basename returned <%s>, should have returned <%s>\n",
				file, expectedFile);
		result = 1;
	} else {
		printf("basename OK\n");
	}

	free(dir);

	return result;
}

//-------------------------------------------------------------------------

int
main(int argc, char **argv)
{
	int result = 0;

	result |= TestAPath(NULL, ".", "");
	result |= TestAPath("", ".", "");
	result |= TestAPath(".", ".", ".");
	result |= TestAPath("f", ".", "f");
	result |= TestAPath("foo", ".", "foo");
	result |= TestAPath("/foo", "/", "foo");
	result |= TestAPath("\\foo", "\\", "foo");
	result |= TestAPath("foo/bar", "foo", "bar");
	result |= TestAPath("/foo/bar", "/foo", "bar");
	result |= TestAPath("\\foo\\bar", "\\foo", "bar");
	result |= TestAPath("foo/bar/", "foo/bar", "");

	// Okay, this should either return "" and "/foo/bar" or "bar" and "/foo",
	// but it actually returns "" and "/foo"!!
	result |= TestAPath("/foo/bar/", "/foo/bar", "");

	result |= TestAPath("/", "/", "");
	result |= TestAPath("\\", "\\", "");
	result |= TestAPath("./bar", ".", "bar");
	result |= TestAPath(".\\bar", ".", "bar");
	result |= TestAPath("./", ".", "");
	result |= TestAPath(".\\", ".", "");
	result |= TestAPath("\\.", "\\", ".");

	result |= TestAPath("foo/bar/zap.txt", "foo/bar", "zap.txt");
	result |= TestAPath("foo\\bar\\zap.txt", "foo\\bar", "zap.txt");
	result |= TestAPath(".foo/bar", ".foo", "bar");
	result |= TestAPath(".foo\\bar", ".foo", "bar");
	result |= TestAPath(".foo/.bar.txt", ".foo", ".bar.txt");
	result |= TestAPath(".foo\\.bar.txt", ".foo", ".bar.txt");

	if ( result == 0 ) {
		printf("\nTest OK\n");
	} else {
		printf("\nTest FAILED !!!!!!!!!!!!!!!!!!!!!!!!\n");
	}

	return result;
}
