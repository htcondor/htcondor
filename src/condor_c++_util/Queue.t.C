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
  
