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

#ifndef COMMAND_STRINGS_H
#define COMMAND_STRINGS_H

#include "condor_common.h"
#include "condor_commands.h"

/* This file contains a mapping from Commands and Signals in 
   condor to the appropriate strings.  Some supporting functions
   are included.  A future implementation should use a hashtable */

/** @name Command and Signal mapping

	Maps commands and signals from name to number and vice-versa.
*/
//@{

/** Given a command number, return the (static buffer) string */
char* get_command_string( int );

/** Given a command/signal name, return the number. */
int get_command_num( char* );

/** Given a collector command number, return the (static buffer) string */
char* get_collector_command_string( int );

/** Given a collector command/signal name, return the number. */
int get_collector_command_num( char* );


#endif
