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


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "proc.h"

#include "SubmissionObject.h"
#include "JobServerObject.h"
#include "ArgsSubmissionGetJobSummaries.h"

#include "Utils.h"

using namespace com::redhat::grid;
using namespace qmf::com::redhat::grid;

SubmissionObject::SubmissionObject ( ManagementAgent *agent,
				     JobServerObject* job_server,
                                     const char *name,
                                     const char *owner ) :
        ownerSet ( false )
{
    mgmtObject = new Submission ( agent, this, job_server );
    ASSERT ( mgmtObject );

    mgmtObject->set_Name ( string ( name ) );
    if ( owner )
    {
        SetOwner ( owner );
    }
    else
    {
        SetOwner ( "Unknown" );
        ownerSet = false;
    }
    mgmtObject->set_QDate((uint64_t) time(NULL)*1000000000);

    // By default the submission will be persistent.
    bool _lifetime = param_boolean("QMF_IS_PERSISTENT", true);
    agent->addObject ( mgmtObject, name,  _lifetime);

    dprintf ( D_FULLDEBUG, "Created new SubmissionObject '%s' for '%s'\n", name, owner);
}

SubmissionObject::~SubmissionObject()
{
	dprintf ( D_FULLDEBUG, "SubmissionObject::~SubmissionObject for '%s'\n", mgmtObject->get_Name().c_str());
    if ( mgmtObject )
    {
        mgmtObject->resourceDestroy();
    }
}

ManagementObject *
SubmissionObject::GetManagementObject ( void ) const
{
    return mgmtObject;
}

void
SubmissionObject::Increment ( const Job *job )
{
    int status = job->GetStatus();

    dprintf ( D_FULLDEBUG, "SubmissionObject::Increment '%s' on '%s'\n", getJobStatusString(status), job->GetKey());

    switch ( status )
    {
        case IDLE:
            m_idle.insert ( job );
            mgmtObject->inc_Idle();
            break;
        case RUNNING:
            m_running.insert ( job );
            mgmtObject->inc_Running();
            break;
        case REMOVED:
            m_removed.insert ( job );
            mgmtObject->inc_Removed();
            break;
        case COMPLETED:
            m_completed.insert ( job );
            mgmtObject->inc_Completed();
            break;
        case HELD:
            m_held.insert ( job );
            mgmtObject->inc_Held();
            break;
        case TRANSFERRING_OUTPUT:
            m_transferring_output.insert ( job );
            mgmtObject->inc_TransferringOutput();
            break;
        case SUSPENDED:
            m_suspended.insert ( job );
            mgmtObject->inc_Suspended();
            break;
        default:
            dprintf ( D_ALWAYS, "error: Unknown %s of %d on %s\n",
                      ATTR_JOB_STATUS, status, job->GetKey() );
            break;
    }
}

void
SubmissionObject::Decrement ( const Job *job )
{
    int status = job->GetStatus();

    dprintf ( D_FULLDEBUG, "SubmissionObject::Decrement '%s' on '%s'\n", getJobStatusString(status), job->GetKey());

    switch ( status )
    {
        case IDLE:
            m_idle.erase ( job );
            mgmtObject->dec_Idle();
            break;
        case RUNNING:
            m_running.erase ( job );
            mgmtObject->dec_Running();
            break;
        case REMOVED:
            m_removed.erase ( job );
            mgmtObject->dec_Removed();
            break;
        case COMPLETED:
            m_completed.erase ( job );
            mgmtObject->dec_Completed();
            break;
        case HELD:
            m_held.erase ( job );
            mgmtObject->dec_Held();
            break;
        case TRANSFERRING_OUTPUT:
            m_transferring_output.erase ( job );
            mgmtObject->dec_TransferringOutput();
            break;
        case SUSPENDED:
            m_suspended.erase ( job );
            mgmtObject->dec_Suspended();
            break;
        default:
            dprintf ( D_ALWAYS, "error: Unknown %s of %d on %s\n",
                      ATTR_JOB_STATUS, status, job->GetKey() );
            break;
    }
}


const SubmissionObject::JobSet &
SubmissionObject::GetIdle()
{
    return m_idle;
}

const SubmissionObject::JobSet &
SubmissionObject::GetRunning()
{
    return m_running;
}

const SubmissionObject::JobSet &
SubmissionObject::GetRemoved()
{
    return m_removed;
}

const SubmissionObject::JobSet &
SubmissionObject::GetCompleted()
{
    return m_completed;
}

const SubmissionObject::JobSet &
SubmissionObject::GetHeld()
{
    return m_held;
}

const SubmissionObject::JobSet &
SubmissionObject::GetTransferringOutput()
{
    return m_transferring_output;
}

const SubmissionObject::JobSet &
SubmissionObject::GetSuspended()
{
    return m_suspended;
}

