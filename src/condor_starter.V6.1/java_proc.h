/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


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
	virtual bool JobReaper( int pid, int status );
	virtual bool PublishUpdateAd( ClassAd *ad );
	virtual char const *getArgv0();

private:
	int ParseExceptionLine( const char *line, std::string &name, std::string &type );
	int ParseExceptionFile( FILE *file );
	java_exit_mode_t ClassifyExit( int status );

	char *execute_dir;
	std::string startfile;
	std::string endfile;
	std::string ex_name;
	std::string ex_type;
	std::string ex_hier;
};

#endif
