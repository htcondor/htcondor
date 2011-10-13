/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
	void dot_print(FILE *, const char *);

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
