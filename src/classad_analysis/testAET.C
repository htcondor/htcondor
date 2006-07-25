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
	cout << "testAd = " << testAdStr << endl;
	pp.Unparse( buffer, newTestAd );
	cout << "newTestAd = " << buffer << endl;
	cout << endl;
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
	cout << "mad = " << buffer << endl;
	buffer = "";
	cout << "ReplaceLeftAd( jobAd )" << endl;

	mad.ReplaceLeftAd( jobAd );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";
	cout << "ReplaceRightAd( machineAd )" << endl;

	mad.ReplaceRightAd( machineAd );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";

	mad.EvaluateAttrBool( "rightMatchesLeft", match );
	if( match ) {
		cout << "MatchClassAd matches" << endl;
	}
	else {
		cout << "MatchClassAd doesn't match" << endl;
	}

	cout << "RemoveLeftAd( )" << endl;

	mad.RemoveLeftAd( );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";
	cout << "RemoveRightAd( )" << endl;

	mad.RemoveRightAd( );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";

	cout << "----------------------------------------" << endl;

	match = false;

	cout << "ReplaceLeftAd( newJobAd )" << endl;

	mad.ReplaceLeftAd( newJobAd );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";
	cout << "ReplaceRightAd( newMachineAd )" << endl;

	mad.ReplaceRightAd( newMachineAd );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";

	mad.EvaluateAttrBool( "rightMatchesLeft", match );
	if( match ) {
		cout << "MatchClassAd + target matches" << endl;
	}
	else {
		cout << "MatchClassAd + target doesn't match" << endl;
	}

	cout << "RemoveLeftAd( )" << endl;

	mad.RemoveLeftAd( );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";
	cout << "RemoveRightAd( )" << endl;

	mad.RemoveRightAd( );

	pp.Unparse( buffer, &mad );
	cout << "mad = " << buffer << endl;
	buffer = "";
}

