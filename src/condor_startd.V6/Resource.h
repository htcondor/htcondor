#ifndef _STARTD_RESOURCE_H
#define _STARTD_RESOURCE_H

class Resource
{
public:
	Resource( Sock*, Sock* );
	~Resource();

		// Members to initialize and refresh the resource classad.
	void	init_classad();		
	void	update_classad();	
	void	timeout_classad();	

	int		update();				// Update the central manager.
	int		eval_and_update();		// Evaluate state and update CM. 

		// Members to control various timers 
	int		start_update_timer();	// Timer for updating CM. 
	int		start_poll_timer();		// Timer for polling the resource
	void	cancel_poll_timer();	//    when it's in use by Condor. 

		// Resource state members
	int		change_state( State );
	int		change_state( Activity );
	State	state() {return r_state->state();};
	Activity	activity() {return r_state->activity();};
	int		eval_state() {return r_state->eval();};
	bool	in_use();
	float	condor_load() {return r_state->condor_load();};

	void	starter_exited();

		// Data members
	ResState*		r_state;	// Startd state object, contains state and activity
	ClassAd*		r_classad;	// Resource classad
	ClassAd*		r_private_classad;	// Private classad w/ capability, etc.
	Starter*		r_starter;	// Starter object
	Match*			r_cur;		// Info about the current match
	Match*			r_pre;		// Info about the possibly preempting match
	Reqexp*			r_reqexp;   // Object for the requirements expression
	ResAttributes*	r_attr;		// Parameters of this resource
	char*			r_name;		// Name of this resource
	
private:
		// Send resource classad to the given sock.  The int indicates
		// if we should send the private classad or not.
	send_classad_to_sock( Sock* sock, int send_private = FALSE );

	int			did_update;		// Flag set when we do an update.

	int			up_tid;		// DaemonCore timer id for update timer.
	int			poll_tid;	// DaemonCore timer id for polling timer.

	Sock* 		coll_sock;		// Sock to the collector.
	Sock*	 	alt_sock;		// Sock to the alternate collector.

};

#endif _STARTD_RESOURCE_H
