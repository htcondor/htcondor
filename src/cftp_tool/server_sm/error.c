#include "error.h"

/*


State_UnknownError

*/
int State_UnknownError( ServerState* state )
{
	if( state->error_string[0] )
		fprintf( stderr, "ERROR: %s\n", state->error_string );
	else
		fprintf( stderr, "ERROR: Unexpected failure, but no error string is set.\n");

	return 0;
}


/*


State_SendSessionClose

*/
int State_SendSessionClose( ServerState* state )
{





	return 0;
}


