/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "startd.h"
static char *_FileName_ = __FILE__;


ResMgr::ResMgr()
{
	int i;
	coll_sock = NULL;
	view_sock = NULL;

	this->init_socks();

		// This should really handle multiple resources here.
	nresources = 1;
	resources = new Resource*[nresources];

	for( i = 0; i < nresources; i++ ) {
		resources[i] = new Resource();
	}
}


void
ResMgr::init_socks()
{

	if( coll_sock ) {
		delete coll_sock;
	}
	coll_sock = new SafeSock( collector_host, 
							  COLLECTOR_UDP_COMM_PORT );

	if( view_sock ) {
		delete view_sock;
	}
	if( condor_view_host ) {
		view_sock = new SafeSock( condor_view_host, 
								  CONDOR_VIEW_PORT );
	}
}


ResMgr::~ResMgr()
{
	delete coll_sock;
	if( view_sock ) {
		delete view_sock;
	}
	delete [] resources;
}


int
ResMgr::walk( int(*func)(Resource*) )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		func(resources[i]);
	}
	return TRUE;
}


int
ResMgr::walk( ResourceMember memberfunc )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))();
	}
	return TRUE;
}


bool
ResMgr::in_use( void )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->in_use() ) {
			return true;
		}
	}
	return false;
}


Resource*
ResMgr::get_by_pid( int pid )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_starter->pid() == pid ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_cur_cap( char* cap )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->cap()->matches(cap) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_any_cap( char* cap )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->cap()->matches(cap) ) {
			return resources[i];
		}
		if( (resources[i]->r_pre) &&
			(resources[i]->r_pre->cap()->matches(cap)) ) {
			return resources[i];
		}
	}
	return NULL;
}


State
ResMgr::state()
{
		// This needs serious help when we get to multiple resources
	return resources[0]->state();
}


void
ResMgr::final_update()
{
		// This needs serious help when we get to multiple resources
	resources[0]->final_update();
}



void
ResMgr::send_update( ClassAd* public_ad, ClassAd* private_ad )
{
	if( coll_sock ) {
		if( send_classad_to_sock(coll_sock, public_ad, private_ad) ) {
			dprintf( D_FULLDEBUG, "Sent UDP update to the collector (%s)\n", 
					 collector_host );
		} else {
			dprintf( D_ALWAYS, "Error sending UDP update to the collector (%s)\n", 
					 collector_host );
		}
	}  

		// If we have an alternate collector, send public CA there.
	if( view_sock ) {
		if( send_classad_to_sock(view_sock, public_ad, NULL) ) {
			dprintf( D_FULLDEBUG, 
					 "Sent UDP update to the condor_view host (%s)\n",
					 condor_view_host );
		} else {
			dprintf( D_ALWAYS, 
					 "Error sending UDP update to the condor_view host (%s)\n",
					 condor_view_host );
		}
	}
}
