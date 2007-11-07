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
  
