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
