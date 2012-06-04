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
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "proc.h"

#include "stringSpace.h"

#include "Globals.h"
// from src/management
#include "Utils.h"

#include <sstream>

// Any key that begins with the '0' char is either the
// header or a cluster, i.e. not a job
#define IS_JOB(key) ((key) && '0' != (key)[0])

// summary attributes
 const char *ATTRS[] = {ATTR_CLUSTER_ID,
                           ATTR_PROC_ID,
                           ATTR_GLOBAL_JOB_ID,
                           ATTR_Q_DATE,
                           ATTR_ENTERED_CURRENT_STATUS,
                           ATTR_JOB_STATUS,
                           ATTR_JOB_CMD,
                           ATTR_JOB_ARGUMENTS1,
                           ATTR_JOB_ARGUMENTS2,
                           ATTR_RELEASE_REASON,
                           ATTR_HOLD_REASON,
                           NULL
                          };

// TODO: C++ utils which may very well exist elsewhere :-)
template <class T>
bool from_string ( T& t,
                   const std::string& s,
                   std::ios_base& ( *f ) ( std::ios_base& ) )
{
    std::istringstream iss ( s );
    return ! ( iss >> f >> t ).fail();
}

template <class T>
std::string to_string ( T t, std::ios_base & ( *f ) ( std::ios_base& ) )
{
    std::ostringstream oss;
    oss << f << t;
    return oss.str();
}

extern ManagementAgent::Singleton *singleton; // XXX: get rid of this
extern com::redhat::grid::JobServerObject* job_server;

////////////
// Attribute
////////////
Attribute::Attribute ( AttributeType _type, const char *_value ) :
        m_type ( _type ),
        m_value ( _value )
{ }

Attribute::~Attribute()
{
    //
}

Attribute::AttributeType
Attribute::GetType() const
{
    return m_type;
}

const char *
Attribute::GetValue() const
{
    return m_value.c_str();
}

//////////////
// LiveJobImpl
//////////////
LiveJobImpl::LiveJobImpl (const char* cluster_proc, ClusterJobImpl* parent)
{
    m_job = NULL;
    m_parent = NULL;
    m_summary_ad = NULL;
    m_full_ad = new ClassAd();
    if ( parent)
    {
        m_parent = parent;
        m_parent->IncrementRef();
        m_full_ad->ChainToAd ( parent->m_full_ad );
    }

    dprintf ( D_FULLDEBUG, "LiveJobImpl created for '%s'\n", cluster_proc);
}

LiveJobImpl::~LiveJobImpl()
{
    // TODO: do we have to unchain our parent?
    if (m_full_ad) {
        m_full_ad->Unchain();
        delete m_full_ad;
        m_full_ad = NULL;
    }

    if (m_summary_ad) {
        delete m_summary_ad;
        m_summary_ad = NULL;
    }

    if (m_parent) {
        m_parent->DecrementRef();
    }

    dprintf ( D_FULLDEBUG, "LiveJobImpl destroyed: key '%s'\n", m_job->GetKey());
}

// TODO: this code doesn't work as expected
// everything seems to get set to EXPR_TYPE
bool
LiveJobImpl::Get ( const char *_name, const Attribute *&_attribute ) const
{
    // our job ad is chained so lookups will
    // encompass our parent ad as well as the child

    // parse the type
    ExprTree *expr = NULL;
    if ( ! ( expr = m_full_ad->Lookup ( _name ) ) )
    {
	    dprintf ( D_FULLDEBUG,
                  "warning: failed to lookup attribute %s in job '%s'\n", _name, m_job->GetKey() );
        return false;
    }
    // decode the type
    classad::Value value;
    m_full_ad->EvaluateExpr(expr,value);
    switch ( value.GetType() )
    {
        case classad::Value::INTEGER_VALUE:
        {
            int i;
            if ( !m_full_ad->LookupInteger ( _name, i ) )
            {
                return false;
            }
            const char* int_str = to_string<int> ( i,dec ).c_str();
            _attribute = new Attribute ( Attribute::INTEGER_TYPE, int_str );
            return true;
        }
        case classad::Value::REAL_VALUE:
        {
            float f;
            if ( !m_full_ad->LookupFloat ( _name, f ) )
            {
                return false;
            }
            const char* float_str = to_string<float> ( f,dec ).c_str();
            _attribute = new Attribute ( Attribute::FLOAT_TYPE, float_str );
            return true;
        }
        case classad::Value::STRING_VALUE:
        {
            std::string str;
            if ( !m_full_ad->LookupString ( _name, str ) )
            {
                return false;
            }
            _attribute = new Attribute ( Attribute::STRING_TYPE, str.c_str() );
            return true;
        }
        default:
        {
            ExprTree* tree = NULL;
            if ( ! ( tree = m_full_ad->Lookup ( _name ) ) )
            {
                return false;
            }
            const char* rhs;
            rhs = ExprTreeToString( expr );
            _attribute = new Attribute ( Attribute::EXPR_TYPE, rhs );
            return true;
        }
    }

    return false;
}

