/***************************************************************
 *
 * Copyright (C) 2012, Condor Team, Computer Sciences Department,
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

/*

7.8.4 shipped with a bug that would lead to corrupt ACCOUNTANT_LOGs (aka
Accountantnew.log).  When the negotiator restarted, it would die after failing
to parse the log.  The problem is errant spaces in some of the messages.  This
program scans the file, writing a new copy with the errant spaces, then swaps
the new copy into place.

*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>

// Headers needed for unlink/_unlink
#ifdef WIN32
#	include <io.h>
#else
#	include <unistd.h>
#endif


#ifdef WIN32
#	define open _open
#	define O_WRONLY _O_WRONLY
#	define O_CREAT _O_CREAT
#	define O_EXCL _O_EXCL
#	define unlink _unlink
#else
#	define O_BINARY 0
#endif

bool readline(FILE * f, std::string & out);
std::string search_and_replace(std::string data, std::string search, std::string replace);
void rename_or_die(const char * src, const char * dst);
void fatal(const char * fmt, ...);

int main(int argc, char ** argv)
{
	if(argc != 2) {
		fatal("Usage: %s <Accountantnew.log>\n"
			"Fixes errors in <Accountantnew.log> introduced by Condor 7.8.4.\n", argv[0]);
	}

	const char * infilename = argv[1];

	FILE * infile = fopen(infilename, "rb");
	if(!infile) {
		fatal("Unable to read %s: %s\n", infilename, strerror(errno));
	}

	std::string outfilename = infilename;
	outfilename.append(".fixnew");
	// Jumping through hoops so we can verify we created the
	// file, avoid race conditions
	int outfilefd = open(outfilename.c_str(), O_WRONLY|O_CREAT|O_EXCL|O_BINARY,0644);
	if(outfilefd == -1) {
		fatal("Unable to write to %s. Does it already exist? %s\n", outfilename.c_str(), strerror(errno));
	}
	FILE * outfile = fdopen(outfilefd, "wb");
	if(!outfile) {
		fatal("Internal error: Unable to convert output file's FD to a FILE*. %s, %s\n", outfilename.c_str(), strerror(errno));
	}

	std::string line;
	while( readline(infile, line) ) {
		line = search_and_replace(line, "< ","<");
		line = search_and_replace(line, " , ",",");
		line = search_and_replace(line, " >",">");
		fprintf(outfile, "%s", line.c_str());
	}

	fclose(outfile);
	fclose(infile);

	std::string newinfilename = infilename;
	newinfilename.append(".fixold");
	rename_or_die(infilename, newinfilename.c_str());
	rename_or_die(outfilename.c_str(), infilename);
	if(unlink(newinfilename.c_str()) != 0) {
		fatal("Unable to remove %s: %s\n", newinfilename.c_str(), strerror(errno));
	}

	return 0;
}

void fatal(const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

void rename_or_die(const char * src, const char * dst)
{
	if(rename(src, dst) != 0) {
		fatal("Unable to rename %s to %s: %s\n", src, dst, strerror(errno));
	}
}

bool readline(FILE * f, std::string & out)
{
	out = "";
	char tmp[1024];
	bool more = true;
	do {
		char * ok = fgets(tmp, sizeof(tmp), f);
		if(!ok) {
			if(feof(f)) { more = false; break; } // Done!
			fatal("Unknown problem reading file\n");
		}
		out.append(tmp);
		// Why '\r' in the test? May exist on Macs. :-p
	} while( out[out.length()-1] != '\n' );
	return more;
}

std::string search_and_replace(std::string data, std::string search, std::string replace)
{
	size_t position = 0;
	while( (position = data.find(search, position)) != std::string::npos) {
		data.replace(position, search.length(), replace);
		position += replace.length();
	}
	return data;
}
