/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#include "classad_distribution.h"
#include <iostream>

using namespace std;
#ifdef WANT_NAMESPACES
using namespace classad;
#endif

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

int main(int argc, char **argv)
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
