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
#include "local_client.h"

#if defined(WIN32)
#define PIPE_ADDR "\\\\.\\pipe\\local_server_test"
#else
#define PIPE_ADDR "/tmp/local_server_test"
#endif

#include <iostream>
using namespace std;

int
main()
{
	LocalClient client(PIPE_ADDR);

	while (true) {
		char c1, c2;
		cin >> c1;
		if (!cin) {
			if (!cin.eof()) {
				cout << "error in input stream" << endl;
			}
			break;
		}
		client.start_connection(&c1, sizeof(char));
		client.read_data(&c2, sizeof(char));
		client.end_connection();
		cout << "received " << c2 << endl;
		if (c2 == 'q') {
			break;
		}
	}

	return 0;
}