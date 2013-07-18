
#define MAXCHANGES 10
#define K 1024
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

	struct sizerequest {
		int memtimeatsize;
		int memsize;
		void *memchunk;
	};

	struct sizerequest timesteps[MAXCHANGES];
	int chunksize;
	void *stack[1000];
	int stackpointer;

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
	const char *name = "grow.out";
	const char *mode = "w";
	int totalchunks;
	int chunkdiff;

	if(argc > ((2 * MAXCHANGES) + 1)) {
		printf("We can conly handle MAXCHANGES pairs\n");
		exit(0);
	}
	stackpointer = 0;

	out = fopen(name,mode);

	/*fprintf(out,"my pid %d\n",getpid());*/
	printf("my pid %d\n",getpid());

	init_storage(MAXCHANGES);
	/*
	display_storage(MAXCHANGES);
	*/
	chunksize = ( K * atoi(argv[1]));
	if(chunksize <= 0) {
		printf("Units of K must be greater then 0\n");
		exit(1);
	}

	printf("Chunk size %d\n",chunksize);

	for(myarg = 2; myarg < argc; myarg++) {
		printf("%s\n",argv[myarg]);
		myarg++;
		printf("%s\n",argv[myarg]);
		/*printf("%d\n",atoi(argv[myarg]));*/
	}

	printf("Storing requests\n");

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
			printf("done\nBye\n");
			fclose(out);
			break;
		} else {
			printf("time = %d size = %d\n",request->memtimeatsize,request->memsize);
			if(request->memsize > 0) {
				if(request->memsize > totalchunks) {
					printf("grow\n");
					chunkdiff = request->memsize - totalchunks;
					totalchunks = request->memsize;
					push(chunkdiff); /* grow */
				} else if(totalchunks > request->memsize) {
					printf("shrink\n");
					chunkdiff = (totalchunks - request->memsize);
					totalchunks = request->memsize;
					pop(chunkdiff); /* shrink */
				} else {
					/* equal so do nothing */
				}
			sleep(request->memtimeatsize);
			}
			request++;
		}
	}
}

/*
 * stackpointer always on location which can be filled
 */

int push(int chunks) {
	int units;

	printf("push %d chunks\n",chunks);
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

	printf("pop %d chunks\n",chunks);
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