int LiveJobImpl::GetStatus() const
{
    const Attribute* attr = NULL;

    if ( !this->Get ( ATTR_JOB_STATUS, attr ) )
    {
	// TODO: assume we might get cluster jobs here also
	return JOB_STATUS_MIN;
    }

    int _status = strtol ( attr->GetValue(), ( char ** ) NULL, 10 );
    delete attr;

    return _status;
}

int LiveJobImpl::GetQDate() const
{
	const Attribute* attr = NULL;

	if ( !this->Get ( ATTR_Q_DATE, attr ) )
	{
		// default to current
		return time(NULL);
	}

	return strtol ( attr->GetValue(), ( char ** ) NULL, 10 );
}

void
LiveJobImpl::Set ( const char *_name, const char *_value )
{

    if ( strcasecmp ( _name, ATTR_JOB_SUBMISSION ) == 0 )
    {
        std::string val = TrimQuotes( _value );
        // TODO: grab the cluster from our key
        PROC_ID id = getProcByString(m_job->GetKey());
	if (m_job) {
		m_job->SetSubmission ( val.c_str(), id.cluster );
	}
    }

    // our status is changing...decrement for old one
    if ( strcasecmp ( _name, ATTR_JOB_STATUS ) == 0 )
    {
	if ( m_job )
        {
	    m_job->SetStatus(this->GetStatus());
            m_job->DecrementSubmission ();
        }
    }

    if ( strcasecmp ( _name, ATTR_OWNER ) == 0 )
    {
	// need to leave an owner for this job
	// to be picked up soon
	// if we are in here, we don't have m_submission
	PROC_ID id = getProcByString(m_job->GetKey());
	std::string val = TrimQuotes( _value );
	g_ownerless_clusters[id.cluster] = val;
	m_job->UpdateSubmission(id.cluster,val.c_str());
    }

    // parse the type
    ExprTree *expr;
    if ( ParseClassAdRvalExpr ( _value, expr ) )
    {
        dprintf ( D_ALWAYS,
                  "error: parsing %s[%s] = %s, skipping\n",
                  m_job->GetKey(), _name, _value );
        return;
    }
    // add this value to the classad
    classad::Value value;
    expr->Evaluate(value);
    switch ( value.GetType() )
    {
        case classad::Value::INTEGER_VALUE:
            int i;
            from_string<int> ( i, std::string ( _value ), std::dec );
            m_full_ad->Assign ( _name, i );
            break;
        case classad::Value::REAL_VALUE:
            float f;
            from_string<float> ( f, std::string ( _value ), std::dec );
            m_full_ad->Assign ( _name, f );
            break;
        case classad::Value::STRING_VALUE:
            m_full_ad->Assign ( _name, _value );
            break;
        default:
            m_full_ad->AssignExpr ( _name, _value );
            break;
    }
    delete expr; expr = NULL;

    // our status has changed...increment for new one
    if ( strcasecmp ( _name, ATTR_JOB_STATUS ) == 0 )
    {
        if ( m_job )
        {
	    m_job->SetStatus(this->GetStatus());
            m_job->IncrementSubmission ();
        }
    }
}

void
LiveJobImpl::Remove ( const char *_name )
{
	// TODO: seems we implode if we don't unchain first
	classad::ClassAd* cp = m_full_ad->GetChainedParentAd();
	m_full_ad->Delete ( _name );
	m_full_ad->ChainToAd(cp);
}

