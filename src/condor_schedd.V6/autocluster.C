/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "autocluster.h"
#include "condor_config.h"
#include "condor_attributes.h"

AutoCluster::AutoCluster()
{
	array.fill(NULL);
	significant_attrs = NULL;
	old_sig_attrs = NULL;
}	

AutoCluster::~AutoCluster()
{
	clearArray();
	if ( old_sig_attrs ) free(old_sig_attrs);
	if ( significant_attrs ) delete significant_attrs;
}

void AutoCluster::clearArray()
{
	int i;
	int size = array.getlast() + 1;
	for (i=0; i < size ; i++) {
		if ( array[i] ) {
			delete array[i];
			array[i] = NULL;
		}
	}
	array.truncate(-1);
}
	
bool AutoCluster::config()
{

	char *new_sig_attrs =  param ("SIGNIFICANT_ATTRIBUTES");

	if ( new_sig_attrs && old_sig_attrs && 
		 (stricmp(new_sig_attrs,old_sig_attrs)==0) ) 
	{
		// nothing changed that we care about, so we want
		// to keep all of our state (i.e. our array)
		free(new_sig_attrs);
		return false;
	}

		// the SIGNIFICANT_ATTRIBUTES setting changed, purge our
		// state.
	clearArray();

		// update our StringList
	if ( significant_attrs ) {
		delete significant_attrs;
		significant_attrs = NULL;
	}
	if ( new_sig_attrs ) {
		significant_attrs = new StringList(new_sig_attrs);
	}

		// update old_sig_attrs
	if ( old_sig_attrs ) {
		free(old_sig_attrs);
		old_sig_attrs = NULL;
	}
	old_sig_attrs = new_sig_attrs;

	return true;
}

void AutoCluster::mark()
{
	int i;
	int size = array.getlast() + 1;
	for (i=0; i < size ; i++) {
		mark_array[i] = true;
	}
	mark_array.truncate(size - 1);
}

void AutoCluster::sweep()
{
	int i;
	int size = mark_array.getlast() + 1;
		// now remove any entries still marked
	for (i=0; i < size ; i++) {
		if ( mark_array[i] ) {
				// found an entry to remove.
			mark_array[i] = false;
			dprintf(D_FULLDEBUG,"removing auto cluster id %d\n",i);
			if ( array[i] ) {
				delete array[i];
				array[i] = NULL;
			}
		}
	}
}

int AutoCluster::getAutoClusterid( ClassAd *job )
{
	int cur_id = -1;
	int i;

		// first check if condor_config file even desires this
		// functionality...
	if ( !significant_attrs ) {
		return -1;
	}

    job->LookupInteger(ATTR_AUTO_CLUSTER_ID, cur_id);
	if ( cur_id != -1 ) {
			// we've previously figured it out...
		
			// tag it as touched
		mark_array[cur_id] = false;

		ASSERT( array[cur_id] != NULL );
		return cur_id;
	}

		// summarize job into a string "signature"
	MyString signature;
	char buf[ATTRLIST_MAX_EXPRESSION];
	significant_attrs->rewind();
	const char* next_attr = NULL;
	ExprTree *tree = NULL;
	string bufS;
	ClassAdUnParser unp;
	while ( (next_attr=significant_attrs->next()) != NULL ) {
		buf[0] = '\0';
//		job->sPrintExpr(buf,sizeof(buf),next_attr);
		tree = job->Lookup( next_attr );
		if( tree ) {
			unp.Unparse( bufS, tree );
			strcpy( buf, bufS.c_str( ) );
		}			
		signature += buf;
	}

		// try to find a fit
	int size = array.getlast() + 1;
	for (i=0; i < size ; i++) {
		if ( array[i] == NULL ) {
			continue;
		}
		if ( signature == *(array[i]) ) {
				// found a match... update job ad
			cur_id = i;
		}
	}

	if ( cur_id == -1 ) {
			// failed to find a fit; need to create a new "cluster"
		for (i=0; array[i]; i++);	// set i to first NULL entry
		cur_id = i;
		array[cur_id] = new MyString(signature);
	}

	sprintf(buf,"%s=%d",ATTR_AUTO_CLUSTER_ID,cur_id);
	job->Insert(buf);

		// tag it as touched
	mark_array[cur_id] = false;

	return cur_id;
}
