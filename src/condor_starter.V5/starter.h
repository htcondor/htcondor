/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 


/*
  This file contains definitions for the finite state machine
  implementing the condor_starter.  All states, events, transitions,
  and prototypes for functions to be executed by those states and
  transitions are defined here.
*/

//

/*
  All possible states for our finite state machine.  N.B. if you change
  this, keep the name table defined below up to date!
*/
typedef enum {
	NO_STATE,		/* *MUST* be 0, fsa depends on it!! */
	START,
	GET_PROC,
	GET_EXEC,
	TERMINATE,
	SUPERVISE,
	UPDATE_ALL,
	PROC_EXIT,
	SEND_CORE,
	UPDATE_WAIT,
	TERMINATE_WAIT,
	SEND_CKPT_ALL,
	UPDATE_ONE,
	SEND_STATUS_ALL,
#if defined(LINK_PVM)
	READ_PVM_MSG,
#endif
	END
} STATE;

/*
  Name table for states.  This allows the finite state machinery to
  print out nice debugging messages.  It *must* match the STATE
  enumeration above.
*/
#if defined(INSERT_TABLES)
static NAME_VALUE	StateNameArray[] = {
	{ NO_STATE,			"(RETURN)"			},
	{ START,			"START"				},
	{ GET_PROC,			"GET_PROC"			},
	{ GET_EXEC,			"GET_EXEC"			},
	{ TERMINATE,		"TERMINATE"			},
	{ SUPERVISE	,		"SUPERVISE"			},
	{ UPDATE_ALL,		"UPDATE_ALL"		},
	{ PROC_EXIT,		"PROC_EXIT"			},
	{ SEND_CORE,		"SEND_CORE"			},
	{ UPDATE_WAIT,		"UPDATE_WAIT"		},
	{ TERMINATE_WAIT,	"TERMINATE_WAIT"	},
	{ SEND_CKPT_ALL,	"SEND_CKPT_ALL"		},
	{ UPDATE_ONE,		"UPDATE_ONE"		},
	{ SEND_STATUS_ALL,	"SEND_STATUS_ALL"	},
#if defined(LINK_PVM)
	{ READ_PVM_MSG,		"READ_PVM_MSG"		},
#endif
	{ END,				"END"				},
	{ -1,				"(UNKNOWN)"			}
};
NameTable StateNames( StateNameArray );
#endif

//

/*
  Events which cause tranitions between states.  Many are generated
  externally and received as signals.  Those *must* be encoded with the
  same integer as the corresponding signal.  N.B. if you change this,
  keep the name table defined below up to date!

  Note: Due to the way enumeration values are assigned, the order of this
  list is important.  All of the asynchronous events which are assigned
  specific values from "signal.h" must come after the 0 valued event
  (NO_EVENT), but before any other event.  Furthermore of the asynch
  events, the one with the highest number (generally SIGUSR2) must
  come last.  This is so that events later in the list will all have
  greater values than any asynchronous events.  If this ordering is
  violated, we may end up with two different events encoded by the
  same value.  (I know POSIX says we shouldn't depend on numeric values
  of signals...)
*/
typedef enum event_type {
	NO_EVENT = 0,				/* place holder */
	GET_NEW_PROC = SIGHUP,		/* get new process from the shadow */
	SUSPEND = SIGUSR1,			/* suspend user job, then suspend self */
	CONTINUE = SIGCONT,			/* continue user job, continue self */
	VACATE = SIGTSTP,			/* terminate user job, return existing ckpt */
	ALARM = SIGALRM,			/* request user job to ckpt */
	DIE = SIGINT,				/* terminate user job, don't return ckpt */
	CHILD_EXIT = SIGCHLD,		/* user process terminated */
	CKPT_and_VACATE = SIGUSR2,	/* create new ckpt, then return it */
	CKPT_EXIT,					/* user process exited for checkpoint */
	SUCCESS,					/* generic successful result */
	FAILURE,					/* generic failure result */
	DO_XFER,					/* transfer checkpoint files */
	DONT_XFER,					/* cleanup w/out tranferring ckpt files */
	DO_QUIT,					/* quit after updating checkpoints */
	DO_WAIT,					/* wait for an asynchronous event */
	NO_CORE,					// Process exited, but didn't leave a core
	HAS_CORE,					// Process exited abnormally with a core
	EXITED,						// Process exited - may or may not have core
	TRY_LATER,					// No new work now - ask again later
#if defined(LINK_PVM)
    PVM_MSG,					// A PVM message has arrived
#endif
    PVM_CHILD_EXIT,				// Child exit via PVM message
	DEFAULT = -1				/* *MUST* be -1 fsa depends on it! */
} EVENT;

