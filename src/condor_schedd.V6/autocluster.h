/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _autocluster_H_
#define _autocluster_H_

#include "MyString.h"
#include "extArray.h"
#include "condor_classad.h"
#include "string_list.h"
#include "HashTable.h"

/** This class manages the computation auto cluster ids for jobs based
	upon a list of significant attributes.  Any jobs with an identical 
	auto cluster id can be considered identical for purposes of matchmaking.
	The significant attributes list is a list of all job classad attributes
	that will be examined during negotiation/matchmaking -- this in effect
	means it is a union of all job attributes referenced by any startd
	requirements or rank expression, plus any job attributes referenced by
	the negotiation such as the PREEMPTION_REQUIREMENTS expression, plus 
	any attributes referenced by the job ad's own Requirements / Rank 
	expressions.  In the schedd, the assumption is this class will be
	configured with the significant attributes passed to the schedd by
	the negotiator.  
*/
class AutoCluster {

public:

	/// Ctor
	AutoCluster();

	/// Dtor
	~AutoCluster();

	/** Configure the class; must be called before any other methods
		are invoked.  May be called as often as desired.
		@param significant_target_attrs A string of attributes delimited 
		by commas.  These attributes will be added to a list of attributes
		considered to be significant by the class.  This parameter is 
		ignored if the config file explicitly specifies a list of 
		significant attributes.
		@return True if the list significant attributes has changed, and
		thus all previously returned auto cluster ids are invalid and need
		to be recomputed; false if the list of significant attributes has
		not changed, and thus previous returned auto cluster ids are still
		valid.   In practice in the schedd, if config() returns True, the
		ATTR_AUTO_CLUSTER_ID attribute should be cleared from all job ads.
	*/
	bool config(const char* significant_target_attrs = NULL);	
	
	/** Given a job classad, return the value of the attribute 
		ATTR_AUTO_CLUSTER_ID, or if this attribute is not present, 
		compute the autocluster id and store it in the ad.  Also,
		the deletion flag for this autocluster entry will be cleared.
		@param job A job classad
		@return The autocluster id for this job, or -1 if it cannot
		be computed.
	*/
	int getAutoClusterid( ClassAd *job );

		// garbage collection methods...

	/** Set the deletion flag for all autocluster id entries in the class.  
		After calling mark(), calls to getAutoClusterid() will clear the deletion
		flag for the autocluster returned.  The idea here is to call mark(),
		then call getAutoClusterid() on all job classads still active, then
		call sweep() to remove data structures assicoated with autoclusters
		no longer being used.
		@see sweep
	*/
	void mark();

	/** Deallocate and purge from memory the data structures associated with
		all autoclusters that have their corresponding deletion flag set.
		@see mark
	*/
	void sweep();

protected:

	void clearArray();
	ExtArray<MyString*>   array;
	ExtArray<bool> mark_array;

	StringList* significant_attrs;
	char *old_sig_attrs;
	bool sig_attrs_came_from_config_file;

};

#endif

