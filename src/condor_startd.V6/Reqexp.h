/* 
    This file defines the Reqexp class.  A Reqexp object contains
    methods and data to manipulate the requirements expression of a
    given resource.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _REQEXP_H
#define _REQEXP_H

class Reqexp
{
public:
	Reqexp( ClassAd* ca );
	~Reqexp();
	void	restore();		// Restore the original requirements
	void	unavail();		// Set requirements to False
	void	avail();		// Set requirements to True
	int		eval();			// Evaluate the original requirements  
							// (-1 = undef, 1 = true, 0 = false)
	void	pub();			// Evaluates orig requirements and sets
							// the classad appropriately
private:
	ClassAd* ca;
	char* origreqexp;
	int modified;
};

#endif _REQEXP_H
