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
	void	init_classad( ClassAd* );
	int		change( Activity );
	int		change( State );
	int 	eval();
	float	condor_load();
private:
	Resource*	rip;
	State 		r_state;
	Activity	r_act;		

	int		act_time;		// time we entered the current activitiy
	LoadQueue*	r_load_q;	// load average queue
		// Function called on every activity change which update the load_q
	void	load_activity_change();  

#if 0
	bool	is_valid( State );
	bool	is_valid( Activity );
	void	print_error( State );
	void	print_error( Activity );
#endif
};

#endif _RES_STATE_H

