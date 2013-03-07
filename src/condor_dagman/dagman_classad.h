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


#ifndef DAGMAN_CLASSAD_H
#define DAGMAN_CLASSAD_H

#include "condor_common.h"
#include "condor_id.h"

class DCSchedd;

class DagmanClassad {
  public:
	DagmanClassad( const CondorID &DAGManJobId );
	~DagmanClassad();

	//TEMPTEMP -- add held?
	//TEMPTEMP -- add DAG status?
	void Update( int total, int done, int pre, int submitted, int post,
				int ready, int failed, int unready );

  private:
	void SetDagAttribute( const char *attrName, int attrVal );

	bool _valid;

	CondorID _dagmanId;

	DCSchedd *_schedd;
};

#endif /* #ifndef DAGMAN_CLASSAD_H */
