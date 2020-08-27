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
#include "conversion.h"

//void usage( char *myName );

int main ( int argc, char *argv[] )
{
	classad::ClassAdParser parser;
	classad::PrettyPrint pp;
	std::string buffer = "";
	std::string testAdStr = "[ a = 12; b = 5; c = ( d < 5 ) && ( b + e < 6); f = target.g]";
	classad::ClassAd *testAd = parser.ParseClassAd( testAdStr ); 
	classad::ClassAd *newTestAd = AddExplicitTargets( testAd );
	std::cout << "testAd = " << testAdStr << std::endl;
	pp.Unparse( buffer, newTestAd );
	std::cout << "newTestAd = " << buffer << std::endl;
	std::cout << std::endl;
	buffer = "";
		// MatchClassAd test

	std::string jobAdString = "";
	std::string machineAdString = "";

	jobAdString += "[";
	jobAdString += "Requirements = ( Arch == \"INTEL\" ) && ";
	jobAdString += "( OpSys == \"WINNT\" );";
	jobAdString += "]";

	machineAdString += "[";
	machineAdString += "Arch = \"INTEL\";";
	machineAdString += "OpSys = \"WINNT\";";
	machineAdString += "]";

	classad::ClassAd *jobAd = NULL;
	classad::ClassAd *machineAd = NULL;
	jobAd = parser.ParseClassAd( jobAdString );
	machineAd = parser.ParseClassAd( machineAdString );
	classad::ClassAd *newJobAd = NULL;
	classad::ClassAd *newMachineAd = NULL;
	bool match = false;
	classad::MatchClassAd mad;
	newJobAd = AddExplicitTargets( jobAd );
	newMachineAd = AddExplicitTargets( machineAd );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";
	std::cout << "ReplaceLeftAd( jobAd )" << std::endl;

	mad.ReplaceLeftAd( jobAd );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";
	std::cout << "ReplaceRightAd( machineAd )" << std::endl;

	mad.ReplaceRightAd( machineAd );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";

	mad.EvaluateAttrBool( "rightMatchesLeft", match );
	if( match ) {
		std::cout << "MatchClassAd matches" << std::endl;
	}
	else {
		std::cout << "MatchClassAd doesn't match" << std::endl;
	}

	std::cout << "RemoveLeftAd( )" << std::endl;

	mad.RemoveLeftAd( );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";
	std::cout << "RemoveRightAd( )" << std::endl;

	mad.RemoveRightAd( );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";

	std::cout << "----------------------------------------" << std::endl;

	match = false;

	std::cout << "ReplaceLeftAd( newJobAd )" << std::endl;

	mad.ReplaceLeftAd( newJobAd );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";
	std::cout << "ReplaceRightAd( newMachineAd )" << std::endl;

	mad.ReplaceRightAd( newMachineAd );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";

	mad.EvaluateAttrBool( "rightMatchesLeft", match );
	if( match ) {
		std::cout << "MatchClassAd + target matches" << std::endl;
	}
	else {
		std::cout << "MatchClassAd + target doesn't match" << std::endl;
	}

	std::cout << "RemoveLeftAd( )" << std::endl;

	mad.RemoveLeftAd( );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";
	std::cout << "RemoveRightAd( )" << std::endl;

	mad.RemoveRightAd( );

	pp.Unparse( buffer, &mad );
	std::cout << "mad = " << buffer << std::endl;
	buffer = "";
}

