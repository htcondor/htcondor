#if defined(AIX32) || defined(ULTRIX42) || defined(ULTRIX43)
#define HAS_PROTO
#endif

#if defined(AIX32)
typedef struct procinfo ProcInfo;
#endif

#if defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41)
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

