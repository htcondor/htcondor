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
  
