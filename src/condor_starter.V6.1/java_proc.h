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
