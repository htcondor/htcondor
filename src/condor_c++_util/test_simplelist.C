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
#include "simplelist.h"

void iterate (const SimpleList<int> & list) {
  SimpleListIterator<int> iter;
  iter.Initialize(list);

  while (1) {
    cout << endl << "List: " << list << endl;
    
    cout << "Choice: (C)urrent, (P)rev, (N)ext, (B)egin, (E)nd "
         << "(Q)uit --> ";

    char choice;
    int i;

    cin >> choice;
    switch (choice) {
     case 'C':
     case 'c':
      if (iter.Current(i)) cout << "Current is: " << i << endl;
      else cout << "No current" << endl;
      break;
     case 'P':
     case 'p':
      if (iter.Prev(i)) cout << "Current is: " << i << endl;
      else cout << "No current" << endl;
      break;
     case 'N':
     case 'n':
      if (iter.Next(i)) cout << "Current is: " << i << endl;
      else cout << "No current" << endl;
      break;
     case 'B':
     case 'b':
      iter.ToBeforeFirst();
      break;
     case 'E':
     case 'e':
      iter.ToAfterLast();
      break;
     case 'Q':
     case 'q':
      return;
    }
  }
}

int main (int argc, char *argv[]) {
  SimpleList<int> list;

  while (1) {
    cout << endl << "List: " << list << endl;
    
    cout << "Choice: (A)ppend, (R)ewind, (C)urrent, (N)ext, (D)eleteCurrent "
         << endl
         << "        (I)terator (Q)uit: ";
    
    char choice;
    int i;
    
    cin >> choice;
    
    switch (choice) {
     case 'A':
     case 'a':
      cin >> i;
      list.Append(i);
      break;
     case 'R':
     case 'r':
      list.Rewind();
      break;
     case 'C':
     case 'c':
      if (list.Current(i)) {
        cout << "Current item is: " << i << endl << endl;
      } else cout << "No current item" << endl << endl;
      break;
     case 'N':
     case 'n':
      if (list.Next(i)) {
        cout << "Next item is: " << i << endl << endl;
      } else cout << "No next item" << endl << endl;
      break;
     case 'D':
     case 'd':
      list.DeleteCurrent();
      cout << "Current item deleted" << endl;
      break;
     case 'I':
     case 'i':
      iterate(list);
      break;
     case 'Q':
     case 'q':
      return 0;
    }
  }
  return 0;
}
  
