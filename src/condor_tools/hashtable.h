
#include <string.h> 
#include "queue.h"   


//this is the new template hash class that i will use.
//it should simplify a lot of code.

const int       SIZE = 991;           //the size of the H_table
const int       NULL_TERMINATOR = 0;  //decimal value of null.


template <class R>
class hash_node {

 public:
    char *key;
    R    element;

    hash_node(char* keyin, R elementin)
      {
      key = keyin;
      element = elementin;
      }

};


template <class T>
class h_table {

 private:
    queue<void*>  table[SIZE];
    int           _currentlist; 
    T             *empty;  //empty to prevent compilation problems.

    /*
    h_table::h_function(char* in_string)

    char* key: the string for which we want to
    return a numerical value for. never tested
    the function. 
    */                                     
    int h_table::h_function(char* key, T* emptyval)
    {
    char* tc     = key;
    int   total  = 1;
    char  letter = *tc;

    while ( *tc != NULL_TERMINATOR )       
      {                                   
      total = (total * (*tc)) % SIZE;
      tc++;
      }

    return total;
    }


 public:
    h_table(){_currentlist = -1;};
    ~h_table(){};
 
    void  insert(char *key, T insertval)
      {
      int           position = h_function(key, NULL);
      hash_node<T>  *inelem = new hash_node<T>(key, insertval);
  
          table[position].insert((void*)inelem);   

      }

    //looks through the list to find the key, if no element then return NULL
    //else it returns the T element.
    T exists(char *key, int &found)
      {
      int           position = h_function(key, NULL);
      hash_node<T>  *holderelement;
      int           success;

          //now we search that list to find a similar element.
          for (int i = 1; ; i++)
            {
            //get the ith element out of the list. 
            holderelement = (hash_node<T>*)table[position].returni(i, success);
            if (!success)
              {
              found = 0;
              return 0;  //invalid user must check found
              }
            else if (!strcmp(key, holderelement->key))
              {
              found = 1;
              return holderelement->element;
              } 
            }
      } 

    void firstnonemptylist(int startval)
      {
      for (int i = startval; i < SIZE; i++ )
        {
        if (!table[i].empty())
            { 
            _currentlist = i;
            return;
            }
        }
      _currentlist = -1;
      }

    int returnfirst(T &retobj)
      {
      void *tmp;
      firstnonemptylist(0);

      if (_currentlist == -1)
          return 0;
      table[_currentlist].returnfirst(tmp);
      retobj = ((hash_node<T>*)tmp)->element;
      return 1;
      }

    int returnnext(T &retobj)
      {
      void *tmp;

      if ( _currentlist < 0 )
          return 0;

      if (!table[_currentlist].returnnext(tmp))
          {
          firstnonemptylist(_currentlist + 1);  //start traversal from that point.
          if (_currentlist == -1)
              return 0;
          table[_currentlist].returnfirst(tmp);
          retobj = ((hash_node<T>*)tmp)->element;
          return 1;
          }
     
      retobj = ((hash_node<T>*)tmp)->element;
      return 1;
      }

};



