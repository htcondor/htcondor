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
  