/*
  Name table for events.  This allows the finite state machinery to
  print out nice debugging messages.  It *must* match the EVENT
  enumeration above.
*/
#if defined(INSERT_TABLES)
static NAME_VALUE	EventNameArray[] = {
	{ NO_EVENT,			"NO_EVENT"			},
	{ GET_NEW_PROC,		"GET_NEW_PROC"		},
	{ SUSPEND,			"SUSPEND"			},
	{ CONTINUE,			"CONTINUE"			},
	{ VACATE,			"VACATE"			},
	{ ALARM,			"ALARM"				},
	{ DIE,				"DIE"				},
	{ CHILD_EXIT,		"CHILD_EXIT"		},
	{ CKPT_and_VACATE,	"CKPT_and_VACATE"	},
	{ CKPT_EXIT,		"CKPT_EXIT"			},
	{ SUCCESS,			"SUCCESS"			},
	{ FAILURE,			"FAILURE"			},
	{ DO_XFER,			"DO_XFER"			},
	{ DONT_XFER,		"DONT_XFER"			},
	{ DO_QUIT,			"DO_QUIT"			},
	{ DO_WAIT,			"DO_WAIT"			},
	{ NO_CORE,			"NO_CORE"			},
	{ HAS_CORE,			"HAS_CORE"			},
	{ TRY_LATER,		"TRY_LATER"			},
#if defined(LINK_PVM)
    { PVM_MSG,			"PVM_MSG"			},
    { PVM_CHILD_EXIT,	"PVM_CHILD_EXIT"	},
#endif
	{ -1,				"(DEFAULT)"			}
};
NameTable EventNames( EventNameArray );
#endif

//

/*
  List state action routines here.  Each routine must take no
  arguments, and return an integer.  The return is an EVENT which is
  intended to encode the outcome of executing the routine.  The
  transition taken out of the state will depend on the EVENT returned.
  If there is only one outcome possible, the routine should return the
  "DEFAULT" EVENT.  N.B. if you change this, keep the name table
  defined below up to date!
*/
int		init();
int		get_proc();
int		get_exec();
int		terminate_all();
int		asynch_wait();
int		supervise_all();
int		proc_exit();
int		send_core();
int		update_all();
int		update_one();
int		send_ckpt_all();
int		dispose_all();
#if defined(LINK_PVM)
int		read_pvm_msg();
#endif

/*
  Name table for state action routines.  This allows the finite state
  machinery to print out nice debugging messages.  It *must* match the
  state action routines listed above.
*/
#if defined(INSERT_TABLES)
NAME_VALUE	StateFuncArray[] = {
	{ (unsigned long)init,			"init()"			},
	{ (unsigned long)get_proc,		"get_proc()"		},
	{ (unsigned long)get_exec,		"get_exec()"		},
	{ (unsigned long)terminate_all,	"terminate_all()"	},
	{ (unsigned long)asynch_wait,	"asynch_wait()"	},
	{ (unsigned long)supervise_all,	"supervise_all()"	},
	{ (unsigned long)proc_exit,		"proc_exit()"		},
	{ (unsigned long)send_core,		"send_core()"		},
	{ (unsigned long)update_all,	"update_all()"		},
	{ (unsigned long)update_one,	"update_one()"		},
	{ (unsigned long)send_ckpt_all,	"send_ckpt_all()"	},
	{ (unsigned long)dispose_all,	"dispose_all()"		},
#if defined(LINK_PVM)
    { (unsigned long)read_pvm_msg,	"read_pvm_msg()"	},
#endif
	{ -1,					""					}
};
NameTable StateFuncNames( StateFuncArray );
#endif

