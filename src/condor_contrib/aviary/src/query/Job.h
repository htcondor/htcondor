/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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

#ifndef _JOB_H
#define _JOB_H

// c++ includes
#include <string>
#include <map>
#include <set>
#include <vector>

// condor includes
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"

// local includes
#include "Codec.h"
#include "HistoryFile.h"
#include "cmpstr.h"

using std::string;
using std::map;
using std::set;
using std::vector;

using namespace aviary::codec;
using namespace aviary::history;

namespace aviary {
namespace query {

class SubmissionObject;
class Job;

// interface class for job impls
class JobImpl
{
public:
        Job* getJob() { return m_job; }
        void setJob(Job* _job) { m_job = _job; }

protected:
        Job* m_job;
        JobImpl() {};
};

class ClusterJobImpl;

// Job delegate that encapsulates the job active in the queue
class LiveJobImpl: public JobImpl
{
    public:
        LiveJobImpl ( const char*, ClusterJobImpl* );
        virtual ~LiveJobImpl();
        int getStatus () const;
        const ClassAd* getSummary ();
        const ClassAd* getFullAd () const;
        void set ( const char* , const char* );
        bool get ( const char * , const AviaryAttribute *& ) const;
        void remove ( const char* );
		int getQDate() const;

        virtual bool destroy();

    private:
        ClusterJobImpl* m_parent;
        ClassAd* m_full_ad;
        ClassAd* m_summary_ad;
};

class ClusterJobImpl: public LiveJobImpl
{
public:
        ClusterJobImpl(const char*);
        ~ClusterJobImpl();
        void incrementRef();
        void decrementRef();
        bool destroy();

private:
        int m_ref_count;
};

// Job delegate that encapsulates the archived job history
// and can derive the attributes of its associated class ad
class HistoryJobImpl: public JobImpl
{
    public:
        HistoryJobImpl (const HistoryEntry&);
        ~HistoryJobImpl();
        int  getStatus () const;
        void getSummary ( ClassAd& ) const;
        void getFullAd ( ClassAd& ) const;
        int  getCluster() const;
        const char* getSubmissionId () const;
		int getQDate() const;

    private:
        HistoryEntry m_he;
};

// the public face of jobs
// not for subclassing - clients (generally) shouldn't be
// burdened with the live/history distinction
class Job
{
    public:
        Job(const char*);
        ~Job();

        const char * getKey() const;

        int  getStatus () const;
        void setStatus(int);

        void getSummary (ClassAd&) const;
        void getFullAd (ClassAd&) const;
        const ClassAd* getClassAd() const;
        int getQDate() const;

        void set ( const char* , const char* );
        void remove ( const char* );

        void setSubmission ( const char*, int );
        void updateSubmission ( int, const char* );
        void incrementSubmission();
        void decrementSubmission();

        void setImpl(LiveJobImpl*);
        void setImpl(HistoryJobImpl*);
        JobImpl* getImpl();

        bool destroy();

    private:
        SubmissionObject *m_submission;
        // live job to start, history job when ready
        LiveJobImpl* m_live_job;
        HistoryJobImpl* m_history_job;
        const char* m_key;
        int m_status;
};

}}

#endif /* _JOB_H */
