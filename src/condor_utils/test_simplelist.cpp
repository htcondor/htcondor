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
#include "simplelist.h"

void iterate (const SimpleList<int> & list) {
  SimpleListIterator<int> iter;
  iter.Initialize(list);

  while (1) {
    std::cout << endl << "List: " << list << endl;
    
    std::cout << "Choice: (C)urrent, (P)rev, (N)ext, (B)egin, (E)nd "
         << "(Q)uit --> ";

    char choice;
    int i;

    std::cin >> choice;
    switch (choice) {
     case 'C':
     case 'c':
      if (iter.Current(i)) std::cout << "Current is: " << i << std::endl;
      else std::cout << "No current" << std::endl;
      break;
     case 'P':
     case 'p':
      if (iter.Prev(i)) std::cout << "Current is: " << i << std::endl;
      else std::cout << "No current" << std::endl;
      break;
     case 'N':
     case 'n':
      if (iter.Next(i)) std::cout << "Current is: " << i << std::endl;
      else std::cout << "No current" << std::endl;
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
    std::cout << endl << "List: " << list << endl;
    
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
  
