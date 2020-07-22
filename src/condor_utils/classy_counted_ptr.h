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


#ifndef CONDOR_REF_COUNTED_H
#define CONDOR_REF_COUNTED_H


/* Notes on ClassyCountedPtr and classy_counted_ptr<>.

   ClassyCountedPtr is a base class for classes that support explicit
   reference counting (where a zero reference count causes deletion).
   classy_counted_ptr<> is a template "smart pointer", nearly
   identical to counted_ptr<>, that automatically calls the
   ClassyCountedPtr methods to increment/decrement the reference
   count.

   Whereas the counted_ptr template provides implicit reference
   counting without modifying your class, classy_counted_ptr requires
   that your class be derived from ClassyCountedPtr.  There are cases
   where it is better to modify your class to explicitly support
   reference counting (see below), and others where it is more
   convenient not to modify the class.

   An example of when to use ClassyCountedPtr is when you find that
   you need to create a ref-counted reference to "this".  The awkward
   thing is that "this" is passed implicitly to member functions as
   a raw pointer rather than as a counted_ptr.  Creating a brand new
   counted_ptr from scratch whenever you need one is not acceptable,
   because the internal counter object must be shared between all
   instances of counted_ptr that are referencing the same object.  In
   the case of ClassyCountedPtr, this is not a problem, because the
   reference count is a member of the main class itself, so you don't
   have to worry about different versions of the reference count
   floating around.

   Example usage of ClassyCountedPtr:

   class MyClass: public ClassyCountedPtr {
   }

   // Given the above definition of MyClass, we can now
   // explicitly trigger reference-counting by calling
   // inc/decRefCount().

   MyClass *x = new MyClass();
   x->incRefCount();

   // ...
   // NOTE: until decRefCount() is called, x is guaranteed not to be deleted.

   x->decRefCount();

   // No further references to the value of x should be made, because
   // we have triggered a decrement of our reference to the object.
   // If the reference count dropped to zero, x has now been deleted.


   Example using classy_counted_ptr with MyClass as defined above:

   MyClass *x = new MyClass();
   classy_counted_ptr<MyClass> p(x);

   // Now, instead of explicitly calling x->inc/decRefCount(), we just
   // let classy_counted_ptr do that for us.  At this point in the
   // code, p has already incremented the reference count.  When p
   // goes out of scope or gets pointed at a different object, it will
   // decrement the reference count.  You can use p as if it was a
   // pointer to the object, like x.  Example:

   p->myFunction(blah,blah);

*/

#include "condor_debug.h"

class ClassyCountedPtr {
public:
	ClassyCountedPtr()
		: m_ref_count(0) {}
	virtual ~ClassyCountedPtr()
		{ASSERT( m_ref_count == 0 );}

	void incRefCount() {m_ref_count++;}
	void decRefCount() {
		ASSERT( m_ref_count > 0 );
		if( --m_ref_count == 0 ) {
			delete this;
		}
	}

private:
	int m_ref_count;
};

template <class X> class classy_counted_ptr
{
public:
    typedef X element_type;

    classy_counted_ptr(X* p = 0) // allocate a new counter
        : itsPtr(p) {if(p) p->incRefCount();}
    ~classy_counted_ptr()
        {if(itsPtr) itsPtr->decRefCount();}
    classy_counted_ptr(const classy_counted_ptr& r) noexcept
    {
		itsPtr = r.itsPtr;
		if(itsPtr) itsPtr->incRefCount();
	}
    classy_counted_ptr& operator=(const classy_counted_ptr& r)
    {
        if (this != &r) {
			if(itsPtr) itsPtr->decRefCount();
			itsPtr = r.itsPtr;
			if(itsPtr) itsPtr->incRefCount();
        }
        return *this;
    }


    X& operator*()  const noexcept   {return *itsPtr;}
    X* operator->() const noexcept   {return itsPtr;}
    X* get()        const noexcept   {return itsPtr;}
    bool unique()   const noexcept
        {return (itsPtr ? itsPtr->refCount() == 1 : true);}

		// Unfortunately, the following auto-conversion to a
		// base-class does not work on the windows compiler, so until
		// windows supports it, this is disabled.
#if 0
		// The following template allows auto-conversion of a
		// classy_counted_ptr<DerivedClass> to classy_counted_ptr<BaseClass>.
	template<class baseType>
	operator classy_counted_ptr<baseType>()
	{
		return classy_counted_ptr<baseType>(itsPtr);
	}
#endif

	int operator== (const classy_counted_ptr& r)
	{
		if ( itsPtr == r.itsPtr ) {
			return 1;
		}
		if ( itsPtr && r.itsPtr )
		{
			if ( *(itsPtr) == *(r.itsPtr) ) {
				return 1;
			}
		}
		return 0;
	}

	int operator< (const classy_counted_ptr& r)
	{
		if ( itsPtr && r.itsPtr )
		{
			if ( *(itsPtr) < *(r.itsPtr) ) {
				return 1;
			}
		}
		return 0;
	}

private:

	X *itsPtr;
};


#endif
