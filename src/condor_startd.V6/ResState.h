#ifndef _RES_STATE_H
#define _RES_STATE_H

class LoadQueue
{
public:
	LoadQueue( int q_size );
	~LoadQueue();
	void	push( int num, char val );
	float	avg();
	float	avg( int num );
private:
	int			head;
	char*	 	buf;
	int			size;
};


class ResState
{
public:
	ResState(Resource* rip);
	~ResState();
	State	state() {return r_state;};
	Activity activity() {return r_act;};
	int		change( Activity );
	int		change( State );
	int 	eval();
	float	condor_load();
private:
	Resource*	rip;
	State 		r_state;
	Activity	r_act;		

	void	compute_load();
	float	r_load_avg;			// condor generated load average
	int		last_compute;
	LoadQueue*	r_load_q;

	bool	is_valid( State );
	bool	is_valid( Activity );
	void	print_error( State );
	void	print_error( Activity );
};

#endif _RES_STATE_H