void
SubmissionObject::SetOwner ( const char *owner )
{
    if (owner && !ownerSet )
    {
        mgmtObject->set_Owner ( string(owner) );
        ownerSet = true;
    }
}

// update the oldest job qdate for this submission
void
SubmissionObject::UpdateQdate(int q_date) {
	int old = mgmtObject->get_QDate();
	if (q_date < old) {
			mgmtObject->set_QDate((uint64_t) q_date*1000000000);
	}
}

Manageable::status_t
SubmissionObject::GetJobSummaries ( Variant::List &jobs,
                            std::string &text )
{

    // id, timestamp (which?), command, args, ins, outs, state, message
    // id, time queued, time entered current state, state, command, args, hold reason, release reason

	ClassAd ad;
	Variant::Map job;

    // find all the jobs in their various states...

    //1) Idle
    for ( SubmissionObject::JobSet::const_iterator i = GetIdle().begin();
            GetIdle().end() != i; i++ )
    {
	    (*i)->GetSummary(ad);
	if ( !PopulateVariantMapFromAd ( ad, job ) )
        {
            text = "Error retrieving attributes for Idle job";
            return STATUS_USER + 1;
        }

        jobs.push_back(job);
    }

    //2) Running
    for ( SubmissionObject::JobSet::const_iterator i = GetRunning().begin();
            GetRunning().end() != i;
            i++ )
    {
	    (*i)->GetSummary(ad);
	if ( !PopulateVariantMapFromAd ( ad, job ) )
        {
            text = "Error retrieving attributes for Running job";
            return STATUS_USER + 1;
        }

        jobs.push_back(job);
    }

    //3) Removed
    for ( SubmissionObject::JobSet::const_iterator i = GetRemoved().begin();
            GetRemoved().end() != i; i++ )
    {
	    (*i)->GetSummary(ad);
	if ( !PopulateVariantMapFromAd ( ad, job ) )
        {
            text = "Error retrieving attributes for Removed job";
            return STATUS_USER + 1;
        }

        jobs.push_back(job);
    }

    //4) Completed
    for ( SubmissionObject::JobSet::const_iterator i = GetCompleted().begin();
            GetCompleted().end() != i; i++ )
    {
	    (*i)->GetSummary(ad);
	if ( !PopulateVariantMapFromAd ( ad, job ) )
        {
            text = "Error retrieving attributes for Completed job";
            return STATUS_USER + 1;
        }

        jobs.push_back(job);
    }


    //5) Held
    for ( SubmissionObject::JobSet::const_iterator i = GetHeld().begin();
            GetHeld().end() != i; i++ )
    {
	    (*i)->GetSummary(ad);
	if ( !PopulateVariantMapFromAd ( ad, job ) )
        {
            text = "Error retrieving attributes for Held job";
            return STATUS_USER + 1;
        }

        jobs.push_back(job);
    }

    //6) Transferring Output
    for ( SubmissionObject::JobSet::const_iterator i = GetTransferringOutput().begin();
            GetTransferringOutput().end() != i; i++ )
    {
	    (*i)->GetSummary(ad);
	if ( !PopulateVariantMapFromAd ( ad, job ) )
        {
            text = "Error retrieving attributes for TransferringOutput job";
            return STATUS_USER + 1;
        }

        jobs.push_back(job);
    }

    //7) Suspended
    for ( SubmissionObject::JobSet::const_iterator i = GetSuspended().begin();
            GetSuspended().end() != i; i++ )
    {
	    (*i)->GetSummary(ad);
	if ( !PopulateVariantMapFromAd ( ad, job ) )
        {
            text = "Error retrieving attributes for Suspended job";
            return STATUS_USER + 1;
        }

        jobs.push_back(job);
    }

    return STATUS_OK;
}

Manageable::status_t
SubmissionObject::ManagementMethod ( uint32_t methodId,
                                     Args &args,
                                     std::string &text )
{
    switch ( methodId )
    {
	case qmf::com::redhat::grid::Submission::METHOD_ECHO:
		if (!param_boolean("QMF_MANAGEMENT_METHOD_ECHO", false)) return STATUS_NOT_IMPLEMENTED;
            return STATUS_OK;
        case qmf::com::redhat::grid::Submission::METHOD_GETJOBSUMMARIES:
            return GetJobSummaries ( ( ( ArgsSubmissionGetJobSummaries & ) args ).o_Jobs,
                             text );
    }

    return STATUS_NOT_IMPLEMENTED;
}

bool
SubmissionObject::AuthorizeMethod(uint32_t methodId, Args& args, const std::string& userId) {
	dprintf(D_FULLDEBUG, "AuthorizeMethod: checking '%s'\n", userId.c_str());
	if (0 == userId.compare("cumin")) {
		return true;
	}
	return false;
}
