/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** All rights reserved except as provided by specific written agreement.
** 
*/ 

#ifndef FSA_H
#define FSA_H

#define N_POSIX_SIGS 19

/* pointer to function taking no args and returning an integer */
typedef int (*STATE_FUNC)();

/* pointer to function taking no args and returning int */
typedef int (*TRANS_FUNC)();

class State {
public:
	void display();
	void display_events();

	int			id;
	sigset_t	asynch_events;
	STATE_FUNC	func;
};


class StateMachine;


class Transition {
public:
	void display();
	void dot_print(FILE *, char *);

	int				from;
	int				event;
	int				to;
	TRANS_FUNC		func;
};

class StateMachine {
public:
	StateMachine(
		State st[], Transition tr[], const int sigs[],
		int start, int end
	);
	void execute();
	Transition	*find_transition( int event );
	void display();
	void dot_print(FILE *);
	void dont_print_transition( Transition *tr) { no_print_tr = tr; }
	Transition  *no_print_tr;

private:
	State		*find_state( int state_id );
	void		init_asynch_events( State & state );
	inline int	is_asynch_event( int event );


	int			start_state;
	int			end_state;
	int			cur_state;
	int			n_states;
	sigset_t	asynch_events;
	State		*StateTab;
	int			n_transitions;
	Transition	*TransitionTab;
};

void fsa_sig_handler( int );

#endif /* FSA_H */
