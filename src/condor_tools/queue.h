


#include <stddef.h>
#include <iostream.h>
#include <assert.h>
#include "node.h"



/*

the queue and node class documentation.


this node and queue class code was taken from code that 
i used in 536.  i changed it a little so that now it 
is a template class.  the functions haven't change, only 
the syntax for the templates.  i turned the lower class
node.h into a template class also.  now we can have a 
queue of any type that we want. 

since i had tested this for 536 i know it is correct and
do not include the tests for the sake of brevity, 
but just wanted to note that it has been tested.

also, since the queue was not a part of the assignment
according to prof livny in lecture i have barely 
commented this code, most of it is pretty obvious
for someone above a 367 level.


added some functions, returni and removei.  that way i can 
go down the list and get each element.  not implemented very
efficiently at this time.  the best is to create a scan
class like in 564 because we would expect that a person
would use these functions only when scanning a list.
*/


template <class T>
class queue {

 private:
    node<T> *_head;   //pointer to the front of the list
    node<T> *_tail;   //pointer to the end of the list
    node<T> *_traverse; //pointer used to traverse the list.

    int      _length;

 public:

    /*
    constructor
   
    simply set the head 
    and tail pointers to null
    */
    queue()
    { 
    _head = NULL; 
    _tail = NULL;
    _traverse = NULL;

    _length = 0;
    }; 


    /*
    destructor

    loop down the entire list untill all the elements are
    removed.
    */
    ~queue()
       {
       node<T> *first, *second;
       if (_head == NULL)
           return;
       else
         {
         first = _head;
         while (first != NULL)     //traverse the list and free all the nodes.
           {
           second = first->return_next();
           delete first;
           first = second;
           }
         } 
       };      


    /*
    insert

    for a queue we will insert onto the tail, and we will 
    remove it from the head.  

    returns a 1 if successful otherwise it will return a 
    0 if unsuccessful. 
    */
    int insert(T obj)
    {
    node<T>* temp;

        _length++;  //increase the length of the queue.

        if (_head == NULL)   //tail will also be NULL here.
           {
           //then we will set the head and tail to a node
           _head = new node<T>(obj);
           _tail = _head;
           }  
        else 
           {
           //put a node at the tail
           temp = new node<T>(obj, _tail, NULL);
           _tail->set_next(temp);
           _tail = temp;
           }   


    }  


 
    /*
    remove

    we will remove the element at the head of the list.

    we go with the Carolyn Rosner standard from 367, if
    the queue is empty and we call remove then we exit
    with an error!!  cannot call remove on an empty list
    queue, or stack...
    */
    T remove() 
      {
      node<T>* temp;
      T        holder;

        _length--;

        if (_head == NULL)   //tail will also be NULL then
          {  
          cerr<<"illegal list operation.  remove called on empty list.\n";
          exit(0);
          }
        else
          {
          //remove a node from the head, move the head back.
          if (_head == _tail)  //if the head == tail and the list isn't null
              {                //then there is one element left.

              assert(_length == 0);  //check state of queue for debugging.

              temp = _head;
              _head = NULL;
              _tail = NULL;
              holder = temp->data();
              delete temp;
              return holder;
              }   
          else  //otherwise there is more than one element left in the queue.
              {
              assert(_length >= 1);  //check state of queue for debugging.

              temp = _head;
              _head = temp->return_next();
              holder = temp->data();
              delete temp;
              _head->set_prev( NULL );   //delete the pointer so it knows its the front.
              return holder;
              }
          }

      }


    /*
    empty

    empty will return a 1, or a boolean of true, if the 
    queue is empty, otherwise it will return a false.
    */
    int empty()
    {
    
        if (_head == NULL)   //tail will also be NULL here.
          {
          return 1;
          }
        else
          {
          return 0;
          }  
   
    }

 

    /*
    size

    returns how many elements are currently in the queue.
    */
    int size()
       {

       return _length;

       };


