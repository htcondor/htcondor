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
ExponentialBackoff::ExponentialBackoff(int minimum, int maximum, double b){
	int theSeed = NEXT_SEED;
	NEXT_SEED++;
	init(minimum, maximum, b, theSeed);
}

ExponentialBackoff::ExponentialBackoff(int minimum, int maximum, double b, int s){
	init(minimum,maximum,b,s);
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
ExponentialBackoff::init(int minimum, int maximum, double b, int s){
	this->min = minimum;
	this->max = maximum;
	this->base = b;
	this->seed = s;

		// init the internals
	this->tries = 0;
	this->prevBackoff = minimum;

		// seed the PRNG
	set_seed(s);
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

