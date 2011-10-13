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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_fix_setjmp.h"
#include "state_machine_driver.h"
#include "event_handler.unix.h"
#include "name_tab.h"


extern NameTable	StateNames;
extern NameTable	EventNames;
extern NameTable 	StateFuncNames;
extern NameTable	TransFuncNames;

static StateMachine	*CurFSA;
static sigjmp_buf		JmpBuf;

static const char	*TransFmt = "\t%-14s%-16s%-14s%-12s\n";

StateMachine::StateMachine(
	State *st, Transition *tr, const int sigs[], int start, int end
	) :
	start_state( start ),
	end_state( end ),
	StateTab( st ),
	TransitionTab( tr )
{
	int		i;

		// Calculate size of state table
	for( n_states=0; StateTab[n_states].id >= 0; n_states++ )
		;

		// Calculate size of transition table
	for( n_transitions=0; TransitionTab[n_transitions].from >= 0;
															n_transitions++)
		;

		// Package up set of signals into a sigset_t
	sigemptyset( &asynch_events );
	for( i=0; sigs[i]; i++ ) {
		sigaddset( &asynch_events, sigs[i] );
	}

		// Calculate signal mask for each state
	for( i=0; i<n_states; i++ ) {
		init_asynch_events( StateTab[i] );
	}
	no_print_tr = NULL;
	cur_state = -1;
}

/*
  Return TRUE if the given event is registered as an asynchronous event
  for this finite state machine, and FALSE otherwise.
*/
inline int
StateMachine::is_asynch_event( int event )
{
	return sigismember(&asynch_events,event) == 1;
}

/*
  Generate an individualized signal mask for a state.  The mask consists
  of those signals which the state machine has registered as an asynch
  event and for which this state has a transition.
*/
void
StateMachine::init_asynch_events( State &state )
{
	int		i;

	sigemptyset( &state.asynch_events );
	for( i=0; i<n_transitions; i++ ) {
		if( TransitionTab[i].from != state.id ) {
			continue;
		}
		if( is_asynch_event(TransitionTab[i].event) ) {
			sigaddset( &state.asynch_events, TransitionTab[i].event );
		}
	}
}

void
StateMachine::execute()
{
	State		*state_ptr;
	Transition	*transition = 0;
	int			next_state;
	int			event;
	EventHandler	h( fsa_sig_handler, asynch_events );


	CurFSA = this;
	cur_state = start_state;

	h.display();
	h.install();

	while( cur_state != end_state ) {
		if ( transition != no_print_tr ) {
			dprintf( D_ALWAYS, "\t*FSM* Transitioning to state \"%s\"\n",
					StateNames.get_name(cur_state) );
		}
		state_ptr = find_state( cur_state );
		next_state = sigsetjmp( JmpBuf, TRUE );
		if( !next_state ) {

			if( state_ptr->func ) {
				h.allow_events( state_ptr->asynch_events );

				if ( transition != no_print_tr ) {
					dprintf( D_ALWAYS, 
							"\t*FSM* Executing state func \"%s\" [ ",
							StateFuncNames.get_name((long)state_ptr->func) );
					state_ptr->display_events();
					dprintf( D_ALWAYS | D_NOHEADER, " ]\n" );
				}

					
				event = state_ptr->func();

				h.block_events( state_ptr->asynch_events );
			} else {
				event = -1;
			}

			transition = find_transition( event );

			if( transition->func ) {
				if (transition != no_print_tr) {
					dprintf( D_ALWAYS, 
							"\t*FSM* Executing transition function \"%s\"\n",
							TransFuncNames.get_name((long)transition->func) );
				}
				transition->func();
			}
			next_state = transition->to;
		}
		cur_state = next_state;
	}
	dprintf( D_ALWAYS, "\t*FSM* Reached state \"%s\"\n", StateNames.get_name(cur_state) );

	h.de_install();
	CurFSA = NULL;
}


void
fsa_sig_handler( int event )
{
	Transition	*transition;

	transition = CurFSA->find_transition( event );

	if (transition != CurFSA->no_print_tr) {
		dprintf( D_ALWAYS, "\t*FSM* Got asynchronous event \"%s\"\n",
				EventNames.get_name(event) );
	}

	if( transition->func ) {
		if (transition != CurFSA->no_print_tr) {
			dprintf( D_ALWAYS,
					"\t*FSM* Executing transition function \"%s\"\n",
					TransFuncNames.get_name((long)transition->func) );
		}
		if ( transition->func() == -2 ) {
			dprintf( D_ALWAYS,"\t*FSM* Aborting transition function \"%s\"\n",
					TransFuncNames.get_name((long)transition->func) );
			return;
		}
	}
	if( transition->to ) {
		siglongjmp( JmpBuf, transition->to );
	}
	return;
}

