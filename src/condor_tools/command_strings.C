/* condor legal shit */

#include "condor_common.h"
#include "command_strings.h"

char * get_command_string( int num ) {

	if ( num < 0 ) return NULL;

	int i, found = FALSE;

	for ( i=0 ; (Condor_Trans[i].name != NULL) ; i++ ) {
		if ( Condor_Trans[i].number == num ) {
			found = TRUE;
			break;
		}
	}

	if ( found ) {
		return Condor_Trans[i].name;
	} else {
		return NULL;
	}
}

int get_command_num( char * command ) {

	if ( !command ) return -1;

	int i, found = FALSE;
	
	for ( i=0 ; (Condor_Trans[i].name != NULL) ; i++ ) {
		if ( !stricmp ( Condor_Trans[i].name, command ) ) {
			found = TRUE;
			break;
		}
	}

	if ( found ) {
		return Condor_Trans[i].number;
	} else {
		return -1;
	}
}





