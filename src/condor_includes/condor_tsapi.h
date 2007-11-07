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

#ifndef TSAPI_H
#define TSAPI_H

/* The purpose of this class is very special, even though the code is
	very small.

	What this does is allow me to create an on stack reference to Singleton
	class defined with the IMPLEMENT_SINGLETON() macro and sibings.

	Say I had a class 'Registration' that was a singleton, I could just do:
	typedef TransparentSingletonAPI<Registration> s_Registration;

	Now I can just invoke a s_Registration on the stack frame and
	use it with -> to get to the singleton class. No more typing garbage to
	get to my singleton classes.

	Like this:
	void foo(void)
	{
		s_Registration reg;
		RegID rid;

		rid = reg->acquire_ID();
	}
*/

template<class T>
class TransparentSingletonAPI
{
	public:
		TransparentSingletonAPI() { };
		~TransparentSingletonAPI() { };

		T * operator->()
		{
			return T::Instance();
		}
};

#endif