//

/*
  List transition routines here.  Each routine must take no arguments,
  and return void.  N.B. if you change this, keep the name table
  defined below up to date!
*/

void susp_all();			// Suspend self and user processes
void req_die();				// Request all procs to exit - don't transfer ckpts
void req_vacate();			// Request all procs to exit - do transfer ckpts
void spawn_all();			// Spawn all user processes
void stop_all();			// Stop all user processes
void handle_vacate_req();	// Deal with Ckpt_and_Vacate in Supervise state
void susp_ckpt_timer();		// Suspend ckpt timer, but save remaining time
void reaper();				// Gather status of exited child process
void set_quit();			// Set quit flag, will exit after updating ckpts
void susp_self();			// Suspend ourself, wait for CONTINUE
void cleanup();				// Forcibly kill procs and delete their files
void unset_xfer();			// Don't transfer checkpoints
void dispose_one();			// Send final status to shadow and delete process
void update_cpu();			// Send updated CPU usage info to shadow
#if defined(LINK_PVM)
void pvm_reaper();			// Reap via PVM message
#endif
void make_runnable();		// Make ckpt'ed process runnable again

/*
  Name table for transition routines.  This allows the finite state
  machinery to print out nice debugging messages.  It *must* match the
  transition routines listed above.
*/
#if defined(INSERT_TABLES)
NAME_VALUE	TransFuncArray[] = {
	{ (unsigned long)susp_all,			"susp_all"			},
	{ (unsigned long)req_die,				"req_die"			},
	{ (unsigned long)req_vacate,			"req_vacate"		},
	{ (unsigned long)spawn_all,			"spawn_all"			},
	{ (unsigned long)stop_all,			"stop_all"			},
	{ (unsigned long)handle_vacate_req,	"handle_vacate_req"	},
	{ (unsigned long)susp_ckpt_timer,		"susp_ckpt_timer"	},
	{ (unsigned long)reaper,				"reaper"			},
	{ (unsigned long)set_quit,			"set_quit"			},
	{ (unsigned long)susp_self,			"susp_self"			},
	{ (unsigned long)cleanup,				"cleanup"			},
	{ (unsigned long)unset_xfer,			"unset_xfer"		},
	{ (unsigned long)dispose_one,			"dispose_one"		},
	{ (unsigned long)update_cpu,			"update_cpu"		},
#if defined(LINK_PVM)
    { (unsigned long)pvm_reaper,			"pvm_reaper"		},
#endif
    { (unsigned long)make_runnable,			"make_runnable"		},
	{ -1,						""					}
};
NameTable TransFuncNames( TransFuncArray );
#endif

//

// Some events my arrive asynchronously in the form of signals.  List
// all such signals here.  These events will be unblocked when executing
// in states that are interruptable, and blocked otherwise.  Terminate the
// list with a zero.
#if defined(INSERT_TABLES)
int	EventSigs[] = {
	SUSPEND,
	VACATE,
	ALARM,
	DIE,
	CHILD_EXIT,
	CKPT_and_VACATE,
	GET_NEW_PROC,
	0
};
#endif


