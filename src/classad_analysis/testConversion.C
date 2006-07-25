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
