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
	void	final_update();
	
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
