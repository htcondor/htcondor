#include "startd.h"
static char *_FileName_ = __FILE__;


ResMgr::ResMgr()
{
	int i;

	coll_sock = new SafeSock( collector_host, 
							  COLLECTOR_UDP_COMM_PORT );

	if( condor_view_host ) {
		view_sock = new SafeSock( condor_view_host,
								  COLLECTOR_UDP_COMM_PORT ); 
	} else {
		view_sock = NULL;
	}

		// This should really handle multiple resources here.
	nresources = 1;
	resources = new Resource*[nresources];

	for( i = 0; i < nresources; i++ ) {
		resources[i] = new Resource( coll_sock, view_sock );
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
