/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include "classad_distribution.h"
#include <iostream>

int main(int argc, char **argv)
{
	ClassAdXMLParser  parser;
	ClassAd  *classad;

	string  xml = "<?xml version=\"1.0\"?>< c >"
                  "<a n=\"A\"><s> Alain &quot;Aslag&quot; Roy</s></a>"
                  "<a n=\"B\"><n> 3 </n></a>"
		          "<a n=\"C\"><b v=\"t\"/></a>"
		          "<a n=\"D\"><l><n>10</n><un/><er/><n>14</n></l></a>"
		          "<a n=\"E\"><c><a n=\"AA\"><n>4</n><a n=\"BB\"><s>x</s></a></c></a>"
		          "<a n=\"F\"><e>(x >= 10)</e></a></c>";

	classad = parser.ParseClassAd(xml);

	PrettyPrint  printer;
	string       printed_classad;

	printer.Unparse(printed_classad, classad);
	cout << "ClassAd as parsed from XML:\n" << printed_classad << endl;

	printed_classad = "";
	ClassAdXMLUnParser  unparser;
	unparser.SetCompactSpacing(false);
	unparser.Unparse(printed_classad, classad);
	cout << "ClassAd converted back to XML:\n" << printed_classad << endl;

	classad = parser.ParseClassAd(printed_classad);

	printed_classad = "";
	printer.Unparse(printed_classad, classad);
	cout << "ClassAd re-read from generated XML:\n" << printed_classad << endl;

	// We should be able to read two classads from this string
	string xml2 = "<?xml version=\"1.0\"?>"
                  "<classads>"
		          "< c >"
                  "<a n=\"A\"><s> Alain &quot;Aslag&quot; Roy</s></a>"
                  "</c><c>"
                  "<a n=\"B\"><n> 3 </n></a>"
		          "</c></classads>";
	int offset = 0;
	ClassAd *classad2;

	classad = parser.ParseClassAd(xml2, offset);
	classad2 = parser.ParseClassAd(xml2, offset);


	printed_classad = "";
	printer.Unparse(printed_classad, classad);
	cout << "First ClassAd as parsed from XML:\n" 
		 << printed_classad << endl;

	printed_classad = "";
	printer.Unparse(printed_classad, classad2);
	cout << "Second ClassAd as parsed from XML:\n" 
		 << printed_classad << endl;

}
