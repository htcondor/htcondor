/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#if defined(AIX32) || defined(ULTRIX42) || defined(ULTRIX43)
#define HAS_PROTO
#endif

#if defined(AIX32)
typedef struct procinfo ProcInfo;
#endif

#if defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41) || defined(OSF1)
typedef struct proc ProcInfo;
#endif



#define QUEUE_SIZE	8
#define DUMMY -1
#define MATCH 0		/* indicates matching strcmp() */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef struct {
	char	data[ QUEUE_SIZE ];
	int		first;
	int		last;
} QUEUE;

typedef struct node {
	int		pid;
	QUEUE	*state;
	int		touched;
	int		type;
	struct node	*parent;
	struct node	*children;
	struct node	*next;
	struct node	*prev;
} NODE;

#if defined(HAS_PROTO)
#define PROTO(args) args
#else
#define PROTO(args) ()
#endif

void	insert_proc PROTO(( ProcInfo *proc, NODE *tree ));
void	add_to_queue PROTO(( QUEUE *q, int val ));
void	display_node PROTO(( NODE *n, int level ));
void	add_to_list PROTO(( NODE *new, NODE *list ));
void	delete_from_list PROTO(( NODE *p ));
void	walk_tree PROTO(( void func(NODE *), NODE *tree ));
void	mark_untouched PROTO(( NODE *n ));
void	delete_terminated_procs PROTO(( NODE *n ));
void	find_active PROTO(( NODE *n ));
void	init PROTO(());
ProcInfo	*search_tab PROTO(( int pid ));
NODE	*search_tree PROTO(( int pid, NODE *tree ));
NODE	*create_node PROTO(( int pid, int state ));
NODE	*create_tree PROTO(());
NODE	*create_list PROTO(());
QUEUE	*create_queue PROTO(());
char	state PROTO(( ProcInfo *p ));

