#ifndef _STARTD_RESOURCE_H
#define _STARTD_RESOURCE_H

class Resource : public Service
{
public:
	Resource( Sock*, Sock* );
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
	float	condor_load() {return r_state->condor_load();};

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

	int		update();				// Update the central manager.
	int		eval_and_update();		// Evaluate state and update CM. 
	void	final_update();			// Send a final update to the CM
									// with Requirements = False

		// Methods to control various timers 
	int		start_update_timer();	// Timer for updating CM. 
	int		start_poll_timer();		// Timer for polling the resource
	void	cancel_poll_timer();	//    when it's in use by Condor. 
	int		start_kill_timer();		// Timer for how long we're willing to 
	void	cancel_kill_timer();	//    be in preempting/killing state. 

 		// Helper functions to evaluate resource expressions
	int		wants_vacate();
	int		wants_suspend();
	int		wants_pckpt();
	int		eval_kill();
	int		eval_vacate();
	int		eval_suspend();
	int		eval_continue();

		// Data members
	ResState*		r_state;	// Startd state object, contains state and activity
	ClassAd*		r_classad;	// Resource classad (contains everything in config file)
	Starter*		r_starter;	// Starter object
	Match*			r_cur;		// Info about the current match
	Match*			r_pre;		// Info about the possibly preempting match
	Reqexp*			r_reqexp;   // Object for the requirements expression
	ResAttributes*	r_attr;		// Parameters of this resource
	char*			r_name;		// Name of this resource
	
private:
		// Make public and private ads out of our classad
	void make_public_ad( ClassAd* );
	void make_private_ad( ClassAd* );
		// Send given classads to the given sock.  If either pointer
		// is NULL, the class ad is not sent.  
	send_classad_to_sock( Sock* sock, ClassAd* pubCA, ClassAd* privCA );

	int		up_tid;		// DaemonCore timer id for update timer.
	int		poll_tid;	// DaemonCore timer id for polling timer.
	int		kill_tid;	// DaemonCore timer id for kiling timer.

	int		did_update;		// Flag set when we do an update.
	int		fast_shutdown;	// Flag set if we're in fast shutdown mode.

	Sock*	coll_sock;		// Sock to the collector.
	Sock* 	alt_sock;		// Sock to the alternate collector.

};

#endif _STARTD_RESOURCE_H
