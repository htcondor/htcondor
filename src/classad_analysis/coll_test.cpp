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

int main( ) {

	classad::ClassAdCollectionServer server;
	std::string filename = "serverlog";
	server.InitializeFromLog( filename, "", "" );

	classad::ClassAdParser parser;
	classad::ClassAdUnParser unparser;
	ClassAdAnalyzer analyzer;
		/*
		  ClassAd *classad_a;
		  int input;
		  int count=1;
		  int viewc=1;
		*/
	ClassAd* one_cla;
	std::string machine( "[ Type = \"Machine\"; Disk = 333333; Arch =
\"SUN\"; OpSys = \"solaris\";Memory = 22222;]" );

	one_cla = parser.ParseClassAd( machine,true );
	std::string mkey( "m1" );
	if ( !server.AddClassAd( mkey, one_cla ) ) {
		printf( "AddClassAd failed!\n" );
		exit( 1 );
	}
	std::string job( "[ Type = \"job\"; owner = \"zs\"; requirements = other.Type
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

};
