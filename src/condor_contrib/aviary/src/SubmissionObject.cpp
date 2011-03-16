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

using namespace aviary::query;
using namespace aviary::codec;
using namespace aviary::util;

SubmissionObject::SubmissionObject (
                                     const char *_name,
                                     const char *_owner ) :
        ownerSet ( false )
{
	m_name = _name;
    if ( _owner )
    {
        setOwner ( _owner );
    }
    else
    {
        setOwner ( "Unknown" );
        ownerSet = false;
    }

    m_codec = new BaseCodec;

    dprintf ( D_FULLDEBUG, "Created new SubmissionObject '%s' for '%s'\n", _name, _owner);
}

SubmissionObject::~SubmissionObject()
{
	dprintf ( D_FULLDEBUG, "SubmissionObject::~SubmissionObject for '%s'\n", m_name.c_str());
	delete m_codec;
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

void
SubmissionObject::setOwner ( const char *_owner )
{
    if ( !ownerSet )
    {
        m_owner = _owner;
        ownerSet = true;
    }
}

bool
SubmissionObject::getJobSummaries ( AttributeVectorType &jobs,
                            std::string &text )
{

    // id, timestamp (which?), command, args, ins, outs, state, message
    // id, time queued, time entered current state, state, command, args, hold reason, release reason

	ClassAd ad;
	AttributeMapType job;

    // find all the jobs in their various states...

    //1) Idle
    for ( SubmissionObject::JobSet::const_iterator i = getIdle().begin();
            getIdle().end() != i; i++ )
    {
	    (*i)->getSummary(ad);
	if ( !m_codec->classAdToMap ( ad, job ) )
        {
            text = "Error retrieving attributes for Idle job";
            return false;
        }

        jobs.push_back(job);
    }

    //2) Running
    for ( SubmissionObject::JobSet::const_iterator i = getRunning().begin();
            getRunning().end() != i;
            i++ )
    {
	    (*i)->getSummary(ad);
	if ( !m_codec->classAdToMap ( ad, job ) )
        {
            text = "Error retrieving attributes for Running job";
            return false;
        }

        jobs.push_back(job);
    }

    //3) Removed
    for ( SubmissionObject::JobSet::const_iterator i = getRemoved().begin();
            getRemoved().end() != i; i++ )
    {
	    (*i)->getSummary(ad);
	if ( !m_codec->classAdToMap ( ad, job ) )
        {
            text = "Error retrieving attributes for Removed job";
            return false;
        }

        jobs.push_back(job);
    }

    //4) Completed
    for ( SubmissionObject::JobSet::const_iterator i = getCompleted().begin();
            getCompleted().end() != i; i++ )
    {
	    (*i)->getSummary(ad);
	if ( !m_codec->classAdToMap ( ad, job ) )
        {
            text = "Error retrieving attributes for Completed job";
            return false;
        }

        jobs.push_back(job);
    }


    //5) Held
    for ( SubmissionObject::JobSet::const_iterator i = getHeld().begin();
            getHeld().end() != i; i++ )
    {
	    (*i)->getSummary(ad);
	if ( !m_codec->classAdToMap ( ad, job ) )
        {
            text = "Error retrieving attributes for Held job";
            return false;
        }

        jobs.push_back(job);
    }

    return true;
}
