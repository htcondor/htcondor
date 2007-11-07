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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basename.h"

//TEMP static const char * VERSION = "0.9.1";

int // 0 = OK, 1 = failed
TestAPath(const char *path, const char *expectedDir,
		const char *expectedFile)
{
	int result = 0;

	printf("\nTesting path <%s>\n", path);
	char *dir = condor_dirname(path);
	const char *file = condor_basename(path);

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

int // 0 = OK, 1 = failed
TestForFull(const char *path, int expectFull)
{
	int result = 0;

	printf("\nTesting fullpath on <%s>\n", path);

	int tmpResult = fullpath(path);
	if ( tmpResult == expectFull ) {
		printf( "fullpath OK\n" );
	} else {
		printf("ERROR: fullpath returned <%d>, should have returned <%d>\n",
				tmpResult, expectFull);
		result = 1;
	}

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

	result |= TestAPath("//foo/bar/zap.txt", "//foo/bar", "zap.txt");
	result |= TestAPath("\\\\foo\\bar\\zap.txt", "\\\\foo\\bar", "zap.txt");

	result |= TestForFull("/tmp/foo", 1);
	result |= TestForFull("tmp/foo", 0);
	result |= TestForFull("c:\\", 1);
	result |= TestForFull(":\\", 0);
	result |= TestForFull("\\", 1);
	result |= TestForFull("x:/", 1);
	result |= TestForFull(":/", 0);

	if ( result == 0 ) {
		printf("\nTest OK\n");
	} else {
		printf("\nTest FAILED !!!!!!!!!!!!!!!!!!!!!!!!\n");
	}

	return result;
}