const ClassAd* LiveJobImpl::GetSummary ()
{
	if (!m_summary_ad) {
		m_summary_ad = new ClassAd();
		m_summary_ad->ResetExpr();
		int i = 0;
		while (NULL != ATTRS[i]) {
			const Attribute* attr = NULL;
			if (this->Get(ATTRS[i],attr)) {
				switch (attr->GetType()) {
					case Attribute::FLOAT_TYPE:
						m_summary_ad->Assign(ATTRS[i], atof(attr->GetValue()));
						break;
					case Attribute::INTEGER_TYPE:
						m_summary_ad->Assign(ATTRS[i], atol(attr->GetValue()));
						break;
					case Attribute::EXPR_TYPE:
					case Attribute::STRING_TYPE:
					default:
						m_summary_ad->Assign(ATTRS[i], attr->GetValue());
				}
			}
		delete attr;
		i++;
		}
	}

    // make sure we're up-to-date with status even if we've cached the summary
	m_summary_ad->Assign(ATTR_JOB_STATUS,this->GetStatus());
    int i;
    if ( m_full_ad->LookupInteger ( ATTR_ENTERED_CURRENT_STATUS, i ) ) {
        m_summary_ad->Assign(ATTR_ENTERED_CURRENT_STATUS,i);
    }
    else {
        dprintf(D_ALWAYS,"Unable to get ATTR_ENTERED_CURRENT_STATUS\n");
    }

	return m_summary_ad;
}

const ClassAd* LiveJobImpl::GetFullAd () const
{
    return m_full_ad;
}

/////////////////
// ClusterJobImpl
/////////////////
ClusterJobImpl::ClusterJobImpl(const char* key): LiveJobImpl(key, NULL)
{
        m_ref_count = 0;
}

ClusterJobImpl::~ClusterJobImpl()
{
        dprintf ( D_FULLDEBUG, "ClusterJobImpl destroyed: key '%s'\n", m_job->GetKey());
}

void
ClusterJobImpl::IncrementRef()
{
        m_ref_count++;
}

void
ClusterJobImpl::DecrementRef()
{
        --m_ref_count;
}

/////////////////
// HistoryJobImpl
/////////////////
HistoryJobImpl::HistoryJobImpl ( const HistoryEntry& _he):
	m_he(_he)
{
    m_job = NULL;
    g_ownerless_clusters[_he.cluster] = _he.owner;
    dprintf ( D_FULLDEBUG, "HistoryJobImpl created for '%d.%d'\n", _he.cluster, _he.proc );
}

HistoryJobImpl::~HistoryJobImpl ()
{
	dprintf ( D_FULLDEBUG, "HistoryJobImpl destroyed: key '%s'\n", m_job->GetKey());
}

int HistoryJobImpl::GetStatus() const
{
    return m_he.status;
}

int HistoryJobImpl::GetCluster() const
{
	return m_he.cluster;
}

int HistoryJobImpl::GetQDate() const
{
	return m_he.q_date;
}

const char* HistoryJobImpl::GetSubmissionId() const
{
	return m_he.submission.c_str();
}

void HistoryJobImpl::GetSummary ( ClassAd& _ad ) const
{
	_ad.ResetExpr();
	// use HistoryEntry data only
	_ad.Assign(ATTR_GLOBAL_JOB_ID,m_he.id.c_str());
	_ad.Assign(ATTR_CLUSTER_ID,m_he.cluster);
	_ad.Assign(ATTR_PROC_ID,m_he.proc);
	_ad.Assign(ATTR_Q_DATE,m_he.q_date);
	_ad.Assign(ATTR_JOB_STATUS,m_he.status);
	_ad.Assign(ATTR_ENTERED_CURRENT_STATUS,m_he.entered_status);
	_ad.Assign(ATTR_JOB_SUBMISSION,m_he.submission.c_str());
	_ad.Assign(ATTR_OWNER,m_he.owner.c_str());
	_ad.Assign(ATTR_JOB_CMD,m_he.cmd.c_str());

	// beyond here these may be empty so don't
	// automatically add to summary
	if (!m_he.args1.empty()) {
		_ad.Assign(ATTR_JOB_ARGUMENTS1,m_he.args1.c_str());
	}
	if (!m_he.args2.empty()) {
		_ad.Assign(ATTR_JOB_ARGUMENTS2,m_he.args2.c_str());
	}
	if (!m_he.release_reason.empty()) {
		_ad.Assign(ATTR_RELEASE_REASON,m_he.release_reason.c_str());
	}
	if (!m_he.hold_reason.empty()) {
		_ad.Assign(ATTR_HOLD_REASON,m_he.hold_reason.c_str());
	}

}

