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
	int		hardkill_claim();	// Just send hardkill to the starter,
								// no state changes, vacates, etc.
	
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

		// Methods to initialize and refresh the resource classads.
	int		init_classad();		
	void	update_classad();	
	void	timeout_classad();	

	int		update();				// Update the central manager.
	int		eval_and_update();		// Evaluate state and update CM. 

		// Methods to control various timers 
	int		start_update_timer();	// Timer for updating CM. 
	int		start_poll_timer();		// Timer for polling the resource
	void	cancel_poll_timer();	//    when it's in use by Condor. 

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

	int			did_update;		// Flag set when we do an update.

	int			up_tid;		// DaemonCore timer id for update timer.
	int			poll_tid;	// DaemonCore timer id for polling timer.

	Sock* 		coll_sock;		// Sock to the collector.
	Sock*	 	alt_sock;		// Sock to the alternate collector.

};

#endif _STARTD_RESOURCE_H
