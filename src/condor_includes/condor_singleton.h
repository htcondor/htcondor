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
