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
