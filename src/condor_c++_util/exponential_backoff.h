#ifndef EXPONENTIAL_BACKOFF_H
#define EXPONENTIAL_BACKOFF_H

#include "condor_common.h"

/*
  An implementation of exponential backoff.
  - Joe Meehean 12/19/05
*/

class ExponentialBackoff{

		// Variables
 private:
	static int NEXT_SEED;
	
	int min;
	int max;
	double base;
	int seed;
	unsigned int tries;
	int prevBackoff;

	
		// Functions
 public:
	ExponentialBackoff(int min, int max, double base);	
	ExponentialBackoff(int min, int max, double base, int seed);
	ExponentialBackoff(const ExponentialBackoff& orig);
	ExponentialBackoff& operator =(const ExponentialBackoff& rhs);
	virtual ~ExponentialBackoff();

		/*
		  Resets the backoff to assume 0 tries.
		*/
	void freshStart();
		/*
		  Returns the next deterministic backoff (base^tries).
		 */
	int nextBackoff();
		/*
		  Returns a random back off between min and base^tries.
		 */
	int nextRandomBackoff();
	
		/*
		  Returns the last backoff returned by this class.
		*/
	int previousBackoff();

		/*
		  Returns the number of tries since the last freshStart()
		 */
	int numberOfTries();

 protected:
	void init(int min, int max, double base, int seed);
	void deepCopy(const ExponentialBackoff& orig);
	void noLeak();

 private:
	ExponentialBackoff();

};

#endif
