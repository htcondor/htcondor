#include <stddef.h>
#include <iostream.h>




/*
goal

we want each node to be a template class,
a node of whatever, we want it to be a class
of whatever, if you want to do it efficiently
and have a queue of only the pointers then with
the template class you want to declare an instance
as a pointer to the object...
*/



/*
the node class

a template class that will be used in a double 
linked list, stack, queue, binary tree...
whatever a user wants.
*/



template <class T>
class node {

 private:  

    node* _prev;    //the previous pointer.
    node* _next;    //the next pointer.
    T     _ob;      //the object 


 public:

    /* 
    destructor is empty

    if we have a node that has a pointer we do not want
    to free the space the pointer points at, that could
    free a valid space.  also, if we store the entire
    object then it will be freed on its own.
    */
    ~node(){};                


    /* 
    simple constructor
 
    this constructor will simple set the data value
    to the incoming object and set the pointers to 
    null.
    */
    node(T obj)
    {

        _prev = NULL;
        _next = NULL;

        _ob = obj;

    }


    /*
    constructor

    this constructor will take two pointers and it
    will set the next and prev to whatever values
    those pointers are set to.
    */
    node(T obj, node* prev, node* next)
    {

        _prev = prev;
        _next = next;

        _ob = obj;

    }


    /*
    data

    the function will return the data itself.
    */
    T data()
    {


        return _ob;

    }


    /*
    datap 

    this returns a pointer to whatever kind of data object
    we are storing in this class.
    */
    T* datap()
    {

        return &_ob;

    }


    /*
    last

    this function retuns a 1 if the _next pointer is null and
    a 0 if it is empty.  for use in a list type implementation.
    */
    int last()
    {

        if (_next == NULL)
             return 1;
        else 
             return 0;

    }


    //returns what '_next' points to.
    node* return_next()
    {

        return _next;

    }


    //returns what '_prev' points to
    node* return_prev()
    {

        return _prev;

    }


    //to change the '_next' pointer
    void  set_next(node* p)
    {

        _next = p;

    }
  

    //to change the '_prev' pointer
    void  set_prev(node* p)
    {

        _prev = p;

    }



};

