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

int main(int argc, char **argv)
{
	ClassAdXMLParser  parser;
	ClassAd  *classad;
	int offset = 0;
	string  xml = "<?xml version=\"1.0\"?>< c >"
                  "<a n=\"A\"><s> Alain &quot;Aslag&quot; Roy</s></a>"
                  "<a n=\"B\"><n> 3 </n></a>"
		          "<a n=\"C\"><b v=\"t\"/></a>"
		          "<a n=\"D\"><l><n>10</n><un/><er/><n>14</n></l></a>"
		          "<a n=\"E\"><c><a n=\"AA\"><n>4</n></c></a>"
		          "<a n=\"F\"><e>(x >= 10)</e></a>";

	classad = parser.ParseClassAd(xml, offset);

	PrettyPrint  printer;
	string       printed_classad;

	printer.Unparse(printed_classad, classad);
	cout << printed_classad << endl;

}
