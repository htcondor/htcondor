/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

/*
 * counted_ptr - simple reference counted pointer.
 *
 * The is a non-intrusive implementation that allocates an additional
 * int and pointer for every counted object.
 * Should be safe to use in STL containers, or in most Condor templates
 * like HashTable.
 *
 * *** PITHY USAGE: ***
 *
 * In general, the idea is to allocate an object with new.  Then instantiate
 * a counted_ptr, passing the regular pointer you got from new into the
 * constructor.  Then, don't ever touch the regular pointer again -- the
 * counted_ptr class will invoke delete on the regular pointer at the right time.
 * Kinda like garbage collection, you can make copies and pass
 * around the counted_ptr to your heart's content.  Every time you copy
 * the counted_ptr, a reference count is incremented.  Every time you delete
 * a counted_ptr, the reference count is decremented.  When the reference count
 * reaches zero, delete is invoked on the regular pointer passed into the 
 * constructor.  Example:
 *      ClassAd *regular_ad_ptr = new ClassAd;
 *      ASSERT(regular_ad_ptr);	// make certain not NULL
 *      counted_ptr<ClassAd> smart_ad_ptr(regular_ad_ptr);
 *      // Now, never refer to regular_ad_ptr again!!  Do not delete it or
 *      // assign it- just forget about it.  Always use smart_ad_ptr from now on.
 *
 * Another example:
 *      counted_ptr<MyString> foo(new MyString("MMmmmm... cool pointers!"));
 * In this example, an advantage is there is no handle to a "regular pointer" 
 * to accidently delete or copy, but a disadvantage is there is no way to 
 * detect if new returned NULL (yeah, not likely on systems of today, but
 * us old-skool over-30 folks still worry about these things).
 *
 * For more examples of how to use it, refer to auto_ptr<> info in any C++
 * book -- but of course, counted_ptr<> is more useful than auto_ptr<> imho. 
 * But the syntax/usage is basically the same.
 *
 * *** CAUTION NOTES: ***
 *
 * a) Initialize a counter_ptr ONLY with a pointer to HEAP memory, not
 *    to stack memory!  Why?  Because when the reference count hits zero,
 *    this template will invoke the delete operator, and calling delete
 *    on a pointer to the stack is a bad-thing(tm).  So do NOT do this:
 *         ClassAd ad;
 *         counted_ptr<ClassAd> my_ad(&ad);  // NO!! BAD, BAD, BAD!!
 *
 * b) Use counted_ptr only for memory allocated by new, NOT for memory
 *    allocated by new[] or malloc().  Why?  Same as note (a) above -- 
 *    eventually this class will call delete, NOT free() or delete[].
 *    So do NOT do this:
 *         counted_ptr<char> foo(new char[200]);  // NO!! BAD, BAD, BAD!!
 *
 * c) Note that the constructor is declared explicit.  For good reason (and
 *    same as auto_ptr).  This means there is no implicit type cast from a 
 *    pointer to a counter_ptr.  For example:
 *       counted_ptr<MyString> p_ms;
 *       MyString *p_reg = new MyString;
 *       p_ms = p_reg;	// NOT ALLOWED (implicit conversion)
 *       p_ms = (counted_ptr<MyString>)(p_reg); // ALLOWED (explicit conversion)
 *       counted_ptr<MyString> pauto = p_ms; // NOT ALLOWED (implicit conversion)
 *       counted_ptr<MyString> pauto(p_ms);	// ALLOWED (explicit conversion)
 * 
 * Find problems?  Talk to Todd.  Or just fix em.  :)
 * - Todd Tannenbaum <tannenba@cs.wisc.edu>, Nov 2005
 */

#ifndef COUNTED_PTR_H
#define COUNTED_PTR_H

template <class X> class counted_ptr
{
public:
    typedef X element_type;

    explicit counted_ptr(X* p = 0) // allocate a new counter
        : itsCounter(0) {if (p) itsCounter = new counter(p);}
    ~counted_ptr()
        {release();}
    counted_ptr(const counted_ptr& r) throw()
        {acquire(r.itsCounter);}
    counted_ptr& operator=(const counted_ptr& r)
    {
        if (this != &r) {
            release();
            acquire(r.itsCounter);
        }
        return *this;
    }


    X& operator*()  const throw()   {return *itsCounter->ptr;}
    X* operator->() const throw()   {return itsCounter->ptr;}
    X* get()        const throw()   {return itsCounter ? itsCounter->ptr : 0;}
    bool unique()   const throw()
        {return (itsCounter ? itsCounter->count == 1 : true);}

	int operator== (const counted_ptr& r)
	{
		if ( itsCounter == itsCounter ) {
			return 1;
		}
		if ( itsCounter && itsCounter->ptr &&
			 r.itsCounter && r.itsCounter->ptr ) 
		{
			if ( *(itsCounter->ptr) == *(r.itsCounter->ptr) ) {
				return 1;
			}
		}
		return 0;
	}

	int operator< (const counted_ptr& r)
	{
		if ( itsCounter && itsCounter->ptr &&
			 r.itsCounter && r.itsCounter->ptr ) 
		{
			if ( *(itsCounter->ptr) < *(r.itsCounter->ptr) ) {
				return 1;
			}
		}
		return 0;
	}

private:

    struct counter {
        counter(X* p = 0, unsigned c = 1) : ptr(p), count(c) {}
        X*          ptr;
        unsigned    count;
    }* itsCounter;

    void acquire(counter* c) throw()
    { // increment the count
        itsCounter = c;
        if (c) ++c->count;
    }

    void release()
    { // decrement the count, delete if it is 0
        if (itsCounter) {
            if (--itsCounter->count == 0) {
                delete itsCounter->ptr;
                delete itsCounter;
            }
            itsCounter = 0;
        }
    }
};

#endif // COUNTED_PTR_H
