#include "condor_common.h"

char *
strnewp( const char *str ) 
{
	if( !str ) return( (char*) 0 );

	char *newstr = new char[ strlen( str ) + 1 ];
	return( newstr ? strcpy( newstr, str ) : (char*) 0 );
}
