/*
	killkids( int pid) 
	Author: Todd Tannenbaum
	
	pass killkids a process id, and killkids will kill all of the
	descendents (children, grand-children, great-grand, etc) of that
	process.  it calls /bin/ps; this is lame, but it is portable.
	if you have a "berkeley" style ps (such as SunOS 4.x or BSD), then
	you should #define BERKELEY_PS somewhere.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define MAX_NUM_PIDS 2000

#if defined(SUNOS41) || defined(LINUX)
#define BERKELEY_PS
#endif

#if defined(BERKELEY_PS)
#define PS_CMD "/bin/ps -jax"
#elif defined(ULTRIX42) || defined(ULTRIX43)
#define PS_CMD "/bin/ps -axl"
#else
#define PS_CMD "/bin/ps -ef "
#endif 

typedef struct mypstree {
	pid_t pid;
	unsigned int left,right,child;
} MYPSTREE;
	
static MYPSTREE mypstree[MAX_NUM_PIDS];
static treei = 1;

static
int
treescan(pid_t findpid)
{
	int i = 1;

	while ( mypstree[i].pid != 0 ) {
		if ( findpid < mypstree[i].pid ) {
			if ( mypstree[i].left > 0 )
				i = mypstree[i].left;
			else {
				mypstree[i].left = ++treei;
				if ( treei == MAX_NUM_PIDS )
					treei--;
				i = treei;
				break;
			}
		} else 
		if ( findpid > mypstree[i].pid ) {
			if ( mypstree[i].right > 0 )
				i = mypstree[i].right;
			else {
				mypstree[i].right = ++treei;
				if ( treei == MAX_NUM_PIDS )
					treei--;
				i = treei;
				break;
			}
		} else {
			break;
		}
	}
	mypstree[i].pid = findpid;
	return(i);
}
		
void
killkids(pid_t inpid, int sig)
{
   FILE *ps;
   char line[250];
   pid_t pid, ppid;
   unsigned int parent,child,temp;

   treei = 1;
   memset(mypstree,0,sizeof(MYPSTREE)*MAX_NUM_PIDS);

   ps = popen(PS_CMD,"r");
   if ( ps == NULL )
      exit(-1);

   fgets(line,249,ps);  /* skip the column header line */

   while ( fgets(line,249,ps) != NULL ) {
#if defined(BERKELEY_PS)
	if ( sizeof(pid_t) == sizeof(long) )
		sscanf(line,"%ld %ld ",&ppid,&pid);
	else
		sscanf(line,"%d %d ",&ppid,&pid);
#elif defined(ULTRIX42) || defined(ULTRIX43)
	if ( sizeof(pid_t) == sizeof(long) )
		sscanf(line,"%*s %*s %ld %ld ",&ppid,&pid);
	else
		sscanf(line,"%*s %*s %d %d ",&ppid,&pid);
#else
	if ( sizeof(pid_t) == sizeof(long) )
		sscanf(line,"%*s %ld %ld ",&pid,&ppid);
	else
		sscanf(line,"%*s %d %d ",&pid,&ppid);
#endif
	if ( (ppid == 0) || (ppid == 1) )
		continue;
	child = treescan(pid);
	parent = treescan(ppid);

	if ( pid != inpid ) {
		temp = mypstree[parent].child;
		mypstree[parent].child = child;
		while ( mypstree[child].child != 0 )
			child = mypstree[child].child;
		mypstree[child].child = temp;
	}
   }

        /* don't use 'pclose()' here, it does its own wait, and messes
           with our handling of SIGCHLD! */
        /* except on HPUX pclose() is both safe & required - Todd */
#if   defined(HPUX9) || defined(Solaris) 
   pclose(ps);
#else
   (void)fclose(ps);
#endif

	temp = treescan(inpid);
	if ( mypstree[temp].child != 0 ) {
		temp = mypstree[temp].child;
		while ( mypstree[temp].pid > 0 ) {

    		if( sig != SIGCONT ) 
		         kill(mypstree[temp].pid,SIGCONT);
 
   		    kill(mypstree[temp].pid,sig);
		
            temp = mypstree[temp].child;
		}
	}
		
}
