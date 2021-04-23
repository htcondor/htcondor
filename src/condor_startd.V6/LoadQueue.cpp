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
#include "startd.h"

LoadQueue::LoadQueue( int queue_size )
{
	q_size = queue_size;
	head = 0;
	buf = new float[q_size];
	this->clear();
}


LoadQueue::~LoadQueue()
{
	delete [] buf;
}


// Return the average value of the queue
float
LoadQueue::avg()
{
	int i;
	float val = 0;
	for( i=0; i<q_size; i++ ) {
		val += buf[i];
	}
	return( (float)val/q_size );
}


// Push num elements onto the array with the given value.
void
LoadQueue::push( int num, float val ) 
{
	int i, j;
	if( num > q_size ) {
		num = q_size;
	}
	for( i=0; i<num; i++ ) {
		j = (head + i) % q_size;
		buf[j] = val;
	}
	head = (head + num) % q_size;
}


// Set all elements of the array to 0
void
LoadQueue::clear() 
{
	memset( (void*)buf, 0, (q_size*sizeof(float)) );
		// Reset the head, too.
	head = 0;
}


void
LoadQueue::setval( float val ) {
	this->push( q_size, val );
}


void
LoadQueue::display( Resource* rip )
{
	int i, j;
	std::string msg;
	msg += "LoadQueue: ";
	char numbuf[64];
	for( i=0; i<q_size; i++ ) {
		j = (head + i) % q_size;
		snprintf( numbuf, 64, "%.2f ", buf[j] );
		msg += numbuf;
	}
	if( rip ) {
		rip->dprintf( D_FULLDEBUG, "%s\n", msg.c_str() );
	} else { 
		dprintf( D_FULLDEBUG, "%s\n", msg.c_str() );
	}
}

