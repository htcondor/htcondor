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
#ifndef DAGMAN_COMMANDS_H
#define DAGMAN_COMMANDS_H

/*
#include "condor_common.h"
#include "types.h"
#include "debug.h"
#include "script.h"
*/

// pause DAGMan's event-processing so changes can be made safely
bool PauseDag(Dagman &dm);
// resume DAGMan's normal event-processing
bool ResumeDag(Dagman &dm);

bool AddNode( const Dagman &dm, Job::job_type_t type, const char *name,
			  const char* cmd,
			  const char *precmd, const char *postcmd, bool done,
			  MyString &failReason );

bool IsValidNodeName( const Dagman &dm, const char *name, MyString &whynot );
bool IsValidSubmitFileName( const char *name, MyString &whynot );

/*
bool RemoveNode( const char *name );
bool AddDependency( const char *parentName, const *childName );
bool RemoveDependency( const char *parentName, const *childName );
*/

#endif	// ifndef DAGMAN_COMMANDS_H
