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
#include <iostream.h>
#include <stdio.h>
#include "Queue.h"

int main (int argc, char *argv[]) {
  Queue<int> queue;
  int * foo;
  int i = 0;

	foo = new int(0);
	queue.enqueue ( *foo );
	foo = new int(0);
	queue.enqueue ( *foo );
	foo = new int(0);
	queue.enqueue ( *foo );
	queue.dequeue ( *foo );
// Enqueue 1000 things...
	for (i = 0; i < 1000; i++) {
		foo = new int(i);
		if ( queue.enqueue(*foo) ) {
			cout << "FAILED at " << *foo << "\n";
		}
	}

	queue.dequeue ( *foo );
	queue.dequeue ( *foo );
// Now, dequeue them!
	for (i = 0; i < 1000; i++) {
		if ( queue.dequeue( *foo ) ) {
			cout << "FAILED to dequeue at " << i << "\n";
		}
		if ( * foo != i) {
			cout << "RETURNED WRONG THING!  got "<<*foo<<" instead of "<<i<<"\n";
		} else {
			cout << "RETURNED right thing.  got "<<*foo<<"\n";
		}
    }
}
  
