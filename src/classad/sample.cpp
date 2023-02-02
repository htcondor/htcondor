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

string job_classad_text = 
"["
"Requirements = (other.Type == \"Machine\" && other.memory >= 4000);"
"Type = \"Job\";"
"Memoryused = 6000;"
"]";

string machine_classad_text = 
"["
"Type = \"machine\";"
"Requirements = (other.Type == \"job\" && other.memoryused < 8000);"
"Memory = 5000;"
"]";

int main(int, char **)
{
	// First we turn the textual description of the ClassAd into 
	// our internal representation of the ClassAd. 
	ClassAdParser  parser;
	ClassAd        *job_classad, *machine_classad;

	job_classad     = parser.ParseClassAd(job_classad_text, true);
	machine_classad = parser.ParseClassAd(machine_classad_text, true);

	// Next we print out the ClassAds. This requires unparsing them first.
	PrettyPrint  printer;
	string       printed_job_classad, printed_machine_classad;

	printer.Unparse(printed_job_classad, job_classad);
	printer.Unparse(printed_machine_classad, machine_classad);
	cout << "Job ClassAd:\n" << printed_job_classad << endl;
	cout << "Machine ClassAd:\n" << printed_machine_classad << endl;

	// Finally we test that the two ClassAds match. They should. 
	// Note that a MatchClassAd is a ClassAd. You could pretty print it
	// like the other ads above, if you wanted to.
	MatchClassAd  match_ad(job_classad, machine_classad);
	bool          can_evaluate, ads_match;

	can_evaluate = match_ad.EvaluateAttrBool("symmetricMatch", ads_match);
	if (!can_evaluate) {
		cout << "Something is seriously wrong.\n";
	} else if (ads_match) {
		cout << "The ads match, as expected.\n";
	} else {
		cout << "The ads don't match! Something is wrong.\n";
	}
	return 0;
}