/*
  This is the acutal state table.  States and their corresponding
  functions along here.  All asynch_event sets start out empty - those
  are computed at run time from the transition table.
*/
#if defined(INSERT_TABLES)
sigset_t	EmptySet;
State	StateTab[] = {
   // ID				ASYNCH_EVENTS	FUNCTION
   // --				-------------	--------
	{ START,			EmptySet,		init			},
	{ GET_PROC,			EmptySet,		get_proc		},
	{ GET_EXEC,			EmptySet,		get_exec		},
	{ TERMINATE,		EmptySet,		terminate_all	},
	{ SUPERVISE,		EmptySet,		supervise_all	},
	{ UPDATE_ALL,		EmptySet,		update_all		},
	{ PROC_EXIT,		EmptySet,		proc_exit		},
	{ SEND_CORE,		EmptySet,		send_core		},
	{ UPDATE_WAIT,		EmptySet,		asynch_wait		},
	{ TERMINATE_WAIT,	EmptySet,		asynch_wait		},
	{ SEND_CKPT_ALL,	EmptySet,		send_ckpt_all	},
	{ UPDATE_ONE,		EmptySet,		update_one		},
	{ SEND_STATUS_ALL,	EmptySet,		dispose_all		},
#if defined(LINK_PVM)
    { READ_PVM_MSG,		EmptySet,		read_pvm_msg	},
#endif
	{ END,				EmptySet,		0				},
	{ -1,				EmptySet,		0				}
};
#endif

//

