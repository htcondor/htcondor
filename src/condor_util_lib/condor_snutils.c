/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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

/*
** Compatibility routine for systems that don't have an snprintf
*/

#ifdef NEED_SNPRINTF
int 
snprintf( char *str, size_t size, const  char  *format, ... )
{
	int len;

	va_list args;
	va_start( args, format );

	len = vsprintf(str, format, args);
	va_end(args);

	/* if len > size, this would be a good place to note that... */
	/* according to the linux manpage, snprintf returns the number of bytes */
	/* that WOULD have been written, had space been available. Since we're */
	/* not checking space requirements, go ahead and lie and say what we */
	/* actually wrote - we can code as though the write "failed" and deal */
	/* with it at the caller */
	return len;
}
#endif
