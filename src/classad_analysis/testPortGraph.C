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
#include "portGraph.h"

int main( int argc, char *argv[] )
{
	classad::ClassAdParser adParser;
	classad::PrettyPrint pp;
	classad::ClassAd* ad;
	classad::ClassAd* bigAd;
	std::string buffer = "";

	std::string ref = "foo.bar";
	std::string label = "";
	std::string attr = "";
	std::vector<classad::ClassAd*> ads;

		// ad (1) 
	buffer += "[";
	buffer += "  Ports =";
 	buffer += "    {";
	buffer += "       [";
	buffer += "          Label = \"ad1\";";
	buffer += "          Type =\"a\";";
	buffer += "          Requirements = ad1.Type == \"b\"";
	buffer += "       ],";
	buffer += "       [";
	buffer += "          Label = \"ad2\";";
	buffer += "          Type = \"a\";";
	buffer += "          Requirements = ad2.Type == \"b\"";
	buffer += "       ],";
	buffer += "       [";
	buffer += "          Label = \"ad3\";";
	buffer += "          Type = \"c\";";
	buffer += "          Requirements = (ad3.Type == \"d\" &&"; 
	buffer += "                          ad3.Foo == ad1.Foo &&";
	buffer += "                          ad3.Bar == ad2.Bar) ||";
	buffer += "                         (ad3.Type == \"d\" && ";
	buffer += "                          ad3.Foo == ad2.Foo &&";
	buffer += "                          ad3.Bar == ad1.Bar)";
	buffer += "       ]";
	buffer += "    }";
	buffer += "]";

	bigAd = adParser.ParseClassAd( buffer );
	if( bigAd ) {
		buffer = "";
		pp.Unparse( buffer, bigAd );
//		cout << "bigAd = " << buffer << endl;
		buffer = "";
	}
	else{
		cout << "error parsing ClassAd" << endl;
		exit(1);
	}
	ads.push_back( bigAd );
		
		// ad (2)
	buffer += "[";
	buffer += "  Ports =";
 	buffer += "    {";
	buffer += "       [";
	buffer += "          Label = \"ad1\";";
	buffer += "          Type =\"a\";";
	buffer += "          Requirements = ad1.Type == \"b\"";
	buffer += "       ],";
	buffer += "       [";
	buffer += "          Label = \"ad2\";";
	buffer += "          Foo = ad1.Foo;";
	buffer += "          Type = \"b\";";
	buffer += "          Requirements = ad2.Type == \"a\"";
	buffer += "       ]";
 	buffer += "    }";
	buffer += "]";

	bigAd = adParser.ParseClassAd( buffer );
	if( bigAd ) {
		buffer = "";
		pp.Unparse( buffer, bigAd );
//		cout << "bigAd = " << buffer << endl;
		buffer = "";
	}
	else{
		cout << "error parsing ClassAd" << endl;
		exit(1);
	}
	ads.push_back( bigAd );
		
/*
		// test splitString
	cout << "test splitString( )" << endl;
	cout << "-------------------" << endl;
	splitString( ref, label, attr );
	cout << "ref = " << ref << endl;
	cout << "label = " << label << endl;
	cout << "attr = " << attr << endl;
	cout << endl;

		// ExtAttrNode tests
	cout << "test ExtAttrNode( )" << endl;
	cout << "-------------------" << endl;
	ExtAttrNode* eNode1 = new ExtAttrNode( );
	eNode1->ToString( buffer );
	cout << "eNode1 = " << buffer << endl;
	buffer = "";

	ExtAttrNode* eNode2 = new ExtAttrNode( );
	eNode2->ToString( buffer );
	cout << "eNode2 = " << buffer << endl;
	buffer = "";
	cout << endl;

		// AttrNode tests
	cout << "test AttrNode( )" << endl;
	cout << "----------------" << endl;	
	AttrNode* aNode1 = new AttrNode( );	
	aNode1->ToString( buffer );
	cout << "aNode1 = " << buffer << endl;
	buffer = "";
	cout << endl;

	cout << "test AttrNode::AddDep( )" << endl;
	cout << "------------------------" << endl;
	cout << "aNode1->AddDep( eNode1 )" << endl;
	aNode1->AddDep( eNode1 );
	aNode1->ToString( buffer );
	cout << "aNode1 = " << buffer << endl;
	buffer = "";
	cout << endl;

	cout << "test AttrNode( Value )" << endl;
	cout << "----------------------" << endl;	
	classad::Value val;
	val.SetStringValue( "foo" );
	AttrNode* aNode2 = new AttrNode( val );	
	aNode2->ToString( buffer );
	cout << "aNode2 = " << buffer << endl;
	buffer = "";
	cout << endl;

	cout << "test AttrNode::AddDep( )" << endl;
	cout << "------------------------" << endl;
	cout << "aNode2->AddDep( eNode2 )" << endl;
	aNode2->AddDep( eNode2 );
	aNode2->ToString( buffer );
	cout << "aNode2 = " << buffer << endl;
	buffer = "";
	cout << endl;

		// PortNode tests
	cout << "test PortNode::PortNode( )" << endl;
	cout << "--------------------------" << endl;
	PortNode* pNode1 = new PortNode( );
	pNode1->ToString( buffer );
	cout << "pNode1 = " << buffer << endl;
	buffer = "";
	cout << endl;

	cout << "test PortNode::PortNode( ClassAd, int, int)" << endl;
	cout << "-------------------------------------------" << endl;
	ad = new classad::ClassAd( );
	ad->InsertAttr( "attr", "foo" );
	PortNode* pNode2 = new PortNode( ad, 2, 5 );
	pNode2->ToString( buffer );
	cout << "pNode2 = " << buffer << endl;
	buffer = "";
	cout << endl;
	
	cout << "test PortNode::AddAttrNode( )" << endl;
	cout << "-----------------------------" << endl;
	cout << "pNode2->AddAttrNode( \"attr\" )" << endl;
	pNode2->AddAttrNode( "attr", val );
	pNode2->ToString( buffer );
	cout << "pNode2 = " << buffer << endl;
	buffer = "";
	cout << endl;

	cout << "test PortNode::AddExtAttrNode( )" << endl;
	cout << "--------------------------------" << endl;
	cout << "pNode2->AddExtAttrNode( \"extAttr\" )" << endl;
	pNode2->AddExtAttrNode( "extAttr" );
	pNode2->ToString( buffer );
	cout << "pNode2 = " << buffer << endl;
	buffer = "";
	cout << endl;

	cout << "test PortNode::AddReqDep( )" << endl;
	cout << "---------------------------" << endl;
	cout << "pNode2->AddReqDep( eNode1 )" << endl;
	pNode2->AddReqDep( eNode1 );
	pNode2->ToString( buffer );
	cout << "pNode2 = " << buffer << endl;
	buffer = "";
	cout << endl;

*/

		// PortGraph tests
	cout << "test PortGraph( )" << endl;
	cout << "-----------------" << endl;
	PortGraph graph;
	graph.ToString( buffer );
	cout << "graph = " << buffer << endl;
	buffer = "";
	cout << endl;

	cout << "test Initialize( )" << endl;
	cout << "------------------" << endl;
	cout << "graph.Initialize( ads )" << endl;
	graph.Initialize( ads );
	graph.ToString( buffer );
	cout << "graph = " << buffer << endl;
	buffer = "";
	cout << endl;
}
