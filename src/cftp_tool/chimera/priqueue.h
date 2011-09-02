/* define some constants,  */

#define MSGSIZE 128       /* max size of a debug message */
#define FAILED   -1       /* define -1 to indicate failure of some operations */
#define EMPTY     1       /* define  1 to indicate that the heap is empty */ 

/* just in case NULL is not defined */

#ifndef NULL
#define NULL    0
#endif

/* define some macros */

#define FREE(x)  free(x) ; x = NULL            /* core if free'ed pointer is used */
#define LEFT(x)  (2*x)                         /* left child of a node */
#define RIGHT(x) ((2*x)+1)                     /* right child of a node */
#define PARENT(x) (x/2)                        /* parent of a node */
#define SWAP(t,x,y) tmp = x ; x = y ; y = tmp  /* swap to variables */

/* global character array for debug & error messages */

char messages[MSGSIZE];

typedef unsigned long long priority;

/* define a structure representing an individual node in the heap, and
 * make it a valid type for convenience */

typedef struct node {
  priority p;
  unsigned int id;
  int duration;
  int niceness;
  int cpu_usage;
  short sentinel;
} node ;

/* create a global node tmp, for swaping purposes */

node tmp;

/* for convience in function declarations, typedef a pointer to a node
 * as its own type, node_ptr */

typedef node * node_ptr;

/* define a structure representing the heap, and make it a valid type
 * for convenience */

typedef struct binary_heap {
  int heap_size;
  int max_elems;
  node_ptr elements;
} binary_heap ; 

/* function prototypes for functions which operate on a binary heap */ 

extern void        heapify(binary_heap *a,int i);
extern node_ptr    heap_max(binary_heap *a);
extern node        heap_extract_max(binary_heap *a);
extern void        heap_insert(binary_heap *a,node key);
extern void        heap_delete(binary_heap *a,int i);
extern void        heap_increase_key(binary_heap *a,int i,priority p);
extern void        heap_initialize(binary_heap *a,int nodes);
extern void        heap_finalize(binary_heap *a);

/* function prototypes for functions which operate on a node */

extern int         node_find(binary_heap a,unsigned int id);
extern node        node_create(unsigned int id,priority p,
			       int duration,int niceness,int cpu_usage);

/* function prototypes for helper functions */

extern int         compare_priority(node i,node j);
extern void        print_error(char *msg);
