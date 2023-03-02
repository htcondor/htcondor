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


#include "classad/classad_distribution.h"
#include <iostream>

using std::string;
using std::vector;
using std::pair;

using namespace classad;

void test_old_time(void);

int main(int, char **)
{
	ClassAdXMLParser  parser;
	ClassAd  *classad;

	string  xml = "<?xml version=\"1.0\"?>< c >"
                  "<a n=\"A\"><s> Alain &lt;Aslag&gt; Roy&#172;</s></a>"
                  "<a n=\"B\"><i> 3 </i></a>"
		          "<a n=\"C\"><b v=\"t\"/></a>"
		          "<a n=\"D\"><l><i>10</i><un/><er/><i>14</i></l></a>"
		          "<a n=\"E\"><c><a n=\"AA\"><i>4</i><a n=\"BB\"><s>x</s></a></c></a>"
		          "<a n=\"F\"><e>(x >= 10)</e></a>"
		          "<a n=\"G\"><s></s></a></c>";

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
                  "<a n=\"B\"><i> 3 </i></a>"
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

	FILE *temp_xml;
	temp_xml = fopen("temp_xml", "w");
	fprintf(temp_xml, "%s\n", xml.c_str());
	fclose(temp_xml);

	temp_xml = fopen("temp_xml", "r");
	ClassAd *file_classad;
	file_classad = parser.ParseClassAd(temp_xml);
	fclose(temp_xml);
	unlink("temp_xml");
	
	printed_classad = "";
	printer.Unparse(printed_classad, file_classad);
	cout << "File ClassAd:\n" 
		 << printed_classad << endl;


	test_old_time();

	return 0;

}

void test_old_time(void)
{
	ClassAd *ad;
	Literal  *time;

	ad = new ClassAd();
	time = Literal::MakeAbsTime();
	ad->Insert("CurrentTime", time);

	string       printed_classad;
	ClassAdXMLUnParser  unparser;
	unparser.SetCompactSpacing(false);
	unparser.Unparse(printed_classad, ad);
	cout << printed_classad << endl;

	ClassAdXMLParser parser;
	ClassAd *new_ad;
	new_ad = parser.ParseClassAd(printed_classad);
	printed_classad = "";
	unparser.Unparse(printed_classad, new_ad);
	cout << printed_classad << endl;
	return;
}
