#ifndef _CONDOR_FUNCTOR_H
#define _CONDOR_FUNCTOR_H

// std::function has some really silly behavior that makes them harder to
// use than should be necessary when passing them heap-allocated functors.
// (You need to wrap each functor in std::ref() before handling them to
// std::function to prevent it from calling the destructor on the heap
// object when the std::function object is destroyed, which may have no
// relation to when the functor's owner wants to destroy it.)

class Functor {
	public:
		virtual int operator() () = 0;
		virtual int rollback() = 0;

		virtual ~Functor() { }
};

#endif /* _CONDOR_FUNCTOR_H */
