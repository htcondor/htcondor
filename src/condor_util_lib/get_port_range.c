/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

/* prototype in condor_includes/get_port_range.h */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

int get_port_range(int *low_port, int *high_port)
{
	char *low = NULL, *high = NULL;

	if ( (low = param("LOWPORT")) == NULL ) {
        dprintf(D_NETWORK, "LOWPORT undefined\n");
		return FALSE;
    }
	if ( (high = param("HIGHPORT")) == NULL ) {
		free(low);
        dprintf(D_ALWAYS, "LOWPORT is defined but, HIGHPORT undefined!\n");
		return FALSE;
	}

    *low_port = atoi(low);
    *high_port = atoi(high);

    if(*low_port < 1024 || *high_port < 1024 || *low_port > *high_port) {
        dprintf(D_ALWAYS, "get_port_range - invalid LOWPORT(%d) \
                           and/or HIGHPORT(%d)\n",
                           *low_port, *high_port);
        free(low);
        free(high);
        return FALSE;
    }

    free(low);
    free(high);
    return TRUE;
}


int
_condor_bind_all_interfaces( void )
{
	int bind_all = FALSE;
	char* tmp = param( "BIND_ALL_INTERFACES" );
	if( ! tmp ) {
			// not defined, defaualts to FALSE;
		return FALSE;
	}
	switch( tmp[0] ) {
	case 'T':
	case 't':
	case 'Y':
	case 'y':
		bind_all = TRUE;
		break;
	default:
		bind_all = FALSE;
		break;
	}
	free( tmp );
	return bind_all;
}




