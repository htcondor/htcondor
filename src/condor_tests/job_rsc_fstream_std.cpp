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


 

#include <iostream>
#include <fstream>
using namespace std;

int main() {
	fstream fin("x_data.in", ios::in);
    if (!fin) {
      cerr << "Open \"x_data.in\" for reading failed." << endl;
      return 1;
    }

	fstream fout("job_rsc_fstream_std.output", ios::out);
    if (!fout) {
      cerr << "Open for writing failed." << endl;
      return 1;
    }

	char c;
	while (fin.get(c))
		fout << c;
	cout << "To determine success of test, do a diff on x_data.in and" << endl;
	cout << "job_rsc_fstream_std.output.  There should be no difference." << endl;
	return 0;
}