    /* return the ith element */
    /* if there is the ith element it will set success to true and */
    /* return it without removing it.  otherwise it will set       */
    /* success to false since it did not return the ith element.   */
    /* this is according to carolyn rosner style, the user must    */
    /* first check the sucess before accessing T. */
    /* NOTE: WE COUNT FROM 1.  THE FIRST ELEMENT IN THE LIST IS 1. */
    T returni(int i, int &success)
       {
       node<T> *p;
       int     spot;

       assert( i > 0 );  //i must be greater than zero.

       if ( this->empty() || _length < i )
           {
           success = 0;   //here we fail.
           return (T)0;
           }
       else
         {
         spot = 1;
         p = _head;
         while ( spot != i )      //can never reach NULL   
           {
           assert( p != NULL );

           spot++;
           p = p->return_next();
           }
        
         success = 1;
         return p->data();  //the element where we are.
         } 
       }; 

    //return first and return next are for sequential scans on the list.
    int returnfirst(T &ret)
       {
       //if the list is empty then fail.
       //else set the global pointer, return the global.
       if ( this->empty() )
           return 0;
       
       _traverse = _head;
       ret = _traverse->data();
       return 1;
       };

    int returnnext(T &ret)
       {
       assert(_traverse);

       if ( _traverse->return_next() == NULL )
           {
           _traverse = NULL;
           return 0;
           }

       _traverse = _traverse->return_next();
       ret = _traverse->data();
       return 1;
       };

    //returns a 1 on success.
    int inserti(int i, T obj)
       {
       node<T> *p;
       node<T> *p2;
       int     spot;
   
       assert( i > 0 );  //inserting into the first position is into the front.
       assert( i < _length+2);  //we cannot insert past the end.

       if (_length == 0){
           _head = new node<T>(obj);
           _tail = _head;
           _length++;
           return 1; 
       }else if (i == _length+1){    //inserting onto the end of the list.
           p = new node<T>(obj);
           _tail->set_next(p);       //connect them up.
           p->set_prev(_tail);
           _tail = p;
           _length++;
           return 1;
       }else{                      //otherwise in the front of middle.
         spot = 1;
         p = _head;
         while ( spot != i )      
           {
           assert( p != NULL );
           spot++;
           p = p->return_next();
           }
         //p now points to the spot we want to insert into.
         p2 = new node<T>(obj,p->return_prev(),p);
         if (p->return_prev() != NULL){
           node<T> *p3 = p->return_prev();
           p3->set_next(p2);
         }
         p->set_prev(p2);
         if (_head == p)
            _head = p2;
       }

       _length++;
       return 1;

       }; 


/*  for debugging purposes. */

    void print()
       {
       node<T> *p;

       if ( this->empty() )
           cout<<"empty\n";
       else
         {
         p = _head;
         while ( p != NULL )      //can never reach NULL   
           {
           cout<<p->data()<<" ";
           p = p->return_next();
           }
        
         cout<<endl;
         } 
       }; 



    /* removes the ith element */
    /* sets success to 1 if it removed the correct element. */
    /* otherwise sets success to 0. */
    /* the user must test success before accessing T */
    /* NOTE: WE WILL COUNT FROM 1. */
    T removei(int i, int &success)
       {
       node<T> *p, *left, *right;
       int     spot;
       T       holder;

       assert( i > 0 );  //i must be greater than zero.
       assert( !this->empty() );  //can't call remove on an empty list!

       if ( _length < i )
           {
           //cerr<<"here we are buddy.\n";
           success = 0;   //here we fail. 
           return (T)0;
           }
       else
         {
         //cerr<<"we are in the else statement.\n";
         spot = 1;
         p = _head;
         while ( spot != i )      //can never reach NULL   
           {
           assert( p != NULL );

           spot++;
           p = p->return_next();
           }
        
         left  = p->return_prev();
         right = p->return_next();

         if ( right == NULL && left == NULL )  //it was the last element in the list.
             {
             _head = NULL;
             _tail = NULL;
             }
         else if ( right == NULL )
             {
             left->set_next( NULL );  //removing last element.
             _tail = left;
             }
         else if ( left == NULL )
             {
             right->set_prev( NULL );
             _head = right;
             }
         else 
             {
             left->set_next(right);       //connect, removing node.
             right->set_prev(left);
             }

         holder = p->data();      //get the data.

         delete p;   //free the node.

         _length--;
         success = 1;

         return holder;  //the element where we are.
         } 

       };  



};





