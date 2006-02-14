#include "condor_common.h"
#include "exponential_backoff.h"
#include "condor_random_num.h"


//////////////////////////////////////////////////////////////////////
// Static variables
//////////////////////////////////////////////////////////////////////
int ExponentialBackoff::NEXT_SEED = 1;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
ExponentialBackoff::ExponentialBackoff(int min, int max, double base){
	int theSeed = NEXT_SEED;
	NEXT_SEED++;
	init(min, max, base, theSeed);
}

ExponentialBackoff::ExponentialBackoff(int min, int max, double base, int seed){
	init(min,max,base,seed);
}

ExponentialBackoff::ExponentialBackoff(const ExponentialBackoff& orig){
	deepCopy(orig);
}

ExponentialBackoff&
ExponentialBackoff::operator =(const ExponentialBackoff& rhs){
	if( this != &rhs ){
		noLeak();
		deepCopy(rhs);
	}
	
	return( *this );
}

ExponentialBackoff::~ExponentialBackoff(){
	noLeak();
}

void
ExponentialBackoff::init(int min, int max, double base, int seed){
	this->min = min;
	this->max = max;
	this->base = base;
	this->seed = seed;

		// init the internals
	this->tries = 0;
	this->prevBackoff = min;

		// seed the PRNG
	set_seed(seed);
}

void
ExponentialBackoff::deepCopy(const ExponentialBackoff& orig){
	this->min = orig.min;
	this->max = orig.max;
	this->base = orig.base;
	this->seed = orig.seed;
	this->tries = orig.tries;
	this->prevBackoff = orig.prevBackoff;
}

void
ExponentialBackoff::noLeak(){
		// no dynamic memory yet
}

///////////////////////////////////////////////////////////////////////
// Methods/Functions
///////////////////////////////////////////////////////////////////////
void
ExponentialBackoff::freshStart(){
	tries = 0;
}

int
ExponentialBackoff::nextBackoff(){

		// easy one
	if( tries < 1 ){
		return min;
	}

		// compute the backoff multiplier
	unsigned int backoff_mult = 2 << (tries - 1);
	

	int result = (int)(base * backoff_mult);
	
		// ensure the minimum
	result += min;


		// prevent overflow and underflow
	if( result > max || result < 0 ){
		result = max;
	}

		// increment tries
	tries++;

		// store the result
	prevBackoff = result;
	
	return( result );
}

int
ExponentialBackoff::nextRandomBackoff(){
	
	// easy one
	if( tries < 1 ){
		return min;
	}

		// compute the maximum multiplier
	unsigned int max_mult = 2 << (tries - 1);
	
		// get a random number between 0 and max mult
	unsigned int backoff_mult = get_random_int() % max_mult;
	
	int result = (int)(base * backoff_mult);
	
		// ensure the minimum
	result += min;


		// prevent overflow and underflow
	if( result > max || result < 0 ){
		result = max;
	}

		// increment tries
	tries++;

		// store the result
	prevBackoff = result;
	
	return( result );
}

int
ExponentialBackoff::numberOfTries(){
	return(tries);
}

