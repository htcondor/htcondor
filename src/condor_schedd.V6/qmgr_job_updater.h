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


#ifndef CONDOR_QMGR_JOB_UPDATER_H
#define CONDOR_QMGR_JOB_UPDATER_H

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "condor_daemon_core.h"
#include "condor_qmgr.h"

#include "userrec.h"

// What kind of update to the job queue are we performing?
typedef enum { 
	U_NONE = 0,
	U_PERIODIC,
	U_TERMINATE,
	U_HOLD,
	U_REMOVE,
	U_REQUEUE,
	U_EVICT,
	U_CHECKPOINT,
	U_X509,
	U_STATUS
} update_t;


class QmgrJobUpdater : public Service
{
public:
	QmgrJobUpdater( ClassAd* job_a, const char*schedd_address );
	QmgrJobUpdater( ) :  job_ad(0), cluster(-1), proc(-1), q_update_tid(-1) {}
	virtual ~QmgrJobUpdater();

	virtual void startUpdateTimer( void );

		/** Reset the timer for periodic updates to the schedd to fire
			immediately.
		 */
	virtual void resetUpdateTimer( void );

		/** Connect to the job queue and update all relevent
			attributes of the job class ad.  This checks our job
			classad to find any dirty attributes, and compares them
			against the lists of attribute names we care about.  
			@param type What kind of update we want to do
			@param commit_flags flags to pass to RemoteCommitTransaction()
			@return true on success, false on failure
		*/
	virtual bool updateJob( update_t type, SetAttributeFlags_t commit_flags = 0 );

		/** Connect to the job queue and update one attribute.
			WARNING: This method is BAD NEWS for schedd scalability.
			We make a whole new qmgmt connection for *every* attribute
			we update.  Worse yet, we use this method to service a
			pseudo syscall from the user job (pseudo_set_job_attr), so
			the schedd can be held hostage by user-jobs that call this
			syscall repeatedly.  :(
		*/
	virtual bool updateAttr( const char *name, const char *expr, bool updateMaster, bool log=false );

		/// Helper version that takes an int value instead of a string expr.
	virtual bool updateAttr( const char *name, int value, bool updateMaster, bool log=false );

		/** Add the given attribute to our list of attributes we
			should watch for changes and update.  The type specifies
			if it's a special attribute that should only be updated on
			a given kind of event.  If not specified, the type
			defaults to 0, which means the given attribute should be
			added to the list of common attributes that should always
			be updated.
			@param attr Attribute we want to add
			@param type What kind of updates does this attribute
			   matter for?
		    @return true if added, false if it was already there
		*/
	virtual bool watchAttribute( const char* attr, update_t type = U_NONE );

		/** Connect to the job queue and retrieve changed attributes
		*/
	bool retrieveJobUpdates( void );

private:

		/** Initialize our lists for attributes we want to keep
			updated in the job queue itself
		*/ 
	void initJobQueueAttrLists( void );

		/** Timer handler which just calls updateJobInQueue with the
			right arguments so we do a periodic update.
		*/
	void periodicUpdateQ( int timerID = -1 );


		/** Update a specific attribute from our job ad into the
			queue.  This checks the type of the given ExprTree and
			calls the appropriate SetAttribute* function to do the
			work.  This function assumes you're in the middle of a
			qmgmt operation, i.e., that you've called ConnectQ().
			@param The ExprTree you want to update in the job queue 
			@return success or failure to set the attribute
		 */
	bool updateExprTree( const char *name, ExprTree* tree ) const;

		/// Pointers to lists of attribute names we care about

		/** Attributes that should go in the job queue regardless of
			what action we're taking with this job. */
	classad::References common_job_queue_attrs;
		/// Attributes specific to certain kinds of updates
	classad::References hold_job_queue_attrs;
	classad::References evict_job_queue_attrs;
	classad::References remove_job_queue_attrs;
	classad::References requeue_job_queue_attrs;
	classad::References terminate_job_queue_attrs;
	classad::References checkpoint_job_queue_attrs;
	classad::References x509_job_queue_attrs;

		// List of attributes we should pull from the schedd when we
		// do an update.
	classad::References m_pull_attrs;

	ClassAd* job_ad;
	DCSchedd m_schedd_obj;
	std::string m_owner;

	int cluster;
	int proc;

	int q_update_tid;
};	

// usefull if you don't want to update the job queue
class NullQmgrJobUpdater : public QmgrJobUpdater
{
public:
	NullQmgrJobUpdater( ClassAd* , const char* ) : QmgrJobUpdater() {}
	virtual ~NullQmgrJobUpdater() {}

	virtual void startUpdateTimer( void ) {return;}
	virtual bool updateJob( update_t /*type*/, SetAttributeFlags_t  /*commit_flags*/ = 0 ) { return true;}
	virtual bool updateAttr( const char *  /*name*/, const char * /*expr*/, bool  /*updateMaster*/, bool log=false ) { if (log) {} return true;}
	virtual bool updateAttr( const char * /*name*/, int  /*value*/, bool  /*updateMaster*/, bool log=false ) { if (log) {} return true;}
	virtual bool watchAttribute( const char*  /*attr*/, update_t /*type*/ = U_NONE ) { return true;}
};

#endif /* CONDOR_QMGR_JOB_UPDATER_H */
