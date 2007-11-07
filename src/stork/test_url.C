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

#include <stdio.h>
#include <string.h>
#include "dap_url.h"

#define NUM_ELEM(_ary) ( (sizeof(_ary)) / (sizeof(_ary)[0] ) )

static size_t total = 0;
static size_t fail = 0;

int compare(char *field, char *verify, char *test)
{
	char *result;
	int status;
	total++;
	if (! verify) {
		verify = "(null)";
	}

	if (! test) {
		test = "(null)";
	}

	if (strcmp(verify, test) ) {
		fail++;
		result = "FAIL";
		status = 0;
	} else{
		result = "pass";
		status = 1;
	}

	printf("%s::	verify: '%s'		test: '%s'		status: %s\n",
			field, verify, test, strcmp(verify, test) ? "FAIL" : "pass" );
	return status;
}

int tester(
	char *url,
	char *nopw,
	char *scheme,
	char *user,
	char *password,
	char *host,
	char *port,
	char *path,

	int success
)
{
	Url parsed(url);
	printf("\n%s\n", url);
	compare("scheme", scheme, parsed.scheme() );
	compare("nopw", nopw, parsed.urlNoPassword() );
	compare("user", user, parsed.user() );
	compare("password", password, parsed.password() );
	compare("host", host, parsed.host() );
	compare("port", port, parsed.port() );
	compare("path", path, parsed.path() );

	{
		total++;
		if (parsed.parsed() == success) {
			printf("success::	verify: %d		test: %d\n		status: %s\n",
					success, parsed.parsed(), "pass");
		} else {
			fail++;
			printf("success::	verify: %d		test: %d\n		status: %s\n",
					success, parsed.parsed(), "FAIL");
		}
	}

	return parsed.parsed();
}

int srbTester(
	char *url,
	char *scheme,
	char *srbUser,
	char *srbMdasDomain,
	char *srbZone,
	char *dirname,
	char *basename,

	int success
)
{
	Url parsed(url);
	parsed.parseSrb();

	printf("\n%s\n", url);
	compare("scheme", scheme, parsed.scheme() );
	compare("srbUser", srbUser, parsed.srbUser() );
	compare("srbMdasDomain", srbMdasDomain, parsed.srbMdasDomain() );
	compare("srbZone", srbZone, parsed.srbZone() );
	compare("dirname", dirname, parsed.dirname() );
	compare("basename", basename, parsed.basename() );

	{
		total++;
		if (parsed.parsedSrb() == success) {
			printf("success::	verify: %d		test: %d\n		status: %s\n",
					success, parsed.parsedSrb(), "pass");
		} else {
			fail++;
			printf("success::	verify: %d		test: %d\n		status: %s\n",
					success, parsed.parsedSrb(), "FAIL");
		}
	}

	return parsed.parsedSrb();
}