// specialization: this GetFullAd has to retrieve its classad attributes
// from the history file based on index pointers
void
 HistoryJobImpl::GetFullAd ( ClassAd& _ad) const
{
    // fseek to he.start
    // ClassAd method to deserialize from a file with "***"

    FILE * hFile;
    int end = 0;
    int error = 0;
    int empty = 0;
	std::ostringstream buf;
	// placeholder in case we want to expose error details to UI
	std::string text;

    // TODO: move the ClassAd/file deserialize back to HistoryFile???
    const char* fName = m_he.file.c_str();
    if ( ! ( hFile = safe_fopen_wrapper ( fName, "r" ) ) )
    {
		buf <<  "unable to open history file " << m_he.file;
		text = buf.str();
        dprintf ( D_ALWAYS, "%s\n", text.c_str());
	_ad.Assign("JOB_AD_ERROR",text.c_str());
	return;
    }
    if ( fseek ( hFile , m_he.start , SEEK_SET ) )
    {
		buf << "bad seek in " << m_he.file << " at " << m_he.start;
		text = buf.str();
        dprintf ( D_ALWAYS, "%s\n", text.c_str());
	_ad.Assign("JOB_AD_ERROR",text.c_str());
        return;
    }

    ClassAd myJobAd ( hFile, "***", end, error, empty );
    fclose ( hFile );

	// TODO: debug logging and error to i/f for now
	// we might not have our original history file anymore
    if ( error )
    {
		buf <<  "malformed ad for job '" << m_job->GetKey() << "' in " << m_he.file;
		text = buf.str();
        dprintf ( D_FULLDEBUG, "%s\n", text.c_str());
	_ad.Assign("JOB_AD_ERROR",text.c_str());
		return;
    }
    if ( empty )
    {
		buf << "empty ad for job '" << m_job->GetKey() << "' in " << m_he.file;
		text = buf.str();
        dprintf ( D_FULLDEBUG,"%s\n", text.c_str());
	_ad.Assign("JOB_AD_ERROR",text.c_str());
		return;
    }

	_ad = myJobAd;

}

//////
// Job
//////
Job::Job(const char* _key):  m_submission(NULL), m_live_job(NULL), m_history_job(NULL), m_key(_key) 
{
        dprintf (D_FULLDEBUG,"Job::Job of '%s'\n", m_key);
}

Job::~Job() {
	// TODO: Destroy will be safe way to
	// cleanup

	dprintf (D_FULLDEBUG,"Job::~Job of '%s'\n", m_key);

    this->DecrementSubmission();

	delete m_live_job;
	delete m_history_job;

	delete [] m_key;
	// submissions are shared and can't be deleted here
}

void Job::SetImpl (LiveJobImpl* lji)
{
	lji->SetJob(this);
	// probably shouldn't happen
	if (m_live_job) {
		delete m_live_job;
	}
	m_live_job = lji;

	// status of a live job always has precedence
	// so decrement if the history job got in ahead of it
	if (m_history_job) {
		m_status = m_history_job->GetStatus();
		m_submission->Decrement(this);
	}
	m_status = m_live_job->GetStatus();
}

void Job::SetImpl (HistoryJobImpl* hji)
{
	hji->SetJob(this);
	// probably shouldn't happen
	if (m_history_job) {
		delete m_history_job;
	}
	m_history_job = hji;

	// stay away from extra inc/decs if the live job is still doing its thing
	if (!m_submission) {
		m_status = m_history_job->GetStatus();
		SetSubmission(m_history_job->GetSubmissionId(),m_history_job->GetCluster());
	}

	// call Destroy to see if we can clean up the live job
	this->Destroy();
}

