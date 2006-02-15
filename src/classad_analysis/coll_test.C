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

#include "condor_common.h"
#include "analysis.h"

int main( ) {

	ClassAdCollectionServer server;
	string filename = "serverlog";
	server.InitializeFromLog( filename, "", "" );

	ClassAdParser parser;
	ClassAdUnParser unparser;
	ClassAdAnalyzer analyzer;
		/*
		  ClassAd *classad_a;
		  int input;
		  int count=1;
		  int viewc=1;
		*/
	ClassAd* one_cla;
	string machine( "[ Type = \"Machine\"; Disk = 333333; Arch =
\"SUN\"; OpSys = \"solaris\";Memory = 22222;]" );

	one_cla = parser.ParseClassAd( machine,true );
	string mkey( "m1" );
	if ( !server.AddClassAd( mkey, one_cla ) ) {
		printf( "AddClassAd failed!\n" );
		exit( 1 );
	}
	string job( "[ Type = \"job\"; owner = \"zs\"; requirements = other.Type
== \"Machine\" && Arch == \"INTEL\" && OpSys == \"LINUX\" && Disk >= 100000 &&
other.Memory >= 10000; ]" );
	string buffer;
	one_cla = parser.ParseClassAd( job,true );
	if( one_cla == NULL ) {
		cout << "one_cla == NULL" << endl;
	}

	bool ret;
	cout << "here we are" << endl;
	ret = analyzer.AnalyzeJobReqToBuffer( one_cla, server, buffer );
	if( !ret ) {
		cout << "ret is false" << endl;
	}
	cout << buffer << endl;

	cout << "end show " << endl;

}
