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
