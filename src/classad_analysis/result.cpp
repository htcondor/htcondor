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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"

#include "result.h"
#include <iostream>
#include <sstream>

#define ID_TO_STRING(x) std::string(#x)

namespace classad_analysis {

  static std::string stringize_failure_kind(matchmaking_failure_kind k) {
    switch (k) {
    case MACHINES_REJECTED_BY_JOB_REQS: return ID_TO_STRING(MACHINES_REJECTED_BY_JOB_REQS);
    case MACHINES_REJECTING_JOB: return ID_TO_STRING(MACHINES_REJECTING_JOB);
    case MACHINES_AVAILABLE: return ID_TO_STRING(MACHINES_AVAILABLE);
    case MACHINES_REJECTING_UNKNOWN: return ID_TO_STRING(MACHINES_REJECTING_UNKNOWN);
    case PREEMPTION_REQUIREMENTS_FAILED: return ID_TO_STRING(PREEMPTION_REQUIREMENTS_FAILED);
    case PREEMPTION_PRIORITY_FAILED: return ID_TO_STRING(PREEMPTION_PRIORITY_FAILED);
    case PREEMPTION_FAILED_UNKNOWN: return ID_TO_STRING(PREEMPTION_FAILED_UNKNOWN);
    default: return std::string("UNKNOWN_FAILURE_KIND");
    }
  }

  suggestion::suggestion(suggestion::kind k, const std::string &tgt, const std::string &val) : my_kind(k), target(tgt), value(val) { }

  suggestion::~suggestion() { }

  std::string suggestion::to_string() const {
    std::stringstream sbuilder;
    switch(get_kind()) {
    case NONE:
      return std::string("No suggestion");
    case MODIFY_ATTRIBUTE:
      sbuilder << "Modify attribute " << get_target() << " to " << get_value();
      return sbuilder.str();
    case MODIFY_CONDITION:
      sbuilder << "Modify condition " << get_target() << " to " << get_value();
      return sbuilder.str();
    case DEFINE_ATTRIBUTE:
      sbuilder << "Define attribute " << get_target();
      return sbuilder.str();
    case REMOVE_CONDITION:
      sbuilder << "Remove condition " << get_target();
      return sbuilder.str();
    default:
      // XXX:  assert here
      sbuilder << "Unknown: (" << get_kind() << ", " << get_target() << ", " << get_value() << ")";
      return sbuilder.str();
    }
  }

  namespace job {

  result::result(classad::ClassAd &a_job) : job(a_job), machines(), my_explanation(), my_suggestions() {}
    
  result::result(classad::ClassAd &a_job, std::list<classad::ClassAd> &some_machines) : job(a_job), machines(some_machines), my_explanation(), my_suggestions() {}

  result::~result() {}
  
  void result::add_explanation(matchmaking_failure_kind reason, classad::ClassAd a_machine) {
    my_explanation[reason].push_back(a_machine);
  }

  void result::add_explanation(matchmaking_failure_kind reason, ClassAd *a_machine) {
    classad::ClassAd *new_machine_ad = toNewClassAd(a_machine);
    my_explanation[reason].push_back(*new_machine_ad);
    delete new_machine_ad;
  }

  void result::add_suggestion(suggestion sg) {
    my_suggestions.push_back(sg);
  }

  void result::add_machine(const classad::ClassAd &machine) {
    machines.push_back(machine);
  }

  explanation::const_iterator result::first_explanation() const {
    return my_explanation.begin();
  }

  explanation::const_iterator result::last_explanation() const {
    return my_explanation.end();
  }

  suggestions::const_iterator result::first_suggestion() const {
    return my_suggestions.begin();
  }

  suggestions::const_iterator result::last_suggestion() const {
    return my_suggestions.end();
  }

  const classad::ClassAd& result::job_ad() const {
    return job;
  }

  std::ostream &operator<<(std::ostream &ostr, const result& oresult) {
    ostr << "Explanation of analysis results:" << std::endl;
    
    for (explanation::const_iterator explain = oresult.first_explanation();
	 explain != oresult.last_explanation(); 
	 ++explain) {
      ostr << stringize_failure_kind(explain->first) << std::endl;
      int i = 0;
      for (explanation::value_type::second_type::const_iterator machine = explain->second.begin();
	   machine != explain->second.end(); 
	   ++machine) {
	classad::PrettyPrint pp;
	std::string ad_repr;
	ostr << "=== Machine " << i++ << " ===" << std::endl;
	
	pp.Unparse(ad_repr, &(*machine));
	
	ostr << ad_repr << std::endl;
      }
    }

    ostr << "Suggestions for job requirements:" << std::endl;

    for (suggestions::const_iterator suggest = oresult.first_suggestion();
	 suggest != oresult.last_suggestion();
	 ++suggest) {
      ostr << "\t" << suggest->to_string() << std::endl;
    }

    return ostr;
  }

  }
}
