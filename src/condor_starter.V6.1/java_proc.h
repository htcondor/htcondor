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

#ifndef _CONDOR_JAVA_PROC_H
#define _CONDOR_JAVA_PROC_H

#include "vanilla_proc.h"
#include "condor_classad.h"

typedef enum {
	JAVA_EXIT_NORMAL,
	JAVA_EXIT_EXCEPTION,
	JAVA_EXIT_SYSTEM_ERROR
} java_exit_mode_t;

class JavaProc : public VanillaProc
{
public:
	JavaProc( ClassAd * jobAd, const char *execute_dir );
	virtual ~JavaProc();

	virtual int  StartJob();
	virtual int  JobCleanup( int pid, int status );
	virtual bool PublishUpdateAd( ClassAd *ad );

private:
	int ParseExceptionLine( const char *line, char *name, char *type );
	int ParseExceptionFile( FILE *file );
	java_exit_mode_t ClassifyExit( int status );

	char *execute_dir;
	char startfile[_POSIX_PATH_MAX];
	char endfile[_POSIX_PATH_MAX];
	char ex_name[ATTRLIST_MAX_EXPRESSION];
	char ex_type[ATTRLIST_MAX_EXPRESSION];
	char ex_hier[ATTRLIST_MAX_EXPRESSION];
};

#endif
