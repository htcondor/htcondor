#ifndef _CONDOR_IDDISPENSER_H
#define _CONDOR_IDDISPENSER_H

#include "extArray.h"

class IdDispenser
{
public:
	IdDispenser( int size, int seed );
	int		next( void );
	void	insert( int id );
private:
	ExtArray<bool> free_ids;
};

#endif
