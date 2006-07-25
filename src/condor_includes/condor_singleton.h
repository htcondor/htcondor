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
#ifndef CONDOR_SINGLETON_H
#define CONDOR_SINGLETON_H

// This declares your class to be a singleton.
// You are still REQUIRED to make your ctor private
// It is also a good idea to make your dtor private.
#define DECLARE_SINGLETON(class)			\
	static class * Instance();			  \
	friend void Destroy##class();		   \
	friend class * Instance##class();

// This is the implementation of your Instance() function
// Simply place "IMPLEMENT_SINGLETON(YourClass);" all by
// itself in your implementation file.
#define IMPLEMENT_SINGLETON(class)		  \
void Destroy##class()					   \
{										   \
	delete Instance##class();		   \
}										   \
class * Instance##class()				   \
{										   \
	static class * pInstance = NULL;		\
											\
	if (pInstance == NULL)				  \
	{									   \
		pInstance = new class;			  \
		atexit(&(Destroy##class));		  \
	}									   \
											\
	return pInstance;					   \
}										   \
class * class::Instance()				   \
{										   \
	return Instance##class();			   \
}

// Use this to declare private copy constructors
// and '=' operators.
#define I_CANT_BE_COPIED(class)             \
private:                                    \
	class& operator =(const class &);              \
	class(const class &)

#endif
