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

#ifndef ANALYSIS_ANALYSISRESULT_H
#define ANALYSIS_ANALYSISRESULT_H

#include <list>
#include <map>
#include <set>
#include <string>
#include "condor_classad.h"
#include "conversion.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"

namespace classad_analysis {
  
  enum matchmaking_failure_kind {
    UNKNOWN_FAILURE_KIND,
    MACHINES_REJECTED_BY_JOB_REQS,
    MACHINES_REJECTING_JOB,
    MACHINES_AVAILABLE,
    MACHINES_REJECTING_UNKNOWN,
    PREEMPTION_REQUIREMENTS_FAILED,
    PREEMPTION_PRIORITY_FAILED,
    PREEMPTION_FAILED_UNKNOWN
  };

  class suggestion {
  public:
    enum kind {
      NONE,
      MODIFY_ATTRIBUTE, /* change the specified attribute */
      MODIFY_CONDITION, /* change the specified condition */
      REMOVE_CONDITION, /* remove the specified condition */
      DEFINE_ATTRIBUTE  /* define the specified attribute */
    };
    suggestion(kind k, const std::string &tgt, const std::string &val="");
    ~suggestion();

    kind get_kind() const { return my_kind; }
    std::string get_target() const { return target; }
    std::string get_value() const { return value; }
    std::string to_string() const;

  private:
    kind my_kind;

    /* what needs to be changed?  (i.e attribute or condition) */
    std::string target;

    /* what should its value be?  (optional, only applies to attributes) */
    std::string value;
  };
  
  /* An explanation is a mapping from reasons for rejection to a
     collection of (new) machine ads that cannot run the job for that
     reason */
  typedef std::map<matchmaking_failure_kind, std::vector<classad::ClassAd> > explanation;
  typedef std::list<suggestion> suggestions;
  
  
  namespace job {
  /* 
     job::result represents the diagnosis from a ClassAd analysis. 
       
     Essentially, it represents a mapping from a machine ad to
     *explanations* and a list of *suggestions*.  An explanation is a
     mapping from reasons for rejection to lists of machine ads.  A
     suggestion is a string indicating a recommended change to job
     requirements.
       
     This class represents the analysis of a job in the context of
     several machines.  It is in namespace classad_analysis::job
     in order to distinguish it from an as-yet-unwritten machine::result
     class that could represent the analysis of a machine in the context
     of several jobs.
  */
  
  class result {
  public:
    result();
    result(classad::ClassAd &a_job);
    result(classad::ClassAd &a_job, std::list<classad::ClassAd> &some_machines);
    ~result();
    
    /* Adds the (new) classad for a_machine to the collection of
       machines that fail to run a job because of reason */
    void add_explanation(matchmaking_failure_kind reason, classad::ClassAd a_machine);
    /* As above; creates a new classad from (old classad) a_machine.
       Does not take ownership of a_machine. */
    void add_explanation(matchmaking_failure_kind reason, ClassAd *a_machine);
    void add_suggestion(suggestion sg);
    void add_machine(const classad::ClassAd &machine);

    explanation::const_iterator first_explanation() const;
    suggestions::const_iterator first_suggestion() const; 

    explanation::const_iterator last_explanation() const;
    suggestions::const_iterator last_suggestion() const; 

    const classad::ClassAd& job_ad() const;
  private:
    classad::ClassAd job;
    std::list<classad::ClassAd> machines;
    explanation my_explanation;
    suggestions my_suggestions;
  };

  std::ostream &operator<<(std::ostream &ostr, const result& oresult);
    
  }
}

#endif /* ANALYSIS_ANALYSISRESULT_H */



/*
 * Local Variables: ***
 * mode: C++ ***
 * End: ***
 */
