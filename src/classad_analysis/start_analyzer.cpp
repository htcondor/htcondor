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
#include "analysis.h"

void usage( char *myName );
int readFileIntoString( char* fileName, char* resultString, int size);

int main (int argc, char *argv[])
{
	const int SIZE = 4096;

	if( argc != 3 ) {
		usage( argv[0] );		
		exit(1);
	}

	char jobString[SIZE];
	char machineString[SIZE];

	readFileIntoString( argv[2], machineString, SIZE );

	readFileIntoString( argv[1], jobString, SIZE );

	classad::ClassAd *jobAd, *machineAd;
	classad::ClassAdParser parser;
	ClassAdAnalyzer analyzer;
	std::string buffer;
	std::string start = ATTR_START;

		// Get the job ad
	if( !( jobAd = parser.ParseClassAd( jobString ) ) ) {
		std::cerr << "error parsing job ad\n" << std::endl;
		exit(1);
	}

		// Get the machine ad
	if( !( machineAd = parser.ParseClassAd( machineString ) ) ) {
		std::cerr << "error parsing machine ad\n" << std::endl;
		exit(1);
	}

		// Do analysis
	if( !( analyzer.AnalyzeExprToBuffer( machineAd, jobAd, start,
										 buffer ) ) ) {
		std::cerr << "error analyzing expression\n" << std::endl;
		exit(1);
	}

	std::cout << buffer;
}

void usage( char *myName ) {
	printf( "Usage: %s <jobfile> <machinefile>\n", myName );
}

int readFileIntoString(char* fileName, char* resultString, int size){
  // this is a retarded way to read the file, but it works.
  int fd = safe_open_wrapper_follow(fileName, O_RDONLY);
  if(fd < 0){
    std::cerr << "couldn't open" << fileName << std::endl;
    exit(1);
  }
  int nread = read(fd, resultString, size);
  if(nread < 0){
    std::cerr << "couldn't read" << fileName << std::endl;
    exit(1);
  }    
  close(fd);
  resultString[nread] = '\0';
  return 0;
}
