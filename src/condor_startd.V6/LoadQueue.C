/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
LoadQueue::display() {
	int i, j;
	dprintf( D_FULLDEBUG, "LoadQueue: " );
	for( i=0; i<q_size; i++ ) {
		j = (head + i) % q_size;
		dprintf( D_FULLDEBUG | D_NOHEADER, "%f ", buf[j] );
	}
	dprintf( D_FULLDEBUG | D_NOHEADER, "\n" );
}