void Job::SetStatus(int status)
{
	m_status = status;
}

const char* Job::GetKey() const
{
	return m_key;
}

void Job::Set ( const char *_name, const char *_value ) {
	if (m_live_job) {
		m_live_job->Set(_name,_value);
	}
	if (m_submission) {
        m_submission->UpdateQdate(this->GetQDate());
    }
}

void Job::Remove ( const char *_name ) {
	if (m_live_job) {
		m_live_job->Remove(_name);
	}
	// ignore for history jobs
}

void Job::IncrementSubmission() {
	if (m_submission) {
		m_submission->Increment(this);
	}
}

void Job::DecrementSubmission() {
	if (m_submission) {
		m_submission->Decrement(this);
	}
}

void
Job::UpdateSubmission ( int cluster, const char* owner )
{
	OwnerlessSubmissionType::const_iterator it = g_ownerless_submissions.find ( cluster );
	if ( g_ownerless_submissions.end() != it ) {
		SubmissionObject* submission = (*it).second;
		submission->SetOwner(owner);
		g_ownerless_submissions.erase(cluster);
	}
}

void
Job::SetSubmission ( const char* _subName, int cluster )
{
	const char* owner = NULL;

	// need to see if someone has left us an owner
	OwnerlessClusterType::const_iterator it = g_ownerless_clusters.find ( cluster );
	if ( g_ownerless_clusters.end() != it )
	{
		owner = ( *it ).second.c_str() ;
	}

	SubmissionCollectionType::const_iterator element = g_submissions.find ( _subName );
	SubmissionObject *submission;
	if ( g_submissions.end() == element )
	{
		submission = new SubmissionObject ( singleton->getInstance(), job_server, _subName, owner );
		g_submissions[strdup ( _subName ) ] = submission;
	}
	else
	{
		submission = ( *element ).second;
	}
	m_submission = submission;

	m_submission->Increment(this);

	if (owner) {
		// ensure that the submission has an owner
		m_submission->SetOwner ( owner );
		g_ownerless_clusters.erase ( cluster );
	}
	else {
		// add it to our list to be updated for owner
		g_ownerless_submissions[cluster] = m_submission;
	}

	// finally update the overall submission qdate
	m_submission->UpdateQdate(this->GetQDate());

}

bool ClusterJobImpl::Destroy()
{
        return m_ref_count == 0;
}

bool
LiveJobImpl::Destroy()
{
        return ((this->GetStatus() == COMPLETED) || (this->GetStatus() == REMOVED));
}

bool
Job::Destroy()
{

        bool live_destroy = (m_live_job && m_live_job->Destroy());

        if (m_history_job && live_destroy) {
                // must be a proc
                delete m_live_job;
                m_live_job = NULL;
                return false;
        }

        // hint to caller
        return live_destroy;
}

// space v. speed tradeoff here - we want to keep the size of the job server down
// since we may have to support millions of jobs in memory
void Job::GetFullAd ( ClassAd& _ad) const
{
	// history ads are always reconstructed
	// so we deep copy the live job ad to be consistent
	// on the interface

	if (m_live_job) {
		_ad = *(m_live_job->GetFullAd());
	}
	else {
		m_history_job->GetFullAd(_ad);
	}

}

// poke a hole for live job ad chaining
const ClassAd* Job::GetClassAd() const
{
	const ClassAd* ad_ptr = NULL;
	if (m_live_job) {
		ad_ptr = m_live_job->GetFullAd();
	}
	return ad_ptr;

}

void Job::GetSummary ( ClassAd& _ad) const
{
	//same thing as full ad
	if (m_live_job) {
		_ad.CopyFrom(*m_live_job->GetSummary());
	}
	else {
		m_history_job->GetSummary(_ad);
	}
}

int Job::GetStatus() const
{
	return m_status;
}

int Job::GetQDate() const
{
	if (m_live_job) {
		return m_live_job->GetQDate();
	}
	else {
		return m_history_job->GetQDate();
	}
}

JobImpl* Job::GetImpl() {
        // only will provide live job impl
        // if it's NULL the caller needs to deal
        return m_live_job;
}
