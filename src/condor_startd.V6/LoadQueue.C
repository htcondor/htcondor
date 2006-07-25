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
	MyString msg;
	msg += "LoadQueue: ";
	char numbuf[64];
	for( i=0; i<q_size; i++ ) {
		j = (head + i) % q_size;
		snprintf( numbuf, 64, "%.2f ", buf[j] );
		msg += numbuf;
	}
	if( rip ) {
		rip->dprintf( D_FULLDEBUG, "%s\n", msg.Value() );
	} else { 
		dprintf( D_FULLDEBUG, "%s\n", msg.Value() );
	}
}

