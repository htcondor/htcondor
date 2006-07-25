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
