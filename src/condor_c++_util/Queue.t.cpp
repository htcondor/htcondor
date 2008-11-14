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
  
