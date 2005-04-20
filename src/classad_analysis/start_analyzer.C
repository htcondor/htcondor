/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
	string buffer;
	string start = ATTR_START;

		// Get the job ad
	if( !( jobAd = parser.ParseClassAd( jobString ) ) ) {
		cerr << "error parsing job ad\n" << endl;
		exit(1);
	}

		// Get the machine ad
	if( !( machineAd = parser.ParseClassAd( machineString ) ) ) {
		cerr << "error parsing machine ad\n" << endl;
		exit(1);
	}

		// Do analysis
	if( !( analyzer.AnalyzeExprToBuffer( machineAd, jobAd, start,
										 buffer ) ) ) {
		cerr << "error analyzing expression\n" << endl;
		exit(1);
	}

	cout << buffer;
}

void usage( char *myName ) {
	printf( "Usage: %s <jobfile> <machinefile>\n", myName );
}

int readFileIntoString(char* fileName, char* resultString, int size){
  // this is a retarded way to read the file, but it works.
  int fd = open(fileName, O_RDONLY);
  if(fd < 0){
    cerr << "couldn't open" << fileName << endl;
    exit(1);
  }
  int nread = read(fd, resultString, size);
  if(nread < 0){
    cerr << "couldn't read" << fileName << endl;
    exit(1);
  }    
  close(fd);
  resultString[nread] = '\0';
  return 0;
}
