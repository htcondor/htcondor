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