int main(int argc, char *argv[])
{
	tester(
			"srb://weber:secret@north.cs.wisc.edu:80/dir/file",
			"srb://weber:******@north.cs.wisc.edu:80/dir/file",
			"srb",		// scheme
			"weber",		// user
			"secret",		// password
			"north.cs.wisc.edu",// host
			"80",		// port
			"/dir/file",// path
			1			// success
		  );

	tester(
			"file:",
			"file:",
			NULL,		// scheme
			NULL,		// user
			NULL,		// password
			NULL,		// host
			NULL,		// port
			NULL,		// path
			0			// success
		  );

	tester(
			"file:/",
			"file:/",
			"file",		// scheme
			NULL,		// user
			NULL,		// password
			NULL,		// host
			NULL,		// port
			NULL,		// path
			0			// success
		  );

	tester(
			"file:/dir/file",
			"file:/dir/file",
			"file",		// scheme
			NULL,		// user
			NULL,		// password
			"localhost",// host
			NULL,		// port
			"/dir/file",// path
			1			// success
		  );

	tester(
			"file:///dir/file",
			"file:///dir/file",
			"file",		// scheme
			NULL,		// user
			NULL,		// password
			"localhost",// host
			NULL,		// port
			"/dir/file",// path
			1			// success
		  );

	tester(
			"file://dir/file",
			"file://dir/file",
			"file",		// scheme
			NULL,		// user
			NULL,		// password
			NULL,		// host
			NULL,		// port
			NULL,		// path
			0			// success
		  );

	tester(
			"file:///dir1/dir2/",
			"file:///dir1/dir2/",
			"file",		// scheme
			NULL,		// user
			NULL,		// password
			"localhost",// host
			NULL,		// port
			"/dir1/dir2/",// path
			1			// success
		  );

	tester(
			"srb://weber:secret@north.cs.wisc.edu:80/dir/file",
			"srb://weber:******@north.cs.wisc.edu:80/dir/file",
			"srb",		// scheme
			"weber",		// user
			"secret",		// password
			"north.cs.wisc.edu",// host
			"80",		// port
			"/dir/file",// path
			1			// success
		  );

	tester(
			"srb://north.cs.wisc.edu/dir/file",
			"srb://north.cs.wisc.edu/dir/file",
			"srb",		// scheme
			NULL,		// user
			NULL,		// password
			"north.cs.wisc.edu",// host
			NULL,		// port
			"/dir/file",// path
			1			// success
		  );

	tester(
			"srb://weber:@north.cs.wisc.edu:80/dir/file",
			"srb://weber:@north.cs.wisc.edu:80/dir/file",
			"srb",		// scheme
			"weber",		// user
			NULL,		// password
			NULL,		//host
			NULL,		// port
			NULL,// path
			0			// success
		  );

	tester(
			"srb://north/",
			"srb://north/",
			"srb",		// scheme
			NULL,		// user
			NULL,		// password
			"north",		// host
			NULL,		// port
			NULL,		// path
			1			// success
		  );

	tester(
			"srb://foo@north:81/",
			"srb://foo@north:81/",
			"srb",		// scheme
			"foo",		// user
			NULL,		// password
			"north",	// host
			"81",		// port
			NULL,		// path
			1			// success
		  );

	srbTester("ftp://north:81/",
			"ftp",		// scheme
			NULL,		// srbUser
			NULL,		// srbMdasDomain
			NULL,		// srbZone
			NULL,		// dirname
			NULL,		// basename
			0			// success
		  );

	srbTester("srb://north:81/",
			"srb",		// scheme
			NULL,		// srbUser
			NULL,		// srbMdasDomain
			NULL,		// srbZone
			NULL,		// dirname
			NULL,		// basename
			1			// success
		  );

	srbTester("srb://foo@north:81/",
			"srb",		// scheme
			"foo",		// srbUser
			NULL,		// srbMdasDomain
			NULL,		// srbZone
			NULL,		// dirname
			NULL,		// basename
			1			// success
		  );

	srbTester("srb://foo.dn@north:81/",
			"srb",		// scheme
			"foo",		// srbUser
			"dn",		// srbMdasDomain
			NULL,		// srbZone
			NULL,		// dirname
			NULL,		// basename
			1			// success
		  );

	srbTester("srb://foo.dn.zn@north:81/",
			"srb",		// scheme
			"foo",		// srbUser
			"dn",		// srbMdasDomain
			"zn",		// srbZone
			NULL,		// dirname
			NULL,		// basename
			1			// success
		  );

	srbTester("srb://foo.dn.zn@north:81/col/lection/object",
			"srb",		// scheme
			"foo",		// srbUser
			"dn",		// srbMdasDomain
			"zn",		// srbZone
			"/col/lection",		// dirname
			"object",		// basename
			1			// success
		  );

	srbTester("srb://foo.dn.zn:secret@north:81/",
			"srb",		// scheme
			"foo",		// srbUser
			"dn",		// srbMdasDomain
			"zn",		// srbZone
			NULL,		// dirname
			NULL,		// basename
			1			// success
		  );

	srbTester("srb://foo.dn.zn:secret@north:81/dir",
			"srb",		// scheme
			"foo",		// srbUser
			"dn",		// srbMdasDomain
			"zn",		// srbZone
			"/dir",		// dirname
			NULL,		// basename
			1			// success
		  );

	srbTester("srb://srb.sdsc.edu/home/kosart.condor/termcap",
			"srb",		// scheme
			NULL,		// srbUser
			NULL,		// srbMdasDomain
			NULL,		// srbZone
			"/home/kosart.condor",	// dirname
			"termcap",	// // basename
			1			// success
		  );

	srbTester("srb://srb.sdsc.edu/home/kosart.condor/termcap/",
			"srb",		// scheme
			NULL,		// srbUser
			NULL,		// srbMdasDomain
			NULL,		// srbZone
			"/home/kosart.condor/termcap",	//dirname
			NULL,		// basename
			1			// success
		  );

	printf("total: %u  failed: %u\n", total, fail);
			return (fail == 0);
}