/*
  This is the actual transition table.  Transitions will be taken if
  the specified EVENT occurs when we are executing in the specified
  STATE.  After the function (if not NULL) is executed, we transition
  to the TO_STATE.  If the TO_STATE is null, we return to the FROM_STATE
  at the point we were interrupted.
*/
#if defined(INSERT_TABLES)
Transition TransTab[] = {
// FROM_STATE		EVENT				TO_STATE		FUNCTION
// ----------		-----				--------		--------
{ START,			DEFAULT,			GET_PROC,		0					},

{ GET_PROC,			SUCCESS,			GET_EXEC,		0					},
{ GET_PROC,			FAILURE,			TERMINATE,		req_vacate			},
{ GET_PROC,			TRY_LATER,			SUPERVISE,		0					},

{ GET_EXEC,			SUSPEND,			0,				susp_all			},
{ GET_EXEC,			DIE,				TERMINATE,		req_die				},
{ GET_EXEC,			VACATE,				TERMINATE,		req_vacate			},
{ GET_EXEC,			FAILURE,			SUPERVISE,		dispose_one			},
{ GET_EXEC,			SUCCESS,			SUPERVISE,		spawn_all			},
{ GET_EXEC,			CKPT_and_VACATE,	TERMINATE,		req_vacate			},

{ TERMINATE,		DO_WAIT,			TERMINATE_WAIT,	0					},
{ TERMINATE,		DONT_XFER,			SEND_STATUS_ALL,0					},
{ TERMINATE,		DO_XFER,			SEND_CKPT_ALL,	0					},

{ TERMINATE_WAIT,	SUSPEND,			0,				susp_self			},
{ TERMINATE_WAIT,	DIE,				0,				unset_xfer			},
{ TERMINATE_WAIT,	ALARM,				END,			cleanup				},
{ TERMINATE_WAIT,	CHILD_EXIT,			TERMINATE,		reaper				},

{ SUPERVISE,		VACATE,				TERMINATE,		req_vacate			},
{ SUPERVISE,		DIE,				TERMINATE,		req_die				},
{ SUPERVISE,		CHILD_EXIT,			PROC_EXIT,		reaper				},
{ SUPERVISE,		GET_NEW_PROC,		GET_PROC,		susp_ckpt_timer		},
{ SUPERVISE,		SUSPEND,			0,				susp_all			},

#if defined(NO_CKPT)
	{ SUPERVISE,		CKPT_and_VACATE,	TERMINATE,		req_die			},
#else
	{ SUPERVISE,		CKPT_and_VACATE,	TERMINATE,		req_vacate		},
#endif

#if defined(LINK_PVM)
	{ SUPERVISE,		PVM_MSG,			READ_PVM_MSG,	0				},
	{ READ_PVM_MSG,		PVM_CHILD_EXIT,		PROC_EXIT,		pvm_reaper		},
	{ READ_PVM_MSG,		DEFAULT,			SUPERVISE,		0				},
#endif

{ PROC_EXIT,		NO_CORE,			SUPERVISE,		dispose_one			},
{ PROC_EXIT,		SEND_CORE,			SEND_CORE,		0					},
{ PROC_EXIT,		CKPT_EXIT,			SUPERVISE,		make_runnable		},
{ PROC_EXIT,		HAS_CORE,			SEND_CORE,		0					},

{ SEND_CORE,		VACATE,				TERMINATE,		req_vacate			},
{ SEND_CORE,		DIE,				TERMINATE,		req_die				},
{ SEND_CORE,		SUSPEND,			0,				susp_all			},
{ SEND_CORE,		DEFAULT,			SUPERVISE,		dispose_one			},

{ UPDATE_ALL,		SUCCESS,			SUPERVISE,		spawn_all			},
{ UPDATE_ALL,		DO_QUIT,			SEND_CKPT_ALL,	0					},
{ UPDATE_ALL,		UPDATE_ONE,			UPDATE_ONE,		0					},
{ UPDATE_ALL,		DO_WAIT,			UPDATE_WAIT,	0					},

{ UPDATE_WAIT,		CHILD_EXIT,			UPDATE_ONE,		reaper				},
{ UPDATE_WAIT,		CKPT_and_VACATE,	0,				set_quit			},
{ UPDATE_WAIT,		SUSPEND,			0,				susp_self			},
{ UPDATE_WAIT,		VACATE,				TERMINATE,		req_vacate			},
{ UPDATE_WAIT,		DIE,				TERMINATE,		req_die				},

{ UPDATE_ONE,		EXITED,				UPDATE_ALL,		dispose_one			},
{ UPDATE_ONE,		FAILURE,			SEND_CKPT_ALL,	0					},
{ UPDATE_ONE,		CKPT_and_VACATE,	0,				set_quit			},
{ UPDATE_ONE,		SUSPEND,			0,				susp_self			},
{ UPDATE_ONE,		VACATE,				TERMINATE,		req_vacate			},
{ UPDATE_ONE,		DIE,				TERMINATE,		req_die				},
{ UPDATE_ONE,		SUCCESS,			UPDATE_ALL,		update_cpu			},

{ SEND_CKPT_ALL,	SUSPEND,			0,				susp_self			},
{ SEND_CKPT_ALL,	DEFAULT,			SEND_STATUS_ALL,0					},
{ SEND_CKPT_ALL,	DIE,				SEND_STATUS_ALL,0					},

{ SEND_STATUS_ALL,	DEFAULT,			END,			0					},

{ -1,				NO_EVENT,			-1,				0					}
};

#if defined(INSERT_TABLES) && defined(LINK_PVM)
NAME_VALUE	PvmMsgNameArray[] = {
{ SM_SPAWN,		"SM_SPAWN"},
{ SM_EXEC,		"SM_EXEC"},
{ SM_EXECACK,	"SM_EXECACK"},
{ SM_TASK,		"SM_TASK"},
{ SM_CONFIG,	"SM_CONFIG  "},
{ SM_ADDHOST,	"SM_ADDHOST "},
{ SM_DELHOST,	"SM_DELHOST "},
{ SM_ADD,	 	"SM_ADD     "},
{ SM_ADDACK,	"SM_ADDACK  "},
{ SM_NOTIFY,	"SM_NOTIFY  "},
{ SM_TASKX,		"SM_TASKX   "},
{ SM_HOSTX,		"SM_HOSTX   "},
{ SM_HANDOFF,	"SM_HANDOFF "},
{ SM_SCHED,		"SM_SCHED   "},
{ SM_STHOST,	"SM_STHOST"},
{ SM_STHOSTACK, "SM_STHOSTACK"},
{ -1,			""}
};
NameTable	PvmMsgNames( PvmMsgNameArray );
#endif
#endif
