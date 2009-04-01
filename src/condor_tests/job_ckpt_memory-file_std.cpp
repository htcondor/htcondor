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


/*
This program extensively tests seeking, reading, and writing.
A random set of operations are performed on a real file and 
a mirror_file object.  After every operation, the contents of 
the real file and the mirror_file are compared.

(They should be identical!)

In order to facilitate debugging, the program prints out its random
seed key and proceeds quietly.  If a discrepancy is found, the errors
in the buffer are displayed, and the program ends.

To reproduce the same sequence in more detail, use -k to specify the
same random key as the erroneous run, and use -d to display the sequence
of operations performed.

This program may also be used as a rough i/o benchmark by specifying
-z.  In this mode, all that happens is a rapid series of reads, writes,
and seeks.

-h gives all the options.

thain Wed Jan 27 1999
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <iostream>

using namespace std;

/*
This is really ugly, but memory_file just refused to link
in when cplus_lib was compiled with g++, and lseek was
compiled with CC.  Go figure.
*/

/* This stupid hack is here to not bring in anything other than the memory file
	code because some vendor C++ compilers don't understand the bool keyword
	which ends up being used in the condor_common.h header file. -psilord */
#define NO_CONDOR_COMMON
#include "../../condor_c++_util/memory_file.h"
#include "../../condor_c++_util/memory_file.cpp"

void use(char *program)
{
	cout << "Use: " << program << " [option]*" << endl
	     << "Options are:\n"
	     << "\t-s <size>     Size of file to exercise\n"
	     << "\t-f <filename> File name to exercise\n"
	     << "\t-o <number>   Operations to perform\n"
	     << "\t-k <key>      Random key to reproduce a sequence\n"
	     << "\t-d            Debug mode -- show all operations\n"
	     << "\t-z            Zoom mode -- skip safety tests.\n"
	     << "\t-h            Display options\n";

	exit(-1);
}

int main( int argc, char *argv[] )
{
	char	*filename="lseek.tempfile";
	int	filesize=10000;
	int	operations=100;
	int	random_key=time(0);
	int	debug=0;
	int	zoom=0;
	int	i;

	// Process arguments

	for( i=1; i<argc; i++ ) {
		if( argv[i][0]!='-' ) use(argv[0]);
			switch( argv[i][1] ) {
				case 'd':
				debug=1;
				break;

				case 'f':
				i++;
				if(i>=argc) use(argv[0]);
				filename=argv[i];
				break;

				case 's':
				i++;
				if(i>=argc) use(argv[0]);
				filesize=atol(argv[i]);
				break;

				case 'o':
				i++;
				if(i>=argc) use(argv[0]);
				operations=atol(argv[i]);
				break;

				case 'k':
				i++;
				if(i>=argc) use(argv[0]);
				random_key=atol(argv[i]);
				break;

				case 'z':
				zoom = 1;
				break;

				default:
				use(argv[0]);
				break;
		}
	}

	// Show the parameters so we can reproduce errors

       	cout << "Random key: " << random_key << endl;
	cout << "Filesize: " << filesize << " (approx) " << endl;
	cout << "Filename: " << filename << endl;
	cout << "Operations: " << operations << endl;

	memory_file memory;

	int fd = open(filename,O_CREAT|O_RDWR|O_TRUNC,0600);
	if( fd<0 ) {
		cerr << "Unable to open " << filename << endl;
		return -1;
	}

	srand( random_key );

	char *buffer = new char[filesize];
	char *buffer2 = new char[filesize];

	int	errors=0;
	off_t	pos;
	size_t	length;

	time_t	begin = time(0);

	// Choose between these operations:
	// - Move the seek pointer
	// - Write the buffer to disk
	// - Read the buffer from disk
	// - Fill the buffer with new random contents

	for( i=0; i<operations && !errors; i++ ) {

		if(debug) cout << i << '\t';

		switch( rand()%15 ) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			pos = rand()%filesize;
			if(debug) cout << "Seeking to " << pos << endl;
			lseek(fd,pos,SEEK_SET);
			if(!zoom) memory.seek(pos,SEEK_SET);
			break;

			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			length = rand()%filesize;
			if(debug) cout << "Writing " << length << " bytes" << endl;
			write(fd,buffer,length);
			if(!zoom) memory.write(buffer,length);
			break;

			case 10:
			case 11:
			case 12:
			case 13:
			length = rand()%filesize;
			if(debug) cout << "Reading " << length << " bytes" << endl;
			if(!zoom) {
				memset(buffer, 0, length);
				memset(buffer2, 0, length);
				memory.read(buffer,length);
			}
			read(fd,buffer2,length);
			if(!zoom) {
				if(debug) cout << "Comparing read buffers..." << endl;
				errors += count_errors(buffer,buffer2,length,0);
			}
			break;

			case 14:
			if(!zoom) {
				if(debug) cout << "Generating new data..." << endl;
				for( off_t j=0; j<filesize; j++ )
					buffer[j] = rand();
			}
			break;
		}

		if(!zoom) {
			if(debug) cout << "Comparing memory_file against file..." << endl;
			errors += memory.compare(filename);
			if(debug) cout << "Comparing seek pointers..." << endl;
			int msp = memory.seek(0,SEEK_CUR);
			int rsp = lseek(fd,0,SEEK_CUR);
			if(msp!=rsp) {
				cout << "SEEK ERROR" << endl;
				cout << "memory: " << msp << "file: " << rsp << endl;
				errors++;
			}
		}
	}

	time_t end = time(0);
	
	unlink(filename);

	if(zoom) {
		cout << operations << " ops in " << (end-begin) << " seconds.\n";
		if((end-begin)>0) {
			cout << operations/(end-begin) << " ops per second." << endl;
		}
	}

	if(!errors) {
		cout << "SUCCESS" << endl;
		return 0;
	} else {
		cout << "FAILURE" << endl;
		volatile int i=5,j=5;
		// Force a core dump.
		i = i/i-j;
		return -1;
	}
}

