/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
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
#include "condor_debug.h"
#include "condor_parameters.h"
#include "condor_config.h"
#include "condor_network.h" 

// Collector parameters
const char *PARAM_COLLECTOR_PORT    = "COLLECTOR_PORT";
const char *PARAM_CONDOR_VIEW_PORT  = "CONDOR_VIEW_PORT";

// Negotiator parameters
const char *PARAM_NEGOTIATOR_PORT   = "NEGOTIATOR_PORT";

/****************************************************************************
 *
 * Function: param_get_collector_port
 * Purpose:  Find the port that the collector should be running on. 
 *           This occurs in multiple places in the code, so we made it
 *           into a function, even though it is rather simple.
 *
 ****************************************************************************/
int param_get_collector_port(void)
{
	int collector_port;

	collector_port = param_integer(PARAM_COLLECTOR_PORT, COLLECTOR_PORT);

	if (collector_port <= 0 || collector_port >= 65536) {
		dprintf(D_ALWAYS, "%s has bad value (%d), resetting to %d\n",
				PARAM_COLLECTOR_PORT, collector_port, COLLECTOR_PORT);
		collector_port = COLLECTOR_PORT;
	}

	return collector_port;
}

/****************************************************************************
 *
 * Function: param_get_condor_view_port
 * Purpose: Find the port that the Condor View collector should be
 *          running on. This occurs in multiple places in the code, so we 
 *          made it into a function, even though it is rather simple.
 *
 ****************************************************************************/
int param_get_condor_view_port(void)
{
	int condor_view_port;

	condor_view_port = param_integer(PARAM_CONDOR_VIEW_PORT, CONDOR_VIEW_PORT);
	if (condor_view_port <= 0 || condor_view_port >= 65536) {
		dprintf(D_ALWAYS, "%s has bad value (%d), resetting to %d\n",
				PARAM_CONDOR_VIEW_PORT, condor_view_port, CONDOR_VIEW_PORT);
		condor_view_port = CONDOR_VIEW_PORT;
	}

	return condor_view_port;
}

/****************************************************************************
 *
 * Function: param_get_negotiator_port
 * Purpose:  Find the port that the negotiator should be running on. 
 *           This occurs in multiple places in the code, so we made it
 *           into a function, even though it is rather simple.
 *
 ****************************************************************************/
int param_get_negotiator_port(void)
{
	int negotiator_port;

	negotiator_port = param_integer(PARAM_NEGOTIATOR_PORT, NEGOTIATOR_PORT);
	if (negotiator_port <= 0 || negotiator_port >= 65536) {
		dprintf(D_ALWAYS, "%s has bad value (%d), resetting to %d\n",
				PARAM_NEGOTIATOR_PORT, negotiator_port, NEGOTIATOR_PORT);
		negotiator_port = NEGOTIATOR_PORT;
	}
	return negotiator_port;
}
