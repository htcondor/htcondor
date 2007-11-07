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

int main( int argc, char *argv[] )
{
	classad::ClassAdParser parser;
	classad::PrettyPrint pp;
	std::string newBuffer = "";
	MyString oldBuffer = "";

	newBuffer += "[";
	newBuffer += "MyType = \"job\";";
	newBuffer += "TargetType = \"machine\";";
	newBuffer += "intattr = 42;";
	newBuffer += "realattr = 3.14;";
	newBuffer += "stringattr = \"foo\";";
	newBuffer += "boolattr = true;";
	newBuffer += "attrrefattr = boolattr;";
	newBuffer += "opattr = intattr + realattr;";
	newBuffer += "]";

	classad::ClassAd *newAd = parser.ParseClassAd( newBuffer );
	newBuffer = "";
	pp.Unparse( newBuffer, newAd );
	cout << "New ClassAd:" << endl;
	cout << "------------" << endl;
	cout << newBuffer << endl << endl;
	ClassAd *oldAd = toOldClassAd( newAd );
	if( oldAd == NULL ) {
		cout << "toOldClassAd returned NULL" << endl;
	}
	else {
		oldAd->sPrint( oldBuffer );
		cout << "Old ClassAd:" << endl;
		cout << "------------" << endl;
		cout << oldBuffer << endl << endl;
	}

	newBuffer = "";
	newAd = toNewClassAd( oldAd );
	pp.Unparse( newBuffer, newAd );
	cout << "New ClassAd:" << endl;
	cout << "------------" << endl;
	cout << newBuffer << endl << endl;

}
