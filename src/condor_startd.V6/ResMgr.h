/*
    This file defines the ResMgr class, the "resource manager".  This
	object contains an array of pointers to Resource objects, one for
	each resource defined the config file.  

	Written on 10/9/97 by Derek Wright <wright@cs.wisc.edu>
*/
#ifndef _RESMGR_H
#define _RESMGR_H

typedef int (Resource::*ResourceMember)();

class ResMgr
{
public:
	ResMgr();
	~ResMgr();

	bool 	in_use();
	
	// These two functions walk through the array of rip pointers and
	// call the specified function on each resource.  The first takes
	// functions that take a rip as an arg.  The second takes Resource
	// member functions that take no args.
	int		walk( int(*)(Resource*) );
	int		walk( ResourceMember );

	// Hack function
	Resource*	rip() {return resources[0];};

	Resource*	get_by_pid(int);		// Find rip by pid of starter
	Resource*	get_by_cur_cap(char*);	// Find rip by r_cur->capab
	Resource*	get_by_any_cap(char*);	// Find rip by r_cur or r_pre
	State	state();					// Return the machine state

private:
	Resource**	resources;		// Array of pointers to resource objects
	int			nresources;		// Size of the array
	SafeSock*	coll_sock;
	SafeSock*	view_sock;

};

#endif _RESMGR_H
