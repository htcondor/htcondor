#include "error.h"

/*


State_UnknownError

*/
int State_UnknownError( TransferState* state )
{
	ENTER_STATE;
	if( state->error_string[0] )
		fprintf( stderr, "ERROR: %s\n", state->error_string );
	else
		fprintf( stderr, "ERROR: Unexpected failure, but no error string is set.\n");

	LEAVE_STATE(0);
}


/*


State_SendSessionClose

*/
int State_SendSessionClose( TransferState* state )
{
	ENTER_STATE;



	LEAVE_STATE(0);
}


