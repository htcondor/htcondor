/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "queue.h"

int main (int argc, char *argv[]) {
  Queue<int> queue;

  while (1) {

    {
      int i;
      cout << "Queue: ";
      queue.Rewind();
      while (queue.Next(i)) cout << i << ", ";
      cout << "<end>" << endl;
    }
    
    cout << "Choice: (E)nqueue, (D)equeue, (Q)uit: ";
    
    char choice;
    int i;
    
    cin >> choice;
    
    switch (choice) {
     case 'E':
     case 'e':
      cin >> i;
      cout << "EnQueue returned: ";
      if (queue.EnQueue(new int(i))) cout << "true";
      else                           cout << "false";
      cout << endl;
      break;
     case 'D':
     case 'd':
      cout << "DeQueue returned: ";
      if (queue.DeQueue(i)) {
        cout << i;
        delete &i;
      } else cout << "NULL";
      cout << endl;
      break;
     case 'Q':
     case 'q':
      return 0;
    }
  }
  return 0;
}
  
