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
#ifndef _STARTD_RESOURCE_H
#define _STARTD_RESOURCE_H

class Resource : public Service
{
public:
	Resource( CpuAttributes*, int );
	~Resource();

		// Public methods that can be called from command handlers
	int		release_claim();	// Gracefully kill starter and release claim
	int		kill_claim();		// Quickly kill starter and release claim
	int		got_alive();			// You got a keep alive command
	int 	periodic_checkpoint();	// Do a periodic checkpoint
		// Multi-shadow wants to run more processes.  Send a SIGHUP to
		// the starter
	int		request_new_proc();
		// Gracefully kill starter but keep claim	
	int		deactivate_claim();	
		// Quickly kill starter but keep claim
	int		deactivate_claim_forcibly();
 
	int		hardkill_starter();	// Send DC_SIGHARDKILL to the starter
								// and start a timer to send SIGKILL
								// to the starter if it's not gone in
								// killing_timeout seconds.
	int		sigkill_starter();  // Send SIGKILL to starter + process group 
	
		// Resource state methods
	int		change_state( State );
	int		change_state( Activity );
	int		change_state( State, Activity );
	State		state()		{return r_state->state();};
	Activity	activity()	{return r_state->activity();};
	int		eval_state()	{return r_state->eval();};
	bool	in_use();

		// Methods for computing and publishing resource attributes 
	void	compute( amask_t mask);
	void	publish( ClassAd*, amask_t );

		// Load Average related methods
	float	condor_load() {return r_attr->condor_load();};
	float	compute_condor_load();
	void	resize_load_queue(); 
	float	owner_load() {return r_attr->owner_load();};
	void	set_owner_load( float val) {r_attr->set_owner_load(val);};

	void	display( amask_t m ) {r_attr->display(m);}

		// dprintf() functions add the CPU id to the header of each
		// message for SMP startds (single CPU machines get no special
		// header and it works just like regular dprintf())
	void	dprintf( int, char*, ... );
	void	dprintf_va( int, char*, va_list );

		// Helper function to log that we're ignoring a command that
		// came in while we were in an unexpected state. 
	void	log_ignore( int cmd, State s );

		// Called from the reaper to handle things for this rip
	void	starter_exited();	

		// Since the preempting state is so weird, and when we want to
		// leave it, we need to decide where we want to go, and we
		// have to do lots of funky twiddling with our match objects,
		// we put all the actions and logic in one place that gets
		// called whenever we're finally ready to leave the preempting
		// state. 
	void	leave_preempting_state();

		// Methods to initialize and refresh the resource classads.
	int		init_classad();		
	void	update_classad();	
	void	timeout_classad();	
	int		force_benchmark();

	int		give_classad( Stream* stream );	// Send this VM's classad
											// to the given Stream

	int		update();				// Update the central manager.
	int		eval_and_update();		// Evaluate state and update CM. 
	void	final_update();			// Send a final update to the CM
									// with Requirements = False.

		// Methods to control various timers 
	int		start_kill_timer();		// Timer for how long we're willing to 
	void	cancel_kill_timer();	//    be in preempting/killing state. 

 		// Helper functions to evaluate resource expressions
	int		wants_vacate();
	int		wants_suspend();
	int		wants_pckpt();
	int		eval_kill();
	int		eval_preempt();
	int		eval_suspend();
	int		eval_continue();

		// Data members
	ResState*		r_state;	// Startd state object, contains state and activity
	ClassAd*		r_classad;	// Resource classad (contains everything in config file)
	Starter*		r_starter;	// Starter object
	Match*			r_cur;		// Info about the current match
	Match*			r_pre;		// Info about the possibly preempting match
	Reqexp*			r_reqexp;   // Object for the requirements expression
	CpuAttributes*	r_attr;		// Attributes of this resource
	LoadQueue*		r_load_queue;  // Holds 1 minute avg % cpu usage
	char*			r_name;		// Name of this resource
	char*			r_id;		// CPU id of this resource

	int				type() { return r_attr->type(); };

private:
	int		kill_tid;	// DaemonCore timer id for kiling timer.

	int		did_update;		// Flag set when we do an update.
	int		fast_shutdown;	// Flag set if we're in fast shutdown mode.
	int		r_load_num_called;		// Counter used for CondorLoadAvg
};

#endif _STARTD_RESOURCE_H
