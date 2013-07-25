
#define MAXCHANGES 10
#define STATUSSPACE 3000
#define K 1024
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

	struct sizerequest {
		int memtimeatsize;
		int memsize;
		void *memchunk;
	};

	struct sizerequest timesteps[MAXCHANGES];
	int chunksize;
	void *stack[1000];
	int stackpointer;
	static char *status = NULL;
	int jobpid;
	int totalchunks;
	int totalK = 0;
	int chunkK;

 	int push(int chunks);
	int pop(int chunks);
	int init_storage(int size);
	static char *stamp();
	int report();

/*
 * This gets us to an argc of 21
*/
main(int argc,char **argv)
{
	int szchng;
	int myarg;
	char *pint;
	void *got;
	struct sizerequest *request ;
	FILE *out, *fopen();
	const char *name = "mem_checker.pid";
	const char *mode = "w";
	int chunkdiff;

	if(argc > ((2 * MAXCHANGES) + 1)) {
		printf("We can conly handle MAXCHANGES pairs\n");
		exit(0);
	}
	stackpointer = 0;
	status = malloc((size_t)STATUSSPACE);

	jobpid = getpid();
	//printf("my pid %d\n",jobpid);
	/*printf("my pid %d\n",getpid());*/

	init_storage(MAXCHANGES);
	/*
	display_storage(MAXCHANGES);
	*/
	chunkK = atoi(argv[1]);
	chunksize = ( K * chunkK);
	if(chunksize <= 0) {
		printf("Units of K must be greater then 0\n");
		exit(1);
	}

	//printf("Chunk size %d\n",chunksize);

	for(myarg = 2; myarg < argc; myarg++) {
		//printf("%s\n",argv[myarg]);
		myarg++;
		//printf("%s\n",argv[myarg]);
		/*printf("%d\n",atoi(argv[myarg]));*/
	}

	//printf("Storing requests\n");

	request = timesteps;

	for(myarg = 2; myarg < argc; myarg++) {
		request->memsize = atoi(argv[myarg]);
		/*
		printf("stored time %s\n",argv[myarg]);
		*/
		myarg++;
		request->memtimeatsize = atoi(argv[myarg]);
		/*
		printf("stored size %s\n",argv[myarg]);
		*/
		request++;
	}

	/*
	display_storage(MAXCHANGES);
	*/

	request = timesteps;
	totalchunks = 0;

	for(myarg = 0; myarg < MAXCHANGES; myarg++) {
		if(request->memsize == 0) {
			report();
			printf("done\nBye\n");
			exit(0);
		} else {
			//printf("%s time = %d size = %d - ",stamp(), request->memtimeatsize,request->memsize);
			if(request->memsize > 0) {
				if(request->memsize > totalchunks) {
					//printf("grow\n");
					chunkdiff = request->memsize - totalchunks;
					totalchunks = request->memsize;
					push(chunkdiff); /* grow */
				} else if(totalchunks > request->memsize) {
					//printf("shrink\n");
					chunkdiff = (totalchunks - request->memsize);
					totalchunks = request->memsize;
					pop(chunkdiff); /* shrink */
				} else {
					/* equal so do nothing */
				}
				totalK = chunkK * totalchunks;
				report();
			sleep(request->memtimeatsize);
			}
			request++;
		}
	}
}

int report()
{
	FILE *fp;
	size_t len = STATUSSPACE;
	size_t read;
	
	int vmpeak=-1, vmsize=-1, vmhwm=-1, vmrss=-1;

	fp = fopen("/proc/self/status","r");
	printf("%s PID %d, ",stamp(),jobpid);
	while ((read = getline(&status, &len, fp)) != -1) {
	    char label[32]; int size;
		int items = sscanf(status, "%s %d", &label, &size);
		//printf("scan `%s` found %d items: '%s' %d\n", status, items, label, size);
		if (match_prefix(label,"VmPeak")) vmpeak = size;
		else if (match_prefix(label, "VmSize")) vmsize = size;
		else if (match_prefix(label, "VmHWM")) vmhwm = size;
		else if (match_prefix(label, "VmRSS")) vmrss = size;
	}
	printf("VmPeak %6d, VmSize %6d, VmHWM %6d, VmRSS %6d, alloced %d kB\n", vmpeak, vmsize, vmhwm, vmrss, totalK);
}

/*
 * stackpointer always on location which can be filled
 */

int push(int chunks) {
	int units;

	//printf("push %d chunks\n",chunks);
	for(units = 0; units < chunks; units++) {
		stack[stackpointer] = malloc((size_t) chunksize);
		memset(stack[stackpointer],units,(size_t) chunksize);
		if(stackpointer != 999) {
			stackpointer++;
		} else {
			printf("exceeded the size of the stack");
			exit(1);
		}
	}
}

int pop(int chunks) {
	int units;

	//printf("pop %d chunks\n",chunks);
	for(units = 0; units < chunks; units++) {
		if(stackpointer != 0) {
			stackpointer--;
		} else {
			printf("Tried to pop empty stack");
			exit(1);
		}
		free(stack[stackpointer]);
	}
}

int init_storage(int size) {
	int szchng;
	struct sizerequest *request ;

	request = timesteps;

	for( szchng = 0; szchng < size; szchng++) {
		request->memtimeatsize = 0;
		request->memsize = 0;
		request->memchunk = NULL;
		request++;
	}

}

int display_storage(int size) {
	int szchng;
	struct sizerequest *request ;

	request = timesteps;

	for( szchng = 0; szchng < size; szchng++) {
		printf("Time %d ",request->memtimeatsize);
		printf("size %d\n",request->memsize);
		request++;
	}

}

static char *stamp()
{
	static char timebuf[80];
	time_t now;
			
	time(&now);
	strftime(timebuf, 80, "%m/%d/%y %H:%M:%S ", localtime(&now));
	return timebuf;
}

int
match_prefix(const char *s1, const char *s2)
{
    size_t  s1l = strlen(s1);
    size_t  s2l = strlen(s2);
    size_t min = (s1l < s2l) ? s1l : s2l;

	/* return true if the strings match (i.e., strcmp() returns 0) */
	if (strncmp(s1, s2, min) == 0)
		return 1;
 
	return 0;
}
