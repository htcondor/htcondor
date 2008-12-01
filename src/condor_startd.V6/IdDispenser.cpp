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

#include "IdDispenser.h"


IdDispenser::IdDispenser( int size, int seed ) :
	free_ids(size+2)
{
	int i;
	free_ids.setFiller(true);
	free_ids.fill(true);
	for( i=0; i<seed; i++ ) {
		free_ids[i] = false;
	}
}


int
IdDispenser::next( void )
{
	int i;
	for( i=1 ; ; i++ ) {
		if( free_ids[i] ) {
			free_ids[i] = false;
			return i;
		}
	}
}


void
IdDispenser::insert( int id )
{
	if( free_ids[id] ) {
		EXCEPT( "IdDispenser::insert: %d is already free", id );
	}
	free_ids[id] = true;
}
