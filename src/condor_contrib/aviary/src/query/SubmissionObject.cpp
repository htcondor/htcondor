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

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "proc.h"

// local includes
#include "SubmissionObject.h"
#include "JobServerObject.h"
#include "AviaryUtils.h"
#include "Globals.h"

using namespace std;
using namespace aviary::query;
using namespace aviary::codec;
using namespace aviary::util;

SubmissionObject::SubmissionObject ( const char* name,
                                     const char* owner ) :
        m_name(NULL),m_owner(NULL), m_oldest_qdate(time(NULL)+hashFuncChars(name))
{

    setName ( name );
    setOwner ( owner );

    m_codec = new BaseCodec;

    dprintf ( D_FULLDEBUG, "Created new SubmissionObject '%s' for '%s'\n", name, owner);
}

SubmissionObject::~SubmissionObject()
{
    dprintf ( D_FULLDEBUG, "SubmissionObject::~SubmissionObject for '%s'\n", m_name);
    if (m_codec) delete m_codec;
    if (m_owner) free((void*)m_owner);
    if (m_name) free((void*)m_name);
}

void
SubmissionObject::increment ( const Job *job )
{
    int status = job->getStatus();

    dprintf ( D_FULLDEBUG, "SubmissionObject::increment '%s' on '%s'\n", getJobStatusString(status), job->getKey());

    switch ( status )
    {
        case IDLE:
            m_idle.insert ( job );
            break;
        case RUNNING:
            m_running.insert ( job );
            break;
        case REMOVED:
            m_removed.insert ( job );
            break;
        case COMPLETED:
            m_completed.insert ( job );
            break;
        case HELD:
            m_held.insert ( job );
            break;
        case TRANSFERRING_OUTPUT:
            m_transferring_output.insert ( job );
            break;
        case SUSPENDED:
            m_suspended.insert ( job );
            break;
        default:
            dprintf ( D_ALWAYS, "error: Unknown %s of %d on %s\n",
                      ATTR_JOB_STATUS, status, job->getKey() );
            break;
    }
}

void
SubmissionObject::decrement ( const Job *job )
{
    int status = job->getStatus();

    dprintf ( D_FULLDEBUG, "SubmissionObject::decrement '%s' on '%s'\n", getJobStatusString(status), job->getKey());

    switch ( status )
    {
        case IDLE:
            m_idle.erase ( job );
            break;
        case RUNNING:
            m_running.erase ( job );
            break;
        case REMOVED:
            m_removed.erase ( job );
            break;
        case COMPLETED:
            m_completed.erase ( job );
            break;
        case HELD:
            m_held.erase ( job );
            break;
        case TRANSFERRING_OUTPUT:
            m_transferring_output.erase ( job );
            break;
        case SUSPENDED:
            m_suspended.erase ( job );
            break;
        default:
            dprintf ( D_ALWAYS, "error: Unknown %s of %d on %s\n",
                      ATTR_JOB_STATUS, status, job->getKey() );
            break;
    }
}


const SubmissionObject::JobSet &
SubmissionObject::getIdle()
{
    return m_idle;
}

const SubmissionObject::JobSet &
SubmissionObject::getRunning()
{
    return m_running;
}

const SubmissionObject::JobSet &
SubmissionObject::getRemoved()
{
    return m_removed;
}

const SubmissionObject::JobSet &
SubmissionObject::getCompleted()
{
    return m_completed;
}

const SubmissionObject::JobSet &
SubmissionObject::getHeld()
{
    return m_held;
}

const SubmissionObject::JobSet &
SubmissionObject::getTransferringOutput()
{
    return m_transferring_output;
}

const SubmissionObject::JobSet &
SubmissionObject::getSuspended()
{
    return m_suspended;
}

void
SubmissionObject::setName ( const char* name )
{
    if (name)
    {
        m_name = strdup(name);
    }
}

void
SubmissionObject::setOwner ( const char *owner )
{
    if (owner)
    {
        m_owner = strdup(owner);
    }
}

JobSummaryPair makeJobPair(const Job* job) {
	JobServerObject* jso = JobServerObject::getInstance();
	const char* job_cp = job->getKey();
	JobSummaryFields* jsf = new JobSummaryFields;
	AviaryStatus status;
	// TODO: should check this return val i suppose
	jso->getSummary(job_cp, *jsf, status);
	return make_pair(job_cp,jsf);
}

void
SubmissionObject::getJobSummaries ( JobSummaryPairCollection &jobs)
{

    // id, timestamp (which?), command, args, ins, outs, state, message
    // id, time queued, time entered current state, state, command, args, hold reason, release reason

    // find all the jobs in their various states...

    //1) Idle
    for ( SubmissionObject::JobSet::const_iterator i = getIdle().begin();
            getIdle().end() != i; i++ )
    {
		jobs.push_back(makeJobPair(*i));
	}

    //2) Running
    for ( SubmissionObject::JobSet::const_iterator i = getRunning().begin();
            getRunning().end() != i;
            i++ )
    {
		jobs.push_back(makeJobPair(*i));
    }

    //3) Removed
    for ( SubmissionObject::JobSet::const_iterator i = getRemoved().begin();
            getRemoved().end() != i; i++ )
    {
		jobs.push_back(makeJobPair(*i));
    }

    //4) Completed
    for ( SubmissionObject::JobSet::const_iterator i = getCompleted().begin();
            getCompleted().end() != i; i++ )
    {
		jobs.push_back(makeJobPair(*i));
    }

    //5) Held
    for ( SubmissionObject::JobSet::const_iterator i = getHeld().begin();
            getHeld().end() != i; i++ )
    {
		jobs.push_back(makeJobPair(*i));
    }

    //6) Transferring Output
    for ( SubmissionObject::JobSet::const_iterator i = getTransferringOutput().begin();
            getTransferringOutput().end() != i; i++ )
    {
		jobs.push_back(makeJobPair(*i));
    }

    //7) Suspended
    for ( SubmissionObject::JobSet::const_iterator i = getSuspended().begin();
            getSuspended().end() != i; i++ )
    {
		jobs.push_back(makeJobPair(*i));
    }

}

// setter/getters for tracking oldest job in submission
int
SubmissionObject::getOldest() {
	return m_oldest_qdate;
}

void
SubmissionObject::setOldest(int qdate) {
    if (m_oldest_qdate > qdate) {
        // swap the old one out
        SubmissionMultiIndexType::iterator it = g_qdate_submissions.find(m_oldest_qdate);
        if (it != g_qdate_submissions.end()) {
            g_qdate_submissions.erase(m_oldest_qdate);
            g_qdate_submissions.insert(make_pair(qdate,this));
        }
        m_oldest_qdate=qdate;
    }
}