void
State::display_events()
{
	NameTableIterator	next_event( EventNames );
	int					event;

	while( (event = next_event()) != -1 ) {
		if( sigismember(&asynch_events,event) == 1 ) {
			dprintf( D_ALWAYS | D_NOHEADER, "%s ", EventNames.get_name(event) );
		}
	}
}


void
State::display()
{

	dprintf( D_ALWAYS,
		"\t%-14s%-20s",
		StateNames.get_name(id),
		StateFuncNames.get_name((long)func)
	);

	display_events();
	dprintf( D_ALWAYS | D_NOHEADER, "\n" );
}

void
Transition::display()
{
	dprintf( D_ALWAYS,TransFmt, StateNames.get_name(from), 
			EventNames.get_name(event), StateNames.get_name(to), 
			TransFuncNames.get_name((long)func));
}


void
Transition::dot_print(FILE *file, const char *color)
{
	if (to != 0) {
		fprintf(file, "\t%s -> %s [label = \"%s(%s)\", color = \"%s\"];\n",
				StateNames.get_name(from),
				StateNames.get_name(to),
				EventNames.get_name(event),
				TransFuncNames.get_name((long)func),
				color);
	} else {
		fprintf(file, "\t%s -> %s [label = \"%s(%s)\", color = \"%s\"];\n",
				StateNames.get_name(from),
				StateNames.get_name(from),
				EventNames.get_name(event),
				TransFuncNames.get_name((long)func),
				color);
	}
}



void
StateMachine::display()
{
	int		i;

	dprintf( D_ALWAYS, "StateMachine {\n" );
	dprintf( D_ALWAYS, "    start_state = %s\n", StateNames.get_name(start_state) );
	dprintf( D_ALWAYS, "    end_state = %s\n", StateNames.get_name(end_state) );

	dprintf( D_ALWAYS, "\n" );
	dprintf( D_ALWAYS, "    StateTable:\n" );
	dprintf( D_ALWAYS, "\t%-14s%-20s%s\n", "name", "function", "asynch_events");
	dprintf( D_ALWAYS, "\t%-14s%-20s%s\n", "----", "--------", "-------------");
	for( i=0; i<n_states; i++ ) {
		StateTab[i].display();
	}
	dprintf( D_ALWAYS, "\n" );
	dprintf( D_ALWAYS, "    TransitionTable:\n" );
	dprintf( D_ALWAYS, TransFmt, "from", "event", "to", "function" );
	dprintf( D_ALWAYS, TransFmt, "----", "-----", "--", "--------" );
	for( i=0; i<n_transitions; i++ ) {
		TransitionTab[i].display();
	}
	dprintf( D_ALWAYS, "}\n" );
}

void
StateMachine::dot_print(FILE *file)
{
	int		i;
	const char	*color;

	fprintf(file, "digraph FSA {\n");

	for( i=0; i<n_transitions; i++ ) {
		color = (is_asynch_event(TransitionTab[i].event) ? "red" : "blue");
		TransitionTab[i].dot_print(file, color);
	}
	fprintf(file, "}\n");
}


State *
StateMachine::find_state( int id )
{
	int		i;

	for( i=0; i<n_states; i++ ) {
		if( StateTab[i].id == id ) {
			return &StateTab[i];
		}
	}

	EXCEPT( "ERROR: Can't find state with id %d", id );
	return NULL;	// EXCEPT doesn't return, but the compiler doesn't know that...
}

Transition *
StateMachine::find_transition( int event )
{
	int		i;

	for( i=0; i<n_transitions; i++ ) {
		if( TransitionTab[i].from == cur_state
										&& TransitionTab[i].event == event ) {
			return( &TransitionTab[i] );
		}
	}

	EXCEPT(
		"Can't find transition out of state \"%s\" for event \"%s\"", 
		StateNames.get_name(cur_state), 
		EventNames.get_name(event) 
	);
	return NULL;	// EXCEPT doesn't return, but the compiler doesn't know that...
}

