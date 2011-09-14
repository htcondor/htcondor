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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"

#include "HistoryFile.h"

#include "cmpstr.h"

#include <string>
#include <map>
#include <set>
#include <vector>

using std::string;
using std::map;
using std::set;
using std::vector;

class SubmissionObject;
class Job;

class Attribute
{
    public:
        enum AttributeType
        {
            EXPR_TYPE = 0,
            INTEGER_TYPE = 1,
            FLOAT_TYPE = 2,
            STRING_TYPE = 3
        };
        Attribute ( AttributeType , const char* );
        ~Attribute();

        AttributeType GetType() const;
        const char * GetValue() const;

    private:
        AttributeType m_type;
        std::string m_value;
};


// interface class for job impls
class JobImpl
{
public:
        Job* GetJob() { return m_job; }
        void SetJob(Job* _job) { m_job = _job; }

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
        int GetStatus () const;
        const ClassAd* GetSummary ();
        const ClassAd* GetFullAd () const;
		int GetQDate() const;
        void Set ( const char* , const char* );
        bool Get ( const char * , const Attribute *& ) const;
        void Remove ( const char* );

        virtual bool Destroy();

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
        void IncrementRef();
        void DecrementRef();
        bool Destroy();

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
        int  GetStatus () const;
        void GetSummary ( ClassAd& ) const;
        void GetFullAd ( ClassAd& ) const;
        int  GetCluster() const;
        const char* GetSubmissionId () const;
		int GetQDate() const;

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

        const char * GetKey() const;

        int  GetStatus () const;
        void SetStatus(int);

        void GetSummary (ClassAd&) const;
        void GetFullAd (ClassAd&) const;
        const ClassAd* GetClassAd() const;
        int GetQDate() const;

        void Set ( const char* , const char* );
        void Remove ( const char* );

        void SetSubmission ( const char*, int );
        void UpdateSubmission ( int, const char* );
        void IncrementSubmission();
        void DecrementSubmission();

        void SetImpl(LiveJobImpl*);
        void SetImpl(HistoryJobImpl*);
        JobImpl* GetImpl();

        bool Destroy();

    private:
        SubmissionObject *m_submission;
        // live job to start, history job when ready
        LiveJobImpl* m_live_job;
        HistoryJobImpl* m_history_job;
        const char* m_key;
        int m_status;
};

#endif /* _JOB_H */
