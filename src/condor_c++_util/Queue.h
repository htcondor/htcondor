/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef QUEUE_H
#define QUEUE_H

//--------------------------------------------------------------------
// Queue template
// Written by Adiel Yoaz (1998)
// Rewritten by Colby O'Donnell (1998)
//--------------------------------------------------------------------

#include "list.h"

/** Implements a FIFO queue, as a subclass of List.  Two basic operations,
    Enqueue and Dequeue are supported in various overloaded forms.  Like
    the parent List class, the Queue class takes reference objects, such
    as pointers and references.  Refer to the List class definition for
    further details.
 */
template <class ObjType>
class Queue : public List<ObjType> {

 public:

  /// Instanciates an empty queue
  Queue() : List<ObjType>() {}

  /** Deprecated.  Used by the previous fixed-size implementation.
   */
  Queue (int tableSize) : List<ObjType>() {}

  /** Add an object to the queue.  Puts the object at the end of the list.
      @param obj pointer to object to be added to end of queue
      @return true: success, false: failure (out of memory)
  */
  inline bool EnQueue ( ObjType * obj ) { return Append (obj); }

  /** Add an object to the queue.  Puts the object at the end of the list.
      @param obj reference to object to be added (as a pointer) to end of queue
      @return true: success, false: failure (out of memory)
  */
  inline bool EnQueue ( ObjType & obj ) { return Append (obj); }

  /** Remove an object from the queue.  Pulls the object from the front of the
      list.
      @return Pointer to object, or NULL if queue is empty
  */
  ObjType * DeQueue ();

  /** Remove an object from the queue.  Pulls the object from the front of the
      list.
      @param obj Reference to 
      @return Pointer to object, or NULL if queue is empty
  */
  bool DeQueue (ObjType & obj);

  /** Remove an object from the queue.  Pulls the object from the front of the
      list.
      @return Pointer to object, or NULL if queue is empty
  */
  inline bool DeQueue (ObjType * & obj) { return NULL != (obj =  DeQueue()); }

  /** Deprecated.
      @return 0 for success, -1 for failure
  */
  inline int enqueue(ObjType & obj) { return EnQueue(obj) ? 0 : -1; }

  /** Deprecated.
      @return 0 for success, -1 for failure
  */
  inline int dequeue(ObjType & obj) { return DeQueue(obj) ? 0 : -1; }

  /** Deprecated.
      @return always false.  The queue is dynamic, and never is full
  */
  inline bool IsFull() { return false; }

  /** Deprecated.
      @return Number of objects in the queue.
  */
  inline int Length() const { return Number(); }

};

/*-------------------------------------------------------------------------
  Implementation of the List class begins here.  This is so that every
  file which uses the class has enough information to instantiate the
  the specific instances it needs.  (This is the g++ way of instantiating
  template classes.)
*/

template <class ObjType>
ObjType *
Queue<ObjType>::DeQueue() {
  Rewind();
  ObjType *obj = Next();
  if (obj != NULL) DeleteCurrent();
  return obj;
}

template <class ObjType>
bool
Queue<ObjType>::DeQueue (ObjType & obj) {
  ObjType *front = DeQueue();
  if (front != NULL) {
    obj = *front;
    return true;
  } else return false;
}

#endif
