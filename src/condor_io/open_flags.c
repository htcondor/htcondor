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

 
#include "condor_common.h"
#include <fcntl.h>

#define CONDOR_O_RDONLY 0x0000
#define CONDOR_O_WRONLY 0x0001
#define CONDOR_O_RDWR	0x0002
#define CONDOR_O_CREAT	0x0100
#define CONDOR_O_TRUNC	0x0200
#define CONDOR_O_EXCL	0x0400
#define CONDOR_O_NOCTTY	0x0800
#define CONDOR_O_APPEND 0x1000

static struct {
	int		system_flag;
	int		condor_flag;
} FlagList[] = {
	{O_RDONLY, CONDOR_O_RDONLY},
	{O_WRONLY, CONDOR_O_WRONLY},
	{O_RDWR, CONDOR_O_RDWR},
	{O_CREAT, CONDOR_O_CREAT},
	{O_TRUNC, CONDOR_O_TRUNC},
#ifndef WIN32
	{O_NOCTTY, CONDOR_O_NOCTTY},
#endif
	{O_EXCL, CONDOR_O_EXCL},
	{O_APPEND, CONDOR_O_APPEND}
};

int open_flags_encode(int old_flags)
{
	int i;
	int new_flags = 0;

	for (i = 0; i < (sizeof(FlagList) / sizeof(FlagList[0])); i++) {
		if (old_flags & FlagList[i].system_flag) {
			new_flags |= FlagList[i].condor_flag;
		}
	}
	return new_flags;
}

int open_flags_decode(int old_flags)
{
	int i;
	int new_flags = 0;

	for (i = 0; i < (sizeof(FlagList) / sizeof(FlagList[0])); i++) {
		if (old_flags & FlagList[i].condor_flag) {
			new_flags |= FlagList[i].system_flag;
		}
	}
#if defined(WIN32)
	new_flags |= _O_BINARY;	// always open in binary mode
#endif
	return new_flags;
}
