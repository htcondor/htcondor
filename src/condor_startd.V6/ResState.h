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
#ifndef _RES_STATE_H
#define _RES_STATE_H

class LoadQueue
{
public:
	LoadQueue( int q_size );
	~LoadQueue();
	void	push( int num, char val );
	void	setval( char val );
	float	avg();
	int		val( int num );
private:
	int			head;	// Index of the next available slot.
	char*	 	buf;	// Actual array to hold values.
	int			size;	
};


class ResState
{
public:
	ResState(Resource* rip);
	~ResState();
	State	state() {return r_state;};
	Activity activity() {return r_act;};
	void	update( ClassAd* );
	int		change( Activity );
	int		change( State );
	int		change( State, Activity );
	int 	eval();
	float	condor_load();
private:
	Resource*	rip;
	State 		r_state;
	Activity	r_act;		

	int		stime;		// time we entered the current state
	int		atime;		// time we entered the current activitiy
	LoadQueue*	r_load_q;	// load average queue
		// Function called on every activity change which update the load_q
	void	load_activity_change();  

	int		enter_action( State, Activity, int, int );
	int		leave_action( State, Activity, int, int );
};

#endif _RES_STATE_H

