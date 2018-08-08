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

#ifndef STARTD_NAMED_CLASSAD_H
#define STARTD_NAMED_CLASSAD_H

#include "condor_common.h"
#include "named_classad.h"

// A name / ClassAd pair to manage together
class StartdCronJob;
class StartdNamedClassAd : public NamedClassAd
{
  public:
	StartdNamedClassAd( const char *name, StartdCronJob &job, ClassAd * ad = NULL );
	~StartdNamedClassAd( void ) { };
	bool InSlotList( unsigned slot ) const;
	StartdNamedClassAd * NewPeer( const char * name, ClassAd * ad = NULL ) { return new StartdNamedClassAd(name, m_job, ad); }
	bool IsJob(StartdCronJob * job) const { return &m_job == job; }
	bool ShouldMergeInto(ClassAd * merge_into, const char ** pattr_used);
	bool MergeInto(ClassAd *merge_to);

	void AggregateFrom(ClassAd *aggregateFrom);
	bool AggregateInto(ClassAd *aggregateInfo);
	bool isResourceMonitor();
	static bool Merge( ClassAd * to, ClassAd * from );
	void reset_monitor();
	void unset_monitor();

  private:
	void Aggregate( ClassAd * to, ClassAd * from );

	static AttrNameSet dont_merge_attrs;
	StartdCronJob	&m_job;
};

#endif
